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
    OP_MANIFEST => "manifest",
    OP_COMPRESS => "compress"
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

    print "name\ttype\tuser\tgroup\tpermission\tmodification_time\tinode\tsize\tlink_destination";

    foreach my $strName (sort(keys $oManifestHash{name}))
    {
        print "\n${strName}\t" .
            $oManifestHash{name}{"${strName}"}{type} . "\t" .
            (defined($oManifestHash{name}{"${strName}"}{user}) ? $oManifestHash{name}{"${strName}"}{user} : "") . "\t" .
            (defined($oManifestHash{name}{"${strName}"}{group}) ? $oManifestHash{name}{"${strName}"}{group} : "") . "\t" .
            (defined($oManifestHash{name}{"${strName}"}{permission}) ? $oManifestHash{name}{"${strName}"}{permission} : "") . "\t" .
            (defined($oManifestHash{name}{"${strName}"}{modification_time}) ?
                $oManifestHash{name}{"${strName}"}{modification_time} : "") . "\t" .
            (defined($oManifestHash{name}{"${strName}"}{inode}) ? $oManifestHash{name}{"${strName}"}{inode} : "") . "\t" .
            (defined($oManifestHash{name}{"${strName}"}{size}) ? $oManifestHash{name}{"${strName}"}{size} : "") . "\t" .
            (defined($oManifestHash{name}{"${strName}"}{link_destination}) ?
                $oManifestHash{name}{"${strName}"}{link_destination} : "");
    }

    exit 0;
}

####################################################################################################################################
# COMPRESS Command
####################################################################################################################################
if ($strOperation eq OP_COMPRESS)
{
    my $strFile = $ARGV[1];
    
    if (!defined($strFile))
    {
        confess "file must be specified for compress operation";
    }

    $oFile->compress(PATH_ABSOLUTE, $strFile);

    exit 0;
}

confess &log(ERROR, "invalid operation ${strOperation}");
