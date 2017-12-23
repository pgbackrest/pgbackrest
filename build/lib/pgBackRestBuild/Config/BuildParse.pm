####################################################################################################################################
# Auto-Generate Option Definition for Parsing with getopt_long().
####################################################################################################################################
package pgBackRestBuild::Config::BuildParse;

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
use pgBackRest::Version;

use pgBackRestBuild::Build::Common;
use pgBackRestBuild::Config::Build;
use pgBackRestBuild::Config::Data;

####################################################################################################################################
# Constants
####################################################################################################################################
use constant BLDLCL_FILE_DEFINE                                     => 'parse';

use constant BLDLCL_DATA_OPTION                                     => '01-dataOption';

####################################################################################################################################
# Definitions for constants and data to build
####################################################################################################################################
my $strSummary = 'Option Parse Definition';

my $rhBuild =
{
    &BLD_FILE =>
    {
        &BLDLCL_FILE_DEFINE =>
        {
            &BLD_SUMMARY => $strSummary,

            &BLD_DATA =>
            {
                &BLDLCL_DATA_OPTION =>
                {
                    &BLD_SUMMARY => 'Option parse data',
                },
            },
        },
    },
};

####################################################################################################################################
# Build configuration constants and data
####################################################################################################################################
sub buildConfigParse
{
    # Build option parse list
    #-------------------------------------------------------------------------------------------------------------------------------
    my $rhConfigDefine = cfgDefine();

    my $strBuildSource =
        "static const struct option optionList[] =\n" .
        "{\n";

    foreach my $strOption (sort(keys(%{$rhConfigDefine})))
    {
        my $rhOption = $rhConfigDefine->{$strOption};
        my $strOptionEnum = buildConfigOptionEnum($strOption);
        my $strOptionArg = ($rhOption->{&CFGDEF_TYPE} ne CFGDEF_TYPE_BOOLEAN ? "        .has_arg = required_argument,\n" : '');
        my $strOptionPrefix = $rhConfigDefine->{$strOption}{&CFGDEF_PREFIX};

        for (my $iOptionIdx = 0; $iOptionIdx <= $rhOption->{&CFGDEF_INDEX_TOTAL}; $iOptionIdx++)
        {
            # Don't and option indexes if it is not indexeds
            next if ($iOptionIdx == 1 && $rhOption->{&CFGDEF_INDEX_TOTAL} == 1);

            # Generate option name
            my $strOptionName = $iOptionIdx > 0 ?
                "${strOptionPrefix}${iOptionIdx}-" . substr($strOption, length($strOptionPrefix) + 1) : $strOption;

            # Generate option value used for parsing (offset is added so options don't conflict with getopt_long return values)
            my $strOptionFlag = 'PARSE_OPTION_FLAG |';

            my $strOptionVal =
                ($iOptionIdx > 1 ? "(" : '') . $strOptionEnum . ($iOptionIdx > 1 ? " + " . ($iOptionIdx - 1) . ')' : '');

            # Add option
            $strBuildSource .=
                "    {\n" .
                "        .name = \"${strOptionName}\",\n" .
                $strOptionArg .
                "        .val = ${strOptionFlag} ${strOptionVal},\n" .
                "    },\n";

            # Add negation when defined
            if ($rhOption->{&CFGDEF_NEGATE})
            {
                $strBuildSource .=
                    "    {\n" .
                    "        .name = \"no-${strOptionName}\",\n" .
                    "        .val = ${strOptionFlag} PARSE_NEGATE_FLAG | ${strOptionVal},\n" .
                    "    },\n";
            }
        }
    }

    # The option list needs to be terminated or getopt_long will just keep on reading
    $strBuildSource .=
        "    // Terminate option list\n" .
        "    {\n" .
        "        .name = NULL\n" .
        "    }\n" .
        "};\n";

    $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_DEFINE}{&BLD_DATA}{&BLDLCL_DATA_OPTION}{&BLD_SOURCE} = $strBuildSource;

    return $rhBuild;
}

push @EXPORT, qw(buildConfigParse);

1;
