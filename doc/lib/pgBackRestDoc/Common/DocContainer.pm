####################################################################################################################################
# DOC CONTAINER MODULE
#
# Build documentation container images, using the container registry (GHCR) as a cache so the expensive package installs are not
# rebuilt on every run.
####################################################################################################################################
package pgBackRestDoc::Common::DocContainer;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Digest::SHA qw(sha1_hex);
use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRestTest::Common::ExecuteTest;

use pgBackRestDoc::Common::Log;
use pgBackRestDoc::ProjectInfo;

####################################################################################################################################
# Build a documentation image, pulling it from the registry cache when possible and pushing a fresh build back to the cache
####################################################################################################################################
sub dockerBuildCached
{
    my $oStorage = shift;           # storage used to write the Dockerfile
    my $strDockerfile = shift;      # path to write the Dockerfile
    my $strImage = shift;           # local image tag, e.g. pgbackrest/doc:u22
    my $strScript = shift;          # full Dockerfile contents
    my $strBasePath = shift;        # docker build context
    my $strRevision = shift;        # manual cache revision (date-style, e.g. 20260721A)

    # Cache the image in the registry, keyed by a hash of the generated Dockerfile combined with the manual revision. Pull from the
    # pgbackrest cache first, then the repository owner's own cache when running on a fork, and build locally if neither has it. A
    # build is pushed to the cache this environment can write to (the fork's own cache on a fork, else the pgbackrest cache), best
    # effort since push requires write access. The image is always tagged locally as ${strImage} so downstream docker calls
    # reference a single name regardless of where it came from. The revision, set date-style (e.g. 20260721A) like the test
    # container revisions in test/container.yaml, forces a rebuild when the Dockerfile is unchanged but the upstream packages have
    # changed, e.g. a new PostgreSQL minor; it is also placed in the tag for reference.
    my $strCacheName =
        (split(':', $strImage))[-1] . "-${strRevision}-" . substr(sha1_hex($strScript . $strRevision), 0, 12);
    my $strCacheTag = 'ghcr.io/' . PROJECT_EXE . "/doc:${strCacheName}";

    # On a fork also use the fork owner's own cache, which it can write to
    my $strOwner = $ENV{GITHUB_REPOSITORY_OWNER} ? lc($ENV{GITHUB_REPOSITORY_OWNER}) : PROJECT_EXE;
    my $strForkCacheTag = $strOwner ne PROJECT_EXE ? "ghcr.io/${strOwner}/doc:${strCacheName}" : undef;

    # Pull from the pgbackrest cache first, then the fork's own cache. A failed pull (image missing or no read access) falls through
    # to the next cache or a local build.
    my $bCached = false;

    foreach my $strPullTag ($strCacheTag, defined($strForkCacheTag) ? $strForkCacheTag : ())
    {
        &log(INFO, "Checking cache ${strPullTag} ...");

        my $oPull = new pgBackRestTest::Common::ExecuteTest(
            "docker pull ${strPullTag}", {bSuppressStdErr => true, bSuppressError => true});
        $oPull->begin();

        if ($oPull->end() == 0)
        {
            &log(INFO, "Using cached ${strPullTag} image");
            executeTest("docker tag ${strPullTag} ${strImage}");
            $bCached = true;
            last;
        }
    }

    if (!$bCached)
    {
        &log(INFO, "Building ${strImage} image (${strCacheTag}) ...");

        $oStorage->put($strDockerfile, $strScript);
        executeTest("docker build -f ${strDockerfile} -t ${strImage} ${strBasePath}", {bSuppressStdErr => true});

        # Push the image to the cache this environment can write to (the fork's own cache on a fork, else the pgbackrest cache) and
        # log when it succeeds. Best effort since it requires write access, which is not available locally or on pull requests from
        # forks; those just use the image they built.
        my $strPushTag = defined($strForkCacheTag) ? $strForkCacheTag : $strCacheTag;
        executeTest("docker tag ${strImage} ${strPushTag}", {bSuppressError => true});

        my $oPush = new pgBackRestTest::Common::ExecuteTest(
            "docker push ${strPushTag}", {bSuppressStdErr => true, bSuppressError => true});
        $oPush->begin();

        if ($oPush->end() == 0)
        {
            &log(INFO, "Cached ${strPushTag} image");
        }
    }
}

push @EXPORT, qw(dockerBuildCached);

1;
