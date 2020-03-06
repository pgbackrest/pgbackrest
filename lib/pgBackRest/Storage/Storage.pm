####################################################################################################################################
# C Storage Interface
####################################################################################################################################
package pgBackRest::Storage::Storage;
use parent 'pgBackRest::Storage::Base';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Digest::SHA qw(sha1_hex);
use File::Basename qw(dirname);
use Fcntl qw(:mode);
use File::stat qw{lstat};
use JSON::PP;

use pgBackRest::Common::Exception;
use pgBackRest::Common::Io::Handle;
use pgBackRest::Common::Log;
use pgBackRest::Storage::Base;
use pgBackRest::Storage::StorageRead;
use pgBackRest::Storage::StorageWrite;

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
        $self->{strType},
        $self->{strPath},
        $self->{lBufferMax},
        $self->{strDefaultPathMode},
        $self->{strDefaultFileMode},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strType'},
            {name => 'strPath', optional => true},
            {name => 'lBufferMax', optional => true, default => 65536},
            {name => 'strDefaultPathMode', optional => true, default => '0750'},
            {name => 'strDefaultFileMode', optional => true, default => '0640'},
        );

    # Create C storage object
    $self->{oStorageC} = pgBackRest::LibC::Storage->new($self->{strType}, $self->{strPath});

    # Get encryption settings
    if ($self->{strType} eq '<REPO>')
    {
        $self->{strCipherType} = $self->{oStorageC}->cipherType();
        $self->{strCipherPass} = $self->{oStorageC}->cipherPass();
    }

    # Create JSON object
    $self->{oJSON} = JSON::PP->new()->allow_nonref();

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# Check if file exists (not a path)
####################################################################################################################################
sub exists
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFileExp,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->exists', \@_,
            {name => 'strFileExp'},
        );

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bExists', value => defined($self->info($strFileExp, {bIgnoreMissing => true}))}
    );
}

####################################################################################################################################
# Read a buffer from storage all at once
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
            {name => 'strCipherPass', optional => true, default => $self->cipherPassUser(), redact => true},
        );

    # Is this an IO object or a file expression? If file expression, then open the file and pass passphrase if one is defined or
    # if the repo has a user passphrase defined - else pass undef
    my $oFileIo = defined($xFile) ? (ref($xFile) ? $xFile : $self->openRead($xFile, {strCipherPass => $strCipherPass})) : undef;

    # Get the file contents
    my $bEmpty = false;
    my $tContent = $self->{oStorageC}->get($oFileIo->{oStorageCRead});

    if (defined($tContent) && length($tContent) == 0)
    {
        $tContent = undef;
        $bEmpty = true;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'rtContent', value => defined($tContent) || $bEmpty ? \$tContent : undef, trace => true},
    );
}

####################################################################################################################################
# Get information for path/file
####################################################################################################################################
sub info
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathFileExp,
        $bIgnoreMissing,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->info', \@_,
            {name => 'strPathFileExp'},
            {name => 'bIgnoreMissing', optional => true, default => false},
        );

    my $rhInfo;
    my $strJson = $self->{oStorageC}->info($strPathFileExp, $bIgnoreMissing);

    if (defined($strJson))
    {
        $rhInfo = $self->{oJSON}->decode($strJson);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'rhInfo', value => $rhInfo, trace => true}
    );
}

####################################################################################################################################
# List all files/paths in path
####################################################################################################################################
sub list
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathExp,
        $strExpression,
        $strSortOrder,
        $bIgnoreMissing,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->list', \@_,
            {name => 'strPathExp', required => false},
            {name => 'strExpression', optional => true},
            {name => 'strSortOrder', optional => true, default => 'forward'},
            {name => 'bIgnoreMissing', optional => true, default => false},
        );

    # Get file list
    my $rstryFileList = [];
    my $strFileList = $self->{oStorageC}->list($strPathExp, $bIgnoreMissing, $strSortOrder eq 'forward', $strExpression);

    if (defined($strFileList) && $strFileList ne '[]')
    {
        $rstryFileList = $self->{oJSON}->decode($strFileList);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'stryFileList', value => $rstryFileList}
    );
}

####################################################################################################################################
# Build path/file/link manifest starting with base path and including all subpaths
####################################################################################################################################
sub manifest
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathExp,
        $strFilter,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->manifest', \@_,
            {name => 'strPathExp'},
            {name => 'strFilter', optional => true, trace => true},
        );

    my $hManifest = $self->{oJSON}->decode($self->manifestJson($strPathExp, {strFilter => $strFilter}));

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'hManifest', value => $hManifest, trace => true}
    );
}

sub manifestJson
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathExp,
        $strFilter,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->manifestJson', \@_,
            {name => 'strPathExp'},
            {name => 'strFilter', optional => true, trace => true},
        );

    my $strManifestJson = $self->{oStorageC}->manifest($strPathExp, $strFilter);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strManifestJson', value => $strManifestJson, trace => true}
    );
}

####################################################################################################################################
# Open file for reading
####################################################################################################################################
sub openRead
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $xFileExp,
        $bIgnoreMissing,
        $rhyFilter,
        $strCipherPass,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->openRead', \@_,
            {name => 'xFileExp'},
            {name => 'bIgnoreMissing', optional => true, default => false},
            {name => 'rhyFilter', optional => true},
            {name => 'strCipherPass', optional => true, default => $self->cipherPassUser(), redact => true},
        );

    # Open the file
    my $oFileIo = pgBackRest::LibC::StorageRead->new($self->{oStorageC}, $xFileExp, $bIgnoreMissing);

    # If cipher is set then decryption is the first filter applied to the read
    if (defined($self->cipherType()))
    {
        $oFileIo->filterAdd(STORAGE_FILTER_CIPHER_BLOCK, $self->{oJSON}->encode([false, $self->cipherType(), $strCipherPass]));
    }

    # Apply any other filters
    if (defined($rhyFilter))
    {
        foreach my $rhFilter (@{$rhyFilter})
        {
            $oFileIo->filterAdd(
                $rhFilter->{strClass}, defined($rhFilter->{rxyParam}) ? $self->{oJSON}->encode($rhFilter->{rxyParam}) : undef);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oFileIo', value => new pgBackRest::Storage::StorageRead($self, $oFileIo), trace => true},
    );
}

####################################################################################################################################
# Open file for writing
####################################################################################################################################
sub openWrite
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $xFileExp,
        $strMode,
        $strUser,
        $strGroup,
        $lTimestamp,
        $bAtomic,
        $bPathCreate,
        $rhyFilter,
        $strCipherPass,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->openWrite', \@_,
            {name => 'xFileExp'},
            {name => 'strMode', optional => true, default => $self->{strDefaultFileMode}},
            {name => 'strUser', optional => true},
            {name => 'strGroup', optional => true},
            {name => 'lTimestamp', optional => true, default => '0'},
            {name => 'bAtomic', optional => true, default => false},
            {name => 'bPathCreate', optional => true, default => true},
            {name => 'rhyFilter', optional => true},
            {name => 'strCipherPass', optional => true, default => $self->cipherPassUser(), redact => true},
        );

    # Open the file
    my $oFileIo = pgBackRest::LibC::StorageWrite->new(
        $self->{oStorageC}, $xFileExp, oct($strMode), $strUser, $strGroup, $lTimestamp, $bAtomic, $bPathCreate);

    # Apply any other filters
    if (defined($rhyFilter))
    {
        foreach my $rhFilter (@{$rhyFilter})
        {
            $oFileIo->filterAdd(
                $rhFilter->{strClass}, defined($rhFilter->{rxyParam}) ? $self->{oJSON}->encode($rhFilter->{rxyParam}) : undef);
        }
    }

    # If cipher is set then encryption is the last filter applied to the write
    if (defined($self->cipherType()))
    {
        $oFileIo->filterAdd(STORAGE_FILTER_CIPHER_BLOCK, $self->{oJSON}->encode([true, $self->cipherType(), $strCipherPass]));
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oFileIo', value => new pgBackRest::Storage::StorageWrite($self, $oFileIo), trace => true},
    );
}

####################################################################################################################################
# Resolve a path expression into an absolute path
####################################################################################################################################
sub pathGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathExp,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->pathGet', \@_,
            {name => 'strPathExp'},
        );

    # Check exists
    my $strPath = $self->{oStorageC}->pathGet($strPathExp);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strPath', value => $strPath, trace => true}
    );
}

####################################################################################################################################
# Remove path and all files below it
####################################################################################################################################
sub pathRemove
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathExp,
        $bIgnoreMissing,
        $bRecurse,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->pathRemove', \@_,
            {name => 'strPathExp'},
            {name => 'bIgnoreMissing', optional => true, default => true},
            {name => 'bRecurse', optional => true, default => false},
        );

    $self->{oStorageC}->pathRemove($strPathExp, $bIgnoreMissing, $bRecurse);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
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
            {name => 'strCipherPass', optional => true, default => $self->cipherPassUser(), trace => true, redact => true},
        );

    # Is this an IO object or a file expression? If file expression, then open the file and pass passphrase if one is defined or if
    # the repo has a user passphrase defined - else pass undef
    my $oFileIo = ref($xFile) ? $xFile : $self->openWrite($xFile, {strCipherPass => $strCipherPass});

    # Write the content
    my $lSize = $self->{oStorageC}->put($oFileIo->{oStorageCWrite}, ref($xContent) ? $$xContent : $xContent);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'lSize', value => $lSize, trace => true},
    );
}

####################################################################################################################################
# Remove file
####################################################################################################################################
sub remove
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
            __PACKAGE__ . '->remove', \@_,
            {name => 'xFileExp'},
            {name => 'bIgnoreMissing', optional => true, default => true},
        );

    foreach my $strFileExp (ref($xFileExp) ? @{$xFileExp} : ($xFileExp))
    {
        $self->{oStorageC}->remove($strFileExp, $bIgnoreMissing);
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# Getters
####################################################################################################################################
sub capability {shift->type() eq STORAGE_POSIX}
sub type {shift->{oStorageC}->type()}
sub cipherType {shift->{strCipherType}}
sub cipherPassUser {shift->{strCipherPass}}

1;
