####################################################################################################################################
# Cipher Miscellaneous
####################################################################################################################################
package pgBackRest::Common::Cipher;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::LibC qw(:random :encode);

####################################################################################################################################
# cipherPassGen - generate a passphrase of the specified size (in bytes)
####################################################################################################################################
sub cipherPassGen
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $iKeySizeInBytes,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::cipherPassGen', \@_,
            {name => 'iKeySizeInBytes', default => 48},
        );

    # Create and base64 encode the key
    my $strCipherPass = encodeToStr(ENCODE_TYPE_BASE64, randomBytes($iKeySizeInBytes));

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strCipherPass', value => $strCipherPass, redact => true}
    );
}

push @EXPORT, qw(cipherPassGen);

1;
