# pgBackRest Distribution

This is a pgBackRest build distribution. Unlike a checkout of the [git repository](https://github.com/pgbackrest/pgbackrest), it ships the generated source and the rendered documentation pre-built, so pgBackRest can be built and installed without the code generation or the documentation tooling.

See <https://pgbackrest.org> for complete documentation.

## Contents

- `src` -- pgBackRest source, including the pre-generated `*.auto.c.inc` files
- `doc/man` -- command reference
- `doc/html` -- HTML documentation
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

## Documentation

The rendered documentation is in `doc`: the command reference in `doc/man` and the HTML documentation in `doc/html`.

Two user guides are provided, Debian/Ubuntu and RHEL/Rocky/Alma. If only one user guide is required, replace user-guide-index.html with the desired user guide. For example, to keep only the Debian/Ubuntu user guide:
```
mv user-guide.html user-guide-index.html
rm user-guide-rhel.html
```
