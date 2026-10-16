// GOW-Port: Nintendo Switch homebrew start-up (libnx calls userAppInit/userAppExit around main()).
// - Logs: stdout/stderr go to `nxlink -s` on the PC when a host listens, else to sdmc:/switch/gow/log.txt (also what
//   an emulator such as Citron shows in its virtual SD card).
// - The settings play.cmd sets on Windows are defaults here (no shell to set them): GS work on its worker thread, the
//   asynchronous GS FINISH, and the memory card file next to the game data.
#if defined(PLATFORM_SWITCH)

#include <switch.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <unistd.h>

extern "C"
{
    // Crashes (data aborts, bad jumps): libnx calls this on its own stack instead of ending the process silently. The
    // log gets the exception and the registers as offsets in the ELF (addr2line -e ps2EntryRunner.elf <offset>).
    extern char __start__;
    alignas(16) u8 __nx_exception_stack[0x8000];
    u64 __nx_exception_stack_size = sizeof(__nx_exception_stack);

    void __libnx_exception_handler(ThreadExceptionDump *ctx)
    {
        const u64 base = reinterpret_cast<u64>(&__start__);
        std::fprintf(stderr, "[switch] EXCEPTION desc=%u pc=%#lx (elf+%#lx) lr=%#lx (elf+%#lx) far=%#lx sp=%#lx esr=%#x\n",
                     ctx->error_desc, ctx->pc.x, ctx->pc.x - base, ctx->lr.x, ctx->lr.x - base, ctx->far.x, ctx->sp.x, ctx->esr);
        for (int i = 0; i < 29; i += 4)
            std::fprintf(stderr, "[switch]   x%-2d=%#lx x%-2d=%#lx x%-2d=%#lx x%-2d=%#lx\n", i, ctx->cpu_gprs[i].x, i + 1,
                         i + 1 < 29 ? ctx->cpu_gprs[i + 1].x : 0ul, i + 2, i + 2 < 29 ? ctx->cpu_gprs[i + 2].x : 0ul, i + 3,
                         i + 3 < 29 ? ctx->cpu_gprs[i + 3].x : 0ul);
        std::fflush(stderr);
        svcExitProcess();
    }

    // ThreadNaming.h: pin the calling thread to one application core (0-2)
    void ps2xSwitchPinCurrentThread(int core)
    {
        const Result rc = svcSetThreadCoreMask(CUR_THREAD_HANDLE, core, 1u << core);
        std::fprintf(stderr, "[switch] thread pinned to core %d%s\n", core, R_SUCCEEDED(rc) ? "" : " (failed)");
    }

    // Thread stacks: libnx gives every pthread (std::thread included) 128 KiB unless a size is set; the runtime's
    // threads were sized by the 1 MiB Windows default (the VIF1/VU1 worker overflowed 128 KiB on its first VU1 job:
    // a silent jump to 0). Linked with --wrap=pthread_create (CMakeLists.txt), threads without an explicit stack size
    // get 1 MiB; explicit sizes (SDL's) are kept.
    int __real_pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start)(void *), void *arg);

    int __wrap_pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start)(void *), void *arg)
    {
        constexpr size_t hostThreadStack = 1u << 20;
        size_t size = 0;
        if (attr)
            pthread_attr_getstacksize(attr, &size);
        if (size != 0)
            return __real_pthread_create(thread, attr, start, arg);
        pthread_attr_t sized;
        if (attr)
            sized = *attr;
        else
            pthread_attr_init(&sized);
        pthread_attr_setstacksize(&sized, hostThreadStack);
        const int result = __real_pthread_create(thread, &sized, start, arg);
        if (!attr)
            pthread_attr_destroy(&sized);
        return result;
    }

    static int s_nxlinkSocket = -1;

    void userAppInit(void)
    {
        if (R_SUCCEEDED(socketInitializeDefault()))
            s_nxlinkSocket = nxlinkStdio();
        if (s_nxlinkSocket < 0)
        {
            // one file descriptor for both streams (two opens of the file would overwrite each other's lines)
            if (std::freopen("sdmc:/switch/gow/log.txt", "w", stdout))
                dup2(fileno(stdout), fileno(stderr));
            std::setvbuf(stdout, nullptr, _IONBF, 0);
            std::setvbuf(stderr, nullptr, _IONBF, 0);
        }
        // sdmc:/switch/gow/env.txt: KEY=VALUE lines (# comments) applied first, like the `set` lines of play.cmd
        if (std::FILE *env = std::fopen("sdmc:/switch/gow/env.txt", "r"))
        {
            char line[512];
            while (std::fgets(line, sizeof(line), env))
            {
                char *end = line + std::strcspn(line, "\r\n");
                *end = '\0';
                char *equals = std::strchr(line, '=');
                if (line[0] == '#' || !equals)
                    continue;
                *equals = '\0';
                setenv(line, equals + 1, 1);
                std::fprintf(stderr, "[switch] env %s=%s\n", line, equals + 1);
            }
            std::fclose(env);
        }
        setenv("PS2X_GS_WORKER", "1", 0);
        setenv("GOW_GS_FINISH_ASINCRONO", "1", 0);
        setenv("GOW_MC0", "sdmc:/switch/gow/play.mcd", 0);
    }

    void userAppExit(void)
    {
        std::fflush(stdout);
        std::fflush(stderr);
        if (s_nxlinkSocket >= 0)
            close(s_nxlinkSocket);
        socketExit();
    }
}

#endif // PLATFORM_SWITCH
