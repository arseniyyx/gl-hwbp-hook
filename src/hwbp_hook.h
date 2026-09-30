#pragma once
#include <windows.h>
#include <cstdint>

#define HWBP_MAX_HOOKS 4

struct HwbpHookEntry {
    void* address;
    void (*callback)(CONTEXT* ctx, void* userData);
    void* userData;
};

struct HwbpHookConfig {
    HwbpHookEntry hooks[HWBP_MAX_HOOKS];
    int hookCount;
};

bool HwbpInstall(const HwbpHookConfig& cfg);
void HwbpRemove();
