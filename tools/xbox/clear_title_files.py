#!/usr/bin/env python3
r"""Remove deploy leftovers from the Halo title directory via XBDM/RDCP.

Deploys put these files next to the XBE, and the running title sees that
directory as D:. They outlive the deploy that wrote them, so they apply to
whatever build runs from the directory next:

  cachebeta.map - Halo's stack walker reads d:\cachebeta.map. A map from
                  another build names every frame wrongly.
  init.txt      - Halo runs it as console commands at boot.

XBEs are never removed.
"""

from __future__ import annotations

import argparse
import os
import sys

_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

from internal.local_env import load_repo_env, maybe_reexec_on_windows


load_repo_env("xbox.env")
maybe_reexec_on_windows(__file__)

from xbox.clear_cache import (DEFAULT_HOST, DEFAULT_PORT, DEFAULT_TIMEOUT,
                              delete_item, list_files_in_path)
from xbox.xbdm_rdcp import RdcpClient, RdcpError


DEFAULT_TITLE_DIR = os.environ.get("XBOX_TITLE_DIR", "E:\\GAMES\\halo-patched\\")
TITLE_FILES = ["cachebeta.map", "init.txt"]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Remove cachebeta.map and init.txt from the title directory",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python tools/xbox/clear_title_files.py --dry-run
  python tools/xbox/clear_title_files.py -x 127.0.0.1
  python tools/xbox/clear_title_files.py --only init.txt
"""
    )
    parser.add_argument("-x", "--host", default=DEFAULT_HOST,
                        help=f"Xbox host (default: {DEFAULT_HOST})")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT,
                        help=f"XBDM port (default: {DEFAULT_PORT})")
    parser.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT,
                        help=f"Timeout (default: {DEFAULT_TIMEOUT})")
    parser.add_argument("--title-dir", default=DEFAULT_TITLE_DIR,
                        help=f"Title directory (default: {DEFAULT_TITLE_DIR})")
    parser.add_argument("--only", choices=TITLE_FILES, action="append",
                        help="Remove only this file (repeatable)")
    parser.add_argument("--dry-run", action="store_true",
                        help="Preview without deleting")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    title_dir = args.title_dir
    if not title_dir.endswith("\\"):
        title_dir += "\\"
    wanted = args.only or TITLE_FILES

    try:
        client = RdcpClient(args.host, args.port, args.timeout)
        client.connect()
        files, error_code, error_msg = list_files_in_path(client, title_dir)
        if error_code == 402:
            print(f"{title_dir} does not exist")
            return 0
        if error_code != 0:
            print(f"error listing {title_dir}: {error_code}- {error_msg}",
                  file=sys.stderr)
            return 1

        present = {item["name"].lower(): item["name"] for item in files
                   if not item.get("is_dir")}
        deleted = []
        failed = 0
        locked = False
        for name in wanted:
            actual = present.get(name.lower())
            if actual is None:
                continue
            full_path = f"{title_dir}{actual}"
            ok, hit_locked = delete_item(client, full_path, dry_run=args.dry_run)
            locked = locked or hit_locked
            if ok:
                deleted.append(full_path)
            else:
                failed += 1
        client.close()
    except RdcpError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    if locked:
        print("files are in use by the running title; stop it and rerun",
              file=sys.stderr)
    if not deleted and not failed:
        print(f"no deploy leftovers in {title_dir}")
    elif not args.dry_run:
        for path in deleted:
            print(f"deleted {path}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
