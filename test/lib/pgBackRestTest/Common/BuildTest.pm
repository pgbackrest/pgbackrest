####################################################################################################################################
# Build Binaries and Auto-Generate Code
####################################################################################################################################
package pgBackRestTest::Common::BuildTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

# use Cwd qw(abs_path);
use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRest::Common::Log;

####################################################################################################################################
# Find last modification time in a list of directories, with optional filters
####################################################################################################################################
sub buildLastModTime
{
    my $oStorage = shift;
    my $strBasePath = shift;
    my $rstrySubPath = shift;
    my $strPattern = shift;

    my $lTimestampLast = 0;

    foreach my $strSubPath (defined($rstrySubPath) ? @{$rstrySubPath} : (''))
    {
        my $hManifest = $oStorage->manifest($strBasePath . ($strSubPath eq '' ? '' : "/${strSubPath}"));

        foreach my $strFile (sort(keys(%{$hManifest})))
        {
            next if (defined($strPattern) && $strFile !~ /$strPattern/);

            if ($hManifest->{$strFile}{type} eq 'f' && $hManifest->{$strFile}{modification_time} > $lTimestampLast)
            {
                $lTimestampLast = $hManifest->{$strFile}{modification_time};
            }
        }
    }

    if ($lTimestampLast == 0)
    {
        confess &log(ERROR, "no files found");
    }

    return $lTimestampLast;
}

push @EXPORT, qw(buildLastModTime);

1;
