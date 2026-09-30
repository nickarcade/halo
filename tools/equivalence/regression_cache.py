#!/usr/bin/env python3
"""Snapshot build + verdict cache for the Unicorn regression gate.

Two problems with running `regression_test.py` straight from the working tree:

1. It tests whatever happens to be on disk (a stale build/ object, or another
   agent's unstaged edits), not what is being committed.
2. Every commit re-derives ~93 deterministic verdicts even when nothing that can
   influence them changed.

This module fixes both without weakening the check:

* `Snapshot` materialises the exact tracked inputs (src/, third_party/xbox,
  kb.json, the decl.h generator) from the git INDEX (or any tree-ish) into a
  temp dir, regenerates decl.h from THAT kb.json, and compiles every target TU
  from it with exactly the CMake Release flags (verified byte-identical to the
  CMake build object apart from the COFF timestamp, which is zeroed here).
* Verdicts are cached by a content key covering EVERY input the verdict can
  depend on (see `target_key`).  Only PASS verdicts are ever stored.  Any doubt
  (unreadable cache, missing input, unhashable flag file) means "no key", which
  means the target runs.

A cache hit is therefore "this exact candidate object, this exact harness, this
exact oracle image, this exact slice of kb.json was already proven equivalent".
"""

import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
EQUIV_DIR = ROOT / "tools" / "equivalence"
CACHE_DIR = ROOT / "artifacts" / "audit"
VERDICT_CACHE = CACHE_DIR / "unicorn_cache.json"
COMPILE_INDEX = CACHE_DIR / "unicorn_obj" / "index.json"
OBJ_STORE = CACHE_DIR / "unicorn_obj"

CACHE_VERSION = 1
COMPILE_VERSION = 1

# Tracked inputs a candidate compile + decl.h generation can read.
SNAPSHOT_PATHS = [
    "src", "third_party/xbox", "tools/analysis",
    "tools/audit/check_requirements.py", "tools/internal",
    "kb.json", "requirements.txt",
]

# Environment variables that do not influence a verdict (set by the hooks or by
# this module).  Any OTHER HALO_*/BIPED_* variable is folded into the key.
_HARMLESS_ENV = {
    "HALO_STAGED_FILES", "HALO_STAGED_C", "HALO_HOOK_JOBS", "HALO_BATCH_COMMIT",
    "HALO_ALLOW_DEACTIVATIONS", "HALO_EQUIV_PRECOMPILED_DIR",
    "HALO_EQUIV_KB_JSON", "HALO_WINDOWS_REEXEC", "HALO_HOOK_TIMING",
    "HALO_VC71_PHASE", "HALO_VC71_STATE", "HALO_STAGED_DIR",
}


def _sha(*parts) -> str:
    h = hashlib.sha256()
    for p in parts:
        if isinstance(p, str):
            p = p.encode("utf-8")
        h.update(len(p).to_bytes(8, "little"))
        h.update(p)
    return h.hexdigest()


def _file_sha(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def _atomic_write_json(path: Path, payload) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, tmp = tempfile.mkstemp(dir=str(path.parent), prefix=path.name + ".")
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            json.dump(payload, f, sort_keys=True)
        os.replace(tmp, path)
    except BaseException:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        raise


def norm_source(source: str) -> str:
    """kb `source` spellings vary ('ai/actors.c' vs 'src/halo/ai/actors.c')."""
    return source[len("src/halo/"):] if source.startswith("src/halo/") else source


def precompiled_name(source: str) -> str:
    """Must match unicorn_diff._precompiled_obj_for_source."""
    return norm_source(source).replace("/", "__") + ".obj"


# ---------------------------------------------------------------------------
# Snapshot
# ---------------------------------------------------------------------------

class SnapshotError(RuntimeError):
    pass


def _git(*args, input=None, env=None):
    r = subprocess.run(["git", "-C", str(ROOT)] + list(args), input=input,
                       capture_output=True, env=env)
    if r.returncode != 0:
        raise SnapshotError("git %s failed: %s" % (" ".join(args[:2]),
                                                     r.stderr.decode(errors="replace").strip()))
    return r.stdout


class Snapshot:
    """Exact tracked content of `rev` ('index' or a tree-ish) in a temp dir."""

    def __init__(self, rev: str):
        self.rev = rev
        self.dir = Path(tempfile.mkdtemp(prefix="halo-unicorn-snap."))
        try:
            env = None
            tmp_index = None
            if rev != "index":
                fd, tmp_index = tempfile.mkstemp(prefix="halo-unicorn-idx.")
                os.close(fd)
                os.unlink(tmp_index)
                env = dict(os.environ, GIT_INDEX_FILE=tmp_index)
                _git("read-tree", rev, env=env)
            names = _git("ls-files", "-z", "--", *SNAPSHOT_PATHS, env=env)
            if not names:
                raise SnapshotError("no tracked inputs found for snapshot")
            _git("checkout-index", "--prefix=%s/" % self.dir, "-z", "--stdin",
                 input=names, env=env)
            if tmp_index and os.path.exists(tmp_index):
                os.unlink(tmp_index)
            self.kb_path = self.dir / "kb.json"
            if not self.kb_path.is_file() or not (self.dir / "src" / "common.h").is_file():
                raise SnapshotError("snapshot is missing kb.json or src/common.h")
            self.gen_dir = self.dir / "build" / "generated"
            self.gen_dir.mkdir(parents=True, exist_ok=True)
            self._gen_decl()
        except BaseException:
            self.cleanup()
            raise

    def _gen_decl(self):
        venv_py = ROOT / ".venv" / "bin" / "python3"
        py = str(venv_py) if venv_py.exists() else sys.executable
        r = subprocess.run(
            [py, str(self.dir / "tools" / "analysis" / "knowledge.py"),
             "--gen-header", str(self.gen_dir / "decl.h")],
            capture_output=True, text=True, cwd=str(self.dir))
        if r.returncode != 0 or not (self.gen_dir / "decl.h").is_file():
            raise SnapshotError("decl.h generation failed: %s" %
                                (r.stderr or r.stdout).strip()[-300:])

    def cleanup(self):
        shutil.rmtree(self.dir, ignore_errors=True)


# ---------------------------------------------------------------------------
# Compile (per-TU, cached by content of source + every header it includes)
# ---------------------------------------------------------------------------

def _clang_version() -> str:
    try:
        r = subprocess.run(["clang", "--version"], capture_output=True, text=True)
        return r.stdout.splitlines()[0] if r.returncode == 0 and r.stdout else ""
    except OSError:
        return ""


def compile_flags(snap_dir) -> list:
    """The CMake Release flags (CMakeLists.txt: CMAKE_C_FLAGS + _RELEASE +
    C_STANDARD 90).  Paths are parameterised so the flag set can be hashed
    independently of where the snapshot lives."""
    s = str(snap_dir)
    return [
        "-Wall", "-Werror", "-target", "i386-pc-win32", "-march=pentium3",
        "-mno-sse", "-nostdlib", "-ffreestanding", "-fno-builtin",
        "-fno-exceptions", "-fno-omit-frame-pointer",
        "-mstack-probe-size=65536",
        "-I" + s + "/build/generated", "-I" + s + "/src",
        "-I" + s + "/third_party/xbox",
        "-include", s + "/src/common.h",
        "-O3", "-DNDEBUG", "-fno-omit-frame-pointer", "-std=gnu90",
        # assert_halt stamps __FILE__ into .rdata; map the snapshot dir back to
        # the real repo root so the object is byte-identical to the CMake build
        # of this checkout and independent of the (random) snapshot path.
        "-ffile-prefix-map=" + s + "=" + str(ROOT),
    ]


def _parse_depfile(text: str) -> list:
    text = text.replace("\\\n", " ")
    _, _, rest = text.partition(":")
    return [t for t in rest.split() if t]


class _Hasher:
    """Per-run memoised file hashing (headers are shared by every TU)."""

    def __init__(self):
        self._memo = {}

    def __call__(self, path) -> str:
        p = str(path)
        if p not in self._memo:
            self._memo[p] = _file_sha(Path(p))
        return self._memo[p]


def _load_compile_index() -> dict:
    try:
        data = json.loads(COMPILE_INDEX.read_text())
        if data.get("version") == COMPILE_VERSION:
            return data.get("entries", {})
    except (OSError, ValueError):
        pass
    return {}


def _rel_dep(dep: str, snap_dir: str) -> str:
    return dep[len(snap_dir) + 1:] if dep.startswith(snap_dir + "/") else dep


def compile_tus(snap: Snapshot, sources, jobs: int):
    """Compile each source (kb `source` spelling) from the snapshot.

    Returns {source: (obj_path | None, obj_sha | None, error | None, from_cache)}.
    Objects are stored under artifacts/audit/unicorn_obj/<sha>.obj with the COFF
    timestamp zeroed (the only nondeterministic bytes).
    """
    snap_dir = str(snap.dir)
    hasher = _Hasher()
    flags = compile_flags(snap_dir)
    flags_id = _sha(str(COMPILE_VERSION), _clang_version(),
                    *[f.replace(snap_dir, "{SNAP}") for f in flags], str(ROOT))
    index = _load_compile_index()
    results = {}
    OBJ_STORE.mkdir(parents=True, exist_ok=True)

    def lookup(src_rel):
        for ent in index.get(flags_id + ":" + src_rel, []):
            try:
                ok = all(hasher(Path(snap_dir) / d if not d.startswith("/") else Path(d)) == h
                         for d, h in ent["deps"].items())
            except OSError:
                ok = False
            obj = OBJ_STORE / ent["obj"]
            if ok and obj.is_file():
                return ent
        return None

    def build(source):
        src_rel = "src/halo/" + norm_source(source)
        src_abs = Path(snap_dir) / src_rel
        if not src_abs.is_file():
            return source, (None, None, "source not in snapshot: " + src_rel, False), None
        ent = lookup(src_rel)
        if ent:
            return source, (OBJ_STORE / ent["obj"], ent["sha"], None, True), None
        tmpd = Path(tempfile.mkdtemp(prefix="halo-unicorn-cc."))
        try:
            out = tmpd / "o.obj"
            dep = tmpd / "o.d"
            r = subprocess.run(["clang"] + flags + ["-MD", "-MF", str(dep), "-c",
                                                    str(src_abs), "-o", str(out)],
                               capture_output=True, text=True, cwd=snap_dir)
            if r.returncode != 0 or not out.is_file():
                return source, (None, None,
                                (r.stderr or r.stdout).strip()[-400:] or "clang failed",
                                False), None
            raw = bytearray(out.read_bytes())
            raw[4:8] = b"\0\0\0\0"   # COFF TimeDateStamp
            sha = hashlib.sha256(raw).hexdigest()
            deps = {}
            for d in _parse_depfile(dep.read_text()):
                ap = d if d.startswith("/") else str(Path(snap_dir) / d)
                deps[_rel_dep(ap, snap_dir)] = hasher(ap)
            dest = OBJ_STORE / (sha + ".obj")
            fd, tmp = tempfile.mkstemp(dir=str(OBJ_STORE), prefix=".obj.")
            with os.fdopen(fd, "wb") as f:
                f.write(raw)
            os.replace(tmp, dest)
            new_ent = {"deps": deps, "obj": sha + ".obj", "sha": sha}
            return source, (dest, sha, None, False), (flags_id + ":" + src_rel, new_ent)
        finally:
            shutil.rmtree(tmpd, ignore_errors=True)

    uniq = sorted(set(sources))
    new_entries = []
    with ThreadPoolExecutor(max_workers=max(1, jobs)) as ex:
        for source, res, new_ent in ex.map(build, uniq):
            results[source] = res
            if new_ent:
                new_entries.append(new_ent)

    if new_entries:
        # Re-read before writing so concurrent commits do not clobber each other.
        cur = _load_compile_index()
        for key, ent in new_entries:
            lst = [e for e in cur.get(key, []) if e["obj"] != ent["obj"]]
            cur[key] = (lst + [ent])[-4:]
        # Bound the store: keep objects referenced by the index only.
        live = {e["obj"] for lst in cur.values() for e in lst}
        try:
            _atomic_write_json(COMPILE_INDEX, {"version": COMPILE_VERSION, "entries": cur})
            for f in OBJ_STORE.glob("*.obj"):
                if f.name not in live and time.time() - f.stat().st_mtime > 3600:
                    f.unlink()
        except OSError:
            pass
    return results


# ---------------------------------------------------------------------------
# kb.json projection and verdict keys
# ---------------------------------------------------------------------------

def _fn_ident(decl: str):
    m = re.search(r"\b(\w+)\s*\(", decl or "")
    return m.group(1) if m else None


def _decl_ident(decl: str):
    if not decl:
        return None
    head = re.split(r"[\[(;]", decl, 1)[0]
    ids = re.findall(r"[A-Za-z_]\w*", head)
    return ids[-1] if ids else None


class KbView:
    """Everything the harness can read from kb.json, indexed for footprinting.

    The harness resolves callees/globals by name and address, parses the
    target's (and its callees') decls, and classifies dwords against the set of
    function entry addresses.  Only the `comment` field is provably unread, so
    it is the only field excluded from the projection."""

    def __init__(self, kb_path: Path):
        kb = json.loads(kb_path.read_text(encoding="utf-8"))
        self.by_ident = {}
        self.by_addr = {}
        self.fn_by_name = {}     # first entry per decl identifier (unicorn's rule)
        fn_addrs = []
        for obj in kb.get("objects", []):
            if not isinstance(obj, dict):
                continue
            oname, osrc = obj.get("name", ""), obj.get("source", "")
            for group in ("data", "functions"):
                for e in obj.get(group, []) or []:
                    proj = {k: v for k, v in e.items() if k != "comment"}
                    proj["_g"] = group
                    proj["_o"] = oname
                    proj["_os"] = osrc
                    a = e.get("addr")
                    if a:
                        self.by_addr.setdefault(a.lower(), []).append(proj)
                    idents = {e.get("name"), _decl_ident(e.get("decl", "")),
                              _fn_ident(e.get("decl", "")) if group == "functions" else None}
                    for i in idents:
                        if i:
                            self.by_ident.setdefault(i, []).append(proj)
                    if group == "functions":
                        fi = _fn_ident(e.get("decl", ""))
                        if fi:
                            self.fn_by_name.setdefault(fi, proj)
                        if a:
                            try:
                                fn_addrs.append(int(a, 16))
                            except ValueError:
                                pass
        self.fn_addr_sha = _sha(json.dumps(sorted(set(fn_addrs))))

    def target_entries(self, addr: str, name: str):
        """Entries unicorn resolves for a target: by name (first match) and addr."""
        out = []
        bare = name.split("[", 1)[0]
        if bare in self.fn_by_name:
            out.append(self.fn_by_name[bare])
        out.extend(self.by_addr.get(addr.lower(), []))
        return out

    def source_of(self, addr: str, name: str):
        """(`_obj_source`) exactly as unicorn_diff._find_kb_entry computes it."""
        bare = name.split("[", 1)[0]
        ent = self.fn_by_name.get(bare)
        if ent is None:
            for e in self.by_addr.get(addr.lower(), []):
                if e["_g"] == "functions":
                    ent = e
                    break
        if ent is None:
            return None
        return ent.get("source") or ent.get("_os") or None

    def footprint_sha(self, obj_bytes: bytes, addr: str, name: str) -> str:
        toks = set(re.findall(rb"[A-Za-z_@?$][A-Za-z0-9_@?$.]{2,}", obj_bytes))
        idents = set()
        fun_addrs = set()
        for t in toks:
            s = t.decode("ascii", "replace")
            idents.add(s)
            c = s
            if c.startswith("__imp_"):
                c = c[len("__imp_"):]
            c = re.sub(r"@\d+$", "", c.lstrip("@").lstrip("_"))
            idents.add(c)
            idents.add(c.lstrip("_"))
            m = re.match(r"FUN_([0-9A-Fa-f]{1,8})$", c)
            if m:
                fun_addrs.add("0x" + m.group(1).lower().lstrip("0"))
                fun_addrs.add("0x" + m.group(1).lower())
        chosen = {}
        for i in idents:
            for e in self.by_ident.get(i, ()):
                chosen[json.dumps(e, sort_keys=True)] = 1
        for a in fun_addrs:
            for e in self.by_addr.get(a, ()):
                chosen[json.dumps(e, sort_keys=True)] = 1
        for e in self.target_entries(addr, name):
            chosen[json.dumps(e, sort_keys=True)] = 1
        return _sha(self.fn_addr_sha, *sorted(chosen))


def _dir_signature(path: Path) -> str:
    """Stat signature of delinked/ (an oracle input for the legacy/real-callee
    paths); gitignored, so there is no content to read from git."""
    parts = []
    try:
        for dp, _dn, fn in os.walk(path):
            for f in sorted(fn):
                if f.endswith(".obj"):
                    st = os.stat(os.path.join(dp, f))
                    parts.append("%s:%d:%d" % (os.path.join(dp, f)[len(str(path)):],
                                                st.st_size, st.st_mtime_ns))
    except OSError:
        return "unreadable"
    return _sha("\n".join(sorted(parts)))


def _version(mod: str) -> str:
    try:
        import importlib.metadata as md
        return md.version(mod)
    except Exception:
        return "absent"


class ToolContext:
    """Key components that are the same for every target in a run."""

    def __init__(self):
        py_files = sorted(EQUIV_DIR.glob("*.py"))
        extra = [ROOT / "tools" / "verify" / n
                 for n in ("xbe_reference.py", "function_bounds.py", "function_bounds.json")]
        extra.append(EQUIV_DIR / "known_globals.json")
        parts = []
        for f in py_files + extra:
            try:
                parts.append("%s=%s" % (f.name, _file_sha(f)))
            except OSError:
                parts.append("%s=absent" % f.name)
        self.tool_sha = _sha(*parts)
        xbe = ROOT / "halo-patched" / "cachebeta.xbe"
        self.xbe_sha = _file_sha(xbe) if xbe.is_file() else "absent"
        self.delinked_sig = _dir_signature(ROOT / "delinked")
        self.env_sig = _sha(json.dumps(sorted(
            (k, v) for k, v in os.environ.items()
            if k.startswith(("HALO_", "BIPED_")) and k not in _HARMLESS_ENV)))
        self.versions = _sha(sys.version.split()[0], _version("unicorn"),
                             _version("z3-solver"), _version("capstone"),
                             _clang_version())


def _flag_files_sha(flags):
    """Hash files named by value-taking flags.  None = cannot be pinned down."""
    parts = []
    for i, flag in enumerate(flags):
        if flag in ("--state-snapshot", "--value-corpus", "--from-halorec") and i + 1 < len(flags):
            p = ROOT / flags[i + 1]
            if not p.is_file():
                return None
            parts.append("%s=%s" % (flags[i + 1], _file_sha(p)))
    return _sha(*parts)


def target_key(ctx: ToolContext, kb: KbView, target: dict, seeds_used,
               obj_sha: str, obj_bytes: bytes, name: str):
    """Content key for one target's verdict, or None if any input is unpinnable."""
    files = _flag_files_sha(target.get("flags", []))
    if files is None or ctx.xbe_sha == "absent":
        return None
    return _sha(
        str(CACHE_VERSION), json.dumps(target, sort_keys=True), str(seeds_used),
        obj_sha, kb.footprint_sha(obj_bytes, target["addr"], name),
        ctx.tool_sha, ctx.xbe_sha, ctx.delinked_sig, ctx.env_sig, ctx.versions, files)


class VerdictCache:
    def __init__(self, enabled=True):
        self.enabled = enabled
        self.entries = {}
        if enabled:
            try:
                data = json.loads(VERDICT_CACHE.read_text())
                if data.get("version") == CACHE_VERSION:
                    self.entries = data.get("entries", {})
            except (OSError, ValueError):
                self.entries = {}     # unreadable -> everything runs
        self._new = {}

    def get(self, key):
        if not self.enabled or key is None:
            return None
        return self.entries.get(key)

    def put_pass(self, key, detail):
        if self.enabled and key is not None:
            self._new[key] = {"detail": detail, "t": int(time.time())}

    def flush(self):
        if not self._new:
            return
        try:
            try:
                data = json.loads(VERDICT_CACHE.read_text())
                cur = data.get("entries", {}) if data.get("version") == CACHE_VERSION else {}
            except (OSError, ValueError):
                cur = {}
            cur.update(self._new)
            if len(cur) > 3000:
                keep = sorted(cur.items(), key=lambda kv: kv[1].get("t", 0))[-3000:]
                cur = dict(keep)
            _atomic_write_json(VERDICT_CACHE, {"version": CACHE_VERSION, "entries": cur})
        except OSError:
            pass
