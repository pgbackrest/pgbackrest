# Release Build Instructions

## Update CI container builds

If there have been PostgreSQL minor releases since the last pgBackRest release then update the CI containers to include the latest releases. This should be committed before the release.

## Create a branch to test the release

```
git checkout -b release-ci
```

## Update the date, version, and release title

Edit the latest release in `doc/xml/release.xml`, e.g.:
```
        <release date="XXXX-XX-XX" version="2.14dev" title="UNDER DEVELOPMENT">
```
to:
```
        <release date="2019-05-20" version="2.14.0" title="Bug Fix and Improvements">
```

Remove development marker `src/version.h`, e.g.:
```
#define PROJECT_VERSION_SUFFIX                                      "dev"
```
to:
```
#define PROJECT_VERSION_SUFFIX                                      ""
```

## Update sponsors

Add new sponsors and move to past sponsors when sponsorship has lapsed.

## Build release documentation
```
pgbackrest/doc/release.pl --build
```

## Commit release branch and push to CI for testing
```
git commit -m "Release test"
git push origin release-ci
```

## Run Coverity

- Prepare Coverity build directory (update version/paths as required):
```
mkdir coverity
tar -xvf ~/Downloads/cov-analysis-linux-arm64-2024.6.1.tar.gz --strip-components=1 -C ~/coverity
export COVERITY_TOKEN=?
export COVERITY_EMAIL=?
export COVERITY_VERSION=?
```

- Clean directories and run Coverity:
```
rm -rf .cache/ccache && rm -rf build && rm -rf pgbackrest.tgz && rm -rf cov-int
meson setup -Dwerror=true -Dfatal-errors=true -Dbuildtype=debug build pgbackrest
coverity/bin/cov-build --dir cov-int ninja -C build
tar czvf pgbackrest.tgz cov-int
```

- Upload results:
```
curl --form token=${COVERITY_TOKEN?} --form email="${COVERITY_EMAIL?}" --form file=@pgbackrest.tgz \
    --form version="${COVERITY_VERSION?}" --form description="pre-release build" \
    "https://scan.coverity.com/builds?project=pgbackrest%2Fpgbackrest"
```

Check issues at https://scan.coverity.com/projects/pgbackrest-pgbackrest then fix and repeat Coverity runs as needed.

## Perform stress testing on release

- Build the documentation with stress testing enabled:
```
pgbackrest/doc/doc.pl --out=html --include=user-guide --require=/stress --var=stress=y --var=stress-scale-table=100 --var=stress-scale-data=1000 --pre --no-cache
```

During data load the archive-push and archive-get processes can be monitored with:
```
docker exec -it doc-pg-primary tail -f /var/log/pgbackrest/demo-archive-push-async.log
docker exec -it doc-pg-standby tail -f /var/log/pgbackrest/demo-archive-get-async.log
```

During backup/restore the processes can be monitored with:
```
docker exec -it doc-repository tail -f /var/log/pgbackrest/demo-backup.log
docker exec -it doc-pg-standby tail -f /var/log/pgbackrest/demo-restore.log
```

Processes can generally be monitored using 'top'. Once `top` is running, press `o` then enter `COMMAND=pgbackrest`. This will filter output to pgbackrest processes.

- Check for many log entries in the `archive-push`/`archive-get` logs to ensure async archiving was enabled:
```
docker exec -it doc-pg-primary vi /var/log/pgbackrest/demo-archive-push-async.log
docker exec -it doc-pg-standby vi /var/log/pgbackrest/demo-archive-get-async.log
```

- Check the backup log to ensure the correct tables/data were created and backed up. It should look something like:
```
INFO: full backup size = 14.9GB, file total = 101004
```

- Check the restore log to ensure the correct tables/data were restored. The size and file total should match exactly.

## Clone web documentation into `doc/site`
```
cd pgbackrest/doc
git clone git@github.com:pgbackrest/website.git site
```

## Deploy web documentation to `doc/site`
```
pgbackrest/doc/release.pl --deploy
```

## Final commit of release to integration

Create release notes based on the pattern in prior git commits (this should be automated at some point), e.g.
```
v2.14.0: Bug Fix and Improvements

Bug Fixes:

* Fix segfault when process-max > 8 for archive-push/archive-get. (Reported by User.)

Improvements:

* Bypass database checks when stanza-delete issued with force. (Contributed by User. Suggested by User.)
* Add configure script for improved multi-platform support.

Documentation Features:

* Add user guide for Debian.
```

Commit to integration with the above message and push to CI.

Wait for CI testing to complete before proceeding to the next step.

## Create release on Github and dist tarball

Set the release variables. `GITHUB_TOKEN` needs the `contents: write` permission to create the release and `actions: read` to download the tarball artifact (a classic token with the `repo` scope covers both):
```
export GITHUB_TOKEN=?
export GITHUB_REPO=pgbackrest/pgbackrest
export GIT_BRANCH=integration
```

Release the distribution tarball that CI built and tested for the release commit rather than building one locally. Every push to the release branch builds the tarball, checks it for missing files, and smoke tests it on a range of distributions, so the most recent run's artifact is the exact tested bytes to release. Run the commands below from an empty directory.

Shallow clone the release branch, then derive the version, release commit, and commit subject from the checkout. `RELEASE_COMMIT` is the tip commit, but only if its subject starts with `v${VERSION}:` -- if it comes back blank a later commit was added and the release steps are out of order:
```
rm -rf dist
git clone --depth 1 --branch ${GIT_BRANCH?} https://github.com/${GITHUB_REPO?}.git dist/repo
export VERSION=$(sed -n "s/^    version: '\([^']*\)',.*/\1/p" dist/repo/meson.build)
export RELEASE_COMMIT=$(git -C dist/repo log --no-walk HEAD --grep="^v${VERSION?}: " --format=%H)
export RELEASE_TITLE=$(git -C dist/repo log -1 --format=%s)
echo ${RELEASE_COMMIT?}
echo ${RELEASE_TITLE?}
```

Find the most recent CI run for the release branch and confirm it built the release commit and passed. A mismatch means CI has not finished for the release commit or a later commit has landed; a non-success conclusion means the tarball was not fully tested:
```
RUN=$(curl -sS -H "Authorization: Bearer ${GITHUB_TOKEN?}" -H "Accept: application/vnd.github+json" \
    "https://api.github.com/repos/${GITHUB_REPO?}/actions/workflows/test.yml/runs?branch=${GIT_BRANCH?}&per_page=1")
export RUN_ID=$(echo "${RUN?}" | jq -r '.workflow_runs[0].id')
RUN_SHA=$(echo "${RUN?}" | jq -r '.workflow_runs[0].head_sha')
RUN_STATUS=$(echo "${RUN?}" | jq -r '.workflow_runs[0].conclusion')
echo "run ${RUN_ID?} ${RUN_SHA?} ${RUN_STATUS?}"
[ "${RUN_SHA?}" = "${RELEASE_COMMIT?}" ] && [ "${RUN_STATUS?}" = "success" ] || { echo "not a passing build of the release commit"; false; }
```

That run uploads the tarball as the `dist-tarball` artifact. Get its download URL, then download and extract it. The download needs the token, and GitHub wraps artifacts in a zip so it must be unzipped to get the tarball:
```
export ARTIFACT_URL=$(curl -sS -H "Authorization: Bearer ${GITHUB_TOKEN?}" -H "Accept: application/vnd.github+json" \
    "https://api.github.com/repos/${GITHUB_REPO?}/actions/runs/${RUN_ID?}/artifacts?name=dist-tarball" \
    | jq -r '.artifacts[0].archive_download_url')
mkdir -p dist/build/meson-dist
curl -sSL -H "Authorization: Bearer ${GITHUB_TOKEN?}" "${ARTIFACT_URL?}" -o dist/dist-tarball.zip
unzip -o dist/dist-tarball.zip -d dist/build/meson-dist
```

The artifact ships only the tarball, so regenerate its checksum. The result is identical to CI's since it is the same bytes, and it lands at the path the upload step below expects, `dist/build/meson-dist/pgbackrest-{version}.tar.gz` with its `pgbackrest-{version}.tar.gz.sha256sum`:
```
( cd dist/build/meson-dist && sha256sum pgbackrest-${VERSION?}.tar.gz > pgbackrest-${VERSION?}.tar.gz.sha256sum )
```

Immutable releases are enabled, so release assets are locked once the release is published. Create the release as a draft with the tarball attached, then set the notes and publish it in the next section.

Generate the release body from the release commit message. It is the same content as the commit, so this bolds the section headers and any leading notes, and converts the bullets to markdown:
```
printf '%s' "$(git -C dist/repo log -1 --format=%b ${RELEASE_COMMIT?} |
    sed -E 's/^([A-Za-z][A-Za-z ]*):$/**\1**:/; s/^([A-Z][A-Z ]*): /**\1**: /; s/^\* /- /')" > dist/release-body
```

Create the draft release and capture its id (requires `jq`). The title is the release commit subject and the body is read from the generated file. The `release/${VERSION}` tag is not created until the release is published:
```
RELEASE_ID=$(curl -sS -X POST \
    -H "Authorization: Bearer ${GITHUB_TOKEN?}" -H "Accept: application/vnd.github+json" \
    https://api.github.com/repos/${GITHUB_REPO?}/releases \
    -d "$(jq -n --arg tag "release/${VERSION?}" --arg commit "${RELEASE_COMMIT?}" \
        --arg name "${RELEASE_TITLE?}" --rawfile body dist/release-body \
        '{tag_name: $tag, target_commitish: $commit, name: $name, body: $body, draft: true}')" \
    | jq -r .id)
echo $RELEASE_ID
```

Upload the tarball and its checksum as release assets:
```
ASSET_ID=$(curl -sS -X POST \
    -H "Authorization: Bearer ${GITHUB_TOKEN?}" -H "Content-Type: application/gzip" \
    --data-binary @dist/build/meson-dist/pgbackrest-${VERSION?}.tar.gz \
    "https://uploads.github.com/repos/${GITHUB_REPO?}/releases/${RELEASE_ID?}/assets?name=pgbackrest-${VERSION?}.tar.gz" \
    | jq -r .id)
curl -sS -X POST \
    -H "Authorization: Bearer ${GITHUB_TOKEN?}" -H "Content-Type: text/plain" \
    --data-binary @dist/build/meson-dist/pgbackrest-${VERSION?}.tar.gz.sha256sum \
    "https://uploads.github.com/repos/${GITHUB_REPO?}/releases/${RELEASE_ID?}/assets?name=pgbackrest-${VERSION?}.tar.gz.sha256sum"
```

The draft now has both assets attached. Set the notes and publish it in the next section; publishing creates the `release/${VERSION}` tag from `RELEASE_COMMIT` and locks the assets into the immutable release.

Before publishing, download the attached asset into `dist/verify`, extract it, and build it. This proves the exact released bytes are self-contained before publishing:
```
rm -rf dist/verify
mkdir -p dist/verify
curl -fsSL -H "Authorization: Bearer ${GITHUB_TOKEN?}" -H "Accept: application/octet-stream" \
    "https://api.github.com/repos/${GITHUB_REPO?}/releases/assets/${ASSET_ID?}" | tar zx -C dist/verify

ls -lahR \
    dist/verify/pgbackrest-${VERSION?}/src/command/help/help.auto.c.inc \
    dist/verify/pgbackrest-${VERSION?}/src/postgres/interface.auto.c.inc \
    dist/verify/pgbackrest-${VERSION?}/doc/html \
    dist/verify/pgbackrest-${VERSION?}/doc/man

rm -rf .cache/ccache
meson setup dist/verify/build dist/verify/pgbackrest-${VERSION?}
ninja -C dist/verify/build
dist/verify/build/src/pgbackrest version
```

## Push to main

Push release commit to main once CI testing is complete.

## Publish the release on github

The draft created above already has the correct title, body, tag, and assets. Show them to verify before publishing:
```
curl -sS -H "Authorization: Bearer ${GITHUB_TOKEN?}" \
    https://api.github.com/repos/${GITHUB_REPO?}/releases/${RELEASE_ID?} |
    jq -r '"TITLE: " + .name, "TAG: " + .tag_name, "COMMIT: " + .target_commitish, "BODY:", .body, "ASSETS:", (.assets[] | .name + " (" + (.size | tostring) + " bytes, " + .state + ", " + (if .digest then (.digest | ltrimstr("sha256:") | .[:8]) else "no digest" end) + ")")'
```

When the output is correct, publish the release. This creates the `release/${VERSION}` tag from the release commit and locks the assets into the immutable release:
```
curl -sS -X PATCH \
    -H "Authorization: Bearer ${GITHUB_TOKEN?}" -H "Accept: application/vnd.github+json" \
    https://api.github.com/repos/${GITHUB_REPO?}/releases/${RELEASE_ID?} \
    -d '{"draft": false}'
```

## Push web documentation to main and deploy
```
cd pgbackrest/doc/site
git commit -m "v2.14.0 documentation."
git push origin main
```

Deploy the documentation on `pgbackrest.org`.

## Notify packagers of new release

Notify the Debian packagers by email and RHEL packagers at https://github.com/pgdg-packaging/pgdg-rpms/issues.

## Announce release on Twitter

## Publish a postgresql.org news item when there are major new features

Start from NEWS.md and update with the new date, version, and interesting features added since the last release. News items are automatically sent to the `pgsql-announce` mailing list once they have been approved.

## Update PostgreSQL ecosystem wiki

Update version, date, and minimum supported version (when changed): https://wiki.postgresql.org/wiki/Ecosystem:Backup#pgBackRest

## Prepare for the next release

Add new release in `doc/xml/release.xml`, e.g.:
```
        <release date="XXXX-XX-XX" version="2.15dev" title="UNDER DEVELOPMENT">
```

Edit version in `src/version.h`, e.g.:
```
#define PROJECT_VERSION_MAJOR                                       2
#define PROJECT_VERSION_MINOR                                       58
#define PROJECT_VERSION_PATCH                                       0
#define PROJECT_VERSION_SUFFIX                                      ""
```
to:
```
#define PROJECT_VERSION_MAJOR                                       2
#define PROJECT_VERSION_MINOR                                       59
#define PROJECT_VERSION_PATCH                                       0
#define PROJECT_VERSION_SUFFIX                                      "dev"
```

Run deploy to generate git history (ctrl-c as soon as the file is generated):
```
pgbackrest/doc/release.pl --build
```

Commit and push to integration:
```
git commit -m "Begin v2.15.0 development."
git push origin integration
```
