####################################################################################################################################
# Db & Repository Storage Helper
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
use pgBackRest::LibC qw(:config);
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
            {name => 'iRemoteIdx', optional => true, default => cfgOptionValid(CFGOPT_HOST_ID) ? cfgOption(CFGOPT_HOST_ID) : 1,
                trace => true},
        );

    # Create storage if not defined
    if (!defined($hStorage->{&STORAGE_DB}{$iRemoteIdx}))
    {
        if (isDbLocal({iRemoteIdx => $iRemoteIdx}))
        {
            $hStorage->{&STORAGE_DB}{$iRemoteIdx} = new pgBackRest::Storage::Local(
                cfgOption(cfgOptionIndex(CFGOPT_DB_PATH, $iRemoteIdx)), new pgBackRest::Storage::Posix::Driver(),
                {strTempExtension => STORAGE_TEMP_EXT, lBufferMax => cfgOption(CFGOPT_BUFFER_SIZE)});
        }
        else
        {
            $hStorage->{&STORAGE_DB}{$iRemoteIdx} = new pgBackRest::Protocol::Storage::Remote(
                protocolGet(CFGOPTVAL_REMOTE_TYPE_DB, $iRemoteIdx));
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
        if (cfgOptionValid(CFGOPT_STANZA) && cfgOptionTest(CFGOPT_STANZA))
        {
            $strStanza = cfgOption(CFGOPT_STANZA);
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

            # Create the driver
            my $oDriver;

            if (cfgOptionTest(CFGOPT_REPO_TYPE, CFGOPTVAL_REPO_TYPE_S3))
            {
                require pgBackRest::Storage::S3::Driver;

                $oDriver = new pgBackRest::Storage::S3::Driver(
                    cfgOption(CFGOPT_REPO_S3_BUCKET), cfgOption(CFGOPT_REPO_S3_ENDPOINT), cfgOption(CFGOPT_REPO_S3_REGION),
                    cfgOption(CFGOPT_REPO_S3_KEY), cfgOption(CFGOPT_REPO_S3_KEY_SECRET),
                    {strHost => cfgOption(CFGOPT_REPO_S3_HOST, false), bVerifySsl => cfgOption(CFGOPT_REPO_S3_VERIFY_SSL, false),
                        strCaPath => cfgOption(CFGOPT_REPO_S3_CA_PATH, false),
                        strCaFile => cfgOption(CFGOPT_REPO_S3_CA_FILE, false), lBufferMax => cfgOption(CFGOPT_BUFFER_SIZE)});
            }
            elsif (cfgOptionTest(CFGOPT_REPO_TYPE, CFGOPTVAL_REPO_TYPE_CIFS))
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
                cfgOption(CFGOPT_REPO_PATH), $oDriver,
                {strTempExtension => STORAGE_TEMP_EXT, hRule => $hRule, lBufferMax => cfgOption(CFGOPT_BUFFER_SIZE)});
        }
        else
        {
            # Create remote storage
            $hStorage->{&STORAGE_REPO}{$strStanza} = new pgBackRest::Protocol::Storage::Remote(
                protocolGet(CFGOPTVAL_REMOTE_TYPE_BACKUP));
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
