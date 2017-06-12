####################################################################################################################################
# HTTP Common
####################################################################################################################################
package pgBackRest::Common::Http::Common;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;

####################################################################################################################################
# httpQuery - encode an HTTP query from a hash
####################################################################################################################################
sub httpQuery
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $hQuery,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::httpQuery', \@_,
            {name => 'hQuery', required => false, trace => true},
        );

    # Generate the query string
    my $strQuery = '';

    # If a hash (the normal case)
    if (ref($hQuery))
    {
        foreach my $strParam (sort(keys(%{$hQuery})))
        {
            # Parameters may not be defined - this is OK
            if (defined($hQuery->{$strParam}))
            {
                $strQuery .= ($strQuery eq '' ? '' : '&') . $strParam . '=' . httpUriEncode($hQuery->{$strParam});
            }
        }
    }
    # Else query string was passed directly as a scalar
    elsif (defined($hQuery))
    {
        $strQuery = $hQuery;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strQuery', value => $strQuery, trace => true}
    );
}

push @EXPORT, qw(httpQuery);

####################################################################################################################################
# httpUriEncode - encode query values to conform with URI specs
####################################################################################################################################
sub httpUriEncode
{
    my $strString = shift;

    # Only encode if source string is defined
    my $strEncodedString;

    if (defined($strString))
    {
        # Iterate all characters in the string
        for (my $iIndex = 0; $iIndex < length($strString); $iIndex++)
        {
            my $cChar = substr($strString, $iIndex, 1);

            # These characters are reproduced verbatim
            if (($cChar ge 'A' && $cChar le 'Z') || ($cChar ge 'a' && $cChar le 'z') || ($cChar ge '0' && $cChar le '9') ||
                $cChar eq '_' || $cChar eq '-' || $cChar eq '~' || $cChar eq '.')
            {
                $strEncodedString .= $cChar;
            }
            # Forward slash is encoded
            elsif ($cChar eq '/')
            {
                $strEncodedString .= '%2F';
            }
            # All other characters are hex-encoded
            else
            {
                $strEncodedString .= sprintf('%%%02X', ord($cChar));
            }
        }
    }

    return $strEncodedString;
}

push @EXPORT, qw(httpUriEncode);

1;
