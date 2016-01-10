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
use BackRest::Common::Exception;
use BackRest::Common::Log;
use BackRest::Common::String;
use BackRest::Common::Wait;
use BackRest::Config::Config;
use BackRest::File;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_DB                                                  => 'Db';

use constant OP_DB_NEW                                              => OP_DB . "->new";
use constant OP_DB_BACKUP_START                                     => OP_DB . "->backupStart";
use constant OP_DB_BACKUP_STOP                                      => OP_DB . "->backupStop";
use constant OP_DB_DESTROY                                          => OP_DB . "->DESTROY";
use constant OP_DB_EXECUTE_SQL                                      => OP_DB . "->executeSql";
    push @EXPORT, qw(OP_DB_EXECUTE_SQL);
use constant OP_DB_EXECUTE_SQL_ONE                                  => OP_DB . "->executeSqlOne";
use constant OP_DB_EXECUTE_SQL_ROW                                  => OP_DB . "->executeSqlRow";
use constant OP_DB_INFO                                             => OP_DB . "->info";
    push @EXPORT, qw(OP_DB_INFO);
use constant OP_DB_TABLESPACE_MAP_GET                               => OP_DB . "->tablespaceMapGet";
use constant OP_DB_VERSION_GET                                      => OP_DB . "->versionGet";
use constant OP_DB_VERSION_SUPPORT                                  => OP_DB . "->versionSupport";

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

    # Assign function parameters, defaults, and log debug info
    (
        my $strOperation
    ) =
        logDebugParam
        (
            OP_DB_NEW
        );

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
    my
    (
        $strOperation
    ) =
        logDebugParam
        (
            OP_DB_DESTROY
        );

    if (defined($self->{hDb}))
    {
        $self->{hDb}->disconnect();
        undef($self->{hDb});
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

####################################################################################################################################
# versionSupport
#
# Returns an array of the supported Postgres versions.
####################################################################################################################################
sub versionSupport
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation
    ) =
        logDebugParam
        (
            OP_DB_VERSION_SUPPORT
        );

    my @strySupportVersion = ('8.3', '8.4', '9.0', '9.1', '9.2', '9.3', '9.4', '9.5');

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strySupportVersion', value => \@strySupportVersion}
    );
}

push @EXPORT, qw(versionSupport);

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
        $bIgnoreError
    ) =
        logDebugParam
        (
            OP_DB_EXECUTE_SQL, \@_,
            {name => 'strSql'},
            {name => 'bIgnoreError', default => false}
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

            logDebugMisc
            (
                $strOperation, 'db connect',
                {name => 'strDbUri', value => $strDbUri},
                {name => 'strDbUser', value => $strDbUser}
            );

            $self->{hDb} = DBI->connect($strDbUri, $strDbUser, undef,
                                        {AutoCommit => 1, RaiseError => 0, PrintError => 0, Warn => 0});

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
        my $oWait = waitInit(optionGet(OPTION_DB_TIMEOUT));
        my $bTimeout = true;

        do
        {
            # Is the statement done?
            if ($hStatement->pg_ready())
            {
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
            OP_DB_EXECUTE_SQL_ROW, \@_,
            {name => 'strSql', trace => true}
        );

    # Return from function and log return values if any
    my @stryResult = split("\t", trim($self->executeSql($strSql)));

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
            OP_DB_EXECUTE_SQL_ONE, \@_,
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
    my
    (
        $strOperation
    ) =
        logDebugParam
        (
            OP_DB_TABLESPACE_MAP_GET
        );

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
            OP_DB_INFO, \@_,
            {name => 'oFile'},
            {name => 'strDbPath'}
        );

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
        if ($iControlVersion == 942 && $iCatalogVersion == 201510051)
        {
            $fDbVersion = '9.5';
        }
        elsif ($iControlVersion == 942 && $iCatalogVersion == 201409291)
        {
            $fDbVersion = '9.4';
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

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'fDbVersion', value => $fDbVersion},
        {name => 'iControlVersion', value => $iControlVersion},
        {name => 'iCatalogVersion', value => $iCatalogVersion},
        {name => 'ullDbSysId', value => $ullDbSysId}
    );
}

####################################################################################################################################
# versionGet
####################################################################################################################################
sub versionGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation
    ) =
        logDebugParam
        (
            OP_DB_VERSION_GET
        );

    # Get data from the cache if possible
    if (defined($self->{fDbVersion}) && defined($self->{strDbPath}))
    {
        return $self->{fDbVersion}, $self->{strDbPath};
    }

    # Get version and db-path from
    ($self->{fDbVersion}, $self->{strDbPath}) =
        $self->executeSqlRow("select (regexp_matches(split_part(version(), ' ', 2), '^[0-9]+\.[0-9]+'))[1], setting" .
                             " from pg_settings where name = 'data_directory'");

    my @stryVersionSupport = versionSupport();

    if ($self->{fDbVersion} < $stryVersionSupport[0])
    {
        confess &log(ERROR, 'unsupported Postgres version' . $self->{fDbVersion}, ERROR_VERSION_NOT_SUPPORTED);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'fDbVersion', value => $self->{fDbVersion}},
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
            OP_DB_BACKUP_START, \@_,
            {name => 'oFile'},
            {name => 'strDbPath'},
            {name => 'strLabel'},
            {name => 'bStartFast'}
        );

    # Get the version from the control file
    my ($fDbVersion) = $self->info($oFile, $strDbPath);

    # Get version and db path from the database
    my ($fCompareDbVersion, $strCompareDbPath) = $self->versionGet();

    # Error if the version from the control file and the configured db-path do not match the values obtained from the database
    if (!($fDbVersion == $fCompareDbVersion && $strDbPath eq $strCompareDbPath))
    {
        confess &log(ERROR, "version '${fCompareDbVersion}' and db-path '${strCompareDbPath}' queried from cluster does not match" .
                            " version '${fDbVersion}' and db-path '${strDbPath}' read from '${strDbPath}/global/pg_control'\n" .
                            "HINT: the db-path and db-port settings likely reference different clusters", ERROR_DB_MISMATCH);
    }

    # Only allow start-fast option for version >= 8.4
    if ($self->{fDbVersion} < 8.4 && $bStartFast)
    {
        &log(WARN, OPTION_START_FAST . ' option is only available in PostgreSQL >= 8.4');
        $bStartFast = false;
    }

    # Error if archive_mode = always (support has not been added yet)
    my $strArchiveMode = trim($self->executeSql('show archive_mode'));

    if ($strArchiveMode eq 'always')
    {
        confess &log(ERROR, "archive_mode=always not supported", ERROR_FEATURE_NOT_SUPPORTED);
    }

    # Acquire the backup advisory lock to make sure that backups are not running from multiple backup servers against the same
    # database cluster.  This lock helps make the stop-auto option safe.
    if (!$self->executeSqlOne('select pg_try_advisory_lock(' . DB_BACKUP_ADVISORY_LOCK . ')'))
    {
        confess &log(ERROR, "unable to acquire backup lock\n" .
                            'HINT: is another backup already running on this cluster?', ERROR_LOCK_ACQUIRE);
    }

    # If stop-auto is enabled check for a running backup
    if (optionGet(OPTION_STOP_AUTO))
    {
        # Running backups can only be detected in PostgreSQL >= 9.3
        if ($self->{fDbVersion} >= 9.3)
        {
            # If a backup is currently in progress emit a warning and then stop it
            if ($self->executeSqlOne('select pg_is_in_backup()'))
            {
                &log(WARN, 'the cluster is already in backup mode but no backup process is running. pg_stop_backup() will be called' .
                           ' so a new backup can be started.');
                $self->backupStop();
            }
        }
        # Else emit a warning that the feature is not supported and continue.  If a backup is running then an error will be
        # generated later on.
        else
        {
            &log(WARN, OPTION_STOP_AUTO . 'option is only available in PostgreSQL >= 9.3');
        }
    }

    # Start the backup
    &log(INFO, "execute pg_start_backup() with label \"${strLabel}\": backup begins after " .
               ($bStartFast ? "the requested immediate checkpoint" : "the next regular checkpoint") . " completes");

    my ($strTimestampDbStart, $strArchiveStart) =
        $self->executeSqlRow("select to_char(current_timestamp, 'YYYY-MM-DD HH24:MI:SS.US TZ'), " .
                             "pg_xlogfile_name(xlog) from pg_start_backup('${strLabel}'" .
                             ($bStartFast ? ', true' : '') . ') as xlog');

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
    my
    (
        $strOperation
    ) =
        logDebugParam
        (
            OP_DB_BACKUP_STOP
        );

    # Stop the backup
    &log(INFO, 'execute pg_stop_backup() and wait for all WAL segments to archive');

    my ($strTimestampDbStop, $strArchiveStop) =
        $self->executeSqlRow("select to_char(clock_timestamp(), 'YYYY-MM-DD HH24:MI:SS.US TZ')," .
                             " pg_xlogfile_name(xlog) from pg_stop_backup() as xlog");

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strArchiveStop', value => $strArchiveStop},
        {name => 'strTimestampDbStop', value => $strTimestampDbStop}
    );
}

1;
