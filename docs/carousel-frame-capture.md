# Carousel captures at a shared render event

Start with [the short usage guide](../tools/xbox/CAPTURE_MATCHING_FRAME.md). The earlier
near-simultaneous QMP pauses aligned wall-clock time only. They captured different
game ticks. Even equal ticks can show different spawns, facing and weapon phases.

## Reproduce the comparison

Start Carousel through the normal multiplayer menus on both xemu instances.
Choose Slayer Pro and two local players on each. QMP defaults are client 4444
and host 4446. The driver runs on Windows and requires WSL GDB, WSL `rtk`, and a
Windows Python with Pillow. On this workstation:

```powershell
rtk C:/Users/stian/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe tools/xbox/capture_matching_frame.py --align-carousel
```

Files are saved under `tmp/frame-comparison-captures/YYYYMMDD-HHMMSS/`:

- `client.png`, `host.png`: the two complete split-screen rendered frames.
- `client-player1.png`, `client-player2.png`, `host-player1.png`,
  `host-player2.png`: crops of those same frames, with no additional game advance.
- `manifest.json`: tick, poses, variant bytes, presentation counters, backbuffer
  descriptor and animation-record verification.
- Per-instance boundary, alignment and weapon-reset JSON files; raw backbuffers.

The driver refuses to export an aligned pair if its target tick was skipped,
positions/facing differ, the checked animation records differ, or any active
weapon is not an assault rifle. A skipped tick can occur when a frame advances
multiple simulation ticks; rerun the command to choose another future tick.

`--output PATH`, `--lead-ticks N`, `--client-qmp PORT`, and `--host-qmp PORT`
are available. An output directory must not already exist. Omitting
`--align-carousel` captures a common tick without applying poses or resetting
weapon presentation; consult the manifest's match checks before comparison.

## State changes and capture boundary

`--align-carousel` maps the first and second active local-player slots to the
top and bottom viewports. Controller slot numbers can differ between instances.
It applies the saved targets through `object_set_position` (0x143ae0) and
`player_control_set_facing` (0xb6ea0):

| Viewport | Position (x, y, z) | Yaw | Pitch |
| --- | --- | --- | --- |
| Top | -4.661903381347656, -11.369110107421875, -0.8562036156654358 | 1.1448506116867065 | 0 |
| Bottom | 5.810912609100342, 3.6055307388305664, -2.723989963531494 | 0.7155909538269043 | 0 |

The helper clears translational velocity, updates the three facing vectors used
by Halo's script teleporter (+0x1d4, +0x1e0, +0x204), and requests the inventory's
assault rifle through the player-control desired weapon index (+0x20). Both
inventory weapons remain available. After settling, it reapplies the poses at
`director_update` (0x875f0) on the target tick, after simulation and before camera
updates. This prevents physics settling from moving one player slightly before
capture.

At that same boundary it calls the existing
`first_person_weapons_initialize_for_new_map` (0xdc7a0) and
`first_person_weapons_update` (0xdeb60) to establish a shared presentation start.
This is an intentional test fixture reset, not a replay of the preceding match.
It checks the first-person state word (+0x0c) and animation-record bytes
(+0x16 through +0x27). An unclassified field at +0x0e has differed between runs;
the tool does not claim equality of the entire first-person state block.

The worker resolves E9 redirects dynamically, because patched functions can
execute elsewhere and direct lifted calls can bypass their original stubs.
It stops at `rasterizer_present` (original address 0x157e40), after rendering
and **before** the buffer flip. It follows Halo's own screenshot path:

1. `D3DDevice_GetBackBuffer(0, 0, &surface)` (0x1e7d50).
2. `D3DSurface_GetDesc` (0x1ef1e0); validate 640x480 and 1,228,800 bytes.
3. `D3DSurface_LockRect(surface, &locked, NULL, 0xc0)` (0x1ef200).
4. Read the returned virtual pixel pointer using GDB, with the returned pitch.
5. Release the temporary surface reference (0x1ed930) and restore scratch memory.

The xemu source's `flatview_read_continue` invokes memory-access callbacks.
NV2A's surface callback schedules a download of a dirty GPU surface and waits
for that download. This is why the readback uses the rendered surface rather
than polling `NV_PCRTC_START`, which identifies displayed scanout and can lag
behind the current render. The tested format was 18, pitch 2560, decoded BGRX.

The worker then stops at the presentation return address read from its stack.
It verifies the simulation tick did not change and the 64-bit presentation
counter at 0x325668 advanced exactly once. It holds that boundary until both
workers finish. Screenshots need not be read at the same wall-clock instant:
each is already a copy of its selected rendered frame.

## Scope and limitations

These addresses/layouts target Halo Xbox debug 2276. The tool does not upload an
XBE, modify `init.txt`, reload a map, or overwrite the game tick. Alignment does
modify live player state and reset first-person presentation through engine
calls. It leaves those poses in the game and resumes both instances afterward.
Debugger breakpoints and temporary GDB servers are removed on normal exit and
handled failure. A failed attempt has diagnostics but no accepted image pair.

The selected comparison event is the first render after the shared pose and
weapon-presentation reset at a common simulation tick. It does **not** establish
equality of the whole match history, RNG, remote/network state, projectiles,
particles, lighting histories or every camera/animation field. Tests involving
those need a common deterministic fixture and inputs. Pixel differences between
the original and patched renderers remain valid comparison results; pixel
identity is not an acceptance requirement.

Live validation of the reusable tool on 2026-10-03 captured both instances at
tick 39952 using the rendered-backbuffer method. Both corresponding positions
and yaw/pitch values were bit-for-bit equal. Active assault rifles and the
checked animation records also matched. Earlier
scanout captures and the tick-1884 pair are not suitable evidence of aligned
spawns or a shared render event.
