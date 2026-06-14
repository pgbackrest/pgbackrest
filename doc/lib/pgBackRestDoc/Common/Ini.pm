####################################################################################################################################
# COMMON INI MODULE
####################################################################################################################################
package pgBackRestDoc::Common::Ini;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use JSON::PP;

use pgBackRestDoc::Common::Exception;
use pgBackRestDoc::Common::Log;
use pgBackRestDoc::Common::String;

####################################################################################################################################
# iniRender() - render hash to standard INI format.
####################################################################################################################################
push @EXPORT, qw(iniRender);

sub iniRender
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oContent,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::iniRender', \@_,
            {name => 'oContent', trace => true},
        );

    # Open the ini file for writing
    my $strContent = '';
    my $bFirst = true;

    # Create the JSON object canonical so that fields are alpha ordered to pass unit tests
    my $oJSON = JSON::PP->new()->canonical()->allow_nonref();

    # Write the INI file
    foreach my $strSection (sort(keys(%$oContent)))
    {
        # Add a linefeed between sections
        if (!$bFirst)
        {
            $strContent .= "\n";
        }

        # Write the section
        $strContent .= "[${strSection}]\n";

        # Iterate through all keys in the section
        foreach my $strKey (sort(keys(%{$oContent->{$strSection}})))
        {
            # If the value is a hash then convert it to JSON, otherwise store as is
            my $strValue = ${$oContent}{$strSection}{$strKey};

            # If the value is an array then save each element to a separate key/value pair
            if (ref($strValue) eq 'ARRAY')
            {
                foreach my $strArrayValue (@{$strValue})
                {
                    $strContent .= "${strKey}=${strArrayValue}\n";
                }
            }
            # Else write a standard key/value pair
            else
            {
                $strContent .= "${strKey}=${strValue}\n";
            }
        }

        $bFirst = false;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strContent', value => $strContent, trace => true}
    );
}

1;
