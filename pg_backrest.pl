#!/usr/bin/perl

#use Getopt::Long;

#my $strCommandCompress = "pigz --rsyncable --best --stdout %file%"; # Ubuntu Linux
my $strCommandCompress = "gzip --stdout %file%"; # Ubuntu Linux
#my $strCommandHash = "sha1sum %file% | awk '{print \$1}'"; # Ubuntu Linux
my $strCommandHash = "shasum %file% | awk '{print \$1}'";

################################################################################
# EXECUTE A COMMAND
################################################################################
sub execute
{
    local($strCommand) = @_;
    my $strOutput;

#   print("$strCommand\n");
    $strOutput = qx($strCommand) or return 0;
#   print("$strOutput\n");
    
    return($strOutput);
}

################################################################################
# FILE_HASH_GET - get the sha1 hash for a file
################################################################################
sub file_hash_get
{
    local($strFile) = @_;
    
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
$strCommand = $ARGV[0];

################################################################################
# ARCHIVE-LOCAL COMMAND
################################################################################
if ($strCommand eq "archive-local")
{
    my $strSource = $ARGV[1];
    my $strDestination = $ARGV[2];
    
    # Calculate sha1 hash for the file
    my $strHash = file_hash_get($strSource);
    
    # Setup the compression string
    my $strCommand = $strCommandCompress;
    $strCommand =~ s/\%file\%/$strSource/g;
    $strCommand .= " > $strDestination-$strHash.gz";
    
    # Execute the compression
    print("$strCommand\n");
    execute($strCommand);
}
