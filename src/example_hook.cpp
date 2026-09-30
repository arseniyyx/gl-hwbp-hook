#include "hwbp_hook.h"
#include "gl_resolve.h"
#include "shared_memory.h"

typedef int GLint;
typedef int GLsizei;
typedef unsigned int GLuint;
typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef float GLfloat;

struct HookSharedData {
    volatile LONG lock;
    volatile int frameId;
    volatile int swapCount;
    volatile int uniformCalls;
    volatile int drawCalls;
    GLuint currentProgram;
};

static SharedMemory<HookSharedData> g_shm;
static GLuint g_curProgram = 0;
static int g_frameSwaps = 0;
static int g_frameUniforms = 0;
static int g_frameDraws = 0;

static void OnSwapBuffers(CONTEXT*, void*) {
    auto* s = g_shm.Get();
    if (s) {
        while (InterlockedCompareExchange(&s->lock, 1, 0) != 0) {}
        s->frameId++;
        s->swapCount = g_frameSwaps;
        s->uniformCalls = g_frameUniforms;
        s->drawCalls = g_frameDraws;
        s->currentProgram = g_curProgram;
        InterlockedExchange(&s->lock, 0);
    }
    g_frameSwaps++;
    g_frameUniforms = 0;
    g_frameDraws = 0;
}

static void OnUniformMatrix4fv(CONTEXT* ctx, void*) {
    g_frameUniforms++;
    GLint location = (GLint)ctx->Rcx;
    GLsizei count = (GLsizei)ctx->Rdx;
    GLboolean transpose = (GLboolean)ctx->R8;
    const GLfloat* value = (const GLfloat*)ctx->R9;
    (void)location; (void)count; (void)transpose; (void)value;
}

static void OnDrawElements(CONTEXT* ctx, void*) {
    g_frameDraws++;
    GLenum mode = (GLenum)ctx->Rcx;
    GLsizei count = (GLsizei)ctx->Rdx;
    GLenum type = (GLenum)ctx->R8;
    const void* indices = (const void*)ctx->R9;
    (void)mode; (void)count; (void)type; (void)indices;
}

static void OnUseProgram(CONTEXT* ctx, void*) {
    g_curProgram = (GLuint)ctx->Rcx;
}

static DWORD WINAPI HookThread(LPVOID) {
    Sleep(3000);

    wchar_t shmName[64];
    swprintf_s(shmName, 64, L"Local\\GLHook_%08X", GetCurrentProcessId());
    g_shm.Create(shmName);

    GlAddresses gl = {};
    if (!ResolveGlAddresses(gl)) return 1;

    HwbpHookConfig cfg = {};
    cfg.hookCount = 4;
    cfg.hooks[0] = { gl.swapBuffers,      OnSwapBuffers,      nullptr };
    cfg.hooks[1] = { gl.uniformMatrix4fv,  OnUniformMatrix4fv, nullptr };
    cfg.hooks[2] = { gl.drawElements,      OnDrawElements,     nullptr };
    cfg.hooks[3] = { gl.useProgram,        OnUseProgram,       nullptr };

    HwbpInstall(cfg);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hInst);
        CreateThread(nullptr, 0, HookThread, nullptr, 0, nullptr);
    }
    if (reason == DLL_PROCESS_DETACH) {
        HwbpRemove();
        g_shm.Close();
    }
    return TRUE;
}
