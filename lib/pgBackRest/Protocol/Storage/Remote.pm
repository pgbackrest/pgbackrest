####################################################################################################################################
# Remote Storage
####################################################################################################################################
package pgBackRest::Protocol::Storage::Remote;
use parent 'pgBackRest::Storage::Base';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::Protocol::Helper;
use pgBackRest::Protocol::Storage::File;
use pgBackRest::Storage::Base;
use pgBackRest::Storage::Filter::Gzip;

####################################################################################################################################
# new
####################################################################################################################################
sub new
{
    my $class = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oProtocol,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'oProtocol'},
        );

    # Init parent
    my $self = $class->SUPER::new({lBufferMax => $oProtocol->io()->bufferMax()});
    bless $self, $class;

    # Set variables
    $self->{oProtocol} = $oProtocol;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# exists
####################################################################################################################################
sub exists
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
            __PACKAGE__ . '->exists', \@_,
            {name => 'strPathExp'},
        );

    my $bExists = $self->{oProtocol}->cmdExecute(OP_STORAGE_EXISTS, [$strPathExp]);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bExists', value => $bExists}
    );
}

####################################################################################################################################
# hashSize
####################################################################################################################################
sub hashSize
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathExp,
        $rhParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->hashSize', \@_,
            {name => 'strPathExp'},
            {name => 'rhParam', required => false},
        );

    my ($strHash, $lSize) = $self->{oProtocol}->cmdExecute(OP_STORAGE_HASH_SIZE, [$strPathExp, $rhParam]);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strHash', value => $strHash},
        {name => 'lSize', value => $lSize}
    );
}

####################################################################################################################################
# list
####################################################################################################################################
sub list
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathExp,
        $rhParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->list', \@_,
            {name => 'strPathExp'},
            {name => 'rhParam', required => false},
        );

    my @stryFileList = $self->{oProtocol}->cmdExecute(OP_STORAGE_LIST, [$strPathExp, $rhParam]);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'stryFileList', value => \@stryFileList}
    );
}

####################################################################################################################################
# manifest
####################################################################################################################################
sub manifest
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathExp,
        $rhParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->manifest', \@_,
            {name => 'strPathExp'},
            {name => 'rhParam', required => false},
        );

    my $hManifest = $self->{oProtocol}->cmdExecute(OP_STORAGE_MANIFEST, [$strPathExp, $rhParam]);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'hManifest', value => $hManifest, trace => true}
    );
}

####################################################################################################################################
# openRead
####################################################################################################################################
sub openRead
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFileExp,
        $rhParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->openRead', \@_,
            {name => 'strFileExp'},
            {name => 'rhParam', required => false},
        );

    # Determine whether protocol compress will be used
    my $bProtocolCompress = protocolCompress($rhParam);

    # Compress on the remote side
    if ($bProtocolCompress)
    {
        push(
            @{$rhParam->{rhyFilter}},
            {strClass => STORAGE_FILTER_GZIP, rxyParam => [STORAGE_COMPRESS, true, cfgOption(CFGOPT_COMPRESS_LEVEL_NETWORK)]});
    }

    my $oSourceFileIo =
        $self->{oProtocol}->cmdExecute(OP_STORAGE_OPEN_READ, [$strFileExp, $rhParam]) ?
            new pgBackRest::Protocol::Storage::File($self->{oProtocol}) : undef;

    # Decompress on the local side
    if ($bProtocolCompress)
    {
        $oSourceFileIo = new pgBackRest::Storage::Filter::Gzip(
            $oSourceFileIo, {strCompressType => STORAGE_DECOMPRESS, bWantGzip => false});
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oFileIo', value => $oSourceFileIo, trace => true},
    );
}

####################################################################################################################################
# openWrite
####################################################################################################################################
sub openWrite
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFileExp,
        $rhParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->openWrite', \@_,
            {name => 'strFileExp'},
            {name => 'rhParam', required => false},
        );

    # Determine whether protocol compress will be used
    my $bProtocolCompress = protocolCompress($rhParam);

    # Decompress on the remote side
    if ($bProtocolCompress)
    {
        push(
            @{$rhParam->{rhyFilter}},
            {strClass => STORAGE_FILTER_GZIP, rxyParam => [STORAGE_DECOMPRESS, true]});
    }

    # Open the remote file
    $self->{oProtocol}->cmdWrite(OP_STORAGE_OPEN_WRITE, [$strFileExp, $rhParam]);
    my $oDestinationFileIo = new pgBackRest::Protocol::Storage::File($self->{oProtocol});

    # Compress on local side
    if ($bProtocolCompress)
    {
        $oDestinationFileIo = new pgBackRest::Storage::Filter::Gzip(
            $oDestinationFileIo, {iLevel => cfgOption(CFGOPT_COMPRESS_LEVEL_NETWORK), bWantGzip => false});
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oFileIo', value => $oDestinationFileIo, trace => true},
    );
}

####################################################################################################################################
# pathGet
####################################################################################################################################
sub pathGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathExp,
        $rhParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->pathGet', \@_,
            {name => 'strPathExp', required => false},
            {name => 'rhParam', required => false},
        );

    my $strPath = $self->{oProtocol}->cmdExecute(OP_STORAGE_PATH_GET, [$strPathExp, $rhParam]);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strPath', value => $strPath}
    );
}

####################################################################################################################################
# move
####################################################################################################################################
sub move
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strSourcePathExp,
        $strDestinationPathExp,
        $rhParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->move', \@_,
            {name => 'strSourcePathExp'},
            {name => 'strDestinationPathExp'},
            {name => 'rhParam', required => false},
        );

    $self->{oProtocol}->cmdExecute(OP_STORAGE_MOVE, [$strSourcePathExp, $strDestinationPathExp, $rhParam], false);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

####################################################################################################################################
# cipherPassUser
####################################################################################################################################
sub cipherPassUser
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->cipherPassUser', \@_,
        );

    my $strCipherPassUser = $self->{oProtocol}->cmdExecute(OP_STORAGE_CIPHER_PASS_USER);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strCipherPassUser', value => $strCipherPassUser, redact => true}
    );
}

####################################################################################################################################
# Used internally to determine if protocol compression should be enabled
####################################################################################################################################
sub protocolCompress
{
    my $rhParam = shift;

    my $bProtocolCompress = false;

    if (defined($rhParam->{bProtocolCompress}))
    {
        $bProtocolCompress = $rhParam->{bProtocolCompress} && cfgOption(CFGOPT_COMPRESS_LEVEL_NETWORK) > 0 ? true : false;
        delete($rhParam->{bProtocolCompress});
    }

    return $bProtocolCompress;
}

####################################################################################################################################
# getters
####################################################################################################################################
sub protocol {shift->{oProtocol}};

1;
