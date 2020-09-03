# Thoughts on Repository Format 6

# Info Format

- Info format will be `JSON`.  Something like (but without whitespace):

```
[
    {"pgbackrest":{"format":6,"version":<pgbackrest version>,"repo-id":<base32 * 16>,id:<base32 * 16>}},
    {"content":...},
    {"checksum":<sha(1/256)hash>}
]
```

## Manifest

- Save **all** options instead of the piecemeal saves that are done now.  These options are for informational purposes only so it would be fine to save them in a KeyValue or JSON blob.

- Store default sections before file/link/path sections to avoid two passes on load.
- Format backup labels using gmtime().

## Options

- Disable symlinks by default on `posix` repos. This is a convenience feature for manual access to the repo and it makes more sense to make it optional.
