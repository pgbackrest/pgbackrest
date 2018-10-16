####################################################################################################################################
# VERSION MODULE
#
# Contains BackRest version and format numbers.
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
use constant BACKREST_NAME                                          => 'pgBackRest';
    push @EXPORT, qw(BACKREST_NAME);
use constant BACKREST_EXE                                           => lc(BACKREST_NAME);
    push @EXPORT, qw(BACKREST_EXE);
use constant BACKREST_CONF                                          => BACKREST_EXE . '.conf';
    push @EXPORT, qw(BACKREST_CONF);

# Binary location
#
# Stores the exe location.
#-----------------------------------------------------------------------------------------------------------------------------------
my $strBackRestBin;

sub backrestBin {return $strBackRestBin};
sub backrestBinSet {$strBackRestBin = shift}

push @EXPORT, qw(backrestBin backrestBinSet);

# BackRest Version Number
#
# Defines the current version of the BackRest executable.  The version number is used to track features but does not affect what
# repositories or manifests can be read - that's the job of the format number.
#-----------------------------------------------------------------------------------------------------------------------------------
use constant BACKREST_VERSION                                       => '2.06';
    push @EXPORT, qw(BACKREST_VERSION);

# Format Format Number
#
# Defines format for info and manifest files as well as on-disk structure.  If this number changes then the repository will be
# invalid unless migration functions are written.
#-----------------------------------------------------------------------------------------------------------------------------------
use constant BACKREST_FORMAT                                        => 5;
    push @EXPORT, qw(BACKREST_FORMAT);

1;
