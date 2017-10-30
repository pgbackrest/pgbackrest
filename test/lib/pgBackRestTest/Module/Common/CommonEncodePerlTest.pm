####################################################################################################################################
# Test Binary to String Encode/Decode
####################################################################################################################################
package pgBackRestTest::Module::Common::CommonEncodePerlTest;
use parent 'pgBackRestTest::Common::RunTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::LibC qw(:encode);
use pgBackRest::Version;

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    ################################################################################################################################
    if ($self->begin("encodeToStrBase64() and decodeToBinBase64()"))
    {
        my $strData = 'string_to_encode';
        my $strEncodedData = 'c3RyaW5nX3RvX2VuY29kZQ==';

        $self->testResult(sub {encodeToStr(ENCODE_TYPE_BASE64, $strData)}, $strEncodedData, 'encode string');
        $self->testResult(sub {decodeToBin(ENCODE_TYPE_BASE64, $strEncodedData)}, $strData, 'decode string');

        #---------------------------------------------------------------------------------------------------------------------------
        my $tData =
            pack('C', 1) . pack('C', 2) . pack('C', 3) . pack('C', 4) . pack('C', 5) . pack('C', 4) . pack('C', 3) . pack('C', 2);
        my $tEncodedData = 'AQIDBAUEAwI=';

        $self->testResult(sub {encodeToStr(ENCODE_TYPE_BASE64, $tData)}, $tEncodedData, 'encode binary');
        $self->testResult(sub {decodeToBin(ENCODE_TYPE_BASE64, $tEncodedData)}, $tData, 'decode binary');

        #---------------------------------------------------------------------------------------------------------------------------
        $tData .= pack('C', 1);
        $tEncodedData = 'AQIDBAUEAwIB';

        $self->testResult(sub {encodeToStr(ENCODE_TYPE_BASE64, $tData)}, $tEncodedData, 'encode binary');
        $self->testResult(sub {decodeToBin(ENCODE_TYPE_BASE64, $tEncodedData)}, $tData, 'decode binary');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(sub {encodeToStr(-1, '')}, ERROR_ASSERT, 'invalid encode type -1');
        $self->testException(
            sub {decodeToBin(ENCODE_TYPE_BASE64, "XX")}, ERROR_FORMAT, 'base64 size 2 is not evenly divisible by 4');
    }
}

1;
