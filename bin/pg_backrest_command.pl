#!/usr/bin/perl
####################################################################################################################################
# pg_backrest_command.pl - Simple Postgres Backup and Restore Command Helper
####################################################################################################################################

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings;
use english;

use File::Basename;
use Getopt::Long;
use Carp;

use lib dirname($0) . '/..';
use pg_backrest_utility;
use pg_backrest_file;

####################################################################################################################################
# Operation constants - basic operations that are allowed in backrest command
####################################################################################################################################
use constant
{
    OP_LIST     => "list",
    OP_EXISTS   => "exists",
    OP_HASH     => "hash",
    OP_REMOVE   => "remove",
    OP_MANIFEST => "manifest"
};

####################################################################################################################################
# Command line parameters
####################################################################################################################################
my $bIgnoreMissing = false;     # Ignore errors due to missing file
my $strExpression = undef;      # Expression to use for filtering
my $strSort = undef;            # Sort order (undef = forward)

GetOptions ("ignore-missing" => \$bIgnoreMissing,
            "expression=s" => \$strExpression,
            "sort=s" => \$strSort)
    or die("Error in command line arguments\n");

####################################################################################################################################
# START MAIN
####################################################################################################################################
# Turn off logging
log_level_set(OFF, OFF);

# Get the operation
my $strOperation = $ARGV[0];

# Validate the operation
if (!defined($strOperation))
{
    confess &log(ERROR, "operation is not defined");
}

if ($strOperation ne OP_LIST &&
    $strOperation ne OP_EXISTS &&
    $strOperation ne OP_HASH &&
    $strOperation ne OP_REMOVE &&
    $strOperation ne OP_MANIFEST)
{
    confess &log(ERROR, "invalid operation ${strOperation}");
}

# Create the file object
my $oFile = pg_backrest_file->new();

####################################################################################################################################
# LIST Command
####################################################################################################################################
if ($strOperation eq OP_LIST)
{
    my $strPath = $ARGV[1];
    
    if (!defined($strPath))
    {
        confess "path must be specified for list operation";
    }
    
    my $bFirst = true;

    foreach my $strFile ($oFile->list(PATH_ABSOLUTE, $strPath, $strExpression, $strSort))
    {
        $bFirst ? $bFirst = false : print "\n";
        
        print $strFile;
    }

    exit 0;
}

####################################################################################################################################
# EXISTS Command
####################################################################################################################################
if ($strOperation eq OP_EXISTS)
{
    my $strFile = $ARGV[1];
    
    if (!defined($strFile))
    {
        confess "filename must be specified for exist operation";
    }

    print $oFile->exists(PATH_ABSOLUTE, $strFile) ? "Y" : "N";

    exit 0;
}

####################################################################################################################################
# HASH Command
####################################################################################################################################
if ($strOperation eq OP_HASH)
{
    my $strFile = $ARGV[1];
    
    if (!defined($strFile))
    {
        confess "filename must be specified for hash operation";
    }

    print $oFile->hash(PATH_ABSOLUTE, $strFile);

    exit 0;
}

####################################################################################################################################
# REMOVE Command
####################################################################################################################################
if ($strOperation eq OP_REMOVE)
{
    my $strFile = $ARGV[1];
    
    if (!defined($strFile))
    {
        confess "filename must be specified for remove operation";
    }

    print $oFile->remove(PATH_ABSOLUTE, $strFile, undef, $bIgnoreMissing) ? "Y" : "N";

    exit 0;
}

####################################################################################################################################
# MANIFEST Command
####################################################################################################################################
if ($strOperation eq OP_MANIFEST)
{
    my $strPath = $ARGV[1];
    
    if (!defined($strPath))
    {
        confess "path must be specified for manifest operation";
    }

    my %oManifestHash;
    $oFile->manifest(PATH_ABSOLUTE, $strPath, \%oManifestHash);
    my $bFirst = true;

    foreach my $strName (sort(keys $oManifestHash{name}))
    {
        $bFirst ? $bFirst = false : print "\n";

        print "${strName}";
    }

    exit 0;
}
