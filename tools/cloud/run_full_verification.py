#!/usr/bin/env python3
"""Execute full function verification and generate CSV/MD reports on Colab."""

import os
import sys
import subprocess
import time
import json
import glob
from pathlib import Path
import pandas as pd

def main():
    halo_dir = Path("/content/halo")
    os.chdir(halo_dir)
    print("=" * 80)
    print("  HALO CE XBOX (BUILD 2276) — FULL FUNCTION BYTE-MATCHING SWEEP")
    print("  Target Binary: cachebeta.xbe | Branch: mainrevised2")
    print("=" * 80)
    
    start_time = time.time()
    
    # 1. Run populate
    cmd = ["python3", "tools/verify/raw_xbe_structural.py", "populate", "--workers", "2"]
    print(f"\n>>> [1/2] Running raw_xbe_structural.py populate across all TUs...")
    print(f"Command: {' '.join(cmd)}\n")
    
    res = subprocess.run(cmd, text=True)
    pop_time = time.time() - start_time
    print(f"\nPopulate finished with return code {res.returncode} in {pop_time/60:.2f} minutes")
    
    # 2. Generate CSV spreadsheet and Markdown document
    print(f"\n>>> [2/2] Generating byte_match_report.csv and byte_match_report.md...")
    records_dir = halo_dir / "artifacts" / "raw_xbe_structural"
    json_files = [f for f in records_dir.glob("*.json") if f.name != "summary.json"]
    print(f"Found {len(json_files)} function audit records to aggregate...")
    
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

    md_path = records_dir / "byte_match_report.md"
    summary_lines = [
        "# Halo CE Xbox (Build 2276) — Full Function Byte-Match Verification Report\n",
        f"**Target Binary:** Pristine `cachebeta.xbe` (MD5: `c7869590a1c64ad034e49a5ee0c02465`)  ",
        f"**Compiler:** MSVC 7.1 (`CL.Exe` v13.10.3052)  ",
        f"**Total Functions Audited:** {len(df):,}\n\n",
        "## 📊 Category Breakdown\n\n",
        df["Category"].value_counts().to_markdown(),
        "\n\n## 🏆 100% Byte Matching Functions (structural exact)\n\n",
    ]

    exact_df = df[df["Category"] == "100% Byte Match"][["Address", "Function", "Source TU", "Matching Bytes"]]
    summary_lines.append(f"**Total 100% Exact Matches:** {len(exact_df):,}\n\n")
    if not exact_df.empty:
        summary_lines.append(exact_df.head(100).to_markdown(index=False))
        if len(exact_df) > 100:
            summary_lines.append(f"\n\n*(Showing top 100 of {len(exact_df):,} exact matches. See byte_match_report.csv for full list)*\n")

    summary_lines.append("\n\n## ⚠️ Functions Requiring Remediation (Needs Work)\n\n")
    needs_work_df = df[df["Category"] != "100% Byte Match"][["Address", "Function", "Accuracy %", "Difference Count", "Top Differences", "Source TU"]]
    summary_lines.append(f"**Total Functions Needing Work:** {len(needs_work_df):,}\n\n")
    if not needs_work_df.empty:
        summary_lines.append(needs_work_df.head(100).to_markdown(index=False))
        if len(needs_work_df) > 100:
            summary_lines.append(f"\n\n*(Showing top 100 of {len(needs_work_df):,} functions needing work. See byte_match_report.csv for full list)*\n")

    with open(md_path, "w", encoding="utf-8") as f:
        f.write("\n".join(summary_lines))
    print(f"✓ Saved summary document: {md_path}")
    
    total_time = time.time() - start_time
    print(f"\n" + "=" * 80)
    print(f"🎉 VERIFICATION & REPORT GENERATION COMPLETE in {total_time/60:.2f} minutes!")
    print("=" * 80)
    print("\nSummary Counts:")
    print(df["Category"].value_counts().to_string())

if __name__ == "__main__":
    main()
