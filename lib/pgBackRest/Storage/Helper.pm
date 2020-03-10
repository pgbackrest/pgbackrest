####################################################################################################################################
# Repository Storage Helper
####################################################################################################################################
package pgBackRest::Storage::Helper;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(basename);

use pgBackRest::Common::Log;
use pgBackRest::Storage::Storage;
use pgBackRest::Version;

####################################################################################################################################
# Temp file extension
####################################################################################################################################
use constant STORAGE_TEMP_EXT                                       => PROJECT_EXE . '.tmp';
    push @EXPORT, qw(STORAGE_TEMP_EXT);

####################################################################################################################################
# Cache storage so it can be retrieved quickly
####################################################################################################################################
my $oRepoStorage;

####################################################################################################################################
# storageRepoCommandSet
####################################################################################################################################
my $strStorageRepoCommand;
my $strStorageRepoType;

sub storageRepoCommandSet
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strCommand,
        $strStorageType,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::storageRepoCommandSet', \@_,
            {name => 'strCommand'},
            {name => 'strStorageType'},
        );

    $strStorageRepoCommand = $strCommand;
    $strStorageRepoType = $strStorageType;

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

push @EXPORT, qw(storageRepoCommandSet);

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
    if (!defined($oRepoStorage))
    {
        $oRepoStorage = new pgBackRest::Storage::Storage($strStorageRepoCommand, $strStorageRepoType, 64 * 1024, 60);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oStorageRepo', value => $oRepoStorage, trace => true},
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

    undef($oRepoStorage);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

push @EXPORT, qw(storageRepoCacheClear);

1;
