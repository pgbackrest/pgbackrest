####################################################################################################################################
# C Storage Interface
####################################################################################################################################
package pgBackRestTest::Common::StorageRepo;
use parent 'pgBackRestTest::Common::StorageBase';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Digest::SHA qw(sha1_hex);
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);
use Fcntl qw(:mode);
use File::stat qw{lstat};
use JSON::PP;

use pgBackRestDoc::Common::Exception;
use pgBackRestDoc::Common::Log;
use pgBackRestDoc::ProjectInfo;

use pgBackRestTest::Common::Io::Handle;
use pgBackRestTest::Common::Io::Process;
use pgBackRestTest::Common::StorageBase;

####################################################################################################################################
# Temp file extension
####################################################################################################################################
use constant STORAGE_TEMP_EXT                                       => PROJECT_EXE . '.tmp';
    push @EXPORT, qw(STORAGE_TEMP_EXT);

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
        $self->{strCommand},
        $self->{strType},
        $self->{lBufferMax},
        $self->{iTimeoutIo},
        $self->{iRepo},
        $self->{strDefaultPathMode},
        $self->{strDefaultFileMode},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strCommand'},
            {name => 'strType'},
            {name => 'lBufferMax'},
            {name => 'iTimeoutIo'},
            {name => 'iRepo'},
            {name => 'strDefaultPathMode', optional => true, default => '0750'},
            {name => 'strDefaultFileMode', optional => true, default => '0640'},
        );

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
# Escape characteres that have special meaning on the command line
####################################################################################################################################
sub escape
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strValue,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->escape', \@_,
            {name => 'strValue', trace => true},
        );

    $strValue =~ s/\\/\\\\/g;
    $strValue =~ s/\</\\\</g;
    $strValue =~ s/\>/\\\>/g;
    $strValue =~ s/\!/\\\!/g;
    $strValue =~ s/\*/\\\*/g;
    $strValue =~ s/\(/\\\(/g;
    $strValue =~ s/\)/\\\)/g;
    $strValue =~ s/\&/\\\&/g;
    $strValue =~ s/\'/\\\'/g;
    $strValue =~ s/\;/\\\;/g;
    $strValue =~ s/\?/\\\?/g;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strValue', value => $strValue},
    );
}

####################################################################################################################################
# Execute command and return the output
####################################################################################################################################
sub exec
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strCommand,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->exec', \@_,
            {name => 'strCommand'},
        );

    $strCommand = "$self->{strCommand} ${strCommand}";
    my $oBuffer = new pgBackRestTest::Common::Io::Buffered(
        new pgBackRestTest::Common::Io::Handle($strCommand), $self->{iTimeoutIo}, $self->{lBufferMax});
    my $oProcess = new pgBackRestTest::Common::Io::Process($oBuffer, $strCommand);

    my $tResult;

    while (!$oBuffer->eof())
    {
        $oBuffer->read(\$tResult, $self->{lBufferMax}, false);
    }

    $oProcess->close();

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'tResult', value => $tResult},
        {name => 'iExitStatus', value => $oProcess->exitStatus()},
    );
}

####################################################################################################################################
# Create storage
####################################################################################################################################
sub create
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->create');

    $self->exec("--repo=$self->{iRepo} repo-create");

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
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
        {name => 'bExists', value => $self->info($strFileExp, {bIgnoreMissing => true})->{type} eq 'f'}
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
        $bRaw,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->get', \@_,
            {name => 'xFile', required => false},
            {name => 'strCipherPass', optional => true, redact => true},
            {name => 'bRaw', optional => true, default => false},
        );

    # If openRead() was called first set values from that call
    my $strFile = $xFile;
    my $bIgnoreMissing = false;

    if (ref($xFile))
    {
        $strFile = $xFile->{strFile};
        $bIgnoreMissing = $xFile->{bIgnoreMissing};
        $strCipherPass = $xFile->{strCipherPass};
    }

    # Check invalid params
    if ($bRaw && defined($strCipherPass))
    {
        confess &log(ERROR, 'bRaw and strCipherPass cannot both be set');
    }

    # Get file
    my ($tResult, $iExitStatus) = $self->exec(
        (defined($strCipherPass) ? ' --cipher-pass=' . $self->escape($strCipherPass) : '') . ($bRaw ? ' --raw' : '') .
        ($bIgnoreMissing ? ' --ignore-missing' : '') . " --repo=$self->{iRepo} repo-get " . $self->escape($strFile));

    # Error if missing an not ignored
    if ($iExitStatus == 1 && !$bIgnoreMissing)
    {
        confess &log(ERROR, "unable to open '${strFile}'", ERROR_FILE_OPEN);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'rtContent', value => $iExitStatus == 0 ? \$tResult : undef, trace => true},
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

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'rhInfo', value => $self->manifest($strPathFileExp, {bRecurse => false})->{'.'}, trace => true}
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
    my $rhManifest = $self->manifest($strPathExp, {bRecurse => false});

    foreach my $strKey ($strSortOrder eq 'reverse' ? sort {$b cmp $a} keys(%{$rhManifest}) : sort keys(%{$rhManifest}))
    {
        next if $strKey eq '.';
        next if defined($strExpression) && $strKey !~ $strExpression;

        push(@{$rstryFileList}, $strKey);
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
        $bRecurse,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->manifest', \@_,
            {name => 'strPathExp'},
            {name => 'bRecurse', optional => true, default => true},
        );

    my $rhManifest = $self->{oJSON}->decode(
        $self->exec(
            "--output=json" . ($bRecurse ? ' --recurse' : '') . " --repo=$self->{iRepo} repo-ls " . $self->escape($strPathExp)));

    # Transform the manifest to the old format
    foreach my $strKey (keys(%{$rhManifest}))
    {
        if ($rhManifest->{$strKey}{type} eq 'file')
        {
            $rhManifest->{$strKey}{type} = 'f';

            if (defined($rhManifest->{$strKey}{time}))
            {
                $rhManifest->{$strKey}{modified_time} = $rhManifest->{$strKey}{time};
                delete($rhManifest->{$strKey}{time});
            }
        }
        elsif ($rhManifest->{$strKey}{type} eq 'path')
        {
            $rhManifest->{$strKey}{type} = 'd';
        }
        elsif ($rhManifest->{$strKey}{type} eq 'link')
        {
            $rhManifest->{$strKey}{type} = 'l';
        }
        elsif ($rhManifest->{$strKey}{type} eq 'special')
        {
            $rhManifest->{$strKey}{type} = 's';
        }
        else
        {
            confess "invalid file type '$rhManifest->{type}'";
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'rhManifest', value => $rhManifest, trace => true}
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
        $strFile,
        $bIgnoreMissing,
        $strCipherPass,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->openRead', \@_,
            {name => 'strFile'},
            {name => 'bIgnoreMissing', optional => true, default => false},
            {name => 'strCipherPass', optional => true, redact => true},
        );

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'rhFileIo', value => {strFile => $strFile, bIgnoreMissing => $bIgnoreMissing, strCipherPass => $strCipherPass},
            trace => true},
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
        $strPath,
        $bRecurse,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->pathRemove', \@_,
            {name => 'strPath'},
            {name => 'bRecurse', optional => true, default => false},
        );

    $self->exec("--repo=$self->{iRepo} repo-rm " . ($bRecurse ? '--recurse ' : '') . $self->escape($strPath));

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
        $strFile,
        $tContent,
        $strCipherPass,
        $bRaw,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->put', \@_,
            {name => 'strFile'},
            {name => 'tContent', required => false},
            {name => 'strCipherPass', optional => true, redact => true},
            {name => 'bRaw', optional => true, default => false},
        );

    # Check invalid params
    if ($bRaw && defined($strCipherPass))
    {
        confess &log(ERROR, 'bRaw and strCipherPass cannot both be set');
    }

    # Put file
    my $strCommand =
        "$self->{strCommand}" . (defined($strCipherPass) ? ' --cipher-pass=' . $self->escape($strCipherPass) : '') .
            ($bRaw ? ' --raw' : '') . " --repo=$self->{iRepo} repo-put " . $self->escape($strFile);

    my $oBuffer = new pgBackRestTest::Common::Io::Buffered(
        new pgBackRestTest::Common::Io::Handle($strCommand), $self->{iTimeoutIo}, $self->{lBufferMax});
    my $oProcess = new pgBackRestTest::Common::Io::Process($oBuffer, $strCommand);

    if (defined($tContent))
    {
        $oBuffer->write(\$tContent);
    }

    close($oBuffer->handleWrite());

    my $tResult;

    while (!$oBuffer->eof())
    {
        $oBuffer->read(\$tResult, $self->{lBufferMax}, false);
    }

    close($oBuffer->handleRead());
    $oProcess->close();

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
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
        $strFile,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->remove', \@_,
            {name => 'xFileExp'},
        );

    $self->exec("--repo=$self->{iRepo} repo-rm " . $self->escape($strFile));

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# Cache storage so it can be retrieved quickly
####################################################################################################################################
my $oRepoStorage;

####################################################################################################################################
# storageRepoCommandSet
####################################################################################################################################
my $strStorageRepoCommand;
my $strStorageRepoType;

sub storageRepoCommandSet
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strCommand,
        $strStorageType,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::storageRepoCommandSet', \@_,
            {name => 'strCommand'},
            {name => 'strStorageType'},
        );

    $strStorageRepoCommand = $strCommand;
    $strStorageRepoType = $strStorageType;

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

push @EXPORT, qw(storageRepoCommandSet);

####################################################################################################################################
# storageRepo - get repository storage
####################################################################################################################################
sub storageRepo
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strStanza,
        $iRepo,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::storageRepo', \@_,
            {name => 'strStanza', optional => true, trace => true},
            {name => 'iRepo', optional => true, default => 1, trace => true},
        );

    # Create storage if not defined
    if (!defined($oRepoStorage->{$iRepo}))
    {
        $oRepoStorage->{$iRepo} = new pgBackRestTest::Common::StorageRepo(
            $strStorageRepoCommand, $strStorageRepoType, 64 * 1024, 60, $iRepo);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oStorageRepo', value => $oRepoStorage->{$iRepo}, trace => true},
    );
}

push @EXPORT, qw(storageRepo);

####################################################################################################################################
# Getters
####################################################################################################################################
sub capability {shift->type() eq STORAGE_POSIX}
sub type {shift->{strType}}

1;
