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

use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Version;

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::VmTest;

####################################################################################################################################
# User/group definitions
####################################################################################################################################
use constant POSTGRES_GROUP                                         => 'postgres';
    push @EXPORT, qw(POSTGRES_GROUP);
use constant POSTGRES_GROUP_ID                                      => 5000;
use constant POSTGRES_USER                                          => POSTGRES_GROUP;
use constant POSTGRES_USER_ID                                       => 5000;

use constant TEST_GROUP                                             => getgrgid($UID) . '';
    push @EXPORT, qw(TEST_GROUP);
use constant TEST_GROUP_ID                                          => getgrnam(TEST_GROUP) . '';
use constant TEST_USER                                              => getpwuid($UID) . '';
    push @EXPORT, qw(TEST_USER);
use constant TEST_USER_ID                                           => $UID;

use constant BACKREST_USER                                          => 'pgbackrest';
    push @EXPORT, qw(BACKREST_USER);
use constant BACKREST_USER_ID                                       => getpwnam(BACKREST_USER) . '';

####################################################################################################################################
# Package constants
####################################################################################################################################
use constant LIB_COVER_VERSION                                      => '1.29-2';
    push @EXPORT, qw(LIB_COVER_VERSION);
use constant LIB_COVER_EXE                                          => '/usr/bin/cover';
    push @EXPORT, qw(LIB_COVER_EXE);

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
# Container Debug - speeds container debugging by splitting each section into a separate intermediate container
####################################################################################################################################
use constant CONTAINER_S3_SERVER_TAG                                => 's3-server-20180612A';

####################################################################################################################################
# Store cache container checksums
####################################################################################################################################
my $hContainerCache;

####################################################################################################################################
# Generate Devel::Cover package name
####################################################################################################################################
sub packageDevelCover
{
    my $strArch = shift;

    return 'libdevel-cover-perl_' . LIB_COVER_VERSION . "_${strArch}.deb";
}

push @EXPORT, qw(packageDevelCover);

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

    if ($strImage =~ /\-base$/)
    {
        $strScriptSha1 = sha1_hex($strScript);

        foreach my $strBuild (reverse(keys(%{$hContainerCache})))
        {
            if (defined($hContainerCache->{$strBuild}{$strOS}) && $hContainerCache->{$strBuild}{$strOS} eq $strScriptSha1)
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
    executeTest('docker build' . (defined($bForce) && $bForce ? ' --no-cache' : '') .
                " -f ${strTempPath}/${strImage} -t ${strTag} ${strTempPath}",
                {bSuppressStdErr => true});
}

####################################################################################################################################
# User/group creation
####################################################################################################################################
sub groupCreate
{
    my $strOS = shift;
    my $strName = shift;
    my $iId = shift;

    return "groupadd -g${iId} ${strName}";
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
        return "adduser -g${strGroup} -u${iId} -n ${strName}";
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
    my $bControlMaster = shift;

    my $strUserPath = $strUser eq 'root' ? "/${strUser}" : "/home/${strUser}";

    my $strScript = sectionHeader() .
        "# Setup SSH\n" .
        "    mkdir ${strUserPath}/.ssh && \\\n" .
        "    echo '-----BEGIN RSA PRIVATE KEY-----' > ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'MIICXwIBAAKBgQDR0yJsZW5d5LcqteiOtv8d+FFeFFHDPI0VTcTOdMn1iDiIP1ou' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'X3Q2OyNjsBaDbsRJd+sp9IRq1LKX3zsBcgGZANwm0zduuNEPEU94ajS/uRoejIqY' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo '/XkKOpnEF6ZbQ2S7TaE4sWeGLvba7kUFs0QTOO+N+nV2dMbdqZf6C8lazwIDAQAB' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'AoGBAJXa6xzrnFVmwgK5BKzYuX/YF5TPgk2j80ch0ct50buQXH/Cb0/rUH5i4jWS' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'T6Hy/DFUehnuzpvV6O9auTOhDs3BhEKFRuRLn1nBwTtZny5Hh+cw7azUCEHFCJlz' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'makCrVbgawtno6oU/pFgQm1FcxD0f+Me5ruNcLHqUZsPQwkRAkEA+8pG+ckOlz6R' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'AJLIHedmfcrEY9T7sfdo83bzMOz8H5soUUP4aOTLJYCla1LO7JdDnXMGo0KxaHBP' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'l8j5zDmVewJBANVVPDJr1w37m0FBi37QgUOAijVfLXgyPMxYp2uc9ddjncif0063' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo '0Wc0FQefoPszf3CDrHv/RHvhHq97jXDwTb0CQQDgH83NygoS1r57pCw9chzpG/R0' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'aMEiSPhCvz757fj+qT3aGIal2AJ7/2c/gRZvwrWNETZ3XIZOUKqIkXzJLPjBAkEA' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'wnP799W2Y8d4/+VX2pMBkF7lG7sSviHEq1sP2BZtPBRQKSQNvw3scM7XcGh/mxmY' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'yx0qpqfKa8SKbNgI1+4iXQJBAOlg8MJLwkUtrG+p8wf69oCuZsnyv0K6UMDxm6/8' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'cbvfmvODulYFaIahaqHWEZoRo5CLYZ7gN43WHPOrKxdDL78=' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo '-----END RSA PRIVATE KEY-----' >> ${strUserPath}/.ssh/id_rsa && \\\n" .
        "    echo 'ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAAAgQDR0yJsZW5d5LcqteiOtv8d+FFeFFHDPI0VTcTOdMn1iDiIP1ouX3Q2OyNjsBaDbsRJd+sp9I" .
             "Rq1LKX3zsBcgGZANwm0zduuNEPEU94ajS/uRoejIqY/XkKOpnEF6ZbQ2S7TaE4sWeGLvba7kUFs0QTOO+N+nV2dMbdqZf6C8lazw== " .
             "user\@pgbackrest-test' > ${strUserPath}/.ssh/authorized_keys && \\\n" .
        "    echo 'Host *' > ${strUserPath}/.ssh/config && \\\n" .
        "    echo '    StrictHostKeyChecking no' >> ${strUserPath}/.ssh/config && \\\n";

    if ($bControlMaster)
    {
        $strScript .=
            "    echo '    ControlMaster auto' >> ${strUserPath}/.ssh/config && \\\n" .
            "    echo '    ControlPath /tmp/\%r\@\%h:\%p' >> ${strUserPath}/.ssh/config && \\\n" .
            "    echo '    ControlPersist 30' >> ${strUserPath}/.ssh/config && \\\n";
    }

    $strScript .=
        "    chown -R ${strUser}:${strGroup} ${strUserPath}/.ssh && \\\n" .
        "    chmod 700 ${strUserPath}/.ssh && \\\n" .
        "    chmod 600 ${strUserPath}/.ssh/*";

    return $strScript;
}

####################################################################################################################################
# Cert Setup
####################################################################################################################################
sub certSetup
{
    my $strOS = shift;

    my $strScript =
        sectionHeader() .
        "# Generate fake certs\n" .
        "    mkdir -p -m 755 /etc/fake-cert && \\\n" .
        "    cd /etc/fake-cert && \\\n" .
        "    openssl genrsa -out ca.key 2048 && \\\n" .
        "    openssl req -new -x509 -extensions v3_ca -key ca.key -out ca.crt -days 99999 \\\n" .
        "        -subj \"/C=US/ST=Country/L=City/O=Organization/CN=pgbackrest.org\" && \\\n" .
        "    openssl genrsa -out server.key 2048 && \\\n" .
        "    openssl req -new -key server.key -out server.csr \\\n" .
        "        -subj \"/C=US/ST=Country/L=City/O=Organization/CN=*.pgbackrest.org\" && \\\n" .
        "    openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -days 99999 \\\n" .
        "        -sha256 && \\\n" .
        "    chmod 644 /etc/fake-cert/* && \\\n";

    my $rhVm = vmGet();

    if ($rhVm->{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_RHEL)
    {
        if ($strOS eq VM_CO6)
        {
            $strScript .=
                "    update-ca-trust enable && \\\n";
        }

        $strScript .=
            "    cp /etc/fake-cert/pgbackrest-test-ca.crt /etc/pki/ca-trust/source/anchors && \\\n" .
            "    update-ca-trust extract";
    }
    elsif ($rhVm->{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_DEBIAN)
    {
        $strScript .=
            "    cp /etc/fake-cert/pgbackrest-test-ca.crt /usr/local/share/ca-certificates && \\\n" .
            "    update-ca-certificates";
    }
    else
    {
        confess &log(ERROR, "unable to install certificate for $rhVm->{$strOS}{&VM_OS_BASE}");
    }

    return $strScript;
}

####################################################################################################################################
# S3 server setup
####################################################################################################################################
sub s3ServerSetup
{
    my $strOS = shift;

    # Install node.js
    my $strScript = sectionHeader() .
        "# Install node.js\n";

    if ($strOS eq VM_CO7)
    {
        $strScript .=
            "    wget -O /root/nodejs.sh https://rpm.nodesource.com/setup_6.x && \\\n" .
            "    bash /root/nodejs.sh && \\\n" .
            "    yum install -y nodejs";
    }
    else
    {
        $strScript .=
            "    wget -O /root/nodejs.sh https://deb.nodesource.com/setup_6.x && \\\n" .
            "    bash /root/nodejs.sh && \\\n" .
            "    wget -qO- https://deb.nodesource.com/setup_8.x | bash - && \\\n" .
            "    apt-get install -y nodejs";
    }

    # Install Scality S3
    $strScript .= sectionHeader() .
        "# Install Scality S3\n";

    $strScript .=
        "    wget -O /root/scalitys3.tar.gz https://github.com/scality/S3/archive/GA6.4.2.1.tar.gz && \\\n" .
        "    mkdir /root/scalitys3 && \\\n" .
        "    tar -C /root/scalitys3 --strip-components 1 -xvf /root/scalitys3.tar.gz && \\\n" .
        "    cd /root/scalitys3 && \\\n" .
        "    npm install && \\\n" .
        '    sed -i "0,/,/s//,\n    \"certFilePaths\":{\"key\":\"\/etc\/fake\-cert\/server.key\",\"cert\":' .
            '\"\/etc\/fake\-cert\/server.crt\",\"ca\":\"\/etc\/fake\-cert\/ca.crt\"},/"' . " \\\n" .
        '        ./config.json' . " && \\\n" .
        '    sed -i "s/ort\"\: 8000/ort\"\: 443/" ./config.json';

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

    if ($oVm->{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_RHEL || $strOS eq VM_U12)
    {
        $strScript .= '/usr/sbin/sshd -D';
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
    my $bVmForce = shift;

    # Create temp path
    my $strTempPath = $oStorageDocker->pathGet('test/.vagrant/docker');
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
        my $strImageParent = "$$oVm{$strOS}{&VM_IMAGE}";
        my $strImage = "${strOS}-base";
        my $strCopy = undef;

        #---------------------------------------------------------------------------------------------------------------------------
        my $strScript = sectionHeader() .
            "# Install base packages\n";

        if ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_RHEL)
        {
            $strScript .=
                "    yum -y install epel-release && \\\n" .
                "    yum -y update && \\\n" .
                "    yum -y install openssh-server openssh-clients wget sudo python-pip build-essential valgrind git \\\n" .
                "        perl perl-Digest-SHA perl-DBD-Pg perl-XML-LibXML perl-IO-Socket-SSL perl-YAML-LibYAML \\\n" .
                "        gcc make perl-ExtUtils-MakeMaker perl-Test-Simple openssl-devel perl-ExtUtils-Embed rpm-build \\\n" .
                "        zlib-devel libxml2-devel";

            if ($strOS eq VM_CO6)
            {
                $strScript .= ' perl-Time-HiRes perl-parent perl-JSON';
            }
            else
            {
                $strScript .= ' perl-JSON-PP';
            }

            if (vmCoverageC($strOS))
            {
                $strScript .= ' lcov';
            }
        }
        else
        {
            $strScript .=
                "    export DEBCONF_NONINTERACTIVE_SEEN=true DEBIAN_FRONTEND=noninteractive && \\\n" .
                "    apt-get update && \\\n" .
                "    apt-get -y install wget python && \\\n" .
                "    wget --no-check-certificate -O /root/get-pip.py https://bootstrap.pypa.io/get-pip.py && \\\n" .
                "    python /root/get-pip.py && \\\n" .
                "    apt-get -y install openssh-server wget sudo python-pip build-essential valgrind git \\\n" .
                "        libdbd-pg-perl libhtml-parser-perl libio-socket-ssl-perl libxml-libxml-perl libssl-dev libperl-dev \\\n" .
                "        libyaml-libyaml-perl tzdata devscripts lintian libxml-checker-perl txt2man debhelper \\\n" .
                "        libppi-html-perl libtemplate-perl libtest-differences-perl zlib1g-dev libxml2-dev";

            if ($strOS eq VM_U12)
            {
                $strScript .= ' libperl5.14';
            }

            if (vmLintC($strOS))
            {
                $strScript .= ' clang-6.0 clang-tools-6.0';
            }

            if (vmCoverageC($strOS))
            {
                $strScript .= ' lcov';
            }
        }

        #---------------------------------------------------------------------------------------------------------------------------
        $strScript .= sectionHeader() .
            "# Regenerate SSH keys\n" .
            "    rm -f /etc/ssh/ssh_host_rsa_key* && \\\n" .
            "    ssh-keygen -t rsa -b 1024 -f /etc/ssh/ssh_host_rsa_key";

        if ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_DEBIAN)
        {
            $strScript .= sectionHeader() .
                "# Fix root tty\n" .
                "    sed -i 's/^mesg n/tty -s \\&\\& mesg n/g' /root/.profile";

            $strScript .= sectionHeader() .
                "# Suppress dpkg interactive output\n" .
                "    rm /etc/apt/apt.conf.d/70debconf";

            if ($strOS eq VM_U12)
            {
                $strScript .= sectionHeader() .
                    "# Create run directory required by SSH (not created automatically on Ubuntu 12.04)\n" .
                    "    mkdir -p /var/run/sshd";
            }
        }

        #---------------------------------------------------------------------------------------------------------------------------
        my $strCertPath = 'test/certificate';
        my $strCertName = 'pgbackrest-test';

        $strCopy = '# Copy Test Certificates';

        foreach my $strFile ('-ca.crt', '.crt', '.key')
        {
            $oStorageDocker->copy("${strCertPath}/${strCertName}${strFile}", "${strTempPath}/${strCertName}${strFile}");
            $strCopy .= "\nCOPY ${strCertName}${strFile} " . CERT_FAKE_PATH . "/${strCertName}${strFile}";
        }

        $strScript .= certSetup($strOS);

        #---------------------------------------------------------------------------------------------------------------------------
        if (!$bDeprecated)
        {
            $strScript .=  sectionHeader() .
                "# Create PostgreSQL user/group with known ids for testing\n" .
                '    ' . groupCreate($strOS, POSTGRES_GROUP, POSTGRES_GROUP_ID) . " && \\\n" .
                '    ' . userCreate($strOS, POSTGRES_USER, POSTGRES_USER_ID, POSTGRES_GROUP);

            $strScript .=  sectionHeader() .
                "# Install PostgreSQL packages\n";

            if ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_RHEL)
            {
                if ($strOS eq VM_CO6)
                {
                    $strScript .=
                        "    rpm -ivh \\\n" .
                        "        http://yum.postgresql.org/9.0/redhat/rhel-6-x86_64/pgdg-centos90-9.0-5.noarch.rpm \\\n" .
                        "        http://yum.postgresql.org/9.1/redhat/rhel-6-x86_64/pgdg-centos91-9.1-6.noarch.rpm \\\n" .
                        "        http://yum.postgresql.org/9.2/redhat/rhel-6-x86_64/pgdg-centos92-9.2-8.noarch.rpm \\\n" .
                        "        http://yum.postgresql.org/9.3/redhat/rhel-6-x86_64/pgdg-centos93-9.3-3.noarch.rpm \\\n" .
                        "        http://yum.postgresql.org/9.4/redhat/rhel-6-x86_64/pgdg-centos94-9.4-3.noarch.rpm \\\n" .
                        "        http://yum.postgresql.org/9.5/redhat/rhel-6-x86_64/pgdg-centos95-9.5-3.noarch.rpm \\\n" .
                        "        http://yum.postgresql.org/9.6/redhat/rhel-6-x86_64/pgdg-centos96-9.6-3.noarch.rpm \\\n" .
                        "        http://yum.postgresql.org/10/redhat/rhel-6-x86_64/pgdg-centos10-10-2.noarch.rpm";
                }
                elsif ($strOS eq VM_CO7)
                {
                    $strScript .=
                        "    rpm -ivh \\\n" .
                        "        http://yum.postgresql.org/9.2/redhat/rhel-7-x86_64/pgdg-centos92-9.2-3.noarch.rpm \\\n" .
                        "        http://yum.postgresql.org/9.3/redhat/rhel-7-x86_64/pgdg-centos93-9.3-3.noarch.rpm \\\n" .
                        "        http://yum.postgresql.org/9.4/redhat/rhel-7-x86_64/pgdg-centos94-9.4-3.noarch.rpm \\\n" .
                        "        http://yum.postgresql.org/9.5/redhat/rhel-7-x86_64/pgdg-centos95-9.5-3.noarch.rpm \\\n" .
                        "        http://yum.postgresql.org/9.6/redhat/rhel-7-x86_64/pgdg-centos96-9.6-3.noarch.rpm \\\n" .
                        "        http://yum.postgresql.org/10/redhat/rhel-7-x86_64/pgdg-centos10-10-2.noarch.rpm";
                }
            }
            else
            {
                $strScript .=
                    "    echo 'deb http://apt.postgresql.org/pub/repos/apt/ " .
                    $$oVm{$strOS}{&VM_OS_REPO} . '-pgdg main' .
                        "' >> /etc/apt/sources.list.d/pgdg.list && \\\n" .
                    "    wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | apt-key add - && \\\n" .
                    "    apt-get update && \\\n" .
                    "    apt-get install -y postgresql-common && \\\n" .
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
                    $strScript .= "    apt-get install -y";
                }

                # Construct list of databases to install
                foreach my $strDbVersion (@{$oOS->{&VM_DB}})
                {
                    if ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_RHEL)
                    {
                        my $strDbVersionNoDot = $strDbVersion;
                        $strDbVersionNoDot =~ s/\.//;

                        $strScript .=  " postgresql${strDbVersionNoDot}-server";
                    }
                    else
                    {
                        $strScript .= " postgresql-${strDbVersion}";
                    }
                }
            }
        }

        #---------------------------------------------------------------------------------------------------------------------------
        if (!$bDeprecated)
        {
            $strScript .= sectionHeader() .
                "# Install AWS CLI\n" .
                "    pip install --upgrade --no-cache-dir pip==9.0.3 && \\\n" .
                "    pip install --upgrade awscli";
        }

        #---------------------------------------------------------------------------------------------------------------------------
        if (!$bDeprecated && $strOS ne VM_CO6 && $strOS ne VM_U12)
        {
            $strScript .= s3ServerSetup($strOS);
        }

        containerWrite(
            $oStorageDocker, $strTempPath, $strOS, 'Base', $strImageParent, $strImage, $strCopy, $strScript, $bVmForce);

        # Build image
        ###########################################################################################################################
        $strImageParent = containerRepo() . ":${strOS}-base";
        $strImage = "${strOS}-build";
        $strCopy = undef;

        my $strPkgDevelCover = packageDevelCover($oVm->{$strOS}{&VM_ARCH});
        my $bPkgDevelCoverBuild = vmCoveragePerl($strOS) && !$oStorageDocker->exists("test/package/${strOS}-${strPkgDevelCover}");

        $strScript = sectionHeader() .
            "# Create test user\n" .
            '    ' . groupCreate($strOS, TEST_GROUP, TEST_GROUP_ID) . " && \\\n" .
            '    ' . userCreate($strOS, TEST_USER, TEST_USER_ID, TEST_GROUP);

        # Install Perl packages
        if ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_DEBIAN)
        {
            $strScript .=  sectionHeader() .
                "# Install pgBackRest package source\n" .
                "    git clone https://salsa.debian.org/postgresql/pgbackrest.git /root/package-src";

            # Build only when a new version has been specified
            if ($bPkgDevelCoverBuild)
            {
                $strScript .=  sectionHeader() .
                    "# Install Devel::Cover package source & build\n" .
                    "    git clone https://salsa.debian.org/perl-team/modules/packages/libdevel-cover-perl.git" .
                        " /root/libdevel-cover-perl && \\\n" .
                    "    cd /root/libdevel-cover-perl && \\\n" .
                    "    git checkout debian/" . LIB_COVER_VERSION . " && \\\n" .
                    "    debuild -i -us -uc -b";
            }
        }
        else
        {
            $strScript .=  sectionHeader() .
                "# Install pgBackRest package source\n" .
                "    mkdir /root/package-src && \\\n" .
                "    wget -O /root/package-src/pgbackrest-conf.patch " .
                    "'https://git.postgresql.org/gitweb/?p=pgrpms.git;a=blob_plain;" .
                    "f=rpm/redhat/master/pgbackrest/master/pgbackrest-conf.patch;hb=refs/heads/master' && \\\n" .
                "    wget -O /root/package-src/pgbackrest.spec " .
                    "'https://git.postgresql.org/gitweb/?p=pgrpms.git;a=blob_plain;" .
                    "f=rpm/redhat/master/pgbackrest/master/pgbackrest.spec;hb=refs/heads/master'";
        }

        $strScript .= entryPointSetup($strOS);

        containerWrite($oStorageDocker, $strTempPath, $strOS, 'Build', $strImageParent, $strImage, $strCopy, $strScript, $bVmForce);

        # Copy Devel::Cover to host so it can be installed in other containers (if it doesn't already exist)
        if ($bPkgDevelCoverBuild)
        {
            executeTest('docker rm -f test-build', {bSuppressError => true});
            executeTest(
                "docker run -itd -h test-build --name=test-build" .
                " -v ${strTempPath}:${strTempPath} " . containerRepo() . ":${strOS}-build",
                {bSuppressStdErr => true});
            executeTest(
                "docker exec -i test-build " .
                "bash -c 'cp /root/${strPkgDevelCover} ${strTempPath}/${strOS}-${strPkgDevelCover}'");
            executeTest('docker rm -f test-build');

            $oStorageDocker->move(
                "test/.vagrant/docker/${strOS}-${strPkgDevelCover}", "test/package/${strOS}-${strPkgDevelCover}");
        }

        # S3 image
        ###########################################################################################################################
        if (!$bDeprecated)
        {
            $strImage = "${strOS}-s3-server";
            $strScript = '';
            $strCopy = undef;

            if ($strOS ne VM_CO6 && $strOS ne VM_U12)
            {
                $strImageParent = containerRepo() . ":${strOS}-base";
                $strScript = "\n\nENTRYPOINT npm start --prefix /root/scalitys3";
            }
            else
            {
                $strImageParent = containerRepo() . ':' . CONTAINER_S3_SERVER_TAG;
            }

            containerWrite(
                $oStorageDocker, $strTempPath, $strOS, 'S3 Server', $strImageParent, $strImage, $strCopy, $strScript, $bVmForce);
        }

        # Test image
        ########################################################################################################################
        if (!$bDeprecated)
        {
            $strImageParent = containerRepo() . ":${strOS}-base";
            $strImage = "${strOS}-test";

            if (vmCoveragePerl($strOS))
            {
                $oStorageDocker->copy(
                    "test/package/${strOS}-${strPkgDevelCover}", "test/.vagrant/docker/${strOS}-${strPkgDevelCover}");

                $strCopy =
                    "# Copy Devel::Cover\n" .
                    "COPY ${strOS}-${strPkgDevelCover} /tmp/${strPkgDevelCover}";

                $strScript = sectionHeader() .
                    "# Install packages\n" .
                    "    apt-get install -y libjson-maybexs-perl";

                $strScript .= sectionHeader() .
                    "# Install Devel::Cover\n" .
                    "    dpkg -i /tmp/${strPkgDevelCover}";
            }
            else
            {
                $strCopy = undef;
                $strScript = '';
            }

            #---------------------------------------------------------------------------------------------------------------------------
            $strScript .= sectionHeader() .
                "# Create banner to make sure pgBackRest ignores it\n" .
                "    echo '***********************************************' >  /etc/issue.net && \\\n" .
                "    echo 'Sample banner to make sure banners are skipped.' >> /etc/issue.net && \\\n" .
                "    echo ''                                                >> /etc/issue.net && \\\n" .
                "    echo 'More banner after a blank line.'                 >> /etc/issue.net && \\\n" .
                "    echo '***********************************************' >> /etc/issue.net && \\\n" .
                "    echo 'Banner /etc/issue.net'                           >> /etc/ssh/sshd_config";

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
                    "    echo '%" . TEST_GROUP . "        ALL=(ALL)       NOPASSWD: ALL' > /etc/sudoers.d/" . TEST_GROUP . " && \\\n" .
                    "    sed -i 's/^Defaults    requiretty\$/\\# Defaults    requiretty/' /etc/sudoers";
            }
            else
            {
                $strScript .=
                    "    echo '%" . TEST_GROUP . " ALL=(ALL) NOPASSWD: ALL' >> /etc/sudoers";
            }

            $strScript .=
                sshSetup($strOS, TEST_USER, TEST_GROUP, $$oVm{$strOS}{&VM_CONTROL_MASTER});

            if (!$bDeprecated)
            {
                $strScript .= sectionHeader() .
                    "# Config AWS CLI\n" .
                    '    sudo -i -u ' . TEST_USER . " aws configure set region us-east-1 && \\\n" .
                    '    sudo -i -u ' . TEST_USER . " aws configure set aws_access_key_id accessKey1 && \\\n" .
                    '    sudo -i -u ' . TEST_USER . " aws configure set aws_secret_access_key verySecretKey1";
            }

            $strScript .= sectionHeader() .
                "# Create pgbackrest user\n" .
                '    ' . userCreate($strOS, BACKREST_USER, BACKREST_USER_ID, TEST_GROUP);

            $strScript .=
                sshSetup($strOS, BACKREST_USER, TEST_GROUP, $$oVm{$strOS}{&VM_CONTROL_MASTER});

            $strScript .=  sectionHeader() .
                "# Make " . TEST_USER . " home dir readable\n" .
                '    chmod g+r,g+x /home/' . TEST_USER;

            $strScript .= entryPointSetup($strOS);

            containerWrite(
                $oStorageDocker, $strTempPath, $strOS, 'Test', $strImageParent, $strImage, $strCopy, $strScript, $bVmForce);
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
