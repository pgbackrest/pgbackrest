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

    # Make sure the archive push command happens on the db side
    if (!isDbLocal())
    {
        confess &log(ERROR, CMD_ARCHIVE_PUSH . ' operation must run on db host', ERROR_HOST_INVALID);
    }

    if (!defined($strWalPathFile))
    {
        confess &log(ERROR, 'WAL file to push required', ERROR_PARAM_REQUIRED);
    }

    # Check for a stop lock
    lockStopTest();

    # Extract WAL path and file
    my $strWalPath = dirname(walPath($strWalPathFile, optionGet(OPTION_DB_PATH, false), commandGet()));
    my $strWalFile = basename($strWalPathFile);

    # Is the async client or server?
    my $bClient = true;

    # Start the async process and wait for WAL to complete
    if (optionGet(OPTION_ARCHIVE_ASYNC))
    {
        # Get the spool path
        $self->{strSpoolPath} = storageSpool()->pathGet(STORAGE_SPOOL_ARCHIVE_OUT);

        # Loop to check for status files and launch async process
        my $bPushed = false;
        my $oWait = waitInit(optionGet(OPTION_ARCHIVE_TIMEOUT));
        $self->{bConfessOnError} = false;

        do
        {
            # Check WAL status
            $bPushed = $self->walStatus($self->{strSpoolPath}, $strWalFile, $self->{bConfessOnError});

            # If not found then launch async process
            if (!$bPushed)
            {
                # Load module dynamically
                require pgBackRest::Archive::Push::Async;
                $bClient = (new pgBackRest::Archive::Push::Async(
                    $strWalPath, $self->{strSpoolPath}, $self->{strBackRestBin}))->process();
            }

            $self->{bConfessOnError} = true;
        }
        while ($bClient && !$bPushed && waitMore($oWait));

        if (!$bPushed && $bClient)
        {
            confess &log(ERROR,
                "unable to push WAL ${strWalFile} asynchronously after " . optionGet(OPTION_ARCHIVE_TIMEOUT) . " second(s)",
                ERROR_ARCHIVE_TIMEOUT);
        }
    }
    # Else push synchronously
    else
    {
        # Load module dynamically
        require pgBackRest::Archive::Push::File;
        pgBackRest::Archive::Push::File->import();

        # Drop file if queue max has been exceeded
        $self->{strWalPath} = $strWalPath;

        if (optionTest(OPTION_ARCHIVE_QUEUE_MAX) && @{$self->dropList($self->readyList())} > 0)
        {
            &log(WARN,
                "dropped WAL file ${strWalFile} because archive queue exceeded " . optionGet(OPTION_ARCHIVE_QUEUE_MAX) . ' bytes');
        }
        # Else push the WAL file
        else
        {
            archivePushFile($strWalPath, $strWalFile, optionGet(OPTION_COMPRESS));
        }
    }

    # Only print the message if this is the async client or the WAL file was pushed synchronously
    if ($bClient)
    {
        &log(INFO, "pushed WAL segment ${strWalFile}" . (optionGet(OPTION_ARCHIVE_ASYNC) ? ' asynchronously' : ''));
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iResult', value => 0, trace => true}
    );
}

####################################################################################################################################
# walStatus
#
# Read a WAL status file and return success or raise a warning or error.
####################################################################################################################################
sub walStatus
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strSpoolPath,
        $strWalFile,
        $bConfessOnError,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->walStatus', \@_,
            {name => 'strSpoolPath'},
            {name => 'strWalFile'},
            {name => 'bConfessOnError', default => true},
        );

    # Default result is false
    my $bResult = false;

    # Find matching status files
    my @stryStatusFile = storageSpool()->list(
        $strSpoolPath, {strExpression => '^' . $strWalFile . '\.(ok|error)$', bIgnoreMissing => true});

    if (@stryStatusFile > 0)
    {
        # If more than one status file was found then assert - this could be a bug in the async process
        if (@stryStatusFile > 1)
        {
            confess &log(ASSERT,
                "multiple status files found in ${strSpoolPath} for ${strWalFile}: " . join(', ', @stryStatusFile));
        }

        # Read the status file
        my $rstrWalStatus = storageSpool()->get("${strSpoolPath}/$stryStatusFile[0]");
        my @stryWalStatus = split("\n", defined($$rstrWalStatus) ? $$rstrWalStatus : '');

        # Status file must have at least two lines if it has content
        my $iCode;
        my $strMessage;

        # Parse status content
        if (@stryWalStatus != 0)
        {
            if (@stryWalStatus < 2)
            {
                confess &log(ASSERT, "$stryStatusFile[0] content must have at least two lines:\n" . join("\n", @stryWalStatus));
            }

            $iCode = shift(@stryWalStatus);
            $strMessage = join("\n", @stryWalStatus);
        }

        # Process ok files
        if ($stryStatusFile[0] =~ /\.ok$/)
        {
            # If there is content in the status file it is a warning
            if (@stryWalStatus != 0)
            {
                # If error code is not success, then this was a renamed .error file
                if ($iCode != 0)
                {
                    $strMessage =
                        "WAL segment ${strWalFile} was not pushed due to error and was manually skipped:\n" . $strMessage;
                }

                &log(WARN, $strMessage);
            }

            $bResult = true;
        }
        # Process error files
        elsif ($bConfessOnError)
        {
            # Error files must have content
            if (@stryWalStatus == 0)
            {
                confess &log(ASSERT, "$stryStatusFile[0] has no content");
            }

            confess &log(ERROR, $strMessage, $iCode);
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
    if (@{$stryReadyFile} > int(optionGet(OPTION_ARCHIVE_QUEUE_MAX) / PG_WAL_SIZE))
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
