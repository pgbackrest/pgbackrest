#!/usr/bin/perl

#use strict;
use DBI;

my $strPgBinPath = '/Library/PostgreSQL/9.3/bin/';

sub execute
{
    local($strCommand) = @_;
    my $strOutput;

    print("$strCommand\n");
    $strOutput = qx($strCommand) or return 0;
    $strOutput =~ s/^\s+|\s+$//g;
    print("$strOutput\n");
    
    return $strOutput;
}

sub pg_create
{
    local($strPath) = @_;
    
    execute($strPgBinPath . "initdb -D $strPath -A trust -k");
    execute("mkdir $strPath/archive");
}

sub pg_start
{
    local($strPath) = @_;
    my $strCommand = $strPgBinPath . "pg_ctl start -o \"-c port=6000 -c wal_level=archive -c archive_mode=on -c archive_command='test ! -f $strPath/archive/%f && cp %p $strPath/archive/%f'\" -D $strPath -l $strPath/postgresql.log -w -s";
    
    execute($strCommand);
}

sub pg_password_set
{
    local($strPath, $strUser) = @_;
    my $strCommand = $strPgBinPath . "psql --port=6000 -c \"alter user $strUser with password 'password'\" postgres";
    
    execute($strCommand);
}

sub pg_stop
{
    local($strPath) = @_;
    my $strCommand = $strPgBinPath . "pg_ctl stop -D $strPath -w -s";
    
    execute($strCommand);
}

sub pg_drop
{
    local($strPath) = @_;
    my $strCommand = "rm -rf $strPath";
    
    execute($strCommand);
}

sub pg_execute
{
    local($dbh, $strSql) = @_;

    $sth = $dbh->prepare($strSql);
    $sth->execute();
}

my $strUser = execute('whoami');
my $strTestPath = "/Users/dsteele/test/";
my $strTestDir = "test2";
my $strArchiveDir = "archive";

pg_stop("$strTestPath$strTestDir");
pg_drop("$strTestPath$strTestDir");
pg_create("$strTestPath$strTestDir");
pg_start("$strTestPath$strTestDir");
pg_password_set("$strTestPath$strTestDir", $strUser);

$dbh = DBI->connect("dbi:Pg:dbname=postgres;port=6000;host=127.0.0.1", 'dsteele', 'password', {AutoCommit => 1});
pg_execute($dbh, "create table test (id int)");

pg_execute($dbh, "insert into test values (1)");
pg_execute($dbh, "select pg_switch_xlog()");

pg_execute($dbh, "insert into test values (2)");
pg_execute($dbh, "select pg_switch_xlog()");

pg_execute($dbh, "insert into test values (3)");
pg_execute($dbh, "select pg_switch_xlog()");

#pg_stop($strTestPath);
