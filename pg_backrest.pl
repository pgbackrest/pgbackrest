#!/usr/bin/perl -w

use strict;
#use Getopt::Long;

#my $strCommandCompress = "pigz --rsyncable --best --stdout %file%"; # Ubuntu Linux
my $strCommandCompress = "gzip --stdout %file%"; # OSX
#my $strCommandHash = "sha1sum %file% | awk '{print \$1}'"; # Ubuntu Linux
my $strCommandHash = "shasum %file% | awk '{print \$1}'"; # OSX

################################################################################
# EXECUTE A COMMAND
################################################################################
sub execute
{
    my $strCommand = shift;
    
#   print("$strCommand\n");
    my $strOutput = qx($strCommand) or return 0;
#   print("$strOutput\n");
    
    return($strOutput);
}

################################################################################
# FILE_HASH_GET - get the sha1 hash for a file
################################################################################
sub file_hash_get
{
    my $strFile = shift;
    
    my $strCommand = $strCommandHash;
    $strCommand =~ s/\%file\%/$strFile/g;
    
    my $strHash = execute($strCommand);
    $strHash =~ s/^\s+|\s+$//g;
    
    return($strHash);
}

################################################################################
# START MAIN
################################################################################
# Get the command
my $strCommand = $ARGV[0];

################################################################################
# ARCHIVE-LOCAL COMMAND
################################################################################
if ($strCommand eq "archive-local")
{
    # Get the source dir/file
    my $strSource = $ARGV[1];
    
    # !!! Make sure this is defined
    # !!! Make sure that the file exists

    # Get the destination dir/file
    my $strDestination = $ARGV[2];
    
    # !!! Make sure this is defined
    # !!! Make sure that the destination dir exists
    
    # Calculate sha1 hash for the file
    my $strHash = file_hash_get($strSource);

    # !!! Make sure the file does not already exist!
    # !!! check only the prefix - not with the checksum in case it changed "file-*.gz"
     
    # Setup the compression string
    my $strCommand = $strCommandCompress;
    $strCommand =~ s/\%file\%/$strSource/g;
    $strCommand .= " > $strDestination-$strHash.gz";
    
    # Execute the compression
    print("$strCommand\n");
    execute($strCommand);
}
