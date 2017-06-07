####################################################################################################################################
# S3 file read.
####################################################################################################################################
package pgBackRest::Storage::S3::FileRead;
use parent 'pgBackRest::Common::Io::Base';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Digest::MD5 qw(md5_base64);
use Fcntl qw(O_RDONLY O_WRONLY O_CREAT O_TRUNC);
use File::Basename qw(dirname);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::Xml;
use pgBackRest::Storage::Base;
use pgBackRest::Storage::S3::Request;

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
our @ISA = ();                                                      ## no critic (ClassHierarchies::ProhibitExplicitISA)

sub new
{
    my $class = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oDriver,
        $strName,
        $bIgnoreMissing,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'oDriver', trace => true},
            {name => 'strName', trace => true},
            {name => 'bIgnoreMissing', optional => true, default => false, trace => true},
        );

    # Open file
    my $self = $oDriver->request(
        HTTP_VERB_GET, {strUri => $strName, strResponseType => S3_RESPONSE_TYPE_IO, bIgnoreMissing => $bIgnoreMissing});

    # Bless with new class if file exists
    if (defined($self))
    {
        @ISA = $self->isA();                                        ## no critic (ClassHierarchies::ProhibitExplicitISA)
        bless $self, $class;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self, trace => true}
    );
}

1;
