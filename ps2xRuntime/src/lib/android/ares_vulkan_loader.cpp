#include "ares_android_vulkan.h"
#include "ares_vulkan_probe.h"

#include <adrenotools/driver.h>
#include <android/asset_manager.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <dlfcn.h>
#include <jni.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <atomic>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

extern "C" android_app *GetAndroidApp();

namespace
{
    constexpr char kDriver[] = "vulkan.ad07xx.so";
    constexpr char kAsset[] = "drivers/turnip-r7-sysmem/vulkan.ad07xx.so";
    constexpr size_t kDriverBytes = 17161105;
    constexpr char kDriverHash[] = "ab879bc6605b81bc790eb3519608a998a681d0c47b2d957ea8219f2e6eaa6082";

    std::string quote(const std::string &value)
    {
        std::ostringstream out;
        out << '"';
        for (unsigned char c : value)
        {
            if (c == '"' || c == '\\') out << '\\' << static_cast<char>(c);
            else if (c < 32) { char escape[7]; std::snprintf(escape, sizeof(escape), "\\u%04x", c); out << escape; }
            else out << static_cast<char>(c);
        }
        out << '"';
        return out.str();
    }

    struct Journal
    {
        std::filesystem::path directory;
        std::ofstream log;
        explicit Journal(ANativeActivity *activity)
        {
            directory = activity->externalDataPath ? activity->externalDataPath : activity->internalDataPath;
            std::error_code error;
            std::filesystem::create_directories(directory, error);
            log.open(directory / "ares-vulkan-report.log", std::ios::trunc);
            if (!log && activity->internalDataPath)
            {
                directory = activity->internalDataPath;
                log.clear();
                log.open(directory / "ares-vulkan-report.log", std::ios::trunc);
            }
        }
        void write(const std::string &message)
        {
            __android_log_write(ANDROID_LOG_INFO, "ares-vulkan", message.c_str());
            if (log) log << message << std::endl;
        }
    };

    // The game thread is not the Java activity thread. Obtain its own JNIEnv.
    std::string nativeLibraryDirectory(ANativeActivity *activity)
    {
        struct Env
        {
            JavaVM *vm;
            JNIEnv *env = nullptr;
            bool attached = false;
            explicit Env(JavaVM *value) : vm(value)
            {
                const jint result = vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
                if (result == JNI_EDETACHED)
                {
                    if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK)
                        throw std::runtime_error("AttachCurrentThread failed");
                    attached = true;
                }
                else if (result != JNI_OK) throw std::runtime_error("GetEnv failed");
            }
            ~Env() { if (attached) vm->DetachCurrentThread(); }
        } scope(activity->vm);
        JNIEnv *env = scope.env;
        if (env->PushLocalFrame(8) < 0)
        {
            env->ExceptionClear();
            throw std::runtime_error("JNI local frame allocation failed");
        }
        auto check = [&]() {
            if (env->ExceptionCheck())
            {
                env->ExceptionClear();
                env->PopLocalFrame(nullptr);
                throw std::runtime_error("JNI nativeLibraryDir query failed");
            }
        };
        jclass activityClass = env->GetObjectClass(activity->clazz);
        check();
        jmethodID getInfo = env->GetMethodID(activityClass, "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;");
        check();
        jobject info = env->CallObjectMethod(activity->clazz, getInfo);
        check();
        if (!info) { env->PopLocalFrame(nullptr); throw std::runtime_error("ApplicationInfo is null"); }
        jclass infoClass = env->GetObjectClass(info);
        check();
        jfieldID field = env->GetFieldID(infoClass, "nativeLibraryDir", "Ljava/lang/String;");
        check();
        auto value = static_cast<jstring>(env->GetObjectField(info, field));
        check();
        if (!value) { env->PopLocalFrame(nullptr); throw std::runtime_error("nativeLibraryDir is null"); }
        const char *chars = env->GetStringUTFChars(value, nullptr);
        check();
        if (!chars) { env->PopLocalFrame(nullptr); throw std::runtime_error("nativeLibraryDir conversion failed"); }
        std::string result(chars);
        env->ReleaseStringUTFChars(value, chars);
        env->PopLocalFrame(nullptr);
        return result;
    }

    std::filesystem::path installDriver(ANativeActivity *activity)
    {
        AAsset *asset = AAssetManager_open(activity->assetManager, kAsset, AASSET_MODE_STREAMING);
        if (!asset) throw std::runtime_error("Bundled Turnip asset missing; run tools/ares/prepare_turnip.py before building");
        struct AssetCloser { AAsset *asset; ~AssetCloser() { AAsset_close(asset); } } closer{asset};
        if (AAsset_getLength64(asset) != kDriverBytes) throw std::runtime_error("Unexpected Turnip asset size");
        const auto directory = std::filesystem::path(activity->internalDataPath) / "ares-drivers" / "turnip-r7-sysmem";
        std::filesystem::create_directories(directory);
        const auto temporary = directory / "driver.tmp";
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("Cannot create private Turnip file");
        std::array<char, 65536> buffer;
        size_t bytes = 0;
        for (;;)
        {
            const int count = AAsset_read(asset, buffer.data(), buffer.size());
            if (count < 0) throw std::runtime_error("Turnip asset read failed");
            if (count == 0) break;
            output.write(buffer.data(), count);
            bytes += static_cast<size_t>(count);
        }
        output.flush();
        if (!output || bytes != kDriverBytes) throw std::runtime_error("Turnip file write incomplete");
        output.close();
        if (!output) throw std::runtime_error("Turnip file close failed");
        std::filesystem::rename(temporary, directory / kDriver);
        return directory;
    }

    void *initialize()
    {
        auto *app = GetAndroidApp();
        if (!app || !app->activity || !app->activity->internalDataPath)
            throw std::runtime_error("NativeActivity not ready");
        auto *activity = app->activity;
        Journal journal(activity);
        std::string requested = "turnip";
        std::ifstream preference(journal.directory / "ares-driver.txt");
        std::string option;
        if (preference >> option)
        {
            if (option == "system" || option == "turnip") requested = option;
            else journal.write("Unknown ares-driver.txt value; using turnip");
        }
        journal.write("Requested driver: " + requested + "; GS Vulkan bridge is not implemented");
        // Leave a valid partial report if the vendor driver aborts the process.
        {
            std::ofstream partial(journal.directory / "ares-vulkan-report.json", std::ios::trunc);
            partial << "{\"requested\":" << quote(requested)
                    << ",\"selected\":\"unavailable\",\"status\":\"in-progress\","
                       "\"gs_vulkan_integrated\":false,\"surface_tested\":false,\"gameplay_tested\":false}\n";
        }
        std::ostringstream attempts;
        bool first = true;
        void *selected = nullptr;
        std::string actual = "unavailable";
        auto attempt = [&](bool custom) {
            const std::string kind = custom ? "turnip" : "system";
            if (!first) attempts << ',';
            first = false;
            attempts << "{\"requested\":" << quote(kind);
            try
            {
                void *library = nullptr;
                if (custom)
                {
                    const auto driverDirectory = installDriver(activity).string() + "/";
                    const auto hookDirectory = nativeLibraryDirectory(activity);
                    journal.write("Opening private Turnip through adrenotools");
                    library = adrenotools_open_libvulkan(RTLD_NOW | RTLD_LOCAL, ADRENOTOOLS_DRIVER_CUSTOM,
                        activity->internalDataPath, hookDirectory.c_str(), driverDirectory.c_str(), kDriver, nullptr, nullptr);
                }
                else library = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
                // adrenotools namespaces and their libraries are process lifetime objects.
                if (!library) throw std::runtime_error("Vulkan loader could not be opened");
                const auto checked = AresVulkanProbe::probe(library, [&](const std::string &message) { journal.write(message); }, true);
                attempts << ",\"probe\":" << checked.json;
                if (!checked.usable) throw std::runtime_error("No device meets the preliminary mobile GS requirements");
                if (custom && !checked.turnip) throw std::runtime_error("Turnip identity not confirmed; refusing to label vendor fallback as Turnip");
                selected = library;
                actual = kind;
                attempts << ",\"accepted\":true";
            }
            catch (const std::exception &error)
            {
                journal.write(kind + " failed: " + error.what());
                attempts << ",\"accepted\":false,\"error\":" << quote(error.what());
            }
            attempts << '}';
        };
        if (requested == "turnip") attempt(true);
        if (!selected) attempt(false);
        const auto reportTemporary = journal.directory / "ares-vulkan-report.json.tmp";
        std::ofstream report(reportTemporary, std::ios::trunc);
        report << "{\"status\":\"completed\",\"requested\":" << quote(requested) << ",\"selected\":" << quote(actual)
               << ",\"bundled_driver_sha256\":" << quote(kDriverHash)
               << ",\"scope\":\"Loader identity and preliminary device creation only\","
                  "\"gs_vulkan_integrated\":false,\"surface_tested\":false,\"gameplay_tested\":false,\"attempts\":["
               << attempts.str() << "]}\n";
        report.flush();
        report.close();
        if (!report) throw std::runtime_error("Cannot write Vulkan report");
        std::filesystem::rename(reportTemporary, journal.directory / "ares-vulkan-report.json");
        journal.write("Selected driver: " + actual + "; report " + (journal.directory / "ares-vulkan-report.json").string());
        return selected;
    }
}

void *aresAndroidVulkanLibrary() noexcept
{
    static void *library = []() -> void * {
        try { return initialize(); }
        catch (const std::exception &error)
        { __android_log_write(ANDROID_LOG_ERROR, "ares-vulkan", error.what()); return nullptr; }
        catch (...) { __android_log_write(ANDROID_LOG_ERROR, "ares-vulkan", "Unknown loader error"); return nullptr; }
    }();
    return library;
}

bool aresAndroidConsumeDiagnosticRequest() noexcept
{
    try
    {
        auto *app = GetAndroidApp();
        if (!app || !app->activity || !app->activity->internalDataPath) return false;
        const auto flag = std::filesystem::path(app->activity->internalDataPath) / "ares-diagnostic-only";
        return std::filesystem::remove(flag);
    }
    catch (...) { return false; }
}

void aresAndroidRunDriverDiagnostic()
{
    auto *app = GetAndroidApp();
    if (!app) { (void)aresAndroidVulkanLibrary(); return; }
    std::atomic<bool> finished{false};
    std::thread worker([&]() {
        (void)aresAndroidVulkanLibrary();
        finished.store(true, std::memory_order_release);
    });
    while (!finished.load(std::memory_order_acquire))
    {
        int events = 0;
        android_poll_source *source = nullptr;
        const int result = ALooper_pollOnce(50, nullptr, &events, reinterpret_cast<void **>(&source));
        if (result >= 0 && source) source->process(app, source);
        if (app->destroyRequested) std::_Exit(0);
        if (result == ALOOPER_POLL_ERROR) std::this_thread::yield();
    }
    worker.join();
}
