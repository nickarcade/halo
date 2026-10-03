from __future__ import annotations

import sys, os
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

import os
import re
import subprocess
import sys
from pathlib import Path


def load_repo_env(filename: str) -> None:
    """Load KEY=VALUE lines from <repo>/<filename> or <repo>/tools/<filename>.

    Already-set environment variables win. The gitignored local files
    (tools/xbox.env, tools/local.env) live in tools/; the repo root is searched
    first for backwards compatibility.
    """
    repo_root = Path(__file__).resolve().parent.parent.parent
    for env_path in (repo_root / filename, repo_root / "tools" / filename):
        _load_env_file(env_path)


def _load_env_file(env_path: Path) -> None:
    if not env_path.is_file():
        return

    for raw_line in env_path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue

        key, value = line.split("=", 1)
        key = key.strip()
        value = value.strip()
        if not key:
            continue

        if len(value) >= 2 and value[0] == value[-1] and value[0] in {'"', "'"}:
            value = value[1:-1]

        os.environ.setdefault(key, value)


DEFAULT_RXDK_ROOT = "/mnt/c/Program Files (x86)/RXDK"


def rxdk_paths() -> tuple[str, str]:
    """Return (WSL-style, Windows-style) paths of the Xbox SDK install.

    The install location comes from RXDK_ROOT (environment or tools/local.env,
    given in either style) and defaults to the standard Windows location.
    """
    load_repo_env("local.env")
    root = os.environ.get("RXDK_ROOT", "").strip() or DEFAULT_RXDK_ROOT
    root = root.rstrip("/\\")
    m = re.match(r"^/mnt/([a-zA-Z])/(.*)$", root)
    if m:
        return root, f"{m.group(1).upper()}:\\" + m.group(2).replace("/", "\\")
    m = re.match(r"^([a-zA-Z]):[\\/](.*)$", root)
    if m:
        return f"/mnt/{m.group(1).lower()}/" + m.group(2).replace("\\", "/"), root
    return root, root


def is_wsl() -> bool:
    return sys.platform.startswith("linux") and "WSL_INTEROP" in os.environ


def to_windows_path(path: str) -> str:
    path = os.path.realpath(path).replace("\\", "/")
    if (
        len(path) >= 7
        and path.startswith("/mnt/")
        and path[5].isalpha()
        and path[6] == "/"
    ):
        return f"{path[5].upper()}:{path[6:]}"
    if sys.platform.startswith("linux"):
        try:
            result = subprocess.run(
                ["wslpath", "-w", path],
                capture_output=True, text=True, check=True,
            )
            win_path = result.stdout.strip()
            if win_path:
                return win_path
        except (subprocess.CalledProcessError, FileNotFoundError):
            pass
    return path


def find_windows_python() -> str | None:
    configured = os.environ.get("WINDOWS_PYTHON", "").strip()
    if configured:
        return configured

    candidates = [
        "/mnt/c/WINDOWS/system32/cmd.exe",
        "/mnt/c/Windows/System32/cmd.exe",
    ]
    for candidate in candidates:
        if os.path.isfile(candidate):
            return candidate
    return None


def build_windows_python_command(script_path: str, script_args: list[str]) -> list[str] | None:
    python_exe = find_windows_python()
    if python_exe is None:
        return None

    command: list[str]
    lower_name = os.path.basename(python_exe).lower()
    if lower_name == "cmd.exe":
        command = [python_exe, "/c", "py", "-3"]
    else:
        command = [python_exe]
        if lower_name == "py.exe":
            command.append("-3")
    command.append(to_windows_path(script_path))
    command.extend(script_args)
    return command


def maybe_reexec_on_windows(script_path: str, script_args: list[str] | None = None) -> None:
    if not is_wsl() or os.environ.get("HALO_WINDOWS_REEXEC") == "1":
        return

    command = build_windows_python_command(script_path, script_args or sys.argv[1:])
    if command is None:
        return

    env = os.environ.copy()
    env["HALO_WINDOWS_REEXEC"] = "1"
    raise SystemExit(subprocess.run(command, env=env, check=False).returncode)
