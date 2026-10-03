# Capture matching Carousel frames

Capture both xemu instances at one Halo Xbox debug **2276** game tick, using
the freshly rendered backbuffer. Save both full frames and two player crops
per instance, plus a JSON manifest of the alignment checks.

1. Start **Carousel → Slayer Pro**, with **two local players on each instance**,
   through the normal multiplayer menus. Avoid controller input during capture.
2. Use Windows Python with **Pillow**, plus **GDB and `rtk` in WSL**. Both xemu
   QMP servers must be reachable on localhost (client **4444**, host **4446**).
3. From the repository root, run this workstation's configured Python:

```powershell
rtk C:/Users/stian/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe tools/xbox/capture_matching_frame.py --align-carousel
```

`--align-carousel` sets the saved positions/facing, selects assault rifles,
and resets first-person weapon presentation. It changes live test state and
resumes both instances after capture. It does not establish whole-match equality.

Find the PNGs and `manifest.json` in `tmp/frame-comparison-captures/<timestamp>/`.
If the requested tick is skipped, rerun the command. Use `--output PATH` for a
new output directory, or `--client-qmp PORT` / `--host-qmp PORT` for other ports.

The accompanying `_capture_matching_frame_gdb.py` is launched automatically.
See [the detailed procedure](../../docs/carousel-frame-capture.md) for the
capture boundary, saved poses, runtime changes, and limitations.
