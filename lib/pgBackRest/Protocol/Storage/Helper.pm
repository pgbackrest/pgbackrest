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
use pgBackRest::LibC qw(:storage);
use pgBackRest::Storage::Helper;

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

    # Create storage if not defined
    if (!defined($hStorage->{&STORAGE_REPO}))
    {
        $hStorage->{&STORAGE_REPO} = new pgBackRest::Storage::Storage(
            STORAGE_REPO, {lBufferMax => cfgOption(CFGOPT_BUFFER_SIZE)});
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
# Clear the repo storage cache - FOR TESTING ONLY!
####################################################################################################################################
sub storageRepoCacheClear
{
    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '::storageRepoCacheClear');

    delete($hStorage->{&STORAGE_REPO});

    storageRepoFree();

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

push @EXPORT, qw(storageRepoCacheClear);

1;
