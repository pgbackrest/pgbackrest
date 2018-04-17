####################################################################################################################################
# Auto-Generate XS and PM Files Required for Perl
####################################################################################################################################
package pgBackRestLibC::Build;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Cwd qw(abs_path);
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);
use Storable qw(dclone);

use lib dirname($0) . '/../build/lib';
use lib dirname($0) . '/../lib';

use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRestBuild::Config::Data;
use pgBackRest::Storage::Local;
use pgBackRest::Storage::Posix::Driver;
use pgBackRest::Version;

use pgBackRestBuild::Build;
use pgBackRestBuild::Build::Common;
use pgBackRestBuild::Error::Data;

use pgBackRestLibC::Config::Build;
use pgBackRestLibC::Config::BuildDefine;

####################################################################################################################################
# Perl function and constant exports
####################################################################################################################################
use constant BLD_EXPORTTYPE_SUB                                     => 'sub';
use constant BLD_EXPORTTYPE_CONSTANT                                => 'constant';

use constant LIB_AUTO_NAME                                          => 'LibCAuto';

####################################################################################################################################
# Static exports
####################################################################################################################################
my $rhExport =
{
    'checksum' =>
    {
        &BLD_EXPORTTYPE_SUB => [qw(
            pageChecksum
            pageChecksumBufferTest
            pageChecksumTest
        )],
    },

    'cipher' =>
    {
        &BLD_EXPORTTYPE_SUB => [qw(
            CIPHER_MODE_ENCRYPT
            CIPHER_MODE_DECRYPT
        )],
    },

    'config' =>
    {
        &BLD_EXPORTTYPE_SUB => [qw(
            cfgCommandName
            cfgOptionIndex
            cfgOptionIndexTotal
            cfgOptionName
        )],
    },

    'configDefine' =>
    {
        &BLD_EXPORTTYPE_SUB => [qw(
            cfgCommandId
            cfgDefOptionDefault
            cfgDefOptionPrefix
            cfgDefOptionSecure
            cfgDefOptionType
            cfgDefOptionValid
            cfgOptionId
            cfgOptionTotal
        )],
    },

    'debug' =>
    {
        &BLD_EXPORTTYPE_CONSTANT => [qw(
            UVSIZE
        )],

        &BLD_EXPORTTYPE_SUB => [qw(
            libcVersion
        )],
    },

    'encode' =>
    {
        &BLD_EXPORTTYPE_CONSTANT => [qw(
            ENCODE_TYPE_BASE64
        )],

        &BLD_EXPORTTYPE_SUB => [qw(
            decodeToBin
            encodeToStr
        )],
    },

    'lock' =>
    {
        &BLD_EXPORTTYPE_SUB => [qw(
            lockAcquire
            lockRelease
        )],
    },

    'random' =>
    {
        &BLD_EXPORTTYPE_SUB => [qw(
            randomBytes
        )],
    },

    'storage' =>
    {
        &BLD_EXPORTTYPE_SUB => [qw(
            storageDriverPosixPathRemove
        )],
    },

    'test' =>
    {
        &BLD_EXPORTTYPE_SUB => [qw(
            cfgParseTest
        )],
    },
};

####################################################################################################################################
# Execute all build functions
####################################################################################################################################
sub buildXsAll
{
    my $strBuildPath = shift;

    my $rhBuild =
    {
        'config' =>
        {
            &BLD_DATA => buildXsConfig(),
            &BLD_PATH => 'xs/config',
        },

        'configDefine' =>
        {
            &BLD_DATA => buildXsConfigDefine(),
            &BLD_PATH => 'xs/config',
        },
    };

    buildAll($strBuildPath, $rhBuild, 'xs');

    # Storage
    my $oStorage = new pgBackRest::Storage::Local(
        $strBuildPath, new pgBackRest::Storage::Posix::Driver({bFileSync => false, bPathSync => false}));

    # Get current version
    my $strVersion = BACKREST_VERSION;
    my $bDev = false;

    if ($strVersion =~ /dev$/)
    {
        $strVersion = substr($strVersion, 0, length($strVersion) - 3) . '.999';
        $bDev = true;
    }

    # Build interface file
    my $strContent =
        ('#' x 132) . "\n" .
        '# ' . bldAutoWarning('Build.pm') . "\n" .
        ('#' x 132) . "\n" .
        "package pgBackRest::LibCAuto;\n" .
        "\n" .
        "# Library version (.999 indicates development version)\n" .
        "sub libcAutoVersion\n" .
        "{\n" .
        "    return '${strVersion}';\n" .
        "}\n";

    # Generate constants for options that have a list of strings as allowed values
    my $rhConfigDefine = cfgDefine();
    my $strConstantBlock;

    foreach my $strOption (sort(keys(%{$rhConfigDefine})))
    {
        my $rhOption = $rhConfigDefine->{$strOption};

        next if $rhOption->{&CFGDEF_TYPE} ne CFGDEF_TYPE_STRING;
        next if $strOption =~ /^log-level-/;

        if (defined($rhOption->{&CFGDEF_ALLOW_LIST}))
        {
            $strConstantBlock .= defined($strConstantBlock) ? "\n" : '';

            foreach my $strValue (@{$rhOption->{&CFGDEF_ALLOW_LIST}})
            {
                my $strConstant = 'CFGOPTVAL_' . uc("${strOption}_${strValue}");
                $strConstant =~ s/\-/\_/g;

                $strConstantBlock .= "        ${strConstant}" . (' ' x (69 - length($strConstant) - 4)) . "=> '${strValue}',\n";
                push(@{$rhExport->{'config'}{&BLD_EXPORTTYPE_CONSTANT}}, $strConstant);
            }
        }

        foreach my $strCommand (sort(keys(%{$rhOption->{&CFGDEF_COMMAND}})))
        {
            my $rhCommand = $rhOption->{&CFGDEF_COMMAND}{$strCommand};

            if (defined($rhCommand->{&CFGDEF_ALLOW_LIST}))
            {
                $strConstantBlock .= defined($strConstantBlock) ? "\n" : '';

                foreach my $strValue (@{$rhCommand->{&CFGDEF_ALLOW_LIST}})
                {
                    my $strConstant = 'CFGOPTVAL_' . uc("${strCommand}_${strOption}_${strValue}");
                    $strConstant =~ s/\-/\_/g;

                    $strConstantBlock .= "        ${strConstant}" . (' ' x (69 - length($strConstant) - 4)) . "=> '${strValue}',\n";
                    push(@{$rhExport->{'config'}{&BLD_EXPORTTYPE_CONSTANT}}, $strConstant);
                }
            }
        }
    }

    $strContent .=
        "\n" .
        "# Configuration option value constants\n" .
        "sub libcAutoConstant\n" .
        "{\n" .
        "    return\n" .
        "    {\n" .
        "        " . trim($strConstantBlock) . "\n" .
        "    }\n" .
        "}\n";

    foreach my $strBuild (sort(keys(%{$rhBuild})))
    {
        foreach my $strFile (sort(keys(%{$rhBuild->{$strBuild}{&BLD_DATA}{&BLD_FILE}})))
        {
            my $rhFileConstant = $rhBuild->{$strBuild}{&BLD_DATA}{&BLD_FILE}{$strFile}{&BLD_CONSTANT_GROUP};

            foreach my $strConstantGroup (sort(keys(%{$rhFileConstant})))
            {
                my $rhConstantGroup = $rhFileConstant->{$strConstantGroup};

                foreach my $strConstant (sort(keys(%{$rhConstantGroup->{&BLD_CONSTANT}})))
                {
                    my $rhConstant = $rhConstantGroup->{&BLD_CONSTANT}{$strConstant};

                    if ($rhConstant->{&BLD_CONSTANT_EXPORT})
                    {
                        push(@{$rhExport->{$strBuild}{&BLD_EXPORTTYPE_CONSTANT}}, $strConstant);
                    }
                }
            }
        }
    }

    # Generate export tags
    my $strExportTags;

    foreach my $strSection (sort(keys(%{$rhExport})))
    {
        my $rhExportSection = $rhExport->{$strSection};

        $strExportTags .= (defined($strExportTags) ? "\n" : '') . "        ${strSection} =>\n        [\n";

        if (defined($rhExportSection->{&BLD_EXPORTTYPE_CONSTANT}) && @{$rhExportSection->{&BLD_EXPORTTYPE_CONSTANT}} > 0)
        {
            foreach my $strConstant (@{$rhExportSection->{&BLD_EXPORTTYPE_CONSTANT}})
            {
                $strExportTags .= "            '${strConstant}',\n";
            }
        }

        if (defined($rhExportSection->{&BLD_EXPORTTYPE_SUB}) && @{$rhExportSection->{&BLD_EXPORTTYPE_SUB}} > 0)
        {
            foreach my $strSub (@{$rhExportSection->{&BLD_EXPORTTYPE_SUB}})
            {
                $strExportTags .= "            '${strSub}',\n";
            }
        }

        $strExportTags .= "        ],\n";
    }

    $strContent .=
        "\n" .
        "# Export function and constants\n" .
        "sub libcAutoExportTag\n" .
        "{\n" .
        "    return\n" .
        "    {\n" .
        $strExportTags .
        "    }\n" .
        "}\n";

    # Add module end marker
    $strContent .=
        "\n" .
        "1;\n";

    # Save the file
    $oStorage->put('lib/' . BACKREST_NAME . '/' . LIB_AUTO_NAME . '.pm', $strContent);

    # Build error file
    #-------------------------------------------------------------------------------------------------------------------------------
    my $rhErrorDefine = errorDefine();

    # Order by id for the list that is id ordered
    my $rhErrorId = {};

    foreach my $strType (sort(keys(%{$rhErrorDefine})))
    {
        my $iCode = $rhErrorDefine->{$strType};
        $rhErrorId->{$iCode} = $strType;
    }

    # Output errors
    $strContent =
        ('#' x 132) . "\n" .
        "# COMMON EXCEPTION AUTO MODULE\n" .
        "# \n" .
        '# ' . bldAutoWarning('Build.pm') . "\n" .
        ('#' x 132) . "\n" .
        "package pgBackRest::Common::ExceptionAuto;\n" .
        "\n" .
        "use strict;\n" .
        "use warnings FATAL => qw(all);\n" .
        "\n" .
        "use Exporter qw(import);\n" .
        "    our \@EXPORT = qw();\n" .
        "\n" .
        ('#' x 132) . "\n" .
        "# Error Definitions\n" .
        ('#' x 132) . "\n" .
        "use constant ERROR_MINIMUM                                          => " . ERRDEF_MIN . ";\n" .
        "push \@EXPORT, qw(ERROR_MINIMUM);\n" .
        "use constant ERROR_MAXIMUM                                          => " . ERRDEF_MAX . ";\n" .
        "push \@EXPORT, qw(ERROR_MAXIMUM);\n" .
        "\n";

    foreach my $iCode (sort({sprintf("%03d", $a) cmp sprintf("%03d", $b)} keys(%{$rhErrorId})))
    {
        my $strType = "ERROR_" . uc($rhErrorId->{$iCode});
        $strType =~ s/\-/\_/g;

        $strContent .=
            "use constant ${strType}" . (' ' x (54 - length($strType))) . " => $iCode;\n" .
            "push \@EXPORT, qw(${strType});\n"
    }

    $strContent .=
        "\n" .
        "1;\n";

    $oStorage->put('../lib/' . BACKREST_NAME . '/Common/ExceptionAuto.pm', $strContent);
}

push @EXPORT, qw(buildXsAll);

1;
