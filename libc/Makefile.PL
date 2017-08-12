####################################################################################################################################
# Build Makefile and Auto-Generate Files Required for Build
####################################################################################################################################
use 5.010001;
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Cwd qw(abs_path);
use File::Basename qw(dirname);

use lib dirname($0) . '/../build/lib';
use lib dirname($0) . '/../lib';

####################################################################################################################################
# Storage object to use for all file operations
####################################################################################################################################
use pgBackRest::Common::Log;
use pgBackRest::Storage::Local;
use pgBackRest::Storage::Posix::Driver;

my $strBuildPath = dirname(dirname(abs_path($0)));

my $oStorage = new pgBackRest::Storage::Local(
    $strBuildPath, new pgBackRest::Storage::Posix::Driver({bFileSync => false, bPathSync => false}));

####################################################################################################################################
# Build C files required for the library
####################################################################################################################################
use pgBackRestBuild::Build;

my $rhBuild = buildAll($strBuildPath);

####################################################################################################################################
# Perl function and constant exports
####################################################################################################################################
use constant BLD_EXPORTTYPE_SUB                                     => 'sub';
use constant BLD_EXPORTTYPE_CONSTANT                                => 'constant';

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

    'config' =>
    {
        &BLD_EXPORTTYPE_SUB => [qw(
            cfgCommandName
            cfgOptionIndexTotal
            cfgOptionName
        )],
    },

    'configRule' =>
    {
        &BLD_EXPORTTYPE_SUB => [qw(
            cfgCommandId
            cfgOptionId
            cfgOptionRuleAllowList
            cfgOptionRuleAllowListValue
            cfgOptionRuleAllowListValueTotal
            cfgOptionRuleAllowListValueValid
            cfgOptionRuleAllowRange
            cfgOptionRuleAllowRangeMax
            cfgOptionRuleAllowRangeMin
            cfgOptionRuleDefault
            cfgOptionRuleDepend
            cfgOptionRuleDependOption
            cfgOptionRuleDependValue
            cfgOptionRuleDependValueTotal
            cfgOptionRuleDependValueValid
            cfgOptionRuleHint
            cfgOptionRuleNameAlt
            cfgOptionRuleNegate
            cfgOptionRulePrefix
            cfgOptionRuleRequired
            cfgOptionRuleSection
            cfgOptionRuleSecure
            cfgOptionRuleType
            cfgOptionRuleValid
            cfgOptionTotal
            cfgOptionRuleValueHash
        )],
    },

    'debug' =>
    {
        &BLD_EXPORTTYPE_CONSTANT => [qw(
            UVSIZE
        )],

        &BLD_EXPORTTYPE_SUB => [qw(
            libCVersion
        )],
    },
};

####################################################################################################################################
# Generate LibC.pm
####################################################################################################################################
use pgBackRest::Common::String;
use pgBackRest::Version;

use pgBackRestBuild::Build::Common;
use pgBackRestBuild::Config::Data;

use constant LIB_NAME                                               => 'LibC';

# Split text into lines and indent
sub formatText
{
    my $strLine = shift;
    my $iLength = shift;
    my $iIndent = shift;

    my $strPart;
    my $strResult;
    my $bFirst = true;

    do
    {
        ($strPart, $strLine) = stringSplit($strLine, ' ', $iLength - $iIndent);

        $strResult .= ($bFirst ? '' : "\n") . (' ' x $iIndent) . trim($strPart);

        $bFirst = false;
    }
    while (defined($strLine));

    return $strResult;
}

# Build file
{
    # Get current version
    my $strVersion = BACKREST_VERSION;
    my $bDev = false;

    if ($strVersion =~ /dev$/)
    {
        $strVersion = substr($strVersion, 0, length($strVersion) - 3) . '.999';
        $bDev = true;
    }

    my $strLibC =
        'package ' . BACKREST_NAME . '::' . LIB_NAME . ";\n" .
        "\n" .
        "use 5.010001;\n" .
        "use strict;\n" .
        "use warnings;\n" .
        "use Carp;\n" .
        "\n" .
        "require Exporter;\n" .
        "use AutoLoader;\n" .
        "\n" .
        "our \@ISA = qw(Exporter);\n" .
        "\n" .
        '# Library version' . ($bDev ? " (.999 indicates development version)" : '') . "\n" .
        "our \$VERSION = '${strVersion}';\n" .
        "\n" .
        "sub libCVersion {return \$VERSION};\n";

    # Generate constants for options that have a list of strings as allowed values
    my $bFirst = true;
    my $rhOptionRule = cfgbldOptionRuleHelpGet();

    $strLibC .=
        "\n# Configuration option value constants\n" .
        "use constant\n" .
        "{\n";

    foreach my $strOption (sort(keys(%{$rhOptionRule})))
    {
        my $rhOption = $rhOptionRule->{$strOption};

        next if $rhOption->{&CFGBLDDEF_RULE_TYPE} ne CFGBLDDEF_TYPE_STRING;
        next if $strOption =~ /^log-level-/;

        if (defined($rhOption->{&CFGBLDDEF_RULE_ALLOW_LIST}))
        {
            $strLibC .= $bFirst ? '' : "\n";
            $bFirst = false;

            foreach my $strValue (@{$rhOption->{&CFGBLDDEF_RULE_ALLOW_LIST}})
            {
                my $strConstant = 'CFGOPTVAL_' . uc("${strOption}_${strValue}");
                $strConstant =~ s/\-/\_/g;

                $strLibC .= "    ${strConstant}" . (' ' x (69 - length($strConstant) - 4)) . "=> '${strValue}',\n";
                push(@{$rhExport->{'config'}{&BLD_EXPORTTYPE_CONSTANT}}, $strConstant);
            }
        }

        foreach my $strCommand (sort(keys(%{$rhOption->{&CFGBLDDEF_RULE_COMMAND}})))
        {
            my $rhCommand = $rhOption->{&CFGBLDDEF_RULE_COMMAND}{$strCommand};

            if (defined($rhCommand->{&CFGBLDDEF_RULE_ALLOW_LIST}))
            {
                $strLibC .= $bFirst ? '' : "\n";
                $bFirst = false;

                foreach my $strValue (@{$rhCommand->{&CFGBLDDEF_RULE_ALLOW_LIST}})
                {
                    my $strConstant = 'CFGOPTVAL_' . uc("${strCommand}_${strOption}_${strValue}");
                    $strConstant =~ s/\-/\_/g;

                    $strLibC .= "    ${strConstant}" . (' ' x (69 - length($strConstant) - 4)) . "=> '${strValue}',\n";
                    push(@{$rhExport->{'config'}{&BLD_EXPORTTYPE_CONSTANT}}, $strConstant);
                }
            }
        }
    }

    $strLibC .=
        "};\n";

    # Generate export sections
    $bFirst = true;

    $strLibC .=
        "\n# Export function and constants\n" .
        "our \%EXPORT_TAGS =\n" .
        "(\n";

    foreach my $strPath (sort(keys(%{$rhBuild})))
    {
        foreach my $strFile (sort(keys(%{$rhBuild->{$strPath}})))
        {
            my $rhFile = $rhBuild->{$strPath}{$strFile}{&BLD_HEADER};

            if (defined($rhFile))
            {
                foreach my $strConstant (sort(keys(%{$rhFile->{&BLD_CONSTANT}})))
                {
                    push(@{$rhExport->{$strFile}{&BLD_EXPORTTYPE_CONSTANT}}, $strConstant);
                }
            }
        }
    }

    foreach my $strSection (sort(keys(%{$rhExport})))
    {
        my $rhExportSection = $rhExport->{$strSection};

        $strLibC .= ($bFirst ? '' : "\n") . "    '${strSection}' => [qw(\n";

        if (defined($rhExportSection->{&BLD_EXPORTTYPE_CONSTANT}) && @{$rhExportSection->{&BLD_EXPORTTYPE_CONSTANT}} > 0)
        {
            $strLibC .= formatText(join(' ', sort(@{$rhExportSection->{&BLD_EXPORTTYPE_CONSTANT}})), 132, 8) . "\n";
        }

        if (defined($rhExportSection->{&BLD_EXPORTTYPE_SUB}) && @{$rhExportSection->{&BLD_EXPORTTYPE_SUB}} > 0)
        {
            $strLibC .= formatText(join(' ', sort(@{$rhExportSection->{&BLD_EXPORTTYPE_SUB}})), 132, 8) . "\n";
        }

        $strLibC .= "    )],\n";
        $bFirst = false;
    }

    $strLibC .=
        ");\n" .
        "\n" .
        "our \@EXPORT_OK = (\n";

    foreach my $strSection (sort(keys(%{$rhExport})))
    {
        $strLibC .= "    \@{\$EXPORT_TAGS{'${strSection}'}},\n";
    }

    $strLibC .=
        ");\n" .
        "\n" .
        "# Nothing is exported by default\n" .
        "our \@EXPORT = qw();\n" .
        "\n" .
        "# Autoload constants from the constant() XS function\n" .
        "sub AUTOLOAD\n" .
        "{\n" .
        "    my \$strConstantFunctionName;\n" .
        "    our \$AUTOLOAD;\n" .
        "\n" .
        "    (\$strConstantFunctionName = \$AUTOLOAD) =~ s/.*:://;\n" .
        "\n" .
        '    croak "&' . BACKREST_NAME . '::' . LIB_NAME . "::constant not defined\"\n" .
        "        if \$strConstantFunctionName eq 'constant';\n" .
        "    my (\$error, \$val) = constant(\$strConstantFunctionName);\n" .
        "    if (\$error) {croak \$error;}\n" .
        "\n" .
        "    {\n" .
        "        no strict 'refs';\n" .
        "        *\$AUTOLOAD = sub {\$val};\n" .
        "    }\n" .
        "\n" .
        "    goto &\$AUTOLOAD;\n" .
        "}\n" .
        "\n" .
        "require XSLoader;\n" .
        "XSLoader::load('" . BACKREST_NAME . '::' . LIB_NAME . "', \$VERSION);\n" .
        "\n" .
        "1;\n" .
        "__END__\n";


    my $strLibFile = 'libc/lib/' . BACKREST_NAME . '/' . LIB_NAME . '.pm';
    $oStorage->pathCreate(dirname($strLibFile), {bCreateParent => true, bIgnoreExists => true});
    $oStorage->put($strLibFile, $strLibC);
}

####################################################################################################################################
# Build list of constants to export from C and add them to the constant array
####################################################################################################################################
{
    # Build constants
    my @stryConstant;

    foreach my $strSection (sort(keys(%{$rhExport})))
    {
        foreach my $strConstant (@{$rhExport->{$strSection}{&BLD_EXPORTTYPE_CONSTANT}})
        {
            push(@stryConstant, $strConstant);
        }
    }

    # Build constant C code
    if  (eval {require ExtUtils::Constant; 1})
    {
        ExtUtils::Constant::WriteConstants
        (
            NAME            => BACKREST_NAME . '::' . LIB_NAME,
            NAMES           => \@stryConstant,
            DEFAULT_TYPE    => 'IV',
            C_FILE          => 'const-c.inc',
            XS_FILE         => 'const-xs.inc',
        );
    }
    else
    {
        die "ExtUtils::Constant is required to build constants!";
    }
}

####################################################################################################################################
# Create C Makefile
####################################################################################################################################
use ExtUtils::MakeMaker;

WriteMakefile
(
    NAME => BACKREST_NAME . '::LibC',
    VERSION_FROM => 'lib/' . BACKREST_NAME . '/LibC.pm',
    AUTHOR => 'David Steele <david@pgbackrest.org>',

    CCFLAGS => join(' ', qw(
        -o $@
        -std=c99
        -D_FILE_OFFSET_BITS=64
        $(CFLAGS)
    )),

    INC => join(' ', qw(
        -I.
        -I../src
    )),

    C => [qw(
        LibC.c
        ../src/config/config.c
        ../src/config/configRule.c
        ../src/postgres/pageChecksum.c
    )],

    OBJECT => '$(O_FILES)',
);
