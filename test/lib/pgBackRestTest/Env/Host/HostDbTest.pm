####################################################################################################################################
# HostDbTest.pm - Database host
####################################################################################################################################
package pgBackRestTest::Env::Host::HostDbTest;
use parent 'pgBackRestTest::Env::Host::HostDbCommonTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use DBI;
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(basename);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Common::Wait;
use pgBackRest::DbVersion;
use pgBackRest::Manifest;
use pgBackRest::Storage::Helper;
use pgBackRest::Version;

use pgBackRestTest::Env::Host::HostBackupTest;
use pgBackRestTest::Env::Host::HostBaseTest;
use pgBackRestTest::Env::Host::HostDbCommonTest;
use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# Db defaults
####################################################################################################################################
use constant HOST_DB_DEFAULT                                        => 'postgres';
use constant HOST_DB_TIMEOUT                                        => 30;

####################################################################################################################################
# new
####################################################################################################################################
sub new
{
    my $class = shift;          # Class name

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'oParam', required => false, trace => true},
        );

    # Get db version
    my $strDbVersion = testRunGet()->pgVersion();

    my $self = $class->SUPER::new(
        {
            strImage => containerRepo() . ':' . testRunGet()->vm() . "-test",
            strBackupDestination => $$oParam{strBackupDestination},
            oLogTest => $$oParam{oLogTest},
            bStandby => $$oParam{bStandby},
            bRepoLocal => $oParam->{bRepoLocal},
            bRepoEncrypt => $oParam->{bRepoEncrypt},
        });
    bless $self, $class;

    # Set parameters
    $self->{strPgSocketPath} = $self->dbPath();
    $self->{iPgPort} = defined($$oParam{bStandby}) && $$oParam{bStandby} ? 6544 : 6543;

    $self->{strPgLogPath} = $self->testPath();
    $self->{strPgLogFile} = $self->pgLogPath() . '/postgresql.log';

    # Get Db version
    if (defined($strDbVersion))
    {
        my $strOutLog = $self->executeSimple($self->pgBinPath() . '/postgres --version');

        my @stryVersionToken = split(/ /, $strOutLog);
        @stryVersionToken = split(/\./, $stryVersionToken[2]);
        my $strDbVersionActual =
            trim($stryVersionToken[0]) .
            (defined($stryVersionToken[1]) && trim($stryVersionToken[0]) < 10 ? '.' . trim($stryVersionToken[1]) : '');

        # Warn if this is a devel/alpha/beta version
        my $strVersionRegExp = '(devel|((alpha|beta|rc)[0-9]+))$';

        if ($strDbVersionActual =~ /$strVersionRegExp/)
        {
            my $strDevVersion = $strDbVersionActual;
            $strDbVersionActual =~ s/$strVersionRegExp//;
            $strDevVersion = substr($strDevVersion, length($strDbVersionActual));

            if (!defined($$oParam{bStandby}) || !$$oParam{bStandby})
            {
                &log(WARN, "Testing against ${strDbVersionActual} ${strDevVersion} version");
            }
        }

        # Don't run unit tests for unsupported versions
        my @stryVersionSupport = versionSupport();

        if ($strDbVersionActual < $stryVersionSupport[0])
        {
            confess &log(ERROR, "only PostgreSQL version $stryVersionSupport[0] and up are supported");
        }

        if ($strDbVersion ne $strDbVersionActual)
        {
            confess &log(ERROR, "actual database version ${strDbVersionActual} does not match expected version ${strDbVersion}");
        }
    }

    # Create wal directory
    storageTest()->pathCreate($self->dbPath() . '/pg_' . $self->walId(), {strMode => '0700'});

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self, trace => true}
    );
}

####################################################################################################################################
# sqlConnect
####################################################################################################################################
sub sqlConnect
{
    my $self = shift;
    my $hParam = shift;

    # Set defaults
    my $iTimeout = defined($$hParam{iTimeout}) ? $$hParam{iTimeout} : HOST_DB_TIMEOUT;
    my $strDb = defined($$hParam{strDb}) ? $$hParam{strDb} : HOST_DB_DEFAULT;

    # If not connected
    if (!defined($self->{db}{$strDb}{hDb}))
    {
        # Retry until connection is successful
        my $oWait = waitInit($iTimeout);

        do
        {
            # Connect to the db (whether it is local or remote)
            $self->{db}{$strDb}{hDb} =
                DBI->connect(
                    "dbi:Pg:dbname=${strDb};port=" . $self->pgPort() . ';host=' . $self->pgSocketPath(),
                    $self->userGet(), undef,
                    {AutoCommit => 0, RaiseError => 0, PrintError => 0});

            return $self->{db}{$strDb}{hDb} if $self->{db}{$strDb}{hDb};
        }
        while (!defined($self->{db}{$strDb}{hDb}) && waitMore($oWait));

        # Error if unable to connect
        if (!defined($self->{db}{$strDb}{hDb}))
        {
            confess &log(ERROR, "unable to connect to PostgreSQL after ${iTimeout} second(s):\n" . $DBI::errstr, ERROR_DB_CONNECT);
        }
    }

    return $self->{db}{$strDb}{hDb};
}

####################################################################################################################################
# sqlDisconnect
####################################################################################################################################
sub sqlDisconnect
{
    my $self = shift;
    my $hParam = shift;

    foreach my $strDb (keys(%{$self->{db}}))
    {
        if (defined($$hParam{$strDb}) && $$hParam{$strDb} ne $strDb)
        {
            next;
        }

        if (defined($self->{db}{$strDb}{hDb}))
        {
            $self->{db}{$strDb}{hDb}->disconnect();
            undef($self->{db}{$strDb}{hDb});
        }
    }
}

####################################################################################################################################
# sqlExecute
####################################################################################################################################
sub sqlExecute
{
    my $self = shift;
    my $strSql = shift;
    my $hParam = shift;

    # Set defaults
    my $bCheckPoint = defined($$hParam{bCheckPoint}) ? $$hParam{bCheckPoint} : false;
    my $bCommit = defined($$hParam{bCommit}) ? $$hParam{bCommit} : true;

    # Get the db handle
    my $hDb = $self->sqlConnect({strDb => $$hParam{strDb}});

    # Set autocommit on/off
    $hDb->{AutoCommit} = defined($$hParam{bAutoCommit}) ? ($$hParam{bAutoCommit} ? true : false) : false;

    # Log and execute the statement
    &log(DETAIL, "SQL: ${strSql}");

    my $hStatement = $hDb->prepare($strSql);

    $hStatement->execute() or
        confess &log(ERROR, "Unable to execute: ${strSql}\n" . $DBI::errstr);
    $hStatement->finish();

    if ($bCommit && !$hDb->{AutoCommit})
    {
        $self->sqlCommit();
    }

    # Perform a checkpoint if requested
    if ($bCheckPoint)
    {
        $self->sqlExecute('checkpoint', {bCommit => false, bCheckPoint => false});
    }

    # Set autocommit off
    $hDb->{AutoCommit} = 0;
}

####################################################################################################################################
# sqlSelect
####################################################################################################################################
sub sqlSelect
{
    my $self = shift;
    my $strSql = shift;
    my $hParam = shift;

    # Get the db handle
    my $hDb = $self->sqlConnect({strDb => $$hParam{strDb}});

    # Log and execute the statement
    &log(DEBUG, (defined($$hParam{strDb}) ? "DB: $$hParam{strDb}, " : "") . "SQL: ${strSql}");
    my $hStatement = $hDb->prepare($strSql);

    $hStatement = $hDb->prepare($strSql);

    $hStatement->execute() or
        confess &log(ERROR, "Unable to execute: ${strSql}\n" . $DBI::errstr);

    my @oyRow = $hStatement->fetchrow_array();

    $hStatement->finish();

    return @oyRow;
}

####################################################################################################################################
# sqlSelectOne
####################################################################################################################################
sub sqlSelectOne
{
    my $self = shift;
    my $strSql = shift;
    my $hParam = shift;

    return ($self->sqlSelect($strSql, $hParam))[0];
}

####################################################################################################################################
# sqlSelectOneTest
####################################################################################################################################
sub sqlSelectOneTest
{
    my $self = shift;
    my $strSql = shift;
    my $strExpectedValue = shift;
    my $hParam = shift;

    # Set defaults
    my $iTimeout = defined($$hParam{iTimeout}) ? $$hParam{iTimeout} : HOST_DB_TIMEOUT;

    my $lStartTime = time();
    my $strActualValue;

    do
    {
        $self->sqlConnect($hParam);
        $strActualValue = $self->sqlSelectOne($strSql, $hParam);

        if (defined($strActualValue) && $strActualValue eq $strExpectedValue)
        {
            return;
        }

        $self->sqlDisconnect();
    }
    while (defined($iTimeout) && (time() - $lStartTime) <= $iTimeout);

    confess &log(
        ERROR, "expected value '${strExpectedValue}' from '${strSql}' but actual was '" .
        (defined($strActualValue) ? $strActualValue : '[undef]') . "'");
}

####################################################################################################################################
# sqlCommit
####################################################################################################################################
sub sqlCommit
{
    my $self = shift;
    my $hParam = shift;

    my $bCheckPoint = defined($$hParam{bCheckPoint}) ? $$hParam{bCheckPoint} : false;

    $self->sqlExecute('commit', {bCommit => false, bCheckPoint => $bCheckPoint});
}

####################################################################################################################################
# sqlWalRotate
####################################################################################################################################
sub sqlWalRotate
{
    my $self = shift;

    $self->sqlExecute('select pg_switch_'  . $self->walId() . '()', {bCommit => false, bCheckPoint => false});
}

####################################################################################################################################
# clusterCreate
#
# Create the PostgreSQL cluster and start it.
####################################################################################################################################
sub clusterCreate
{
    my $self = shift;
    my $hParam = shift;

    # Set defaults
    my $strWalPath = defined($$hParam{strWalPath}) ? $$hParam{strWalPath} : $self->dbPath() . '/pg_' . $self->walId();

    # Don't link WAL directory for versions < 9.2 because some recovery scenarios won't work
    $self->executeSimple(
        $self->pgBinPath() . '/initdb ' .
        ($self->pgVersion() >= PG_VERSION_93 ? ' -k' : '') .
        ($self->pgVersion() >= PG_VERSION_92 ? ' --' . $self->walId() . "dir=${strWalPath}" : '') .
        ($self->pgVersion() >= PG_VERSION_11 ? ' --wal-segsize=1' : '') .
        ' --pgdata=' . $self->dbBasePath() . ' --auth=trust');

    if (!$self->standby() && $self->pgVersion() >= PG_VERSION_HOT_STANDBY)
    {
        $self->executeSimple(
            "echo 'host replication replicator db-standby trust' >> " . $self->dbBasePath() . '/pg_hba.conf');
    }

    $self->clusterStart(
        {bHotStandby => $$hParam{bHotStandby}, bArchive => $$hParam{bArchive}, bArchiveAlways => $$hParam{bArchiveAlways},
         bArchiveInvalid => $$hParam{bArchiveInvalid}});

    if (!$self->standby() && $self->pgVersion() >= PG_VERSION_HOT_STANDBY)
    {
        $self->sqlExecute("create user replicator replication", {bCommit =>true});
    }
}

####################################################################################################################################
# clusterStart
#
# Start the PostgreSQL cluster with various test options.
####################################################################################################################################
sub clusterStart
{
    my $self = shift;
    my $hParam = shift;

    # Set defaults
    my $bHotStandby = defined($$hParam{bHotStandby}) ? $$hParam{bHotStandby} : false;
    my $bArchive = defined($$hParam{bArchive}) ? $$hParam{bArchive} : true;
    my $bArchiveAlways = defined($$hParam{bArchiveAlways}) ? $$hParam{bArchiveAlways} : false;
    my $bArchiveInvalid = defined($$hParam{bArchiveInvalid}) ? $$hParam{bArchiveInvalid} : false;
    my $bArchiveEnabled = defined($$hParam{bArchiveEnabled}) ? $$hParam{bArchiveEnabled} : true;

    # Make sure postgres is not running
    if (-e $self->dbBasePath() . '/postmaster.pid')
    {
        confess 'postmaster.pid exists';
    }

    # Create the archive command
    my $strArchive =
        $self->backrestExe() . ' --stanza=' . ($bArchiveInvalid ? 'bogus' : $self->stanza()) .
        ' --config=' . $self->backrestConfig() . ' archive-push %p';

    # Start the cluster
    my $strCommand =
        $self->pgBinPath() . '/pg_ctl start -o "-c port=' . $self->pgPort() .
        ($self->pgVersion() < PG_VERSION_95 ? ' -c checkpoint_segments=1' : '');

    if ($bArchiveEnabled)
    {
        if ($self->pgVersion() >= PG_VERSION_95 && $bArchiveAlways)
        {
            $strCommand .= " -c archive_mode=always";
        }
        else
        {
            $strCommand .= " -c archive_mode=on";
        }
    }
    else
    {
        $strCommand .= " -c archive_mode=off";
    }

    if ($bArchive)
    {
        $strCommand .= " -c archive_command='${strArchive}'";
    }
    else
    {
        $strCommand .= " -c archive_command=true";
    }

    if ($self->pgVersion() >= PG_VERSION_90)
    {
        $strCommand .= ' -c wal_level=hot_standby -c hot_standby=' . ($bHotStandby ? 'on' : 'off');
    }

    $strCommand .=
        ($self->pgVersion() >= PG_VERSION_HOT_STANDBY ? ' -c max_wal_senders=3' : '') .
        ' -c listen_addresses=\'*\'' .
        ' -c log_directory=\'' . $self->pgLogPath() . "'" .
        ' -c log_filename=\'' . basename($self->pgLogFile()) . "'" .
        ' -c log_rotation_age=0' .
        ' -c log_rotation_size=0' .
        ' -c log_error_verbosity=verbose' .
        ' -c unix_socket_director' . ($self->pgVersion() < PG_VERSION_93 ? 'y=\'' : 'ies=\'') . $self->dbPath() . '\'"' .
        ' -D ' . $self->dbBasePath() . ' -l ' . $self->pgLogFile() . ' -s';

    $self->executeSimple($strCommand);

    # Connect user session
    $self->sqlConnect();
}

####################################################################################################################################
# clusterStop
#
# Stop the PostgreSQL cluster and optionally check for errors in the server log.
####################################################################################################################################
sub clusterStop
{
    my $self = shift;
    my $hParam = shift;

    # Set defaults
    my $bIgnoreLogError = defined($$hParam{bIgnoreLogError}) ? $$hParam{bIgnoreLogError} : false;

    # Disconnect user session
    $self->sqlDisconnect();

    # Grep for errors in postgresql.log - this is done first because we want to ignore any errors that happen during shutdown
    if (!$bIgnoreLogError && storageTest()->exists($self->pgLogFile()))
    {
        $self->executeSimple(
            'grep -v "FATAL\:  57P03\: the database system is starting up" ' . $self->pgLogFile() . ' | grep "ERROR\|FATAL"',
            {iExpectedExitStatus => 1});
    }

    # If postmaster process is running then stop the cluster
    if (-e $self->dbBasePath() . '/' . DB_FILE_POSTMASTERPID)
    {
        $self->executeSimple($self->pgBinPath() . '/pg_ctl stop -D ' . $self->dbBasePath() . ' -w -s -m fast');
    }

    # Remove the log file
    storageTest()->remove($self->pgLogFile(), {bIgnoreMissing => true});
}

####################################################################################################################################
# clusterRestart
#
# Restart the PostgreSQL cluster.
####################################################################################################################################
sub clusterRestart
{
    my $self = shift;
    my $hParam = shift;

    $self->clusterStop($hParam);
    $self->clusterStart($hParam);
}

####################################################################################################################################
# Getters
####################################################################################################################################
sub walId {return shift->pgVersion() >= PG_VERSION_10 ? 'wal' : 'xlog'}
sub pgBinPath {return testRunGet()->pgBinPath()}
sub pgLogFile {return shift->{strPgLogFile}}
sub pgLogPath {return shift->{strPgLogPath}}
sub pgPort {return shift->{iPgPort}}
sub pgSocketPath {return shift->{strPgSocketPath}}
sub pgVersion {return testRunGet()->pgVersion()}

1;
