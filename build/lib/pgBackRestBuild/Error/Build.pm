####################################################################################################################################
# Auto-Generate Error Mappings
####################################################################################################################################
package pgBackRestBuild::Error::Build;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRestDoc::Common::Log;

use pgBackRestBuild::Build::Common;
use pgBackRestBuild::Error::Data;

####################################################################################################################################
# Constants
####################################################################################################################################
use constant BLDLCL_FILE_DEFINE                                     => 'error';

use constant BLDLCL_DATA_ERROR                                      => '01-dataError';
use constant BLDLCL_DATA_ERROR_ARRAY                                => '01-dataErrorArray';

####################################################################################################################################
# Definitions for constants and data to build
####################################################################################################################################
my $strSummary = 'Error Type Definition';

my $rhBuild =
{
    &BLD_FILE =>
    {
        &BLDLCL_FILE_DEFINE =>
        {
            &BLD_SUMMARY => $strSummary,

            &BLD_DECLARE =>
            {
                &BLDLCL_DATA_ERROR =>
                {
                    &BLD_SUMMARY => 'Error type declarations',
                },
            },

            &BLD_DATA =>
            {
                &BLDLCL_DATA_ERROR =>
                {
                    &BLD_SUMMARY => 'Error type definitions',
                },

                &BLDLCL_DATA_ERROR_ARRAY =>
                {
                    &BLD_SUMMARY => 'Error type array',
                },
            },
        },
    },
};

####################################################################################################################################
# Build configuration constants and data
####################################################################################################################################
sub buildError
{
    # Build error list
    #-------------------------------------------------------------------------------------------------------------------------------
    my $rhErrorDefine = errorDefine();

    # Order by id for the list that is id ordered
    my $rhErrorId = {};

    foreach my $strType (sort(keys(%{$rhErrorDefine})))
    {
        my $iCode = $rhErrorDefine->{$strType};

        if (defined($rhErrorId->{$iCode}))
        {
            confess &log(ERROR, "error code ${iCode} is by '" . $rhErrorId->{$iCode} . "' and '${strType}'");
        }

        $rhErrorId->{$iCode} = $strType;
    }

    # Output errors
    my $strBuildSource;

    foreach my $iCode (sort({sprintf("%03d", $a) cmp sprintf("%03d", $b)}  keys(%{$rhErrorId})))
    {
        my $strType = $rhErrorId->{$iCode};

        $strBuildSource .=
            "ERROR_DECLARE(" . bldEnum("", $strType, true) . "Error);\n";
    }

    $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_DEFINE}{&BLD_DECLARE}{&BLDLCL_DATA_ERROR}{&BLD_SOURCE} = $strBuildSource;

    # Output error definition data
    $strBuildSource = undef;

    foreach my $iCode (sort({sprintf("%03d", $a) cmp sprintf("%03d", $b)} keys(%{$rhErrorId})))
    {
        my $strType = $rhErrorId->{$iCode};

        $strBuildSource .=
            "ERROR_DEFINE(" . (' ' x (3 - length($iCode))) . "${iCode}, " . bldEnum("", $strType, true) . "Error, RuntimeError);\n";
    }

    $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_DEFINE}{&BLD_DATA}{&BLDLCL_DATA_ERROR}{&BLD_SOURCE} = $strBuildSource;

    # Output error array
    $strBuildSource =
        "static const ErrorType *errorTypeList[] =\n" .
        "{\n";

    foreach my $iCode (sort({sprintf("%03d", $a) cmp sprintf("%03d", $b)}  keys(%{$rhErrorId})))
    {
        $strBuildSource .=
            "    &" . bldEnum("", $rhErrorId->{$iCode}, true) . "Error,\n";
    }

    $strBuildSource .=
        "    NULL,\n" .
        "};";

    $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_DEFINE}{&BLD_DATA}{&BLDLCL_DATA_ERROR_ARRAY}{&BLD_SOURCE} = $strBuildSource;

    return $rhBuild;
}

push @EXPORT, qw(buildError);

1;
