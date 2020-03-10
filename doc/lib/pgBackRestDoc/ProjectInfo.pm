####################################################################################################################################
# PROJECT INFO MODULE
#
# Contains project name, version and format.
####################################################################################################################################
package pgBackRestDoc::ProjectInfo;

use strict;
use warnings FATAL => qw(all);

use Cwd qw(abs_path);
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);

# Project Name
#
# Defines the official project name, exe, and config file.
#-----------------------------------------------------------------------------------------------------------------------------------
push @EXPORT, qw(PROJECT_NAME);
push @EXPORT, qw(PROJECT_EXE);
push @EXPORT, qw(PROJECT_CONF);

# Project Version Number
#
# Defines the current version of the BackRest executable.  The version number is used to track features but does not affect what
# repositories or manifests can be read - that's the job of the format number.
#-----------------------------------------------------------------------------------------------------------------------------------
push @EXPORT, qw(PROJECT_VERSION);

# Repository Format Number
#
# Defines format for info and manifest files as well as on-disk structure.  If this number changes then the repository will be
# invalid unless migration functions are written.
#-----------------------------------------------------------------------------------------------------------------------------------
push @EXPORT, qw(REPOSITORY_FORMAT);

####################################################################################################################################
# Load project info from src/version.h
####################################################################################################################################
require pgBackRestTest::Common::Storage;
require pgBackRestTest::Common::StoragePosix;

my $strProjectInfo = ${new pgBackRestTest::Common::Storage(
    dirname(dirname(abs_path($0))), new pgBackRestTest::Common::StoragePosix())->get('src/version.h')};

foreach my $strLine (split("\n", $strProjectInfo))
{

    if ($strLine =~ /^#define PROJECT_NAME/)
    {
        eval("use constant PROJECT_NAME => " . (split(" ", $strLine))[-1]);
    }
    elsif ($strLine =~ /^#define PROJECT_BIN/)
    {
        eval("use constant PROJECT_EXE => " . (split(" ", $strLine))[-1]);
        eval("use constant PROJECT_CONF => " . (split(" ", $strLine))[-1] . " . \'.conf\'");
    }
    elsif ($strLine =~ /^#define PROJECT_VERSION/)
    {
        eval("use constant PROJECT_VERSION => " . (split(" ", $strLine))[-1]);
    }
    elsif ($strLine =~ /^#define REPOSITORY_FORMAT/)
    {
        eval("use constant REPOSITORY_FORMAT => " . (split(" ", $strLine))[-1]);
    }
}

1;
