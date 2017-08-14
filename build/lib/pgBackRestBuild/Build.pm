####################################################################################################################################
# Build Makefile and Auto-Generate Files Required for Build
####################################################################################################################################
package pgBackRestBuild::Build;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use Storable qw(dclone);

use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Storage::Local;
use pgBackRest::Storage::Posix::Driver;

use pgBackRestBuild::Build::Common;
use pgBackRestBuild::Config::Build;

####################################################################################################################################
# buildAll - execute all build functions and generate C source code
####################################################################################################################################
sub buildAll
{
    my $strBuildPath = shift;

    # Storage object
    my $oStorage = new pgBackRest::Storage::Local(
        $strBuildPath, new pgBackRest::Storage::Posix::Driver({bFileSync => false, bPathSync => false}));

    # Build hash that contains generated source code and constants
    my $rhBuild =
    {
        'src/config' => buildConfig(),
    };

    # Output source code
    foreach my $strPath (sort(keys(%{$rhBuild})))
    {
        foreach my $strFile (sort(keys(%{$rhBuild->{$strPath}})))
        {
            foreach my $strFileType (sort(keys(%{$rhBuild->{$strPath}{$strFile}})))
            {
                my $rhFile = $rhBuild->{$strPath}{$strFile}{$strFileType};

                $oStorage->put("${strPath}/${strFile}.auto.${strFileType}", trim($rhFile->{&BLD_SOURCE}) . "\n");
            }
        }
    }

    # Return build hash to caller for further processing
    return $rhBuild;
}

push @EXPORT, qw(buildAll);

1;
