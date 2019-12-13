####################################################################################################################################
# DB MODULE
####################################################################################################################################
package pgBackRest::Db;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT =  qw();
use Fcntl qw(O_RDONLY);
use File::Basename qw(dirname);
use JSON::PP;

use pgBackRest::DbVersion;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Helper;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Version;

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
        201608131 => PG_VERSION_96,
    },
    1002 =>
    {
        201707211 => PG_VERSION_10,
    },
    1100 =>
    {
        201809051 => PG_VERSION_11,
    },
    1201 =>
    {
        201909212 => PG_VERSION_12,
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
    (
        my $strOperation,
        $self->{iRemoteIdx},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'iRemoteIdx', required => false},
        );

    if (defined($self->{iRemoteIdx}))
    {
        $self->{strDbPath} = cfgOption(cfgOptionIdFromIndex(CFGOPT_PG_PATH, $self->{iRemoteIdx}));

        if (!isDbLocal({iRemoteIdx => $self->{iRemoteIdx}}))
        {
            $self->{oProtocol} = protocolGet(CFGOPTVAL_REMOTE_TYPE_DB, $self->{iRemoteIdx});
        }
    }

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

    if (defined($self->{oDb}))
    {
        $self->{oDb}->close();
        undef($self->{oDb});
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
    if (defined($self->{oProtocol}))
    {
        # Set bResult to false if undef is returned
        $bResult = $self->{oProtocol}->cmdExecute(OP_DB_CONNECT, undef, false, $bWarnOnError) ? true : false;
    }
    # Else run locally
    else
    {
        if (!defined($self->{oDb}))
        {
            $self->{oDb} = new pgBackRest::LibC::PgClient(
                cfgOption(cfgOptionIdFromIndex(CFGOPT_PG_SOCKET_PATH, $self->{iRemoteIdx}), false),
                cfgOption(cfgOptionIdFromIndex(CFGOPT_PG_PORT, $self->{iRemoteIdx})), 'postgres',
                cfgOption(CFGOPT_DB_TIMEOUT) * 1000);

            if ($bWarnOnError)
            {
                eval
                {
                    $self->{oDb}->open();
                    return true;
                }
                or do
                {
                    &log(WARN, exceptionMessage($EVAL_ERROR));
                    $bResult = false;

                    undef($self->{oDb});
                }
            }
            else
            {
                $self->{oDb}->open();
            }

            if (defined($self->{oDb}))
            {
                my ($fDbVersion) = $self->versionGet();

                if ($fDbVersion >= PG_VERSION_APPLICATION_NAME)
                {
                    # Set application name for monitoring and debugging
                    $self->{oDb}->query(
                        "set application_name = '" . PROJECT_NAME . ' [' .
                        (cfgOptionValid(CFGOPT_COMMAND) ? cfgOption(CFGOPT_COMMAND) : cfgCommandName(cfgCommandGet())) . "]'");

                    # Clear search path to prevent possible function overrides
                    $self->{oDb}->query("set search_path = 'pg_catalog'");
                }
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
    my @stryResult;

    # Run remotely
    if (defined($self->{oProtocol}))
    {
        # Execute the command
        @stryResult = @{$self->{oProtocol}->cmdExecute(OP_DB_EXECUTE_SQL, [$strSql, $bIgnoreError, $bResult], $bResult)};
    }
    # Else run locally
    else
    {
        $self->connect();
        my $strResult = $self->{oDb}->query($strSql);

        if (defined($strResult))
        {
            @stryResult = @{JSON::PP->new()->allow_nonref()->decode($strResult)};
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'stryResult', value => \@stryResult, ref => true}
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
            {name => 'strSql'}
        );

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'stryResult', value => @{$self->executeSql($strSql)}[0]}
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
            {name => 'strSql'}
        );

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strResult', value => @{@{$self->executeSql($strSql)}[0]}[0]}
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
        $strDbPath
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->info', \@_,
            {name => 'strDbPath', default => $self->{strDbPath}}
        );

    # Get info if it is not cached
    #-------------------------------------------------------------------------------------------------------------------------------
    if (!defined($self->{info}{$strDbPath}))
    {
        # Get info from remote
        #---------------------------------------------------------------------------------------------------------------------------
        if (defined($self->{oProtocol}))
        {
            # Execute the command
            ($self->{info}{$strDbPath}{strDbVersion}, $self->{info}{$strDbPath}{iDbControlVersion},
                $self->{info}{$strDbPath}{iDbCatalogVersion}, $self->{info}{$strDbPath}{ullDbSysId}) =
                    $self->{oProtocol}->cmdExecute(OP_DB_INFO, [$strDbPath], true);
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

            $self->{info}{$strDbPath}{iDbControlVersion} = unpack('L', $tBlock);

            # Read catalog version
            sysread($hFile, $tBlock, 4) == 4
                or confess &log(ERROR, "unable to read catalog version");

            $self->{info}{$strDbPath}{iDbCatalogVersion} = unpack('L', $tBlock);

            # Close the control file
            close($hFile);

            # Get PostgreSQL version
            $self->{info}{$strDbPath}{strDbVersion} =
                $oPgControlVersionHash->{$self->{info}{$strDbPath}{iDbControlVersion}}
                    {$self->{info}{$strDbPath}{iDbCatalogVersion}};

            if (!defined($self->{info}{$strDbPath}{strDbVersion}))
            {
                confess &log(
                    ERROR,
                    'unexpected control version = ' . $self->{info}{$strDbPath}{iDbControlVersion} .
                    ' and catalog version = ' . $self->{info}{$strDbPath}{iDbCatalogVersion} . "\n" .
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
        {name => 'iDbControlVersion', value => $self->{info}{$strDbPath}{iDbControlVersion}},
        {name => 'iDbCatalogVersion', value => $self->{info}{$strDbPath}{iDbCatalogVersion}},
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

    # Get version and pg-path from
    my ($strVersionNum, $strDbPath) =
        $self->executeSqlRow(
            "select (select setting from pg_settings where name = 'server_version_num'), " .
                " (select setting from pg_settings where name = 'data_directory')");

    # Get first part of the major version - for 10 and above there will only be one part
    $self->{strDbVersion} = substr($strVersionNum, 0, length($strVersionNum) - 4);

    # Now retrieve the second part of the major version for versions less than 10
    if ($self->{strDbVersion} < PG_VERSION_10)
    {
        $self->{strDbVersion} .= qw{.} . int(substr($strVersionNum, 1, 2));
    }

    # Check that the version is supported
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
        {name => 'strDbPath', value => $strDbPath}
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
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->configValidate', \@_,
        );

    # Get the version from the control file
    my ($strDbVersion) = $self->info();

    # Get version and db path from the database
    my ($fCompareDbVersion, $strCompareDbPath) = $self->versionGet();

    # Error if the version from the control file and the configured pg-path do not match the values obtained from the database
    if (!($strDbVersion == $fCompareDbVersion && $self->{strDbPath} eq $strCompareDbPath))
    {
        confess &log(ERROR,
            "version '${fCompareDbVersion}' and path '${strCompareDbPath}' queried from cluster do not match version" .
                " '${strDbVersion}' and " . cfgOptionName(CFGOPT_PG_PATH) . " '$self->{strDbPath}' read from" .
                " '$self->{strDbPath}/" . DB_FILE_PGCONTROL . "'\n" .
            "HINT: the " . cfgOptionName(CFGOPT_PG_PATH) . " and " . cfgOptionName(CFGOPT_PG_PORT) .
                " settings likely reference different clusters.",
            ERROR_DB_MISMATCH);
    }

    # If cluster is not a standby and archive checking is enabled, then perform various validations
    if (!$self->isStandby() && cfgOptionValid(CFGOPT_ARCHIVE_CHECK) && cfgOption(CFGOPT_ARCHIVE_CHECK))
    {
        my $strArchiveMode = $self->executeSqlOne('show archive_mode');

        # Error if archive_mode = off since pg_start_backup () will fail
        if ($strArchiveMode eq 'off')
        {
            confess &log(ERROR, 'archive_mode must be enabled', ERROR_ARCHIVE_DISABLED);
        }

        # Error if archive_mode = always (support has not been added yet)
        if ($strArchiveMode eq 'always')
        {
            confess &log(ERROR, "archive_mode=always not supported", ERROR_FEATURE_NOT_SUPPORTED);
        }

        # Check if archive_command is set
        my $strArchiveCommand = $self->executeSqlOne('show archive_command');

        if (index($strArchiveCommand, PROJECT_EXE) == -1)
        {
            confess &log(ERROR,
                'archive_command ' . (defined($strArchiveCommand) ? "'${strArchiveCommand}'" : '[null]') . ' must contain \'' .
                PROJECT_EXE . '\'', ERROR_ARCHIVE_COMMAND_INVALID);
        }
    }

    return logDebugReturn
    (
        $strOperation
    );
}

####################################################################################################################################
# walId
#
# Returns 'wal' or 'xlog' depending on the version of PostgreSQL.
####################################################################################################################################
sub walId
{
    my $self = shift;

    return $self->{strDbVersion} >= PG_VERSION_10 ? 'wal' : 'xlog';
}

####################################################################################################################################
# isStandby
#
# Determines if a database is a standby by testing if it is in recovery mode.
####################################################################################################################################
sub isStandby
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->isStandby');

    if (!defined($self->{bStandby}))
    {
        my ($strDbVersion) = $self->versionGet();

        if ($strDbVersion <= PG_VERSION_90)
        {
            $self->{bStandby} = false;
        }
        else
        {
            $self->{bStandby} = $self->executeSqlOne('select pg_is_in_recovery()') ? true : false;
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bStandby', value => $self->{bStandby}}
    );
}

####################################################################################################################################
# dbObjectGet
#
# Gets the database objects(s) and indexes. The databases required for the backup type must be online. A connection to the available
# databases will be established to determine which is the master and which, if any, is the standby. If there is a master and a
# standby to which a connection can be established, it returns both, else just the master.
####################################################################################################################################
sub dbObjectGet
{
    # Assign function parameters, defaults, and log debug info
    my (
            $strOperation,
            $bMasterOnly,
        ) =
            logDebugParam
            (
                __PACKAGE__ . '::dbObjectGet', \@_,
                {name => 'bMasterOnly', optional => true, default => false},
            );

    my $iStandbyIdx = undef;
    my $iMasterRemoteIdx = 1;
    my $oDbMaster = undef;
    my $oDbStandby = undef;

    # Only iterate databases if online and more than one is defined.  It might be better to check the version of each database but
    # this is simple and works.
    if (!$bMasterOnly && cfgOptionTest(CFGOPT_ONLINE) && cfgOption(CFGOPT_ONLINE) && multipleDb())
    {
        for (my $iRemoteIdx = 1; $iRemoteIdx <= cfgOptionIndexTotal(CFGOPT_PG_HOST); $iRemoteIdx++)
        {
            # Make sure a db is defined for this index
            if (cfgOptionTest(cfgOptionIdFromIndex(CFGOPT_PG_PATH, $iRemoteIdx)) ||
                cfgOptionTest(cfgOptionIdFromIndex(CFGOPT_PG_HOST, $iRemoteIdx)))
            {
                # Create the db object
                my $oDb;

                logWarnOnErrorEnable();
                eval
                {
                    $oDb = new pgBackRest::Db($iRemoteIdx);
                    return true;
                }
                or do {};

                logWarnOnErrorDisable();
                my $bAssigned = false;

                if (defined($oDb))
                {
                    # If able to connect then test if the database is a master or a standby.  It's OK if some databases cannot be
                    # reached as long as the databases required for the backup type are present.
                    if ($oDb->connect(true))
                    {
                        # If this db is a standby
                        if ($oDb->isStandby())
                        {
                            # If standby backup is requested then use the first standby found
                            if (cfgOption(CFGOPT_BACKUP_STANDBY) && !defined($oDbStandby))
                            {
                                $oDbStandby = $oDb;
                                $iStandbyIdx = $iRemoteIdx;
                                $bAssigned = true;
                            }
                        }
                        # Else this db is a master
                        else
                        {
                            # Error if more than one master is found
                            if (defined($oDbMaster))
                            {
                                confess &log(ERROR, 'more than one master database found');
                            }

                            $oDbMaster = $oDb;
                            $iMasterRemoteIdx = $iRemoteIdx;
                            $bAssigned = true;
                        }
                    }
                }

                # If the db was not used then destroy the protocol object underneath it
                if (!$bAssigned)
                {
                    protocolDestroy(CFGOPTVAL_REMOTE_TYPE_DB, $iRemoteIdx, true);
                }
            }
        }

        # Make sure a standby database is defined when backup from standby option is set
        if (cfgOption(CFGOPT_BACKUP_STANDBY) && !defined($oDbStandby))
        {
            # Throw an error that is distinct from connecting to the master for testing purposes
            confess &log(ERROR, 'unable to find standby database - cannot proceed', ERROR_HOST_CONNECT);
        }

        # A master database is always required
        if (!defined($oDbMaster))
        {
            # Throw an error that is distinct from connecting to a standy for testing purposes
            confess &log(ERROR, 'unable to find master database - cannot proceed', ERROR_DB_CONNECT);
        }
    }

    # If master db is not already defined then set to default
    if (!defined($oDbMaster))
    {
        $oDbMaster = new pgBackRest::Db($iMasterRemoteIdx);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oDbMaster', value => $oDbMaster},
        {name => 'iDbMasterIdx', value => $iMasterRemoteIdx},
        {name => 'oDbStandby', value => $oDbStandby},
        {name => 'iDbStandbyIdx', value => $iStandbyIdx},
    );
}

push @EXPORT, qw(dbObjectGet);

####################################################################################################################################
# dbMasterGet
#
# Usually only the master database is required so this function makes getting it simple.  If in offline mode (which is true for a
# lot of archive operations) then the database returned is simply the first configured.
####################################################################################################################################
sub dbMasterGet
{
    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '::dbMasterGet');

    my ($oDbMaster) = dbObjectGet({bMasterOnly => true});

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oDbMaster', value => $oDbMaster, trace => true},
    );
}

push @EXPORT, qw(dbMasterGet);

####################################################################################################################################
# multipleDb
#
# Helper function to determine if there is more than one database defined.
####################################################################################################################################
sub multipleDb
{
    for (my $iDbPathIdx = 2; $iDbPathIdx <= cfgOptionIndexTotal(CFGOPT_PG_PATH); $iDbPathIdx++)
    {
        # If an index exists above 1 then return true
        if (cfgOptionTest(cfgOptionIdFromIndex(CFGOPT_PG_PATH, $iDbPathIdx)))
        {
            return true;
        }
    }

    return false;
}

1;
