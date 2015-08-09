####################################################################################################################################
# FILE COMMON    MODULE
####################################################################################################################################
package BackRest::FileCommon;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
use File::Basename qw(dirname);
use IO::Handle;

use lib dirname($0) . '/../lib';
use BackRest::Exception;
use BackRest::Utility;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_FILE_COMMON                                       => 'FileCommon';

use constant OP_FILE_COMMON_PATH_SYNC                             => OP_FILE_COMMON . '::filePathSync';

####################################################################################################################################
# filePathSync
#
# Sync a directory.
####################################################################################################################################
sub filePathSync
{
    my $strPath = shift;

    logTrace(OP_FILE_COMMON_PATH_SYNC, DEBUG_CALL, undef, {path => \$strPath});

    open(my $hPath, "<", $strPath)
        or confess &log(ERROR, "unable to open ${strPath}", ERROR_PATH_OPEN);
    open(my $hPathDup, ">&", $hPath)
        or confess &log(ERROR, "unable to duplicate handle for ${strPath}", ERROR_PATH_OPEN);

    $hPathDup->sync
        or confess &log(ERROR, "unable to sync ${strPath}", ERROR_PATH_SYNC);
}

our @EXPORT = qw(filePathSync);

1;
