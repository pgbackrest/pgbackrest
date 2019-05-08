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
use pgBackRest::Protocol::Helper;
use pgBackRest::Protocol::Storage::Remote;
use pgBackRest::Storage::Base;
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
                cfgOption(cfgOptionIdFromIndex(CFGOPT_PG_PATH, $iRemoteIdx)), new pgBackRest::Storage::Posix::Driver(),
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
# storageRepo - get repository storage
####################################################################################################################################
sub storageRepo
{
    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '::storageLocal');

    # Create storage if not defined
    if (!defined($hStorage->{&STORAGE_REPO}))
    {
        $hStorage->{&STORAGE_REPO} = new pgBackRest::Storage::Storage(STORAGE_REPO);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oStorageRepo', value => $hStorage->{&STORAGE_REPO}, trace => true},
    );
}

push @EXPORT, qw(storageRepo);

####################################################################################################################################
# storageRepoCacheClear - FOR TESTING ONLY!
####################################################################################################################################
sub storageRepoCacheClear
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strStanza,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::storageRepoCacheClear', \@_,
            {name => 'strStanza'},
        );

    delete($hStorage->{&STORAGE_REPO});

    storageRepoFree();

    return;
}

push @EXPORT, qw(storageRepoCacheClear);

1;
