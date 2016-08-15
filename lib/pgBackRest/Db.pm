####################################################################################################################################
# DB MODULE
####################################################################################################################################
package pgBackRest::Db;

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
use pgBackRest::DbVersion;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::File;
use pgBackRest::Manifest;
use pgBackRest::Version;

####################################################################################################################################
# Remote operation constants
####################################################################################################################################
use constant OP_DB                                                  => 'Db';

use constant OP_DB_CONNECT                                          => OP_DB . '->connect';
    push @EXPORT, qw(OP_DB_CONNECT);
use constant OP_DB_EXECUTE_SQL                                      => OP_DB . '->executeSql';
    push @EXPORT, qw(OP_DB_EXECUTE_SQL);
use constant OP_DB_INFO                                             => OP_DB . '->info';
    push @EXPORT, qw(OP_DB_INFO);

####################################################################################################################################
# Backup advisory lock
####################################################################################################################################
use constant DB_BACKUP_ADVISORY_LOCK                                => '12340078987004321';
    push @EXPORT, qw(DB_BACKUP_ADVISORY_LOCK);

####################################################################################################################################
# Map the control and catalog versions to PostgreSQL version.
#
# The control version can be found in src/include/catalog/pg_control.h and may not change with every version of PostgreSQL but is
# still checked to detect custom builds which change the structure.  The catalog version can be found in
# src/include/catalog/catversion.h and should change with every release.
####################################################################################################################################
my $oPgControlVersionHash =
{
    # iControlVersion => {iCatalogVersion => strDbVersion}
    833 => {200711281 => PG_VERSION_83},
    843 => {200904091 => PG_VERSION_84},
    903 =>
    {
        201008051 => PG_VERSION_90,
        201105231 => PG_VERSION_91,
    },
    922 => {201204301 => PG_VERSION_92},
    937 => {201306121 => PG_VERSION_93},
    942 =>
    {
        201409291 => PG_VERSION_94,
        201510051 => PG_VERSION_95,
    },
    960 =>
    {
        201608031 => PG_VERSION_96,
    },
};

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;          # Class name

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->new');

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# DESTRUCTOR
####################################################################################################################################
sub DESTROY
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->DESTROY');

    if (defined($self->{hDb}))
    {
        $self->{hDb}->disconnect();
        undef($self->{hDb});
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# connect
####################################################################################################################################
sub connect
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $bWarnOnError,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::connect', \@_,
            {name => 'bWarnOnError', default => false},
        );

    # Only connect if not already connected
    my $bResult = true;

    # Run remotely
    if (optionRemoteTypeTest(DB))
    {
        # Build param hash
        my %oParamHash;

        $oParamHash{'warn-on-error'} = $bWarnOnError;

        # Execute the command
        $bResult = protocolGet()->cmdExecute(OP_DB_CONNECT, \%oParamHash, false, $bWarnOnError);

        if (!defined($bResult))
        {
            $bResult = false;
        }
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

            # Make sure the socket path is absolute
            if (defined($strDbSocketPath) && $strDbSocketPath !~ /^\//)
            {
                confess &log(ERROR, "'${strDbSocketPath}' is not valid for '" . OPTION_DB_SOCKET_PATH . "' option:" .
                                    " path must be absolute", ERROR_OPTION_INVALID_VALUE);
            }

            my $strDbUri = "dbi:Pg:dbname=${strDbName};port=" . optionGet(OPTION_DB_PORT) .
                           (optionTest(OPTION_DB_SOCKET_PATH) ? ';host=' . optionGet(OPTION_DB_SOCKET_PATH) : '');

            logDebugMisc
            (
                $strOperation, undef,
                {name => 'strDbUri', value => $strDbUri},
                {name => 'strDbUser', value => $strDbUser}
            );

            $self->{hDb} = DBI->connect($strDbUri, $strDbUser, undef,
                                        {AutoCommit => 1, RaiseError => 0, PrintError => 0, Warn => 0});

            # If db handle is not valid then check error
            if (!$self->{hDb})
            {
                # Throw an error unless a warning was requested
                if (!$bWarnOnError)
                {
                    confess &log(ERROR, $DBI::errstr, ERROR_DB_CONNECT);
                }

                # Log a warning
                &log(WARN, $DBI::errstr);

                $bResult = false;
                undef($self->{hDb});
            }
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bResult', value => $bResult}
    );
}

####################################################################################################################################
# executeSql
####################################################################################################################################
sub executeSql
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strSql,
        $bIgnoreError,
        $bResult,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::executeSql', \@_,
            {name => 'strSql'},
            {name => 'bIgnoreError', default => false},
            {name => 'bResult', default => true},
        );

    # Get the user-defined command for psql
    my $strResult;

    # Run remotely
    if (optionRemoteTypeTest(DB))
    {
        # Build param hash
        my %oParamHash;

        $oParamHash{'script'} = $strSql;
        $oParamHash{'ignore-error'} = $bIgnoreError;
        $oParamHash{'result'} = $bResult;

        # Execute the command
        $strResult = protocolGet()->cmdExecute(OP_DB_EXECUTE_SQL, \%oParamHash, $bResult);
    }
    # Else run locally
    else
    {
        $self->connect();

        # Prepare the query
        my $hStatement = $self->{hDb}->prepare($strSql, {pg_async => PG_ASYNC})
            or confess &log(ERROR, $DBI::errstr . ":\n${strSql}", ERROR_DB_QUERY);

        # Execute the query
        $hStatement->execute()
            or confess &log(ERROR, $DBI::errstr. ":\n${strSql}", ERROR_DB_QUERY);

        # Wait for the query to return
        my $oWait = waitInit(optionGet(OPTION_DB_TIMEOUT));
        my $bTimeout = true;

        do
        {
            # Is the statement done?
            if ($hStatement->pg_ready())
            {
                # return now if there is no result expected
                if (!$bResult)
                {
                    return;
                }

                if (!$hStatement->pg_result())
                {
                    # Return if the error should be ignored
                    if ($bIgnoreError)
                    {
                        return '';
                    }

                    # Else report it
                    confess &log(ERROR, $DBI::errstr . ":\n${strSql}", ERROR_DB_QUERY);
                }

                # Get rows and return them
                my @stryRow;

                do
                {
                    # Get next row
                    @stryRow = $hStatement->fetchrow_array;

                    # If the row has data then add it to the result
                    if (@stryRow)
                    {
                        # Add an LF after the first row
                        $strResult .= (defined($strResult) ? "\n" : '');

                        # Add row to result
                        for (my $iColumnIdx = 0; $iColumnIdx < @stryRow; $iColumnIdx++)
                        {
                            # Add tab between columns
                            $strResult .= $iColumnIdx == 0 ? '' : "\t";

                            # Add column data
                            $strResult .= defined($stryRow[$iColumnIdx]) ? $stryRow[$iColumnIdx] : '';
                        }
                    }
                    # Else check for error
                    elsif ($hStatement->err)
                    {
                        confess &log(ERROR, $DBI::errstr . ":\n${strSql}", ERROR_DB_QUERY);
                    }
                }
                while (@stryRow);

                $bTimeout = false;
            }
        } while ($bTimeout && waitMore($oWait));

        # If timeout then cancel the query and confess
        if ($bTimeout)
        {
            $hStatement->pg_cancel();
            confess &log(ERROR, 'statement timed out after ' . waitInterval($oWait) .
                                " second(s):\n${strSql}", ERROR_DB_TIMEOUT);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strResult', value => $strResult}
    );
}

####################################################################################################################################
# executeSqlRow
####################################################################################################################################
sub executeSqlRow
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strSql
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->executeSqlRow', \@_,
            {name => 'strSql', trace => true}
        );

    # Return from function and log return values if any
    my @stryResult = split("\t", $self->executeSql($strSql));

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strResult', value => \@stryResult}
    );
}

####################################################################################################################################
# executeSqlOne
####################################################################################################################################
sub executeSqlOne
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strSql
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->executeSqlOne', \@_,
            {name => 'strSql', trace => true}
        );

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strResult', value => ($self->executeSqlRow($strSql))[0], trace => true}
    );
}

####################################################################################################################################
# tablespaceMapGet
#
# Get the mapping between oid and tablespace name.
####################################################################################################################################
sub tablespaceMapGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->tablespaceMapGet');

    dataHashBuild(my $oTablespaceMapRef = {}, "oid\tname\n" . $self->executeSql(
                  'select oid, spcname from pg_tablespace'), "\t");

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oTablespaceMapRef', value => $oTablespaceMapRef}
    );
}

####################################################################################################################################
# databaseMapGet
#
# Get the mapping between oid and database name.
####################################################################################################################################
sub databaseMapGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->databaseMapGet');

    dataHashBuild(my $oDatabaseMapRef = {}, "name\t" . MANIFEST_KEY_DB_ID . "\t" . MANIFEST_KEY_DB_LAST_SYSTEM_ID  . "\n" .
                  $self->executeSql('select datname, oid, datlastsysoid from pg_database'), "\t");

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oDatabaseMapRef', value => $oDatabaseMapRef}
    );
}

####################################################################################################################################
# info
####################################################################################################################################
sub info
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oFile,
        $strDbPath
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->info', \@_,
            {name => 'oFile'},
            {name => 'strDbPath'}
        );

    # Get info if it is not cached
    #-------------------------------------------------------------------------------------------------------------------------------
    if (!defined($self->{info}{$strDbPath}))
    {
        # Get info from remote
        #---------------------------------------------------------------------------------------------------------------------------
        if ($oFile->isRemote(PATH_DB_ABSOLUTE))
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

            # Cache info so it does not need to read again for the same database
            $self->{info}{$strDbPath}{strDbVersion} = $stryToken[0];
            $self->{info}{$strDbPath}{iControlVersion} = $stryToken[1];
            $self->{info}{$strDbPath}{iCatalogVersion} = $stryToken[2];
            $self->{info}{$strDbPath}{ullDbSysId} = $stryToken[3];
        }
        # Get info locally
        #---------------------------------------------------------------------------------------------------------------------------
        else
        {
            # Open the control file and read system id and versions
            #-----------------------------------------------------------------------------------------------------------------------
            my $strControlFile = "${strDbPath}/" . DB_FILE_PGCONTROL;
            my $hFile;
            my $tBlock;

            sysopen($hFile, $strControlFile, O_RDONLY)
                or confess &log(ERROR, "unable to open ${strControlFile}", ERROR_FILE_OPEN);

            # Read system identifier
            sysread($hFile, $tBlock, 8) == 8
                or confess &log(ERROR, "unable to read database system identifier");

            $self->{info}{$strDbPath}{ullDbSysId} = unpack('Q', $tBlock);

            # Read control version
            sysread($hFile, $tBlock, 4) == 4
                or confess &log(ERROR, "unable to read control version");

            $self->{info}{$strDbPath}{iControlVersion} = unpack('L', $tBlock);

            # Read catalog version
            sysread($hFile, $tBlock, 4) == 4
                or confess &log(ERROR, "unable to read catalog version");

            $self->{info}{$strDbPath}{iCatalogVersion} = unpack('L', $tBlock);

            # Close the control file
            close($hFile);

            # Get PostgreSQL version
            $self->{info}{$strDbPath}{strDbVersion} =
                $$oPgControlVersionHash{$self->{info}{$strDbPath}{iControlVersion}}{$self->{info}{$strDbPath}{iCatalogVersion}};

            if (!defined($self->{info}{$strDbPath}{strDbVersion}))
            {
                confess &log(
                    ERROR,
                    'unexpected control version = ' . $self->{info}{$strDbPath}{iControlVersion} .
                    ' and catalog version = ' . $self->{info}{$strDbPath}{iCatalogVersion} . "\n" .
                    'HINT: is this version of PostgreSQL supported?',
                    ERROR_VERSION_NOT_SUPPORTED);
            }
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strDbVersion', value => $self->{info}{$strDbPath}{strDbVersion}},
        {name => 'iControlVersion', value => $self->{info}{$strDbPath}{iControlVersion}},
        {name => 'iCatalogVersion', value => $self->{info}{$strDbPath}{iCatalogVersion}},
        {name => 'ullDbSysId', value => $self->{info}{$strDbPath}{ullDbSysId}}
    );
}

####################################################################################################################################
# versionGet
####################################################################################################################################
sub versionGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->versionGet');

    # Get data from the cache if possible
    if (defined($self->{strDbVersion}) && defined($self->{strDbPath}))
    {
        return $self->{strDbVersion}, $self->{strDbPath};
    }

    # Get version and db-path from
    ($self->{strDbVersion}, $self->{strDbPath}) =
        $self->executeSqlRow("select (regexp_matches(split_part(version(), ' ', 2), '^[0-9]+\.[0-9]+'))[1], setting" .
                             " from pg_settings where name = 'data_directory'");

    my @stryVersionSupport = versionSupport();

    if ($self->{strDbVersion} < $stryVersionSupport[0])
    {
        confess &log(ERROR, 'unsupported Postgres version' . $self->{strDbVersion}, ERROR_VERSION_NOT_SUPPORTED);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strDbVersion', value => $self->{strDbVersion}},
        {name => 'strDbPath', value => $self->{strDbPath}}
    );
}

####################################################################################################################################
# backupStart
####################################################################################################################################
sub backupStart
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oFile,
        $strDbPath,
        $strLabel,
        $bStartFast
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->backupStart', \@_,
            {name => 'oFile'},
            {name => 'strDbPath'},
            {name => 'strLabel'},
            {name => 'bStartFast'}
        );

    # Validate the database configuration
    $self->configValidate($oFile, $strDbPath);

    # Only allow start-fast option for version >= 8.4
    if ($self->{strDbVersion} < PG_VERSION_84 && $bStartFast)
    {
        &log(WARN, OPTION_START_FAST . ' option is only available in PostgreSQL >= ' . PG_VERSION_84);
        $bStartFast = false;
    }

    # Acquire the backup advisory lock to make sure that backups are not running from multiple backup servers against the same
    # database cluster.  This lock helps make the stop-auto option safe.
    if (!$self->executeSqlOne('select pg_try_advisory_lock(' . DB_BACKUP_ADVISORY_LOCK . ')'))
    {
        confess &log(ERROR, "unable to acquire backup lock\n" .
                            'HINT: is another backup already running on this cluster?', ERROR_LOCK_ACQUIRE);
    }

    # If stop-auto is enabled check for a running backup.  This feature is not supported for PostgreSQL >= 9.6 since backups are
    # run in non-exclusive mode.
    if (optionGet(OPTION_STOP_AUTO) && $self->{strDbVersion} < PG_VERSION_96)
    {
        # Running backups can only be detected in PostgreSQL >= 9.3
        if ($self->{strDbVersion} >= PG_VERSION_93)
        {
            # If a backup is currently in progress emit a warning and then stop it
            if ($self->executeSqlOne('select pg_is_in_backup()'))
            {
                &log(WARN, 'the cluster is already in backup mode but no backup process is running.' .
                           ' pg_stop_backup() will be called so a new backup can be started.');
                $self->backupStop();
            }
        }
        # Else emit a warning that the feature is not supported and continue.  If a backup is running then an error will be
        # generated later on.
        else
        {
            &log(WARN, OPTION_STOP_AUTO . 'option is only available in PostgreSQL >= ' . PG_VERSION_93);
        }
    }

    # Start the backup
    &log(INFO, 'execute ' . ($self->{strDbVersion} >= PG_VERSION_96 ? 'non-' : '') .
               "exclusive pg_start_backup() with label \"${strLabel}\": backup begins after " .
               ($bStartFast ? "the requested immediate checkpoint" : "the next regular checkpoint") . " completes");

    my ($strTimestampDbStart, $strArchiveStart) = $self->executeSqlRow(
        "select to_char(current_timestamp, 'YYYY-MM-DD HH24:MI:SS.US TZ'), lsn::text" .
            " from pg_start_backup('${strLabel}'" .
            ($bStartFast ? ', true' : $self->{strDbVersion} >= PG_VERSION_84 ? ', false' : '') .
            ($self->{strDbVersion} >= PG_VERSION_96 ? ', false' : '') . ') as lsn');

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strArchiveStart', value => $strArchiveStart},
        {name => 'strTimestampDbStart', value => $strTimestampDbStart}
    );
}

####################################################################################################################################
# backupStop
####################################################################################################################################
sub backupStop
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->backupStop');

    # Stop the backup
    &log(INFO, 'execute ' . ($self->{strDbVersion} >= PG_VERSION_96 ? 'non-' : '') .
               'exclusive pg_stop_backup() and wait for all WAL segments to archive');

    my ($strTimestampDbStop, $strArchiveStop, $strLabel, $strTablespaceMap) =
        $self->executeSqlRow(
            "select to_char(clock_timestamp(), 'YYYY-MM-DD HH24:MI:SS.US TZ'), lsn::text, " .
            ($self->{strDbVersion} >= PG_VERSION_96 ? 'labelfile, spcmapfile' : "null as labelfile, null as spcmapfile") .
            ' from pg_stop_backup(' .
            ($self->{strDbVersion} >= PG_VERSION_96 ? 'false)' : ') as lsn'));

    # Build a hash of the files that need to be written to the backup
    my $oFileHash =
    {
        &MANIFEST_FILE_BACKUPLABEL => $strLabel,
        &MANIFEST_FILE_TABLESPACEMAP => $strTablespaceMap
    };

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strArchiveStop', value => $strArchiveStop},
        {name => 'strTimestampDbStop', value => $strTimestampDbStop},
        {name => 'oFileHash', value => $oFileHash}
    );
}

####################################################################################################################################
# configValidate
#
# Validate the database configuration and archiving.
####################################################################################################################################
sub configValidate
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oFile,
        $strDbPath
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->configValidate', \@_,
            {name => 'oFile'},
            {name => 'strDbPath'}
        );

    # Get the version from the control file
    my ($strDbVersion) = $self->info($oFile, $strDbPath);

    # Get version and db path from the database
    my ($fCompareDbVersion, $strCompareDbPath) = $self->versionGet();

    # Error if the version from the control file and the configured db-path do not match the values obtained from the database
    if (!($strDbVersion == $fCompareDbVersion && $strDbPath eq $strCompareDbPath))
    {
        confess &log(ERROR,
            "version '${fCompareDbVersion}' and db-path '${strCompareDbPath}' queried from cluster does not match" .
            " version '${strDbVersion}' and db-path '${strDbPath}' read from '${strDbPath}/" . DB_FILE_PGCONTROL . "'\n" .
            "HINT: the db-path and db-port settings likely reference different clusters", ERROR_DB_MISMATCH);
    }

    # Error if archive_mode = always (support has not been added yet)
    if (optionGet(OPTION_BACKUP_ARCHIVE_CHECK) && $self->executeSql('show archive_mode') eq 'always')
    {
        confess &log(ERROR, "archive_mode=always not supported", ERROR_FEATURE_NOT_SUPPORTED);
    }

    # Check if archive_command is set
    if (optionGet(OPTION_BACKUP_ARCHIVE_CHECK))
    {
        my $strArchiveCommand = $self->executeSql('show archive_command');

        if (index($strArchiveCommand, BACKREST_EXE) == -1)
        {
            confess &log(ERROR,
                'archive_command \'${strArchiveCommand}\' must contain \'' . BACKREST_EXE . '\'', ERROR_ARCHIVE_COMMAND_INVALID);
        }
    }

    return logDebugReturn
    (
        $strOperation
    );
}

####################################################################################################################################
# xlogSwitch
#
# Forces a switch to the next transaction log in order to archive the current log.
####################################################################################################################################
sub xlogSwitch
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my $strOperation = logDebugParam(__PACKAGE__ . '->xlogSwitch');

    # Create a restore point to ensure current xlog will be archived.  For versions <= 9.0 activity will need to be generated by
    # the user if there have been no writes since the last xlog switch.
    if ($self->{strDbVersion} >= PG_VERSION_91)
    {
        $self->executeSql("select pg_create_restore_point('" . BACKREST_NAME . " Archive Check');");
    }

    my $strWalFileName = $self->executeSqlRow('select pg_xlogfile_name from pg_xlogfile_name(pg_switch_xlog());');

    &log(INFO, "switch xlog ${strWalFileName}");

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strXlogFileName', value => $strWalFileName}
    );
}

1;
