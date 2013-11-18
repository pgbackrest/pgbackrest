#!/usr/bin/perl

#use Getopt::Long;

my $strCommandCompress = "pigz --rsyncable --best --stdout %file%";
my $strCommandHash = "sha1sum %file% | awk '{print \$1}'";

sub execute
{
    local($strCommand) = @_;
    my $strOutput;

#   print("$strCommand\n");
    $strOutput = qx($strCommand) or return 0;
#   print("$strOutput\n");
    
    return($strOutput);
}

sub file_hash_get
{
    local($strFile) = @_;
    
    my $strCommand = $strCommandHash;
    $strCommand =~ s/\%file\%/$strFile/g;
    
    my $strHash = execute($strCommand);
    $strHash =~ s/^\s+|\s+$//g;
    
    return($strHash);
}

# Get the command
$strCommand = $ARGV[0];

if ($strCommand eq "archive-local")
{
    my $strSource = $ARGV[1];
    my $strDestination = $ARGV[2];
    
    my $strCommand = $strCommandCompress;
    $strCommand =~ s/\%file\%/$strFile/g;
    
    my $strHash = file_hash_get($strSource);
    
    $strCommand !!! construct the rest of the string
    
    print("$strHash");
#    execute("$strCommandCompress $strSource > 
}

  