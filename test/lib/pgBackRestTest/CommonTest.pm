####################################################################################################################################
# CommonTest.pm - Common globals used for testing
####################################################################################################################################
package pgBackRestTest::CommonTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Cwd qw(abs_path);
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);

use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;

use pgBackRestTest::Common::LogTest;

####################################################################################################################################
# Module variables
####################################################################################################################################
my $strPgSqlBin;
my $strCommonCommandMain;
my $strCommonBasePath;
my $strCommonTestPath;
my $strCommonDataPath;
my $strCommonRepoPath;
my $strCommonDbPath;
my $strCommonDbCommonPath;
my $iModuleTestRunOnly;
my $bDryRun;
my $bNoCleanup;
my $bLogForce;

####################################################################################################################################
# testRun
####################################################################################################################################
sub testRun
{
    my $iRun = shift;
    my $strLog = shift;
    my $strModuleParam = shift;
    my $strModuleTestParam = shift;
    my $oLogTestRef = shift;

    # Save the previous test log
    if (defined($$oLogTestRef))
    {
        $$oLogTestRef->logWrite($strCommonBasePath, $strCommonTestPath);
        $$oLogTestRef = undef;
    }

    # Return if this test should not be run
    if (defined($iModuleTestRunOnly) && $iModuleTestRunOnly != $iRun)
    {
        return false;
    }

    # Output information about test to run
    &log(INFO, 'run ' . sprintf('%03d', $iRun) . ' - ' . $strLog);

    if ($bDryRun)
    {
        return false;
    }

    # If the module is defined then create a LogTest object
    if (defined($strModuleParam))
    {
        $$oLogTestRef = new pgBackRestTest::Common::LogTest(
            $strModuleParam, $strModuleTestParam, $iRun, $bLogForce, $strLog, $strCommonCommandMain, $strPgSqlBin,
            $strCommonTestPath, $strCommonRepoPath);
    }

    return true;
}

push(@EXPORT, qw(testRun));

####################################################################################################################################
# testCleanup
####################################################################################################################################
sub testCleanup
{
    my $oLogTestRef = shift;

    # Save the previous test log
    if (defined($$oLogTestRef))
    {
        $$oLogTestRef->logWrite($strCommonBasePath, $strCommonTestPath);
        $$oLogTestRef = undef;
    }

    # Return false if there is no cleanup or if this was a test run (this prevents cleanup from being run)
    return !$bNoCleanup && !$bDryRun;
}

push(@EXPORT, qw(testCleanup));

####################################################################################################################################
# testSetup
####################################################################################################################################
sub testSetup
{
    my $strTestPathParam = shift;
    my $strPgSqlBinParam = shift;
    my $iModuleTestRunOnlyParam = shift;
    my $bDryRunParam = shift;
    my $bNoCleanupParam = shift;
    my $bLogForceParam = shift;

    $strCommonBasePath = dirname(dirname(abs_path($0)));

    $strPgSqlBin = $strPgSqlBinParam;

    if (defined($strTestPathParam))
    {
        $strCommonTestPath = $strTestPathParam;
    }
    else
    {
        $strCommonTestPath = cwd() . "/test";
    }

    $strCommonDataPath = "${strCommonBasePath}/test/data";
    $strCommonRepoPath = "${strCommonTestPath}/backrest";
    $strCommonDbPath = "${strCommonTestPath}/db-master/db";
    $strCommonDbCommonPath = "${strCommonDbPath}/common";

    $strCommonCommandMain = $strCommonBasePath . '/bin/pgbackrest';

    $iModuleTestRunOnly = $iModuleTestRunOnlyParam;
    $bDryRun = $bDryRunParam;
    $bNoCleanup = $bNoCleanupParam;
    $bLogForce = $bLogForceParam;

    return true;
}

push(@EXPORT, qw(testSetup));

####################################################################################################################################
# testIniSave
####################################################################################################################################
sub testIniSave
{
    my $strFileName = shift;
    my $oIniRef = shift;
    my $bChecksum = shift;

    # Calculate a new checksum if requested
    if (defined($bChecksum) && $bChecksum)
    {
        delete($$oIniRef{&INI_SECTION_BACKREST}{&INI_KEY_CHECKSUM});

        my $oSHA = Digest::SHA->new('sha1');
        my $oJSON = JSON::PP->new()->canonical()->allow_nonref();
        $oSHA->add($oJSON->encode($oIniRef));

        $$oIniRef{&INI_SECTION_BACKREST}{&INI_KEY_CHECKSUM} = $oSHA->hexdigest();
    }

    iniSave($strFileName, $oIniRef);
}

push(@EXPORT, qw(testIniSave));

####################################################################################################################################
# Get Methods
####################################################################################################################################
sub testDataPath
{
    return $strCommonDataPath;
}

push(@EXPORT, qw(testDataPath));

1;
