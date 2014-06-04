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
    OP_LIST        => "list",
    OP_EXISTS      => "exists",
    OP_HASH        => "hash",
    OP_REMOVE      => "remove",
    OP_MANIFEST    => "manifest",
    OP_COMPRESS    => "compress",
    OP_MOVE        => "move",
    OP_PATH_CREATE => "path_create"
};

####################################################################################################################################
# Command line parameters
####################################################################################################################################
my $bIgnoreMissing = false;          # Ignore errors due to missing file
my $bDestinationPathCreate = false;  # Create destination path if it does not exist
my $strExpression = undef;           # Expression to use for filtering (undef = no filtering)
my $strPermission = undef;           # Permission when creating directory or file (undef = default)
my $strSort = undef;                 # Sort order (undef = forward)

GetOptions ("ignore-missing" => \$bIgnoreMissing,
            "dest-path-create" => \$bDestinationPathCreate,
            "expression=s" => \$strExpression,
            "permission=s" => \$strPermission,
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
        confess "path must be specified for ${strOperation} operation";
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
        confess "filename must be specified for ${strOperation} operation";
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
        confess "filename must be specified for ${strOperation} operation";
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
        confess "filename must be specified for ${strOperation} operation";
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
        confess "path must be specified for ${strOperation} operation";
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
        confess "file must be specified for ${strOperation} operation";
    }

    $oFile->compress(PATH_ABSOLUTE, $strFile);

    exit 0;
}

####################################################################################################################################
# MOVE Command
####################################################################################################################################
if ($strOperation eq OP_MOVE)
{
    my $strFileSource = $ARGV[1];

    if (!defined($strFileSource))
    {
        confess "source file source must be specified for ${strOperation} operation";
    }

    my $strFileDestination = $ARGV[2];

    if (!defined($strFileDestination))
    {
        confess "destination file must be specified for ${strOperation} operation";
    }

    $oFile->move(PATH_ABSOLUTE, $strFileSource, PATH_ABSOLUTE, $strFileDestination, $bDestinationPathCreate);

    exit 0;
}

####################################################################################################################################
# PATH_CREATE Command
####################################################################################################################################
if ($strOperation eq OP_PATH_CREATE)
{
    my $strPath = $ARGV[1];

    if (!defined($strPath))
    {
        confess "path must be specified for ${strOperation} operation";
    }

    $oFile->path_create(PATH_ABSOLUTE, $strPath, $strPermission);

    exit 0;
}

confess &log(ERROR, "invalid operation ${strOperation}");
