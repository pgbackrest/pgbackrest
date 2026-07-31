"""Test Container Build.

The script for one vm is compared in full, since its hash is the cache key and any change to it rebuilds every image built from it.
The user and group are whoever runs the tests so the expected script names them with tokens, and the certificate and key data is
collapsed because it is copied in a line at a time and listing every line would only make the expected script harder to read.

Docker is not run here. What matters is the script that goes into an image and the tag it is looked up under, so the commands are
captured and checked instead."""

####################################################################################################################################
import os
import re
import tempfile
from unittest.mock import patch

from harness.test import *

from command.test.container import *
from common.error import *
from common.log import *
from common.storage import file_read, file_write
from common.user import group_id, group_name, user_id, user_name
from common.vm import *

# Revisions as test/container.yaml declares them
REVISION = {"all": "1", VM_U24: "2", "%s-%s" % (VM_U24, VM_ARCH_X86_64): "3"}

# The base script for a vm that installs from the PostgreSQL repo and collects coverage, i.e. one that exercises every option the
# debian build has
SCRIPT_BASE_U24 = """ && \\

# Install packages
    export DEBCONF_NONINTERACTIVE_SEEN=true DEBIAN_FRONTEND=noninteractive && \\
    apt-get update && \\
    apt-get install -y --no-install-recommends openssh-server sudo gcc make git \\
        ca-certificates libssl-dev tzdata zlib1g-dev libxml2-dev pkg-config \\
        libbz2-dev bzip2 liblz4-dev liblz4-tool gnupg lsb-release ccache meson \\
        libssh2-1-dev libcurl4-openssl-dev libsystemd-dev python3-yaml valgrind python3-coverage zstd libzstd-dev && \\

# Regenerate SSH keys
    rm -f /etc/ssh/ssh_host_rsa_key* && \\
    ssh-keygen -t rsa -b 2048 -f /etc/ssh/ssh_host_rsa_key && \\

# Fix root tty
    sed -i 's/^mesg n/tty -s \\&\\& mesg n/g' /root/.profile && \\

# Suppress dpkg interactive output
    rm /etc/apt/apt.conf.d/70debconf && \\

# Install PostgreSQL packages
    apt-get install -y --no-install-recommends postgresql-common && \\
    /usr/share/postgresql-common/pgdg/apt.postgresql.org.sh -y -c 19 && \\
    apt-get install -y --no-install-recommends postgresql-common libpq-dev && \\
    sed -i 's/^\\#create\\_main\\_cluster.*$/create\\_main\\_cluster \\= false/' /etc/postgresql-common/createcluster.conf && \\

# Install PostgreSQL
    apt-get install -y --no-install-recommends postgresql-9.6 postgresql-10 postgresql-11 postgresql-12 postgresql-13 \
postgresql-14 postgresql-15 postgresql-16 postgresql-17 postgresql-18 postgresql-19 && \\

# Cleanup
    apt-get autoremove -y && \\
    apt-get clean && \\
    rm -rf /var/lib/apt/lists/*"""

# The test script for the same vm
SCRIPT_TEST_U24 = """ && \\

# Install CA
    echo '-----BEGIN CERTIFICATE-----' > /usr/local/share/ca-certificates/pgbackrest-test-ca.crt && \\
[DATA]
    echo '-----END CERTIFICATE-----' >> /usr/local/share/ca-certificates/pgbackrest-test-ca.crt && \\
    chmod 644 /usr/local/share/ca-certificates/pgbackrest-test-ca.crt && \\
    update-ca-certificates && \\

# Create banner to make sure pgBackRest ignores it
    echo '***********************************************' >  /etc/issue.net && \\
    echo 'Sample banner to make sure banners are skipped.' >> /etc/issue.net && \\
    echo ''                                                >> /etc/issue.net && \\
    echo 'More banner after a blank line.'                 >> /etc/issue.net && \\
    echo '***********************************************' >> /etc/issue.net && \\
    echo 'Banner /etc/issue.net'                           >> /etc/ssh/sshd_config && \\

    echo '# Add PubkeyAcceptedAlgorithms (required for SFTP)'              >> /etc/ssh/sshd_config && \\
    echo 'HostKeyAlgorithms=+ssh-rsa,ssh-rsa-cert-v01@openssh.com'        >> /etc/ssh/sshd_config && \\
    echo 'PubkeyAcceptedAlgorithms=+ssh-rsa,ssh-rsa-cert-v01@openssh.com' >> /etc/ssh/sshd_config && \\

# Rename conflicting group
    sed -i 's/^[^:]*:x:[GROUP_ID]:.*/[GROUP]:x:[GROUP_ID]:/' /etc/group && \\

# Remove conflicting user
    sed -i '/^[^:]*:x:[USER_ID]:/d' /etc/passwd && \\

# Create test user
    groupadd -f -g[GROUP_ID] [GROUP] && \\
    adduser --uid=[USER_ID] --ingroup=[GROUP] --disabled-password --gecos "" [USER] && \\
    mkdir -m 750 /home/[USER]/test && \\
    chown [USER]:[GROUP] /home/[USER]/test && \\

# Configure sudo
    echo '%[GROUP] ALL=(ALL) NOPASSWD: ALL' >> /etc/sudoers && \\

# Setup SSH
    mkdir /home/[USER]/.ssh && \\
    echo '-----BEGIN RSA PRIVATE KEY-----' > /home/[USER]/.ssh/id_rsa && \\
[DATA]
    echo '-----END RSA PRIVATE KEY-----' >> /home/[USER]/.ssh/id_rsa && \\
    echo 'ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQDlXJKkRqflGftBDlc/XdiRfFOD5en0355zIOEP3Y5CTRVk4WcQqVdRoxcfRZWNFGCOBsXvwk\
cVsRCNQeKZvOv16/Ui4NohLjYTWdSQFoeCCcc0BbZqq1zNQNKcsZxep3Rd4O3CmOPUjTFY54mWL/GNT55LCPOo2BfplfY/S5Nd+W7bni5KM4XcgBGj0\
WIMQpB2SyjE04CEJNiAMDKeaRx8n17zsWOJMaZ+dk7cfqBTaSJ1IVevB0e103dCW/pGAKugsYBlmma2oGitPBKFZFJxYACD86pbAumF81tcggdk7gQn\
hLYkuCyAkdvbRInZcQjYiWf2yP6gJaAsnab5Eu9b user@pgbackrest-test' > /home/[USER]/.ssh/authorized_keys && \\
    echo 'Host *' > /home/[USER]/.ssh/config && \\
    echo '    StrictHostKeyChecking no' >> /home/[USER]/.ssh/config && \\
    cp /home/[USER]/.ssh/authorized_keys /home/[USER]/.ssh/id_rsa.pub && \\
    chown -R [USER]:[GROUP] /home/[USER]/.ssh && \\
    chmod 700 /home/[USER]/.ssh && \\
    chmod 600 /home/[USER]/.ssh/* && \\

# Make [USER] home dir readable
    chmod g+r,g+x /home/[USER]

# Start SSH when container starts
ENTRYPOINT service ssh restart && bash"""


####################################################################################################################################
def _expect(script):
    """Fill in the user and group the tests run as."""

    return (
        script.replace("[USER_ID]", str(user_id()))
        .replace("[GROUP_ID]", str(group_id()))
        .replace("[USER]", user_name())
        .replace("[GROUP]", group_name())
    )


####################################################################################################################################
def _collapse(script):
    """Collapse the certificate and key data, which is copied in a line at a time and is not generated."""

    return re.sub(r"(    echo '[A-Za-z0-9+/=]{40,}' >> [^\n]+\n)+", "[DATA]\n", script)


####################################################################################################################################
def _repo_write(path):
    """Write the part of the repository the container build reads."""

    file_write(os.path.join(path, "test/container.yaml"), "all: '1'\n%s: '2'\n%s-%s: '3'\n" % (VM_U24, VM_U24, VM_ARCH_X86_64))
    file_write(
        os.path.join(path, "test/certificate/pgbackrest-test-ca.crt"),
        "-----BEGIN CERTIFICATE-----\nQUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVphYmNkZWZnaGlqa2xtbm9w\n-----END CERTIFICATE-----\n",
    )


####################################################################################################################################
class _Config:
    """The options the container build reads."""

    def __init__(self, path_repo, vm, vm_arch=VM_ARCH_X86_64, vm_force=False, log_level=INFO):
        self.repo_path = path_repo
        self.vm = vm
        self.vm_arch = vm_arch
        self.vm_force = vm_force
        self.log_level = log_level


####################################################################################################################################
class _Docker:
    """Capture the commands the build would run and report whatever status the test asked for."""

    def __init__(self, status=0, status_push=None):
        self.command_list = []
        self.status = status  # Status reported for a command that is allowed to fail, e.g. a pull from a cache
        self.status_push = status if status_push is None else status_push

    ################################################################################################################################
    def one(self, command, result_expect=0, show_output=False):
        self.command_list.append(command)

        if command.startswith("docker images"):
            return "%s:%s-base-%s\nubuntu:24.04\n" % (container_repo(), VM_U24, VM_ARCH_X86_64)

        if command.startswith("docker ps"):
            return "test-0\ntest-build\nunrelated\n"

        return ""

    ################################################################################################################################
    def status_get(self, command):
        self.command_list.append(command)

        return self.status_push if command.startswith("docker push") else self.status

    ################################################################################################################################
    def match(self, expression):
        """The commands that match an expression."""

        return [command for command in self.command_list if re.search(expression, command)]


####################################################################################################################################
def test_container_repo():
    """Images are named in the pgbackrest namespace unless an owner asks for its own."""

    assert_equal(container_repo(), "ghcr.io/pgbackrest/test")
    assert_equal(container_repo("someone"), "ghcr.io/someone/test")


####################################################################################################################################
def test_container_script():
    """The generated script is what goes into the image."""

    with tempfile.TemporaryDirectory() as path:
        _repo_write(path)

        assert_equal(script_base(VM_U24, VM_ARCH_X86_64), SCRIPT_BASE_U24)
        assert_equal(_collapse(script_test(VM_U24, path)), _expect(SCRIPT_TEST_U24))

        # The script is wrapped in a Dockerfile that runs it as a single layer
        assert_equal(
            container_script("Base", "ubuntu:24.04", " && \\\n\n# Install packages\n    apt-get update"),
            "# Base Container\nFROM ubuntu:24.04\n\nRUN echo 'OPTIMIZED BUILD' && \\\n\n# Install packages\n    apt-get update",
        )

        # A script with nothing in it is just the image it came from
        assert_equal(container_script("Base", "ubuntu:24.04", ""), "# Base Container\nFROM ubuntu:24.04")


####################################################################################################################################
def test_container_script_vm():
    """Each vm installs what its os base and PostgreSQL repository need."""

    # RHEL 8 has no valgrind or coverage package and needs a python that PyYAML is packaged for
    script = script_base(VM_RH8, VM_ARCH_X86_64)

    assert_true("--set-enabled powertools" in script)
    assert_true("valgrind" not in script)
    assert_true("python3.12-pyyaml" in script)

    # RHEL 8 installs PostgreSQL from the application stream rather than from the PostgreSQL repo
    assert_true("dnf -y module enable postgresql:10" in script)
    assert_true("yum -y install postgresql-server" in script)
    assert_true("ENV PATH=" not in script)

    # RHEL 9 packages a coverage too old to report the branch detail the report needs, so it comes from pip
    script = script_base(VM_RH9, VM_ARCH_X86_64)

    assert_true("python3-pip" in script)
    assert_true("pip3 install --no-cache-dir 'coverage>=6.5'" in script)
    assert_true("RPM-GPG-KEY-PGDG" in script)
    assert_true("reporpms/EL-9-%s/" % VM_ARCH_X86_64 in script)
    assert_true("ENV PATH=/usr/pgsql-18/bin:$PATH" in script)

    # RHEL 10 cannot check the PostgreSQL repo signature so it skips the key and disables the check
    script = script_base(VM_RH10, VM_ARCH_X86_64)

    assert_true("python3-coverage" in script)
    assert_true("RPM-GPG-KEY-PGDG" not in script)
    assert_true("s/gpgcheck=1/gpgcheck=0/g" in script)

    # Fedora installs from the Fedora build of the PostgreSQL repo
    assert_true("reporpms/F-44-%s/" % VM_ARCH_X86_64 in script_base(VM_F44, VM_ARCH_X86_64))

    # Debian 12 installs PostgreSQL from the distribution and runs neither valgrind nor coverage
    script = script_base(VM_D12, VM_ARCH_X86_64)

    assert_true("valgrind" not in script)
    assert_true("python3-coverage" not in script)
    assert_true("apt.postgresql.org.sh" not in script)

    # Ubuntu 22.04 is the last release the python tests do not run on
    assert_true("python3-coverage" not in script_base(VM_U22, VM_ARCH_X86_64))

    # Alpine installs everything from one package manager and needs no PostgreSQL repo
    script = script_base(VM_A321, VM_ARCH_X86_64)

    assert_true("apk add --no-cache postgresql15 postgresql16 postgresql17" in script)
    assert_true("Install PostgreSQL packages" not in script)

    # An architecture the PostgreSQL repo does not build for installs one version for the smoke test
    script = script_base(VM_U24, VM_ARCH_PPC64LE)

    assert_true("apt.postgresql.org.sh -y &&" in script)
    assert_true("Install a single PostgreSQL version for the smoke test" in script)

    # There is nothing to install for alpine on that architecture, since it has no smoke test
    assert_true("Install PostgreSQL" not in script_base(VM_A324, VM_ARCH_PPC64LE))


####################################################################################################################################
def test_container_script_test_vm():
    """The test image adds the test user and what it needs on each os base."""

    with tempfile.TemporaryDirectory() as path:
        _repo_write(path)

        # RHEL puts the certificate authority elsewhere, needs sudo configured differently, and starts ssh differently. The
        # architecture is the host architecture rather than the one being built for, so it is set here to get the same script
        # whichever machine the tests are run on.
        with patch("command.test.container.host_arch", return_value=VM_ARCH_X86_64):
            script = script_test(VM_RH9, path)

        assert_true("/etc/pki/ca-trust/source/anchors/pgbackrest-test-ca.crt" in script)
        assert_true("update-ca-trust extract" in script)
        assert_true("adduser -g%s -u%u -N %s" % (group_name(), user_id(), user_name()) in script)
        assert_true('echo "Set disable_coredump false" >> /etc/sudo.conf' in script)
        assert_true("ENTRYPOINT rm -rf /run/nologin && /usr/sbin/sshd -D" in script)

        # The language setup only gets in the way on aarch64
        assert_true("rm /etc/profile.d/lang.sh" not in script)

        with patch("command.test.container.host_arch", return_value=VM_ARCH_AARCH64):
            assert_true("rm /etc/profile.d/lang.sh" in script_test(VM_RH9, path))

        # Alpine creates the group differently, needs a password for the test user, and starts ssh without a service
        script = script_test(VM_A321, path)

        assert_true("getent group %s >/dev/null || addgroup" % group_name() in script)
        assert_true("passwd -d '%s' %s" % (user_name(), user_name()) in script)
        assert_true("ENTRYPOINT /usr/sbin/sshd -D" in script)

        # Only the releases with an ssh that rejects the test key by default need the algorithms added back
        assert_true("PubkeyAcceptedAlgorithms" not in script_test(VM_D12, path))


####################################################################################################################################
def test_container_revision():
    """A revision must be readable and name a vm that exists, else a rebuild would silently never happen."""

    assert_is_none(revision_check(REVISION))

    with assert_raises(ToolError) as error:
        revision_check(["1"])

    assert_equal(str(error.exception), "the 'all' revision is required in test/container.yaml")

    with assert_raises(ToolError) as error:
        revision_check({VM_U24: "1"})

    assert_equal(str(error.exception), "the 'all' revision is required in test/container.yaml")

    with assert_raises(ToolError) as error:
        revision_check({VM_ALL: "1", VM_U24: None})

    assert_equal(str(error.exception), "revision '%s' in test/container.yaml must be set to a value" % VM_U24)

    with assert_raises(ToolError) as error:
        revision_check({VM_ALL: "1", "bogus": "1"})

    assert_equal(str(error.exception), "revision 'bogus' in test/container.yaml has invalid vm 'bogus'")

    # There is no container for none so a revision for it could never be used
    with assert_raises(ToolError) as error:
        revision_check({VM_ALL: "1", "none": "1"})

    assert_equal(str(error.exception), "revision 'none' in test/container.yaml has invalid vm 'none'")

    with assert_raises(ToolError) as error:
        revision_check({VM_ALL: "1", "%s-bogus" % VM_U24: "1"})

    assert_equal(str(error.exception), "revision '%s-bogus' in test/container.yaml has invalid architecture 'bogus'" % VM_U24)


####################################################################################################################################
def test_container_build():
    """A base image is pulled from the cache when it is there and built and pushed when it is not."""

    with tempfile.TemporaryDirectory() as path:
        _repo_write(path)

        image_base = "%s:%s-base-%s" % (container_repo(), VM_U24, VM_ARCH_X86_64)
        image_test = "%s:%s-test-%s" % (container_repo(), VM_U24, VM_ARCH_X86_64)

        # The base image is in the cache so only the test image is built. The revision for the vm and architecture is the most
        # specific one so it is the one in the cache tag.
        docker = _Docker(status=0)

        with patch("command.test.container.exec_one", docker.one):
            with patch("command.test.container.exec_status", docker.status_get):
                container_build(_Config(path, VM_U24))

        assert_equal(len(docker.match("^docker pull .*-base-.*-3-")), 1)
        assert_equal(docker.match("^docker tag"), ["docker tag %s-3-%s %s" % (image_base, _hash(docker), image_base)])
        assert_equal(len(docker.match("^docker build")), 1)
        assert_true(docker.match("^docker build")[0].endswith("-t %s %s" % (image_test, os.path.join(path, "test"))))

        # The Dockerfile is written where the build can find it and is the script with the trailing separator removed
        assert_true(file_read(os.path.join(path, "test/result/docker/%s-test-%s" % (VM_U24, VM_ARCH_X86_64))).endswith("bash\n"))

        # Nothing is in the cache so the base image is built and pushed. A fork pushes to its own cache since it cannot write to
        # the pgbackrest one.
        docker = _Docker(status=1, status_push=0)

        with patch("command.test.container.exec_one", docker.one):
            with patch("command.test.container.exec_status", docker.status_get):
                with patch.dict(os.environ, {"GITHUB_REPOSITORY_OWNER": "Someone"}):
                    container_build(_Config(path, VM_U24, log_level=DETAIL))

        assert_equal(len(docker.match("^docker pull %s:" % container_repo())), 1)
        assert_equal(len(docker.match("^docker pull %s:" % container_repo("someone"))), 1)
        assert_equal(len(docker.match("^docker build")), 2)
        assert_equal(len(docker.match("^docker push %s:" % container_repo("someone"))), 1)

        # A push that fails is not reported, since write access is not available everywhere
        docker = _Docker(status=1)

        with patch("command.test.container.exec_one", docker.one):
            with patch("command.test.container.exec_status", docker.status_get):
                container_build(_Config(path, VM_D12))

        assert_equal(len(docker.match("^docker push %s:" % container_repo())), 1)

        # Force removes what was built before and builds without the cache, both the images and the files they were built from
        docker = _Docker(status=1)

        with patch("command.test.container.exec_one", docker.one):
            with patch("command.test.container.exec_status", docker.status_get):
                container_build(_Config(path, VM_U24, vm_force=True))

        assert_equal(docker.match("^rm -f"), ["rm -f %s/test/result/docker/%s-*" % (path, VM_U24)])
        assert_equal(docker.match("^docker rmi"), ["docker rmi -f %s" % image_base])
        assert_equal(len(docker.match("^docker pull")), 0)
        assert_equal(len(docker.match("--no-cache")), 2)

        # Every vm is built when all of them are asked for, and force then removes every file and image
        docker = _Docker(status=1)

        with patch("command.test.container.exec_one", docker.one):
            with patch("command.test.container.exec_status", docker.status_get):
                container_build(_Config(path, VM_ALL, vm_force=True))

        assert_equal(docker.match("^rm -f"), ["rm -f %s/test/result/docker/*" % path])
        assert_equal(len(docker.match("^docker build")), len(VM_LIST) * 2)


####################################################################################################################################
def _hash(docker):
    """The hash the build put in the cache tag, which is a hash of the script so it cannot be written out here."""

    return re.search(r"-base-[^ ]*-3-([0-9a-f]{12})", docker.match("^docker pull")[0]).group(1)


####################################################################################################################################
def test_container_remove():
    """Containers are removed by name and an image that does not match is left alone."""

    docker = _Docker()

    with patch("command.test.container.exec_one", docker.one):
        with patch("command.test.container.exec_status", docker.status_get):
            container_remove("^test-([0-9]+|build)$")

    assert_equal(docker.match("^docker rm"), ["docker rm -f test-0", "docker rm -f test-build"])
