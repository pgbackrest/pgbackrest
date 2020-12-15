####################################################################################################################################
# Auto-Generate Command and Option Configuration Enums, Constants and Data
####################################################################################################################################
package pgBackRestBuild::Config::Build;

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
use pgBackRestBuild::Config::Data;

####################################################################################################################################
# Constants
####################################################################################################################################
use constant BLDLCL_FILE_CONFIG                                     => 'config';

use constant BLDLCL_CONSTANT_COMMAND                                => '01-constantCommand';
use constant BLDLCL_CONSTANT_COMMAND_TOTAL                          => 'CFG_COMMAND_TOTAL';
use constant BLDLCL_CONSTANT_OPTION_GROUP                           => '02-constantOptionGroup';
use constant BLDLCL_CONSTANT_OPTION_GROUP_TOTAL                     => 'CFG_OPTION_GROUP_TOTAL';
use constant BLDLCL_CONSTANT_OPTION                                 => '03-constantOption';
use constant BLDLCL_CONSTANT_OPTION_TOTAL                           => 'CFG_OPTION_TOTAL';

use constant BLDLCL_DATA_COMMAND_CONSTANT                           => '01-commandConstant';
use constant BLDLCL_DATA_COMMAND                                    => '02-command';
use constant BLDLCL_DATA_OPTION_GROUP                               => '03-optionGroup';
use constant BLDLCL_DATA_OPTION_CONSTANT                            => '04-optionConstant';
use constant BLDLCL_DATA_OPTION                                     => '05-option';

use constant BLDLCL_ENUM_COMMAND                                    => '01-enumCommand';
use constant BLDLCL_ENUM_OPTION_GROUP                               => '02-enumOptionGroup';
use constant BLDLCL_ENUM_OPTION                                     => '03-enumOption';

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
                },
                &BLDLCL_CONSTANT_OPTION_GROUP =>
                {
                    &BLD_SUMMARY => 'Option group',
                },
                &BLDLCL_CONSTANT_OPTION =>
                {
                    &BLD_SUMMARY => 'Option',
                },
            },

            &BLD_ENUM =>
            {
                &BLDLCL_ENUM_COMMAND =>
                {
                    &BLD_SUMMARY => 'Command',
                    &BLD_NAME => 'ConfigCommand',
                    &BLD_LIST => [],
                },

                &BLDLCL_ENUM_OPTION_GROUP =>
                {
                    &BLD_SUMMARY => 'Option group',
                    &BLD_NAME => 'ConfigOptionGroup',
                    &BLD_LIST => [],
                },

                &BLDLCL_ENUM_OPTION =>
                {
                    &BLD_SUMMARY => 'Option',
                    &BLD_NAME => 'ConfigOption',
                    &BLD_LIST => [],
                },
            },

            &BLD_DATA =>
            {
                &BLDLCL_DATA_COMMAND_CONSTANT =>
                {
                    &BLD_SUMMARY => 'Command constants',
                },

                &BLDLCL_DATA_COMMAND =>
                {
                    &BLD_SUMMARY => 'Command data',
                },

                &BLDLCL_DATA_OPTION_GROUP =>
                {
                    &BLD_SUMMARY => 'Option group data',
                },

                &BLDLCL_DATA_OPTION_CONSTANT =>
                {
                    &BLD_SUMMARY => 'Option constants',
                },

                &BLDLCL_DATA_OPTION =>
                {
                    &BLD_SUMMARY => 'Option data',
                },
            },
        },
    },
};

####################################################################################################################################
# Generate enum names
####################################################################################################################################
sub buildConfigCommandEnum
{
    return bldEnum('cfgCmd', shift)
}

push @EXPORT, qw(buildConfigCommandEnum);

sub buildConfigOptionEnum
{
    return bldEnum('cfgOpt', shift)
}

push @EXPORT, qw(buildConfigOptionEnum);

sub buildConfigOptionGroupEnum
{
    return bldEnum('cfgOptGrp', shift)
}

push @EXPORT, qw(buildConfigOptionGroupEnum);

####################################################################################################################################
# Build constants and data
####################################################################################################################################
sub buildConfig
{
    # Build command constants and data
    #-------------------------------------------------------------------------------------------------------------------------------
    my $strCommandConst;
    my $rhCommandDefine = cfgDefineCommand();
    my $rhEnum = $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_CONFIG}{&BLD_ENUM}{&BLDLCL_ENUM_COMMAND};
    my $iCommandTotal = 0;

    my $strBuildSource =
        'static ConfigCommandData configCommandData[' . BLDLCL_CONSTANT_COMMAND_TOTAL . "] = CONFIG_COMMAND_LIST\n" .
        "(";
    my $strBuildSourceConstant = '';

    foreach my $strCommand (sort(keys(%{$rhCommandDefine})))
    {
        my $rhCommand = $rhCommandDefine->{$strCommand};

        # Build command constant name
        $strCommandConst = "CFGCMD_" . uc($strCommand);
        $strCommandConst =~ s/\-/_/g;

        # Build C enum
        my $strCommandEnum = buildConfigCommandEnum($strCommand);
        push(@{$rhEnum->{&BLD_LIST}}, $strCommandEnum);

        # Build command data
        $strBuildSource .=
            "\n" .
            "    CONFIG_COMMAND\n" .
            "    (\n" .
            "        CONFIG_COMMAND_NAME(${strCommandConst})\n" .
            "\n" .
            "        CONFIG_COMMAND_LOG_FILE(" . ($rhCommand->{&CFGDEF_LOG_FILE} ? 'true' : 'false') . ")\n" .
            "        CONFIG_COMMAND_LOG_LEVEL_DEFAULT(logLevel" . ucfirst(lc($rhCommand->{&CFGDEF_LOG_LEVEL_DEFAULT})) . ")\n" .
            "        CONFIG_COMMAND_LOCK_REQUIRED(" . ($rhCommand->{&CFGDEF_LOCK_REQUIRED} ? 'true' : 'false') . ")\n" .
            "        CONFIG_COMMAND_LOCK_REMOTE_REQUIRED(" .
                ($rhCommand->{&CFGDEF_LOCK_REMOTE_REQUIRED} ? 'true' : 'false') . ")\n" .
            "        CONFIG_COMMAND_LOCK_TYPE(lockType" . ucfirst(lc($rhCommand->{&CFGDEF_LOCK_TYPE})) . ")\n" .
            "        CONFIG_COMMAND_PARAMETER_ALLOWED(" . ($rhCommand->{&CFGDEF_PARAMETER_ALLOWED} ? 'true' : 'false') . ")\n" .
            "    )\n";

        $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_CONFIG}{&BLD_CONSTANT_GROUP}{&BLDLCL_CONSTANT_COMMAND}{&BLD_CONSTANT}
            {$strCommandConst}{&BLD_CONSTANT_VALUE} = "\"${strCommand}\"\n    STRING_DECLARE(${strCommandConst}_STR);";

        $strBuildSourceConstant .=
            "STRING_EXTERN(${strCommandConst}_STR," . (' ' x (49 - length($strCommandConst))) . "${strCommandConst});\n";

        $iCommandTotal++;
    }

    # Add "none" command that is used to initialize the current command before anything is parsed
    push(@{$rhEnum->{&BLD_LIST}}, buildConfigCommandEnum('none'));
    $iCommandTotal++;

    $strBuildSource .=
        ")\n";

    $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_CONFIG}{&BLD_DATA}{&BLDLCL_DATA_COMMAND}{&BLD_SOURCE} = $strBuildSource;
    $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_CONFIG}{&BLD_DATA}{&BLDLCL_DATA_COMMAND_CONSTANT}{&BLD_SOURCE} = $strBuildSourceConstant;

    # Add an LF to the last command constant so there's whitespace before the total
    $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_CONFIG}{&BLD_CONSTANT_GROUP}{&BLDLCL_CONSTANT_COMMAND}{&BLD_CONSTANT}
        {$strCommandConst}{&BLD_CONSTANT_VALUE} .= "\n";

    # Set option total constant
    $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_CONFIG}{&BLD_CONSTANT_GROUP}{&BLDLCL_CONSTANT_COMMAND}{&BLD_CONSTANT}
        {&BLDLCL_CONSTANT_COMMAND_TOTAL}{&BLD_CONSTANT_VALUE} = $iCommandTotal;

    # Build option group constants and data
    #-------------------------------------------------------------------------------------------------------------------------------
    my $rhOptionGroupDefine = cfgDefineOptionGroup();
    $rhEnum = $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_CONFIG}{&BLD_ENUM}{&BLDLCL_ENUM_OPTION_GROUP};
    my $iGroupTotal = 0;

    $strBuildSource =
        'static ConfigOptionGroupData configOptionGroupData[' . BLDLCL_CONSTANT_OPTION_GROUP_TOTAL . "] = \n" .
        "{";

    foreach my $strGroup (sort(keys(%{$rhOptionGroupDefine})))
    {
        my $strGroupEnum = buildConfigOptionGroupEnum($strGroup);
        push(@{$rhEnum->{&BLD_LIST}}, $strGroupEnum);

        $strBuildSource .=
            "\n" .
            "    // ${strGroupEnum}\n" .
            "    //" . (qw{-} x 126) . "\n" .
            "    {\n" .
            "        .name = \"" . $strGroup . "\"\n" .
            "    },\n";

        $iGroupTotal++;
    }

    $strBuildSource .=
        "};\n";

    $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_CONFIG}{&BLD_DATA}{&BLDLCL_DATA_OPTION_GROUP}{&BLD_SOURCE} = $strBuildSource;

    # Set option total constant
    $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_CONFIG}{&BLD_CONSTANT_GROUP}{&BLDLCL_CONSTANT_OPTION_GROUP}{&BLD_CONSTANT}
        {&BLDLCL_CONSTANT_OPTION_GROUP_TOTAL}{&BLD_CONSTANT_VALUE} = $iGroupTotal;

    # Build option constants and data
    #-------------------------------------------------------------------------------------------------------------------------------
    my $strOptionConst;
    my $rhConfigDefine = cfgDefine();
    $rhEnum = $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_CONFIG}{&BLD_ENUM}{&BLDLCL_ENUM_OPTION};
    my $iOptionTotal = 0;

    $strBuildSource =
        'static ConfigOptionData configOptionData[' . BLDLCL_CONSTANT_OPTION_TOTAL . "] = CONFIG_OPTION_LIST\n" .
        "(";
    $strBuildSourceConstant = '';

    foreach my $strOption (sort(keys(%{$rhConfigDefine})))
    {
        my $iOptionIndexTotal = $rhConfigDefine->{$strOption}{&CFGDEF_INDEX_TOTAL};
        my $strOptionPrefix = $rhConfigDefine->{$strOption}{&CFGDEF_PREFIX};

        # Build C enum
        my $strOptionEnum = buildConfigOptionEnum($strOption);
        push(@{$rhEnum->{&BLD_LIST}}, $strOptionEnum);
        $rhEnum->{&BLD_VALUE}{$strOptionEnum} = $iOptionTotal;

        # Build option constant name
        $strOptionConst = "CFGOPT_" . uc($strOption);
        $strOptionConst =~ s/\-/_/g;

        # Add option data
        $strBuildSource .=
            "\n" .
            "    //" . (qw{-} x 126) . "\n" .
            "    CONFIG_OPTION\n" .
            "    (\n" .
            "        CONFIG_OPTION_NAME(\"${strOption}\")\n";

        if ($rhConfigDefine->{$strOption}{&CFGDEF_GROUP})
        {
            $strBuildSource .=
                "        CONFIG_OPTION_GROUP(true)\n" .
                "        CONFIG_OPTION_GROUP_ID(" . buildConfigOptionGroupEnum($rhConfigDefine->{$strOption}{&CFGDEF_GROUP}) .
                    ")\n";
        }

        $strBuildSource .=
            "    )\n";


        if (!$rhConfigDefine->{$strOption}{&CFGDEF_GROUP})
        {
            $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_CONFIG}{&BLD_CONSTANT_GROUP}{&BLDLCL_CONSTANT_OPTION}{&BLD_CONSTANT}
                {$strOptionConst}{&BLD_CONSTANT_VALUE} = "\"${strOption}\"\n    STRING_DECLARE(${strOptionConst}_STR);";

            $strBuildSourceConstant .=
                "STRING_EXTERN(${strOptionConst}_STR," . (' ' x (49 - length($strOptionConst))) . "${strOptionConst});\n";
        }

        $iOptionTotal += 1;
    }

    $strBuildSource .=
        ")\n";

    $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_CONFIG}{&BLD_DATA}{&BLDLCL_DATA_OPTION}{&BLD_SOURCE} = $strBuildSource;
    $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_CONFIG}{&BLD_DATA}{&BLDLCL_DATA_OPTION_CONSTANT}{&BLD_SOURCE} = $strBuildSourceConstant;

    # Add an LF to the last option constant so there's whitespace before the total
    $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_CONFIG}{&BLD_CONSTANT_GROUP}{&BLDLCL_CONSTANT_OPTION}{&BLD_CONSTANT}
        {$strOptionConst}{&BLD_CONSTANT_VALUE} .= "\n";

    # Set option total constant
    $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_CONFIG}{&BLD_CONSTANT_GROUP}{&BLDLCL_CONSTANT_OPTION}{&BLD_CONSTANT}
        {&BLDLCL_CONSTANT_OPTION_TOTAL}{&BLD_CONSTANT_VALUE} = $iOptionTotal;

    return $rhBuild;
}

push @EXPORT, qw(buildConfig);

1;
