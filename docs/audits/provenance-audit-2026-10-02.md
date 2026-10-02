# Provenance audit - 2026-10-02

This is the initial audit checkpoint. The user's later published `delinked/`
rewrite and remaining exposure are recorded in
[partial-rewrite-followup-2026-10-02.md](partial-rewrite-followup-2026-10-02.md).

The confirmed purge set is **382 historical paths / 416 blob IDs**. No history
rewrite, ref update, push, deployment, or canonical-target migration was performed.
Existing uncommitted provenance edits were preserved and extended. Source layouts
and implementation bodies were not broadly replaced.

## Confirmed history-purge targets

| Category | Paths | Evidence / disposition |
|---|---:|---|
| Original-XBE-derived COFF objects | 326 | Delinking origin and COFF headers; includes the eight known original objects and other branch/stash/PR histories |
| Raw RAM/game-state captures | 21 | Eight binary captures plus thirteen JSON captures, including ten current regression fixtures and ci/snapshot.json |
| Whole-binary exports from PR #25 | 4 | halo_2276_functions.txt; halo_decompiled/cachebeta.elf.c, .h and .asm |
| Embedded target/runtime byte corpora | 2 | tools/equivalence/_global_bytes.py and known_globals.json, including historical variants |
| Original waveform tables | 18 | tests/golden/periodic_tables/*.bin; producer documents capture from original executable |
| Original section seed copies | 8 | build-standalone-nogpu/generated/seed_* and build-standalone-seed/generated/seed_*; byte comparisons match original .data/BINKDATA and .rdata/DOLBY prefixes |
| Patched/seeded game executables | 3 | build-standalone{,-nogpu,-seed}/default.xbe; XBE signatures and original section content |

Exact files:

No committed original PDB/debug-symbol package or original-game linker map was
confirmed. The map candidates reviewed were CSS source maps or independently
produced linker output. User-owned local XBEs and disc images are research inputs,
not repository artifacts to purge.

- [Manifest with introduction commits and reasons](history-purge-manifest.tsv).
- [Every addition/modification/deletion commit and old/new blob](history-purge-revisions.tsv).
- [Literal path set](history-purge-paths.txt) and [exact blob IDs](history-purge-blobs.txt).
- [Affected ref tips](history-ref-exposure.tsv) and [per-public-ref paths](history-public-ref-paths.tsv).

The eight known objects were introduced by fb09a81f2ea18eb35fc45941fa78092429786f34
(rasterizer lights) and f6e4d67c7f72daa06252a2514719eb34658d72c5 (seven telnet objects),
then deleted by 24ae0ed40036bb9028ab4eb917254d12042e26a5. Deletion did not remove history.
PR #25 is closed/unmerged; its historical head is e9b5d758a525cccd5cba69be343b2f6f937be054.
Its export remains reachable through PR #25/#28 and nickarcade/halo main_with_revised.

Coverage: 1,099 original local refs; 27,609 reachable blobs; all 33,906 locally
stored blobs including reflog/unreachable objects; another 9,200 blobs from
separately fetched advertised public/PR/fork histories. Combined scanning covered
32,287,175,863 bytes and 4,870 historical path aliases. Alias enumeration used
root and every merge-parent diff, rather than trusting rev-list's one path per blob.
The inventory is complete for the inspected local database and advertised refs;
it cannot enumerate inaccessible private refs or third-party copies.

All 29 advertised primary PR heads are affected. Primary main has 12 purge paths
in ancestry; PR #1 has 238; PR #25/#28 have 16. The other five primary branch
histories are clean for this purge set. No advertised primary tags or releases
were found. 1,055 local commit refs, 16 additional private Codex tree-snapshot refs, and
35 advertised fork branches are affected. Tree snapshots are inventoried separately
in history-tree-snapshots.tsv; their three artifact paths/blob IDs are already
in the purge set. Preserve them privately and exclude them from publication.
The six downstream forks inspected are MataPuelc0s, scudo005, USAvery, General-101,
pastudan and nickarcade. The exposure table names every affected branch/tip.
Recheck refs and releases during the publication freeze; this is a point-in-time census.

## Current-tree disposition

| Item | Classification | Concrete action / recommendation |
|---|---|---|
| docs/halocea/ | KEEP as HIST; REMOVE FROM PUBLIC WORKFLOW as authority | Preserve metadata and disclosure; historical transfer advice explicitly superseded |
| ghidra_scripts/CeaPdbExtract.java | REMOVE FROM PUBLIC WORKFLOW | Existing deletion retained; no history purge of an independently written extractor merely because it mentions CEA |
| ghidra_scripts/ApplyPalCorroboration.java | REMOVE FROM PUBLIC WORKFLOW | Existing deletion retained; automatic corroboration does not independently prove a target fact |
| tools/archive/extract_pal_prototype_symbols.py | ARCHIVE/LOCAL-ONLY | Existing public deletion retained; private experiments may suggest hypotheses only |
| tools/archive/match_pal_names_to_ida.py | ARCHIVE/LOCAL-ONLY | Existing public deletion retained; no automatic promotion of matched names |
| tools/bytematch/pal_campaign.py | REWRITE | Eleven-line compatibility shim now routes to target_campaign.py; PAL corpus access/ranking/body lookup removed; queue, measurements, fingerprints, snapshots and gates preserved |
| .claude/skills/byte-campaign/ and command | REMOVE FROM PUBLIC WORKFLOW | Existing deletion retained; no matching/PAL-source campaign remains advertised |
| .claude/skills/recover-campaign/ | REWRITE | Target-only recovery doctrine and explicit historical disclosure retained |
| Remaining CeaTypeExtract/CeaApplyRenames/CeaImportTypes/CeaImportEnums/CeaDumpNames/CeaAssertLines | ARCHIVE/LOCAL-ONLY recommendation | Historical inventory retained; default workflow no longer recommends them. Quarantine any derived outputs. Generic validation/extraction components may be rewritten for explicit target evidence |
| known_globals.json / global-byte extraction | REWRITE / LOCAL-ONLY outputs | Committed capture removed; loaders/extractors use ignored local artifacts and original target data; corpus-dependent self-test replaced with constructed bytes |
| Ten live regression JSONs | ARCHIVE/LOCAL-ONLY | Preserved under ignored host_snapshots/provenance-local; regression configuration updated. Missing inputs remain explicit prerequisite skips, not verification passes |
| Target-only Ghidra exporters, KB renaming, runtime/disassembly helpers | KEEP | Useful independent research tooling; raw outputs must remain local |

No new binary inputs were copied into the public tree. Local original executables
and captures are retained for research. Moving an artifact out of the current tree
is separate from removing its historical Git blobs.

## Types and implementations

[The ledger](provenance-remediation-ledger.md) distinguishes historical influence,
new independent evidence and pending work. src/types.h was audited first:
137 explicit external-origin claim lines were inventoried; 17 unused externally
suggested field names in prop_t and actor_debug_info_t were neutralized without
changing storage or layout. Unknown accessed bytes use field_<offset>; unobserved
padding uses pad_<offset>. Existing target-proven layouts and ABI were preserved.
Offsets supported only by old comments remain review leads, not fresh proof.

Priority substantive cases: action_wait_perform's explicitly PAL-shaped control
flow; object_type_* callback loops and region termination; glow_initialize's
allocation shape; perception filters; AI communication predicates; fog lifecycle
and structures. actor_combat_update is a large inactive implementation with
externally assisted actor/debug layout dependencies; confirm body influence
historically before deciding reconstruction scope. Parameter/local spelling-only
cases are lower priority. Source discovery found 559 explicit claims in 74 files.

actor_compute_prop_unopposable @0x2fc20 received a fresh target CFG review and
49,152 constructed model cases, checking return, datum lookup handles, complete
actor/prop state, store widths and ordering. All 71 instruction addresses executed.
This establishes the documented target/model domain; compiled-candidate and
live-game verification remain open. Broader type/function re-verification has
not been represented as complete.

## Evidence policy

[PROVENANCE.md](../../PROVENANCE.md) defines TGT-BIN, TGT-RUN, TGT-TEST,
OPEN-SRC, EXT-HYP and HIST. Behavior authority and acquisition provenance are
recorded separately. External material may suggest an inspection hypothesis;
implementation bodies, source-level shape, layouts, local names and parameter
names require independent target evidence. Historical disclosures stay visible.
Current debug 2276 acquisition has not been established by owning retail discs.

## Automated safeguards and verification

The pre-commit guard scans staged blobs, not working copies. This checkout now
uses the portable tools/hooks entry point; its previous absolute Linux hook path
was ineffective in Windows Git. Contributor clones must install the hook and
preserve its executable bit. CI scans the checked
out tree and every introduced revision in a push/PR range, including artifacts
subsequently deleted. Path rules and content checks cover original/patched
executables, game assets, COFF objects, PDBs, dumps/recordings, raw hex captures,
large generated exports and inspected ZIP/gzip contents. Unsupported/encrypted
or oversized compressed packages fail closed. Metadata exceptions still undergo
content checks; eight reviewed constructed memory fixtures have exact hashes.
These are repository enforcement heuristics, not proof of provenance for arbitrary
renamed data. Review remains required, and branch protection must require the CI job.

Validation: current tracked tree artifact guard PASS; 27 guard/campaign unit tests
PASS; target model 49,152 cases PASS; full local source build and regenerated
ignored patched XBE PASS; changed-source hazard scan PASS; shared-data harness
17 tests PASS with synthetic file/BSS data. No hardware/live-game verification
was performed. The broader suite initially reported stale-XBE and removed-corpus
dependencies; those causes were addressed and affected suites rerun: shared-data 17/17 and
inflate 20/20 cases passed, with 13/13 lifted inflate functions executed.

## Retail NTSC investigation

User reports physical NTSC/PAL ownership and supplied a retail XBE path. Its MD5
is e7b18148a665a13c6ed710f710e5c44b; SHA-256
ed3a8e962351ad6c4b3b620768fb6a0bda658963390252439ea036d5ede3a3ac.
It matches the USA disc-image root executable. It identifies as release 2276;
version strings alone do not distinguish the debug/prototype acquisition source.
The earlier prototype-folder final.xbe has identical executable/data sections to
this NTSC image; only 560 bytes in the first 4096-byte header differ.

[Machine-readable measurements](retail-target-comparison.json): debug .text is
1,919,436 bytes; NTSC retail .text is 1,506,880. Twelve-instruction windows
normalize register/type/width/mnemonics and address/branch operands, retain
non-address immediates and exclude padding. Of 542,153 meaningful debug windows,
42,038 appear in retail (7.754%); 30,820 have a unique retail occurrence (5.685%).
1,008 of 6,298 eligible debug functions have a unique anchor, with 989 having
a majority consistent byte delta. These rates are structural anchor coverage,
not percentages of shared behavior or validated function matches. PAL is similar.

Migration would materially improve the documented acquisition basis when the
canonical input is independently ripped from the owned retail disc. It does not
erase historical external influence or turn existing work into a clean-room
implementation. Preserve independently derived C, models, algorithms and layout
proof, then validate applicability per retail function. Debug assertions,
optimization, ABI, global placement and platform/library changes prevent a simple
address rebase. Pilot 30-50 diverse anchored functions, verify boundaries/call
sites/globals, then estimate the unmatched tail. No migration or KB rewrite occurred.

## Safe one-time rewrite plan - prepared, not executed

1. Freeze writers/publication and re-enumerate primary heads/tags, PR head/merge
   refs, all local/remotes/stashes/worktree refs and the six fork inventories.
   Recheck releases, LFS pointers/objects, Actions caches/artifacts and Pages
   deployments. LFS-specific exposure was not established; inspect it explicitly during
   the freeze. Hosting caches are a separate deletion surface. Stop if new proprietary material appears.
2. Preserve the entire dirty worktree, index, ignored research inputs and all
   linked worktrees in private backups. Resolve git-common-dir and back up its
   object database, refs, reflogs, config and alternates with writers stopped.
   A mirror/bundle alone omits unreachable/reflog-only objects. Also create an
   independent mirror/bundle of every intended published ref and verify restoration
   in an isolated directory. Backups contain proprietary material: never publish them.
3. Agree the publication ref allowlist and owners' fork/PR handling. Rewrite only
   a disposable independent mirror, without alternates to the working repository.
   Include affected PR/fork histories needed for preservation as temporary audit
   refs. Record original tips and retain legitimate commits/source. Exclude private
   Codex tree-snapshot refs from the disposable publication mirror after private
   backup; filter-repo commit rewriting alone does not sanitize those tree refs.
   Do not delete app-owned refs in the active repository.
4. Review and freeze both manifests. Paths SHA-256:
   cbb0da32715463028ca064a9160e98fc701200901c08c477bff2d9b4108f1617.
   Blob-list SHA-256:
   89009f2186a8b33b35b408c78cea39ed11a073af6125d589575e0548465fc20e.
   These are the LF-normalized publication bytes; `.gitattributes` enforces LF
   for both lists. The earlier CRLF files are preserved in the private backup.
   Use ONE filter-repo invocation on the disposable mirror, selecting both exact
   paths and IDs so historical aliases cannot survive:

   ```
   rtk proxy git filter-repo --invert-paths --paths-from-file <reviewed-absolute-path-list> --strip-blobs-with-ids <reviewed-absolute-blob-list>
   ```

   Do not replace PAL/CEA comment text or commit messages. Do not run this command
   in the active checkout. Record filter-repo commit/ref maps for collaboration.
5. Repeat the full historical path/signature/corpus census on the rewritten
   mirror. Assert every prohibited path and blob ID is absent from reachable
   published objects. Verify expected surviving metadata/source, preserve HIST
   notes, run fsck, build, artifact guard, relevant tests and review the tree diff.
   Compare clean branch trees against originals; legitimate content must survive.
   Use a fresh independent clone for final checks. Retain rollback backups until
   maintainers confirm publication and fork coordination.
6. Publish only the approved branches/tags with recorded-tip leases. Do not
   blindly push --mirror: that can publish stashes/worktree/audit refs. GitHub PR
   refs cannot be force-updated by normal Git pushes; fork owners must separately
   remove/rewrite their affected histories. Ask the host about cached commit/PR
   views under its applicable removal process. GitHub's sensitive-data Support
   process has eligibility limits; provenance cleanup is not guaranteed eligible.
7. Verify fresh remote clones and advertised refs, coordinate re-clones rather
   than merges of old history, retire unsafe caches/artifacts and inspect fork
   results. If the host cannot retire old PR/fork-network objects, consider a new
   independently initialized repository outside the old fork network and retire
   the old publication with owner/host coordination. This does not erase copies
   controlled by others. Mark cleanup complete only for verified publication
   surfaces, explicitly recording residual old refs/copies.

Host behavior reference: [GitHub removal guidance](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/removing-sensitive-data-from-a-repository).
Filter semantics: [git-filter-repo manual](https://github.com/newren/git-filter-repo/blob/main/Documentation/git-filter-repo.txt).

## Estimated remaining scope

Follow-up: [50-function retail pilot](retail-migration-pilot.md),
[publication coordination](history-cleanup-coordination.md), and
[recovery-backup checkpoint](cleanup-checkpoint-2026-10-02.md). The pilot is complete;
the history rewrite and canonical target migration remain unexecuted.

| Category | Remaining effort / uncertainty |
|---|---|
| History rewrite and verification | Approximately 1-2 maintainer days after freeze/backups; hosting and fork coordination are outside that estimate |
| Public workflow retirement | Main requested paths addressed; remaining CEA tool quarantine/rewrite approximately 0.5-1 day |
| Type/name remediation | 137 explicit claim lines plus implicit names; several focused days for simple fields, weeks for actor/rasterizer clusters requiring runtime evidence |
| Substantive implementation verification | Initial priority cluster is several dozen routines; days to weeks, longer for combat/fog state machines; one routine newly checked here |
| Safeguards | Implemented and tested; require branch-protection job and hook installation/verification on contributor clones |
| Retail pilot / full migration | 50-function feasibility pilot completed; full migration likely weeks or months depending on unmatched functions, ABI/globals verification and activation requirements |

Leave alone: PAL/CEA mentions in comments/messages; historical provenance notes;
CSS source maps; delinked/manifest.json and PR #25 README/index metadata; independently
built compiler probes/linker maps (ordinary output hygiene is separate); licensed
open-source tooling; independently constructed fixtures and minimal evidence
summaries. Do not purge those merely to make the repository look cleaner.

Hook installation for contributor clones: `rtk git config core.hooksPath tools/hooks`.
Keep tools/hooks/pre-commit executable when committing it. Require the
Reject proprietary artifacts job in branch protection after the reviewed workflow has passed on the publication branch.
