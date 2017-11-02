####################################################################################################################################
# Auto-Generate Command and Option Configuration Constants for Export to Perl
####################################################################################################################################
package pgBackRestLibC::Config::Build;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Cwd qw(abs_path);
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);
use Storable qw(dclone);

use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Config::Data;
use pgBackRest::Config::Define;
use pgBackRest::Version;

use pgBackRestBuild::Build::Common;
use pgBackRestBuild::Config::Build;

####################################################################################################################################
# Constants
####################################################################################################################################
use constant BLDLCL_FILE_CONFIG                                     => 'config';

use constant BLDLCL_CONSTANT_COMMAND                                => '01-command';
use constant BLDLCL_CONSTANT_OPTION                                 => '02-option';

####################################################################################################################################
# Definitions for constants and data to build
####################################################################################################################################
my $rhBuild =
{
    &BLD_FILE =>
    {
        #---------------------------------------------------------------------------------------------------------------------------
        &BLDLCL_FILE_CONFIG =>
        {
            &BLD_SUMMARY => 'Command and Option Configuration',

            &BLD_CONSTANT_GROUP =>
            {
                &BLDLCL_CONSTANT_COMMAND =>
                {
                    &BLD_SUMMARY => 'Command',
                    &BLD_CONSTANT => {},
                },

                &BLDLCL_CONSTANT_OPTION =>
                {
                    &BLD_SUMMARY => 'Option',
                    &BLD_CONSTANT => {},
                },
            },
        },
    },
};

####################################################################################################################################
# Build constants and data
####################################################################################################################################
sub buildXsConfig
{
    # Build command constants and data
    #-------------------------------------------------------------------------------------------------------------------------------
    my $rhConstant = $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_CONFIG}{&BLD_CONSTANT_GROUP}{&BLDLCL_CONSTANT_COMMAND}{&BLD_CONSTANT};

    foreach my $strCommand (cfgDefineCommandList())
    {
        # Build Perl constant
        my $strCommandEnum = buildConfigCommandEnum($strCommand);
        my $strCommandConstant = "CFGCMD_" . uc($strCommand);
        $strCommandConstant =~ s/\-/\_/g;

        $rhConstant->{$strCommandConstant}{&BLD_CONSTANT_VALUE} = $strCommandEnum;
        $rhConstant->{$strCommandConstant}{&BLD_CONSTANT_EXPORT} = true;
    }


    # Build option constants and data
    #-------------------------------------------------------------------------------------------------------------------------------
    my $rhConfigDefine = cfgDefine();
    $rhConstant = $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_CONFIG}{&BLD_CONSTANT_GROUP}{&BLDLCL_CONSTANT_OPTION}{&BLD_CONSTANT};

    foreach my $strOption (sort(keys(%{$rhConfigDefine})))
    {
        # Build Perl constant
        my $strOptionEnum = buildConfigOptionEnum($strOption);
        my $strOptionConstant = "CFGOPT_" . uc($strOption);
        $strOptionConstant =~ s/\-/\_/g;

        $rhConstant->{$strOptionConstant}{&BLD_CONSTANT_VALUE} = $strOptionEnum;
        $rhConstant->{$strOptionConstant}{&BLD_CONSTANT_EXPORT} = true;
    }

    return $rhBuild;
}

push @EXPORT, qw(buildXsConfig);

1;
