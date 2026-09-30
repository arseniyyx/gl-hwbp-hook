#pragma once
#include <windows.h>

typedef void*(APIENTRY* PFN_wglGetProcAddress)(const char*);
typedef BOOL(APIENTRY* PFN_wglSwapBuffers)(HDC);

struct GlAddresses {
    void* swapBuffers;
    void* uniformMatrix4fv;
    void* drawElements;
    void* useProgram;
};

inline bool ResolveGlAddresses(GlAddresses& out) {
    HMODULE hGL = GetModuleHandleA("opengl32.dll");
    if (!hGL) return false;

    out.swapBuffers = (void*)GetProcAddress(hGL, "wglSwapBuffers");
    if (!out.swapBuffers) return false;

    auto pGetProc = (PFN_wglGetProcAddress)GetProcAddress(hGL, "wglGetProcAddress");
    if (!pGetProc) return false;

    out.uniformMatrix4fv = (void*)pGetProc("glUniformMatrix4fv");
    out.drawElements = (void*)pGetProc("glDrawElements");
    out.useProgram = (void*)pGetProc("glUseProgram");

    if (!out.uniformMatrix4fv || !out.drawElements || !out.useProgram) {
        typedef HDC(WINAPI* PFN_wglGetCurrentDC)();
        typedef HGLRC(WINAPI* PFN_wglGetCurrentContext)();
        typedef HGLRC(WINAPI* PFN_wglCreateContext)(HDC);
        typedef BOOL(WINAPI* PFN_wglMakeCurrent)(HDC, HGLRC);
        typedef BOOL(WINAPI* PFN_wglDeleteContext)(HGLRC);

        auto pGetDC = (PFN_wglGetCurrentDC)GetProcAddress(hGL, "wglGetCurrentDC");
        auto pGetCtx = (PFN_wglGetCurrentContext)GetProcAddress(hGL, "wglGetCurrentContext");
        auto pCreate = (PFN_wglCreateContext)GetProcAddress(hGL, "wglCreateContext");
        auto pMake = (PFN_wglMakeCurrent)GetProcAddress(hGL, "wglMakeCurrent");
        auto pDel = (PFN_wglDeleteContext)GetProcAddress(hGL, "wglDeleteContext");
        if (!pCreate || !pMake || !pDel) return false;

        HWND wnd = CreateWindowExA(0, "STATIC", "", WS_OVERLAPPED,
            0, 0, 1, 1, nullptr, nullptr, nullptr, nullptr);
        if (!wnd) return false;
        HDC dc = GetDC(wnd);

        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize = sizeof(pfd); pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
        pfd.iPixelType = PFD_TYPE_RGBA;
        SetPixelFormat(dc, ChoosePixelFormat(dc, &pfd), &pfd);

        HDC savedDC = pGetDC ? pGetDC() : nullptr;
        HGLRC savedCtx = pGetCtx ? pGetCtx() : nullptr;

        HGLRC ctx = pCreate(dc);
        if (ctx) {
            pMake(dc, ctx);
            out.uniformMatrix4fv = (void*)pGetProc("glUniformMatrix4fv");
            out.drawElements = (void*)pGetProc("glDrawElements");
            out.useProgram = (void*)pGetProc("glUseProgram");
            pMake(savedDC, savedCtx);
            pDel(ctx);
        }

        ReleaseDC(wnd, dc);
        DestroyWindow(wnd);
    }

    return out.uniformMatrix4fv && out.drawElements && out.useProgram;
}
