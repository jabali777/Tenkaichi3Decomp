# Local measurements

These results come from existing logs dated October 25, 2026, before repository preparation. They were not repeated during this review.

System: Pentium B950, Intel HD Graphics 2000 (SNB GT1), OpenGL 3.3 and Mesa 26.2.5, according to local diagnostics. The approximately 180-second session was replayed with the same controls and copies of the initial saves.

| Configuration | Ultimate attack (FPS) | Many effects (FPS) |
| --- | ---: | ---: |
| Previous profile, enlarged window | 18.46 | 12.35 |
| Reduced profile, 640 × 480 window | 28.98 | 28.00 |
| Reduced profile repeat, 640 × 480 | 29.22 | 27.45 |

Historical tests disabled interpolation. The current profile enables it and can present up to 60 FPS; these figures do not measure that option. Combat simulation remains at 30 Hz.

The reduced profile combines 256 × 224 internal resolution, a small window, simple shading, disabled effects, and no extra texture workers. Several conditions change in the comparison, so the entire improvement cannot be attributed to one setting. Neither reducing only the window nor enlarging the reduced profile window reproduced the improvement. Load spikes of up to 171 ms remain.

The local summary reported no OpenGL errors in completed repeats. This does not demonstrate stable 30 FPS, general support for other GPUs, or online compatibility. The profile sacrifices HUD detail, shading, and character shadows.

Complete logs and controls remain in the private installation, `performance-diagnostics/`, and are not distributed. These are historical results, not a benchmark reproducible from the repository alone.

For a new comparison, record a local session, keep the window, resolution, seed, and duration consistent, and repeat the same scene several times. Separate initial loading spikes from sustained performance and document every change.
