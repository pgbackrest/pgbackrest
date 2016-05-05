####################################################################################################################################
# VERSION MODULE
#
# Contains BackRest version and format numbers.
####################################################################################################################################
package pgBackRest::Version;

use strict;
use warnings FATAL => qw(all);

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

# BackRest Version Number
#
# Defines the current version of the BackRest executable.  The version number is used to track features but does not affect what
# repositories or manifests can be read - that's the job of the format number.
#-----------------------------------------------------------------------------------------------------------------------------------
our # 'our' keyword is on a separate line to make the ExtUtils::MakeMaker parser happy.
$VERSION = '1.01dev';

push @EXPORT, qw($VERSION);

# Format Format Number
#
# Defines format for info and manifest files as well as on-disk structure.  If this number changes then the repository will be
# invalid unless migration functions are written.
#-----------------------------------------------------------------------------------------------------------------------------------
our $FORMAT = 5;

push @EXPORT, qw($FORMAT);

1;
