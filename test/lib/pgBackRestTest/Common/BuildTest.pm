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

use pgBackRestDoc::Common::Log;
use pgBackRestDoc::Common::String;

####################################################################################################################################
# VM hash keywords
####################################################################################################################################
use constant BUILD_AUTO_H                                           => 'build.auto.h';
    push @EXPORT, qw(BUILD_AUTO_H);

####################################################################################################################################
# Save contents to a file if the file is missing or the contents are different.  This saves write IO and prevents the timestamp from
# changing.
####################################################################################################################################
sub buildPutDiffers
{
    my $oStorage = shift;
    my $strFile = shift;
    my $strContentNew = shift;

    # Attempt to load the file
    my $bSave = true;
    my $oFile = $oStorage->openRead($strFile, {bIgnoreMissing => true});

    # If file was found see if the content is the same
    if (defined($oFile))
    {
        my $strContentFile = ${$oStorage->get($oFile)};

        if ((defined($strContentFile) ? $strContentFile : '') eq (defined($strContentNew) ? $strContentNew : ''))
        {
            $bSave = false;
        }
    }

    # Save if the contents are different or missing
    if ($bSave)
    {
        $oStorage->put($oStorage->openWrite($strFile, {bPathCreate => true}), $strContentNew);
    }

    # Was the file saved?
    return $bSave;
}

push @EXPORT, qw(buildPutDiffers);

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
