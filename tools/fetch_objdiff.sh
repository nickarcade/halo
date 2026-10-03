#!/usr/bin/env bash
# Download the pinned objdiff CLI to tools/objdiff-cli-linux-x86_64.
# Used by tools/verify/objdiff_lift.py and tools/report/matching.py.
set -euo pipefail

OBJDIFF_VERSION=3.7.1
OBJDIFF_SHA256=40d856da01e0a676c0f33af534e562f098cd50e45ecb64f3b346418c7b264ae6
ASSET=objdiff-cli-linux-x86_64
URL="https://github.com/encounter/objdiff/releases/download/v${OBJDIFF_VERSION}/${ASSET}"

dest="$(cd "$(dirname "$0")" && pwd)/${ASSET}"

if [[ -f "$dest" ]] && echo "${OBJDIFF_SHA256}  ${dest}" | sha256sum -c --status; then
    echo "objdiff ${OBJDIFF_VERSION} already present: ${dest}"
    exit 0
fi

tmp="$(mktemp "${dest}.XXXXXX")"
trap 'rm -f "$tmp"' EXIT
curl -fsSL -o "$tmp" "$URL"
echo "${OBJDIFF_SHA256}  ${tmp}" | sha256sum -c --status || {
    echo "error: checksum mismatch for ${URL}" >&2
    exit 1
}
chmod +x "$tmp"
mv "$tmp" "$dest"
trap - EXIT
echo "Installed objdiff ${OBJDIFF_VERSION}: ${dest}"
