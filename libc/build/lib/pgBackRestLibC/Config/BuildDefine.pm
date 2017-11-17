####################################################################################################################################
# Auto-Generate Command and Option Configuration Definition Constants for Export to Perl
####################################################################################################################################
package pgBackRestLibC::Config::BuildDefine;

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
use pgBackRestBuild::Config::BuildDefine;

####################################################################################################################################
# Constants
####################################################################################################################################
use constant BLDLCL_FILE_DEFINE                                     => 'define';

use constant BLDLCL_CONSTANT_OPTION_TYPE                            => '01-constantOptionType';

####################################################################################################################################
# Definitions for constants and data to build
####################################################################################################################################
my $strSummary = 'Command and Option Definitions';

my $rhBuild =
{
    &BLD_FILE =>
    {
        &BLDLCL_FILE_DEFINE =>
        {
            &BLD_SUMMARY => $strSummary,

            &BLD_CONSTANT_GROUP =>
            {
                &BLDLCL_CONSTANT_OPTION_TYPE =>
                {
                    &BLD_SUMMARY => 'Option type',
                    &BLD_CONSTANT => {},
                },
            },
        },
    },
};

####################################################################################################################################
# Build configuration constants and data
####################################################################################################################################
sub buildXsConfigDefine
{
    # Build option type constants
    #-------------------------------------------------------------------------------------------------------------------------------
    my $rhConstant = $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_DEFINE}{&BLD_CONSTANT_GROUP}{&BLDLCL_CONSTANT_OPTION_TYPE}{&BLD_CONSTANT};

    foreach my $strOptionType (cfgDefineOptionTypeList())
    {
        # Build Perl constant
        my $strOptionTypeConstant = "CFGDEF_TYPE_" . uc($strOptionType);
        $strOptionTypeConstant =~ s/\-/\_/g;

        $rhConstant->{$strOptionTypeConstant}{&BLD_CONSTANT_VALUE} = buildConfigDefineOptionTypeEnum($strOptionType);
        $rhConstant->{$strOptionTypeConstant}{&BLD_CONSTANT_EXPORT} = true;
    };

    return $rhBuild;
}

push @EXPORT, qw(buildXsConfigDefine);

1;
