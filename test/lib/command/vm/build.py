"""Vm Build Command.

Generates a Dockerfile for each vm and builds it. Every vm has a base image with the packages the tests need and a test image that
adds the test user, ssh, and the certificate authority, so a change to the test setup does not rebuild the packages.

What a vm installs and configures comes from its definition in test/container.yaml rather than from a test on its name here, so a
vm that needs a package or a repository the others do not is a change to the declaration rather than to the script generated here.

Base images are cached in a container registry keyed by a hash of the generated Dockerfile combined with the manual revision in
test/container.yaml. The revision forces a rebuild when the Dockerfile is unchanged but the upstream packages have changed, e.g. a
new PostgreSQL beta.

The containers are defined here so the registry they are tagged in and removing them by name are here as well, which is what a test
run needs to know about them."""

####################################################################################################################################
import hashlib
import os
import re

from common.error import check
from common.exec import exec_one, exec_status
from common.log import *
from common.storage import file_read, file_write, path_create
from common.user import group_id, group_name, user_id, user_name
from common.vm import *
from common.yaml import yaml_load, yaml_map_dict

# Separator between sections of the generated script. Every section is a continuation of a single RUN so the image has one layer.
_SECTION = " && \\\n\n"

# The pgbackrest namespace is the canonical cache that every environment pulls from and only pgbackrest CI can push to. A fork
# pushes to and pulls from its own namespace instead.
_REPO_OWNER = "pgbackrest"


####################################################################################################################################
def container_repo(owner=None):
    """Container registry the images are stored in.

    Pass an owner to build the tag for that owner's own cache. Images are always tagged in the pgbackrest namespace locally so
    docker calls do not need to know which cache they came from."""

    return "ghcr.io/%s/test" % (_REPO_OWNER if owner is None else owner)


####################################################################################################################################
def _ssh_setup(user, group):
    """Install the test key pair so tests and the documentation can ssh between containers."""

    path_user = "/%s" % user if user == "root" else "/home/%s" % user

    key_list = [
        "-----BEGIN RSA PRIVATE KEY-----",
        "MIIEowIBAAKCAQEA5VySpEan5Rn7QQ5XP13YkXxTg+Xp9N+ecyDhD92OQk0VZOFn",
        "EKlXUaMXH0WVjRRgjgbF78JHFbEQjUHimbzr9ev1IuDaIS42E1nUkBaHggnHNAW2",
        "aqtczUDSnLGcXqd0XeDtwpjj1I0xWOeJli/xjU+eSwjzqNgX6ZX2P0uTXflu254u",
        "SjOF3IARo9FiDEKQdksoxNOAhCTYgDAynmkcfJ9e87FjiTGmfnZO3H6gU2kidSFX",
        "rwdHtdN3Qlv6RgCroLGAZZpmtqBorTwShWRScWAAg/OqWwLphfNbXIIHZO4EJ4S2",
        "JLgsgJHb20SJ2XEI2Iln9sj+oCWgLJ2m+RLvWwIDAQABAoIBAArBC0EiQkyxf1Xe",
        "txmKAWsE4iI85/oqzVJG7YvhuVdY0j16J2vLvNk05T3P9JdPqB1QqlGNEZSDSlbi",
        "isjm55tkFl4tyRx61F9A5tYLWwwuVYWWFPutuuVcJOPi8gWAItUkruaLu6GjgyJQ",
        "143QA//lBp4sYRxUEX71defO19iKkDz+xEuOzYMd16j76OKMcmbnog7hbMrXR4Yi",
        "kNXuhnwBadutaXLve3mZ0JowrZyHKfTUWOHgvuULpCVD5su3NdbpE2AVn1idcF8V",
        "jaj6p0vtcvEnXEC69XwX+yL0TgvOE4Vu/OWg8lDWQdetONIHGbElJZvB6eyTF1nl",
        "IIgLc2ECgYEA7PL3soOfH3dMNUt4KGSw1cK/kwvy7UsT6QrAPi1Cc/A4szluk6/O",
        "5YhKfTjzHW5WDmsTTAcT29MLmW8dQXeUAe/1BtIATubsav+uSelfBmUAnQj9fvKT",
        "ieJ6JMf20OTbS6XODA3+jJAdApLCu61Lv6nePOuNzLY/uqSMWu9kO/sCgYEA981v",
        "YIUaadFaHnPnmax0+jJMs8S5AIEjSfSIxR8oNOWNUxBBvwFd4zWTApVfZqKjmI83",
        "Ng5tISxspzHseakyrIpoDqzxRQPxZF7RTO6VUX+ZQj6iIXVp9FDqWAjvDACuSky2",
        "mGAiiA+fWZ1za62opgoYQZ17O17SyHF9/vJ7XCECgYANwNyXxAQMc4Q847CJx65r",
        "+e3cvyjOlTkGodUexsnAqQThgkfk0qOTtyF7uz6BStI77AMmupJwhAN8WHK+Rg6V",
        "PjRevPm/mq/GVijrqVwWpu4uL0NnhvUBX9/vGpw868u+zFT1ZiqMRiEo8RPUiO6I",
        "pXd82b9VTo7Mapiq/pI22QKBgC8Kb586BUabOGlZhVi11Ur9q3Pg32HKIgHTCveo",
        "r4BDJ23iQyjYQJN2Qx8VbhPUwgue/FMlr+/BOCsRHhwGU5lPeOt4RyDb28I7Aa6C",
        "CBR9jYF21F5XpLJ9fc8SexajNnLiVzNb5JJBrPVdH2EMiVxjxDEIjTE7EfZ9HPb9",
        "3w8hAoGBALyik3jr0W6DxzqOW0jPXPLDqp1JvzieC03nZD1scWeI8qIzUOpLX7cc",
        "jaMU/8QMBRvyEcZK82Cedilm30nLf+C/FR5TsUmftS7IcjoC4Z2ZXWNOhMv22TUJ",
        "Ml6z//+WSZ3qVZ5rvAeo4obwiBfe+Uh+AyyprEGgmimF9qDejcwc",
        "-----END RSA PRIVATE KEY-----",
    ]

    public_key = (
        "ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQDlXJKkRqflGftBDlc/XdiRfFOD5en0355zIOEP3Y5CTRVk4WcQqVdRoxcfRZWNFGCOBsXvwk"
        "cVsRCNQeKZvOv16/Ui4NohLjYTWdSQFoeCCcc0BbZqq1zNQNKcsZxep3Rd4O3CmOPUjTFY54mWL/GNT55LCPOo2BfplfY/S5Nd+W7bni5KM4XcgBGj0"
        "WIMQpB2SyjE04CEJNiAMDKeaRx8n17zsWOJMaZ+dk7cfqBTaSJ1IVevB0e103dCW/pGAKugsYBlmma2oGitPBKFZFJxYACD86pbAumF81tcggdk7gQn"
        "hLYkuCyAkdvbRInZcQjYiWf2yP6gJaAsnab5Eu9b user@pgbackrest-test"
    )

    result = _SECTION + "# Setup SSH\n" + "    mkdir %s/.ssh && \\\n" % path_user

    for idx, key in enumerate(key_list):
        result += "    echo '%s' %s %s/.ssh/id_rsa && \\\n" % (key, ">" if idx == 0 else ">>", path_user)

    result += (
        "    echo '%s' > %s/.ssh/authorized_keys && \\\n" % (public_key, path_user)
        + "    echo 'Host *' > %s/.ssh/config && \\\n" % path_user
        + "    echo '    StrictHostKeyChecking no' >> %s/.ssh/config && \\\n" % path_user
        + "    cp %s/.ssh/authorized_keys %s/.ssh/id_rsa.pub && \\\n" % (path_user, path_user)
        + "    chown -R %s:%s %s/.ssh && \\\n" % (user, group, path_user)
        + "    chmod 700 %s/.ssh && \\\n" % path_user
        + "    chmod 600 %s/.ssh/*" % path_user
    )

    return result


####################################################################################################################################
def _file_copy(path_repo, file_source, file_dest):
    """Copy a text file into the container.

    This does not work if the file contains single quotes."""

    result = ""

    # Trailing newlines are dropped so the file does not end up with a blank line appended to it
    for line in file_read(os.path.join(path_repo, file_source)).rstrip("\n").split("\n"):
        result += "    echo '%s' %s %s && \\\n" % (line, ">>" if result else ">", file_dest)

    return result


####################################################################################################################################
def _ca_setup(os_base, path_repo, file_ca):
    """Install the certificate authority the test certificates are signed with."""

    if os_base == VM_OS_BASE_RHEL:
        file_cert = "/etc/pki/ca-trust/source/anchors"
    else:
        file_cert = "/usr/local/share/ca-certificates"

    file_cert += "/pgbackrest-test-ca.crt"

    result = _SECTION + "# Install CA\n" + _file_copy(path_repo, file_ca, file_cert) + "    chmod 644 %s && \\\n" % file_cert

    if os_base == VM_OS_BASE_RHEL:
        result += "    update-ca-trust extract"
    else:
        result += "    update-ca-certificates"

    return result


####################################################################################################################################
def _group_create(os_base, name, id):
    """Command to create a group."""

    if os_base == VM_OS_BASE_ALPINE:
        return "getent group %s >/dev/null || addgroup -g%u %s" % (name, id, name)

    return "groupadd -f -g%u %s" % (id, name)


####################################################################################################################################
def _user_create(os_base, name, id, group):
    """Command to create a user."""

    if os_base == VM_OS_BASE_RHEL:
        return "adduser -g%s -u%u -N %s" % (group, id, name)

    return 'adduser --uid=%u --ingroup=%s --disabled-password --gecos "" %s' % (id, group, name)


####################################################################################################################################
def _entry_point_setup(os_base):
    """Start ssh when the container starts."""

    result = "\n\n# Start SSH when container starts\n" + "ENTRYPOINT "

    if os_base == VM_OS_BASE_RHEL:
        result += "rm -rf /run/nologin && /usr/sbin/sshd -D"
    elif os_base == VM_OS_BASE_DEBIAN:
        result += "service ssh restart && bash"
    else:
        result += "/usr/sbin/sshd -D"

    return result


####################################################################################################################################
def _script_package(vm):
    """Install the packages the tests need."""

    result = ""

    # Tune dnf to fail over from a slow mirror. By default a mirror is only abandoned when it delivers less than 1000 bytes per
    # second for 30 seconds, so a mirror that has not failed but is crawling is retried rather than dropped. Raise the rate floor
    # well above that, shorten the timeout, sort mirrors by latency, and fetch in parallel.
    if vm.os_base == VM_OS_BASE_RHEL:
        result += (
            _SECTION
            + "# Tune dnf to fail over from a slow mirror\n"
            + "    { echo minrate=50k; echo timeout=20; echo retries=5; echo max_parallel_downloads=10; \\\n"
            + "      echo fastestmirror=True; } >> /etc/dnf/dnf.conf"
        )

    result += _SECTION + "# Install packages\n"

    if vm.os_base == VM_OS_BASE_RHEL:
        # Packages the tests need that rhel does not ship are in EPEL and CRB, which was called powertools before EL-9
        if vm.epel:
            result += "    dnf install -y dnf-plugins-core && \\\n"

            if vm.powertools:
                result += "    dnf config-manager --set-enabled powertools && \\\n"

            result += "    dnf -y install epel-release && \\\n" + "    crb enable && \\\n"

        result += (
            "    yum -y update && \\\n"
            + "    yum -y install openssh-server openssh-clients sudo git ca-certificates gcc make ccache meson openssl\\\n"
            + "        openssl-devel zlib-devel libxml2-devel lz4-devel lz4 bzip2-devel bzip2 libssh2-devel zstd libzstd-devel\\\n"
            + "        systemd-devel"
        )

        if vm.valgrind:
            result += " valgrind"

        # Coverage for the python tests, which comes from pip where the packaged version predates the per line branch detail the
        # report needs. The pip install itself is below, since it needs the package installed here first.
        if vm.coverage_python == "pip":
            result += " python3-pip"
        elif vm.coverage_python is not None:
            result += " python3-coverage"

        # The test harness needs a python3 that PyYAML is packaged for. Where that is not the platform python, which meson installs
        # and which claims the python3 alternative at a priority nothing else can outrank, point it explicitly.
        if vm.python is None:
            result += " python3-pyyaml"
        else:
            result += " %s %s-pyyaml && \\\n" % (vm.python, vm.python) + "    alternatives --set python3 /usr/bin/%s" % vm.python
    elif vm.os_base == VM_OS_BASE_DEBIAN:
        result += (
            "    export DEBCONF_NONINTERACTIVE_SEEN=true DEBIAN_FRONTEND=noninteractive && \\\n"
            + "    apt-get update && \\\n"
            + "    apt-get install -y --no-install-recommends openssh-server sudo gcc make git \\\n"
            + "        ca-certificates libssl-dev tzdata zlib1g-dev libxml2-dev pkg-config \\\n"
            + "        libbz2-dev bzip2 liblz4-dev liblz4-tool gnupg lsb-release ccache meson \\\n"
            + "        libssh2-1-dev libcurl4-openssl-dev libsystemd-dev python3-yaml"
        )

        if vm.valgrind:
            result += " valgrind"

        # Coverage for the python tests, which are run on u24 and later
        if vm.coverage_python is not None:
            result += " python3-coverage"

        result += " zstd libzstd-dev"
    else:
        result += (
            "    apk update && \\\n"
            + "    apk add --no-cache sudo openssh git tzdata openssh ca-certificates openrc bash && \\\n"
            + "    rc-update add sshd && \\\n"
            + "    apk add --no-cache meson build-base libpq-dev openssl-dev libxml2-dev pkgconfig lz4-dev bzip2-dev\\\n"
            + "        openssh-keygen zlib-dev libssh2-dev valgrind lz4 zstd zstd-dev py3-yaml"
        )

    # Coverage that comes from pip rather than from the distribution
    if vm.coverage_python == "pip":
        result += _SECTION + "# Install python coverage\n" + "    pip3 install --no-cache-dir 'coverage>=6.5'"

    return result


####################################################################################################################################
def _script_pg(vm, arch):
    """Install PostgreSQL."""

    result = ""

    if vm.os_base in (VM_OS_BASE_RHEL, VM_OS_BASE_DEBIAN):
        result += _SECTION + "# Install PostgreSQL packages\n"

    if vm.os_base == VM_OS_BASE_RHEL:
        if not vm.pg_repo:
            # Enable the native PostgreSQL application stream instead of adding the PGDG repo
            result += "    dnf -y module enable postgresql:%s && \\\n" % vm.db_list[-1]
        else:
            # The repo package is named for the distribution the release belongs to, e.g. EL-9 is redhat and F-44 is fedora
            distro = "fedora" if vm.pg_repo_release.startswith("F-") else "redhat"
            package = "https://download.postgresql.org/pub/repos/yum/reporpms/%s-%s/pgdg-%s-repo-latest.noarch.rpm" % (
                vm.pg_repo_release,
                arch,
                distro,
            )

            # The rpm-sequoia OpenPGP backend rejects the SHA-1 binding signature in the PGDG GPG key, and the crypto-policy SHA1
            # subpolicy that could re-enable it was removed in EL-10. A vm with that backend skips the key and installs the repo
            # and its packages with gpg checks disabled, which is acceptable for a throwaway test container.
            if vm.pg_repo_key:
                result += (
                    "    rpm --import https://download.postgresql.org/pub/repos/yum/keys/RPM-GPG-KEY-PGDG && \\\n"
                    + "    rpm -ivh \\\n"
                    + "        %s && \\\n" % package
                )
            else:
                result += (
                    "    dnf -y install --nogpgcheck \\\n"
                    + "        %s && \\\n" % package
                    + "    sed -i 's/gpgcheck=1/gpgcheck=0/g' /etc/yum.repos.d/pgdg-%s-all.repo && \\\n" % distro
                )

            # Where the distribution still has dnf modularity its postgresql module shadows the PGDG packages, so disable it
            if vm.dnf_module:
                result += "    dnf -qy module disable postgresql && \\\n"

            result += "    yum -y install libcurl-devel && \\\n"

        result += "    yum -y install postgresql-devel"
    elif vm.os_base == VM_OS_BASE_DEBIAN:
        # Install repo from apt.postgresql.org, adding the beta repo when an unreleased version is installed. The beta repo is not
        # built for every architecture, so it is only added on the ones it is built for.
        if vm.pg_repo:
            beta = vm.pg_beta is not None and arch in (VM_ARCH_AARCH64, VM_ARCH_X86_64)

            result += (
                "    apt-get install -y --no-install-recommends postgresql-common && \\\n"
                + "    /usr/share/postgresql-common/pgdg/apt.postgresql.org.sh -y"
                + (" -c %s" % vm.pg_beta if beta else "")
                + " && \\\n"
            )

        result += (
            "    apt-get install -y --no-install-recommends postgresql-common libpq-dev && \\\n"
            + "    sed -i 's/^\\#create\\_main\\_cluster.*$/create\\_main\\_cluster \\= false/' "
            + "/etc/postgresql-common/createcluster.conf"
        )

    if vm.db_list and arch in (VM_ARCH_AARCH64, VM_ARCH_X86_64, VM_ARCH_I386):
        result += _SECTION + "# Install PostgreSQL\n"

        if vm.os_base == VM_OS_BASE_RHEL:
            result += "    yum -y install"
        elif vm.os_base == VM_OS_BASE_DEBIAN:
            result += "    apt-get install -y --no-install-recommends"
        else:
            result += "    apk add --no-cache"

        # Construct list of databases to install
        for db_version in vm.db_list:
            if vm.os_base == VM_OS_BASE_RHEL:
                # Native RHEL installs the unversioned server package from the application stream
                if not vm.pg_repo:
                    result += " postgresql-server"
                else:
                    db_version_no_dot = db_version.replace(".", "")

                    result += " postgresql%s-server" % db_version_no_dot

                    # Add development package for the latest version of postgres
                    if db_version == vm.db_list[-1]:
                        result += " postgresql%s-devel" % db_version_no_dot
            elif vm.os_base == VM_OS_BASE_DEBIAN:
                result += " postgresql-%s" % db_version
            else:
                result += " postgresql%s" % db_version

    # On other architectures (e.g. ppc64le, s390x) install a single PostgreSQL version for the smoke test via the postgresql
    # metapackage (its default from the configured repo). The full version matrix is skipped to keep the emulated build fast; one
    # version is enough to check the build and exercise checksums end-to-end on big-endian.
    elif vm.db_list and vm.os_base == VM_OS_BASE_DEBIAN:
        result += (
            _SECTION
            + "# Install a single PostgreSQL version for the smoke test\n"
            + "    apt-get install -y --no-install-recommends postgresql"
        )

    # Add path to latest version of postgres (PGDG installs to a versioned path; native packages use /usr/bin)
    if vm.os_base == VM_OS_BASE_RHEL and vm.pg_repo:
        result += (
            "\n\nENV PATH=/usr/pgsql-%s/bin:$PATH\n" % vm.db_list[-1]
            + "ENV PKG_CONFIG_PATH=/usr/pgsql-%s/lib/pkgconfig:$PKG_CONFIG_PATH\n" % vm.db_list[-1]
        )

    return result


####################################################################################################################################
def script_base(name, arch):
    """Generate the script for the base image, which holds everything installed from a package."""

    vm = vm_get(name)
    result = _script_package(vm)

    result += (
        _SECTION
        + "# Regenerate SSH keys\n"
        + "    rm -f /etc/ssh/ssh_host_rsa_key* && \\\n"
        + "    ssh-keygen -t rsa -b 2048 -f /etc/ssh/ssh_host_rsa_key"
    )

    if vm.os_base == VM_OS_BASE_DEBIAN:
        result += _SECTION + "# Fix root tty\n" + "    sed -i 's/^mesg n/tty -s \\&\\& mesg n/g' /root/.profile"
        result += _SECTION + "# Suppress dpkg interactive output\n" + "    rm /etc/apt/apt.conf.d/70debconf"

    result += _script_pg(vm, arch)

    if vm.os_base == VM_OS_BASE_DEBIAN:
        result += (
            _SECTION
            + "# Cleanup\n"
            + "    apt-get autoremove -y && \\\n"
            + "    apt-get clean && \\\n"
            + "    rm -rf /var/lib/apt/lists/*"
        )

    return result


####################################################################################################################################
def script_test(name, path_repo):
    """Generate the script for the test image, which adds the test user and everything it needs."""

    vm = vm_get(name)
    result = ""

    if vm.os_base == VM_OS_BASE_RHEL and host_arch() == VM_ARCH_AARCH64:
        result += _SECTION + "# Remove unneeded language setup\n" + "    rm /etc/profile.d/lang.sh"

    result += _ca_setup(vm.os_base, path_repo, "test/certificate/pgbackrest-test-ca.crt")

    result += (
        _SECTION
        + "# Create banner to make sure pgBackRest ignores it\n"
        + "    echo '***********************************************' >  /etc/issue.net && \\\n"
        + "    echo 'Sample banner to make sure banners are skipped.' >> /etc/issue.net && \\\n"
        + "    echo ''                                                >> /etc/issue.net && \\\n"
        + "    echo 'More banner after a blank line.'                 >> /etc/issue.net && \\\n"
        + "    echo '***********************************************' >> /etc/issue.net && \\\n"
        + "    echo 'Banner /etc/issue.net'                           >> /etc/ssh/sshd_config"
    )

    if vm.ssh_rsa:
        result += (
            _SECTION
            + "    echo '# Add PubkeyAcceptedAlgorithms (required for SFTP)'              >> /etc/ssh/sshd_config && \\\n"
            + "    echo 'HostKeyAlgorithms=+ssh-rsa,ssh-rsa-cert-v01@openssh.com'        >> /etc/ssh/sshd_config && \\\n"
            + "    echo 'PubkeyAcceptedAlgorithms=+ssh-rsa,ssh-rsa-cert-v01@openssh.com' >> /etc/ssh/sshd_config"
        )

    # Rename existing group that would conflict with our group name. This is pretty hacky but should be OK since we are the only
    # thing running in the container.
    result += (
        _SECTION
        + "# Rename conflicting group\n"
        + "    sed -i 's/^[^:]*:x:%u:.*/%s:x:%u:/' /etc/group" % (group_id(), group_name(), group_id())
    )

    # Remove an existing user that would conflict with our user id, e.g. Ubuntu 24.04 ships one at 1000. What matters is the id
    # rather than the name, so the account holding it goes. Hacky like the group rename above and safe for the same reason.
    result += _SECTION + "# Remove conflicting user\n" + "    sed -i '/^[^:]*:x:%u:/d' /etc/passwd" % user_id()

    result += (
        _SECTION
        + "# Create test user\n"
        + "    %s && \\\n" % _group_create(vm.os_base, group_name(), group_id())
        + "    %s && \\\n" % _user_create(vm.os_base, user_name(), user_id(), group_name())
        + "    mkdir -m 750 /home/%s/test && \\\n" % user_name()
        + "    chown %s:%s /home/%s/test" % (user_name(), group_name(), user_name())
    )

    # On Alpine the test account must have a password for SSH logon
    if vm.os_base == VM_OS_BASE_ALPINE:
        result += " && \\\n" + "    passwd -d '%s' %s" % (user_name(), user_name())

    result += _SECTION + "# Configure sudo\n"

    if vm.os_base == VM_OS_BASE_RHEL:
        result += (
            # Don't allow sudo to disable core dump (suppresses errors, see https://github.com/sudo-project/sudo/issues/42)
            '    echo "Set disable_coredump false" >> /etc/sudo.conf && \\\n'
            + "    echo '%%%s        ALL=(ALL)       NOPASSWD: ALL' > /etc/sudoers.d/%s && \\\n" % (group_name(), group_name())
            + "    sed -i 's/^Defaults    requiretty$/\\# Defaults    requiretty/' /etc/sudoers"
        )
    else:
        result += "    echo '%%%s ALL=(ALL) NOPASSWD: ALL' >> /etc/sudoers" % group_name()

    result += _ssh_setup(user_name(), group_name())

    result += _SECTION + "# Make %s home dir readable\n" % user_name() + "    chmod g+r,g+x /home/%s" % user_name()

    result += _entry_point_setup(vm.os_base)

    return result


####################################################################################################################################
def container_script(title, image_parent, script):
    """Wrap a script in the Dockerfile that builds it."""

    return "# %s Container\n" % title + "FROM %s" % image_parent + ("\n\nRUN echo 'OPTIMIZED BUILD'" + script if script else "")


####################################################################################################################################
def _revision_get(revision, name, arch):
    """Revision for a container.

    A container uses the revision for its vm and architecture if it has one, else the revision for its vm, else the required "all"
    revision. So bumping "all" rebuilds only the containers without a more specific revision."""

    return revision.get("%s-%s" % (name, arch), revision.get(name, revision[VM_ALL]))


####################################################################################################################################
def revision_check(revision):
    """Check the revisions declared in container.yaml, returning them keyed as they were declared.

    A revision is keyed by "all", a vm, or a vm qualified with an architecture, e.g. "u22-x86_64". Without this an invalid key
    would silently fall back to a less specific revision and the expected rebuild would never happen."""

    result = yaml_map_dict(revision, "the 'revision' section in %s" % VM_PATH_CONTAINER)

    # The "all" revision is required so it can never be accidentally unset, which would change every image hash at once. It can
    # only be bumped.
    check(VM_ALL in result, "the 'all' revision is required in %s" % VM_PATH_CONTAINER)

    for key in sorted(result):
        check(result[key] != "", "revision '%s' in %s must be set to a value" % (key, VM_PATH_CONTAINER))

        if key == VM_ALL:
            continue

        key_vm, _, key_arch = key.partition("-")

        check(
            vm_valid(key_vm) and key_vm != VM_NONE,
            "revision '%s' in %s has invalid vm '%s'" % (key, VM_PATH_CONTAINER, key_vm),
        )
        check(
            key_arch == "" or key_arch in VM_ARCH_LIST,
            "revision '%s' in %s has invalid architecture '%s'" % (key, VM_PATH_CONTAINER, key_arch),
        )

    return result


####################################################################################################################################
def _container_write(path_repo, name, arch, title, image_parent, image, script, revision_map, cache, show_output):
    """Write the Dockerfile for an image and build it, using the cache when the image is already there."""

    path_temp = os.path.join(path_repo, "test/result/docker")
    tag = "%s:%s" % (container_repo(), image)
    script = container_script(title, image_parent, script)

    # Only a base image is cached since a test image is cheap to build and depends on the user that built it
    cached = False
    cache_tag = None
    fork_cache_tag = None

    if "-base-" in image:
        revision = _revision_get(revision_map, name, arch)
        cache_image = "%s-%s-%s" % (image, revision, hashlib.sha1((script + revision).encode()).hexdigest()[:12])
        cache_tag = "%s:%s" % (container_repo(), cache_image)

        # On a fork also use the fork owner's own cache, which it can write to
        owner = os.environ.get("GITHUB_REPOSITORY_OWNER", "").lower() or _REPO_OWNER
        fork_cache_tag = "%s:%s" % (container_repo(owner), cache_image) if owner != _REPO_OWNER else None

        # Pull from the pgbackrest cache first, then the fork's own cache. A failed pull (image missing or no read access) falls
        # through to the next cache or a local build.
        if cache:
            for pull_tag in [cache_tag] + ([] if fork_cache_tag is None else [fork_cache_tag]):
                log(INFO, "Checking cache %s ..." % pull_tag)

                if exec_status("docker pull %s" % pull_tag) == 0:
                    log(INFO, "Using cached %s image" % pull_tag)
                    exec_one("docker tag %s %s" % (pull_tag, tag))
                    cached = True

                    break

    if cached:
        return

    log(INFO, "Building %s image%s ..." % (tag, "" if cache_tag is None else " (%s)" % cache_tag))

    # Write and build the image
    file_write(os.path.join(path_temp, image), script.strip() + "\n")
    exec_one(
        "docker build --platform linux/%s%s -f %s -t %s %s"
        % (arch, "" if cache else " --no-cache", os.path.join(path_temp, image), tag, os.path.join(path_repo, "test")),
        show_output=show_output,
    )

    # Push the base image to the cache this environment can write to (the fork's own cache on a fork, else the pgbackrest cache)
    # and log when it succeeds. Best effort since it requires write access, which is not available locally or on pull requests from
    # forks; those just use the image they built.
    if cache_tag is not None:
        push_tag = cache_tag if fork_cache_tag is None else fork_cache_tag
        exec_status("docker tag %s %s" % (tag, push_tag))

        if exec_status("docker push %s" % push_tag) == 0:
            log(INFO, "Cached %s image" % push_tag)


####################################################################################################################################
def container_remove(expression):
    """Remove the containers whose name matches an expression."""

    regexp = re.compile(expression)

    for container in sorted(exec_one('docker ps -a --format "{{.Names}}"').strip().split("\n")):
        if regexp.search(container):
            exec_status("docker rm -f %s" % container)


####################################################################################################################################
def cmd_vm_build(config):
    """Build the containers for one vm or for all of them."""

    path_repo = config.repo_path
    path_temp = os.path.join(path_repo, "test/result/docker")
    arch = config.vm_arch if config.vm_arch is not None else host_arch()
    show_output = config.log_level >= DETAIL

    # There is no container to build when the tests run on the host. Check the vm now so a typo is reported before anything is
    # written or built.
    check(config.vm != VM_NONE, "select a vm to build, or all of them")

    if config.vm != VM_ALL:
        vm_get(config.vm)

    path_create(path_temp, mode=0o770)

    # Revisions come from the repository being built rather than from the definitions, which are read from the repository this
    # module is part of
    path_container = os.path.join(path_repo, VM_PATH_CONTAINER)
    container = yaml_map_dict(yaml_load(file_read(path_container), VM_PATH_CONTAINER), VM_PATH_CONTAINER)

    check("revision" in container, "the 'revision' section is required in %s" % VM_PATH_CONTAINER)

    revision_map = revision_check(container["revision"])

    for name in VM_LIST if config.vm == VM_ALL else [config.vm]:
        # Base image
        image_base = "%s-base-%s" % (name, arch)

        _container_write(
            path_repo,
            name,
            arch,
            "Base",
            vm_get(name).image,
            image_base,
            script_base(name, arch),
            revision_map,
            config.cache,
            show_output,
        )

        # Test image
        _container_write(
            path_repo,
            name,
            arch,
            "Test",
            "%s:%s" % (container_repo(), image_base),
            "%s-test-%s" % (name, arch),
            script_test(name, path_repo),
            revision_map,
            config.cache,
            show_output,
        )

    log(INFO, "Build Complete")
