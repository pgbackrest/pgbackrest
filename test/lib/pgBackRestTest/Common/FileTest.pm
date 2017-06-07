####################################################################################################################################
# CommonTest.pm - Common globals used for testing
####################################################################################################################################
package pgBackRestTest::Common::FileTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Cwd qw(abs_path cwd);
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);
use File::Copy qw(move);
use File::Path qw(remove_tree);
use IO::Select;
use IPC::Open3;
use POSIX ':sys_wait_h';
use Symbol 'gensym';

use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::Manifest;
use pgBackRest::Storage::Local;

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::LogTest;
use pgBackRestTest::Common::VmTest;
use pgBackRestTest::Env::Host::HostBackupTest;
use pgBackRestTest::Env::Host::HostDbCommonTest;
use pgBackRestTest::Env::Host::HostDbTest;

####################################################################################################################################
# testLinkCreate
#
# Create a symlink
####################################################################################################################################
sub testLinkCreate
{
    my $strLink = shift;
    my $strDestination = shift;

    # Create the file
    symlink($strDestination, $strLink)
        or confess "unable to link ${strLink} to ${strDestination}";
}

push(@EXPORT, qw(testLinkCreate));

####################################################################################################################################
# testPathMode
#
# Set mode of an existing path.
####################################################################################################################################
sub testPathMode
{
    my $strPath = shift;
    my $strMode = shift;

    # Set the mode
    chmod(oct($strMode), $strPath)
        or confess 'unable to set mode ${strMode} for ${strPath}';
}

push(@EXPORT, qw(testPathMode));

####################################################################################################################################
# testPathRemove
#
# Remove a path and all subpaths.
####################################################################################################################################
sub testPathRemove
{
    my $strPath = shift;
    my $bSuppressError = shift;

    executeTest('sudo rm -rf ' . $strPath, {bSuppressError => $bSuppressError});
}

push(@EXPORT, qw(testPathRemove));

####################################################################################################################################
# testFileCreate
#
# Create a file specifying content, mode, and time.
####################################################################################################################################
sub testFileCreate
{
    my $strFile = shift;
    my $strContent = shift;
    my $lTime = shift;
    my $strMode = shift;

    # Open the file and save strContent to it
    my $hFile = shift;

    open($hFile, '>', $strFile)
        or confess "unable to open ${strFile} for writing";

    syswrite($hFile, $strContent)
        or confess "unable to write to ${strFile}: $!";

    close($hFile);

    # Set the time
    if (defined($lTime))
    {
        utime($lTime, $lTime, $strFile)
            or confess 'unable to set time ${lTime} for ${strPath}';
    }

    # Set the mode
    chmod(oct(defined($strMode) ? $strMode : '0600'), $strFile)
        or confess 'unable to set mode ${strMode} for ${strFile}';
}

push(@EXPORT, qw(testFileCreate));

####################################################################################################################################
# testFileRemove
#
# Remove a file.
####################################################################################################################################
sub testFileRemove
{
    my $strFile = shift;

    unlink($strFile)
        or confess "unable to remove ${strFile}: $!";
}

push(@EXPORT, qw(testFileRemove));

####################################################################################################################################
# forceStorageMode - force mode on a file or path
####################################################################################################################################
sub forceStorageMode
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oStorage,
        $strPathExp,
        $strMode,
        $bRecurse
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::forceStorageMode', \@_,
            {name => 'oStorage'},
            {name => 'strPathExp'},
            {name => 'strMode'},
            {name => 'bRecurse', optional => true, default => false},
        );

    executeTest('sudo chmod ' . ($bRecurse ? '-R ' : '') . "${strMode} " . $oStorage->pathGet($strPathExp));

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

push(@EXPORT, qw(forceStorageMode));

####################################################################################################################################
# forceStorageMove - force move a directory or file
####################################################################################################################################
sub forceStorageMove
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oStorage,
        $strSourcePathExp,
        $strDestinationPathExp,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->forceStorageMove', \@_,
            {name => 'oStorage'},
            {name => 'strSourcePathExp'},
            {name => 'strDestinationPathExp'},
        );

    executeTest('sudo mv ' . $oStorage->pathGet($strSourcePathExp) . ' ' . $oStorage->pathGet($strDestinationPathExp));

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

push(@EXPORT, qw(forceStorageMove));

####################################################################################################################################
# forceStorageOwner - force ownership on a file or path
####################################################################################################################################
sub forceStorageOwner
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oStorage,
        $strPathExp,
        $strOwner,
        $bRecurse
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::forceStorageOwner', \@_,
            {name => 'oStorage'},
            {name => 'strPathExp'},
            {name => 'strOwner'},
            {name => 'bRecurse', optional => true, default => false},
        );

    executeTest('sudo chown ' . ($bRecurse ? '-R ' : '') . "${strOwner} " . $oStorage->pathGet($strPathExp));

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

push(@EXPORT, qw(forceStorageOwner));

####################################################################################################################################
# forceStorageRemove - force remove a file or path from storage
####################################################################################################################################
sub forceStorageRemove
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oStorage,
        $strPathExp,
        $bRecurse
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->forceStorageRemove', \@_,
            {name => 'oStorage'},
            {name => 'strPathExp'},
            {name => 'bRecurse', optional => true, default => false},
        );

    executeTest('sudo rm -f' . ($bRecurse ? 'r ' : ' ') . $oStorage->pathGet($strPathExp));

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

push(@EXPORT, qw(forceStorageRemove));

1;
