#include "shared_memory.h"
#include <cstdio>
#include <cstdlib>

struct HookSharedData {
    volatile LONG lock;
    volatile int frameId;
    volatile int swapCount;
    volatile int uniformCalls;
    volatile int drawCalls;
    unsigned int currentProgram;
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: reader <PID>\n");
        return 1;
    }

    DWORD pid = (DWORD)atoi(argv[1]);
    wchar_t shmName[64];
    swprintf_s(shmName, 64, L"Local\\GLHook_%08X", pid);

    SharedMemory<HookSharedData> shm;
    if (!shm.Open(shmName)) {
        printf("Failed to open shared memory for PID %u\n", pid);
        return 1;
    }

    printf("Connected to PID %u. Press Ctrl+C to stop.\n\n", pid);

    int lastFrame = -1;
    while (true) {
        auto* s = shm.Get();
        if (s->frameId != lastFrame) {
            lastFrame = s->frameId;
            printf("Frame %d | Swaps: %d | Uniforms: %d | Draws: %d | Program: %u\n",
                s->frameId, s->swapCount, s->uniformCalls, s->drawCalls, s->currentProgram);
        }
        Sleep(16);
    }

    return 0;
}
