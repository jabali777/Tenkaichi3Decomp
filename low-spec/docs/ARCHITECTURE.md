# Architecture and compatibility

`play.sh` selects a profile and `play-fast.sh` configures the environment before starting the port. `BT3_INTERP=1` requests interpolated frames: simulation remains at 30 Hz and presentation can reach 60 FPS. Set `BT3_INTERP=0` to disable it. This does not modify the executable. `BT3_OPTIMIZACION_GL=0` disables injection of both adapters for that launch.

`bt3_opengl_fast.c` intercepts `SDL_GL_GetProcAddress` through `LD_PRELOAD`. It sends uniforms to aligned regions of a 1 MiB buffer, renews its storage when full, and avoids duplicate writes. Simple shading identifies shaders by content and checks uniform layout before skipping certain passes. Unrecognized shaders retain their draws.

`bt3_reduced_resolution.c` halves eligible 1024 × 1024 OpenGL targets. It adjusts viewport, scissor, copies, pixel reads, and GS shader coordinates; the usual effective image changes from 512 × 448 to 256 × 224. Resource textures and the window framebuffer are retained.

Both modules use global state and are intended for the current renderer with `BT3_GS_THREAD=0`. They are not a general OpenGL optimization library; multiple contexts and concurrent rendering have not been validated. Changes to shaders, render targets, or port calls require renewed validation.

The inspected installation contains a Linux x86-64 ELF with Build ID:

```text
26afae723ddea8232349cdb75b346e23c107d9b9
```

The executable could not be associated with a source tag. The local installation has modified data from a Latin American edition; this does not establish that edition's compatibility with the port, and its data is not included here.

`record_attacks.py` copies saves and settings before starting the process, records controls and timings, and reads Linux sensors when available. Missing sensors leave empty cells. Results may contain local paths and save copies; they are local files excluded from Git.
