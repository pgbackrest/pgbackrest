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
use pgBackRest::FileCommon;
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

use constant TEST_GROUP                                             => POSTGRES_GROUP;
    push @EXPORT, qw(TEST_GROUP);
use constant TEST_GROUP_ID                                          => POSTGRES_GROUP_ID;
use constant TEST_USER                                              => getpwuid($UID) . '';
    push @EXPORT, qw(TEST_USER);
use constant TEST_USER_ID                                           => $UID;

use constant BACKREST_GROUP                                         => POSTGRES_GROUP;
use constant BACKREST_GROUP_ID                                      => POSTGRES_GROUP_ID;
use constant BACKREST_USER                                          => 'backrest';
    push @EXPORT, qw(BACKREST_USER);
use constant BACKREST_USER_ID                                       => getpwnam(BACKREST_USER) . '';

####################################################################################################################################
# Container namespace
####################################################################################################################################
sub containerNamespace
{
    return BACKREST_EXE . qw(/) . TEST_USER;
}

push @EXPORT, qw(containerNamespace);

####################################################################################################################################
# User/group creation
####################################################################################################################################
sub groupCreate
{
    my $strOS = shift;
    my $strName = shift;
    my $iId = shift;

    return "RUN groupadd -g${iId} ${strName}";
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
        return "RUN adduser -g${strGroup} -u${iId} -n ${strName}";
    }
    elsif ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_DEBIAN)
    {
        return "RUN adduser --uid=${iId} --ingroup=${strGroup} --disabled-password --gecos \"\" ${strName}";
    }

    confess &log(ERROR, "unable to create user for os '${strOS}'");
}

sub postgresGroupCreate
{
    my $strOS = shift;

    return "# Create PostgreSQL group\n" .
           groupCreate($strOS, POSTGRES_GROUP, POSTGRES_GROUP_ID);
}

sub postgresUserCreate
{
    my $strOS = shift;

    return "# Create PostgreSQL user\n" .
           userCreate($strOS, POSTGRES_USER, POSTGRES_USER_ID, POSTGRES_GROUP);
}

sub backrestUserCreate
{
    my $strOS = shift;

    return "# Create BackRest group\n" .
           userCreate($strOS, BACKREST_USER, BACKREST_USER_ID, BACKREST_GROUP);
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
           "RUN touch /etc/pgbackrest.conf\n" .
           "RUN chmod 640 /etc/pgbackrest.conf\n" .
           "RUN chown ${strUser}:${strGroup} /etc/pgbackrest.conf";
}

####################################################################################################################################
# Write the Docker container
####################################################################################################################################
sub containerWrite
{
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
        containerWrite($strTempPath, $strOS, $strTitle, $strImageParent, $strImage, $strScript, $bForce, true);

        if (defined($bPreOnly) && $bPreOnly)
        {
            return;
        }
    }

    &log(INFO, "Building ${strImage} image...");

    $strScript =
        "# ${strTitle} Container\n" .
        "FROM ${strImageParent}\n\n" .
        $strScript;

    # Write the image
    fileStringWrite("${strTempPath}/${strImage}", trim($strScript) . "\n", false);
    executeTest('docker build' . (defined($bForce) && $bForce ? ' --no-cache' : '') .
                " -f ${strTempPath}/${strImage} -t " . containerNamespace() . "/${strImage} ${strTempPath}",
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
        "RUN mkdir /home/${strUser}/.ssh\n" .
        "COPY id_rsa  /home/${strUser}/.ssh/id_rsa\n" .
        "COPY id_rsa.pub  /home/${strUser}/.ssh/authorized_keys\n" .
        "RUN echo 'Host *' > /home/${strUser}/.ssh/config\n" .
        "RUN echo '    StrictHostKeyChecking no' >> /home/${strUser}/.ssh/config\n";

    if ($bControlMaster)
    {
        $strScript .=
            "RUN echo '    ControlMaster auto' >> /home/${strUser}/.ssh/config\n" .
            "RUN echo '    ControlPath /tmp/\%r\@\%h:\%p' >> /home/${strUser}/.ssh/config\n" .
            "RUN echo '    ControlPersist 30' >> /home/${strUser}/.ssh/config\n";
    }

    $strScript .=
        "RUN chown -R ${strUser}:${strGroup} /home/${strUser}/.ssh\n" .
        "RUN chmod 700 /home/${strUser}/.ssh\n" .
        "RUN chmod 600 /home/${strUser}/.ssh/*";

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
           "RUN mkdir /var/lib/pgbackrest\n" .
           "RUN chown -R ${strUser}:${strGroup} /var/lib/pgbackrest\n" .
           "RUN chmod 750 /var/lib/pgbackrest";
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
            "\nRUN yum -y install sudo\n" .
            "\nRUN echo '%${strGroup}        ALL=(ALL)       NOPASSWD: ALL' > /etc/sudoers.d/${strGroup}" .
            "\nRUN sed -i 's/^Defaults    requiretty\$/\\# Defaults    requiretty/' /etc/sudoers";
    }
    elsif ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_DEBIAN)
    {
        $strScript .=
            "\nRUN apt-get -y install sudo\n" .
            "\nRUN echo '%${strGroup} ALL=(ALL) NOPASSWD: ALL' >> /etc/sudoers";
    }
    else
    {
        confess &log(ERROR, "unable to find base for ${strOS}");
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
        return $strImage .
            "RUN yum install -y perl perl-Time-HiRes perl-parent perl-JSON perl-Digest-SHA perl-DBD-Pg";
    }
    elsif ($strOS eq VM_CO7)
    {
        return $strImage .
            "RUN yum install -y perl perl-JSON-PP perl-Digest-SHA perl-DBD-Pg";
    }
    elsif ($strOS eq VM_U12 || $strOS eq VM_U14)
    {
        return $strImage .
            "RUN apt-get install -y libdbd-pg-perl libdbi-perl libnet-daemon-perl libplrpc-perl";
    }
    elsif ($strOS eq VM_U16 || $strOS eq VM_D8)
    {
        return $strImage .
            "RUN apt-get install -y libdbd-pg-perl libdbi-perl" .
            ($strOS eq VM_U16 ? ' libdevel-cover-perl' : '');
    }

    confess &log(ERROR, "unable to install perl for os '${strOS}'");
}

####################################################################################################################################
# Build containers
####################################################################################################################################
sub containerBuild
{
    my $strVm = shift;
    my $bVmForce = shift;
    my $strDbVersionBuild = shift;

    # Create temp path
    my $strTempPath = dirname(abs_path($0)) . '/.vagrant/docker';
    filePathCreate($strTempPath, '0770', true, true);

    # Remove old images on force
    if ($bVmForce)
    {
        my $strRegExp = '^' . containerNamespace() . '/';

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
                "RUN apt-get update\n" .
                "RUN apt-get -y install openssh-server wget\n";
        }

        # Add PostgreSQL packages
        $strScript .= "\n# Add PostgreSQL packages\n";

        if ($strOS eq VM_CO6)
        {
            $strScript .=
                "RUN rpm -ivh http://yum.postgresql.org/9.0/redhat/rhel-6-x86_64/pgdg-centos90-9.0-5.noarch.rpm\n" .
                "RUN rpm -ivh http://yum.postgresql.org/9.1/redhat/rhel-6-x86_64/pgdg-centos91-9.1-6.noarch.rpm\n" .
                "RUN rpm -ivh http://yum.postgresql.org/9.2/redhat/rhel-6-x86_64/pgdg-centos92-9.2-8.noarch.rpm\n" .
                "RUN rpm -ivh http://yum.postgresql.org/9.3/redhat/rhel-6-x86_64/pgdg-centos93-9.3-3.noarch.rpm\n" .
                "RUN rpm -ivh http://yum.postgresql.org/9.4/redhat/rhel-6-x86_64/pgdg-centos94-9.4-3.noarch.rpm\n" .
                "RUN rpm -ivh http://yum.postgresql.org/9.5/redhat/rhel-6-x86_64/pgdg-centos95-9.5-3.noarch.rpm\n" .
                "RUN rpm -ivh http://yum.postgresql.org/9.6/redhat/rhel-6-x86_64/pgdg-centos96-9.6-3.noarch.rpm";
        }
        elsif ($strOS eq VM_CO7)
        {
            $strScript .=
                "RUN rpm -ivh http://yum.postgresql.org/9.3/redhat/rhel-7-x86_64/pgdg-centos93-9.3-3.noarch.rpm\n" .
                "RUN rpm -ivh http://yum.postgresql.org/9.4/redhat/rhel-7-x86_64/pgdg-centos94-9.4-3.noarch.rpm\n" .
                "RUN rpm -ivh http://yum.postgresql.org/9.5/redhat/rhel-7-x86_64/pgdg-centos95-9.5-3.noarch.rpm\n" .
                "RUN rpm -ivh http://yum.postgresql.org/9.6/redhat/rhel-7-x86_64/pgdg-centos96-9.6-3.noarch.rpm";
        }
        elsif ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_DEBIAN)
        {
            $strScript .=
                "RUN echo 'deb http://apt.postgresql.org/pub/repos/apt/ " .
                $$oVm{$strOS}{&VM_OS_REPO} . "-pgdg main' >> /etc/apt/sources.list.d/pgdg.list\n" .
                "RUN wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | apt-key add -\n" .
                "RUN apt-get update";
        }

        $strScript .=
            "\n\n# Regenerate SSH keys\n" .
            "RUN rm -f /etc/ssh/ssh_host_rsa_key*\n" .
            "RUN ssh-keygen -t rsa -b 1024 -f /etc/ssh/ssh_host_rsa_key";

        $strScript .=
            "\n\n# Add banner to make sure protocol ignores it\n" .
            "RUN echo '***********************************************' >  /etc/issue.net\n" .
            "RUN echo 'Sample banner to make sure banners are skipped.' >> /etc/issue.net\n" .
            "RUN echo ''                                                >> /etc/issue.net\n" .
            "RUN echo 'More banner after a blank line.'                 >> /etc/issue.net\n" .
            "RUN echo '***********************************************' >> /etc/issue.net\n" .
            "RUN echo 'Banner /etc/issue.net'                           >> /etc/ssh/sshd_config";

        # Create PostgreSQL Group
        $strScript .= "\n\n" . postgresGroupCreate($strOS);

        # Create test user
        $strScript .=
            "\n\n# Create test user\n" .
            userCreate($strOS, TEST_USER, TEST_USER_ID, TEST_GROUP) . "\n" .
            'RUN mkdir -m 750 /home/' . TEST_USER . "/test\n" .
            'RUN chown ' . TEST_USER . ':' . TEST_GROUP . ' /home/' . TEST_USER . '/test';

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
        containerWrite($strTempPath, $strOS, 'Base', $strImageParent, $strImage, $strScript, $bVmForce, false);

        # Base pre image
        ###########################################################################################################################
        $strImageParent = containerNamespace() . "/${strOS}-base";
        $strImage = "${strOS}-base-pre";

        # Install Perl packages
        $strScript =
            perlInstall($strOS) . "\n";

        # Write the image
        containerWrite($strTempPath, $strOS, 'Base (Pre-Installed)', $strImageParent, $strImage, $strScript, $bVmForce, false);

        # Build image
        ###########################################################################################################################
        $strImageParent = containerNamespace() . "/${strOS}-base-pre";
        $strImage = "${strOS}-build";

        # Install Perl packages
        if ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_DEBIAN)
        {
            $strScript =
                "RUN apt-get install -y gcc make\n";
        }
        else
        {
            $strScript =
                "RUN yum install -y gcc make perl-ExtUtils-MakeMaker perl-Test-Simple\n";
        }

        # Write the image
        containerWrite($strTempPath, $strOS, 'Build', $strImageParent, $strImage, $strScript, $bVmForce, false);

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
        else
        {
            push(@stryDbBuild, $strDbVersionBuild);
        }

        # Merge db versions required for docs
        if (defined($$oOS{&VM_DB_DOC}))
        {
            @stryDbBuild = keys %{{map {($_ => 1)}                  ## no critic (BuiltinFunctions::ProhibitVoidMap)
                (@stryDbBuild, @{$$oOS{&VM_DB_DOC}})}};
            $bDocBuild = true;
        }

        foreach my $strDbVersion (sort(@stryDbBuild))
        {
            my $strDbVersionNoDot = $strDbVersion;
            $strDbVersionNoDot =~ s/\.//;
            my $strTitle = "Db ${strDbVersion}";

            my $bDocBuildVersion = ($bDocBuild && grep(/^$strDbVersion$/, @{$$oOS{&VM_DB_DOC}}));

            $strImageParent = containerNamespace() . "/${strOS}-base";
            $strImage = "${strOS}-db-${strDbVersion}";

            # Create PostgreSQL User
            $strScript = postgresUserCreate($strOS);

            # Install PostgreSQL
            $strScript .=
                "\n\n# Install PostgreSQL";

            if ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_DEBIAN)
            {
                $strScript .=
                    "\nRUN apt-get install -y postgresql-${strDbVersion}" .
                    "\nRUN pg_dropcluster --stop ${strDbVersion} main";
            }
            elsif ($$oVm{$strOS}{&VM_OS_BASE} eq VM_OS_BASE_RHEL)
            {
                $strScript .=
                    "\nRUN yum -y install postgresql${strDbVersionNoDot}-server";
            }

            # Write the image
            containerWrite($strTempPath, $strOS, $strTitle, $strImageParent, $strImage, $strScript, $bVmForce,
                           $bDocBuildVersion ? undef : true, $bDocBuildVersion ? undef : true);

            # Db doc image
            ########################################################################################################################
            if ($bDocBuildVersion)
            {
                $strImageParent = containerNamespace() . "/${strOS}-db-${strDbVersion}";
                $strImage = "${strOS}-db-${strDbVersion}-doc";

                # Install SSH key
                $strScript = sshSetup($strOS, POSTGRES_USER, POSTGRES_GROUP, $$oVm{$strOS}{&VM_CONTROL_MASTER});

                # Setup sudo
                $strScript .= "\n\n" . sudoSetup($strOS, TEST_GROUP);

                # Write the image
                containerWrite($strTempPath, $strOS, "${strTitle} Doc", $strImageParent, $strImage, $strScript, $bVmForce);
            }

            # Db test image
            ########################################################################################################################
            $strImageParent = containerNamespace() . "/${strOS}-db-${strDbVersion}";
            $strImage = "${strOS}-db-${strDbVersion}-test";

            # Install SSH key
            $strScript = sshSetup($strOS, TEST_USER, TEST_GROUP, $$oVm{$strOS}{&VM_CONTROL_MASTER});

            # Write the image
            containerWrite($strTempPath, $strOS, "${strTitle} Test", $strImageParent, $strImage, $strScript, $bVmForce, true, true);
        }

        # Db test image (for synthetic tests)
        ########################################################################################################################
        $strImageParent = containerNamespace() . "/${strOS}-base";
        $strImage = "${strOS}-db-test";

        # Install SSH key
        $strScript = sshSetup($strOS, TEST_USER, TEST_GROUP, $$oVm{$strOS}{&VM_CONTROL_MASTER});

        # Write the image
        containerWrite($strTempPath, $strOS, 'Db Synthetic Test', $strImageParent, $strImage, $strScript, $bVmForce, true, true);

        # Loop test image
        ########################################################################################################################
        $strImageParent = containerNamespace() . "/${strOS}-base";
        $strImage = "${strOS}-loop-test";

        # Create BackRest User
        $strScript = backrestUserCreate($strOS);

        # Install SSH key
        $strScript .=
            "\n\n" . sshSetup($strOS, BACKREST_USER, BACKREST_GROUP, $$oVm{$strOS}{&VM_CONTROL_MASTER});

        # Install SSH key
        $strScript .=
            "\n\n" . sshSetup($strOS, TEST_USER, TEST_GROUP, $$oVm{$strOS}{&VM_CONTROL_MASTER});

        # Make test user home readable
        $strScript .=
            "\n\n# Make " . TEST_USER . " home dir readable\n" .
            'RUN chmod g+r,g+x /home/' . TEST_USER;

        # Setup sudo
        $strScript .= "\n\n" . sudoSetup($strOS, TEST_GROUP);

        # Write the image
        containerWrite($strTempPath, $strOS, 'Loop Test', $strImageParent, $strImage, $strScript, $bVmForce, true, true);

        # Backup image
        ###########################################################################################################################
        $strImageParent = containerNamespace() . "/${strOS}-base";
        $strImage = "${strOS}-backup";
        my $strTitle = "Backup";

        # Create BackRest User
        $strScript = backrestUserCreate($strOS);

        # Install SSH key
        $strScript .=
            "\n\n" . sshSetup($strOS, BACKREST_USER, BACKREST_GROUP, $$oVm{$strOS}{&VM_CONTROL_MASTER});

        # Write the image
        containerWrite($strTempPath, $strOS, $strTitle, $strImageParent, $strImage, $strScript, $bVmForce,
                       $bDocBuild ? undef : true, $bDocBuild ? undef : true);

        # Backup Doc image
        ###########################################################################################################################
        if ($bDocBuild)
        {
            $strImageParent = containerNamespace() . "/${strOS}-backup";
            $strImage = "${strOS}-backup-doc";

            # Create configuration file
            $strScript = backrestConfigCreate($strOS, BACKREST_USER, BACKREST_GROUP);

            # Setup repository
            $strScript .=
                "\n\n" . repoSetup($strOS, BACKREST_USER, BACKREST_GROUP);

            # Setup sudo
            $strScript .= "\n\n" . sudoSetup($strOS, TEST_GROUP);

            # Write the image
            containerWrite($strTempPath, $strOS, "${strTitle} Doc", $strImageParent, $strImage, $strScript, $bVmForce, true, true);
        }

        # Backup Test image
        ###########################################################################################################################
        $strImageParent = containerNamespace() . "/${strOS}-backup";
        $strImage = "${strOS}-backup-test";

        # Make test user home readable
        $strScript =
            "# Make " . TEST_USER . " home dir readable\n" .
            'RUN chmod g+r,g+x /home/' . TEST_USER;

        # Write the image
        containerWrite($strTempPath, $strOS, "${strTitle} Test", $strImageParent, $strImage, $strScript, $bVmForce, true, true);
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

    foreach my $strImage (sort(split("\n", trim(executeTest('docker images --format "{{.Repository}}"')))))
    {
        if ($strImage =~ $strRegExp)
        {
            &log(INFO, "Removing ${strImage} image...");
            executeTest("docker rmi -f ${strImage}", {bSuppressError => true});
        }
    }
}

1;
