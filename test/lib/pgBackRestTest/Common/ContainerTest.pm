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

use constant BACKREST_USER                                          => 'backrest';
    push @EXPORT, qw(BACKREST_USER);
use constant BACKREST_USER_ID                                       => getpwnam(BACKREST_USER) . '';

####################################################################################################################################
# Package constants
####################################################################################################################################
use constant LIB_COVER_VERSION                                      => '1.23-2';
    push @EXPORT, qw(LIB_COVER_VERSION);
use constant LIB_COVER_PACKAGE                                      => 'libdevel-cover-perl_' . LIB_COVER_VERSION . '_amd64.deb';
    push @EXPORT, qw(LIB_COVER_PACKAGE);
use constant LIB_COVER_EXE                                          => '/usr/bin/cover';
    push @EXPORT, qw(LIB_COVER_EXE);

####################################################################################################################################
# Container repo
####################################################################################################################################
sub containerRepo
{
    return BACKREST_EXE . qw(/) . 'test';
}

push @EXPORT, qw(containerRepo);

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

sub postgresGroupCreate
{
    my $strOS = shift;

    return "# Create PostgreSQL group\n" .
           'RUN ' . groupCreate($strOS, POSTGRES_GROUP, POSTGRES_GROUP_ID);
}

sub postgresUserCreate
{
    my $strOS = shift;

    return "# Create PostgreSQL user\n" .
           'RUN ' . userCreate($strOS, POSTGRES_USER, POSTGRES_USER_ID, POSTGRES_GROUP);
}

sub backrestUserCreate
{
    my $strOS = shift;
    my $strGroup = shift;

    return "# Create BackRest group\n" .
           'RUN ' . userCreate($strOS, BACKREST_USER, BACKREST_USER_ID, $strGroup);
}

####################################################################################################################################
# Create configuration file
####################################################################################################################################
sub backrestConfigCreate
{
    my $strOS = shift;
    my $strUser = shift;
    my $strGroup = shift;

    return "# Create pgbackrest.conf\n" .
           "RUN touch /etc/pgbackrest.conf && \\\n" .
           "    chmod 640 /etc/pgbackrest.conf && \\\n" .
           "    chown ${strUser}:${strGroup} /etc/pgbackrest.conf";
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
    my $strScript = shift;
    my $bForce = shift;
    my $bPre = shift;
    my $bPreOnly = shift;

    if (defined($bPre) && $bPre)
    {
        $strImage .= '-pre';
        $strImageParent .= '-pre';
        $strTitle .= ' (Pre-Installed)'
    }
    elsif (!defined($bPre))
    {
        containerWrite($oStorageDocker, $strTempPath, $strOS, $strTitle, $strImageParent, $strImage, $strScript, $bForce, true);

        if (defined($bPreOnly) && $bPreOnly)
        {
            return;
        }
    }

    my $strTag = containerRepo() . ":${strImage}";
    &log(INFO, "Building ${strTag} image...");

    $strScript =
        "# ${strTitle} Container\n" .
        "FROM ${strImageParent}\n\n" .
        $strScript;

    # Write the image
    $oStorageDocker->put("${strTempPath}/${strImage}", trim($strScript) . "\n");
    executeTest('docker build' . (defined($bForce) && $bForce ? ' --no-cache' : '') .
                " -f ${strTempPath}/${strImage} -t ${strTag} ${strTempPath}",
                {bSuppressStdErr => true});
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

    my $strScript =
        "# Setup SSH\n" .
        "RUN mkdir /home/${strUser}/.ssh && \\\n" .
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
# Repo setup
####################################################################################################################################
sub repoSetup
{
    my $strOS = shift;
    my $strUser = shift;
    my $strGroup = shift;

    return "# Setup repository\n" .
           "RUN mkdir /var/lib/pgbackrest && \\\n" .
           "    chown -R ${strUser}:${strGroup} /var/lib/pgbackrest && \\\n" .
           "    chmod 750 /var/lib/pgbackrest";
}

####################################################################################################################################
# Sudo setup
####################################################################################################################################
sub sudoSetup
{
    my $strOS = shift;
    my $strGroup = shift;

    my $strScript =
        "# Setup sudo";

    my $oVm = vmGet();

    if ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_RHEL)
    {
        $strScript .=
            "\nRUN yum -y install sudo && \\" .
            "\n    echo '%${strGroup}        ALL=(ALL)       NOPASSWD: ALL' > /etc/sudoers.d/${strGroup} && \\" .
            "\n    sed -i 's/^Defaults    requiretty\$/\\# Defaults    requiretty/' /etc/sudoers";
    }
    elsif ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_DEBIAN)
    {
        $strScript .=
            "\nRUN apt-get -y install sudo && \\" .
            "\n    echo '%${strGroup} ALL=(ALL) NOPASSWD: ALL' >> /etc/sudoers";
    }
    else
    {
        confess &log(ERROR, "unable to find base for ${strOS}");
    }

    return $strScript;
}

####################################################################################################################################
# Devel::Cover setup
####################################################################################################################################
sub coverSetup
{
    my $strOS = shift;

    my $strScript = '';
    my $oVm = vmGet();

    if ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_DEBIAN)
    {
        $strScript .=
            "\n\n# Install Devel::Cover\n" .
            "COPY ${strOS}-" . LIB_COVER_PACKAGE . ' /tmp/' . LIB_COVER_PACKAGE . "\n" .
            'RUN dpkg -i /tmp/' . LIB_COVER_PACKAGE;
    }

    return $strScript;
}

####################################################################################################################################
# Install Perl packages
####################################################################################################################################
sub perlInstall
{
    my $strOS = shift;

    my $strImage =
        "# Install Perl packages\n";

    if ($strOS eq VM_CO6)
    {
        $strImage .=
            'RUN yum install -y perl perl-Time-HiRes perl-parent perl-JSON perl-Digest-SHA perl-DBD-Pg';
    }
    elsif ($strOS eq VM_CO7)
    {
        $strImage .=
            'RUN yum install -y perl perl-JSON-PP perl-Digest-SHA perl-DBD-Pg';
    }
    elsif ($strOS eq VM_U12 || $strOS eq VM_U14)
    {
        $strImage .=
            'RUN apt-get install -y libdbd-pg-perl libdbi-perl libnet-daemon-perl libplrpc-perl libhtml-parser-perl';
    }
    elsif ($strOS eq VM_U16 || $strOS eq VM_D8)
    {
        $strImage .=
            'RUN apt-get install -y libdbd-pg-perl libdbi-perl libhtml-parser-perl';
    }
    else
    {
        confess &log(ERROR, "unable to install perl for os '${strOS}'");
    }

    return $strImage;
}

####################################################################################################################################
# Build containers
####################################################################################################################################
sub containerBuild
{
    my $oStorageDocker = shift;
    my $strVm = shift;
    my $bVmForce = shift;
    my $strDbVersionBuild = shift;

    # Create temp path
    my $strTempPath = $oStorageDocker->pathGet('test/.vagrant/docker');
    $oStorageDocker->pathCreate($strTempPath, {strMode => '0770', bIgnoreExists => true, bCreateParent => true});

    # Remove old images on force
    if ($bVmForce)
    {
        my $strRegExp = '^' . containerRepo();

        if ($strVm ne 'all')
        {
            $strRegExp .= "${strVm}-";
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

    foreach my $strOS (sort(keys(%$oVm)))
    {
        if ($strVm ne 'all' && $strVm ne $strOS)
        {
            next;
        }

        my $oOS = $$oVm{$strOS};
        my $bDocBuild = false;

        # Base image
        ###########################################################################################################################
        my $strImageParent = "$$oVm{$strOS}{&VM_IMAGE}";
        my $strImage = "${strOS}-base";

        # Install base packages
        my $strScript = "# Install base packages\n";

        if ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_RHEL)
        {
            $strScript .= "RUN yum -y install openssh-server openssh-clients\n";
        }
        elsif ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_DEBIAN)
        {
            $strScript .=
                "RUN apt-get update && \\\n" .
                "    apt-get -y install openssh-server wget vim net-tools iputils-ping\n";

            $strScript .=
                "\n# Fix root tty\n" .
                "RUN sed -i 's/^mesg n/tty -s \\&\\& mesg n/g' /root/.profile\n";
        }

        # Add PostgreSQL packages
        $strScript .= "\n# Add PostgreSQL packages\n";

        if ($strOS eq VM_CO6)
        {
            $strScript .=
                "RUN rpm -ivh http://yum.postgresql.org/9.0/redhat/rhel-6-x86_64/pgdg-centos90-9.0-5.noarch.rpm && \\\n" .
                "    rpm -ivh http://yum.postgresql.org/9.1/redhat/rhel-6-x86_64/pgdg-centos91-9.1-6.noarch.rpm && \\\n" .
                "    rpm -ivh http://yum.postgresql.org/9.2/redhat/rhel-6-x86_64/pgdg-centos92-9.2-8.noarch.rpm && \\\n" .
                "    rpm -ivh http://yum.postgresql.org/9.3/redhat/rhel-6-x86_64/pgdg-centos93-9.3-3.noarch.rpm && \\\n" .
                "    rpm -ivh http://yum.postgresql.org/9.4/redhat/rhel-6-x86_64/pgdg-centos94-9.4-3.noarch.rpm && \\\n" .
                "    rpm -ivh http://yum.postgresql.org/9.5/redhat/rhel-6-x86_64/pgdg-centos95-9.5-3.noarch.rpm && \\\n" .
                "    rpm -ivh http://yum.postgresql.org/9.6/redhat/rhel-6-x86_64/pgdg-centos96-9.6-3.noarch.rpm";
        }
        elsif ($strOS eq VM_CO7)
        {
            $strScript .=
                "RUN rpm -ivh http://yum.postgresql.org/9.3/redhat/rhel-7-x86_64/pgdg-centos93-9.3-3.noarch.rpm && \\\n" .
                "    rpm -ivh http://yum.postgresql.org/9.4/redhat/rhel-7-x86_64/pgdg-centos94-9.4-3.noarch.rpm && \\\n" .
                "    rpm -ivh http://yum.postgresql.org/9.5/redhat/rhel-7-x86_64/pgdg-centos95-9.5-3.noarch.rpm && \\\n" .
                "    rpm -ivh http://yum.postgresql.org/9.6/redhat/rhel-7-x86_64/pgdg-centos96-9.6-3.noarch.rpm";
        }
        elsif ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_DEBIAN)
        {
            $strScript .=
                "RUN echo 'deb http://apt.postgresql.org/pub/repos/apt/ " .
                $$oVm{$strOS}{&VM_OS_REPO} . "-pgdg main' >> /etc/apt/sources.list.d/pgdg.list && \\\n" .
                "    wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | apt-key add - && \\\n" .
                "    apt-get update";
        }

        $strScript .=
            "\n\n# Regenerate SSH keys\n" .
            "RUN rm -f /etc/ssh/ssh_host_rsa_key* && \\\n" .
            "    ssh-keygen -t rsa -b 1024 -f /etc/ssh/ssh_host_rsa_key";

        $strScript .=
            "\n\n# Add banner to make sure protocol ignores it\n" .
            "RUN echo '***********************************************' >  /etc/issue.net && \\\n" .
            "    echo 'Sample banner to make sure banners are skipped.' >> /etc/issue.net && \\\n" .
            "    echo ''                                                >> /etc/issue.net && \\\n" .
            "    echo 'More banner after a blank line.'                 >> /etc/issue.net && \\\n" .
            "    echo '***********************************************' >> /etc/issue.net && \\\n" .
            "    echo 'Banner /etc/issue.net'                           >> /etc/ssh/sshd_config";

        # Copy ssh keys
        $strScript .=
            "\n\n# Copy ssh keys to root\n" .
            "RUN mkdir -m 700 -p /root/.ssh\n" .
            "COPY id_rsa  /root/.ssh/id_rsa\n" .
            "COPY id_rsa.pub  /root/.ssh/authorized_keys\n" .
            "RUN echo 'Host *' > /root/.ssh/config && \\\n" .
            "    echo '    StrictHostKeyChecking no' >> /root/.ssh/config && \\\n" .
            "    chmod 600 /root/.ssh/*";

        # Create PostgreSQL Group
        $strScript .= "\n\n" . postgresGroupCreate($strOS);

        # Create test user
        $strScript .=
            "\n\n# Create test user\n" .
            'RUN ' . groupCreate($strOS, TEST_GROUP, TEST_GROUP_ID) . " && \\\n" .
            '    ' . userCreate($strOS, TEST_USER, TEST_USER_ID, TEST_GROUP) . " && \\\n" .
            '    mkdir -m 750 /home/' . TEST_USER . "/test && \\\n" .
            '    chown ' . TEST_USER . ':' . TEST_GROUP . ' /home/' . TEST_USER . '/test';

        # Suppress dpkg interactive output
        if ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_DEBIAN)
        {
            $strScript .=
                "\n\n# Suppress dpkg interactive output\n" .
                "RUN rm /etc/apt/apt.conf.d/70debconf";
        }

        # Start SSH when container starts
        $strScript .=
            "\n\n# Start SSH when container starts\n";

        # This is required in newer versions of u12 containers - seems like a bug in the container
        if ($strOS eq VM_U12)
        {
            $strScript .=
                "RUN mkdir /var/run/sshd\n";
        }

        if ($strOS eq VM_U14 || $strOS eq VM_D8 || $strOS eq VM_U16)
        {
            $strScript .=
                "ENTRYPOINT service ssh restart && bash";
        }
        else
        {
            $strScript .=
                "ENTRYPOINT /usr/sbin/sshd -D";
        }

        # Write the image
        containerWrite($oStorageDocker, $strTempPath, $strOS, 'Base', $strImageParent, $strImage, $strScript, $bVmForce, false);

        # Base pre image
        ###########################################################################################################################
        $strImageParent = containerRepo() . ":${strOS}-base";
        $strImage = "${strOS}-base-pre";

        # Install Perl packages
        $strScript =
            perlInstall($strOS) . "\n";

        # Write the image
        containerWrite(
            $oStorageDocker, $strTempPath, $strOS, 'Base (Pre-Installed)', $strImageParent, $strImage, $strScript, $bVmForce,
            false);

        # Build image
        ###########################################################################################################################
        $strImageParent = containerRepo() . ":${strOS}-base-pre";
        $strImage = "${strOS}-build";

        # Install Perl packages
        if ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_DEBIAN)
        {
            $strScript =
                "# Install package build tools and package source\n" .
                "RUN apt-get install -y devscripts build-essential lintian git libxml-checker-perl txt2man debhelper" .
                    " libpod-coverage-perl libppi-html-perl libtemplate-perl libtest-differences-perl && \\\n" .
                "    git clone https://anonscm.debian.org/git/pkg-postgresql/pgbackrest.git /root/package-src && \\\n" .
                "    git clone https://anonscm.debian.org/git/pkg-perl/packages/libdevel-cover-perl.git" .
                    " /root/libdevel-cover-perl && \\\n" .
                '    cd /root/libdevel-cover-perl && git checkout debian/' . LIB_COVER_VERSION . ' && debuild -i -us -uc -b';
        }
        else
        {
            $strScript =
                "# Install package build tools and package source\n" .
                "RUN yum install -y gcc make perl-ExtUtils-MakeMaker perl-Test-Simple git\n";
        }

        # Write the image
        containerWrite($oStorageDocker, $strTempPath, $strOS, 'Build', $strImageParent, $strImage, $strScript, $bVmForce, false);

        # Copy Devel::Cover to host so it can be installed in other containers
        if ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_DEBIAN)
        {
            executeTest('docker rm -f test-build', {bSuppressError => true});
            executeTest(
                "docker run -itd -h test-build --name=test-build" .
                " -v ${strTempPath}:${strTempPath} " . containerRepo() . ":${strOS}-build",
                {bSuppressStdErr => true});
            executeTest(
                "docker exec -i test-build " .
                "bash -c 'cp /root/" . LIB_COVER_PACKAGE . " ${strTempPath}/${strOS}-" . LIB_COVER_PACKAGE . "'");
            executeTest('docker rm -f test-build');
        }

        # Db image
        ###########################################################################################################################
        my @stryDbBuild;

        # Get the list of db versions required for testing
        if ($strDbVersionBuild eq 'all')
        {
            push(@stryDbBuild, @{$$oOS{&VM_DB}});
        }
        elsif ($strDbVersionBuild eq 'minimal')
        {
            push(@stryDbBuild, @{$$oOS{&VM_DB_MINIMAL}});
        }
        elsif ($strDbVersionBuild ne 'none')
        {
            push(@stryDbBuild, $strDbVersionBuild);
        }

        # Merge db versions required for docs
        if ($strDbVersionBuild eq 'all' || $strDbVersionBuild eq 'minimal')
        {
            if (defined($$oOS{&VM_DB_DOC}))
            {
                @stryDbBuild = keys %{{map {($_ => 1)}                  ## no critic (BuiltinFunctions::ProhibitVoidMap)
                    (@stryDbBuild, @{$$oOS{&VM_DB_DOC}})}};
                $bDocBuild = true;
            }
        }

        foreach my $strDbVersion (sort(@stryDbBuild))
        {
            my $strDbVersionNoDot = $strDbVersion;
            $strDbVersionNoDot =~ s/\.//;
            my $strTitle = "Db ${strDbVersion}";

            my $bDocBuildVersion = ($bDocBuild && grep(/^$strDbVersion$/, @{$$oOS{&VM_DB_DOC}}));

            $strImageParent = containerRepo() . ":${strOS}-base";
            $strImage = "${strOS}-db-${strDbVersion}";

            # Create PostgreSQL User
            $strScript = postgresUserCreate($strOS);

            # Install PostgreSQL
            $strScript .=
                "\n\n# Install PostgreSQL";

            if ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_DEBIAN)
            {
                $strScript .=
                    "\nRUN apt-get install -y postgresql-${strDbVersion} && \\" .
                    "\n    pg_dropcluster --stop ${strDbVersion} main";
            }
            elsif ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_RHEL)
            {
                $strScript .=
                    "\nRUN yum -y install postgresql${strDbVersionNoDot}-server";
            }

            # Write the image
            containerWrite(
                $oStorageDocker, $strTempPath, $strOS, $strTitle, $strImageParent, $strImage, $strScript, $bVmForce,
                $bDocBuildVersion ? undef : true, $bDocBuildVersion ? undef : true);

            # Db doc image
            ########################################################################################################################
            if ($bDocBuildVersion)
            {
                $strImageParent = containerRepo() . ":${strOS}-db-${strDbVersion}";
                $strImage = "${strOS}-db-${strDbVersion}-doc";

                # Install SSH key
                $strScript = sshSetup($strOS, POSTGRES_USER, POSTGRES_GROUP, $$oVm{$strOS}{&VM_CONTROL_MASTER});

                # Setup sudo
                $strScript .= "\n\n" . sudoSetup($strOS, TEST_GROUP);

                # Write the image
                containerWrite(
                    $oStorageDocker, $strTempPath, $strOS, "${strTitle} Doc", $strImageParent, $strImage, $strScript, $bVmForce);
            }

            # Db test image
            ########################################################################################################################
            $strImageParent = containerRepo() . ":${strOS}-db-${strDbVersion}";
            $strImage = "${strOS}-db-${strDbVersion}-test";

            # Install SSH key
            $strScript = sshSetup($strOS, TEST_USER, TEST_GROUP, $$oVm{$strOS}{&VM_CONTROL_MASTER});

            # Setup Devel::Cover
            $strScript .= coverSetup($strOS);

            # Write the image
            containerWrite(
                $oStorageDocker, $strTempPath, $strOS, "${strTitle} Test", $strImageParent, $strImage, $strScript, $bVmForce, true,
                true);
        }

        # Db test image (for synthetic tests)
        ########################################################################################################################
        $strImageParent = containerRepo() . ":${strOS}-base";
        $strImage = "${strOS}-db-test";

        # Install SSH key
        $strScript = sshSetup($strOS, TEST_USER, TEST_GROUP, $$oVm{$strOS}{&VM_CONTROL_MASTER});

        # Setup Devel::Cover
        $strScript .= coverSetup($strOS);

        # Write the image
        containerWrite(
            $oStorageDocker, $strTempPath, $strOS, 'Db Synthetic Test', $strImageParent, $strImage, $strScript, $bVmForce, true,
            true);

        # Loop test image
        ########################################################################################################################
        $strImageParent = containerRepo() . ":${strOS}-base";
        $strImage = "${strOS}-loop-test";

        # Create BackRest User
        $strScript = backrestUserCreate($strOS, TEST_GROUP);

        # Install SSH key
        $strScript .=
            "\n\n" . sshSetup($strOS, BACKREST_USER, TEST_GROUP, $$oVm{$strOS}{&VM_CONTROL_MASTER});

        # Install SSH key
        $strScript .=
            "\n\n" . sshSetup($strOS, TEST_USER, TEST_GROUP, $$oVm{$strOS}{&VM_CONTROL_MASTER});

        # Make test user home readable
        $strScript .=
            "\n\n# Make " . TEST_USER . " home dir readable\n" .
            'RUN chmod g+r,g+x /home/' . TEST_USER;

        # Setup sudo
        $strScript .= "\n\n" . sudoSetup($strOS, TEST_GROUP);

        # Setup Devel::Cover
        $strScript .= coverSetup($strOS);

        # Write the image
        containerWrite(
            $oStorageDocker, $strTempPath, $strOS, 'Loop Test', $strImageParent, $strImage, $strScript, $bVmForce, true, true);

        # Backup image
        ###########################################################################################################################
        my $strTitle = "Backup";

        # Backup Doc image
        ###########################################################################################################################
        if ($bDocBuild)
        {
            $strImageParent = containerRepo() . ":${strOS}-base";
            $strImage = "${strOS}-backup-doc";

            # Create BackRest User
            $strScript = backrestUserCreate($strOS, POSTGRES_GROUP);

            # Install SSH key
            $strScript .=
                "\n\n" . sshSetup($strOS, BACKREST_USER, POSTGRES_GROUP, $$oVm{$strOS}{&VM_CONTROL_MASTER});

            # Create configuration file
            $strScript .=
                "\n\n" . backrestConfigCreate($strOS, BACKREST_USER, POSTGRES_GROUP);

            # Setup repository
            $strScript .=
                "\n\n" . repoSetup($strOS, BACKREST_USER, POSTGRES_GROUP);

            # Setup sudo
            $strScript .= "\n\n" . sudoSetup($strOS, TEST_GROUP);

            # Write the image
            containerWrite(
                $oStorageDocker, $strTempPath, $strOS, "${strTitle} Doc", $strImageParent, $strImage, $strScript, $bVmForce, true,
                true);
        }

        # Backup Test image
        ###########################################################################################################################
        $strImageParent = containerRepo() . ":${strOS}-base";
        $strImage = "${strOS}-backup-test";

        # Create BackRest User
        $strScript = backrestUserCreate($strOS, TEST_GROUP);

        # Install SSH key
        $strScript .=
            "\n\n" . sshSetup($strOS, BACKREST_USER, TEST_GROUP, $$oVm{$strOS}{&VM_CONTROL_MASTER});

        # Make test user home readable
        $strScript .=
            "\n\n# Make " . TEST_USER . " home dir readable\n" .
            'RUN chmod g+r,g+x /home/' . TEST_USER;

        # Setup Devel::Cover
        $strScript .= coverSetup($strOS);

        # Write the image
        containerWrite(
            $oStorageDocker, $strTempPath, $strOS, "${strTitle} Test", $strImageParent, $strImage, $strScript, $bVmForce, true,
            true);
    }
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

    foreach my $strImage (sort(split("\n", trim(executeTest('docker images --format "{{.Repository}}/{{.Tag}}"')))))
    {
        if ($strImage =~ $strRegExp)
        {
            &log(INFO, "Removing ${strImage} image...");
            executeTest("docker rmi -f ${strImage}", {bSuppressError => true});
        }
    }
}

1;
