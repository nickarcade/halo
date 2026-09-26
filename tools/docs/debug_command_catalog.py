#!/usr/bin/env python3
"""Extract every console-settable debug command from the pristine XBE.

Two command sources live in the binary (debug build 2276):

1. **External HS globals** (443) — pointer table at ``0x2f3708``, count at
   ``0x27d504``. Descriptors are 0x0c bytes: ``{char *name; int16_t type;
   void *backing;}``. The console evaluate path finds a bare first word via
   ``hs_find_global_by_name`` and wraps it as ``(set <name> <args>)``, so
   typing ``ai_render true`` is enough. Types: 5=bool, 6=real, 7=short,
   8=long (same indices as the HS type-name table at ``0x2f14a8``).

2. **HS function table** (418) — pointer table at ``0x2f1588``. Descriptors
   are 0x1c bytes (variable-length arg types at ``+0x1a``): flags/return
   type, name, type-check cb, eval cb, help, syntax, argc, arg types.
   Invoked parenthesized: ``(cheat_all_weapons)``. Help strings exist for
   all 418 entries; 27 carry an explicit usage string, the rest format
   from arg types the way ``hs_function_format_usage`` does.

Grouping is prefix-based (renderer / ai / collision / sound / ...).
Descriptions for globals are joined from the Reclaimers extract at
``docs/references/h1/scripting-reference.md`` when the name matches; those
strings are PC/H1A-derived commentary, not Xbox binary evidence, and the
catalog labels them as such. Names present only in the doc (73) or only in
the Xbox binary (16) are listed separately.

Usage:
    python3 tools/docs/debug_command_catalog.py                 # write md
    python3 tools/docs/debug_command_catalog.py --json out.json
    python3 tools/docs/debug_command_catalog.py --grep ai_render
    python3 tools/docs/debug_command_catalog.py --area AI
"""

from __future__ import annotations

import argparse
import json
import re
import struct
import sys
from collections import OrderedDict
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "tools" / "equivalence"))

from xbe_image import load_xbe, read_va, read_va_raw, section_at  # noqa: E402

OUT_MD = REPO_ROOT / "docs" / "debug-command-catalog.md"
DOC_REF = REPO_ROOT / "docs" / "references" / "h1" / "scripting-reference.md"

HS_TYPE_TABLE = 0x2F14A8  # char *type_names[]
HS_FUNC_TABLE = 0x2F1588
HS_FUNC_COUNT = 0x1A2
EXT_GLOBAL_TABLE = 0x2F3708
EXT_GLOBAL_COUNT_ADDR = 0x27D504

# Descriptor type codes seen on this binary (subset of the 0x2f14a8 table).
GLOBAL_TYPE_NAMES = {5: "boolean", 6: "real", 7: "short", 8: "long"}
GLOBAL_TYPE_HELP = {
    5: "`true`/`false` (also accepts `1`/`0`)",
    6: "float",
    7: "signed 16-bit",
    8: "signed 32-bit",
}
FN_TYPE_NAMES = {
    3: "passthrough",
    4: "void",
    5: "boolean",
    6: "real",
    7: "short",
    8: "long",
    23: "object_list",
    32: "game_difficulty",
    37: "object",
    38: "unit",
}

# Ordered prefix rules for external globals. First match wins; longest
# prefixes first so ``ai_render_paths`` lands under AI not the bare ``ai_``.
GLOBAL_AREA_RULES = [
    ("ai_render", "AI"),
    ("ai_print", "AI"),
    ("ai_show", "AI"),
    ("ai_debug", "AI"),
    ("ai_", "AI"),
    ("rasterizer_", "Renderer"),
    ("render_", "Renderer"),
    ("radiosity_", "Renderer"),
    ("texture_cache_", "Renderer"),
    ("framerate_", "Renderer"),
    ("display_framerate", "Renderer"),
    ("display_vblank", "Renderer"),
    ("display_precache", "Renderer"),
    ("screenshot_", "Renderer"),
    ("debug_framerate", "Renderer"),
    ("debug_no_drawing", "Renderer"),
    ("debug_no_frustum", "Renderer"),
    ("debug_frustum", "Renderer"),
    ("debug_lights", "Renderer"),
    ("debug_sprites", "Renderer"),
    ("debug_decals", "Renderer"),
    ("debug_permanent_decals", "Renderer"),
    ("debug_detail_objects", "Renderer"),
    ("debug_fog_planes", "Renderer"),
    ("debug_render_freeze", "Renderer"),
    ("debug_texture_cache", "Renderer"),
    ("debug_material_effects", "Effects"),
    ("debug_effects", "Effects"),
    ("debug_damage", "Effects"),
    ("effects_corpse", "Effects"),
    ("decals", "Effects"),
    ("weather", "Effects"),
    ("debug_objects", "Objects"),
    ("debug_object", "Objects"),
    ("debug_inactive_objects", "Objects"),
    ("debug_unit", "Units"),
    ("debug_biped", "Units"),
    ("debug_player", "Player"),
    ("debug_camera", "Camera"),
    ("freeze_flying_camera", "Camera"),
    ("force_all_player_views", "Camera"),
    ("director_camera", "Camera"),
    ("debug_bsp", "Structures"),
    ("debug_leaf", "Structures"),
    ("debug_portals", "Structures"),
    ("debug_structure", "Structures"),
    ("structures_use_pvs", "Structures"),
    ("debug_trigger_volumes", "Structures"),
    ("debug_obstacle", "Pathfinding"),
    ("debug_physics", "Physics"),
    ("debug_point_physics", "Physics"),
    ("debug_collision", "Collision"),
    ("collision_", "Collision"),
    ("debug_sound", "Sound"),
    ("debug_looping_sound", "Sound"),
    ("sound_", "Sound"),
    ("loud_dialog", "Sound"),
    ("model_animation", "Animation"),
    ("debug_recording", "Animation"),
    ("debug_bink", "Animation"),
    ("debug_scripting", "Game"),
    ("debug_game_save", "Game"),
    ("run_game_scripts", "Game"),
    ("recover_saved_games", "Game"),
    ("debug_input", "Input"),
    ("controls_", "Input"),
    ("pad3", "Input"),
    ("player", "Player"),
    ("cheat_", "Cheats"),
    ("profile_", "Profiling"),
    ("console", "Console"),
    ("terminal", "Console"),
    ("temporary_hud", "HUD/UI"),
    ("object_light", "Lighting"),
    ("global_connection", "Network"),
    ("allow_out_of_sync", "Network"),
    ("find_all_", "Misc"),
    ("f0", "Misc"),
    ("f1", "Misc"),
    ("f2", "Misc"),
    ("f3", "Misc"),
    ("f4", "Misc"),
    ("f5", "Misc"),
    ("stun_", "Misc"),
    ("rider_", "Misc"),
    ("breakable_", "Misc"),
    ("debug_", "Misc"),
]

# Same idea for the 418 parenthesized functions.
FN_AREA_RULES = [
    ("ai_", "AI"),
    ("rasterizer_", "Renderer"),
    ("radiosity_", "Renderer"),
    ("render_", "Renderer"),
    ("texture_cache", "Renderer"),
    ("sound_", "Sound"),
    ("debug_sounds", "Sound"),
    ("hud_", "HUD/UI"),
    ("show_hud", "HUD/UI"),
    ("enable_hud", "HUD/UI"),
    ("activate_nav_point", "HUD/UI"),
    ("deactivate_nav_point", "HUD/UI"),
    ("activate_team_nav_point", "HUD/UI"),
    ("deactivate_team_nav_point", "HUD/UI"),
    ("time_code", "HUD/UI"),
    ("cinematic_", "Cinematics"),
    ("camera_", "Camera"),
    ("recording_", "Camera"),
    ("fade_", "Cinematics"),
    ("attract_mode", "Cinematics"),
    ("object_", "Objects"),
    ("objects_", "Objects"),
    ("effect_", "Effects"),
    ("damage_", "Effects"),
    ("scenery_", "Effects"),
    ("breakable_surfaces", "Objects"),
    ("garbage_collect", "Objects"),
    ("unit_", "Units"),
    ("units_", "Units"),
    ("vehicle", "Units"),
    ("custom_animation", "Units"),
    ("magic_", "Units"),
    ("device_", "Devices"),
    ("cheat", "Cheats"),
    ("player_action_test", "Player"),
    ("player_effect", "Player"),
    ("player0_", "Player"),
    ("player_", "Player"),
    ("players", "Player"),
    ("game_", "Game"),
    ("map_", "Game"),
    ("core_", "Game"),
    ("multiplayer_map", "Game"),
    ("switch_bsp", "Game"),
    ("structure_bsp", "Game"),
    ("delete_save", "Game"),
    ("script_screen_effect", "Screen effects"),
    ("debug_", "Debug"),
    ("profile_", "Profiling"),
    ("script_doc", "Debug"),
    ("script_recompile", "Debug"),
    ("help", "Debug"),
    ("print", "Debug"),
    ("cls", "Debug"),
    ("inspect", "Debug"),
    ("error_overflow", "Debug"),
    ("enumerate_memory", "Debug"),
    ("version", "Debug"),
    ("playback", "Debug"),
    ("crash", "Debug"),
    ("fast_setup_network", "Network"),
    ("network_game", "Network"),
    ("xbox_set_machine", "Misc"),
    ("ui_widget", "Misc"),
    ("display_scenario", "Misc"),
    ("numeric_countdown", "Misc"),
    ("volume_", "Objects"),
    ("list_", "Misc"),
    ("random_range", "Misc"),
    ("real_random_range", "Misc"),
    ("game_time", "Game"),
    ("set", "Control"),
    ("if", "Control"),
    ("cond", "Control"),
    ("begin", "Control"),
    ("sleep", "Control"),
    ("wake", "Control"),
    ("and", "Logic"),
    ("or", "Logic"),
    ("not", "Logic"),
]

# Function names that are pure control/math plumbing — always grouped there
# regardless of prefix rules above (they have no prefix).
CONTROL_NAMES = {
    "begin", "begin_random", "if", "cond", "set", "and", "or", "not",
    "=", "!=", ">", "<", ">=", "<=", "+", "-", "*", "/", "min", "max",
    "sleep", "sleep_until", "wake", "inspect", "print", "cls",
    "random_range", "real_random_range",
}


def _u32(raw, secs, va):
    b = read_va(raw, secs, va, 4)
    return struct.unpack("<I", b)[0] if len(b) == 4 else None


def _i16(raw, secs, va):
    b = read_va(raw, secs, va, 2)
    return struct.unpack("<h", b)[0] if len(b) == 2 else None


def _cstr(raw, secs, va, maxlen=2048):
    if not va:
        return None
    b = read_va_raw(raw, secs, va, maxlen)
    if not b:
        return None
    i = b.find(b"\x00")
    if i < 0:
        i = len(b)
    try:
        return b[:i].decode("ascii")
    except UnicodeDecodeError:
        return None


def type_name(idx):
    if idx is None:
        return "?"
    if idx in FN_TYPE_NAMES:
        return FN_TYPE_NAMES[idx]
    return None


def classify(name, rules, default="Misc"):
    if name in CONTROL_NAMES:
        if name in (
            "begin", "begin_random", "if", "cond", "set", "sleep",
            "sleep_until", "wake",
        ):
            return "Control"
        if name in (
            "and", "or", "not", "=", "!=", ">", "<", ">=", "<=",
        ):
            return "Logic"
        if name in ("print", "cls", "inspect"):
            return "Debug"
        return "Math"
    low = name.lower()
    for prefix, area in rules:
        if low.startswith(prefix):
            return area
    return default


def load_globals(raw, secs):
    count = _i16(raw, secs, EXT_GLOBAL_COUNT_ADDR)
    if not count or count < 0:
        raise RuntimeError("external global count unreadable")
    out = []
    for i in range(count):
        desc = _u32(raw, secs, EXT_GLOBAL_TABLE + 4 * i)
        if desc is None:
            continue
        name = _cstr(raw, secs, _u32(raw, secs, desc))
        typ = _i16(raw, secs, desc + 4)
        backing = _u32(raw, secs, desc + 8)
        default = None
        if backing:
            sec = section_at(secs, backing)
            if sec and sec.raw_contains_va(backing):
                if typ == 5:
                    b = read_va(raw, secs, backing, 1)
                    default = bool(b[0]) if b else None
                elif typ == 6:
                    b = read_va(raw, secs, backing, 4)
                    default = struct.unpack("<f", b)[0] if len(b) == 4 else None
                elif typ == 7:
                    b = read_va(raw, secs, backing, 2)
                    default = struct.unpack("<h", b)[0] if len(b) == 2 else None
                elif typ == 8:
                    b = read_va(raw, secs, backing, 4)
                    default = struct.unpack("<i", b)[0] if len(b) == 4 else None
        if name:
            out.append({
                "index": i,
                "name": name,
                "type": GLOBAL_TYPE_NAMES.get(typ, str(typ)),
                "type_code": typ,
                "backing": f"0x{backing:08x}" if backing else None,
                "default": default,
                "area": classify(name, GLOBAL_AREA_RULES),
            })
    return out


def load_functions(raw, secs):
    out = []
    for i in range(HS_FUNC_COUNT):
        desc = _u32(raw, secs, HS_FUNC_TABLE + 4 * i)
        if desc is None:
            continue
        flags = _u32(raw, secs, desc)
        name = _cstr(raw, secs, _u32(raw, secs, desc + 4))
        help_s = _cstr(raw, secs, _u32(raw, secs, desc + 0x10))
        syntax = _cstr(raw, secs, _u32(raw, secs, desc + 0x14), 512)
        argc = _i16(raw, secs, desc + 0x18)
        arg_types = []
        if argc and 0 < argc <= 32:
            for j in range(argc):
                t = _i16(raw, secs, desc + 0x1A + 2 * j)
                tn = type_name(t) if t is not None and t < 54 else None
                arg_types.append(tn or f"type_{t}")
        if syntax:
            usage = syntax
        elif arg_types:
            usage = " ".join(f"<{t}>" for t in arg_types)
        else:
            usage = ""
        if not name:
            continue
        ret = type_name(flags) if flags is not None and flags < 54 else f"flags_{flags}"
        out.append({
            "index": i,
            "name": name,
            "return": ret or f"flags_{flags}",
            "usage": usage,
            "help": help_s or "",
            "area": classify(name, FN_AREA_RULES),
        })
    return out


def load_doc_prose():
    """Parse Reclaimers external-global descriptions keyed by name.

    Returns {name: prose}. The extract packs several `(name [type])` entries
    into one ```lisp fence and puts the prose after the closing fence, so a
    fence-scoped walk is required rather than one regex per entry. Prose is
    PC/H1A commentary — flagged as such in the catalog, never presented as
    Xbox binary evidence.
    """
    if not DOC_REF.exists():
        return {}
    text = DOC_REF.read_text(encoding="utf-8", errors="replace")
    start = text.find("## External globals")
    if start < 0:
        return {}
    end = text.find("\n## ", start + 1)
    sec = text[start:end if end > 0 else len(text)]

    out = {}
    pending: list[str] = []
    for block in re.split(r"```(?:lisp)?\n?", sec):
        if not block.strip():
            continue
        if block.lstrip().startswith("("):
            pending = re.findall(r"^\(([A-Za-z_][\w]*)", block, re.M)
            continue
        prose = " ".join(block.split())
        if prose and pending:
            for name in pending:
                out.setdefault(name, prose[:400])
            pending = []
    return out


def md_escape(s):
    return (s or "").replace("|", "\\|").replace("\n", " ").strip()


def fmt_default(val, typ):
    if val is None:
        return "—"
    if typ == "boolean":
        return "true" if val else "false"
    return str(val)


def build_markdown(globals_, functions, doc_prose, only_debug=False, area=None, grep=None):
    def keep(entry):
        if area and entry["area"] != area:
            return False
        if grep and grep.lower() not in entry["name"].lower():
            return False
        if only_debug and not _is_debug(entry["name"]):
            return False
        return True

    g_all = [g for g in globals_ if keep(g)]
    f_all = [f for f in functions if keep(f)]

    lines = []
    w = lines.append
    w("# Halo CE Xbox Debug Command Catalog")
    w("")
    w("Generated from the pristine debug build 2276 XBE by")
    w("`tools/docs/debug_command_catalog.py`.")
    w("")
    w("## How console dispatch works")
    w("")
    w("Open the console with <kbd>~</kbd>. `hs_console_evaluate()`")
    w("(`src/halo/hs/hs.c`) wraps the line before compiling:")
    w("")
    w("| You type | Becomes | Source table |")
    w("|----------|---------|--------------|")
    w("| `ai_render true` | `(set ai_render true)` | external globals `0x2f3708` (443) |")
    w("| `ai_render` | `(ai_render)` (global read) | external globals |")
    w("| `(cheat_all_weapons)` | pass-through | HS function table `0x2f1588` (418) |")
    w("| `cheat_all_weapons` | `(cheat_all_weapons)` | HS function table |")
    w("")
    w("A leading word that matches a global name is treated as a `set`.")
    w("Anything else is wrapped as a function call. `;` blanks the line")
    w("(comment). In-game help: `(help <name>)` prints one entry;")
    w("`(script_doc)` writes all 418 function signatures + help to")
    w("`hs_doc.txt` — the Xbox build does **not** dump external globals")
    w("there, which is why this catalog exists.")
    w("")

    # Area index
    w("## Area index")
    w("")
    w("| Area | Globals | Functions |")
    w("|------|--------:|----------:|")
    areas = sorted({g["area"] for g in g_all} | {f["area"] for f in f_all})
    for a in areas:
        gc = sum(1 for g in g_all if g["area"] == a)
        fc = sum(1 for f in f_all if f["area"] == a)
        w(f"| [{a}](#{a.lower().replace('/', '').replace(' ', '-')}) | {gc} | {fc} |")
    w(f"| **Total** | **{len(g_all)}** | **{len(f_all)}** |")
    w("")

    # Globals by area
    w("## External globals (set bare, no parens needed)")
    w("")
    w("Type is the HS descriptor type at `desc+4`. Default is the value")
    w("backed by the live C variable in the pristine image (BSS tails read")
    w("as zero). Descriptions marked *(Reclaimers)* come from the PC/H1A")
    w("extract and are commentary, not Xbox binary evidence.")
    w("")
    for a in areas:
        rows = [g for g in g_all if g["area"] == a]
        if not rows:
            continue
        w(f"### {a}")
        w("")
        w("| Command | Type | Default | Description |")
        w("|---------|------|---------|-------------|")
        for g in sorted(rows, key=lambda x: x["name"]):
            prose = doc_prose.get(g["name"], "")
            if prose:
                desc = f"*(Reclaimers)* {md_escape(prose)}"
            else:
                desc = ""
            w(
                f"| `{g['name']}` | {g['type']} | "
                f"`{fmt_default(g['default'], g['type'])}` | {desc} |"
            )
        w("")

    # Functions by area
    w("## HaloScript functions (parenthesized)")
    w("")
    w("Return type is the descriptor flags word interpreted through the")
    w("HS type-name table at `0x2f14a8`. Help is the descriptor's own")
    w("string (same text `help` / `script_doc` print).")
    w("")
    for a in areas:
        rows = [f for f in f_all if f["area"] == a]
        if not rows:
            continue
        w(f"### {a}")
        w("")
        w("| Command | Returns | Usage | Help |")
        w("|---------|---------|-------|------|")
        for f in sorted(rows, key=lambda x: x["name"]):
            usage = f"({f['name']}" + (f" {f['usage']}" if f["usage"] else "") + ")"
            w(
                f"| `{md_escape(usage)}` | {f['return']} | "
                f"`{md_escape(f['usage'])}` | {md_escape(f['help'])} |"
            )
        w("")

    # Diff vs Reclaimers doc
    if doc_prose and not only_debug and not area and not grep:
        bin_names = {g["name"] for g in globals_}
        doc_names = set(doc_prose)
        only_bin = sorted(bin_names - doc_names)
        only_doc = sorted(doc_names - bin_names)
        w("## Coverage vs Reclaimers external-globals extract")
        w("")
        w(f"- Xbox binary: **{len(globals_)}** entries "
          f"(**{len(bin_names)}** unique names; "
          f"{len(globals_) - len(bin_names)} duplicate)")
        w(f"- Reclaimers extract: **{len(doc_names)}** entries")
        w(f"- In binary, missing from extract ({len(only_bin)}): "
          + ", ".join(f"`{n}`" for n in only_bin))
        w(f"- In extract, not in this Xbox build ({len(only_doc)}) — "
          "PC/H1A/server-only names, not usable here: "
          + ", ".join(f"`{n}`" for n in only_doc))
        w("")
        w("## Related docs")
        w("")
        w("- `docs/debug-commands-keyboard.md` — keyboard shortcuts,")
        w("  cheats.txt, console evaluate internals")
        w("- `docs/references/h1/scripting-reference.md` — Reclaimers HSC")
        w("  reference (PC/H1A; includes names absent from this binary)")
        w("- In-game: `(script_doc)` → `hs_doc.txt`, `(help <name>)`")
        w("")
    return "\n".join(lines) + "\n"


_DEBUG_PREFIXES = (
    "debug_", "ai_render", "ai_print", "ai_show", "ai_debug",
    "collision_debug", "collision_log", "rasterizer_debug",
    "rasterizer_stats", "rasterizer_profile", "profile_",
    "cheat_", "radiosity_",
)
_DEBUG_EXACT = {
    "rasterizer_wireframe", "rasterizer_smart", "rasterizer_environment",
    "error_overflow_suppression", "script_doc", "script_recompile",
    "help", "print", "cls", "inspect", "enumerate_memory_units",
    "version", "playback", "crash", "debug_camera_save",
    "debug_camera_load", "rasterizer_decals_flush",
    "rasterizer_fps_accumulate", "texture_cache_flush",
    "sound_cache_flush", "structure_lens_flares_place",
    "breakable_surfaces_enable", "game_speed", "map_name",
    "camera_control", "error_suppress_all",
}


def _is_debug(name):
    return name.startswith(_DEBUG_PREFIXES) or name in _DEBUG_EXACT


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-o", "--out", type=Path, default=OUT_MD,
                    help="markdown output path")
    ap.add_argument("--json", type=Path, help="also write full JSON dump")
    ap.add_argument("--area", help="filter to one area (e.g. AI, Renderer)")
    ap.add_argument("--grep", help="substring filter on command name")
    ap.add_argument("--debug-only", action="store_true",
                    help="keep only debug-flavored commands")
    ap.add_argument("--stdout", action="store_true",
                    help="print markdown instead of writing the file")
    args = ap.parse_args(argv)

    raw, secs = load_xbe()
    globals_ = load_globals(raw, secs)
    functions = load_functions(raw, secs)
    doc_prose = load_doc_prose()

    if not globals_ or not functions:
        print("error: extracted 0 commands — wrong XBE?", file=sys.stderr)
        return 1

    md = build_markdown(globals_, functions, doc_prose,
                        only_debug=args.debug_only,
                        area=args.area, grep=args.grep)

    if args.json:
        payload = {
            "globals": globals_,
            "functions": functions,
            "doc_prose_keys": sorted(doc_prose),
            "counts": {"globals": len(globals_), "functions": len(functions)},
        }
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(payload, indent=2), encoding="utf-8")
        print(f"wrote {args.json}")

    if args.stdout:
        sys.stdout.write(md)
    else:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(md, encoding="utf-8")
        print(f"wrote {args.out}  ({len(globals_)} globals, "
              f"{len(functions)} functions)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
