#include "hwbp_hook.h"
#include <tlhelp32.h>

static HwbpHookConfig g_cfg = {};
static PVOID g_vehHandle = nullptr;

static LONG CALLBACK VehHandler(EXCEPTION_POINTERS* ep) {
    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP)
        return EXCEPTION_CONTINUE_SEARCH;

    void* addr = ep->ExceptionRecord->ExceptionAddress;
    CONTEXT* ctx = ep->ContextRecord;

    for (int i = 0; i < g_cfg.hookCount; i++) {
        if (addr == g_cfg.hooks[i].address) {
            g_cfg.hooks[i].callback(ctx, g_cfg.hooks[i].userData);
            ctx->EFlags |= 0x10000;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

static void SetDrRegs(CONTEXT* ctx) {
    DWORD64* dr[] = { &ctx->Dr0, &ctx->Dr1, &ctx->Dr2, &ctx->Dr3 };
    ctx->Dr7 &= ~0xFFu;
    for (int i = 0; i < g_cfg.hookCount && i < 4; i++) {
        *dr[i] = (DWORD64)g_cfg.hooks[i].address;
        ctx->Dr7 |= (1u << (i * 2));
    }
}

static void ApplyToAllThreads() {
    DWORD pid = GetCurrentProcessId();
    DWORD myTid = GetCurrentThreadId();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;

    THREADENTRY32 te = {};
    te.dwSize = sizeof(te);
    if (Thread32First(snap, &te)) {
        do {
            if (te.th32OwnerProcessID == pid && te.th32ThreadID != myTid) {
                HANDLE ht = OpenThread(
                    THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
                    FALSE, te.th32ThreadID);
                if (ht) {
                    SuspendThread(ht);
                    CONTEXT ctx = {};
                    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
                    if (GetThreadContext(ht, &ctx)) {
                        SetDrRegs(&ctx);
                        SetThreadContext(ht, &ctx);
                    }
                    ResumeThread(ht);
                    CloseHandle(ht);
                }
            }
        } while (Thread32Next(snap, &te));
    }
    CloseHandle(snap);
}

bool HwbpInstall(const HwbpHookConfig& cfg) {
    if (cfg.hookCount <= 0 || cfg.hookCount > HWBP_MAX_HOOKS) return false;
    for (int i = 0; i < cfg.hookCount; i++) {
        if (!cfg.hooks[i].address || !cfg.hooks[i].callback) return false;
    }

    g_cfg = cfg;
    g_vehHandle = AddVectoredExceptionHandler(1, VehHandler);
    if (!g_vehHandle) return false;

    ApplyToAllThreads();
    return true;
}

void HwbpRemove() {
    if (g_vehHandle) {
        RemoveVectoredExceptionHandler(g_vehHandle);
        g_vehHandle = nullptr;
    }
    g_cfg = {};
}
