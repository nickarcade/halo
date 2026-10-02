# Cleanup checkpoint - 2026-10-02

This checkpoint preceded the user's published partial rewrite. See the
[follow-up](partial-rewrite-followup-2026-10-02.md) for the new public tips and
verified remaining exposure.

The exact purge set remains 382 paths / 416 blobs. No history rewrite, force-push
or canonical-target migration has been performed. Hardening is a working-tree
change and ordinary local commit; removing current artifacts does not purge history.

Private recovery archives cover the common Git database, the main dirty/ignored
checkout, both linked worktrees, and the separate fetched PR/fork census. Archive
SHA-256 checksums and inventories are retained in the private backup directory.
The restored common database and an independent mirror both passed `git fsck
--full`; fifteen selected main-worktree files matched archived content hashes.
The separately restored fork-census Git database also passed `fsck` and has no
alternates dependency. No backup contents are included in this public repository.

**This is not a frozen cutover backup.** Another writer advanced
`campaign_10-02-2026` while the linked worktrees were being copied. No individual
file was observed changing during its copy and the main status stayed stable, but
linked worktree files may span commits. The private status is
`VERIFIED_RECOVERY_BACKUP_NOT_FROZEN`, with `rewrite_approved: false`. Stop writers
and capture/verify a fresh coherent snapshot before rewriting publication history.

The [retail pilot](retail-migration-pilot.md) compared fifty functions, finding
twenty-one complete normalized bodies, nineteen partial anchors and ten functions
without unique anchors. Retail evidence is kept in a hypothesis namespace; current
source addresses and canonical inputs are unchanged.

The [coordination document](history-cleanup-coordination.md) contains unsent fork
owner and hosting drafts, current protection/ref state, and execution gates.
Public main trails the pre-hardening local branch by 210 commits. Review those
commits before publication; the artifact guard should become a required check
after its first successful run on the intended publication branch.

The public local-ref snapshot uses `-` for an absent optional final column.
Its raw output is retained privately. Purge-list line endings are fixed to LF;
the updated checksums in the rewrite plan match both the working files and Git.
