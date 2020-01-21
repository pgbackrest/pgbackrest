####################################################################################################################################
# VERSION MODULE
#
# Contains project version and format numbers.
####################################################################################################################################
package pgBackRest::Version;

use strict;
use warnings FATAL => qw(all);

use Cwd qw(abs_path);
use Exporter qw(import);
    our @EXPORT = qw();

# Project Name
#
# Defines the official project name.
#-----------------------------------------------------------------------------------------------------------------------------------
use constant PROJECT_NAME                                           => 'pgBackRest';
    push @EXPORT, qw(PROJECT_NAME);
use constant PROJECT_EXE                                            => lc(PROJECT_NAME);
    push @EXPORT, qw(PROJECT_EXE);
use constant PROJECT_CONF                                           => PROJECT_EXE . '.conf';
    push @EXPORT, qw(PROJECT_CONF);

# Binary location
#
# Stores the exe location.
#-----------------------------------------------------------------------------------------------------------------------------------
my $strProjectBin;

sub projectBin {return $strProjectBin};
sub projectBinSet {$strProjectBin = shift}

push @EXPORT, qw(projectBin projectBinSet);

# Project Version Number
#
# Defines the current version of the BackRest executable.  The version number is used to track features but does not affect what
# repositories or manifests can be read - that's the job of the format number.
#-----------------------------------------------------------------------------------------------------------------------------------
use constant PROJECT_VERSION                                        => '2.22';
    push @EXPORT, qw(PROJECT_VERSION);

# Repository Format Number
#
# Defines format for info and manifest files as well as on-disk structure.  If this number changes then the repository will be
# invalid unless migration functions are written.
#-----------------------------------------------------------------------------------------------------------------------------------
use constant REPOSITORY_FORMAT                                        => 5;
    push @EXPORT, qw(REPOSITORY_FORMAT);

1;
