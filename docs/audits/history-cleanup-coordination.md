# History cleanup coordination - prepared, not executed

The user subsequently published a partial `delinked/` rewrite. See the
[follow-up](partial-rewrite-followup-2026-10-02.md) for current tips and the remaining
purge targets. The final coordinated purge and hosting/fork steps remain pending.

Primary publication: [stianeklund/halo](https://github.com/stianeklund/halo).
This supplements the [audit and one-time rewrite plan](provenance-audit-2026-10-02.md).
No history rewrite, force-push, fork-owner message or support request is authorized
by this document. The private backups must never be pushed or attached to a PR.

## Current hosting state

Read-only checks on 2026-10-02 found public `main` at
`0e6f309a563a926f5e52adf6087eaafb1226246a`, five other branch tips, no advertised
tags, no open PRs, no main branch-protection rule and no repository rulesets.
The local pre-hardening main was 210 commits ahead, so a safeguard publication
also requires review of the previously unpublished commits.

The CI workflow is `Provenance Artifact Guard`; the status check to require is the
job name **Reject proprietary artifacts**. Publish the reviewed workflow, confirm
a successful run on the intended publication branch, and then require that check
for main updates. Contributor hooks are installed with
`rtk git config core.hooksPath tools/hooks`; verify that the wrapper is executable.

## Coordination inventory

- Primary main has 12 purge paths in its ancestry; five other primary branch
  histories had none in the inspected inventory. Preserve those clean histories.
- 29 historical primary PR heads contain purge paths. PR #1 contains 238 paths.
  PR #25/#28 each contain 16, including the never-merged whole-binary exports.
- PR #25 is closed and unmerged, head
  `e9b5d758a525cccd5cba69be343b2f6f937be054` from `nickarcade/halo:branch3revised`.
- 35 affected fork branches span MataPuelc0s, scudo005, USAvery, General-101,
  pastudan and nickarcade. Exact tips and paths are in
  [history-ref-exposure.tsv](history-ref-exposure.tsv) and
  [history-public-ref-paths.tsv](history-public-ref-paths.tsv).
- Private refs include 1,055 affected commit refs and 16 non-commit tree snapshot
  refs. Preserve privately; exclude them from any publication ref allowlist.

## Ready-to-send fork-owner draft

We are preparing a coordinated provenance cleanup of stianeklund/halo. Our audit
identified original-game-derived objects, live captures and whole-binary exports
in historical commits reachable from your fork. The attached metadata inventory
identifies your affected branch tips and paths; it contains no game bytes. Please
coordinate a publication freeze and a single cleanup with us so legitimate source
commits remain available. We will provide the reviewed purge set and old/new commit
mapping after verification. Please avoid merging old history into the cleaned
branches. We need confirmation of which refs you control and whether you can
rewrite or retire affected refs. This request has not yet been sent.

## Ready-to-send hosting inquiry

We have identified proprietary original-game-derived artifacts in historical
commits and closed, unmerged PRs in stianeklund/halo, including PR #25/#28. We are
preparing one verified filter-repo rewrite of refs we control. Please advise which
process, if any, can retire cached views and read-only PR/fork-network references
for this type of provenance cleanup, and what evidence you need. We understand
that your sensitive-data-removal process has restricted eligibility; we are not
representing this as exposed credentials or promising eligibility. This inquiry
has not yet been sent.

GitHub documents read-only PR refs, separate fork coordination, and restricted
Support eligibility in its
[removal guidance](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/removing-sensitive-data-from-a-repository).

## Execution gates

1. Freeze writers; refresh refs and artifact/census manifests. Validate releases,
   LFS and Actions/Pages caches separately. Reconcile newly published commits.
2. Verify private raw Git, dirty/ignored worktree and fork-census backups, plus an
   independent restoration. Retain the checksum/status records privately.
3. Agree owner-controlled publication refs and fork/hosting handling. Keep audit,
   stash, worktree and app refs out of the publication namespace.
4. Run exactly one reviewed filter-repo invocation in a disposable independent
   mirror using the 382-path and 416-blob lists. Leave comments/messages intact.
5. Prove purge IDs/paths absent, rescan all reachable publication history, verify
   retained source/metadata and unchanged clean branch trees, fsck, build and test
   an independent fresh clone. Preserve commit/ref maps.
6. Publish approved refs with recorded-tip leases; verify the remote independently.
   Record unresolved PR refs and third-party copies explicitly. Reclone contributor
   workspaces rather than merging old histories back.

Windows Git currently lacks filter-repo. The WSL installation reports version
`31ebad4c8fb3`; use the verified WSL environment in the disposable mirror.
No rewrite has been performed to test availability.
