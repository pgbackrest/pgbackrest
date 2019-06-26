####################################################################################################################################
# Base Storage Module
####################################################################################################################################
package pgBackRest::Storage::Base;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Io::Base;
use pgBackRest::Common::Log;

####################################################################################################################################
# Storage constants
####################################################################################################################################
use constant STORAGE_LOCAL                                          => '<LOCAL>';
    push @EXPORT, qw(STORAGE_LOCAL);

use constant STORAGE_S3                                             => 's3';
    push @EXPORT, qw(STORAGE_S3);
use constant STORAGE_POSIX                                          => 'posix';
    push @EXPORT, qw(STORAGE_POSIX);

####################################################################################################################################
# Compress constants
####################################################################################################################################
use constant STORAGE_COMPRESS                                       => 'compress';
    push @EXPORT, qw(STORAGE_COMPRESS);
use constant STORAGE_DECOMPRESS                                     => 'decompress';
    push @EXPORT, qw(STORAGE_DECOMPRESS);

####################################################################################################################################
# Cipher constants
####################################################################################################################################
use constant STORAGE_ENCRYPT                                        => 'encrypt';
    push @EXPORT, qw(STORAGE_ENCRYPT);
use constant STORAGE_DECRYPT                                        => 'decrypt';
    push @EXPORT, qw(STORAGE_DECRYPT);
use constant CIPHER_MAGIC                                           => 'Salted__';
    push @EXPORT, qw(CIPHER_MAGIC);

####################################################################################################################################
# Filter constants
####################################################################################################################################
use constant STORAGE_FILTER_CIPHER_BLOCK                            => 'pgBackRest::Storage::Filter::CipherBlock';
    push @EXPORT, qw(STORAGE_FILTER_CIPHER_BLOCK);
use constant STORAGE_FILTER_GZIP                                    => 'pgBackRest::Storage::Filter::Gzip';
    push @EXPORT, qw(STORAGE_FILTER_GZIP);
use constant STORAGE_FILTER_SHA                                     => 'pgBackRest::Storage::Filter::Sha';
    push @EXPORT, qw(STORAGE_FILTER_SHA);

####################################################################################################################################
# Capability constants
####################################################################################################################################
# Can the size in the storage be different than what was written?  For example, a ZFS filesystem could be doing compression of a
# backup where compression was not enabled.  This affects how repo-size is calculated.  If the file system only stores what was
# written or won't report differently then we can save some time by just setting repo-size to size.
use constant STORAGE_CAPABILITY_SIZE_DIFF                           => 'size-diff';
    push @EXPORT, qw(STORAGE_CAPABILITY_SIZE_DIFF);

use constant STORAGE_CAPABILITY_LINK                                => 'link';
    push @EXPORT, qw(STORAGE_CAPABILITY_LINK);
use constant STORAGE_CAPABILITY_PATH_SYNC                           => 'path-sync';
    push @EXPORT, qw(STORAGE_CAPABILITY_PATH_SYNC);

####################################################################################################################################
# new
####################################################################################################################################
sub new
{
    my $class = shift;

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Assign function parameters, defaults, and log debug info
    (
        my $strOperation,
        $self->{lBufferMax},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'lBufferMax', optional => true, default => COMMON_IO_BUFFER_MAX, trace => true},
        );

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}


####################################################################################################################################
# Copy a file. If special encryption settings are required, then the file objects from openRead/openWrite must be passed instead of
# file names.
####################################################################################################################################
sub copy
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $xSourceFile,
        $xDestinationFile,
        $bSourceOpen,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->copy', \@_,
            {name => 'xSourceFile', required => false},
            {name => 'xDestinationFile'},
            {name => 'bSourceOpen', optional => true, default => false},
        );

    # Is source/destination an IO object or a file expression?
    my $oSourceFileIo = defined($xSourceFile) ? (ref($xSourceFile) ? $xSourceFile : $self->openRead($xSourceFile)) : undef;

    # Does the source file exist?
    my $bResult = false;

    # Copy if the source file exists
    if (defined($oSourceFileIo))
    {
        my $oDestinationFileIo = ref($xDestinationFile) ? $xDestinationFile : $self->openWrite($xDestinationFile);

        # Use C copy if source and destination are C objects
        if (defined($oSourceFileIo->{oStorageCRead}) && defined($oDestinationFileIo->{oStorageCWrite}))
        {
            $bResult = $self->{oStorageC}->copy(
                $oSourceFileIo->{oStorageCRead}, $oDestinationFileIo->{oStorageCWrite}) ? true : false;
        }
        else
        {
            # Open the source file if it is a C object
            $bResult = defined($oSourceFileIo->{oStorageCRead}) ? ($bSourceOpen || $oSourceFileIo->open()) : true;

            if ($bResult)
            {
                # Open the destination file if it is a C object
                if (defined($oDestinationFileIo->{oStorageCWrite}))
                {
                    $oDestinationFileIo->open();
                }

                # Copy the data
                do
                {
                    # Read data
                    my $tBuffer = '';

                    $oSourceFileIo->read(\$tBuffer, $self->{lBufferMax});
                    $oDestinationFileIo->write(\$tBuffer);
                }
                while (!$oSourceFileIo->eof());

                # Close files
                $oSourceFileIo->close();
                $oDestinationFileIo->close();
            }
        }
    }

    return logDebugReturn
    (
        $strOperation,
        {name => 'bResult', value => $bResult, trace => true},
    );
}

####################################################################################################################################
# get - reads a buffer from storage all at once
####################################################################################################################################
sub get
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $xFile,
        $strCipherPass,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->get', \@_,
            {name => 'xFile', required => false, trace => true},
            {name => 'strCipherPass', optional => true, redact => true},
        );

    # Is this an IO object or a file expression? If file expression, then open the file and pass passphrase if one is defined or
    # if the repo has a user passphrase defined - else pass undef
    my $oFileIo = defined($xFile) ? (ref($xFile) ? $xFile : $self->openRead(
        $xFile, {strCipherPass => defined($strCipherPass) ? $strCipherPass : $self->cipherPassUser()})) : undef;

    # Read only if there is something to read from
    my $tContent;
    my $lSize = 0;

    if (defined($oFileIo))
    {
        my $lSizeRead;

        do
        {
            $lSizeRead = $oFileIo->read(\$tContent, $self->{lBufferMax});
            $lSize += $lSizeRead;
        }
        while ($lSizeRead != 0);

        # Close the file
        $oFileIo->close();

        # If nothing was read then set to undef
        if ($lSize == 0)
        {
            $tContent = undef;
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'rtContent', value => defined($oFileIo) ? \$tContent : undef, trace => true},
    );
}

####################################################################################################################################
# pathAbsolute - generate an absolute path from an absolute base path and a relative path
####################################################################################################################################
sub pathAbsolute
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strBasePath,
        $strPath
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::pathAbsolute', \@_,
            {name => 'strBasePath', trace => true},
            {name => 'strPath', trace => true}
        );

    # Working variables
    my $strAbsolutePath;

    # If the path is already absolute
    if (index($strPath, '/') == 0)
    {
        $strAbsolutePath = $strPath;
    }
    # Else make it absolute using the base path
    else
    {
        # Make sure the absolute path is really absolute
        if (index($strBasePath, '/') != 0 || index($strBasePath, '/..') != -1)
        {
            confess &log(ERROR, "${strBasePath} is not an absolute path", ERROR_PATH_TYPE);
        }

        while (index($strPath, '..') == 0)
        {
            $strBasePath = dirname($strBasePath);
            $strPath = substr($strPath, 2);

            if (index($strPath, '/') == 0)
            {
                $strPath = substr($strPath, 1);
            }
        }

        $strAbsolutePath = "${strBasePath}/${strPath}";
    }

    # Make sure the result is really an absolute path
    if (index($strAbsolutePath, '/') != 0 || index($strAbsolutePath, '/..') != -1)
    {
        confess &log(ERROR, "result ${strAbsolutePath} was not an absolute path", ERROR_PATH_TYPE);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strAbsolutePath', value => $strAbsolutePath, trace => true}
    );
}

####################################################################################################################################
# put - writes a buffer out to storage all at once
####################################################################################################################################
sub put
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $xFile,
        $xContent,
        $strCipherPass,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->put', \@_,
            {name => 'xFile', trace => true},
            {name => 'xContent', required => false, trace => true},
            {name => 'strCipherPass', optional => true, trace => true, redact => true},
        );

    # Is this an IO object or a file expression? If file expression, then open the file and pass passphrase if one is defined or if
    # the repo has a user passphrase defined - else pass undef
    my $oFileIo = ref($xFile) ? $xFile : $self->openWrite(
        $xFile, {strCipherPass => defined($strCipherPass) ? $strCipherPass : $self->cipherPassUser()});

    # Determine size of content
    my $lSize = defined($xContent) ? length(ref($xContent) ? $$xContent : $xContent) : 0;

    # Write only if there is something to write
    if ($lSize > 0)
    {
        $oFileIo->write(ref($xContent) ? $xContent : \$xContent);
    }
    # Else open the file so a zero length file is created (since file is not opened until first write)
    else
    {
        $oFileIo->open();
    }

    # Close the file
    $oFileIo->close();

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'lSize', value => $lSize, trace => true},
    );
}

1;
