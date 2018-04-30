####################################################################################################################################
# Create and manage protocol objects.
####################################################################################################################################
package pgBackRest::Protocol::Helper;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::Protocol::Remote::Master;
use pgBackRest::Version;

####################################################################################################################################
# Operation constants
####################################################################################################################################
# Backup module
use constant OP_BACKUP_FILE                                          => 'backupFile';
    push @EXPORT, qw(OP_BACKUP_FILE);

# Archive Module
use constant OP_ARCHIVE_GET_CHECK                                   => 'archiveCheck';
    push @EXPORT, qw(OP_ARCHIVE_GET_CHECK);
use constant OP_ARCHIVE_PUSH_CHECK                                  => 'archivePushCheck';
    push @EXPORT, qw(OP_ARCHIVE_PUSH_CHECK);

# Archive Push Async Module
use constant OP_ARCHIVE_PUSH_ASYNC                                  => 'archivePushAsync';
    push @EXPORT, qw(OP_ARCHIVE_PUSH_ASYNC);

# Archive File Module
use constant OP_ARCHIVE_PUSH_FILE                                   => 'archivePushFile';
    push @EXPORT, qw(OP_ARCHIVE_PUSH_FILE);

# Check Module
use constant OP_CHECK_BACKUP_INFO_CHECK                             => 'backupInfoCheck';
    push @EXPORT, qw(OP_CHECK_BACKUP_INFO_CHECK);

# Db Module
use constant OP_DB_CONNECT                                          => 'dbConnect';
    push @EXPORT, qw(OP_DB_CONNECT);
use constant OP_DB_EXECUTE_SQL                                      => 'dbExecSql';
    push @EXPORT, qw(OP_DB_EXECUTE_SQL);
use constant OP_DB_INFO                                             => 'dbInfo';
    push @EXPORT, qw(OP_DB_INFO);

# Storage Module
use constant OP_STORAGE_OPEN_READ                                   => 'storageOpenRead';
    push @EXPORT, qw(OP_STORAGE_OPEN_READ);
use constant OP_STORAGE_OPEN_WRITE                                  => 'storageOpenWrite';
    push @EXPORT, qw(OP_STORAGE_OPEN_WRITE);
use constant OP_STORAGE_CIPHER_PASS_USER                            => 'storageCipherPassUser';
    push @EXPORT, qw(OP_STORAGE_CIPHER_PASS_USER);
use constant OP_STORAGE_EXISTS                                      => 'storageExists';
    push @EXPORT, qw(OP_STORAGE_EXISTS);
use constant OP_STORAGE_LIST                                        => 'storageList';
    push @EXPORT, qw(OP_STORAGE_LIST);
use constant OP_STORAGE_MANIFEST                                    => 'storageManifest';
    push @EXPORT, qw(OP_STORAGE_MANIFEST);
use constant OP_STORAGE_MOVE                                        => 'storageMove';
    push @EXPORT, qw(OP_STORAGE_MOVE);
use constant OP_STORAGE_PATH_GET                                    => 'storagePathGet';
    push @EXPORT, qw(OP_STORAGE_PATH_GET);

# Info module
use constant OP_INFO_STANZA_LIST                                    => 'infoStanzList';
    push @EXPORT, qw(OP_INFO_STANZA_LIST);

# Restore module
use constant OP_RESTORE_FILE                                         => 'restoreFile';
    push @EXPORT, qw(OP_RESTORE_FILE);

# Wait
use constant OP_WAIT                                                 => 'wait';
    push @EXPORT, qw(OP_WAIT);

####################################################################################################################################
# Module variables
####################################################################################################################################
my $hProtocol = {};         # Global remote hash that is created on first request

####################################################################################################################################
# isRepoLocal
#
# Is the backup/archive repository local?  This does not take into account the spool path.
####################################################################################################################################
sub isRepoLocal
{
    # Not valid for remote
    if (cfgCommandTest(CFGCMD_REMOTE) && !cfgOptionTest(CFGOPT_TYPE, CFGOPTVAL_REMOTE_TYPE_BACKUP))
    {
        confess &log(ASSERT, 'isRepoLocal() not valid on ' . cfgOption(CFGOPT_TYPE) . ' remote');
    }

    return cfgOptionTest(CFGOPT_REPO_HOST) ? false : true;
}

push @EXPORT, qw(isRepoLocal);

####################################################################################################################################
# isDbLocal - is the database local?
####################################################################################################################################
sub isDbLocal
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $iRemoteIdx,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::isDbLocal', \@_,
            {name => 'iRemoteIdx', optional => true, default => cfgOptionValid(CFGOPT_HOST_ID) ? cfgOption(CFGOPT_HOST_ID) : 1,
                trace => true},
        );

    # Not valid for remote
    if (cfgCommandTest(CFGCMD_REMOTE) && !cfgOptionTest(CFGOPT_TYPE, CFGOPTVAL_REMOTE_TYPE_DB))
    {
        confess &log(ASSERT, 'isDbLocal() not valid on ' . cfgOption(CFGOPT_TYPE) . ' remote');
    }

    my $bLocal = cfgOptionTest(cfgOptionIdFromIndex(CFGOPT_PG_HOST, $iRemoteIdx)) ? false : true;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bLocal', value => $bLocal, trace => true}
    );
}

push @EXPORT, qw(isDbLocal);

####################################################################################################################################
# Gets the parameters required to setup the protocol
####################################################################################################################################
sub protocolParam
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strCommand,
        $strRemoteType,
        $iRemoteIdx,
        $strBackRestBin,
        $iProcessIdx,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::protocolParam', \@_,
            {name => 'strCommand'},
            {name => 'strRemoteType'},
            {name => 'iRemoteIdx', default => cfgOptionValid(CFGOPT_HOST_ID) ? cfgOption(CFGOPT_HOST_ID) : 1},
            {name => 'strBackRestBin', optional => true},
            {name => 'iProcessIdx', optional => true},
        );

    # Return the remote when required
    my $iOptionIdCmd = CFGOPT_REPO_HOST_CMD;
    my $iOptionIdConfig = CFGOPT_REPO_HOST_CONFIG;
    my $iOptionIdConfigIncludePath = CFGOPT_REPO_HOST_CONFIG_INCLUDE_PATH;
    my $iOptionIdConfigPath = CFGOPT_REPO_HOST_CONFIG_PATH;
    my $iOptionIdHost = CFGOPT_REPO_HOST;
    my $iOptionIdUser = CFGOPT_REPO_HOST_USER;
    my $strOptionDbPath = undef;
    my $strOptionDbPort = undef;
    my $strOptionDbSocketPath = undef;
    my $strOptionSshPort = CFGOPT_REPO_HOST_PORT;

    if ($strRemoteType eq CFGOPTVAL_REMOTE_TYPE_DB)
    {
        $iOptionIdCmd = cfgOptionIdFromIndex(CFGOPT_PG_HOST_CMD, $iRemoteIdx);
        $iOptionIdConfig = cfgOptionIdFromIndex(CFGOPT_PG_HOST_CONFIG, $iRemoteIdx);
        $iOptionIdConfigIncludePath = cfgOptionIdFromIndex(CFGOPT_PG_HOST_CONFIG_INCLUDE_PATH, $iRemoteIdx);
        $iOptionIdConfigPath = cfgOptionIdFromIndex(CFGOPT_PG_HOST_CONFIG_PATH, $iRemoteIdx);
        $iOptionIdHost = cfgOptionIdFromIndex(CFGOPT_PG_HOST, $iRemoteIdx);
        $iOptionIdUser = cfgOptionIdFromIndex(CFGOPT_PG_HOST_USER, $iRemoteIdx);
        $strOptionSshPort = cfgOptionIdFromIndex(CFGOPT_PG_HOST_PORT, $iRemoteIdx);
    }

    # Db path is not valid in all contexts (restore, for instance)
    if (cfgOptionValid(cfgOptionIdFromIndex(CFGOPT_PG_PATH, $iRemoteIdx)))
    {
        $strOptionDbPath =
            cfgOptionSource(cfgOptionIdFromIndex(CFGOPT_PG_PATH, $iRemoteIdx)) eq CFGDEF_SOURCE_DEFAULT ?
                undef : cfgOption(cfgOptionIdFromIndex(CFGOPT_PG_PATH, $iRemoteIdx));
    }

    # Db port is not valid in all contexts (restore, for instance)
    if (cfgOptionValid(cfgOptionIdFromIndex(CFGOPT_PG_PORT, $iRemoteIdx)))
    {
        $strOptionDbPort =
            cfgOptionSource(cfgOptionIdFromIndex(CFGOPT_PG_PORT, $iRemoteIdx)) eq CFGDEF_SOURCE_DEFAULT ?
                undef : cfgOption(cfgOptionIdFromIndex(CFGOPT_PG_PORT, $iRemoteIdx));
    }

    # Db socket is not valid in all contexts (restore, for instance)
    if (cfgOptionValid(cfgOptionIdFromIndex(CFGOPT_PG_SOCKET_PATH, $iRemoteIdx)))
    {
        $strOptionDbSocketPath =
            cfgOptionSource(cfgOptionIdFromIndex(CFGOPT_PG_SOCKET_PATH, $iRemoteIdx)) eq CFGDEF_SOURCE_DEFAULT ?
                undef : cfgOption(cfgOptionIdFromIndex(CFGOPT_PG_SOCKET_PATH, $iRemoteIdx));
    }

    # Build hash to set and override command options
    my $rhCommandOption =
    {
        &CFGOPT_COMMAND => {value => $strCommand},
        &CFGOPT_PROCESS => {value => $iProcessIdx},
        &CFGOPT_CONFIG =>
            {value => cfgOptionValid($iOptionIdConfig) && cfgOptionSource($iOptionIdConfig) eq CFGDEF_SOURCE_DEFAULT ?
                undef : cfgOption($iOptionIdConfig)},
        &CFGOPT_CONFIG_INCLUDE_PATH =>
            {value => cfgOptionValid($iOptionIdConfigIncludePath) &&
                cfgOptionSource($iOptionIdConfigIncludePath) eq CFGDEF_SOURCE_DEFAULT ?
                    undef : cfgOption($iOptionIdConfigIncludePath)},
        &CFGOPT_CONFIG_PATH =>
            {value => cfgOptionValid($iOptionIdConfigPath) && cfgOptionSource($iOptionIdConfigPath) eq CFGDEF_SOURCE_DEFAULT ?
                undef : cfgOption($iOptionIdConfigPath)},
        &CFGOPT_TYPE => {value => $strRemoteType},
        &CFGOPT_LOG_PATH => {},
        &CFGOPT_LOCK_PATH => {},

        # Don't pass CFGOPT_LOG_LEVEL_STDERR because in the case of the local process calling the remote process the
        # option will be set to 'protocol' which is not a valid value from the command line.
        &CFGOPT_LOG_LEVEL_STDERR => {},

        cfgOptionIdFromIndex(CFGOPT_PG_PATH, 1) => {value => $strOptionDbPath},
        cfgOptionIdFromIndex(CFGOPT_PG_PORT, 1) => {value => $strOptionDbPort},
        cfgOptionIdFromIndex(CFGOPT_PG_SOCKET_PATH, 1) => {value => $strOptionDbSocketPath},

        # Set protocol options explicitly so values are not picked up from remote config files
        &CFGOPT_BUFFER_SIZE =>  {value => cfgOption(CFGOPT_BUFFER_SIZE)},
        &CFGOPT_COMPRESS_LEVEL =>  {value => cfgOption(CFGOPT_COMPRESS_LEVEL)},
        &CFGOPT_COMPRESS_LEVEL_NETWORK =>  {value => cfgOption(CFGOPT_COMPRESS_LEVEL_NETWORK)},
        &CFGOPT_PROTOCOL_TIMEOUT =>  {value => cfgOption(CFGOPT_PROTOCOL_TIMEOUT)}
    };

    # Override some per-db options that shouldn't be passed to the command. ??? This could be done better as a new define
    # for these options which would then be implemented in cfgCommandWrite().
    for (my $iOptionIdx = 1; $iOptionIdx <= cfgOptionIndexTotal(CFGOPT_PG_HOST); $iOptionIdx++)
    {
        if ($iOptionIdx != 1)
        {
            $rhCommandOption->{cfgOptionIdFromIndex(CFGOPT_PG_HOST_CONFIG, $iOptionIdx)} = {};
            $rhCommandOption->{cfgOptionIdFromIndex(CFGOPT_PG_HOST_CONFIG_INCLUDE_PATH, $iOptionIdx)} = {};
            $rhCommandOption->{cfgOptionIdFromIndex(CFGOPT_PG_HOST_CONFIG_PATH, $iOptionIdx)} = {};
            $rhCommandOption->{cfgOptionIdFromIndex(CFGOPT_PG_HOST, $iOptionIdx)} = {};
            $rhCommandOption->{cfgOptionIdFromIndex(CFGOPT_PG_PATH, $iOptionIdx)} = {};
            $rhCommandOption->{cfgOptionIdFromIndex(CFGOPT_PG_PORT, $iOptionIdx)} = {};
            $rhCommandOption->{cfgOptionIdFromIndex(CFGOPT_PG_SOCKET_PATH, $iOptionIdx)} = {};
        }

        $rhCommandOption->{cfgOptionIdFromIndex(CFGOPT_PG_HOST_CMD, $iOptionIdx)} = {};
        $rhCommandOption->{cfgOptionIdFromIndex(CFGOPT_PG_HOST_USER, $iOptionIdx)} = {};
        $rhCommandOption->{cfgOptionIdFromIndex(CFGOPT_PG_HOST_PORT, $iOptionIdx)} = {};
    }

    # Generate the remote command
    my $strRemoteCommand = cfgCommandWrite(
        CFGCMD_REMOTE, true, defined($strBackRestBin) ? $strBackRestBin : cfgOption($iOptionIdCmd), undef, $rhCommandOption);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strRemoteHost', value => cfgOption($iOptionIdHost)},
        {name => 'strRemoteHostUser', value => cfgOption($iOptionIdUser)},
        {name => 'strRemoteHostSshPort', value => cfgOption($strOptionSshPort, false)},
        {name => 'strRemoteCommand', value => $strRemoteCommand},
    );
}

####################################################################################################################################
# protocolGet
#
# Get the protocol object or create it if does not exist.  Shared protocol objects are used because they create an SSH connection
# to the remote host and the number of these connections should be minimized.
####################################################################################################################################
sub protocolGet
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strRemoteType,
        $iRemoteIdx,
        $bCache,
        $strBackRestBin,
        $iProcessIdx,
        $strCommand,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::protocolGet', \@_,
            {name => 'strRemoteType'},
            {name => 'iRemoteIdx', default => cfgOptionValid(CFGOPT_HOST_ID) ? cfgOption(CFGOPT_HOST_ID) : 1},
            {name => 'bCache', optional => true, default => true},
            {name => 'strBackRestBin', optional => true},
            {name => 'iProcessIdx', optional => true},
            {name => 'strCommand', optional => true, default => cfgCommandName(cfgCommandGet())},
        );

    # Protocol object
    my $oProtocol;

    # If no remote requested or if the requested remote type is local then return a local protocol object
    if (!cfgOptionTest(
            cfgOptionIdFromIndex($strRemoteType eq CFGOPTVAL_REMOTE_TYPE_DB ? CFGOPT_PG_HOST : CFGOPT_REPO_HOST, $iRemoteIdx)))
    {
        confess &log(ASSERT, 'protocol cannot be created when remote host is not specified');
    }
    # Else create the remote protocol
    else
    {
        # Set protocol to cached value
        $oProtocol =
            $bCache && defined($$hProtocol{$strRemoteType}{$iRemoteIdx}) ? $$hProtocol{$strRemoteType}{$iRemoteIdx} : undef;

        if ($bCache && $$hProtocol{$strRemoteType}{$iRemoteIdx})
        {
            $oProtocol = $$hProtocol{$strRemoteType}{$iRemoteIdx};
            logDebugMisc($strOperation, 'found cached protocol');

            # Issue a noop on protocol pulled from the cache to be sure it is still functioning.  It's better to get an error at
            # request time than somewhere randomly later.
            $oProtocol->noOp();
        }

        # If protocol was not returned from cache then create it
        if (!defined($oProtocol))
        {
            logDebugMisc($strOperation, 'create (' . ($bCache ? '' : 'un') . 'cached) remote protocol');

            my ($strRemoteHost, $strRemoteHostUser, $strRemoteHostSshPort, $strRemoteCommand) = protocolParam(
                $strCommand, $strRemoteType, $iRemoteIdx, {strBackRestBin => $strBackRestBin, iProcessIdx => $iProcessIdx});

            $oProtocol = new pgBackRest::Protocol::Remote::Master
            (
                cfgOption(CFGOPT_CMD_SSH),
                $strRemoteCommand,
                cfgOption(CFGOPT_BUFFER_SIZE),
                cfgOption(CFGOPT_COMPRESS_LEVEL),
                cfgOption(CFGOPT_COMPRESS_LEVEL_NETWORK),
                $strRemoteHost,
                $strRemoteHostUser,
                $strRemoteHostSshPort,
                cfgOption(CFGOPT_PROTOCOL_TIMEOUT)
            );

            # Cache the protocol
            if ($bCache)
            {
                $$hProtocol{$strRemoteType}{$iRemoteIdx} = $oProtocol;
            }
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oProtocol', value => $oProtocol, trace => true}
    );
}

push @EXPORT, qw(protocolGet);

####################################################################################################################################
# protocolList - list all active protocols
####################################################################################################################################
sub protocolList
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strRemoteType,
        $iRemoteIdx,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::protocolList', \@_,
            {name => 'strRemoteType', required => false, trace => true},
            {name => 'iRemoteIdx', required => false, trace => true},
        );

    my @oyProtocol;

    if (!defined($strRemoteType))
    {
        foreach my $strRemoteType (sort(keys(%{$hProtocol})))
        {
            push(@oyProtocol, protocolList($strRemoteType));
        }
    }
    elsif (!defined($iRemoteIdx))
    {
        foreach my $iRemoteIdx (sort(keys(%{$hProtocol->{$strRemoteType}})))
        {
            push(@oyProtocol, protocolList($strRemoteType, $iRemoteIdx));
        }
    }
    elsif (defined($hProtocol->{$strRemoteType}{$iRemoteIdx}))
    {
        push(@oyProtocol, {strRemoteType => $strRemoteType, iRemoteIdx => $iRemoteIdx});
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oyProtocol', value => \@oyProtocol, trace => true}
    );
}

####################################################################################################################################
# protocolDestroy
#
# Undefine the protocol if it is stored locally.
####################################################################################################################################
sub protocolDestroy
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strRemoteType,
        $iRemoteIdx,
        $bComplete,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::protocolDestroy', \@_,
            {name => 'strRemoteType', required => false},
            {name => 'iRemoteIdx', required => false},
            {name => 'bComplete', default => false},
        );

    my $iExitStatus = 0;

    foreach my $rhProtocol (protocolList($strRemoteType, $iRemoteIdx))
    {
        logDebugMisc(
            $strOperation, 'found cached protocol',
            {name => 'strRemoteType', value => $rhProtocol->{strRemoteType}},
            {name => 'iRemoteIdx', value => $rhProtocol->{iRemoteIdx}});

        $iExitStatus = $hProtocol->{$rhProtocol->{strRemoteType}}{$rhProtocol->{iRemoteIdx}}->close($bComplete);
        delete($hProtocol->{$rhProtocol->{strRemoteType}}{$rhProtocol->{iRemoteIdx}});
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iExitStatus', value => $iExitStatus}
    );
}

push @EXPORT, qw(protocolDestroy);

####################################################################################################################################
# protocolKeepAlive - call keepAlive() on all protocols
####################################################################################################################################
sub protocolKeepAlive
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strRemoteType,
        $iRemoteIdx,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::protocolDestroy', \@_,
            {name => 'strRemoteType', required => false, trace => true},
            {name => 'iRemoteIdx', required => false, trace => true},
        );

    foreach my $rhProtocol (protocolList($strRemoteType, $iRemoteIdx))
    {
        $hProtocol->{$rhProtocol->{strRemoteType}}{$rhProtocol->{iRemoteIdx}}->keepAlive();
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

push @EXPORT, qw(protocolKeepAlive);

1;
