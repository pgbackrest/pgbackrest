####################################################################################################################################
# ARCHIVE PUSH MODULE
####################################################################################################################################
package pgBackRest::Archive::Push::Push;
use parent 'pgBackRest::Archive::Base';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(basename dirname);

use pgBackRest::Archive::Common;
use pgBackRest::DbVersion;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Lock;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::Protocol::Helper;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Storage::Helper;

####################################################################################################################################
# WAL status constants
####################################################################################################################################
use constant WAL_STATUS_ERROR                                       => 'error';
    push @EXPORT, qw(WAL_STATUS_ERROR);
use constant WAL_STATUS_OK                                          => 'ok';
    push @EXPORT, qw(WAL_STATUS_OK);

####################################################################################################################################
# process
#
# Push a WAL segment.  The WAL can be pushed in sync or async mode.
####################################################################################################################################
sub process
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strWalPathFile,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->process', \@_,
            {name => 'strWalPathFile', required => false},
        );

    # Make sure the command happens on the db side
    if (!isDbLocal())
    {
        confess &log(ERROR, cfgCommandName(CFGCMD_ARCHIVE_PUSH) . ' operation must run on db host', ERROR_HOST_INVALID);
    }

    if (!defined($strWalPathFile))
    {
        confess &log(ERROR, 'WAL file to push required', ERROR_PARAM_REQUIRED);
    }

    # Check for a stop lock
    lockStopTest();

    # Extract WAL path and file
    my $strWalPath = dirname(walPath($strWalPathFile, cfgOption(CFGOPT_PG_PATH, false), cfgCommandName(cfgCommandGet())));
    my $strWalFile = basename($strWalPathFile);

    # Start the async process and wait for WAL to complete
    if (cfgOption(CFGOPT_ARCHIVE_ASYNC))
    {
        # Load module dynamically
        require pgBackRest::Archive::Push::Async;
        (new pgBackRest::Archive::Push::Async(
            $strWalPath, storageSpool()->pathGet(STORAGE_SPOOL_ARCHIVE_OUT), $self->{strBackRestBin}))->process();
    }
    # Else push synchronously
    else
    {
        # Load module dynamically
        require pgBackRest::Archive::Push::File;
        pgBackRest::Archive::Push::File->import();

        # Drop file if queue max has been exceeded
        $self->{strWalPath} = $strWalPath;

        if (cfgOptionTest(CFGOPT_ARCHIVE_PUSH_QUEUE_MAX) && @{$self->dropList($self->readyList())} > 0)
        {
            &log(WARN,
                "dropped WAL file ${strWalFile} because archive queue exceeded " . cfgOption(CFGOPT_ARCHIVE_PUSH_QUEUE_MAX) . ' bytes');
        }
        # Else push the WAL file
        else
        {
            archivePushFile($strWalPath, $strWalFile, cfgOption(CFGOPT_COMPRESS), cfgOption(CFGOPT_COMPRESS_LEVEL));
            &log(INFO, "pushed WAL segment ${strWalFile}");
        }
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# readyList
#
# Determine which WAL PostgreSQL has marked as ready to be archived.  This is the heart of the "look ahead" functionality in async
# archiving.
####################################################################################################################################
sub readyList
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->readyList');

    # Read the .ok files
    my $hOkFile = {};

    if (defined($self->{strSpoolPath}))
    {
        foreach my $strOkFile (storageSpool()->list($self->{strSpoolPath}, {strExpression => '\.ok$', bIgnoreMissing => true}))
        {
            $strOkFile = substr($strOkFile, 0, length($strOkFile) - length('.ok'));
            $hOkFile->{$strOkFile} = true;
        }
    }

    # Read the .ready files
    my $strWalStatusPath = "$self->{strWalPath}/archive_status";
    my @stryReadyFile = storageDb()->list($strWalStatusPath, {strExpression => '\.ready$'});

    # Generate a list of new files
    my @stryNewReadyFile;
    my $hReadyFile = {};

    foreach my $strReadyFile (@stryReadyFile)
    {
        # Remove .ready extension
        $strReadyFile = substr($strReadyFile, 0, length($strReadyFile) - length('.ready'));

        # Add the file if it is not already queued or previously processed
        if (!defined($hOkFile->{$strReadyFile}))
        {
            # Push onto list of new files
            push(@stryNewReadyFile, $strReadyFile);
        }

        # Add to the ready hash for speed finding removed files
        $hReadyFile->{$strReadyFile} = true;
    }

    # Remove .ok files that are no longer in .ready state
    foreach my $strOkFile (sort(keys(%{$hOkFile})))
    {
        if (!defined($hReadyFile->{$strOkFile}))
        {
            storageSpool()->remove("$self->{strSpoolPath}/${strOkFile}.ok");
        }
    }

    return logDebugReturn
    (
        $strOperation,
        {name => 'stryWalFile', value => \@stryNewReadyFile, ref => true}
    );
}

####################################################################################################################################
# dropList
#
# Determine if the queue of WAL ready to be archived has grown larger the the user-configurable setting.  If so, return the list of
# WAL that should be dropped to allow PostgreSQL to continue running.  For the moment this is the entire list of ready WAL,
# otherwise the process may archive small spurts of WAL when at queue max which is not likely to be useful.
####################################################################################################################################
sub dropList
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $stryReadyFile,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->dropList', \@_,
            {name => 'stryReadyList'},
        );

    my $stryDropFile = [];

    # Determine if there are any to be dropped
    if (@{$stryReadyFile} > int(cfgOption(CFGOPT_ARCHIVE_PUSH_QUEUE_MAX) / PG_WAL_SIZE))
    {
        $stryDropFile = $stryReadyFile;
    }

    return logDebugReturn
    (
        $strOperation,
        {name => 'stryDropFile', value => $stryDropFile, ref => true}
    );
}

1;
