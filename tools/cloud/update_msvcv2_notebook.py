#!/usr/bin/env python3
"""Generates the updated msvcv2.ipynb with modern mainrevised2 tools."""

import json
from pathlib import Path

def create_msvcv2_notebook():
    cells = []

    def md(source):
        cells.append({
            "cell_type": "markdown",
            "metadata": {},
            "source": [line + "\n" for line in source.strip().split("\n")]
        })

    def code(source, title=None):
        meta = {}
        lines = []
        if title:
            lines.append(f"# @title {title}\n")
        lines.extend([line + "\n" for line in source.strip().split("\n")])
        cells.append({
            "cell_type": "code",
            "execution_count": None,
            "metadata": meta,
            "outputs": [],
            "source": lines
        })

    # --- Header ---
    md("""# 🎮 Halo CE Xbox (Build 2276) — MSVC 7.1 Byte-Matching & Verification Pipeline

> **Repository:** [nickarcade/halo](https://github.com/nickarcade/halo)  
> **Target Branch:** `mainrevised2`  
> **Target Binary:** Pristine Halo Xbox Debug Build 2276 (`cachebeta.xbe`, MD5: `c7869590a1c64ad034e49a5ee0c02465`)  
> **Compiler Toolchain:** Microsoft Visual C++ Toolkit 2003 (`CL.Exe` v13.10.3077 / MSVC 7.1) via Wine32  
> **Active Verification Suite:**
> - `tools/verify/raw_xbe_structural.py` (relocation-aware structural & aligned byte accuracy against raw XBE)
> - `tools/verify/raw_byte_audit.py` (strict literal raw byte verification)
> - `tools/verify/vc71_verify.py` (canonical VC71 mnemonic DP-LCS compiler scoring)
> - `tools/verify/measure.py` (dormant lift tracking & byte accuracy)
> - `tools/verify/vc71_low_score_census.py` (score census & candidate emitter)
> - `tools/audit/check_lift_hazards.py` (lift safety & regression gates)

---
### 📋 Pipeline Workflow Overview
1. **Stage Assets** (`VCToolkitSetup.exe`, `cachebeta.xbe`) from `/content` or Google Drive
2. **Synchronize Workspace** from branch `mainrevised2`
3. **Install Dependencies** (Wine 32-bit, LLVM objdump, `pefile`, `pyxbe`, `pytest`, `capstone`, `pandas`, `tabulate`)
4. **Install Headless MSVC 7.1 & Configure Transparent Shims** (`/mnt/c/.../CL.Exe`, `wslpath -> winepath`)
5. **Generate Headers** (`decl.h` from `kb.json`)
6. **Smoke Test Verification** (`vc71_verify.py`)
7. **Execute Full Function Byte-Match Sweep** (`raw_xbe_structural.py populate`)
8. **Export Comprehensive Spreadsheet & Document** (`byte_match_report.csv` & `byte_match_report.md`)
9. **Inspect Dormant Lifts & Permuter Candidates** (`measure.py`, `vc71_low_score_census.py`)
10. **Run Mandatory Hazard Gates** (`check_lift_hazards.py`)""")

    # --- Step 1 ---
    code(r"""import os
import sys
import shutil
import subprocess
import hashlib
import time

print("=" * 80)
print("  HALO CE XBOX (BUILD 2276) - MSVC 7.1 & VERIFICATION ENVIRONMENT SETUP")
print("  Branch: mainrevised2 | Target: cachebeta.xbe")
print("=" * 80)

# --- 1. STAGE ASSETS (Local /content or Google Drive) ---
print("\n>>> [1/6] Staging Assets (VCToolkitSetup.exe & cachebeta.xbe)...")
drive_paths = [
    "/content",
    "/content/drive/MyDrive/MSVC 7.1",
    "/content/drive/MyDrive/Colab Notebooks",
    "/content/drive/MyDrive"
]

assets = [
    ("VCToolkitSetup.exe", "/content/VCToolkitSetup.exe"),
    ("cachebeta.xbe", "/content/cachebeta.xbe"),
]

for filename, dest in assets:
    if not os.path.exists(dest):
        for dp in drive_paths:
            candidate = os.path.join(dp, filename)
            if os.path.exists(candidate):
                print(f"  Staging {filename} from {dp}...")
                shutil.copy2(candidate, dest)
                break
    if os.path.exists(dest):
        print(f"  ✓ Asset ready: {dest} ({os.path.getsize(dest):,} bytes)")
    else:
        print(f"  ⚠ Asset missing: {filename}. Please upload to /content/{filename} or Google Drive.")

# --- 2. CLONE / UPDATE REPOSITORY (BRANCH: mainrevised2) ---
print("\n>>> [2/6] Synchronizing repository (branch: mainrevised2)...")
os.chdir("/content")
halo_dir = "/content/halo"
repo_url = "https://github.com/nickarcade/halo.git"

if not os.path.exists(os.path.join(halo_dir, ".git")):
    if os.path.exists(halo_dir):
        shutil.rmtree(halo_dir)
    print(f"  Cloning {repo_url} on branch 'mainrevised2'...")
    subprocess.run(["git", "clone", "-b", "mainrevised2", repo_url, halo_dir], check=True)
else:
    print("  Repository already present. Pulling latest mainrevised2...")
    subprocess.run(["git", "-C", halo_dir, "fetch", "origin", "mainrevised2"], check=True)
    subprocess.run(["git", "-C", halo_dir, "checkout", "mainrevised2"], check=True)
    subprocess.run(["git", "-C", halo_dir, "pull", "origin", "mainrevised2"], check=True)

# Stage pristine cachebeta.xbe into halo-patched/
target_xbe_dir = os.path.join(halo_dir, "halo-patched")
os.makedirs(target_xbe_dir, exist_ok=True)
dest_xbe = os.path.join(target_xbe_dir, "cachebeta.xbe")
if os.path.exists("/content/cachebeta.xbe"):
    if not os.path.exists(dest_xbe) or os.path.getsize(dest_xbe) != os.path.getsize("/content/cachebeta.xbe"):
        shutil.copy2("/content/cachebeta.xbe", dest_xbe)

if os.path.exists(dest_xbe):
    with open(dest_xbe, "rb") as f:
        md5 = hashlib.md5(f.read()).hexdigest()
    expected = "c7869590a1c64ad034e49a5ee0c02465"
    print(f"  ✓ cachebeta.xbe verified in halo-patched/ (MD5: {md5}) {'[MATCH]' if md5 == expected else '[MISMATCH!]'}")
else:
    print("  ⚠ cachebeta.xbe not yet in halo-patched/. Ensure it is staged for verification.")

# --- 3. SYSTEM & PYTHON DEPENDENCIES ---
print("\n>>> [3/6] Installing Linux packages (Wine32, LLVM, msitools, Python analysis tools)...")
subprocess.run(["dpkg", "--add-architecture", "i386"], check=True)
subprocess.run(["apt-get", "update", "-qq"], check=True)
subprocess.run(["apt-get", "install", "-y", "-qq", "wine32:i386", "wine", "llvm", "msitools", "xvfb", "p7zip-full"], check=True)

subprocess.run([
    "pip", "install", "-q",
    "pefile~=2023.2.7", "pyxbe~=1.0.2", "pytest", "capstone", "pandas", "openpyxl", "tabulate",
    "--break-system-packages"
], check=True)
print("  ✓ System packages & Python libraries installed successfully.")

# --- 4. HEADLESS MSVC COMPILER INSTALLATION ---
print("\n>>> [4/6] Installing Microsoft Visual C++ Toolkit 2003 in Wine...")
wine_vc_dir = os.path.expanduser("~/.wine/drive_c/Program Files/Microsoft Visual C++ Toolkit 2003")
cl_path = os.path.join(wine_vc_dir, "bin", "cl.exe")

env = os.environ.copy()
env["WINEARCH"] = "win32"
env["WINEDEBUG"] = "-all"

if not os.path.exists(cl_path):
    if not os.path.exists("/content/VCToolkitSetup.exe"):
        raise FileNotFoundError("Missing /content/VCToolkitSetup.exe! Please upload it to /content or Google Drive.")
    # Initialize wine prefix cleanly
    subprocess.run(["wineboot", "-i"], env=env, check=True)
    subprocess.run(["wineserver", "-w"], env=env, check=True)
    temp_is = os.path.expanduser("~/.wine/drive_c/temp_is")
    os.makedirs(temp_is, exist_ok=True)
    print("  Extracting MSI from VCToolkitSetup.exe package...")
    subprocess.run(["wine", "/content/VCToolkitSetup.exe", "/a", "/bC:\\temp_is"], env=env, capture_output=True, timeout=60)
    msi_path = os.path.join(temp_is, "Microsoft Visual C++ Toolkit 2003.msi")
    if not os.path.exists(msi_path):
        # Fallback to searching temp directories
        for root, dirs, files in os.walk(os.path.expanduser("~/.wine/drive_c")):
            if "Microsoft Visual C++ Toolkit 2003.msi" in files:
                msi_path = os.path.join(root, "Microsoft Visual C++ Toolkit 2003.msi")
                break
    extract_dir = "/content/vctoolkit_extracted"
    os.makedirs(extract_dir, exist_ok=True)
    print(f"  Unpacking compiler files from {os.path.basename(msi_path)} via msiextract...")
    subprocess.run(["msiextract", "--directory", extract_dir, msi_path], check=True)
    src_dir = os.path.join(extract_dir, "Program Files", "Microsoft Visual C++ Toolkit 2003")
    os.makedirs(os.path.dirname(wine_vc_dir), exist_ok=True)
    if os.path.exists(wine_vc_dir):
        shutil.rmtree(wine_vc_dir)
    shutil.copytree(src_dir, wine_vc_dir)
    subprocess.run(["wineserver", "-w"], env=env)

if not os.path.exists(cl_path):
    raise RuntimeError(f"cl.exe not found at {cl_path} after installation!")

print(f"  ✓ MSVC Toolkit compiler verified: {cl_path}")

# --- 5. CONFIGURE TRANSPARENT WINE SHIMS FOR REPO SCRIPTS ---
print("\n>>> [5/6] Configuring transparent WSL/Wine shims for vc71_verify.py and raw_xbe_structural.py...")
# 1) wslpath shim -> winepath
with open("/usr/local/bin/wslpath", "w") as f:
    f.write('#!/bin/bash\nif [ "$1" = "-w" ]; then winepath -w "$2"; elif [ "$1" = "-u" ]; then winepath -u "$2"; else winepath "$@"; fi\n')
os.chmod("/usr/local/bin/wslpath", 0o755)

# 2) CL.Exe launcher (intercepts VC71_CL_WSL from vc71_verify.py)
rxdk_bin = "/mnt/c/Program Files (x86)/RXDK/xbox/bin/vc71"
os.makedirs(rxdk_bin, exist_ok=True)
with open(os.path.join(rxdk_bin, "CL.Exe"), "w") as f:
    f.write('#!/bin/bash\nexport WINEARCH=win32\nexport WINEDEBUG=-all\ncl="C:\\\\Program Files\\\\Microsoft Visual C++ Toolkit 2003\\\\bin\\\\cl.exe"\nexec wine "$cl" "$@"\n')
os.chmod(os.path.join(rxdk_bin, "CL.Exe"), 0o755)

# 3) RXDK include symlink
rxdk_inc = os.path.expanduser("~/.wine/drive_c/Program Files (x86)/RXDK/xbox/include")
vc_inc = os.path.expanduser("~/.wine/drive_c/Program Files/Microsoft Visual C++ Toolkit 2003/include")
os.makedirs(os.path.dirname(rxdk_inc), exist_ok=True)
if not os.path.exists(rxdk_inc):
    os.symlink(vc_inc, rxdk_inc)

print("  ✓ Shims configured: /usr/local/bin/wslpath -> winepath, /mnt/c/.../CL.Exe -> wine cl.exe.")

# --- 6. GENERATE DECL.H ---
print("\n>>> [6/6] Generating build/generated/decl.h from kb.json...")
os.chdir(halo_dir)
os.makedirs("build/generated", exist_ok=True)
res_decl = subprocess.run(["python3", "tools/analysis/knowledge.py", "--gen-header", "build/generated/decl.h"], capture_output=True, text=True)
if res_decl.returncode == 0:
    print(f"  ✓ decl.h generated successfully ({os.path.getsize('build/generated/decl.h'):,} bytes)")
else:
    print("  ⚠ Note on decl.h generation:", res_decl.stderr.strip()[:200])

print("\n" + "=" * 80)
print("🎉 ENVIRONMENT READY FOR VERIFICATION & BYTE-MATCHING ON mainrevised2!")
print("=" * 80)""", title="🛠️ Step 1: Environment Setup & Toolchain Installation")

    # --- Step 2 ---
    md("""### 🧪 Step 2: Smoke Test — MSVC 7.1 Single Function Verification
Test that `vc71_verify.py` compiles and scores a function cleanly using the transparent Wine toolchain.""")

    code(r"""# @title 🔍 Run Single Function Verification Smoke Test
target_tu = "src/halo/interface/event_manager.c" # @param {type:"string"}
target_func = "FUN_000dc000" # @param {type:"string"}

import subprocess, os

os.chdir("/content/halo")
cmd = ["python3", "tools/verify/vc71_verify.py", target_tu]
if target_func:
    cmd.extend(["--function", target_func])
cmd.append("--no-cache")

print(f"Executing: {' '.join(cmd)}\n")
res = subprocess.run(cmd, capture_output=True, text=True)
print(res.stdout)
if res.stderr:
    print("STDERR:", res.stderr)""", title="🔍 Run Single Function Verification Smoke Test")

    # --- Step 3 ---
    md("""---
## 🔬 Step 3: Full Function Relocation-Aware Byte-Matching Sweep
Runs `tools/verify/raw_xbe_structural.py populate` across all ported functions in the codebase.
This compares each candidate COFF function against pristine `cachebeta.xbe`, masking only verified relocation operands (e.g. symbol targets and absolute data references), and writes structured audit records into `artifacts/raw_xbe_structural/`.""")

    code(r"""# @title 📊 Run Full Function Structural & Byte-Match Audit
workers = 4 # @param {type:"integer"}
specific_source = "" # @param {type:"string"}

import subprocess, os, time

os.chdir("/content/halo")
cmd = ["python3", "tools/verify/raw_xbe_structural.py", "populate", "--workers", str(workers)]
if specific_source.strip():
    cmd.extend(["--source", specific_source.strip()])

print(f"Executing: {' '.join(cmd)}\n")
start_time = time.time()
res = subprocess.run(cmd, text=True)
elapsed = time.time() - start_time
print(f"\nAudit completed in {elapsed/60:.2f} minutes (exit code: {res.returncode})")""", title="📊 Run Full Function Structural & Byte-Match Audit")

    # --- Step 4 ---
    md("""---
## 📈 Step 4: Comprehensive Spreadsheet & Status Report Generation
Aggregates all JSON audit records from `artifacts/raw_xbe_structural/` into:
1. `artifacts/raw_xbe_structural/byte_match_report.csv` (complete spreadsheet with all metrics)
2. `artifacts/raw_xbe_structural/byte_match_report.md` (formatted summary document)
3. Interactive DataFrame tables displaying **100% Byte Matching** functions and functions requiring work.""")

    code(r"""# @title 📑 Generate Byte-Match Spreadsheet & Report Document
import os
import glob
import json
import pandas as pd
from pathlib import Path

os.chdir("/content/halo")
records_dir = Path("artifacts/raw_xbe_structural")
json_files = list(records_dir.glob("*.json"))
json_files = [f for f in json_files if f.name != "summary.json"]

print(f"Processing {len(json_files)} function audit records...")

rows = []
for f in json_files:
    try:
        data = json.loads(f.read_text(encoding="utf-8"))
    except Exception:
        continue
    
    addr = data.get("address", "")
    addr_hex = f"0x{addr:08x}" if isinstance(addr, int) else str(addr)
    func_name = data.get("function", "")
    tu_source = data.get("source", "")
    verdict = data.get("verdict", "unknown")
    
    aligned = data.get("aligned_byte_match") or {}
    status = aligned.get("status", "")
    matching_bytes = aligned.get("matching_bytes", 0) or 0
    compared_bytes = aligned.get("compared_bytes", 0) or 0
    accuracy = (matching_bytes / compared_bytes * 100.0) if compared_bytes > 0 else 0.0
    
    # Differences breakdown
    diffs = aligned.get("differences") or []
    diff_kinds = {}
    for d in diffs:
        k = d.get("kind", "unknown")
        diff_kinds[k] = diff_kinds.get(k, 0) + 1
    top_diffs = ", ".join(f"{k}:{v}" for k, v in sorted(diff_kinds.items(), key=lambda p: -p[1])[:3])
    
    is_100_percent = (verdict == "structural exact")
    
    category = "100% Byte Match" if is_100_percent else (
        ">=95% Match" if accuracy >= 95.0 else (
            "85-95% Match" if accuracy >= 85.0 else (
                "<85% Match" if status == "scored" else verdict
            )
        )
    )

    rows.append({
        "Address": addr_hex,
        "Function": func_name,
        "Source TU": tu_source,
        "Verdict": verdict,
        "Accuracy %": round(accuracy, 2) if status == "scored" else None,
        "Matching Bytes": matching_bytes,
        "Compared Bytes": compared_bytes,
        "Difference Count": len(diffs),
        "Top Differences": top_diffs,
        "Category": category,
        "Reason": data.get("reason", "")
    })

df = pd.DataFrame(rows)
if not df.empty:
    df.sort_values(by=["Category", "Accuracy %", "Function"], ascending=[True, False, True], inplace=True)

csv_path = records_dir / "byte_match_report.csv"
df.to_csv(csv_path, index=False)
print(f"✓ Saved spreadsheet: {csv_path} ({len(df)} rows)")

# Generate Markdown Document
md_path = records_dir / "byte_match_report.md"
summary_lines = [
    "# Halo CE Xbox (Build 2276) — Full Function Byte-Match Verification Report\n",
    f"**Total Functions Audited:** {len(df)}\n",
    "## Category Breakdown\n",
    df["Category"].value_counts().to_markdown(),
    "\n\n## 🏆 100% Byte Matching Functions (structural exact)\n",
]

exact_df = df[df["Category"] == "100% Byte Match"][["Address", "Function", "Source TU", "Matching Bytes"]]
summary_lines.append(f"**Total 100% Exact Matches:** {len(exact_df)}\n\n")
if not exact_df.empty:
    summary_lines.append(exact_df.head(100).to_markdown(index=False))
    if len(exact_df) > 100:
        summary_lines.append(f"\n*(Showing top 100 of {len(exact_df)} exact matches. See CSV for full list)*\n")

summary_lines.append("\n\n## ⚠️ Functions Requiring Remediation (Needs Work)\n")
needs_work_df = df[df["Category"] != "100% Byte Match"][["Address", "Function", "Accuracy %", "Difference Count", "Top Differences", "Source TU"]]
summary_lines.append(f"**Total Functions Needing Work:** {len(needs_work_df)}\n\n")
if not needs_work_df.empty:
    summary_lines.append(needs_work_df.head(100).to_markdown(index=False))
    if len(needs_work_df) > 100:
        summary_lines.append(f"\n*(Showing top 100 of {len(needs_work_df)} functions needing work. See CSV for full list)*\n")

with open(md_path, "w", encoding="utf-8") as f:
    f.write("\n".join(summary_lines))
print(f"✓ Saved summary document: {md_path}")

# Display Summary in Notebook
print("\n" + "=" * 60)
print("📊 VERIFICATION SUMMARY:")
print("=" * 60)
print(df["Category"].value_counts())
print("=" * 60)""", title="📑 Generate Byte-Match Spreadsheet & Report Document")

    # --- Step 5 ---
    md("""---
## 🔍 Step 5: Strict Literal Raw-Byte Audit (`raw_byte_audit.py`)
Performs a strict literal, byte-for-byte comparison between candidate section bytes and the committed span in `cachebeta.xbe` (no relocation masking).
Verdicts: `raw-byte exact`, `bytes differ`, or `not comparable`.""")

    code(r"""# @title 🔬 Run Strict Literal Byte Audit for Single Function
source_tu = "src/halo/interface/event_manager.c" # @param {type:"string"}
func_name = "FUN_000dc000" # @param {type:"string"}
address_hex = "0x000dc000" # @param {type:"string"}

import subprocess, os

os.chdir("/content/halo")
cmd = [
    "python3", "tools/verify/raw_byte_audit.py",
    source_tu,
    "--function", func_name,
    "--address", address_hex
]
res = subprocess.run(cmd, capture_output=True, text=True)
print(res.stdout)
if res.stderr:
    print(res.stderr)""", title="🔬 Run Strict Literal Byte Audit for Single Function")

    # --- Step 6 ---
    md("""---
## 📦 Step 6: Dormant Lift Census & Worklist (`measure.py`)
Identifies functions in `kb.json` that are marked `ported != true` but whose implementations already exist in `src/`.
Compiles each dormant lift with VC71 and measures its byte accuracy against `cachebeta.xbe`.""")

    code(r"""# @title 📋 List Dormant Lifts & Generate Worklist
limit = 50 # @param {type:"integer"}

import subprocess, os

os.chdir("/content/halo")
cmd = ["python3", "tools/verify/measure.py", "--list"]
res = subprocess.run(cmd, capture_output=True, text=True)
print(res.stdout)
if res.stderr:
    print(res.stderr)""", title="📋 List Dormant Lifts & Generate Worklist")

    # --- Step 7 ---
    md("""---
## 🎯 Step 7: Low-Score Census & Permuter Campaign Candidates (`vc71_low_score_census.py`)
Emits candidates for score recovery campaigns (typically functions scoring between 85% and 98% that need instruction re-ordering or compiler register tuning).""")

    code(r"""# @title 🎯 Emit Candidates for Score Improvement (85% - 98%)
min_score = 85.0 # @param {type:"number"}
max_score = 98.0 # @param {type:"number"}

import subprocess, os

os.chdir("/content/halo")
cmd = [
    "python3", "tools/verify/vc71_low_score_census.py",
    "--candidates",
    "--min-score", str(min_score),
    "--max-score", str(max_score),
    "--format", "tsv"
]
res = subprocess.run(cmd, capture_output=True, text=True)
print(res.stdout[:2000])
if len(res.stdout) > 2000:
    print(f"\n... (truncated {len(res.stdout)-2000} bytes)")""", title="🎯 Emit Candidates for Score Improvement")

    # --- Step 8 ---
    md("""---
## 🛡️ Step 8: Mandatory Safety & Lift Hazard Scan
Verifies that no undefined behaviors, register hazards, buffer under-sizes, or dropped compiler intrinsics exist in the codebase.""")

    code(r"""# @title 🛡️ Run Lift Hazard Scan
scan_all = False # @param {type:"boolean"}

import subprocess, os

os.chdir("/content/halo")
cmd = ["python3", "tools/audit/check_lift_hazards.py"]
if not scan_all:
    cmd.append("--changed-only")

print(f"Running: {' '.join(cmd)}\n")
res = subprocess.run(cmd, capture_output=True, text=True)
print(res.stdout)
if res.stderr:
    print(res.stderr)""", title="🛡️ Run Lift Hazard Scan")

    nb = {
        "cells": cells,
        "metadata": {
            "colab": {"provenance": []},
            "kernelspec": {"display_name": "Python 3", "name": "python3"},
            "language_info": {"name": "python"}
        },
        "nbformat": 4,
        "nbformat_minor": 2
    }

    out_path = Path("/storage/1F34-EBBE/halo/msvcv2.ipynb")
    out_path.write_text(json.dumps(nb, indent=2), encoding="utf-8")
    print(f"Successfully wrote {out_path} ({out_path.stat().st_size:,} bytes, {len(cells)} cells)")

if __name__ == "__main__":
    create_msvcv2_notebook()
