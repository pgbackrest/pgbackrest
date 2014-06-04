#!/usr/bin/perl

# /Library/PostgreSQL/9.3/bin/pg_ctl start -o "-c port=7000" -D /Users/backrest/test/backup/db/20140205-103801F/base -l /Users/backrest/test/backup/db/20140205-103801F/base/postgresql.log -w -s

#use strict;
use DBI;
use IPC::System::Simple qw(capture);
use Config::IniFiles;
use File::Find;

sub trim
{
    local($strBuffer) = @_;

	$strBuffer =~ s/^\s+|\s+$//g;

	return $strBuffer;
}

sub execute
{
    local($strCommand) = @_;
    my $strOutput;

    print("$strCommand");
    $strOutput = trim(capture($strCommand));

    if ($strOutput eq "")
    {
        print(" ... complete\n\n");
    }
    else
    {
        print(" ... complete\n$strOutput\n\n");
    }

    return $strOutput;
}

sub pg_create
{
    local($strPgBinPath, $strTestPath, $strTestDir, $strArchiveDir, $strBackupDir) = @_;

    execute("mkdir $strTestPath");
    execute("mkdir $strTestPath/$strTestDir");
    execute("mkdir $strTestPath/$strTestDir/ts1");
    execute("mkdir $strTestPath/$strTestDir/ts2");
    execute($strPgBinPath . "/initdb -D $strTestPath/$strTestDir/common -A trust -k");
    execute("mkdir $strTestPath/$strBackupDir");
#    execute("mkdir -p $strTestPath/$strArchiveDir");
}

sub pg_start
{
    local($strPgBinPath, $strDbPath, $strPort, $strAchiveCommand) = @_;
    my $strCommand = "$strPgBinPath/pg_ctl start -o \"-c port=$strPort -c checkpoint_segments=1 -c wal_level=archive -c archive_mode=on -c archive_command=\'$strAchiveCommand\'\" -D $strDbPath -l $strDbPath/postgresql.log -w -s";

    execute($strCommand);
}

sub pg_password_set
{
    local($strPgBinPath, $strPath, $strUser, $strPort) = @_;
    my $strCommand = "$strPgBinPath/psql --port=$strPort -c \"alter user $strUser with password 'password'\" postgres";

    execute($strCommand);
}

sub pg_stop
{
    local($strPgBinPath, $strPath) = @_;
    my $strCommand = "$strPgBinPath/pg_ctl stop -D $strPath -w -s -m fast";

    execute($strCommand);
}

sub pg_drop
{
    local($strTestPath) = @_;
    my $strCommand = "rm -rf $strTestPath";

    execute($strCommand);
}

sub pg_execute
{
    local($dbh, $strSql) = @_;

    print($strSql);
    $sth = $dbh->prepare($strSql);
    $sth->execute() or die;
    $sth->finish();

    print(" ... complete\n\n");
}

sub archive_command_build
{
    my $strBackRestBinPath = shift;
    my $strDestinationPath = shift;
    my $bCompression = shift;
    my $bChecksum = shift;

    my $strCommand = "$strBackRestBinPath/pg_backrest.pl --stanza=db --config=$strBackRestBinPath/pg_backrest.conf";

#    if (!$bCompression)
#    {
#        $strCommand .= " --no-compression"
#    }
#
#    if (!$bChecksum)
#    {
#        $strCommand .= " --no-checksum"
#    }

    return $strCommand . " archive-push %p";
}

sub wait_for_file
{
    my $strDir = shift;
    my $strRegEx = shift;
    my $iSeconds = shift;

    my $lTime = time();
    my $hDir;

    while ($lTime > time() - $iSeconds)
    {
        opendir $hDir, $strDir or die "Could not open dir: $!\n";
        my @stryFile = grep(/$strRegEx/i, readdir $hDir);
        close $hDir;

        if (scalar @stryFile == 1)
        {
            return;
        }

        sleep(1);
    }

    die "could not find $strDir/$strRegEx after $iSeconds second(s)";
}

sub pgbr_backup
{
    my $strBackRestBinPath = shift;
    my $strCluster = shift;

    my $strCommand = "$strBackRestBinPath/pg_backrest.pl --config=$strBackRestBinPath/pg_backrest.conf backup $strCluster";

    execute($strCommand);
}

my $strUser = execute('whoami');

my $strTestPath = "/Users/dsteele/test";
my $strDbDir = "db";
my $strArchiveDir = "backup/db/archive";
my $strBackupDir = "backup";

my $strPgBinPath = "/Library/PostgreSQL/9.3/bin";
my $strPort = "6001";

my $strBackRestBinPath = "/Users/dsteele/pg_backrest";
my $strArchiveCommand = archive_command_build($strBackRestBinPath, "$strTestPath/$strArchiveDir", 0, 0);

################################################################################
# Stop the current test cluster if it is running and create a new one
################################################################################
eval {pg_stop($strPgBinPath, "$strTestPath/$strDbDir")};

if ($@)
{
    print(" ... unable to stop pg server (ignoring): " . trim($@) . "\n\n")
}

pg_drop($strTestPath);
pg_create($strPgBinPath, $strTestPath, $strDbDir, $strArchiveDir, $strBackupDir);
pg_start($strPgBinPath, "$strTestPath/$strDbDir/common", $strPort, $strArchiveCommand);
pg_password_set($strPgBinPath, "$strTestPath/$strDbDir/common", $strUser, $strPort);

################################################################################
# Connect and start tests
################################################################################
$dbh = DBI->connect("dbi:Pg:dbname=postgres;port=$strPort;host=127.0.0.1", $strUser,
                    'password', {AutoCommit => 1});

pg_execute($dbh, "create tablespace ts1 location '$strTestPath/$strDbDir/ts1'");
pg_execute($dbh, "create tablespace ts2 location '$strTestPath/$strDbDir/ts2'");

pg_execute($dbh, "create table test (id int)");
pg_execute($dbh, "create table test_ts1 (id int) tablespace ts1");
pg_execute($dbh, "create table test_ts2 (id int) tablespace ts1");

pg_execute($dbh, "insert into test values (1)");
pg_execute($dbh, "select pg_switch_xlog()");

execute("mkdir -p $strTestPath/$strArchiveDir/0000000100000000");

# Test for archive log file 000000010000000000000001
wait_for_file("$strTestPath/$strArchiveDir/0000000100000000", "^000000010000000000000001\$", 5);

# Turn on log checksum for the next test
$dbh->disconnect();
pg_stop($strPgBinPath, "$strTestPath/$strDbDir/common");
$strArchiveCommand = archive_command_build($strBackRestBinPath, "$strTestPath/$strArchiveDir", 0, 1);
pg_start($strPgBinPath, "$strTestPath/$strDbDir/common", $strPort, $strArchiveCommand);
$dbh = DBI->connect("dbi:Pg:dbname=postgres;port=$strPort;host=127.0.0.1", $strUser,
                    'password', {AutoCommit => 1});

# Write another value into the test table
pg_execute($dbh, "insert into test values (2)");
pg_execute($dbh, "select pg_switch_xlog()");

# Test for archive log file 000000010000000000000002
wait_for_file("$strTestPath/$strArchiveDir/0000000100000000", "^000000010000000000000002-([a-f]|[0-9]){40}\$", 5);

# Turn on log compression and checksum for the next test
$dbh->disconnect();
pg_stop($strPgBinPath, "$strTestPath/$strDbDir/common");
$strArchiveCommand = archive_command_build($strBackRestBinPath, "$strTestPath/$strArchiveDir", 1, 1);
pg_start($strPgBinPath, "$strTestPath/$strDbDir/common", $strPort, $strArchiveCommand);
$dbh = DBI->connect("dbi:Pg:dbname=postgres;port=$strPort;host=127.0.0.1", $strUser,
                    'password', {AutoCommit => 1});

# Write another value into the test table
pg_execute($dbh, "insert into test values (3)");
pg_execute($dbh, "select pg_switch_xlog()");

# Test for archive log file 000000010000000000000003
wait_for_file("$strTestPath/$strArchiveDir/0000000100000000", "^000000010000000000000003-([a-f]|[0-9]){40}\\.gz\$", 5);

$dbh->disconnect();

################################################################################
# Stop the server
################################################################################
#pg_stop($strPgBinPath, "$strTestPath/$strDbDir/common");

################################################################################
# Start an offline backup
################################################################################
#pgbr_backup($strBackRestBinPath, "db");
