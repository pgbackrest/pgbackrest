####################################################################################################################################
# ContainerTest.pm - Build containers for testing and documentation
####################################################################################################################################
package pgBackRestTest::Common::ContainerTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess longmess);
use English '-no_match_vars';

use Cwd qw(abs_path);
use Digest::SHA qw(sha1_hex);
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);
use Getopt::Long qw(GetOptions);

use pgBackRestDoc::Common::Log;
use pgBackRestDoc::Common::String;
use pgBackRestDoc::ProjectInfo;

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::VmTest;

####################################################################################################################################
# User/group definitions
####################################################################################################################################
use constant TEST_USER                                              => getpwuid($UID) . '';
    push @EXPORT, qw(TEST_USER);
use constant TEST_USER_ID                                           => $UID;
    push @EXPORT, qw(TEST_USER_ID);
use constant TEST_GROUP                                             => getgrgid((getpwnam(TEST_USER))[3]) . '';
    push @EXPORT, qw(TEST_GROUP);
use constant TEST_GROUP_ID                                          => getgrnam(TEST_GROUP) . '';
    push @EXPORT, qw(TEST_GROUP_ID);

####################################################################################################################################
# Cert file constants
####################################################################################################################################
use constant CERT_FAKE_PATH                                         => '/etc/fake-cert';
use constant CERT_FAKE_CA                                           => CERT_FAKE_PATH . '/ca.crt';
    push @EXPORT, qw(CERT_FAKE_CA);
use constant CERT_FAKE_SERVER                                       => CERT_FAKE_PATH . '/server.crt';
    push @EXPORT, qw(CERT_FAKE_SERVER);
use constant CERT_FAKE_SERVER_KEY                                   => CERT_FAKE_PATH . '/server.key';
    push @EXPORT, qw(CERT_FAKE_SERVER_KEY);

####################################################################################################################################
# Container Debug - speeds container debugging by splitting each section into a separate intermediate container
####################################################################################################################################
use constant CONTAINER_DEBUG                                        => false;

####################################################################################################################################
# Store cache container checksums
####################################################################################################################################
my $hContainerCache;

####################################################################################################################################
# Container repo - defines the Docker repository where the containers will be located
####################################################################################################################################
sub containerRepo
{
    return PROJECT_EXE . qw(/) . 'test';
}

push @EXPORT, qw(containerRepo);

####################################################################################################################################
# Section header
####################################################################################################################################
sub sectionHeader
{
    return (CONTAINER_DEBUG ? "\n\nRUN \\\n" : " && \\\n\n");
}

####################################################################################################################################
# Write the Docker container
####################################################################################################################################
sub containerWrite
{
    my $oStorageDocker = shift;
    my $strTempPath = shift;
    my $strOS = shift;
    my $strArch = shift;
    my $strTitle = shift;
    my $strImageParent = shift;
    my $strImage = shift;
    my $strCopy = shift;
    my $strScript = shift;
    my $bForce = shift;

    my $strTag = containerRepo() . ":${strImage}";

    $strScript =
        "# ${strTitle} Container\n" .
        "FROM ${strImageParent}" .
        (defined($strCopy) ? "\n\n${strCopy}" : '') .
        (defined($strScript) && $strScript ne ''?
            "\n\nRUN echo '" . (CONTAINER_DEBUG ? 'DEBUG' : 'OPTIMIZED') . " BUILD'" . $strScript : '');

    # Search for the image in the cache
    my $strScriptSha1;
    my $bCached = false;

    if ($strImage =~ /\-base\-/)
    {
        $strScriptSha1 = sha1_hex($strScript);

        foreach my $strBuild (reverse(keys(%{$hContainerCache})))
        {
            my $strArchLookup = defined($strArch) ? $strArch : hostArch();

            if (defined($hContainerCache->{$strBuild}{$strArchLookup}{$strOS}) &&
                $hContainerCache->{$strBuild}{$strArchLookup}{$strOS} eq $strScriptSha1)
            {
                &log(INFO, "Using cached ${strTag}-${strBuild} image (${strScriptSha1}) ...");

                $strScript =
                    "# ${strTitle} Container\n" .
                    "FROM ${strTag}-${strBuild}";

                $bCached = true;
            }
        }
    }

    if (!$bCached)
    {
        &log(INFO, "Building ${strTag} image" . (defined($strScriptSha1) ? " (${strScriptSha1})" : '') . ' ...');
    }

    # Write the image
    $oStorageDocker->put("${strTempPath}/${strImage}", trim($strScript) . "\n");
    executeTest(
        'docker build' . (defined($strArch) ? " --platform linux/${strArch}" : '') .
        (defined($bForce) && $bForce ? ' --no-cache' : '') . " -f ${strTempPath}/${strImage} -t ${strTag} " .
            $oStorageDocker->pathGet('test'),
        {bSuppressStdErr => true, bShowOutputAsync => (logLevel())[1] eq DETAIL});
}

####################################################################################################################################
# User/group creation
####################################################################################################################################
sub groupCreate
{
    my $strOS = shift;
    my $strName = shift;
    my $iId = shift;

    return "groupadd -f -g${iId} ${strName}";
}

sub userCreate
{
    my $strOS = shift;
    my $strName = shift;
    my $iId = shift;
    my $strGroup = shift;

    my $oVm = vmGet();

    if ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_RHEL)
    {
        return "adduser -g${strGroup} -u${iId} -N ${strName}";
    }
    elsif ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_DEBIAN)
    {
        return "adduser --uid=${iId} --ingroup=${strGroup} --disabled-password --gecos \"\" ${strName}";
    }

    confess &log(ERROR, "unable to create user for os '${strOS}'");
}

####################################################################################################################################
# Setup SSH
####################################################################################################################################
sub sshSetup
{
    my $strOS = shift;
    my $strUser = shift;
    my $strGroup = shift;
    my $bControlMtr = shift;

    my $strUserPath = $strUser eq 'root' ? "/${strUser}" : "/home/${strUser}";

    my $strScript = sectionHeader() .
        "# Setup SSH\n" .
        "    mkdir ${strUserPath}/.ssh && \\\n" .
        "    echo '-----BEGIN RSA PRIVATE KEY-----' > ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'MIIEowIBAAKCAQEA5VySpEan5Rn7QQ5XP13YkXxTg+Xp9N+ecyDhD92OQk0VZOFn' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'EKlXUaMXH0WVjRRgjgbF78JHFbEQjUHimbzr9ev1IuDaIS42E1nUkBaHggnHNAW2' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'aqtczUDSnLGcXqd0XeDtwpjj1I0xWOeJli/xjU+eSwjzqNgX6ZX2P0uTXflu254u' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'SjOF3IARo9FiDEKQdksoxNOAhCTYgDAynmkcfJ9e87FjiTGmfnZO3H6gU2kidSFX' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'rwdHtdN3Qlv6RgCroLGAZZpmtqBorTwShWRScWAAg/OqWwLphfNbXIIHZO4EJ4S2' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'JLgsgJHb20SJ2XEI2Iln9sj+oCWgLJ2m+RLvWwIDAQABAoIBAArBC0EiQkyxf1Xe' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'txmKAWsE4iI85/oqzVJG7YvhuVdY0j16J2vLvNk05T3P9JdPqB1QqlGNEZSDSlbi' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'isjm55tkFl4tyRx61F9A5tYLWwwuVYWWFPutuuVcJOPi8gWAItUkruaLu6GjgyJQ' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo '143QA//lBp4sYRxUEX71defO19iKkDz+xEuOzYMd16j76OKMcmbnog7hbMrXR4Yi' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'kNXuhnwBadutaXLve3mZ0JowrZyHKfTUWOHgvuULpCVD5su3NdbpE2AVn1idcF8V' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'jaj6p0vtcvEnXEC69XwX+yL0TgvOE4Vu/OWg8lDWQdetONIHGbElJZvB6eyTF1nl' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'IIgLc2ECgYEA7PL3soOfH3dMNUt4KGSw1cK/kwvy7UsT6QrAPi1Cc/A4szluk6/O' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo '5YhKfTjzHW5WDmsTTAcT29MLmW8dQXeUAe/1BtIATubsav+uSelfBmUAnQj9fvKT' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'ieJ6JMf20OTbS6XODA3+jJAdApLCu61Lv6nePOuNzLY/uqSMWu9kO/sCgYEA981v' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'YIUaadFaHnPnmax0+jJMs8S5AIEjSfSIxR8oNOWNUxBBvwFd4zWTApVfZqKjmI83' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'Ng5tISxspzHseakyrIpoDqzxRQPxZF7RTO6VUX+ZQj6iIXVp9FDqWAjvDACuSky2' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'mGAiiA+fWZ1za62opgoYQZ17O17SyHF9/vJ7XCECgYANwNyXxAQMc4Q847CJx65r' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo '+e3cvyjOlTkGodUexsnAqQThgkfk0qOTtyF7uz6BStI77AMmupJwhAN8WHK+Rg6V' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'PjRevPm/mq/GVijrqVwWpu4uL0NnhvUBX9/vGpw868u+zFT1ZiqMRiEo8RPUiO6I' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'pXd82b9VTo7Mapiq/pI22QKBgC8Kb586BUabOGlZhVi11Ur9q3Pg32HKIgHTCveo' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'r4BDJ23iQyjYQJN2Qx8VbhPUwgue/FMlr+/BOCsRHhwGU5lPeOt4RyDb28I7Aa6C' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'CBR9jYF21F5XpLJ9fc8SexajNnLiVzNb5JJBrPVdH2EMiVxjxDEIjTE7EfZ9HPb9' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo '3w8hAoGBALyik3jr0W6DxzqOW0jPXPLDqp1JvzieC03nZD1scWeI8qIzUOpLX7cc' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'jaMU/8QMBRvyEcZK82Cedilm30nLf+C/FR5TsUmftS7IcjoC4Z2ZXWNOhMv22TUJ' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'Ml6z//+WSZ3qVZ5rvAeo4obwiBfe+Uh+AyyprEGgmimF9qDejcwc' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo '-----END RSA PRIVATE KEY-----' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQDlXJKkRqflGftBDlc/XdiRfFOD5en0355zIOEP3Y5CTRVk4WcQqVdRoxcfRZWNFGCOBsXvwk" .
             "cVsRCNQeKZvOv16/Ui4NohLjYTWdSQFoeCCcc0BbZqq1zNQNKcsZxep3Rd4O3CmOPUjTFY54mWL/GNT55LCPOo2BfplfY/S5Nd+W7bni5KM4XcgBGj0" .
             "WIMQpB2SyjE04CEJNiAMDKeaRx8n17zsWOJMaZ+dk7cfqBTaSJ1IVevB0e103dCW/pGAKugsYBlmma2oGitPBKFZFJxYACD86pbAumF81tcggdk7gQn" .
             "hLYkuCyAkdvbRInZcQjYiWf2yP6gJaAsnab5Eu9b user\@pgbackrest-test' > ${strUserPath}/.ssh/authorized_keys && \\\n" .
        "    echo 'Host *' > ${strUserPath}/.ssh/config && \\\n" .
        "    echo '    StrictHostKeyChecking no' >> ${strUserPath}/.ssh/config && \\\n";

    if ($bControlMtr)
    {
        $strScript .=
            "    echo '    ControlMas'.'ter auto' >> ${strUserPath}/.ssh/config && \\\n" .
            "    echo '    ControlPath /tmp/\%r\@\%h:\%p' >> ${strUserPath}/.ssh/config && \\\n" .
            "    echo '    ControlPersist 30' >> ${strUserPath}/.ssh/config && \\\n";
    }

    $strScript .=
        "    cp ${strUserPath}/.ssh/authorized_keys ${strUserPath}/.ssh/id_rsa.pub && \\\n" .
        "    chown -R ${strUser}:${strGroup} ${strUserPath}/.ssh && \\\n" .
        "    chmod 700 ${strUserPath}/.ssh && \\\n" .
        "    chmod 600 ${strUserPath}/.ssh/*";

    return $strScript;
}

####################################################################################################################################
# Copy text file into container. Note that this will not work if the file contains single quotes.
####################################################################################################################################
sub fileCopy
{
    my $oStorage = shift;
    my $strSourceFile = shift;
    my $strDestFile = shift;

    my $strScript;

    foreach my $strLine (split("\n", ${$oStorage->get($strSourceFile)}))
    {
        $strScript .= "    echo '${strLine}' " . (defined($strScript) ? '>>' : '>') . " ${strDestFile} && \\\n";
    }

    return $strScript;
}

####################################################################################################################################
# CA Setup
####################################################################################################################################
sub caSetup
{
    my $strOS = shift;
    my $oStorage = shift;
    my $strCaFile = shift;

    my $strOsBase = vmGet()->{$strOS}{&VM_OS_BASE};

    # Determine CA location
    my $strCertFile = undef;

    if ($strOsBase eq VM_OS_BASE_RHEL)
    {
        $strCertFile = '/etc/pki/ca-trust/source/anchors';
    }
    elsif ($strOsBase eq VM_OS_BASE_DEBIAN)
    {
        $strCertFile = '/usr/local/share/ca-certificates';
    }
    else
    {
        confess &log(ERROR, "unable to install CA for ${strOsBase}");
    }

    $strCertFile .= '/pgbackrest-test-ca.crt';

    # Write CA
    my $strScript =
        sectionHeader() .
        "# Install CA\n" .
        fileCopy($oStorage, $strCaFile, $strCertFile) .
        "    chmod 644 ${strCertFile} && \\\n";

    # Install CA
    if ($strOsBase eq VM_OS_BASE_RHEL)
    {
        $strScript .=
            "    update-ca-trust extract";
    }
    elsif ($strOsBase eq VM_OS_BASE_DEBIAN)
    {
        $strScript .=
            "    update-ca-certificates";
    }

    return $strScript;
}

####################################################################################################################################
# Entry point setup
####################################################################################################################################
sub entryPointSetup
{
    my $strOS = shift;

    my $strScript =
        "\n\n# Start SSH when container starts\n" .
        'ENTRYPOINT ';

    my $oVm = vmGet();

    if ($oVm->{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_RHEL)
    {
        $strScript .= 'rm -rf /run/nologin && /usr/sbin/sshd -D';
    }
    else
    {
        $strScript .= 'service ssh restart && bash';
    }

    return $strScript;
}

####################################################################################################################################
# Build containers
####################################################################################################################################
sub containerBuild
{
    my $oStorageDocker = shift;
    my $strVm = shift;
    my $strArch = shift;
    my $bVmForce = shift;

    # Create temp path
    my $strTempPath = $oStorageDocker->pathGet('test/result/docker');
    $oStorageDocker->pathCreate($strTempPath, {strMode => '0770', bIgnoreExists => true, bCreateParent => true});

    # Load container definitions from yaml
    require YAML::XS;
    YAML::XS->import(qw(Load));

    $hContainerCache = Load(${$oStorageDocker->get($oStorageDocker->pathGet('test/container.yaml'))});

    # Remove old images on force
    if ($bVmForce)
    {
        my $strRegExp = '^' . containerRepo();

        if ($strVm ne 'all')
        {
            $strRegExp .= "\:${strVm}-";
        }

        executeTest("rm -f ${strTempPath}/" . ($strVm eq 'all' ? '*' : "${strVm}-*"));
        imageRemove("${strRegExp}.*");
    }

    # VM Images
    ################################################################################################################################
    my $oVm = vmGet();

    foreach my $strOS ($strVm eq 'all' ? VM_LIST : ($strVm))
    {
        my $oOS = $oVm->{$strOS};
        my $bDocBuild = false;

        # Deprecated OSs can only be used to build packages
        my $bDeprecated = $oVm->{$strOS}{&VM_DEPRECATED} ? true : false;

        # Base image
        ###########################################################################################################################
        my $strImageParent =
            (defined($strArch) ? "${strArch}/" : (vmArch($strOS) eq VM_ARCH_AMD64 ? '' : vmArch($strOS) . '/')) .
            "$$oVm{$strOS}{&VM_IMAGE}";
        my $strImage = "${strOS}-base" . (defined($strArch) ? "-${strArch}" : '-' . hostArch());
        my $strCopy = undef;

        #---------------------------------------------------------------------------------------------------------------------------
        my $strScript = sectionHeader() .
            "# Install packages\n";

        if ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_RHEL)
        {
            if ($strOS eq VM_RH8)
            {
                $strScript .=
                    "    dnf install -y dnf-plugins-core && \\\n" .
                    "    dnf config-manager --set-enabled powertools && \\\n" .
                    "    dnf -y install epel-release && \\\n" .
                    "    crb enable && \\\n";
            }

            $strScript .=
                "    yum -y update && \\\n" .
                "    yum -y install openssh-server openssh-clients wget sudo git \\\n" .
                "        perl perl-Digest-SHA perl-DBD-Pg perl-YAML-LibYAML openssl \\\n" .
                "        gcc make perl-ExtUtils-MakeMaker perl-Test-Simple openssl-devel perl-ExtUtils-Embed rpm-build \\\n" .
                "        libyaml-devel zlib-devel libxml2-devel lz4-devel lz4 bzip2-devel bzip2 perl-JSON-PP ccache meson \\\n" .
                "        libssh2-devel";
        }
        else
        {
            $strScript .=
                "    export DEBCONF_NONINTERACTIVE_SEEN=true DEBIAN_FRONTEND=noninteractive && \\\n" .
                "    apt-get update && \\\n" .
                "    apt-get install -y --no-install-recommends openssh-server wget sudo gcc make git \\\n" .
                "        libdbd-pg-perl libhtml-parser-perl libssl-dev libperl-dev python3-distutils \\\n" .
                "        libyaml-libyaml-perl tzdata devscripts lintian libxml-checker-perl txt2man debhelper \\\n" .
                "        libppi-html-perl libtemplate-perl libtest-differences-perl zlib1g-dev libxml2-dev pkg-config \\\n" .
                "        libbz2-dev bzip2 libyaml-dev libjson-pp-perl liblz4-dev liblz4-tool gnupg lsb-release ccache meson \\\n" .
                "        libssh2-1-dev";

            if ($strOS eq VM_U20 || $strOS eq VM_U22)
            {
                $strScript .= " valgrind";
            }
        }

        # Add zst command-line tool and development libs when available
        if (vmWithZst($strOS))
        {
            if ($oVm->{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_RHEL)
            {
                $strScript .= ' zstd libzstd-devel';
            }
            else
            {
                $strScript .= ' zstd libzstd-dev';
            }
        }

        #---------------------------------------------------------------------------------------------------------------------------
        $strScript .= sectionHeader() .
            "# Regenerate SSH keys\n" .
            "    rm -f /etc/ssh/ssh_host_rsa_key* && \\\n" .
            "    ssh-keygen -t rsa -b 2048 -f /etc/ssh/ssh_host_rsa_key";

        if ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_DEBIAN)
        {
            $strScript .= sectionHeader() .
                "# Fix root tty\n" .
                "    sed -i 's/^mesg n/tty -s \\&\\& mesg n/g' /root/.profile";

            $strScript .= sectionHeader() .
                "# Suppress dpkg interactive output\n" .
                "    rm /etc/apt/apt.conf.d/70debconf";
        }

        #---------------------------------------------------------------------------------------------------------------------------
        if (!$bDeprecated)
        {
            $strScript .= sectionHeader() .
                "# Install PostgreSQL packages\n";

            if ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_RHEL)
            {
                $strScript .=
                    "    rpm --import https://download.postgresql.org/pub/repos/yum/keys/RPM-GPG-KEY-PGDG && \\\n";

                if ($strOS eq VM_RH8)
                {
                    $strScript .=
                        "    rpm -ivh \\\n" .
                        "        https://download.postgresql.org/pub/repos/yum/reporpms/EL-8-" . hostArch() . "/" .
                            "pgdg-redhat-repo-latest.noarch.rpm && \\\n" .
                        "    dnf -qy module disable postgresql && \\\n";
                }
                elsif ($strOS eq VM_F41)
                {
                    $strScript .=
                        "    rpm -ivh \\\n" .
                        "        https://download.postgresql.org/pub/repos/yum/reporpms/F-41-" . hostArch() . "/" .
                            "pgdg-fedora-repo-latest.noarch.rpm && \\\n";
                }

                $strScript .= "    yum -y install postgresql-devel";
            }
            else
            {
                # Install repo from apt.postgresql.org
                if (vmPgRepo($strVm))
                {
                    $strScript .=
                        "    echo \"deb http://apt.postgresql.org/pub/repos/apt/ \$(lsb_release -s -c)-pgdg main" .
                            "\" >> /etc/apt/sources.list.d/pgdg.list && \\\n" .
                        "    wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | apt-key add - && \\\n" .
                        "    apt-get update && \\\n";
                }

                $strScript .=
                    "    apt-get install -y --no-install-recommends postgresql-common libpq-dev && \\\n" .
                    "    sed -i 's/^\\#create\\_main\\_cluster.*\$/create\\_main\\_cluster \\= false/' " .
                        "/etc/postgresql-common/createcluster.conf";
            }

            if (defined($oOS->{&VM_DB}) && @{$oOS->{&VM_DB}} > 0)
            {
                $strScript .= sectionHeader() .
                    "# Install PostgreSQL\n";

                if ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_RHEL)
                {
                    $strScript .= "    yum -y install";
                }
                else
                {
                    $strScript .= "    apt-get install -y --no-install-recommends";
                 }

                # Construct list of databases to install
                foreach my $strDbVersion (@{$oOS->{&VM_DB}})
                {
                    if ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_RHEL)
                    {
                        my $strDbVersionNoDot = $strDbVersion;
                        $strDbVersionNoDot =~ s/\.//;

                        $strScript .= " postgresql${strDbVersionNoDot}-server";

                        # Add development package for the latest version of postgres
                        if ($strDbVersion eq @{$oOS->{&VM_DB}}[-1])
                        {
                            $strScript .= " postgresql${strDbVersionNoDot}-devel";
                        }
                    }
                    else
                    {
                        $strScript .= " postgresql-${strDbVersion}";
                    }
                }
            }
        }

        # Add path to latest version of postgres
        if ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_RHEL)
        {
            $strScript .=
                "\n\nENV PATH=/usr/pgsql-" . @{$oOS->{&VM_DB}}[-1] . "/bin:\$PATH\n" .
                "ENV PKG_CONFIG_PATH=/usr/pgsql-" . @{$oOS->{&VM_DB}}[-1] . "/lib/pkgconfig:\$PKG_CONFIG_PATH\n";
        }

        #---------------------------------------------------------------------------------------------------------------------------
        if ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_DEBIAN)
        {
        $strScript .= sectionHeader() .
            "# Cleanup\n";

            $strScript .=
                "    apt-get autoremove -y && \\\n" .
                "    apt-get clean && \\\n" .
                "    rm -rf /var/lib/apt/lists/*";
        }

        containerWrite(
            $oStorageDocker, $strTempPath, $strOS, $strArch, 'Base', $strImageParent, $strImage, $strCopy, $strScript, $bVmForce);

        # Test image
        ########################################################################################################################
        if (!$bDeprecated)
        {
            $strImageParent = containerRepo() . ":${strImage}";
            $strImage = "${strOS}-test" . (defined($strArch) ? "-${strArch}" : '-' . hostArch());

            $strCopy = undef;
            $strScript = '';

            #-----------------------------------------------------------------------------------------------------------------------
            if ($oVm->{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_RHEL && hostArch() eq VM_ARCH_AARCH64)
            {
                $strScript .= sectionHeader() .
                    "# Remove unneeded language setup\n" .
                    "    rm /etc/profile.d/lang.sh";
            }

            #-----------------------------------------------------------------------------------------------------------------------
            $strScript .= caSetup($strOS, $oStorageDocker, "test/certificate/pgbackrest-test-ca.crt");

            #-----------------------------------------------------------------------------------------------------------------------
            $strScript .= sectionHeader() .
                "# Create banner to make sure pgBackRest ignores it\n" .
                "    echo '***********************************************' >  /etc/issue.net && \\\n" .
                "    echo 'Sample banner to make sure banners are skipped.' >> /etc/issue.net && \\\n" .
                "    echo ''                                                >> /etc/issue.net && \\\n" .
                "    echo 'More banner after a blank line.'                 >> /etc/issue.net && \\\n" .
                "    echo '***********************************************' >> /etc/issue.net && \\\n" .
                "    echo 'Banner /etc/issue.net'                           >> /etc/ssh/sshd_config";

            if ($strOS eq VM_U22)
            {
                $strScript .= sectionHeader() .
                    "    echo '# Add PubkeyAcceptedAlgorithms (required for SFTP)'              >> /etc/ssh/sshd_config && \\\n" .
                    "    echo 'HostKeyAlgorithms=+ssh-rsa,ssh-rsa-cert-v01\@openssh.com'        >> /etc/ssh/sshd_config && \\\n" .
                    "    echo 'PubkeyAcceptedAlgorithms=+ssh-rsa,ssh-rsa-cert-v01\@openssh.com' >> /etc/ssh/sshd_config";
            }

            # Rename existing group that would conflict with our group name. This is pretty hacky but should be OK since we are the
            # only thing running in the container.
            $strScript .= sectionHeader() .
                "# Rename conflicting group\n" .
                '    sed -i s/.*\:x\:' . TEST_GROUP_ID . '\:$/' . TEST_GROUP . '\:x\:' . TEST_GROUP_ID . "\:/ /etc/group";

            $strScript .= sectionHeader() .
                "# Create test user\n" .
                '    ' . groupCreate($strOS, TEST_GROUP, TEST_GROUP_ID) . " && \\\n" .
                '    ' . userCreate($strOS, TEST_USER, TEST_USER_ID, TEST_GROUP) . " && \\\n" .
                '    mkdir -m 750 /home/' . TEST_USER . "/test && \\\n" .
                '    chown ' . TEST_USER . ':' . TEST_GROUP . ' /home/' . TEST_USER . "/test";

            $strScript .= sectionHeader() .
                "# Configure sudo\n";

            if ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_RHEL)
            {
                $strScript .=
                    # Don't allow sudo to disable core dump (suppresses errors, see https://github.com/sudo-project/sudo/issues/42)
                    "    echo \"Set disable_coredump false\" >> /etc/sudo.conf && \\\n" .
                    "    echo '%" . TEST_GROUP . "        ALL=(ALL)       NOPASSWD: ALL' > /etc/sudoers.d/" . TEST_GROUP .
                        " && \\\n" .
                    "    sed -i 's/^Defaults    requiretty\$/\\# Defaults    requiretty/' /etc/sudoers";
            }
            else
            {
                $strScript .=
                    "    echo '%" . TEST_GROUP . " ALL=(ALL) NOPASSWD: ALL' >> /etc/sudoers";
            }

            $strScript .=
                sshSetup($strOS, TEST_USER, TEST_GROUP, $$oVm{$strOS}{&VM_CONTROL_MTR});

            $strScript .= sectionHeader() .
                "# Make " . TEST_USER . " home dir readable\n" .
                '    chmod g+r,g+x /home/' . TEST_USER;

            $strScript .= entryPointSetup($strOS);

            containerWrite(
                $oStorageDocker, $strTempPath, $strOS, $strArch, 'Test', $strImageParent, $strImage, $strCopy, $strScript,
                $bVmForce);
        }
    }

    &log(INFO, "Build Complete");
}

push @EXPORT, qw(containerBuild);

####################################################################################################################################
# containerRemove
#
# Remove containers that match a regexp.
####################################################################################################################################
sub containerRemove
{
    my $strRegExp = shift;

    foreach my $strContainer (sort(split("\n", trim(executeTest('docker ps -a --format "{{.Names}}"')))))
    {
        if ($strContainer =~ $strRegExp)
        {
            executeTest("docker rm -f ${strContainer}", {bSuppressError => true});
        }
    }
}

push @EXPORT, qw(containerRemove);

####################################################################################################################################
# imageRemove
#
# Remove images that match a regexp.
####################################################################################################################################
sub imageRemove
{
    my $strRegExp = shift;

    foreach my $strImage (sort(split("\n", trim(executeTest('docker images --format "{{.Repository}}:{{.Tag}}"')))))
    {
        if ($strImage =~ $strRegExp)
        {
            &log(INFO, "Removing ${strImage} image...");
            executeTest("docker rmi -f ${strImage}");
        }
    }
}

1;
