####################################################################################################################################
# Base Storage Module
####################################################################################################################################
package pgBackRest::Storage::Base;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Digest::SHA qw(sha1_hex);
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

use constant STORAGE_OBJECT                                         => 'object';
    push @EXPORT, qw(STORAGE_OBJECT);
use constant STORAGE_POSIX                                          => 'posix';
    push @EXPORT, qw(STORAGE_POSIX);

####################################################################################################################################
# Filter constants
####################################################################################################################################
use constant STORAGE_FILTER_CIPHER_BLOCK                            => 'pgBackRest::Storage::Filter::CipherBlock';
    push @EXPORT, qw(STORAGE_FILTER_CIPHER_BLOCK);

####################################################################################################################################
# Capability constants
####################################################################################################################################
# Does the storage support symlinks and hardlinks?
use constant STORAGE_CAPABILITY_LINK                                => 'link';
    push @EXPORT, qw(STORAGE_CAPABILITY_LINK);

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
# Calculate sha1 hash and size of file. If special encryption settings are required, then the file objects from openRead/openWrite
# must be passed instead of file names.
####################################################################################################################################
sub hashSize
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $xFileExp,
        $bIgnoreMissing,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->hashSize', \@_,
            {name => 'xFileExp'},
            {name => 'bIgnoreMissing', optional => true, default => false},
        );

    # Set operation variables
    my $strHash;
    my $lSize;

    # Is this an IO object or a file expression?
    my $rtContent = $self->get($xFileExp, {bIgnoreMissing => $bIgnoreMissing});

    if (defined($rtContent))
    {
        if (defined($$rtContent))
        {
            $strHash = sha1_hex($$rtContent);
            $lSize = length($$rtContent);
        }
        else
        {
            $strHash = sha1_hex('');
            $lSize = 0;
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strHash', value => $strHash},
        {name => 'lSize', value => $lSize}
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

1;
