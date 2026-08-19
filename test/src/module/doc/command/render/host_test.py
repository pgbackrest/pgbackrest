"""Test Documentation Hosts.

Nothing here starts a container. What is checked is the docker commands the host would run, since that is where the behaviour is --
the rest is docker's."""

####################################################################################################################################
import os
import tempfile

from harness.test import *

import command.render.host as host_module
from common.storage import file_read
from command.render.host import Host, HostGroup, image_build_cached

# Commands that were run, and the exit status to give a command rather than the success everything else gets
COMMAND_LIST = []
STATUS_MAP = {}


####################################################################################################################################
class _Exec:
    """Stands in for a command that is run and waited on."""

    def __init__(self, command, **kwargs):
        self.command = command

    def begin(self):
        COMMAND_LIST.append(self.command)

    def end(self):
        return STATUS_MAP.get(self.command, 0)


####################################################################################################################################
def _exec_result(command, **kwargs):
    """Stand in for a command that is run and checked."""

    COMMAND_LIST.append(command)

    return STATUS_MAP.get(command, 0), "172.17.0.2\n", ""


####################################################################################################################################
def _begin():
    """Start a test with nothing run yet and everything succeeding."""

    COMMAND_LIST.clear()
    STATUS_MAP.clear()

    # Start every test as if this were not a fork. The fork cache is keyed on the repository owner, so a test that does not set
    # the owner itself would otherwise see whatever the environment has. This only makes a difference when the tests run outside
    # a container, since docker exec does not pass the environment through.
    os.environ.pop("GITHUB_REPOSITORY_OWNER", None)

    host_module.exec_result = _exec_result
    host_module.Exec = _Exec


####################################################################################################################################
def test_host():
    """A host is a container, and what it is called on the network is what the documentation calls it."""

    _begin()

    host = Host("repo", "doc-repo", "pgbackrest/doc:u24", "vagrant")

    assert_equal(host.ip, "172.17.0.2")
    assert_true(host.active)

    # Anything left over from a previous build is removed first, since a build is not required to have finished cleanly
    assert_equal(COMMAND_LIST[0], "docker rm -f doc-repo")
    assert_equal(COMMAND_LIST[1], "docker run -itd -h repo --name=doc-repo pgbackrest/doc:u24 ")
    assert_in("docker inspect", COMMAND_LIST[2])

    # A host is only removed once, however many times it is asked for
    host.remove()
    host.remove()

    assert_false(host.active)
    assert_equal(COMMAND_LIST[-1], "docker rm -f doc-repo")
    assert_equal(len([command for command in COMMAND_LIST if command == "docker rm -f doc-repo"]), 2)


####################################################################################################################################
def test_host_option():
    """Everything a host is given is passed to docker in the order docker takes it."""

    _begin()

    Host("pg", "doc-pg", "image:1", "vagrant", ["/a:/a", "/b:/b"], "--memory=4g", "server -p 1", host_update=False)

    assert_equal(COMMAND_LIST[1], "docker run -itd -h pg --name=doc-pg --memory=4g -v /a:/a -v /b:/b image:1  server -p 1")


####################################################################################################################################
def test_host_execute():
    """A command runs through a login shell as the user the documentation says to run it as."""

    _begin()

    host = Host("repo", "doc-repo", "image:1", "vagrant")

    host.execute("ls -l")

    assert_equal(COMMAND_LIST[-1], "docker exec -u vagrant doc-repo bash -l -c 'ls -l'")

    # A command that must not see the environment of a login, and one that is not a shell command at all
    host.execute("echo x", user="root", load_env=False)

    assert_equal(COMMAND_LIST[-1], "docker exec -u root doc-repo bash -c 'echo x'")

    host.execute("psql -c 'select 1'", bash_wrap=False)

    assert_equal(COMMAND_LIST[-1], "docker exec -u vagrant doc-repo psql -c 'select 1'")

    # A quote in a command is escaped so the shell sees the command rather than the end of it
    host.execute("psql -c 'select 1'")

    assert_equal(COMMAND_LIST[-1], "docker exec -u vagrant doc-repo bash -l -c 'psql -c '\\''select 1'\\'''")


####################################################################################################################################
def test_host_copy():
    """A file copied to a host is given an owner and a mode, since what a reader would have is not what docker leaves behind."""

    _begin()

    host = Host("repo", "doc-repo", "image:1", "vagrant")

    host.copy_to("/local/pgbackrest.conf", "/etc/pgbackrest.conf", "postgres:postgres", "640")

    assert_equal(COMMAND_LIST[-3], "docker cp /local/pgbackrest.conf doc-repo:/etc/pgbackrest.conf")
    assert_equal(COMMAND_LIST[-2], "docker exec -u root doc-repo bash -l -c 'chown postgres:postgres /etc/pgbackrest.conf'")
    assert_equal(COMMAND_LIST[-1], "docker exec -u root doc-repo bash -l -c 'chmod 640 /etc/pgbackrest.conf'")

    # A file that is only being fetched has neither
    host.copy_from("/etc/postgresql.conf", "/local/postgresql.conf")

    assert_equal(COMMAND_LIST[-1], "docker cp doc-repo:/etc/postgresql.conf /local/postgresql.conf")

    host.copy_to("/local/x", "/etc/x")

    assert_equal(COMMAND_LIST[-1], "docker cp /local/x doc-repo:/etc/x")


####################################################################################################################################
def test_host_group():
    """Hosts of a build know each other by name, which is what lets the documentation use names rather than addresses."""

    _begin()

    group = HostGroup()

    repo = Host("repo", "doc-repo", "image:1", "vagrant")
    group.host_add(repo)

    # The first host has no other host to learn about
    assert_equal(COMMAND_LIST[-1], "docker exec -u root doc-repo bash -c 'echo \"# Test Hosts\" >> /etc/hosts'")

    pg = Host("pg", "doc-pg", "image:1", "vagrant")
    group.host_add(pg)

    assert_in('echo "172.17.0.2 pg" >> /etc/hosts\'', COMMAND_LIST[-2])
    assert_in('echo "172.17.0.2 repo" >> /etc/hosts\'', COMMAND_LIST[-1])

    # A host that is not updated is still reachable, it just does not learn the names of the others
    fixed = Host("s3", "doc-s3", "image:1", "vagrant", host_update=False)
    group.host_add(fixed)

    assert_not_in("doc-s3", COMMAND_LIST[-1])


####################################################################################################################################
def test_image_build_cached():
    """An image is taken from the cache when it is there and built and pushed back when it is not."""

    _begin()

    with tempfile.TemporaryDirectory() as path:
        path_docker_file = os.path.join(path, "doc-host.dockerfile")

        image_build_cached(path_docker_file, "pgbackrest/doc:u24", "FROM ubuntu:24.04\n", path, "20260721A")

        # The cache is keyed by what built the image, so an image that has not changed is not built again
        assert_in("docker pull ghcr.io/pgbackrest/doc:u24-20260721A-", COMMAND_LIST[0])
        assert_in("docker tag ghcr.io/pgbackrest/doc:u24-20260721A-", COMMAND_LIST[1])
        assert_equal(len(COMMAND_LIST), 2)


####################################################################################################################################
def test_image_build_cached_miss():
    """An image that is not in the cache is built here and pushed so the next build does not have to."""

    _begin()

    with tempfile.TemporaryDirectory() as path:
        path_docker_file = os.path.join(path, "doc-host.dockerfile")

        # Nothing is in the cache, so every pull fails
        STATUS_MAP.update({"docker pull " + tag: 1 for tag in _tag_list("u24", "20260721A", "FROM ubuntu:24.04\n")})

        image_build_cached(path_docker_file, "pgbackrest/doc:u24", "FROM ubuntu:24.04\n", path, "20260721A")

        assert_equal(file_read(path_docker_file), "FROM ubuntu:24.04\n")
        assert_in("docker build -f %s -t pgbackrest/doc:u24 %s" % (path_docker_file, path), COMMAND_LIST)
        assert_in("docker push ghcr.io/pgbackrest/doc:u24-20260721A-", COMMAND_LIST[-1])


####################################################################################################################################
def test_image_build_cached_fork():
    """On a fork the cache of the fork is used as well, since that is the one the fork can write to."""

    _begin()

    os.environ["GITHUB_REPOSITORY_OWNER"] = "SomeOne"

    try:
        with tempfile.TemporaryDirectory() as path:
            # The project cache does not have it but the cache of the fork does
            STATUS_MAP["docker pull " + _tag_list("u24", "0", "FROM ubuntu:24.04\n")[0]] = 1

            image_build_cached(os.path.join(path, "doc-host.dockerfile"), "pgbackrest/doc:u24", "FROM ubuntu:24.04\n", path, "0")

            assert_in("docker pull ghcr.io/pgbackrest/doc:", COMMAND_LIST[0])
            assert_in("docker pull ghcr.io/someone/doc:", COMMAND_LIST[1])
            assert_in("docker tag ghcr.io/someone/doc:", COMMAND_LIST[2])
    finally:
        del os.environ["GITHUB_REPOSITORY_OWNER"]


####################################################################################################################################
def _tag_list(name, revision, script):
    """The tags an image would be cached under, which a test needs in order to say which of them is a miss."""

    import hashlib

    cache_name = "%s-%s-%s" % (name, revision, hashlib.sha1((script + revision).encode()).hexdigest()[:12])

    return ["ghcr.io/pgbackrest/doc:%s" % cache_name, "ghcr.io/someone/doc:%s" % cache_name]


####################################################################################################################################
def test_image_build_cached_push_fail():
    """A build that cannot be pushed is still a build, since pushing needs access a local build does not have."""

    _begin()

    with tempfile.TemporaryDirectory() as path:
        tag = _tag_list("u24", "0", "FROM ubuntu:24.04\n")[0]

        STATUS_MAP["docker pull " + tag] = 1
        STATUS_MAP["docker push " + tag] = 1

        image_build_cached(os.path.join(path, "doc-host.dockerfile"), "pgbackrest/doc:u24", "FROM ubuntu:24.04\n", path, "0")

        assert_in("docker push " + tag, COMMAND_LIST)


####################################################################################################################################
def test_host_group_fixed():
    """A host that does not learn the names of the others is still told to the others."""

    _begin()

    group = HostGroup()

    group.host_add(Host("s3", "doc-s3", "image:1", "vagrant", host_update=False))
    group.host_add(Host("pg", "doc-pg", "image:1", "vagrant"))

    # The host that was added learns about the fixed one, and the fixed one is not told about it
    assert_in('echo "172.17.0.2 s3" >> /etc/hosts\'', COMMAND_LIST[-1])
    assert_not_in("doc-s3", COMMAND_LIST[-1])
