# Repository Format 6

**Working Document!!!** This spec guides development and will be removed before the final commit. Anything durable (user-facing behavior, the format feature table, migration guidance) moves to the user documentation before this file is deleted. This branch is the repository format infrastructure only: the ability to write, read, and adopt more than one format. Format 6 carries no features yet. The features that need it, key rotation among them, are specified and developed separately and land on top of this.

## Goals

- Make the repository format a property of each info file and manifest rather than a property of the binary, so a version can read a format it does not write.
- Introduce repository format 6 as a single opt-in boundary that future format changes bundle into, rather than adding an option per feature.
- Provide a migration path for existing stanzas via `stanza-upgrade`, so large repositories can adopt format 6 without being recreated.

## Prior State (Format 5)

- `REPOSITORY_FORMAT` was a compile-time constant stored as `backrest-format` in every info file and manifest.
- Any mismatch at load was a hard `FormatError`, raised before any data is read, so the only format a version could read was the one it wrote.
- Format 5 has been in place since `1.00` (2016-04-14, commit `9457e1534`). That release was a flag day that required creating a new repository, which is the cost this design exists to avoid repeating.

## Format Constants

- `REPOSITORY_FORMAT_5` and `REPOSITORY_FORMAT_6` name each format so that code which varies by format is explicit about the format it applies to (`src/version.h:35`).
- `REPOSITORY_FORMAT_MIN`/`MAX` are the range that can be read (`src/version.h:38`). Both are expressed in terms of the named formats, so `MIN` moves on its own when the oldest readable format changes.
- What new repositories get is the default of `repo-format` rather than a constant, since nothing in `src` needs to name it. Tests do, so `REPOSITORY_FORMAT_DEFAULT` is defined in the info harness (`test/src/harness/info.h:13`) and must match that default.
- The range is asserted wherever a format is set from code, in `infoNew()` and `infoFormatSet()`, so a format outside it can only ever arrive from a file, which is where it is reported rather than asserted.

## Format Option

- Repo-indexed option `repo-format`, so `--repo1-format`, consistent with the other repo options.
- **Command line only.** A format left in the configuration would migrate a stanza as a side effect of a `stanza-upgrade` run for an unrelated reason, such as a PostgreSQL version upgrade. A configuration file containing it warns and is ignored.
- The environment is not covered. The environment parser accepts any option that is valid for the command (`src/config/parse.c:2056`) and records it as `cfgSourceConfig`, which `stanza-upgrade` reads as a format having been asked for, so `PGBACKREST_REPO1_FORMAT=6` left in a cron or service environment migrates on the next run. What the environment may set is a rule for every option rather than for this one, so the behavior stands and the option reference carries a caution to set the option for the run that migrates rather than leaving it in the environment.
- Valid only for `stanza-create` and `stanza-upgrade` (see Migration); every other command derives the format from repository state. Not required after creation since the repository is self-describing.
- Allowed values: 5, 6. Default remains 5. The allow list is duplicated between `build/config.yaml` and `version.h` and must be kept in sync; a comment in each says so.

## Format Handling Rules

- **Readers trust the per-file stored format.** Every manifest and info file already records `backrest-format`; the value is stored on load and written back out on save rather than compared against a constant (`src/info/info.c:212`).
- **The info file format is the write target.** `backup.info` / `archive.info` format determines the format of new WAL and of new backup sets. Individual backups within a stanza may be older formats until they expire.
- **Backups adopt a new format only at a full backup.** A prior backup is a candidate for diff/incr only if it is at the format new backups are written with (`src/command/backup/incr.c.inc:42`), alongside the existing checks that a diff's prior must be full and that the prior must come from the same cluster. After an upgrade no prior matches, so the backup is changed to full at the new format by the path that already handles having no prior at all. The full backup is the single adoption boundary for the backup domain.
- **A backup set is never mixed-format, and a backup never references a file at another format.** Refusing the diff/incr rather than continuing the set at its original format is the stricter of the two options, and deliberately so. Backup is complex enough that a format applying to some files in a manifest and not others would have to be reasoned about at every point that touches a file, in backup and again in restore, verify, and expire. Refusing keeps format a property of a whole backup set that downstream code can read once. The cost is one extra full backup per stanza at migration, paid once.
- **A resumable backup at another format is not resumable** (`src/command/backup/resume.c.inc:243`). Its files were written at the format the aborted backup ran at, so reusing them would mix formats within the new backup by the same route the diff/incr rule closes. This joins the version, type, prior label, and compression checks already there, and like them it discards the resumable backup with a warning rather than failing.
- A backup's entry in `backup:current` records the format its manifest was written with rather than the running binary's format (`src/info/infoBackup.c:476`).
- Consequence: once the info files are format 6, binaries that only support format 5 cannot read the stanza at all, including its remaining format 5 backups. This is accepted; the info files gate everything.

## Migration

- `stanza-upgrade --repo1-format=6` flips the write target on an existing stanza. It rewrites the two info files and nothing else, so a repository of any size migrates in the time it takes to write them.
- The format is updated only when `repo-format` is explicitly given (`src/command/stanza/upgrade.c:65`). Otherwise the option default would downgrade a stanza that has already been migrated.
- **Downgrade is refused** (`src/command/stanza/upgrade.c:77`). The info files are what stop a version that does not support a format from reading a stanza at all; lowering them would let that version past the gate and into backups and archives it cannot read.
- The downgrade check reads the higher of the two formats. `archive.info` is saved first, so it is normally the file that leads, but a repository put back together by hand can have the other one ahead and the file that is ahead must not be downgraded to match the file behind it.
- The two info files are saved separately, so an upgrade interrupted between them leaves them at different formats. The upgrade therefore runs when either file differs from the requested format, not just when `archive.info` does, so re-running it brings the lagging file forward. That is what the version and system id they also carry have always done.
- An upgrade that changes the format says so (`src/command/stanza/upgrade.c:123`). The change cannot be undone and a version that does not support the new format can no longer read the stanza at all, which is too much to do silently. The format reported as the starting point is the lower of the two files, since that is the one that moves.
- Info files at different formats are an error wherever they are checked together, which is `stanza-upgrade`, `check`, `stanza-create` on an existing stanza, `verify`, and `backup` when it loads `archive.info`, i.e. an online backup with `archive-check` enabled (`src/command/check/common.c:127`). The hint names the repository the mismatch is in and the format to pass to `stanza-upgrade` to finish the job, since a hint without the repository would send a multi-repository configuration to migrate the wrong one. `stanza-upgrade` sets the format on both files before the check, so an upgrade that is given the format still repairs the mismatch rather than reporting it.
- Old backup sets remain format 5 until expired; old archive-ids remain format 5 until expired.
- The first backup after an upgrade is full even when diff or incr was requested, since no prior backup is at the new format. It warns and converts by the same path a stanza with no backups at all takes, so the extra cost of migrating is one full backup per stanza.

## Errors

- A format above the supported range: `repository format N requires a newer version of pgBackRest`, with a hint naming the range this version supports (`src/info/info.c:193`). It does not name the version that would be needed, since a version cannot know which version added a format that did not exist when it was written.
- A format below the supported range: `repository format N is no longer supported by pgBackRest`, with the same hint (`src/info/info.c:203`).
- A file with no format at all: `repository format not found`, with a hint asking whether it is an info file (`src/info/info.c:283`). The key was never required before, which was harmless while the format was a constant supplied on save, and is not now that a file is written back at the format it was read with.
- A released version that predates format 6 reports `expected format 5 but found 6`, which fails cleanly at info load before any data is touched. That path needs no change and is what gates old binaries out of a migrated stanza.

## Testing

- `harnessInfoChecksumFormat()` builds an info file at a given format; `harnessInfoChecksum()` is left as the wrapper for the default format that existing tests already call, so only tests that care about format mention one.
- The unit tests cover a stanza created at a non-default format, a `stanza-upgrade` with and without `repo-format`, a refused downgrade, both out-of-range load errors, a diff or incr converted to full because the prior is at another format, and a resumable backup discarded for the same reason.
- They also cover the rules that are easy to lose: a format in a configuration file is warned about and does not migrate anything, `stanza-create` leaves the format of a stanza that already exists alone, a manifest is loaded and saved at the format it was written with, and the `info` command reports the format each backup carries rather than one format for all of them.
- Every format between `REPOSITORY_FORMAT_MIN` and `MAX` is loaded as an option value and the format above `MAX` is rejected, so the allow list in `build/config.yaml` and the range in `version.h` cannot drift apart without a test failing.

## Default Format Bump

- Not in this branch. The `stanza-create` default moves from 5 to 6 in a designated future release, announced when format 6 first ships. Roughly a year out -- about four releases that support format 6 before the default flips -- so a buggy release always leaves supported fallbacks. Release-tied, never wall-clock-tied: the same binary and config must always produce the same repository.
- The default flip is the point where mixed-version deployments can break on new stanzas (new repo host writes format 6, old db host cannot read it). It gets a prominent release note, and `--repo-format=5` remains the explicit escape hatch for as long as format 5 is writable.

## Documentation Plan

- The `repo-format` option reference is written for both commands and carries an allow list naming each format -- the one place users look to answer "what do I get at format 6". The format 6 entry says it adds no features yet and is a stub until it carries something.
- Migration guidance (stanza-upgrade path, mixed-format stanzas, old-binary behavior) goes in the user guide.
- Release notes announce the introduction and, later, the default flip.
