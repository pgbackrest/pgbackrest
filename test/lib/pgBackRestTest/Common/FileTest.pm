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

use pgBackRestDoc::Common::Ini;
use pgBackRestDoc::Common::Log;
use pgBackRestDoc::Common::String;

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::HostGroupTest;
use pgBackRestTest::Common::LogTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Common::StorageBase;
use pgBackRestTest::Common::VmTest;
use pgBackRestTest::Common::Wait;
use pgBackRestTest::Env::Host::HostBaseTest;
use pgBackRestTest::Env::Host::HostBackupTest;
use pgBackRestTest::Env::Host::HostDbCommonTest;
use pgBackRestTest::Env::Host::HostDbTest;
use pgBackRestTest::Env::Host::HostS3Test;
use pgBackRestTest::Env::Manifest;

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

    executeTest('rm -rf ' . $strPath, {bSuppressError => $bSuppressError});
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

    if (defined($strContent) && $strContent ne '')
    {
        syswrite($hFile, $strContent)
            or confess "unable to write to ${strFile}: $!";
    }

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
        $strPath,
        $strMode,
        $bRecurse
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::forceStorageMode', \@_,
            {name => 'oStorage'},
            {name => 'strPath'},
            {name => 'strMode'},
            {name => 'bRecurse', optional => true, default => false},
        );

    # Mode commands are ignored on object storage
    if ($oStorage->type() ne STORAGE_OBJECT)
    {
        executeTest('chmod ' . ($bRecurse ? '-R ' : '') . "${strMode} ${strPath}");
    }

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
        $strSourcePath,
        $strDestinationPath,
        $bRecurse,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->forceStorageMove', \@_,
            {name => 'oStorage'},
            {name => 'strSourcePath'},
            {name => 'strDestinationPath'},
            {name => 'bRecurse', optional => true, default => true},
        );

    # If object storage then use storage commands to remove
    if ($oStorage->type() eq STORAGE_OBJECT)
    {
        if ($bRecurse)
        {
            my $rhManifest = $oStorage->manifest($strSourcePath);

            foreach my $strName (sort(keys(%{$rhManifest})))
            {
                if ($rhManifest->{$strName}{type} eq 'f')
                {
                    $oStorage->put(
                        "${strDestinationPath}/${strName}", ${$oStorage->get("${strSourcePath}/${strName}", {bRaw => true})},
                        {bRaw => true});
                }
            }

            $oStorage->pathRemove($strSourcePath, {bRecurse => true});
        }
        else
        {
            $oStorage->put($strDestinationPath, ${$oStorage->get($strSourcePath, {bRaw => true})}, {bRaw => true});
            $oStorage->remove($strSourcePath);
        }
    }
    # Else remove using filesystem commands
    else
    {
        executeTest("mv ${strSourcePath} ${strDestinationPath}");
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

push(@EXPORT, qw(forceStorageMove));

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
        $strPath,
        $bRecurse
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->forceStorageRemove', \@_,
            {name => 'oStorage'},
            {name => 'strPath'},
            {name => 'bRecurse', optional => true, default => false},
        );

    # If object storage then use storage commands to remove
    if ($oStorage->type() eq STORAGE_OBJECT)
    {
        $oStorage->pathRemove($strPath, {bRecurse => true});
    }
    else
    {
        executeTest('rm -f' . ($bRecurse ? 'r ' : ' ') . $strPath);
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

push(@EXPORT, qw(forceStorageRemove));

1;
