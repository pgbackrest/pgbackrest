####################################################################################################################################
# Helper functions for creating storage objects that can be either local or remote.
####################################################################################################################################
package pgBackRest::Protocol::Storage::Helper;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(basename);

use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::Protocol::Helper;
use pgBackRest::Protocol::Storage::Remote;
use pgBackRest::Storage::Helper;
use pgBackRest::Storage::Local;

####################################################################################################################################
# Storage constants
####################################################################################################################################
use constant STORAGE_DB                                             => '<DB>';
    push @EXPORT, qw(STORAGE_DB);

use constant STORAGE_REPO                                           => '<REPO>';
    push @EXPORT, qw(STORAGE_REPO);
use constant STORAGE_REPO_ARCHIVE                                   => '<REPO:ARCHIVE>';
    push @EXPORT, qw(STORAGE_REPO_ARCHIVE);
use constant STORAGE_REPO_BACKUP                                    => '<REPO:BACKUP>';
    push @EXPORT, qw(STORAGE_REPO_BACKUP);

####################################################################################################################################
# Cache storage so it can be retrieved quickly
####################################################################################################################################
my $hStorage;

####################################################################################################################################
# storageDb - get db storage
####################################################################################################################################
sub storageDb
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $iRemoteIdx,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::storageDb', \@_,
            {name => 'iRemoteIdx', optional => true, default => optionValid(OPTION_HOST_ID) ? optionGet(OPTION_HOST_ID) : 1,
                trace => true},
        );

    # Create storage if not defined
    if (!defined($hStorage->{&STORAGE_DB}{$iRemoteIdx}))
    {
        if (isDbLocal({iRemoteIdx => $iRemoteIdx}))
        {
            $hStorage->{&STORAGE_DB}{$iRemoteIdx} = new pgBackRest::Storage::Local(
                optionGet(optionIndex(OPTION_DB_PATH, $iRemoteIdx)), new pgBackRest::Storage::Posix::Driver(),
                {strTempExtension => STORAGE_TEMP_EXT, lBufferMax => optionGet(OPTION_BUFFER_SIZE)});
        }
        else
        {
            $hStorage->{&STORAGE_DB}{$iRemoteIdx} = new pgBackRest::Protocol::Storage::Remote(protocolGet(DB, $iRemoteIdx));
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oStorageDb', value => $hStorage->{&STORAGE_DB}{$iRemoteIdx}, trace => true},
    );
}

push @EXPORT, qw(storageDb);

####################################################################################################################################
# storageRepoRule - rules for paths in the repository
####################################################################################################################################
sub storageRepoRule
{
    my $strRule = shift;
    my $strFile = shift;
    my $strStanza = shift;

    # Result path and file
    my $strResultFile;

    # Return archive path
    if ($strRule eq STORAGE_REPO_ARCHIVE)
    {
        $strResultFile = "archive/${strStanza}";

        # If file is not defined nothing further to do
        if (defined($strFile))
        {
            my ($strArchiveId, $strWalFile) = split('/', $strFile);

            # If a WAL file (including .backup)
            if (defined($strWalFile) && $strWalFile =~ /^[0-F]{24}/)
            {
                $strResultFile .= "/${strArchiveId}/" . substr($strWalFile, 0, 16) . "/${strWalFile}";
            }
            # Else other files
            else
            {
                $strResultFile .= "/${strFile}";
            }
        }
    }
    # Return backup path
    elsif ($strRule eq STORAGE_REPO_BACKUP)
    {
        $strResultFile = "backup/${strStanza}" . (defined($strFile) ? "/${strFile}" : '');
    }
    # Else error
    else
    {
        confess &log(ASSERT, "invalid " . STORAGE_REPO . " storage rule ${strRule}");
    }

    return $strResultFile;
}

####################################################################################################################################
# storageRepo - get repository storage
####################################################################################################################################
sub storageRepo
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strStanza,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::storageRepo', \@_,
            {name => 'strStanza', optional => true, trace => true},
        );

    if (!defined($strStanza))
    {
        if (optionValid(OPTION_STANZA) && optionTest(OPTION_STANZA))
        {
            $strStanza = optionGet(OPTION_STANZA);
        }
        else
        {
            $strStanza = STORAGE_REPO;
        }
    }

    # Create storage if not defined
    if (!defined($hStorage->{&STORAGE_REPO}{$strStanza}))
    {
        if (isRepoLocal())
        {
            # Path rules
            my $hRule;

            if ($strStanza ne STORAGE_REPO)
            {
                $hRule =
                {
                    &STORAGE_REPO_ARCHIVE =>
                    {
                        fnRule => \&storageRepoRule,
                        xData => $strStanza,
                    },
                    &STORAGE_REPO_BACKUP =>
                    {
                        fnRule => \&storageRepoRule,
                        xData => $strStanza,
                    },
                }
            };

            # Select a driver
            my $oDriver;

            if (optionTest(OPTION_REPO_TYPE, REPO_TYPE_CIFS))
            {
                require pgBackRest::Storage::Cifs::Driver;

                $oDriver = new pgBackRest::Storage::Cifs::Driver();
            }
            else
            {
                $oDriver = new pgBackRest::Storage::Posix::Driver();
            }

            # Create local storage
            $hStorage->{&STORAGE_REPO}{$strStanza} = new pgBackRest::Storage::Local(
                optionGet(OPTION_REPO_PATH), $oDriver,
                {hRule => $hRule, bAllowTemp => false, lBufferMax => optionGet(OPTION_BUFFER_SIZE)});
        }
        else
        {
            # Create remote storage
            $hStorage->{&STORAGE_REPO}{$strStanza} = new pgBackRest::Protocol::Storage::Remote(protocolGet(BACKUP));
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oStorageRepo', value => $hStorage->{&STORAGE_REPO}{$strStanza}, trace => true},
    );
}

push @EXPORT, qw(storageRepo);

1;
