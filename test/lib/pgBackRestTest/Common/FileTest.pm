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
use pgBackRest::File;
use pgBackRest::Manifest;

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::LogTest;
use pgBackRestTest::Common::VmTest;
use pgBackRestTest::Common::Host::HostBackupTest;
use pgBackRestTest::Common::Host::HostDbCommonTest;
use pgBackRestTest::Common::Host::HostDbTest;

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

    # remove_tree($strPath, {result => \my $oError});
    #
    # if (@$oError)
    # {
    #     my $strMessage = "error(s) occurred while removing ${strPath}:";
    #
    #     for my $strFile (@$oError)
    #     {
    #         $strMessage .= "\nunable to remove: " . $strFile;
    #     }
    #
    #     confess $strMessage;
    # }
}

push(@EXPORT, qw(testPathRemove));

####################################################################################################################################
# testPathCopy
#
# Copy a path.
####################################################################################################################################
sub testPathCopy
{
    my $strSourcePath = shift;
    my $strDestinationPath = shift;
    my $bSuppressError = shift;

    executeTest("cp -RpP ${strSourcePath} ${strDestinationPath}", {bSuppressError => $bSuppressError});
}

####################################################################################################################################
# testPathMove
#
# Copy a path.
####################################################################################################################################
sub testPathMove
{
    my $strSourcePath = shift;
    my $strDestinationPath = shift;
    my $bSuppressError = shift;

    testPathCopy($strSourcePath, $strDestinationPath, $bSuppressError);
    testPathRemove($strSourcePath, $bSuppressError);
}

push(@EXPORT, qw(testPathMove));

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

1;
