#!/usr/bin/perl -w

use strict;
use Cwd 'abs_path';
use File::Basename;
use Getopt::Long;

# Process flags
my $bNoCompression;
my $bNoChecksum;

GetOptions ("no-compression" => \$bNoCompression,
            "no-checksum" => \$bNoChecksum)
    or die("Error in command line arguments\n");

# Command strings - temporary until these are in the config file
#my $strCommandCompress = "pigz --rsyncable --best --stdout %file%"; # Ubuntu Linux
my $strCommandCompress = "gzip --stdout %file%"; # OSX --best was removed for testing
my $strCommandCopy = "cp %source% %destination%";
#my $strCommandHash = "sha1sum %file% | awk '{print \$1}'"; # Ubuntu Linux
my $strCommandHash = "shasum %file% | awk '{print \$1}'"; # OSX

####################################################################################################################################
# TRIM - trim whitespace off strings
####################################################################################################################################
sub trim
{
    my $strBuffer = shift;

	$strBuffer =~ s/^\s+|\s+$//g;

	return $strBuffer;
}

####################################################################################################################################
# EXECUTE - execute a command
####################################################################################################################################
sub execute
{
    my $strCommand = shift;
    
#   print("$strCommand\n");
    my $strOutput = qx($strCommand) or return 0;
#   print("$strOutput\n");
    
    return($strOutput);
}

####################################################################################################################################
# FILE_HASH_GET - get the sha1 hash for a file
####################################################################################################################################
sub file_hash_get
{
    my $strFile = shift;
    
    my $strCommand = $strCommandHash;
    $strCommand =~ s/\%file\%/$strFile/g;
    
    my $strHash = trim(execute($strCommand));
    
    return($strHash);
}

####################################################################################################################################
# START MAIN
####################################################################################################################################
# Get the command
my $strCommand = $ARGV[0];

####################################################################################################################################
# ARCHIVE-LOCAL COMMAND
####################################################################################################################################
if ($strCommand eq "archive-local")
{
    # archive-local command must have three arguments
    if (@ARGV != 3)
    {
        die "not enough arguments - show usage";
    }

    # Get the source dir/file
    my $strSourceFile = $ARGV[1];
    
    unless (-e $strSourceFile)
    {
        die "source file does not exist - show usage";
    }

    # Get the destination dir/file
    my $strDestinationFile = $ARGV[2];

    # Make sure the destination directory exists
    unless (-e dirname($strDestinationFile))
    {
        die "destination dir does not exist - show usage";
    }

    # Make sure the destination file does NOT exist - ignore checksum and extension in case they (or options) have changed
    if (glob("$strDestinationFile*"))
    {
        die "destination file already exists";
    }

    # Calculate sha1 hash for the file (unless disabled)
    if (!$bNoChecksum)
    {
        $strDestinationFile .= "-" . file_hash_get($strSourceFile);
    }
    
    # Setup the copy command
    my $strCommand = "";

    if ($bNoCompression)
    {
        $strCommand = $strCommandCopy;
        $strCommand =~ s/\%source\%/$strSourceFile/g;
        $strCommand =~ s/\%destination\%/$strDestinationFile/g;
    }
    else
    {
        $strCommand = $strCommandCompress;
        $strCommand =~ s/\%file\%/$strSourceFile/g;
        $strCommand .= " > $strDestinationFile.gz";
    }
    
    # Execute the copy
    execute($strCommand);
}
