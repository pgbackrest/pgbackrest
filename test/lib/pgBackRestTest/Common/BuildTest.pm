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
# Save contents to a file if the file is missing or the contents are different. This saves write IO and prevents the timestamp from
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

