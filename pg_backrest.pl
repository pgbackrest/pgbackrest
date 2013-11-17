#!/usr/bin/perl

#use Getopt::Long;

sub execute
{
    local($strCommand) = @_;
    my $strOutput;

    print("$strCommand\n");
    $strOutput = qx($strCommand) or return 0;
    print("$strOutput\n");
    
    return 1;
}

  