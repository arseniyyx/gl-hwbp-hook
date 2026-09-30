# gl-hwbp-hook

Hardware breakpoint hooking framework for OpenGL applications on Windows x64.

Uses debug registers (DR0-DR3) and a Vectored Exception Handler to intercept OpenGL function calls without modifying any code in memory. Communicates intercepted data to an external process via shared memory.

## How it works

1. DLL is injected into a target process that uses OpenGL
2. `gl_resolve.h` finds OpenGL function addresses via `wglGetProcAddress` (creates a temp GL context if needed)
3. `hwbp_hook` sets hardware breakpoints (DR0-DR3) on up to 4 function addresses across all threads
4. VEH handler catches `EXCEPTION_SINGLE_STEP`, identifies which hook triggered, calls your callback with the thread's `CONTEXT*`
5. Callback reads function arguments from registers (x64 ABI: RCX, RDX, R8, R9)
6. `shared_memory.h` provides a typed shared memory channel for IPC with an external reader process

## Files

| File | Description |
|------|-------------|
| `src/hwbp_hook.h/cpp` | Generic hardware breakpoint hook engine (up to 4 hooks) |
| `src/shared_memory.h` | Template for named shared memory IPC |
| `src/gl_resolve.h` | Resolves OpenGL extension function addresses |
| `src/example_hook.cpp` | Example DLL — hooks SwapBuffers, UniformMatrix4fv, DrawElements, UseProgram |
| `src/reader_example.cpp` | Example reader — connects to shared memory and prints frame stats |

## Build

```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Outputs:
- `build/Release/hwbp_hook.lib` — static library
- `build/Release/example_hook.dll` — example hook DLL
- `build/Release/reader_example.exe` — example reader

## Usage

1. Inject `example_hook.dll` into an OpenGL process
2. Run `reader_example.exe <PID>` to see frame stats
3. Build your own hook by linking `hwbp_hook.lib` and writing callbacks

## Limitations

- x64 Windows only (uses DR0-DR3 debug registers)
- Max 4 simultaneous hooks (hardware limit)
- Target must use OpenGL (for `gl_resolve.h`; `hwbp_hook` itself works with any function)
- Anti-cheat software may monitor debug registers
- New threads created after hook installation won't have breakpoints set (call `HwbpInstall` again or set DR on new threads manually)

## License

MIT
