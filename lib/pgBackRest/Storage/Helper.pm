####################################################################################################################################
# Local Storage Helper
####################################################################################################################################
package pgBackRest::Storage::Helper;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(basename);

use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::Storage::Posix::Driver;
use pgBackRest::Storage::Local;
use pgBackRest::Storage::Storage;
use pgBackRest::Version;

####################################################################################################################################
# Storage constants
####################################################################################################################################
use constant STORAGE_LOCAL                                          => '<LOCAL>';
    push @EXPORT, qw(STORAGE_LOCAL);

####################################################################################################################################
# Compression extension
####################################################################################################################################
use constant COMPRESS_EXT                                           => 'gz';
    push @EXPORT, qw(COMPRESS_EXT);

####################################################################################################################################
# Temp file extension
####################################################################################################################################
use constant STORAGE_TEMP_EXT                                       => PROJECT_EXE . '.tmp';
    push @EXPORT, qw(STORAGE_TEMP_EXT);

####################################################################################################################################
# storageLocal - get local storage
#
# Local storage is generally read-only (except for locking) and can never reference a remote path.  Used for adhoc activities like
# reading pgbackrest.conf.
####################################################################################################################################
sub storageLocal
{
    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '::storageLocal');

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oStorageLocal', value => new pgBackRest::Storage::Storage(STORAGE_LOCAL), trace => true},
    );
}

push @EXPORT, qw(storageLocal);

1;
