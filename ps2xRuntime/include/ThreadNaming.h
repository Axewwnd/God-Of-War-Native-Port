#pragma once

#include <string_view>
#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WINAPI
#define WINAPI __stdcall
#endif

extern "C"
{
    typedef void *HANDLE;
    typedef void *HMODULE;
    typedef const wchar_t *PCWSTR;
    typedef long HRESULT;

    __declspec(dllimport) HMODULE WINAPI GetModuleHandleW(const wchar_t *lpModuleName);
    __declspec(dllimport) void *WINAPI GetProcAddress(HMODULE hModule, const char *lpProcName);
    __declspec(dllimport) HANDLE WINAPI GetCurrentThread(void);
}
#elif defined(__APPLE__) || defined(__linux__)
#include <pthread.h>
#elif defined(__SWITCH__)
// GOW-Port: ps2_switch_runtime.cpp. libnx starts every thread on the process's default core and Horizon does not
// time-slice threads of one priority, so the runtime's busy threads are spread over the three application cores.
extern "C" void ps2xSwitchPinCurrentThread(int core);
#endif

namespace ThreadNaming
{
    inline void SetCurrentThreadName(std::string_view name)
    {
#if defined(_WIN32) 
        using SetThreadDescriptionFn = HRESULT(WINAPI*)(HANDLE, PCWSTR);

        HMODULE kernel32 = ::GetModuleHandleW(L"Kernel32.dll");
        auto setThreadDescription =
            reinterpret_cast<SetThreadDescriptionFn>(::GetProcAddress(kernel32, "SetThreadDescription"));

        if (setThreadDescription)
        {
            std::wstring wname(name.begin(), name.end());
            setThreadDescription(::GetCurrentThread(), wname.c_str());
        }

#elif defined(__APPLE__)
        pthread_setname_np(name.data());

#elif defined(__linux__)
        pthread_setname_np(pthread_self(), name.data());
#elif defined(__SWITCH__)
        // EE + IOP on core 1, VIF1/VU1 on core 2; the presentation thread, the GS frontend and GL backend on core 0
        if (name == "GameThread")
            ps2xSwitchPinCurrentThread(1);
        else if (name == "GSWorker")
            ps2xSwitchPinCurrentThread(2);
        else
            ps2xSwitchPinCurrentThread(0);
#endif
    }
}
