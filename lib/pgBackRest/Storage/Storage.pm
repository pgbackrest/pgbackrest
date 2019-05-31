####################################################################################################################################
# C Storage Interface
####################################################################################################################################
package pgBackRest::Storage::Storage;
use parent 'pgBackRest::Storage::Base';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use File::Basename qw(dirname);
use JSON::PP;

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;

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
        $self->{lBufferMax},
        $self->{strDefaultPathMode},
        $self->{strDefaultFileMode},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strType'},
            {name => 'lBufferMax', optional => true, default => 65536},
            {name => 'strDefaultPathMode', optional => true, default => '0750'},
            {name => 'strDefaultFileMode', optional => true, default => '0640'},
        );


    # Create C storage object
    $self->{oStorageC} = new pgBackRest::LibC::Storage($self->{strType});

    # Create JSON object
    $self->{oJSON} = JSON::PP->new()->allow_nonref();

    # $self->{strCipherType} = $strCipherType;
    # $self->{strCipherPassUser} = $strCipherPassUser;

    # if (defined($self->{strCipherType}))
    # {
    #     require pgBackRest::Storage::Filter::CipherBlock;
    #     pgBackRest::Storage::Filter::CipherBlock->import();
    # }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# exists - check if file exists
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

    # Check exists
    my $bExists = $self->{oStorageC}->exists($strFileExp);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bExists', value => $bExists}
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

    # Get the file contents
    my $tContent = $self->{oStorageC}->get($oFileIo);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'rtContent', value => defined($oFileIo) ? \$tContent : undef, trace => true},
    );
}

####################################################################################################################################
# hashSize - calculate sha1 hash and size of file. If special encryption settings are required, then the file objects from
# openRead/openWrite must be passed instead of file names.
####################################################################################################################################
# sub hashSize
# {
#     my $self = shift;
#
#     # Assign function parameters, defaults, and log debug info
#     my
#     (
#         $strOperation,
#         $xFileExp,
#         $bIgnoreMissing,
#     ) =
#         logDebugParam
#         (
#             __PACKAGE__ . '->hashSize', \@_,
#             {name => 'xFileExp'},
#             {name => 'bIgnoreMissing', optional => true, default => false},
#         );
#
#     # Set operation variables
#     my $strHash;
#     my $lSize;
#
#     # Is this an IO object or a file expression?
#     my $oFileIo =
#         defined($xFileExp) ? (ref($xFileExp) ? $xFileExp :
#             $self->openRead($self->pathGet($xFileExp), {bIgnoreMissing => $bIgnoreMissing})) : undef;
#
#     if (defined($oFileIo))
#     {
#         $lSize = 0;
#         my $oShaIo = new pgBackRest::Storage::Filter::Sha($oFileIo);
#         my $lSizeRead;
#
#         do
#         {
#             my $tContent;
#             $lSizeRead = $oShaIo->read(\$tContent, $self->{lBufferMax});
#             $lSize += $lSizeRead;
#         }
#         while ($lSizeRead != 0);
#
#         # Close the file
#         $oShaIo->close();
#
#         # Get the hash
#         $strHash = $oShaIo->result(STORAGE_FILTER_SHA);
#     }
#
#     # Return from function and log return values if any
#     return logDebugReturn
#     (
#         $strOperation,
#         {name => 'strHash', value => $strHash},
#         {name => 'lSize', value => $lSize}
#     );
# }

####################################################################################################################################
# info - get information for path/file
####################################################################################################################################
# sub info
# {
#     my $self = shift;
#
#     # Assign function parameters, defaults, and log debug info
#     my
#     (
#         $strOperation,
#         $strPathFileExp,
#         $bIgnoreMissing,
#     ) =
#         logDebugParam
#         (
#             __PACKAGE__ . '::fileStat', \@_,
#             {name => 'strPathFileExp'},
#             {name => 'bIgnoreMissing', optional => true, default => false},
#         );
#
#     # Stat the path/file
#     my $oInfo = $self->driver()->info($self->pathGet($strPathFileExp), {bIgnoreMissing => $bIgnoreMissing});
#
#     # Return from function and log return values if any
#     return logDebugReturn
#     (
#         $strOperation,
#         {name => 'oInfo', value => $oInfo, trace => true}
#     );
# }

####################################################################################################################################
# linkCreate - create a link
####################################################################################################################################
# sub linkCreate
# {
#     my $self = shift;
#
#     # Assign function parameters, defaults, and log debug info
#     my
#     (
#         $strOperation,
#         $strSourcePathFileExp,
#         $strDestinationLinkExp,
#         $bHard,
#         $bRelative,
#         $bPathCreate,
#         $bIgnoreExists,
#     ) =
#         logDebugParam
#         (
#             __PACKAGE__ . '->linkCreate', \@_,
#             {name => 'strSourcePathFileExp'},
#             {name => 'strDestinationLinkExp'},
#             {name => 'bHard', optional=> true, default => false},
#             {name => 'bRelative', optional=> true, default => false},
#             {name => 'bPathCreate', optional=> true, default => true},
#             {name => 'bIgnoreExists', optional => true, default => false},
#         );
#
#     # Get source and destination paths
#     my $strSourcePathFile = $self->pathGet($strSourcePathFileExp);
#     my $strDestinationLink = $self->pathGet($strDestinationLinkExp);
#
#     # Generate relative path if requested
#     if ($bRelative)
#     {
#         # Determine how much of the paths are common
#         my @strySource = split('/', $strSourcePathFile);
#         my @stryDestination = split('/', $strDestinationLink);
#
#         while (defined($strySource[0]) && defined($stryDestination[0]) && $strySource[0] eq $stryDestination[0])
#         {
#             shift(@strySource);
#             shift(@stryDestination);
#         }
#
#         # Add relative path sections
#         $strSourcePathFile = '';
#
#         for (my $iIndex = 0; $iIndex < @stryDestination - 1; $iIndex++)
#         {
#             $strSourcePathFile .= '../';
#         }
#
#         # Add path to source
#         $strSourcePathFile .= join('/', @strySource);
#
#         logDebugMisc
#         (
#             $strOperation, 'apply relative path',
#             {name => 'strSourcePathFile', value => $strSourcePathFile, trace => true}
#         );
#     }
#
#     # Create the link
#     $self->driver()->linkCreate(
#         $strSourcePathFile, $strDestinationLink, {bHard => $bHard, bPathCreate => $bPathCreate, bIgnoreExists => $bIgnoreExists});
#
#     # Return from function and log return values if any
#     return logDebugReturn($strOperation);
# }

####################################################################################################################################
# list - list all files/paths in path
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
    my @stryFileList;
    my $strFileList = $self->{oStorageC}->list(
        $strPathExp, $bIgnoreMissing, $strSortOrder eq 'forward', defined($strExpression) ? $strExpression : '');

    if (defined($strFileList))
    {
        @stryFileList = $self->{oJSON}->decode($strFileList);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'stryFileList', value => \@stryFileList}
    );
}

####################################################################################################################################
# manifest - build path/file/link manifest starting with base path and including all subpaths
####################################################################################################################################
# sub manifest
# {
#     my $self = shift;
#
#     # Assign function parameters, defaults, and log debug info
#     my
#     (
#         $strOperation,
#         $strPathExp,
#         $strFilter,
#     ) =
#         logDebugParam
#         (
#             __PACKAGE__ . '->manifest', \@_,
#             {name => 'strPathExp'},
#             {name => 'strFilter', optional => true, trace => true},
#         );
#
#     my $hManifest = $self->driver()->manifest($self->pathGet($strPathExp), {strFilter => $strFilter});
#
#     # Return from function and log return values if any
#     return logDebugReturn
#     (
#         $strOperation,
#         {name => 'hManifest', value => $hManifest, trace => true}
#     );
# }

####################################################################################################################################
# move - move path/file
####################################################################################################################################
# sub move
# {
#     my $self = shift;
#
#     # Assign function parameters, defaults, and log debug info
#     my
#     (
#         $strOperation,
#         $strSourcePathFileExp,
#         $strDestinationPathFileExp,
#         $bPathCreate,
#     ) =
#         logDebugParam
#         (
#             __PACKAGE__ . '->move', \@_,
#             {name => 'strSourcePathExp'},
#             {name => 'strDestinationPathExp'},
#             {name => 'bPathCreate', optional => true, default => false, trace => true},
#         );
#
#     # Set operation variables
#     $self->driver()->move(
#         $self->pathGet($strSourcePathFileExp), $self->pathGet($strDestinationPathFileExp), {bPathCreate => $bPathCreate});
#
#     # Return from function and log return values if any
#     return logDebugReturn
#     (
#         $strOperation
#     );
# }

####################################################################################################################################
# openRead - open file for reading
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
            {name => 'strCipherPass', optional => true, redact => true},
        );

    # Open the file
    my $oFileIo = new pgBackRest::LibC::StorageRead($self->{oStorageC}, $xFileExp, $bIgnoreMissing);

    # Apply filters if file is defined
    # if (defined($oFileIo))
    # {
    #     # If cipher is set then add the filter so that decryption is the first filter applied to the data read before any of the
    #     # other filters
    #     if (defined($self->cipherType()))
    #     {
    #         $oFileIo = &STORAGE_FILTER_CIPHER_BLOCK->new(
    #             $oFileIo, $self->cipherType(), defined($strCipherPass) ? $strCipherPass : $self->cipherPassUser(),
    #             {strMode => STORAGE_DECRYPT});
    #     }
    #
    #     # Apply any other filters
    #     if (defined($rhyFilter))
    #     {
    #         foreach my $rhFilter (@{$rhyFilter})
    #         {
    #             $oFileIo = $rhFilter->{strClass}->new($oFileIo, @{$rhFilter->{rxyParam}});
    #         }
    #     }
    # }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oFileIo', value => $oFileIo, trace => true},
    );
}

####################################################################################################################################
# openWrite - open file for writing
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
            {name => 'strUser', optional => true, default => ''},
            {name => 'strGroup', optional => true, default => ''},
            {name => 'lTimestamp', optional => true, default => '0'},
            {name => 'bAtomic', optional => true, default => false},
            {name => 'bPathCreate', optional => true, default => false},
            {name => 'rhyFilter', optional => true},
            {name => 'strCipherPass', optional => true, redact => true},
        );

    # Open the file
    my $oFileIo = new pgBackRest::LibC::StorageWrite(
        $self->{oStorageC}, $xFileExp, oct($strMode), $strUser, $strGroup, $lTimestamp, $bAtomic, $bPathCreate);

    # # If cipher is set then add filter so that encryption is performed just before the data is actually written
    # if (defined($self->cipherType()))
    # {
    #     $oFileIo = &STORAGE_FILTER_CIPHER_BLOCK->new(
    #         $oFileIo, $self->cipherType(), defined($strCipherPass) ? $strCipherPass : $self->cipherPassUser());
    # }
    #
    # # Apply any other filters
    # if (defined($rhyFilter))
    # {
    #     foreach my $rhFilter (reverse(@{$rhyFilter}))
    #     {
    #         $oFileIo = $rhFilter->{strClass}->new($oFileIo, @{$rhFilter->{rxyParam}});
    #     }
    # }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oFileIo', value => $oFileIo, trace => true},
    );
}

####################################################################################################################################
# owner - change ownership of path/file
####################################################################################################################################
# sub owner
# {
#     my $self = shift;
#
#     # Assign function parameters, defaults, and log debug info
#     my
#     (
#         $strOperation,
#         $strPathFileExp,
#         $strUser,
#         $strGroup
#     ) =
#         logDebugParam
#         (
#             __PACKAGE__ . '->owner', \@_,
#             {name => 'strPathFileExp'},
#             {name => 'strUser', required => false},
#             {name => 'strGroup', required => false}
#         );
#
#     # Set ownership
#     $self->driver()->owner($self->pathGet($strPathFileExp), {strUser => $strUser, strGroup => $strGroup});
#
#     # Return from function and log return values if any
#     return logDebugReturn
#     (
#         $strOperation
#     );
# }


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
# pathCreate - create path
####################################################################################################################################
sub pathCreate
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathExp,
        $strMode,
        $bIgnoreExists,
        $bCreateParent,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->pathCreate', \@_,
            {name => 'strPathExp'},
            {name => 'strMode', optional => true},
            {name => 'bIgnoreExists', optional => true, default => false},
            {name => 'bCreateParent', optional => true, default => false},
        );

    # Create path
    $self->{oStorageC}->pathCreate($strPathExp, defined($strMode) ? $strMode : '', $bIgnoreExists, $bCreateParent);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

####################################################################################################################################
# pathExists - check if path exists
####################################################################################################################################
sub pathExists
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
            __PACKAGE__ . '->pathExists', \@_,
            {name => 'strPathExp'},
        );

    # Check exists
    my $bExists = $self->{oStorageC}->pathExists($strPathExp);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bExists', value => $bExists}
    );
}

####################################################################################################################################
# pathGet - resolve a path expression into an absolute path
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
# Sync path so newly added file entries are not lost
####################################################################################################################################
# sub pathSync
# {
#     my $self = shift;
#
#     # Assign function parameters, defaults, and log debug info
#     my
#     (
#         $strOperation,
#         $strPathExp,
#     ) =
#         logDebugParam
#         (
#             __PACKAGE__ . '->pathSync', \@_,
#             {name => 'strPathExp'},
#         );
#
#     $self->driver()->pathSync($self->pathGet($strPathExp));
#
#     # Return from function and log return values if any
#     return logDebugReturn($strOperation);
# }

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

    # Write the content
    my $lSize = $self->{oStorageC}->put($oFileIo, ref($xContent) ? $$xContent : $xContent);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'lSize', value => $lSize, trace => true},
    );
}

####################################################################################################################################
# remove - remove path/file
####################################################################################################################################
# sub remove
# {
#     my $self = shift;
#
#     # Assign function parameters, defaults, and log debug info
#     my
#     (
#         $strOperation,
#         $xstryPathFileExp,
#         $bIgnoreMissing,
#         $bRecurse,
#     ) =
#         logDebugParam
#         (
#             __PACKAGE__ . '->remove', \@_,
#             {name => 'xstryPathFileExp'},
#             {name => 'bIgnoreMissing', optional => true, default => true},
#             {name => 'bRecurse', optional => true, default => false, trace => true},
#         );
#
#     # Evaluate expressions for all files
#     my @stryPathFileExp;
#
#     if (ref($xstryPathFileExp))
#     {
#         foreach my $strPathFileExp (@{$xstryPathFileExp})
#         {
#             push(@stryPathFileExp, $self->pathGet($strPathFileExp));
#         }
#     }
#
#     # Remove path(s)/file(s)
#     my $bRemoved = $self->driver()->remove(
#         ref($xstryPathFileExp) ? \@stryPathFileExp : $self->pathGet($xstryPathFileExp),
#         {bIgnoreMissing => $bIgnoreMissing, bRecurse => $bRecurse});
#
#     # Return from function and log return values if any
#     return logDebugReturn
#     (
#         $strOperation,
#         {name => 'bRemoved', value => $bRemoved}
#     );
# }

####################################################################################################################################
# encrypted - determine if the file is encrypted or not
####################################################################################################################################
sub encrypted
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFileName,
        $bIgnoreMissing,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->encrypted', \@_,
            {name => 'strFileName'},
            {name => 'bIgnoreMissing', optional => true, default => false},
        );

    # my $tMagicSignature;
    my $bEncrypted = false;

    # # Open the file via the driver
    # my $oFile = $self->driver()->openRead($self->pathGet($strFileName), {bIgnoreMissing => $bIgnoreMissing});
    #
    # # If the file does not exist because we're ignoring missing (else it would error before this is executed) then determine if it
    # # should be encrypted based on the repo
    # if (!defined($oFile))
    # {
    #     if (defined($self->{strCipherType}))
    #     {
    #         $bEncrypted = true;
    #     }
    # }
    # else
    # {
    #     # If the file does exist, then read the magic signature
    #     my $lSizeRead = $oFile->read(\$tMagicSignature, length(CIPHER_MAGIC));
    #
    #     # Close the file handle
    #     $oFile->close();
    #
    #     # If the file is able to be read, then if it is encrypted it must at least have the magic signature, even if it were
    #     # originally a 0 byte file
    #     if (($lSizeRead > 0) && substr($tMagicSignature, 0, length(CIPHER_MAGIC)) eq CIPHER_MAGIC)
    #     {
    #         $bEncrypted = true;
    #     }
    #
    # }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bEncrypted', value => $bEncrypted}
    );
}

####################################################################################################################################
# encryptionValid - determine if encyption set properly based on the value passed
####################################################################################################################################
sub encryptionValid
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $bEncrypted,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->encryptionValid', \@_,
            {name => 'bEncrypted'},
        );

    my $bValid = true;

    # # If encryption is set on the file then make sure the repo is encrypted and visa-versa
    # if ($bEncrypted)
    # {
    #     if (!defined($self->{strCipherType}))
    #     {
    #         $bValid = false;
    #     }
    # }
    # else
    # {
    #     if (defined($self->{strCipherType}))
    #     {
    #         $bValid = false;
    #     }
    # }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bValid', value => $bValid}
    );
}

####################################################################################################################################
# Getters
####################################################################################################################################
sub type {shift->{oStorageC}->type()}
# sub pathBase {shift->{strPathBase}}
sub cipherType {shift->{strCipherType}}
sub cipherPassUser {shift->{strCipherPassUser}}

1;
