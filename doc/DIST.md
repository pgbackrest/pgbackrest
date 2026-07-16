# pgBackRest Distribution

This is a pgBackRest build distribution. Unlike a checkout of the [git repository](https://github.com/pgbackrest/pgbackrest), it ships the generated source and the rendered documentation pre-built, so pgBackRest can be built and installed without the code generation or the documentation tooling.

See <https://pgbackrest.org> for complete documentation.

## Contents

- `src` -- pgBackRest source, including the pre-generated `*.auto.c.inc` files
- `doc/man` -- command reference
- `doc/html` -- HTML documentation
- `test` -- smoke test to verify the build
- `meson.build`, `meson_options.txt` -- build configuration
- `LICENSE`

## Build

pgBackRest builds with meson and ninja. The required libraries are libpq, OpenSSL (>= 1.1.1), libxml2, lz4, bz2, and zlib; zstd, libssh2 (for SFTP), and libsystemd are optional. For example, on Debian/Ubuntu:

```
apt-get install meson gcc pkg-config libpq-dev libssl-dev libxml2-dev \
    liblz4-dev libzstd-dev libbz2-dev libz-dev libssh2-1-dev libsystemd-dev
```

Then configure, build, and install:
```
meson setup build .
ninja -C build
meson install -C build
```

The `pgbackrest` binary is built at `build/src/pgbackrest`.

## Test

A smoke test is included to verify that the build works. It finds each PostgreSQL installation on the system and runs the newly-built `pgbackrest` through a backup and restore cycle against every supported version found: create a cluster, configure archiving, create the stanza, check, back up, restore, and verify that the restored data matches. Versions that pgBackRest does not support are reported and skipped. The test requires nothing beyond the build environment and PostgreSQL -- no additional Python packages.

Run it with meson after building:
```
meson test -C build --suite smoke
```

Add `-v` to stream the output of each step as it runs:
```
meson test -C build --suite smoke -v
```

PostgreSQL is searched for in the standard packaging paths:

- `/usr/lib/postgresql/<version>/bin` (Debian/Ubuntu)
- `/usr/pgsql-<version>/bin` (RHEL)
- `/usr/libexec/postgresql<version>` (Alpine)
- `/usr/bin`

Each version is tested in a temporary directory with its own repository and a private unix socket, so a running PostgreSQL cluster is not disturbed. The test fails when there is no PostgreSQL to test against, since a test that silently does nothing does not verify the build. Since PostgreSQL will not run as root, running the test as root drops to the `postgres` user and fails if that user does not exist.

The test can also be run directly, which allows the defaults to be overridden:
```
python3 test/smoke.py --pgbackrest build/src/pgbackrest
```

Useful options are `--pg-bin-path` to search an additional path for PostgreSQL, `--version` to test only a specific version, and `--user` to select the user to drop to when running as root. Use `--help` for the complete list.

## Documentation

The rendered documentation is in `doc`: the command reference in `doc/man` and the HTML documentation in `doc/html`.

Two user guides are provided, Debian/Ubuntu and RHEL/Rocky/Alma. If only one user guide is required, replace user-guide-index.html with the desired user guide. For example, to keep only the Debian/Ubuntu user guide:
```
mv user-guide.html user-guide-index.html
rm user-guide-rhel.html
```
