# User-published partial rewrite - 2026-10-02

The user ran filter-repo on `delinked/` in a separate clone and force-pushed
`stianeklund/halo`. Public main moved from
`0e6f309a563a926f5e52adf6087eaafb1226246a` to
`33623ec723213a6576c679e7e701d628a737ba45`; gh-pages also changed. Comparing old/new
main tip trees found no added, removed or modified files. This step removed
historical delinked content, but did not complete the reviewed purge set.

## Verified remaining exposure

Seven reviewed blob versions remain reachable from rewritten main, across four
paths:

- `tools/equivalence/known_globals.json`
- `tools/equivalence/_global_bytes.py`
- `tools/equivalence/regression_snapshots/evaluate_game_complexity.json`
- `tools/equivalence/regression_snapshots/player_count.json`

Across the rewritten clone's 37 refs, 38 reviewed blob versions remain across 36
purge paths. These include the PR #25 exports, additional captured state, original
waveform tables and `ci/snapshot.json`. This is a local-ref inventory, not a claim
that every local PR ref was updated on GitHub. The clone contains 29 locally
rewritten PR heads as well as its six branch heads and two remote-tracking refs.

PR #25 demonstrates the distinction: the rewritten local PR head is
`a90f8b99c3f671ea4a72a2d6e696dc0e2dfb57d5`, while GitHub still advertises original
`e9b5d758a525cccd5cba69be343b2f6f937be054`. Normal pushes cannot update that
read-only hosted ref. Fork and hosting coordination remain necessary.

A full read-only census of the rewritten local object database inspected 10,138
stored commits, 20,300 blobs and 10,934,253,495 uncompressed blob bytes. It found
91 candidates for review; candidate flags are not automatically purge targets.
In particular, independently constructed synthetic regression fixtures must remain.
The rewrite records and post-rewrite inventory are preserved in the private backup.

## Reconciliation and final cutover

Preserve the current old-history worktree and linked branches privately. The
unpublished 210-commit development series and the local hardening changes must be
preserved and transferred through the final commit map before publication. Pushing
or merging the old ancestry back would restore artifacts already removed.

Use one coordinated final purge pass with the full reviewed path/blob set in an
independent disposable mirror. The earlier user-published partial pass is recorded
here; it is not hidden or treated as a complete cleanup. Refresh all publication
tips and record leases against the newly published tips before the next cutover.
Freeze all writers first, refresh the census, verify the private frozen backup,
preserve legitimate source changes, rescan, build/test and verify a fresh clone.
Use the existing [rewrite plan](provenance-audit-2026-10-02.md) and
[coordination package](history-cleanup-coordination.md), with this updated state.

## Hosted artifact screening

Read-only hosting checks found no releases, 83 downloadable Actions artifacts and
eight small measured-leaf caches. All 83 artifacts were privately preserved and
screened. Fourteen archives produced 258 path-name flags; all flagged files were
generated batch comparison reports with schema version 2, rather than raw state
captures. Independent content-signature checks found no additional flags in them.
No new proprietary artifact package was confirmed by this screening, and no
Actions artifact/cache was deleted. Signature screening does not establish all
content provenance. Refresh and review hosting surfaces again during the freeze.
