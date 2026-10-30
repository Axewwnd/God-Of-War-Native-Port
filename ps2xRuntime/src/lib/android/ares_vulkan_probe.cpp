#include "ares_vulkan_probe.h"
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
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

    std::string version(uint32_t api)
    {
        return std::to_string(VK_VERSION_MAJOR(api)) + "." + std::to_string(VK_VERSION_MINOR(api)) + "." +
               std::to_string(VK_VERSION_PATCH(api));
    }

}

namespace AresVulkanProbe {
    Result probe(void *library, const std::function<void(const std::string &)> &log, bool androidSurface)
    {
        Result result;
        auto get = reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(library, "vkGetInstanceProcAddr"));
        if (!get) throw std::runtime_error("Loader missing vkGetInstanceProcAddr");
        auto enumerateVersion = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(get(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"));
        uint32_t loaderApi = VK_API_VERSION_1_0;
        if (enumerateVersion && enumerateVersion(&loaderApi) != VK_SUCCESS)
            throw std::runtime_error("vkEnumerateInstanceVersion failed");
        log("Loader API " + version(loaderApi));
        if (loaderApi < VK_API_VERSION_1_1) throw std::runtime_error("Vulkan 1.1 loader required for this probe");
        auto create = reinterpret_cast<PFN_vkCreateInstance>(get(VK_NULL_HANDLE, "vkCreateInstance"));
        if (!create) throw std::runtime_error("Loader missing vkCreateInstance");
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.pApplicationName = "ARES driver capability probe";
        app.apiVersion = VK_API_VERSION_1_1;
        VkInstanceCreateInfo createInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        createInfo.pApplicationInfo = &app;
        // VK_KHR_swapchain on the test device depends on VK_KHR_surface.
        const char *instanceExtensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, "VK_KHR_android_surface"};
        createInfo.enabledExtensionCount = androidSurface ? 2 : 1;
        createInfo.ppEnabledExtensionNames = instanceExtensions;
        VkInstance instance = VK_NULL_HANDLE;
        VkResult status = create(&createInfo, nullptr, &instance);
        if (status != VK_SUCCESS) throw std::runtime_error("vkCreateInstance returned " + std::to_string(status));
        auto destroy = reinterpret_cast<PFN_vkDestroyInstance>(get(instance, "vkDestroyInstance"));
        if (!destroy) throw std::runtime_error("Instance missing vkDestroyInstance");
        struct InstanceCloser { VkInstance instance; PFN_vkDestroyInstance destroy; ~InstanceCloser() { destroy(instance, nullptr); } } closer{instance, destroy};
        auto proc = [&](const char *name) {
            auto address = get(instance, name);
            if (!address) throw std::runtime_error(std::string("Instance missing ") + name);
            return address;
        };
        auto enumerate = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(proc("vkEnumeratePhysicalDevices"));
        auto properties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(proc("vkGetPhysicalDeviceProperties"));
        auto properties2 = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(proc("vkGetPhysicalDeviceProperties2"));
        auto features = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures>(proc("vkGetPhysicalDeviceFeatures"));
        auto formats = reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties>(proc("vkGetPhysicalDeviceFormatProperties"));
        auto extensions = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(proc("vkEnumerateDeviceExtensionProperties"));
        auto queueProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(proc("vkGetPhysicalDeviceQueueFamilyProperties"));
        auto createDevice = reinterpret_cast<PFN_vkCreateDevice>(proc("vkCreateDevice"));
        auto getDeviceProc = reinterpret_cast<PFN_vkGetDeviceProcAddr>(proc("vkGetDeviceProcAddr"));
        uint32_t count = 0;
        if (enumerate(instance, &count, nullptr) != VK_SUCCESS || count == 0)
            throw std::runtime_error("Vulkan reports no physical devices");
        std::vector<VkPhysicalDevice> devices(count);
        if (enumerate(instance, &count, devices.data()) != VK_SUCCESS)
            throw std::runtime_error("Device enumeration failed");
        std::ostringstream report;
        report << "{\"loader_api\":" << quote(version(loaderApi)) << ",\"devices\":[";
        for (uint32_t index = 0; index < count; ++index)
        {
            VkPhysicalDeviceProperties props{};
            properties(devices[index], &props);
            VkPhysicalDeviceFeatures support{};
            features(devices[index], &support);
            uint32_t extensionCount = 0;
            if (extensions(devices[index], nullptr, &extensionCount, nullptr) != VK_SUCCESS)
                throw std::runtime_error("Device extension count failed");
            std::vector<VkExtensionProperties> extensionList(extensionCount);
            if (extensions(devices[index], nullptr, &extensionCount, extensionList.data()) != VK_SUCCESS)
                throw std::runtime_error("Device extension enumeration failed");
            auto has = [&](const char *name) {
                return std::any_of(extensionList.begin(), extensionList.end(), [&](const auto &entry) { return std::strcmp(entry.extensionName, name) == 0; });
            };
            VkPhysicalDeviceDriverProperties driver{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
            if (props.apiVersion >= VK_API_VERSION_1_2 || has(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME))
            {
                VkPhysicalDeviceProperties2 chain{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
                chain.pNext = &driver;
                properties2(devices[index], &chain);
            }
            const bool isTurnip = driver.driverID == VK_DRIVER_ID_MESA_TURNIP;
            log(std::string("Enumerated GPU ") + props.deviceName + "; driver " + driver.driverName);
            VkFormatProperties r32{};
            formats(devices[index], VK_FORMAT_R32_UINT, &r32);
            uint32_t queueCount = 0;
            queueProperties(devices[index], &queueCount, nullptr);
            std::vector<VkQueueFamilyProperties> queues(queueCount);
            queueProperties(devices[index], &queueCount, queues.data());
            int selectedQueue = -1;
            for (uint32_t q = 0; q < queueCount; ++q)
                if (queues[q].queueCount && (queues[q].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) ==
                    (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) { selectedQueue = static_cast<int>(q); break; }
            VkResult deviceStatus = VK_ERROR_FEATURE_NOT_PRESENT;
            if (selectedQueue >= 0 && support.fragmentStoresAndAtomics && has(VK_KHR_SWAPCHAIN_EXTENSION_NAME))
            {
                const float priority = 1.0f;
                VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
                queue.queueFamilyIndex = static_cast<uint32_t>(selectedQueue);
                queue.queueCount = 1;
                queue.pQueuePriorities = &priority;
                VkPhysicalDeviceFeatures requested{};
                requested.fragmentStoresAndAtomics = VK_TRUE;
                const char *swapchain = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
                VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
                deviceInfo.queueCreateInfoCount = 1;
                deviceInfo.pQueueCreateInfos = &queue;
                deviceInfo.enabledExtensionCount = 1;
                deviceInfo.ppEnabledExtensionNames = &swapchain;
                deviceInfo.pEnabledFeatures = &requested;
                VkDevice device = VK_NULL_HANDLE;
                deviceStatus = createDevice(devices[index], &deviceInfo, nullptr, &device);
                if (deviceStatus == VK_SUCCESS)
                {
                    auto destroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(getDeviceProc(device, "vkDestroyDevice"));
                    if (!destroyDevice) throw std::runtime_error("Device missing vkDestroyDevice");
                    destroyDevice(device, nullptr);
                    result.usable = true;
                    result.turnip = result.turnip || isTurnip;
                }
            }
            log(std::string("GPU ") + props.deviceName + "; Vulkan " + version(props.apiVersion) +
                          "; driver " + driver.driverName + "; device result " + std::to_string(deviceStatus));
            if (index) report << ',';
            report << "{\"name\":" << quote(props.deviceName) << ",\"api\":" << quote(version(props.apiVersion))
                   << ",\"vendor_id\":" << props.vendorID << ",\"device_id\":" << props.deviceID
                   << ",\"driver_id\":" << driver.driverID << ",\"driver_name\":" << quote(driver.driverName)
                   << ",\"driver_info\":" << quote(driver.driverInfo) << ",\"turnip_identity\":" << (isTurnip ? "true" : "false")
                   << ",\"fragment_stores_and_atomics\":" << (support.fragmentStoresAndAtomics ? "true" : "false")
                   << ",\"graphics_compute_queue\":" << selectedQueue
                   << ",\"logical_device_result\":" << deviceStatus << ",\"extensions\":[";
            for (size_t ext = 0; ext < extensionList.size(); ++ext)
            { if (ext) report << ','; report << quote(extensionList[ext].extensionName); }
            report << "],\"limits\":{\"max_storage_buffer_range\":" << props.limits.maxStorageBufferRange
                   << ",\"max_per_stage_storage_buffers\":" << props.limits.maxPerStageDescriptorStorageBuffers
                   << ",\"max_per_stage_input_attachments\":" << props.limits.maxPerStageDescriptorInputAttachments
                   << ",\"max_compute_workgroup_invocations\":" << props.limits.maxComputeWorkGroupInvocations
                   << ",\"max_compute_workgroup_size\":[" << props.limits.maxComputeWorkGroupSize[0] << ','
                   << props.limits.maxComputeWorkGroupSize[1] << ',' << props.limits.maxComputeWorkGroupSize[2]
                   << "],\"max_image_dimension_2d\":" << props.limits.maxImageDimension2D
                   << ",\"r32_uint_optimal_tiling_features\":" << r32.optimalTilingFeatures << "}}";
        }
        report << "]}";
        result.json = report.str();
        return result;
    }

}
