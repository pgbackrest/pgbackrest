# Key Rotation

**Working Document!!!** This spec guides development and will be removed before the final commit. Anything durable (user-facing behavior, the format feature table, migration guidance) moves to the user documentation before this file is deleted. The repository format 6 infrastructure this builds on is merged; format 6 carries the SHA-256 key derivation and nothing else, and this branch is what fills it out.

## Goals

- Rotate the archive key, which is the only bulk-data key that never rotates today.
- Give each backup set one key covering both its manifests and its data files, rotating with every full backup.
- Rotate the user passphrase without touching bulk data, when the user asks for it. Later commit.

## Current State (Format 5)

- `repo-cipher-pass` (user-supplied) encrypts `backup.info` and `archive.info`.
- `backup.info` holds one sub-passphrase that encrypts every `backup.manifest` in the stanza.
- Each manifest holds a sub-passphrase that encrypts the data files of its own backup set. A new one is generated per full backup (`src/command/backup/backup.c:244`) and inherited by diff and incr (`src/command/backup/incr.c.inc:149`), so this level already rotates per set and expires with it.
- `archive.info` holds one sub-passphrase that encrypts all WAL across all archive-ids for the life of the stanza. This is the only bulk-data key that never rotates.
- Sub-passphrases are 48 random bytes, base64-encoded to 64 characters (`cipherSpecGen()`, `src/command/stanza/common.c:20`).

## What Format 6 Already Provides

- Each info file and manifest stores the format it was written with, and readers trust the stored value, so a repository holds more than one format at once while older backups and archives expire.
- A stanza is created at a format with `stanza-create --repoN-format` and migrated with `stanza-upgrade --repoN-format`. Once the info files are at format 6 a version that does not support it cannot read the stanza at all, which is the gate that makes the changes below safe.
- Encrypted info files begin with an eight byte plaintext header, `PGBR` plus three format digits plus one more byte, written in place of the OpenSSL `Salted__` magic. `CipherBlockFormat` reads the header and streams the rest through a block cipher built from what the header said.
- A key stores the digest it derives with, so a key outlives the format of the file it is stored in. Keys carried over by migration keep SHA-1; only new keys get SHA-256.
- The eighth byte of the header is reserved, and that byte is where the archive key id goes.

## Key Storage

- At format 6 the `cipher-pass` value in both info files holds every key the file is responsible for rather than the single key it holds today, as an object keyed by the key id.

```
cipher-pass={"1":{"key":"afg...","digest":"sha256"}, ...}
```

- The value is an object rather than the key on its own because the digest has to travel with each key. A flat `{"1":"afg..."}` would mean inferring the digest from the id, which is the inference the stored digest exists to avoid.
- In `archive.info` the id is the sequential key id that the file header names. In `backup.info` it is the full backup label of the set the key belongs to.
- A key inherited from a format 5 migration goes in id `0`: the stanza-wide manifest key in `backup.info`, the stanza-wide archive key in `archive.info`. Format 6 never writes id 0 itself and a backup label cannot collide with it.
- `stanza-create` at format 6 writes a single archive key at id 1. `stanza-upgrade` from format 5 keeps the key it already has at id 0 and adds a new key at id 1, which becomes current, so WAL written before the migration stays readable under the key that wrote it.
- There is no scalar `cipher-pass` at format 6. A stanza created at format 6 has no id 0 entry at all.
- The digest belongs to the entry rather than to the file. An id 0 key derives with SHA-1 and every format 6 key derives with SHA-256, so the scalar `cipher-digest` written by the format 6 infrastructure commit moves into the entries. This also survives a future format that changes the digest again, which a scalar would not.
- Keys are held in memory as a new `CipherSpecMap` in `common/format`, keyed by the id, which is what `CipherBlockFormat` selects from and what the read path packs for the protocol. It lives beside the format rather than in `common/crypto` so the block cipher stays free of repository concepts.
- `Info` holds a `CipherSpecMap` at every format. A format 5 file loads its single scalar key as the one entry, so only the loader and the saver branch on format and nothing above them does.
- Neither info file carries a key id of its own, since both are encrypted with `repo-cipher-pass` and the map holds only the sub-keys for dependent files. Their header stays eight bytes with the `_` marker, so replacing those bytes with `Salted__` still hands the file to `openssl enc` for a user who wants to read it by hand. A format 6 info file needs `-md sha256` where format 5 needed `-md sha1`. WAL is not worth the same treatment: between the id in front and the key map there is no realistic manual path, and there never was much of one.

## Backup Set Key

- Format 6 stores one key per backup set in `backup.info`, keyed by the full backup label, and it encrypts both the set's manifests and its data files. Every full backup generates a fresh entry, so rotation is automatic and needs no option, marker, or age tracking.
- This merges two format 5 levels. The split had no security value, since the data key lives inside a manifest encrypted with the manifest key, so holding the manifest key already unlocked the data transitively. Once the manifest key is per set they are the same key.
- Format 6 manifests drop the `[cipher]` sub-passphrase. Format 5 manifests keep theirs and are read according to their stored format.
- Lookup is exact. A diff or incr label carries its full's label as the prefix before `_`, so the key for any manifest or data file, current or under `backup.history`, comes from parsing the label. No boundary comparison exists anywhere. The prior-manifest inheritance at `src/command/backup/incr.c.inc:149` is no longer needed for format 6 sets, though the format inheritance beside it stays.
- Keying by label rather than by a sequential id is deliberate. `repo-retention-history` keeps manifests under `backup.history` after their entry leaves `[backup:current]`, so an id recorded against the backup entry would disappear while the manifest it unlocks survives. The label is in the path either way.
- The entry is recorded when the label is assigned, before the first manifest copy is saved, so a resumed backup finds its key by the normal lookup with no special case. An entry left by an aborted full is inert and is pruned once no manifest references it.
- Every data key consumer already holds the manifest and reaches the key through `manifestCipherSpec()`. At format 6 that getter is filled in from the `backup.info` entry once the manifest loads, so verify, restore, expire, `repo get`, resume, incr, and `backup/db` are unchanged.
- A migrated stanza keeps its format 5 stanza-wide manifest key as id 0 until the last format 5 manifest expires. Format 5 sets keep reading their data keys from their manifests as always.

## Archive Key

- `archive.info` holds a map of keys. One is current and encrypts new WAL; the rest are retained only so WAL already written can still be read.
- Key ids are sequential from 1 and are never reused. Which one is current is recorded in `cipher-pass-current` beside the map rather than inferred, since the map is keyed by an opaque id and sorts as text, so the highest id is not the last entry once there are ten of them.
- Only the most recent rotation is dated, in a `cipher-pass-rotate` key beside the map, as seconds since the epoch. Rotation compares the current key's age and nothing looks at when a retired key was made, so dating every entry would store what nothing reads.
- Rotation appends a key and makes it current. No existing file is rewritten and nothing else in the repository moves.

### Key id in the file

- The eighth byte of the format header says whether a key id follows. `_` means nothing follows, which is what the info files write since they are encrypted with the user passphrase and there is only ever one of those. `K` means a key id follows immediately: one byte of length, then that many bytes of id, then the salt.
- The id is stored as bytes rather than as a number, so it is whatever the map is keyed by and needs no conversion at either end. Archive ids happen to be decimal text today, but an id that is not a number, e.g. a backup label, costs nothing to support later.
- A single length byte allows 255 bytes of id, which is ample: a backup label is under 40 and a decimal id would have to pass 10^255 to overflow.
- The reader therefore holds back a variable number of bytes rather than a fixed eight. `cipherBlockFormatProcess()` reads the fixed header, then the length byte when the marker is `K`, then that many bytes, growing its held-back size as each part arrives.
- WAL already carries the `Salted__` magic, since `src/command/archive/push/file.c:273` encrypts with the default header, so the fixed part of the header replaces it for free and only the length byte and the id are new. The files that omit the magic to save space are bundled and block-incremental backup files, and those are keyed by label, so they keep no header at all.
- The key id in the file is what makes rotation need no coordination with running archivers. A pusher still holding a previous `archive.info` keeps writing under the previous key, and the file records which key that was, so a reader finds it without guessing. There is no boundary to compare against and no trial decryption anywhere in the design.

### Reading

- archive-get caches one cipher spec per repo (`src/command/archive/get/get.c:419`) and packs it into the per-file protocol payload (`src/command/archive/get/get.c:927`). The id is not known until the file is open, so the key map goes down to the local process instead and `CipherBlockFormat` selects from it. verify (`src/command/verify/verify.c:859`) and restore's timeline read (`src/command/restore/timeline.c:97`) change the same way.
- The payload is per file, so the whole map repeats for every segment requested. That is accepted: there is no easy way to move the payload to job level today, and a healthy repository has only a few active keys. Trimming the map by some heuristic before packing it is a later option if it ever matters.
- Pruning at expire is what keeps the map small. An unpruned entry is only wasted bytes, so pruning is never required for correctness.
- Archive-id handling is unaffected. archive-push writes only to the current archive-id and archive-get already searches every archive-id matching the current version and system-id (`src/command/archive/get/get.c:430`), and neither cares which key a file used.

### Rotation schedule

- A new option, `repo-cipher-rotate`, gives the rotation period as a time and defaults to 90 days, with a minimum of 15 days. Setting it to the max value effectively disables rotation. It is repo indexed and applies only to the archive key: the backup set key rotates with every full backup, and the user passphrase is user-supplied so it cannot rotate on its own.
- Expire performs the rotation, since it writes `archive.info` on every run and runs regularly, typically daily through backup's expire phase. When the current key is older than the period, expire appends a new key and makes it current. Rotation latency is bounded by expire cadence, which is negligible against a period measured in weeks. If expire never runs then neither does rotation.
- A dry run logs the rotation but does not save it. At format 5 the option is ignored, with a warning when it is not the default, since there is only the one key.
- `stanza-create` at format 6 and a migration to format 6 both add their key through rotation, which assigns the next id and sets `cipher-pass-rotate`, so the clock starts there. The next id is the current id plus one, or 1 when there is no current key.

## Key Pruning at Expire

- Archive: a key is dropped once no WAL that references it remains. Expire already removes WAL, so it knows when the last file for a key is gone. The current key is never dropped.
- Backup set: an entry serves exactly one set, so it is dropped when the last manifest of that set is removed, current or under `backup.history`. Expire performs both removals. The entry for the newest set is never dropped.
- Both happen in expire's existing info file rewrites under the backup lock, so no additional locking is required.

## Info File Write Coordination

- Every info file writer, backup and expire for `backup.info`, expire for `archive.info`, and the stanza commands for both, requires the backup lock and sets `lock-remote-required` (`build/config.yaml`), so the repository host's lock directory is the single serialization point regardless of which host each command runs on.
- archive-push and archive-get never write `archive.info`, so rotation needs no coordination with them beyond the atomic info save. A reader sees the old file or the new one and both are valid, per the running-archiver rule above.
- Backup used to resave `archive.info` at completion only to freshen its timestamp so object-store lifecycle settings do not remove it early. That resave moved to expire, which saves `archive.info` on every run that is not a dry run, so backup no longer writes `archive.info`. With `expire-auto` disabled the timestamp is freshened only when expire is run separately.
- With directly-reached storage (object stores, shared NFS) and writers invoked from several hosts there is no common lock point. This predates this work and applies to backup and expire today. The documented practice remains to run backup and expire against a given repo from one place.

## Later Commits

- User passphrase rotation. `repo-cipher-pass` wraps only the info files, so rotating it rewrites four small objects (`backup.info`, `archive.info`, and their copies) and never touches bulk data. The plan is a config-first flow: set `repo-cipher-pass` to the new passphrase and `repo-cipher-pass-old` to the old one, then run a command that converges each info file. Every command retries an info file load with the old passphrase and warns when the fallback is used, so config can be updated across hosts non-atomically.
- A `stanza-rekey` command to drive that convergence and to force an archive key rotation on demand. It is repo-only by design, with no cluster access, so keys can be rotated during incident response with the cluster down or unreachable. That is why it is not a flag on `stanza-upgrade`, which requires cluster access for `pgValidate()`.

## Documentation Plan

- The features per format version table in the `repo-format` option reference is a stub today. This work fills in the format 6 entries.
- The `repo-cipher-rotate` option reference, and the `stanza-rekey` command reference when that lands.
- Release notes announce archive key rotation and the per backup set key.
