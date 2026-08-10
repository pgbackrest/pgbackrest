"""Documentation Hosts.

The user guide is built by running its commands on real hosts, which are containers started here. A host is what a reader would have
in front of them, so a command runs as the user the documentation says to run it as and against a host that knows the others by
name.

Building an image for a host is expensive, so a built image is cached in the container registry keyed by a hash of what built it.
That way a run only pays for an image when the image has actually changed."""

####################################################################################################################################
import hashlib
import os

from common.exec import Exec, exec_result
from common.log import *
from common.storage import file_write
from config.project import PROJECT_EXE


####################################################################################################################################
def image_build_cached(path_docker_file, image, script, path_context, revision):
    """Build an image, taking it from the registry cache when it is there and putting a fresh build back.

    The cache is keyed by a hash of the script combined with a revision that is set by hand, which forces a rebuild when the script
    is unchanged but what it installs has moved on, e.g. a new PostgreSQL minor."""

    cache_name = "%s-%s-%s" % (
        image.split(":")[-1],
        revision,
        hashlib.sha1((script + revision).encode()).hexdigest()[:12],
    )
    cache_tag = "ghcr.io/%s/doc:%s" % (PROJECT_EXE, cache_name)

    # On a fork also use the cache of the fork owner, which is the one the fork can write to
    owner = os.environ.get("GITHUB_REPOSITORY_OWNER", "").lower() or PROJECT_EXE
    fork_cache_tag = "ghcr.io/%s/doc:%s" % (owner, cache_name) if owner != PROJECT_EXE else None

    # Try the project cache first and then the cache of the fork. A pull that fails, because the image is not there or cannot be
    # read, falls through to the next cache or to building it here.
    for pull_tag in [cache_tag] + ([fork_cache_tag] if fork_cache_tag is not None else []):
        log(INFO, "Checking cache %s ..." % pull_tag)

        pull = Exec("docker pull %s" % pull_tag)
        pull.begin()

        if pull.end() == 0:
            log(INFO, "Using cached %s image" % pull_tag)
            exec_result("docker tag %s %s" % (pull_tag, image))

            return

    log(INFO, "Building %s image (%s) ..." % (image, cache_tag))

    file_write(path_docker_file, script)
    exec_result("docker build -f %s -t %s %s" % (path_docker_file, image, path_context), suppress_stderr=True)

    # Push to the cache this run can write to, which is the cache of the fork on a fork. Best effort, since it needs write access
    # that a local run and a pull request from a fork do not have; those just use the image they built.
    push_tag = fork_cache_tag if fork_cache_tag is not None else cache_tag
    exec_result("docker tag %s %s" % (image, push_tag), suppress_error=True)

    push = Exec("docker push %s" % push_tag)
    push.begin()

    if push.end() == 0:
        log(INFO, "Cached %s image" % push_tag)


####################################################################################################################################
class Host:
    """A host the documentation runs commands on."""

    def __init__(self, name, container, image, user, mount_list=None, option=None, param=None, host_update=True):
        self.name = name
        self.container = container
        self.user = user
        self.host_update = host_update  # Does this host learn the names of the others?

        exec_result("docker rm -f %s" % container, suppress_error=True)

        exec_result(
            "docker run -itd -h %s --name=%s%s%s %s %s"
            % (
                name,
                container,
                "" if option is None else " " + option,
                "" if mount_list is None else " -v " + " -v ".join(mount_list),
                image,
                "" if param is None else " " + param,
            ),
            suppress_stderr=True,
        )

        self.ip = exec_result("docker inspect --format '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' %s" % container)[
            1
        ].strip()

        self.active = True

    ################################################################################################################################
    def remove(self):
        """Remove the host."""

        if self.active:
            exec_result("docker rm -f %s" % self.container, suppress_error=True)
            self.active = False

    ################################################################################################################################
    def execute(
        self,
        command,
        user=None,
        load_env=True,
        bash_wrap=True,
        status_expect=0,
        suppress_error=False,
        suppress_stderr=False,
        retry=None,
    ):
        """Run a command on the host and return the exit status, the output, and the error.

        A command runs through a login shell unless it says otherwise, so it sees the environment a reader would have."""

        return exec_result(
            "docker exec -u %s %s%s"
            % (
                self.user if user is None else user,
                self.container,
                (" bash%s -c '%s'" % (" -l" if load_env else "", command.replace("'", "'\\''")) if bash_wrap else " " + command),
            ),
            status_expect=status_expect,
            suppress_error=suppress_error,
            suppress_stderr=suppress_stderr,
            retry=retry,
        )

    ################################################################################################################################
    def copy_to(self, source, destination, owner=None, mode=None):
        """Copy a file to the host, and set what it belongs to and what may be done with it."""

        exec_result("docker cp %s %s:%s" % (source, self.container, destination))

        if owner is not None:
            self.execute("chown %s %s" % (owner, destination), user="root")

        if mode is not None:
            self.execute("chmod %s %s" % (mode, destination), user="root")

    ################################################################################################################################
    def copy_from(self, source, destination):
        """Copy a file from the host."""

        exec_result("docker cp %s:%s %s" % (self.container, source, destination))


####################################################################################################################################
class HostGroup:
    """The hosts of a documentation build, which know each other by name."""

    def __init__(self):
        self.host_map = {}

    ################################################################################################################################
    def host_add(self, host):
        """Add a host and tell it and every other host how to reach each other."""

        self.host_map[host.name] = host

        if host.host_update:
            host.execute('echo "" >> /etc/hosts', user="root", load_env=False)
            host.execute('echo "# Test Hosts" >> /etc/hosts', user="root", load_env=False)

        for other_name in sorted(self.host_map):
            other = self.host_map[other_name]

            if other_name == host.name:
                continue

            if other.host_update:
                other.execute('echo "%s %s" >> /etc/hosts' % (host.ip, host.name), user="root", load_env=False)

            if host.host_update:
                host.execute('echo "%s %s" >> /etc/hosts' % (other.ip, other_name), user="root", load_env=False)
