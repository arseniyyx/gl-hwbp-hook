# gl-hwbp-hook

Hardware breakpoint hooking for OpenGL on Windows x64.

Перехват OpenGL-вызовов через debug-регистры (DR0–DR3) + VEH — без патчинга кода в памяти. Данные передаются внешнему процессу через shared memory.

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

## Почему hardware breakpoints?

| Метод | Детектится через | Меняет память |
|-------|-----------------|---------------|
| IAT hook | проверка таблицы импорта | да |
| Inline hook (jmp patch) | проверка байтов функции | да |
| **HWBP (этот проект)** | чтение DR0-DR7 | **нет** |

DR-регистры — часть CPU, не трогают память процесса. Единственный способ обнаружить — `GetThreadContext` и проверить DR0-DR3.

## Структура

```
src/
├── hwbp_hook.h/.cpp      — движок хуков (generic, не привязан к OpenGL)
├── shared_memory.h       — шаблон shared memory IPC
├── gl_resolve.h          — резолв адресов GL-функций через wglGetProcAddress
├── example_hook.cpp      — пример DLL (хукает 4 GL-функции)
└── reader_example.cpp    — пример reader (читает shared memory, печатает статы)
```

## Сборка

```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

На выходе:
- `hwbp_hook.lib` — статическая либа
- `example_hook.dll` — пример хук-DLL
- `reader_example.exe` — пример reader

## Использование

### Свой хук за 30 строк

```cpp
#include "hwbp_hook.h"
#include "gl_resolve.h"

static void OnSwap(CONTEXT* ctx, void*) {
    // вызывается каждый кадр вместо wglSwapBuffers
}

static void OnDraw(CONTEXT* ctx, void*) {
    // GLsizei count = (GLsizei)ctx->Rdx;
}

DWORD WINAPI Init(LPVOID) {
    Sleep(3000); // ждём пока GL инициализируется

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
// В DLL (внутри таргета):
SharedMemory<MyData> shm;
shm.Create(L"Local\\MyHook_12345");
shm.Get()->frameCount = 42;

// В reader (внешний процесс):
SharedMemory<MyData> shm;
shm.Open(L"Local\\MyHook_12345");
printf("frames: %d\n", shm.Get()->frameCount);
```

## x64 calling convention

VEH даёт полный `CONTEXT*` потока. Аргументы функций по Windows x64 ABI:

| Регистр | Аргумент |
|---------|----------|
| RCX | 1-й (int/ptr) |
| RDX | 2-й |
| R8 | 3-й |
| R9 | 4-й |
| XMM0-3 | 1-4й (float/double) |
| Stack | 5+ |

Пример — `glUniformMatrix4fv(location, count, transpose, value)`:
- `ctx->Rcx` = location
- `ctx->Rdx` = count
- `ctx->R8` = transpose
- `ctx->R9` = pointer to float[16] matrix

## Ограничения

- Только Windows x64
- Максимум 4 хука одновременно (аппаратный лимит DR0-DR3)
- `gl_resolve.h` — для OpenGL, но сам `hwbp_hook` работает с любой функцией
- Потоки созданные после установки хуков не получат breakpoints автоматически
- Античит может читать debug-регистры через `GetThreadContext`

## License

MIT
