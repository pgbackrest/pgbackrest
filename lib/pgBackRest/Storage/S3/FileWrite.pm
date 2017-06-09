####################################################################################################################################
# S3 file write.
####################################################################################################################################
package pgBackRest::Storage::S3::FileWrite;
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
# Constants
####################################################################################################################################
use constant S3_BUFFER_MAX                                          => 16777216;

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oDriver,
        $strName,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'oDriver', trace => true},
            {name => 'strName', trace => true},
        );

    # Create the class hash
    my $self = $class->SUPER::new("'${strName}'");
    bless $self, $class;

    # Set variables
    $self->{oDriver} = $oDriver;
    $self->{strName} = $strName;

    # Start with an empty buffer
    $self->{rtBuffer} = '';

    # Has anything been written?
    $self->{bWritten} = false;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self, trace => true}
    );
}

####################################################################################################################################
# open - open the file
####################################################################################################################################
sub open
{
    my $self = shift;

    # Request an upload id
    my $oResponse = $self->{oDriver}->request(
        HTTP_VERB_POST, {strUri => $self->{strName}, hQuery => 'uploads=', strResponseType => S3_RESPONSE_TYPE_XML});

    $self->{strUploadId} = xmlTagText($oResponse, 'UploadId');

    # Intialize the multi-part array
    $self->{rstryMultiPart} = [];
}

####################################################################################################################################
# write - write data to a file
####################################################################################################################################
sub write
{
    my $self = shift;
    my $rtBuffer = shift;

    # Note that write has been called
    $self->{bWritten} = true;

    if (defined($rtBuffer))
    {
        $self->{rtBuffer} .= $$rtBuffer;

        # Wait until buffer is at least max before writing to avoid writing smaller files multi-part
        if (length($self->{rtBuffer}) >= S3_BUFFER_MAX)
        {
            $self->flush();
        }

        return length($$rtBuffer);
    }

    return 0;
}

####################################################################################################################################
# flush - flush whatever is in the buffer out
####################################################################################################################################
sub flush
{
    my $self = shift;

    # Open file if it is not open already
    $self->open() if !$self->opened();

    # Put a file
    $self->{oDriver}->request(
        HTTP_VERB_PUT,
        {strUri => $self->{strName},
            hQuery => {'partNumber' => @{$self->{rstryMultiPart}} + 1, 'uploadId' => $self->{strUploadId}},
            rstrBody => \$self->{rtBuffer}, hHeader => {'content-md5' => md5_base64($self->{rtBuffer}) . '=='}});

    # Store the returned etag
    push(@{$self->{rstryMultiPart}}, $self->{oDriver}->{hResponseHeader}{&S3_HEADER_ETAG});

    # Clear the buffer
    $self->{rtBuffer} = '';
}

####################################################################################################################################
# close - close the file
####################################################################################################################################
sub close
{
    my $self = shift;

    # Only close if something was written
    if ($self->{bWritten})
    {
        # Make sure close does not run again
        $self->{bWritten} = false;

        # If the file is open then multipart transfer has already started and must be completed
        if ($self->opened())
        {
            # flush out whatever is in the buffer
            $self->flush();

            my $strXml = XML_HEADER . '<CompleteMultipartUpload>';
            my $iPartNo = 0;

            foreach my $strETag (@{$self->{rstryMultiPart}})
            {
                $iPartNo++;

                $strXml .= "<Part><PartNumber>${iPartNo}</PartNumber><ETag>${strETag}</ETag></Part>";
            }

            $strXml .= '</CompleteMultipartUpload>';

            # Finalize file
            my $oResponse = $self->{oDriver}->request(
                HTTP_VERB_POST,
                {strUri => $self->{strName}, hQuery => {'uploadId' => $self->{strUploadId}},
                    rstrBody => \$strXml, hHeader => {'content-md5' => md5_base64($strXml) . '=='},
                    strResponseType => S3_RESPONSE_TYPE_XML});
        }
        # Else the file can be transmitted in one block
        else
        {
            $self->{oDriver}->request(
                HTTP_VERB_PUT,
                {strUri => $self->{strName}, rstrBody => \$self->{rtBuffer},
                    hHeader => {'content-md5' => md5_base64($self->{rtBuffer}) . '=='}});
        }
    }

    return true;
}

####################################################################################################################################
# Getters
####################################################################################################################################
sub opened {defined(shift->{strUploadId})}

1;
