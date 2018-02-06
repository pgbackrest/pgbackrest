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
        "{";

    foreach my $strOption (sort(keys(%{$rhConfigDefine})))
    {
        my $rhOption = $rhConfigDefine->{$strOption};
        my $strOptionEnum = buildConfigOptionEnum($strOption);
        my $strOptionArg = ($rhOption->{&CFGDEF_TYPE} ne CFGDEF_TYPE_BOOLEAN ? "        .has_arg = required_argument,\n" : '');
        my $strOptionPrefix = $rhConfigDefine->{$strOption}{&CFGDEF_PREFIX};

        my @stryOptionName = ($strOption);

        if (defined($rhOption->{&CFGDEF_NAME_ALT}))
        {
            foreach my $strOptionNameAlt (sort(keys(%{$rhOption->{&CFGDEF_NAME_ALT}})))
            {
                push(@stryOptionName, $strOptionNameAlt);
            }
        }

        $strBuildSource .=
            "\n" .
            "    // ${strOption} option" . (@stryOptionName > 1 ? ' and deprecations' : '') . "\n" .
            "    // " . (qw{-} x 125) . "\n";

        for (my $iOptionIdx = 1; $iOptionIdx <= $rhOption->{&CFGDEF_INDEX_TOTAL}; $iOptionIdx++)
        {
            for (my $iOptionNameIdx = 0; $iOptionNameIdx < @stryOptionName; $iOptionNameIdx++)
            {
                my $strOptionName = $stryOptionName[$iOptionNameIdx];
                my $rhNameAlt = $rhOption->{&CFGDEF_NAME_ALT}{$strOptionName};

                # Skip alt name if it is not valid for this option index
                if ($iOptionNameIdx > 0 && defined($rhNameAlt->{&CFGDEF_INDEX}) && $rhNameAlt->{&CFGDEF_INDEX} != $iOptionIdx)
                {
                    next;
                }

                # Generate output name
                my $strOptionNameOut = $strOptionName;

                if (defined($strOptionPrefix))
                {
                    if ($iOptionNameIdx == 0)
                    {
                        $strOptionNameOut =
                            "${strOptionPrefix}${iOptionIdx}-" . substr($strOptionName, length($strOptionPrefix) + 1);
                    }
                    else
                    {
                        $strOptionNameOut =~ s/\?/$iOptionIdx/g;
                    }
                }

                # Generate option value used for parsing (offset is added so options don't conflict with getopt_long return values)
                my $strOptionFlag = 'PARSE_OPTION_FLAG |';

                if ($iOptionNameIdx > 0)
                {
                    $strOptionFlag .= ' PARSE_DEPRECATE_FLAG |';
                }

                my $strOptionVal =
                    ($iOptionIdx > 1 ? "(" : '') . $strOptionEnum . ($iOptionIdx > 1 ? " + " . ($iOptionIdx - 1) . ')' : '');

                # Add option
                $strBuildSource .=
                    "    {\n" .
                    "        .name = \"${strOptionNameOut}\",\n" .
                    $strOptionArg .
                    "        .val = ${strOptionFlag} ${strOptionVal},\n" .
                    "    },\n";

                # Add negation when defined
                if ($rhOption->{&CFGDEF_NEGATE} &&
                    !($iOptionNameIdx > 0 && defined($rhNameAlt->{&CFGDEF_NEGATE}) && !$rhNameAlt->{&CFGDEF_NEGATE}))
                {
                    $strBuildSource .=
                        "    {\n" .
                        "        .name = \"no-${strOptionNameOut}\",\n" .
                        "        .val = ${strOptionFlag} PARSE_NEGATE_FLAG | ${strOptionVal},\n" .
                        "    },\n";
                }

                # Add reset when defined
                if ($rhOption->{&CFGDEF_RESET} &&
                    !($iOptionNameIdx > 0 && defined($rhNameAlt->{&CFGDEF_RESET}) && !$rhNameAlt->{&CFGDEF_RESET}))
                {
                    $strBuildSource .=
                        "    {\n" .
                        "        .name = \"reset-${strOptionNameOut}\",\n" .
                        "        .val = ${strOptionFlag} PARSE_RESET_FLAG | ${strOptionVal},\n" .
                        "    },\n";
                }
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
