#!/usr/bin/perl

#use strict;

my $strPgBinPath = '/Library/PostgreSQL/9.3/bin/';

sub execute
{
    local($strCommand) = @_;
    my $strOutput;

    print("$strCommand\n");
    $strOutput = qx($strCommand) or return 0;
    print("$strOutput\n");
    
    return 1;
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

pg_stop("/Users/dsteele/test/test2");
pg_drop("/Users/dsteele/test/test2");
pg_create("/Users/dsteele/test/test2");
pg_start("/Users/dsteele/test/test2");
pg_stop("/Users/dsteele/test/test2");

