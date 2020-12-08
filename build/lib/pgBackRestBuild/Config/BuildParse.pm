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

use pgBackRestDoc::Common::Log;
use pgBackRestDoc::Common::String;
use pgBackRestDoc::ProjectInfo;

use pgBackRestBuild::Build::Common;
use pgBackRestBuild::Config::Build;
use pgBackRestBuild::Config::Data;

####################################################################################################################################
# Constants
####################################################################################################################################
use constant BLDLCL_FILE_DEFINE                                     => 'parse';

use constant BLDLCL_DATA_OPTION                                     => '01-dataOption';
use constant BLDLCL_DATA_OPTION_RESOLVE                             => '01-dataOptionResolve';

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

                &BLDLCL_DATA_OPTION_RESOLVE =>
                {
                    &BLD_SUMMARY => 'Order for option parse resolution',
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
                my $strOptionConst;

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

                # Build option constant name if this is the current name for the option
                if ($iOptionNameIdx == 0)
                {
                    if (!$rhConfigDefine->{$strOption}{&CFGDEF_GROUP})
                    {
                        $strOptionConst = "CFGOPT_" . uc($strOptionNameOut);
                        $strOptionConst =~ s/\-/_/g;
                    }
                }
                # Else use bare string and mark as deprecated
                else
                {
                    $strOptionFlag .= ' PARSE_DEPRECATE_FLAG |';
                }

                my $strOptionVal =
                    ($rhOption->{&CFGDEF_GROUP} ? "(" . ($iOptionIdx - 1) . " << PARSE_KEY_IDX_SHIFT) | " : '') . $strOptionEnum;

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

    # Build option resolve order list.  This allows the option validation in C to take place in a single pass.
    #
    # Always process the stanza option first since confusing error message are produced if it is missing.
    #-------------------------------------------------------------------------------------------------------------------------------
    my @stryResolveOrder = (buildConfigOptionEnum(CFGOPT_STANZA));
    my $rhResolved = {&CFGOPT_STANZA => true};
    my $bAllResolved;

    do
    {
        # Assume that we will finish on this loop
        $bAllResolved = true;

        # Loop through all options
        foreach my $strOption (sort(keys(%{$rhConfigDefine})))
        {
            my $bSkip = false;

            # Check the default depend
            my $strOptionDepend =
                ref($rhConfigDefine->{$strOption}{&CFGDEF_DEPEND}) eq 'HASH' ?
                    $rhConfigDefine->{$strOption}{&CFGDEF_DEPEND}{&CFGDEF_DEPEND_OPTION} :
                    $rhConfigDefine->{$strOption}{&CFGDEF_DEPEND};

            if (defined($strOptionDepend) && !$rhResolved->{$strOptionDepend})
            {
                # &log(WARN, "$strOptionDepend is not resolved");
                $bSkip = true;
            }

            # Check the command depends
            foreach my $strCommand (sort(keys(%{$rhConfigDefine->{$strOption}{&CFGDEF_COMMAND}})))
            {
                my $strOptionDepend =
                    ref($rhConfigDefine->{$strOption}{&CFGDEF_COMMAND}{$strCommand}{&CFGDEF_DEPEND}) eq 'HASH' ?
                        $rhConfigDefine->{$strOption}{&CFGDEF_COMMAND}{$strCommand}{&CFGDEF_DEPEND}{&CFGDEF_DEPEND_OPTION} :
                        $rhConfigDefine->{$strOption}{&CFGDEF_COMMAND}{$strCommand}{&CFGDEF_DEPEND};

                if (defined($strOptionDepend) && !$rhResolved->{$strOptionDepend})
                {
                    $bSkip = true;
                }
            }

            # If dependency was not found try again on the next loop
            if ($bSkip)
            {
                $bAllResolved = false;
            }
            # Else add option to resolve order list
            elsif (!$rhResolved->{$strOption})
            {
                push(@stryResolveOrder, buildConfigOptionEnum($strOption));

                $rhResolved->{$strOption} = true;
            }
        }
    }
    while (!$bAllResolved);

    $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_DEFINE}{&BLD_DATA}{&BLDLCL_DATA_OPTION_RESOLVE}{&BLD_SOURCE} =
        "static const ConfigOption optionResolveOrder[] =\n" .
        "{\n" .
        "    " . join(",\n    ", @stryResolveOrder) . ",\n" .
        "};\n";

    return $rhBuild;
}

push @EXPORT, qw(buildConfigParse);

1;
