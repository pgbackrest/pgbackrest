####################################################################################################################################
# DB MODULE
####################################################################################################################################
package BackRest::Db;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use DBD::Pg ':async';
use DBI;
use Exporter qw(import);
    our @EXPORT =  qw();
use Fcntl qw(O_RDONLY);
use File::Basename qw(dirname);

use lib dirname($0);
use BackRest::Config;
use BackRest::Exception;
use BackRest::File;
use BackRest::Utility;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_DB                                                  => 'Db';

use constant OP_DB_NEW                                              => OP_DB . "->new";
use constant OP_DB_BACKUP_START                                     => OP_DB . "->backupStart";
use constant OP_DB_BACKUP_STOP                                      => OP_DB . "->backupStop";
use constant OP_DB_EXECUTE_SQL                                      => OP_DB . "->executeSql";
    push @EXPORT, qw(OP_DB_EXECUTE_SQL);
use constant OP_DB_INFO                                             => OP_DB . "->info";
    push @EXPORT, qw(OP_DB_INFO);
use constant OP_DB_TABLESPACE_MAP_GET                               => OP_DB . "->tablespaceMapGet";
use constant OP_DB_VERSION_GET                                      => OP_DB . "->versionGet";

####################################################################################################################################
# Postmaster process Id file
####################################################################################################################################
use constant FILE_POSTMASTER_PID                                    => 'postmaster.pid';
    push @EXPORT, qw(FILE_POSTMASTER_PID);

####################################################################################################################################
# Backup advisory lock
####################################################################################################################################
use constant DB_BACKUP_ADVISORY_LOCK                                => '12340078987004321';
    push @EXPORT, qw(DB_BACKUP_ADVISORY_LOCK);

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;          # Class name

    # Create the class hash
    my $self = {};
    bless $self, $class;

    return $self;
}

####################################################################################################################################
# DESTRUCTOR
####################################################################################################################################
sub DESTROY
{
    my $self = shift;

    if (defined($self->{hDb}))
    {
        $self->{hDb}->disconnect();
        undef($self->{hDb});
    }
}

####################################################################################################################################
# versionSupport
#
# Returns an array of the supported Postgres versions.
####################################################################################################################################
sub versionSupport
{
    my @strySupportVersion = ('8.3', '8.4', '9.0', '9.1', '9.2', '9.3', '9.4', '9.5');

    return \@strySupportVersion;
}

push @EXPORT, qw(versionSupport);

####################################################################################################################################
# executeSql
####################################################################################################################################
sub executeSql
{
    my $self = shift;
    my $strSql = shift;
    my $bIgnoreError = shift;

    logDebug(OP_DB_EXECUTE_SQL, DEBUG_CALL, undef, {isRemote => optionRemoteTypeTest(DB), script => \$strSql});

    # Get the user-defined command for psql
    my $strResult;

    # Run remotely
    if (optionRemoteTypeTest(DB))
    {
        # Build param hash
        my %oParamHash;

        $oParamHash{'script'} = $strSql;

        if (defined($bIgnoreError) && $bIgnoreError)
        {
            $oParamHash{'ignore-error'} = true;
        }

        # Execute the command
        $strResult = protocolGet()->cmdExecute(OP_DB_EXECUTE_SQL, \%oParamHash, true);
    }
    # Else run locally
    else
    {
        if (!defined($self->{hDb}))
        {
            # Connect to the db
            my $strDbName = 'postgres';
            my $strDbUser = getpwuid($<);
            my $strDbSocketPath = optionGet(OPTION_DB_SOCKET_PATH, false);

            if (defined($strDbSocketPath) && $strDbSocketPath !~ /^\//)
            {
                confess &log(ERROR, "'${strDbSocketPath}' is not valid for '" . OPTION_DB_SOCKET_PATH . "' option:" .
                                    " path must be absolute", ERROR_OPTION_INVALID_VALUE);
            }

            my $strDbUri = "dbi:Pg:dbname=${strDbName};port=" . optionGet(OPTION_DB_PORT) .
                           (optionTest(OPTION_DB_SOCKET_PATH) ? ';host=' . optionGet(OPTION_DB_SOCKET_PATH) : '');

            logDebug(OP_DB_EXECUTE_SQL, DEBUG_MISC, 'db connect', {strDbUri => $strDbUri, strDbUser => $strDbUser});

            $self->{hDb} = DBI->connect($strDbUri, $strDbUser, undef, {AutoCommit => 1, RaiseError => 0, PrintError => 0});

            if (!$self->{hDb})
            {
                confess &log(ERROR, $DBI::errstr, ERROR_DB_CONNECT);
            }
        }

        # Prepare the query
        my $hStatement = $self->{hDb}->prepare($strSql, {pg_async => PG_ASYNC})
            or confess &log(ERROR, $DBI::errstr, ERROR_DB_QUERY);

        # Execute the query
        $hStatement->execute();

        # Wait for the query to return
        my $iWaitSeconds = optionGet(OPTION_DB_TIMEOUT);
        my $oExecuteWait = waitInit($iWaitSeconds);
        my $bTimeout = true;

        do
        {
            # Is the statement done?
            if ($hStatement->pg_ready())
            {
                if (!$hStatement->pg_result())
                {
                    # Return if the error should be ignored
                    if (defined($bIgnoreError) && $bIgnoreError)
                    {
                        return '';
                    }

                    # Else report it
                    confess &log(ERROR, $DBI::errstr . ":\n${strSql}", ERROR_DB_QUERY);
                }

                # Get rows and return them
                my @stryArray;

                do
                {
                    @stryArray = $hStatement->fetchrow_array;

                    if (!@stryArray && $hStatement->err)
                    {
                        confess &log(ERROR, $DBI::errstr . ":\n${strSql}", ERROR_DB_QUERY);
                    }

                    $strResult = (defined($strResult) ? "${strResult}\n" : '') . join("\t", @stryArray);
                }
                while (@stryArray);

                $bTimeout = false;
            }
        } while ($bTimeout && waitMore($oExecuteWait));

        # If timeout then cancel the query and confess
        if ($bTimeout)
        {
            $hStatement->pg_cancel();
            confess &log(ERROR, "statement timed out after ${iWaitSeconds} second(s):\n${strSql}", ERROR_DB_TIMEOUT);
        }
    }

    logDebug(OP_DB_EXECUTE_SQL, DEBUG_RESULT, undef, {strResult => $strResult});

    return $strResult;
}

####################################################################################################################################
# executeSqlRow
####################################################################################################################################
sub executeSqlRow
{
    my $self = shift;
    my $strSql = shift;

    return split("\t", trim($self->executeSql($strSql)));
}

####################################################################################################################################
# executeSqlOne
####################################################################################################################################
sub executeSqlOne
{
    my $self = shift;
    my $strSql = shift;

    return ($self->executeSqlRow($strSql))[0];
}

####################################################################################################################################
# tablespaceMapGet
#
# Get the mapping between oid and tablespace name.
####################################################################################################################################
sub tablespaceMapGet
{
    my $self = shift;

    logTrace(OP_DB_TABLESPACE_MAP_GET, DEBUG_CALL);

    my $oHashRef = {};

    data_hash_build($oHashRef, "oid\tname\n" . $self->executeSql(
                    'select oid, spcname from pg_tablespace'), "\t");

    return $oHashRef;
}

####################################################################################################################################
# info
####################################################################################################################################
sub info
{
    my $self = shift;
    my $oFile = shift;
    my $strDbPath = shift;

    logDebug(OP_DB_INFO, DEBUG_CALL, undef, {isRemote => $oFile->is_remote(PATH_DB_ABSOLUTE), dbPath => \$strDbPath});

    # Return data from the cache if it exists
    if (defined($self->{info}{$strDbPath}))
    {
        return $self->{info}{$strDbPath}{fDbVersion},
               $self->{info}{$strDbPath}{iControlVersion},
               $self->{info}{$strDbPath}{iCatalogVersion},
               $self->{info}{$strDbPath}{ullDbSysId};
    }

    # Database info
    my $iCatalogVersion;
    my $iControlVersion;
    my $ullDbSysId;
    my $fDbVersion;

    if ($oFile->is_remote(PATH_DB_ABSOLUTE))
    {
        # Build param hash
        my %oParamHash;

        $oParamHash{'db-path'} = ${strDbPath};

        # Output remote trace info
        &log(TRACE, OP_DB_INFO . ": remote (" . $oFile->{oProtocol}->commandParamString(\%oParamHash) . ')');

        # Execute the command
        my $strResult = $oFile->{oProtocol}->cmdExecute(OP_DB_INFO, \%oParamHash, true);

        # Split the result into return values
        my @stryToken = split(/\t/, $strResult);

        $fDbVersion = $stryToken[0];
        $iControlVersion = $stryToken[1];
        $iCatalogVersion = $stryToken[2];
        $ullDbSysId = $stryToken[3];
    }
    else
    {
        # Open the control file
        my $strControlFile = "${strDbPath}/global/pg_control";
        my $hFile;
        my $tBlock;

        sysopen($hFile, $strControlFile, O_RDONLY)
            or confess &log(ERROR, "unable to open ${strControlFile}", ERROR_FILE_OPEN);

        # Read system identifier
        sysread($hFile, $tBlock, 8) == 8
            or confess &log(ERROR, "unable to read database system identifier");

        $ullDbSysId = unpack('Q', $tBlock);

        # Read control version
        sysread($hFile, $tBlock, 4) == 4
            or confess &log(ERROR, "unable to read control version");

        $iControlVersion = unpack('L', $tBlock);

        # Read catalog version
        sysread($hFile, $tBlock, 4) == 4
            or confess &log(ERROR, "unable to read catalog version");

        $iCatalogVersion = unpack('L', $tBlock);

        # Close the control file
        close($hFile);

        # Make sure the control version is supported
        if ($iControlVersion == 942 && $iCatalogVersion == 201409291)
        {
            $fDbVersion = '9.4';
        }
        # Leave 9.5 catalog version out until it stabilizes (then move 9.5 to the top of the list)
        elsif ($iControlVersion == 942) # && $iCatalogVersion == 201505311)
        {
            $fDbVersion = '9.5';
        }
        elsif ($iControlVersion == 937 && $iCatalogVersion == 201306121)
        {
            $fDbVersion = '9.3';
        }
        elsif ($iControlVersion == 922 && $iCatalogVersion == 201204301)
        {
            $fDbVersion = '9.2';
        }
        elsif ($iControlVersion == 903 && $iCatalogVersion == 201105231)
        {
            $fDbVersion = '9.1';
        }
        elsif ($iControlVersion == 903 && $iCatalogVersion == 201008051)
        {
            $fDbVersion = '9.0';
        }
        elsif ($iControlVersion == 843 && $iCatalogVersion == 200904091)
        {
            $fDbVersion = '8.4';
        }
        elsif ($iControlVersion == 833 && $iCatalogVersion == 200711281)
        {
            $fDbVersion = '8.3';
        }
        elsif ($iControlVersion == 822 && $iCatalogVersion == 200611241)
        {
            $fDbVersion = '8.2';
        }
        elsif ($iControlVersion == 812 && $iCatalogVersion == 200510211)
        {
            $fDbVersion = '8.1';
        }
        elsif ($iControlVersion == 74 && $iCatalogVersion == 200411041)
        {
            $fDbVersion = '8.0';
        }
        else
        {
            confess &log(ERROR, "unexpected control version = ${iControlVersion} and catalog version = ${iCatalogVersion}" .
                                ' (unsupported PostgreSQL version?)',
                         ERROR_VERSION_NOT_SUPPORTED);
        }
    }

    # Store data in the cache
    $self->{info}{$strDbPath}{fDbVersion} = $fDbVersion;
    $self->{info}{$strDbPath}{iControlVersion} = $iControlVersion;
    $self->{info}{$strDbPath}{iCatalogVersion} = $iCatalogVersion;
    $self->{info}{$strDbPath}{ullDbSysId} = $ullDbSysId;

    &log(DEBUG, OP_DB_INFO . "=>: dbVersion = ${fDbVersion}, controlVersion = ${iControlVersion}" .
                ", catalogVersion = ${iCatalogVersion}, dbSysId = ${ullDbSysId}");

    return $fDbVersion, $iControlVersion, $iCatalogVersion, $ullDbSysId;
}

####################################################################################################################################
# versionGet
####################################################################################################################################
sub versionGet
{
    my $self = shift;

    # Get data from the cache if possible
    if (defined($self->{fVersion}) && defined($self->{strDbPath}))
    {
        return $self->{fVersion}, $self->{strDbPath};
    }

    # Get version and db-path from
    ($self->{fVersion}, $self->{strDbPath}) =
        $self->executeSqlRow("select (regexp_matches(split_part(version(), ' ', 2), '^[0-9]+\.[0-9]+'))[1], setting" .
                             " from pg_settings where name = 'data_directory'");

    my $strVersionSupport = versionSupport();

    if ($self->{fVersion} < ${$strVersionSupport}[0])
    {
        confess &log(ERROR, "unsupported Postgres version ${$strVersionSupport}[0]", ERROR_VERSION_NOT_SUPPORTED);
    }

    logDebug(OP_DB_VERSION_GET, DEBUG_RESULT, {dbVersion => $self->{fVersion}});

    return $self->{fVersion}, $self->{strDbPath};
}

####################################################################################################################################
# backupStart
####################################################################################################################################
sub backupStart
{
    my $self = shift;
    my $oFile = shift;
    my $strDbPath = shift;
    my $strLabel = shift;
    my $bStartFast = shift;

    logDebug(OP_DB_BACKUP_START, DEBUG_CALL, undef, {dbPath => \$strDbPath, strLabel => \$strLabel, isStartFast => $bStartFast});

    # Get the version from the control file
    my ($fDbVersion) = $self->info($oFile, $strDbPath);

    # Get version and db path from the database
    my ($fCompareDbVersion, $strCompareDbPath) = $self->versionGet();

    # Error if they are not identical
    if (!($fDbVersion == $fCompareDbVersion && $strDbPath eq $strCompareDbPath))
    {
        confess &log(ERROR, "version '${fCompareDbVersion}' and db-path '${strCompareDbPath}' queried from cluster does not match" .
                            " version '${fDbVersion}' and db-path '${strDbPath}' read from '${strDbPath}/global/pg_control'\n" .
                            "HINT: the db-path and db-port settings likely reference different clusters", ERROR_DB_MISMATCH);
    }

    # Only allow start-fast option for version >= 8.4
    if ($self->{fVersion} < 8.4 && $bStartFast)
    {
        &log(WARN, OPTION_START_FAST . ' option is only available in PostgreSQL >= 8.4');
        $bStartFast = false;
    }

    # Acquire the backup advisory lock
    if (!$self->executeSqlOne('select pg_try_advisory_lock(' . DB_BACKUP_ADVISORY_LOCK . ')'))
    {
        confess &log(ERROR, "unable to acquire backup lock\n" .
                            'HINT: is another backup already running on this cluster?', ERROR_LOCK_ACQUIRE);
    }

    # Test if a backup is already running when version >= 9.3
    if (optionGet(OPTION_STOP_AUTO))
    {
        if ($self->{fVersion} >= 9.3)
        {
            if ($self->executeSqlOne('select pg_is_in_backup()'))
            {
                &log(WARN, 'the cluster is already in backup mode but no backup process is running. pg_stop_backup() will be called' .
                           ' so a new backup can be started.');
                $self->backupStop();
            }
        }
        else
        {
            &log(WARN, OPTION_STOP_AUTO . 'option is only available in PostgreSQL >= 9.3');
        }
    }

    &log(INFO, "executing pg_start_backup() with label \"${strLabel}\": backup will begin after " .
               ($bStartFast ? "the requested immediate checkpoint" : "the next regular checkpoint") . " completes");

    my ($strTimestampDbStart, $strArchiveStart) =
        $self->executeSqlRow("select to_char(current_timestamp, 'YYYY-MM-DD HH24:MI:SS.US TZ'), " .
                             "pg_xlogfile_name(xlog) from pg_start_backup('${strLabel}'" .
                             ($bStartFast ? ', true' : '') . ') as xlog');

    return $strArchiveStart, $strTimestampDbStart;
}

####################################################################################################################################
# backupStop
####################################################################################################################################
sub backupStop
{
    my $self = shift;

    logDebug(OP_DB_BACKUP_STOP, DEBUG_CALL);
    &log(INFO, 'executing pg_stop_backup() and waiting for all WAL segments to be archived');

    my ($strTimestampDbStop, $strArchiveStop) =
        $self->executeSqlRow("select to_char(clock_timestamp(), 'YYYY-MM-DD HH24:MI:SS.US TZ')," .
                             " pg_xlogfile_name(xlog) from pg_stop_backup() as xlog");

    return $strArchiveStop, $strTimestampDbStop;
}

1;
