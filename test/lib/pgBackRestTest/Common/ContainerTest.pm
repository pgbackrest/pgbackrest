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
use constant POSTGRES_GROUP_ID                                      => getgrnam(POSTGRES_GROUP) . '';
use constant POSTGRES_USER                                          => POSTGRES_GROUP;
use constant POSTGRES_USER_ID                                       => POSTGRES_GROUP_ID;

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
use constant LIB_COVER_VERSION                                      => '1.23-2';
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
    return BACKREST_EXE . qw(/) . 'test';
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
    &log(INFO, "Building ${strTag} image...");

    $strScript =
        "# ${strTitle} Container\n" .
        "FROM ${strImageParent}\n\n" .
        (defined($strCopy) ? "${strCopy}\n\n" : '') .
        "RUN echo '" . (CONTAINER_DEBUG ? 'DEBUG' : 'OPTIMIZED') . " BUILD'" .
        $strScript;

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

    my $strScript = sectionHeader() .
        "# Setup SSH\n" .
        "    mkdir /home/${strUser}/.ssh && \\\n" .
        "    cp /root/.ssh/id_rsa  /home/${strUser}/.ssh/id_rsa && \\\n" .
        "    cp /root/.ssh/authorized_keys  /home/${strUser}/.ssh/authorized_keys && \\\n" .
        "    cp /root/.ssh/config /home/${strUser}/.ssh/config && \\\n";

    if ($bControlMaster)
    {
        $strScript .=
            "    echo '    ControlMaster auto' >> /home/${strUser}/.ssh/config && \\\n" .
            "    echo '    ControlPath /tmp/\%r\@\%h:\%p' >> /home/${strUser}/.ssh/config && \\\n" .
            "    echo '    ControlPersist 30' >> /home/${strUser}/.ssh/config && \\\n";
    }

    $strScript .=
        "    chown -R ${strUser}:${strGroup} /home/${strUser}/.ssh && \\\n" .
        "    chmod 700 /home/${strUser}/.ssh && \\\n" .
        "    chmod 600 /home/${strUser}/.ssh/*";

    return $strScript;
}

####################################################################################################################################
# Cert Setup
####################################################################################################################################
sub certSetup
{
    return
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
        "        -sha256";
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

    # Create SSH key (if it does not already exist)
    if (-e "${strTempPath}/id_rsa")
    {
        &log(INFO, "SSH key already exists");
    }
    else
    {
        &log(INFO, "Building SSH keys...");

        executeTest("ssh-keygen -f ${strTempPath}/id_rsa -t rsa -b 1024 -N ''", {bSuppressStdErr => true});
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

        #---------------------------------------------------------------------------------------------------------------------------
        my $strCopy =
            "# Copy root SSH keys\n" .
            "COPY id_rsa /root/.ssh/id_rsa\n" .
            "COPY id_rsa.pub /root/.ssh/authorized_keys";

        #---------------------------------------------------------------------------------------------------------------------------
        my $strScript = sectionHeader() .
            "# Install base packages\n";

        if ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_RHEL)
        {
            $strScript .=
                "    yum -y install epel-release && \\\n" .
                "    yum -y update && \\\n" .
                "    yum -y install openssh-server openssh-clients wget sudo python-pip build-essential valgrind git \\\n" .
                "        perl perl-Digest-SHA perl-DBD-Pg perl-XML-LibXML perl-IO-Socket-SSL \\\n" .
                "        gcc make perl-ExtUtils-MakeMaker perl-Test-Simple openssl-devel perl-ExtUtils-Embed";

            if ($strOS eq VM_CO6)
            {
                $strScript .= ' perl-Time-HiRes perl-parent perl-JSON';
            }
            else
            {
                $strScript .= ' perl-JSON-PP';
            }
        }
        else
        {
            $strScript .=
                "    apt-get update && \\\n" .
                "    apt-get -y install wget python && \\\n" .
                "    wget --no-check-certificate -O /root/get-pip.py https://bootstrap.pypa.io/get-pip.py && \\\n" .
                "    python /root/get-pip.py && \\\n" .
                "    apt-get -y install openssh-server wget sudo python-pip build-essential valgrind git \\\n" .
                "        libdbd-pg-perl libhtml-parser-perl libio-socket-ssl-perl libxml-libxml-perl libssl-dev libperl-dev";

            if ($strOS eq VM_U12)
            {
                $strScript .= ' libperl5.14';
            }
            elsif ($strOS eq VM_U16)
            {
                $strScript .= ' clang-5.0';
            }
        }

        #---------------------------------------------------------------------------------------------------------------------------
        $strScript .= sectionHeader() .
            "# Regenerate SSH keys\n" .
            "    rm -f /etc/ssh/ssh_host_rsa_key* && \\\n" .
            "    ssh-keygen -t rsa -b 1024 -f /etc/ssh/ssh_host_rsa_key";

        $strScript .= sectionHeader() .
            "# Fix SSH permissions\n" .
            "    chmod 700 /root/.ssh && \\\n" .
            "    chmod 600 /root/.ssh/*";

        $strScript .= sectionHeader() .
            "# Disable strict host key checking (so tests will run without a prior logon)\n" .
            "    echo 'Host *' > /root/.ssh/config && \\\n" .
            "    echo '    StrictHostKeyChecking no' >> /root/.ssh/config";

        $strScript .= sectionHeader() .
            "# Create banner to make sure pgBackRest ignores it\n" .
            "    echo '***********************************************' >  /etc/issue.net && \\\n" .
            "    echo 'Sample banner to make sure banners are skipped.' >> /etc/issue.net && \\\n" .
            "    echo ''                                                >> /etc/issue.net && \\\n" .
            "    echo 'More banner after a blank line.'                 >> /etc/issue.net && \\\n" .
            "    echo '***********************************************' >> /etc/issue.net && \\\n" .
            "    echo 'Banner /etc/issue.net'                           >> /etc/ssh/sshd_config";

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
        $strScript .= certSetup();

        #---------------------------------------------------------------------------------------------------------------------------
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

        #---------------------------------------------------------------------------------------------------------------------------
        if (!$bDeprecated)
        {
            $strScript .=  sectionHeader() .
                "# Create PostgreSQL user/group with known ids for testing\n" .
                '    ' . groupCreate($strOS, POSTGRES_GROUP, POSTGRES_GROUP_ID) . " && \\\n" .
                '    ' . userCreate($strOS, POSTGRES_USER, POSTGRES_USER_ID, POSTGRES_GROUP);

            $strScript .=  sectionHeader() .
                "# Install Postgresql packages\n";

            if ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_RHEL)
            {
                if ($strOS eq VM_CO6)
                {
                    $strScript .=
                        "    rpm -ivh http://yum.postgresql.org/9.0/redhat/rhel-6-x86_64/pgdg-centos90-9.0-5.noarch.rpm \\\n" .
                        "        http://yum.postgresql.org/9.1/redhat/rhel-6-x86_64/pgdg-centos91-9.1-6.noarch.rpm \\\n" .
                        "        http://yum.postgresql.org/9.5/redhat/rhel-6-x86_64/pgdg-centos95-9.5-3.noarch.rpm \\\n" .
                        "        http://yum.postgresql.org/9.6/redhat/rhel-6-x86_64/pgdg-centos96-9.6-3.noarch.rpm";
                }
                elsif ($strOS eq VM_CO7)
                {
                    $strScript .=
                        "    rpm -ivh http://yum.postgresql.org/9.6/redhat/rhel-7-x86_64/pgdg-centos96-9.6-3.noarch.rpm";
                }
            }
            else
            {
                $strScript .=
                    "    echo 'deb http://apt.postgresql.org/pub/repos/apt/ " .
                    $$oVm{$strOS}{&VM_OS_REPO} . "-pgdg main' >> /etc/apt/sources.list.d/pgdg.list && \\\n" .
                    "    wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | apt-key add - && \\\n" .
                    "    apt-get update";
            }

            if (defined($oOS->{&VM_DB}) && @{$oOS->{&VM_DB}} > 0)
            {
                $strScript .=  sectionHeader() .
                    "# Install Postgresql\n";

                if ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_RHEL)
                {
                    $strScript .= "    yum -y install";
                }
                else
                {
                    $strScript .= "    apt-get install -y";
                }

                # Construct list of databases to install
                my $strRunAfterInstall;

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

                        $strRunAfterInstall .= (defined($strRunAfterInstall) ? " && \\\n" : '') .
                            "    pg_dropcluster --stop ${strDbVersion} main";
                    }
                }

                # If there are commands to run after install
                if (defined($strRunAfterInstall))
                {
                    $strScript .= " && \\\n" . $strRunAfterInstall;
                }
            }
        }

        #---------------------------------------------------------------------------------------------------------------------------
        if (!$bDeprecated)
        {
            $strScript .= sectionHeader() .
                "# Install AWS CLI\n" .
                "    pip install --upgrade pip && \\\n" .
                "    pip install --upgrade awscli && \\\n" .
                '    sudo -i -u ' . TEST_USER . " aws configure set region us-east-1 && \\\n" .
                '    sudo -i -u ' . TEST_USER . " aws configure set aws_access_key_id accessKey1 && \\\n" .
                '    sudo -i -u ' . TEST_USER . " aws configure set aws_secret_access_key verySecretKey1";
        }

        #---------------------------------------------------------------------------------------------------------------------------
        if (!$bDeprecated && $strOS ne VM_CO6 && $strOS ne VM_U12)
        {
            $strScript .= s3ServerSetup($strOS);
        }

        #---------------------------------------------------------------------------------------------------------------------------
        $strScript .=
            "\n\n# Start SSH when container starts\n" .
            'ENTRYPOINT ';

        if ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_RHEL || $strOS eq VM_U12)
        {
            $strScript .= '/usr/sbin/sshd -D';
        }
        else
        {
            $strScript .= 'service ssh restart && bash';
        }

        containerWrite($oStorageDocker, $strTempPath, $strOS, 'Base', $strImageParent, $strImage, $strCopy, $strScript, $bVmForce);

        # Build image
        ###########################################################################################################################
        $strImageParent = containerRepo() . ":${strOS}-base";
        $strImage = "${strOS}-build";
        $strCopy = undef;

        my $strPkgDevelCover = packageDevelCover($oVm->{$strOS}{&VM_ARCH});
        my $bPkgDevelCoverBuild = vmCoverage($strOS) && !$oStorageDocker->exists("test/package/${strOS}-${strPkgDevelCover}");

        # Install Perl packages
        if ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_DEBIAN)
        {
            $strScript = sectionHeader() .
                "# Install package build tools\n" .
                "    apt-get install -y devscripts lintian libxml-checker-perl txt2man debhelper \\\n" .
                "        libpod-coverage-perl libppi-html-perl libtemplate-perl libtest-differences-perl";

            $strScript .=  sectionHeader() .
                "# Install pgBackRest package source\n" .
                "    git clone https://salsa.debian.org/postgresql/pgbackrest.git /root/package-src";

            # Build only when a new version has been specified
            if ($bPkgDevelCoverBuild)
            {
                $strScript .=  sectionHeader() .
                    "# Install Devel::Cover package source & build\n" .
                    "    git clone https://anonscm.debian.org/git/pkg-perl/packages/libdevel-cover-perl.git" .
                        " /root/libdevel-cover-perl && \\\n" .
                    "    cd /root/libdevel-cover-perl && \\\n" .
                    "    git checkout debian/" . LIB_COVER_VERSION . " && \\\n" .
                    "    debuild -i -us -uc -b";
            }
        }
        else
        {
            $strScript = sectionHeader() .
                "# Install package build tools\n" .
                "    yum install -y rpm-build";

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

        containerWrite($oStorageDocker, $strTempPath, $strOS, 'Build', $strImageParent, $strImage, $strCopy,$strScript, $bVmForce);

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
            $strCopy = undef;

            if ($strOS ne VM_CO6 && $strOS ne VM_U12)
            {
                $strImageParent = containerRepo() . ":${strOS}-base";
                $strScript = '';
            }
            else
            {
                $strImageParent = $oVm->{&VM_U16}{&VM_IMAGE};

                $strScript = sectionHeader() .
                    "# Install required packages\n" .
                    "    apt-get update && \\\n" .
                    "    apt-get install -y wget git";

                $strScript .= certSetup();
                $strScript .= s3ServerSetup(VM_U16);

                $strScript .= sectionHeader() .
                    "# Fix root tty\n" .
                    "    echo 'tty -s && mesg n || true' > /root/.profile";
            }

            $strScript .= "\n\nENTRYPOINT npm start --prefix /root/scalitys3";

            containerWrite(
                $oStorageDocker, $strTempPath, $strOS, 'S3 Server', $strImageParent, $strImage, $strCopy, $strScript, $bVmForce);
        }

        # Test image
        ########################################################################################################################
        if (!$bDeprecated)
        {
            $strImageParent = containerRepo() . ":${strOS}-base";
            $strImage = "${strOS}-test";

            if (vmCoverage($strOS))
            {
                $oStorageDocker->copy(
                    "test/package/${strOS}-${strPkgDevelCover}", "test/.vagrant/docker/${strOS}-${strPkgDevelCover}");

                $strCopy =
                    "# Copy Devel::Cover\n" .
                    "COPY ${strOS}-${strPkgDevelCover} /tmp/${strPkgDevelCover}";

                $strScript = sectionHeader() .
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
                "# Create pgbackrest user\n" .
                '    ' . userCreate($strOS, BACKREST_USER, BACKREST_USER_ID, TEST_GROUP);

            $strScript .=
                sshSetup($strOS, BACKREST_USER, TEST_GROUP, $$oVm{$strOS}{&VM_CONTROL_MASTER});

            $strScript .=
                sshSetup($strOS, TEST_USER, TEST_GROUP, $$oVm{$strOS}{&VM_CONTROL_MASTER});

            $strScript .=  sectionHeader() .
                "# Make " . TEST_USER . " home dir readable\n" .
                '    chmod g+r,g+x /home/' . TEST_USER;

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
