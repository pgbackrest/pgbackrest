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
use constant OP_ARCHIVE_GET_ARCHIVE_ID                              => 'archiveId';
    push @EXPORT, qw(OP_ARCHIVE_GET_ARCHIVE_ID);
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
    if (commandTest(CMD_REMOTE) && !optionTest(OPTION_TYPE, BACKUP))
    {
        confess &log(ASSERT, 'isRepoLocal() not valid on ' . optionGet(OPTION_TYPE) . ' remote');
    }

    return optionTest(OPTION_BACKUP_HOST) ? false : true;
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
            {name => 'iRemoteIdx', optional => true, default => optionValid(OPTION_HOST_ID) ? optionGet(OPTION_HOST_ID) : 1,
                trace => true},
        );

    # Not valid for remote
    if (commandTest(CMD_REMOTE) && !optionTest(OPTION_TYPE, DB))
    {
        confess &log(ASSERT, 'isDbLocal() not valid on ' . optionGet(OPTION_TYPE) . ' remote');
    }

    my $bLocal = optionTest(optionIndex(OPTION_DB_HOST, $iRemoteIdx)) ? false : true;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bLocal', value => $bLocal, trace => true}
    );
}

push @EXPORT, qw(isDbLocal);

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
            {name => 'iRemoteIdx', default => optionValid(OPTION_HOST_ID) ? optionGet(OPTION_HOST_ID) : 1},
            {name => 'bCache', optional => true, default => true},
            {name => 'strBackRestBin', optional => true},
            {name => 'iProcessIdx', optional => true},
            {name => 'strCommand', optional => true, default => commandGet()},
        );

    # Protocol object
    my $oProtocol;

    # If no remote requested or if the requested remote type is local then return a local protocol object
    my $strRemoteHost = $strRemoteType eq NONE ? undef : optionIndex("${strRemoteType}-host", $iRemoteIdx);

    if ($strRemoteType eq NONE || !optionTest($strRemoteHost))
    {
        confess &log(ASSERT, 'protocol cannot be created when remote type is NONE or host is not specified');
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

            # Return the remote when required
            my $strOptionCmd = OPTION_BACKUP_CMD;
            my $strOptionConfig = OPTION_BACKUP_CONFIG;
            my $strOptionHost = OPTION_BACKUP_HOST;
            my $strOptionUser = OPTION_BACKUP_USER;
            my $strOptionDbPort = undef;
            my $strOptionDbSocketPath = undef;

            if ($strRemoteType eq DB)
            {
                $strOptionCmd = optionIndex(OPTION_DB_CMD, $iRemoteIdx);
                $strOptionConfig = optionIndex(OPTION_DB_CONFIG, $iRemoteIdx);
                $strOptionHost = optionIndex(OPTION_DB_HOST, $iRemoteIdx);
                $strOptionUser = optionIndex(OPTION_DB_USER, $iRemoteIdx);
            }

            # Db socket is not valid in all contexts (restore, for instance)
            if (optionValid(optionIndex(OPTION_DB_PORT, $iRemoteIdx)))
            {
                $strOptionDbPort =
                    optionSource(optionIndex(OPTION_DB_PORT, $iRemoteIdx)) eq SOURCE_DEFAULT ?
                        undef : optionGet(optionIndex(OPTION_DB_PORT, $iRemoteIdx));
            }

            # Db socket is not valid in all contexts (restore, for instance)
            if (optionValid(optionIndex(OPTION_DB_SOCKET_PATH, $iRemoteIdx)))
            {
                $strOptionDbSocketPath =
                    optionSource(optionIndex(OPTION_DB_SOCKET_PATH, $iRemoteIdx)) eq SOURCE_DEFAULT ?
                        undef : optionGet(optionIndex(OPTION_DB_SOCKET_PATH, $iRemoteIdx));
            }

            $oProtocol = new pgBackRest::Protocol::Remote::Master
            (
                $strRemoteType,
                optionGet(OPTION_CMD_SSH),
                commandWrite(
                    CMD_REMOTE, true,
                    defined($strBackRestBin) ? $strBackRestBin : optionGet($strOptionCmd), undef,
                    {
                        &OPTION_COMMAND => {value => $strCommand},
                        &OPTION_PROCESS => {value => $iProcessIdx},
                        &OPTION_CONFIG => {
                            value => optionSource($strOptionConfig) eq SOURCE_DEFAULT ? undef : optionGet($strOptionConfig)},
                        &OPTION_TYPE => {value => $strRemoteType},
                        &OPTION_LOG_PATH => {},
                        &OPTION_LOCK_PATH => {},
                        &OPTION_DB_PORT => {value => $strOptionDbPort},
                        &OPTION_DB_SOCKET_PATH => {value => $strOptionDbSocketPath},

                        # ??? Not very pretty but will work until there is nicer code in commandWrite to handle this case.  It
                        # doesn't hurt to pass these params but it's messy and distracting for debugging.
                        optionIndex(OPTION_DB_PORT, 2) => {},
                        optionIndex(OPTION_DB_SOCKET_PATH, 2) => {},

                        # Set protocol options explicitly so values are not picked up from remote config files
                        &OPTION_BUFFER_SIZE =>  {value => optionGet(OPTION_BUFFER_SIZE)},
                        &OPTION_COMPRESS_LEVEL =>  {value => optionGet(OPTION_COMPRESS_LEVEL)},
                        &OPTION_COMPRESS_LEVEL_NETWORK =>  {value => optionGet(OPTION_COMPRESS_LEVEL_NETWORK)},
                        &OPTION_PROTOCOL_TIMEOUT =>  {value => optionGet(OPTION_PROTOCOL_TIMEOUT)}
                    }),
                optionGet(OPTION_BUFFER_SIZE),
                optionGet(OPTION_COMPRESS_LEVEL),
                optionGet(OPTION_COMPRESS_LEVEL_NETWORK),
                optionGet($strOptionHost),
                optionGet($strOptionUser),
                optionGet(OPTION_PROTOCOL_TIMEOUT)
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
