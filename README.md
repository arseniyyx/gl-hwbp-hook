# gl-hwbp-hook

Hardware breakpoint hooking for OpenGL on Windows x64.

Intercepts OpenGL calls via debug registers (DR0–DR3) + Vectored Exception Handler — zero code patching in target memory. Sends intercepted data to an external process through named shared memory.

```
┌──────────────────────────────────────────────────────────┐
│                   TARGET PROCESS (OpenGL)                 │
│                                                          │
│  ┌─────────────┐    DR0 ──► wglSwapBuffers               │
│  │  example_    │    DR1 ──► glUniformMatrix4fv           │
│  │  hook.dll    │    DR2 ──► glDrawElements               │
│  │             │    DR3 ──► glUseProgram                  │
│  │  VEH Handler├───────────────────────┐                  │
│  └──────┬──────┘                       │                  │
│         │ writes                       │ EXCEPTION_       │
│         ▼                              │ SINGLE_STEP      │
│  ┌─────────────┐         ┌─────────────┘                  │
│  │ Shared Mem  │         │ callback(CONTEXT* ctx)         │
│  │ (named)     │         │   RCX = arg1                   │
│  └──────┬──────┘         │   RDX = arg2                   │
│         │                │   R8  = arg3                   │
│         │                │   R9  = arg4                   │
└─────────┼────────────────┴───────────────────────────────┘
          │ FILE_MAP
          ▼
┌──────────────────────┐
│   READER PROCESS     │
│   reader_example.exe │
│                      │
│   Frame 1042         │
│   Draws: 3847        │
│   Uniforms: 12450    │
│   Program: 96        │
└──────────────────────┘
```

## Why hardware breakpoints?

| Method | Detected by | Modifies memory |
|--------|-------------|-----------------|
| IAT hook | import table integrity check | yes |
| Inline hook (jmp patch) | function prologue byte scan | yes |
| **HWBP (this project)** | reading DR0-DR7 | **no** |

Debug registers live in the CPU, they don't touch process memory at all. The only way to detect them is `GetThreadContext` + checking DR0-DR3.

## Project structure

```
src/
├── hwbp_hook.h/.cpp      — generic hook engine (not tied to OpenGL)
├── shared_memory.h       — typed shared memory IPC template
├── gl_resolve.h          — resolves GL extension addresses via wglGetProcAddress
├── example_hook.cpp      — example DLL (hooks 4 GL functions)
└── reader_example.cpp    — example reader (connects to shared memory, prints frame stats)
```

## Build

```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Output:
- `hwbp_hook.lib` — static library
- `example_hook.dll` — example hook DLL
- `reader_example.exe` — example shared memory reader

## Usage

### Your own hook in 30 lines

```cpp
#include "hwbp_hook.h"
#include "gl_resolve.h"

static void OnSwap(CONTEXT* ctx, void*) {
    // called every frame on wglSwapBuffers
}

static void OnDraw(CONTEXT* ctx, void*) {
    // GLsizei count = (GLsizei)ctx->Rdx;
}

DWORD WINAPI Init(LPVOID) {
    Sleep(3000); // wait for GL to initialize

    GlAddresses gl = {};
    if (!ResolveGlAddresses(gl)) return 1;

    HwbpHookConfig cfg = {};
    cfg.hookCount = 2;
    cfg.hooks[0] = { gl.swapBuffers,  OnSwap, nullptr };
    cfg.hooks[1] = { gl.drawElements, OnDraw, nullptr };

    HwbpInstall(cfg);
    return 0;
}
```

### Shared memory IPC

```cpp
// Inside DLL (target process):
SharedMemory<MyData> shm;
shm.Create(L"Local\\MyHook_12345");
shm.Get()->frameCount = 42;

// External reader process:
SharedMemory<MyData> shm;
shm.Open(L"Local\\MyHook_12345");
printf("frames: %d\n", shm.Get()->frameCount);
```

## x64 calling convention reference

The VEH callback receives the full thread `CONTEXT*`. Function arguments follow the Windows x64 ABI:

| Register | Argument |
|----------|----------|
| RCX | 1st (int/ptr) |
| RDX | 2nd |
| R8 | 3rd |
| R9 | 4th |
| XMM0-3 | 1st-4th (float/double) |
| Stack | 5th+ |

Example — `glUniformMatrix4fv(location, count, transpose, value)`:
- `ctx->Rcx` = location
- `ctx->Rdx` = count
- `ctx->R8` = transpose
- `ctx->R9` = pointer to float[16] matrix

## Limitations

- Windows x64 only
- Max 4 hooks at a time (CPU hardware limit, DR0-DR3)
- `gl_resolve.h` is OpenGL-specific, but `hwbp_hook` itself works with any function address
- Threads spawned after hook installation won't have breakpoints set automatically
- Anti-cheat can read debug registers via `GetThreadContext`

## License

MIT
