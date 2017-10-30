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
use pgBackRestBuild::CodeGen::Common;
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
            cfgRuleOptionAllowList
            cfgRuleOptionAllowListValue
            cfgRuleOptionAllowListValueTotal
            cfgRuleOptionAllowListValueValid
            cfgRuleOptionAllowRange
            cfgRuleOptionAllowRangeMax
            cfgRuleOptionAllowRangeMin
            cfgRuleOptionDefault
            cfgRuleOptionDepend
            cfgRuleOptionDependOption
            cfgRuleOptionDependValue
            cfgRuleOptionDependValueTotal
            cfgRuleOptionDependValueValid
            cfgRuleOptionNameAlt
            cfgRuleOptionNegate
            cfgRuleOptionPrefix
            cfgRuleOptionRequired
            cfgRuleOptionSection
            cfgRuleOptionSecure
            cfgRuleOptionType
            cfgRuleOptionValid
            cfgOptionTotal
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

    'encode' =>
    {
        &BLD_EXPORTTYPE_CONSTANT => [qw(
            ENCODE_TYPE_BASE64
        )],

        &BLD_EXPORTTYPE_SUB => [qw(
            encodeToStr decodeToBin
        )],
    },
};

####################################################################################################################################
# Generate LibC.pm
####################################################################################################################################
use pgBackRest::Common::String;
use pgBackRest::Config::Data;
use pgBackRest::Version;

use pgBackRestBuild::Build::Common;

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
    my $strLibC = ${$oStorage->get('libc/' . LIB_NAME . '.template.pm')};

    # Generate auto-build warning
    my $strAutoWarning = cgenAutoWarning('Makefile.PL');
    $strLibC =~ s/\{\[LIBC\_AUTO\_WARNING\]\}/$strAutoWarning/g;

    # Get current version
    my $strVersion = BACKREST_VERSION;
    my $bDev = false;

    if ($strVersion =~ /dev$/)
    {
        $strVersion = substr($strVersion, 0, length($strVersion) - 3) . '.999';
        $bDev = true;
    }

    $strLibC =~ s/\{\[LIBC\_VERSION\]\}/$strVersion/g;

    # Generate constants for options that have a list of strings as allowed values
    my $rhOptionRule = cfgdefRule();
    my $strConstantBlock;

    foreach my $strOption (sort(keys(%{$rhOptionRule})))
    {
        my $rhOption = $rhOptionRule->{$strOption};

        next if $rhOption->{&CFGBLDDEF_RULE_TYPE} ne CFGOPTDEF_TYPE_STRING;
        next if $strOption =~ /^log-level-/;

        if (defined($rhOption->{&CFGBLDDEF_RULE_ALLOW_LIST}))
        {
            $strConstantBlock .= defined($strConstantBlock) ? "\n" : '';

            foreach my $strValue (@{$rhOption->{&CFGBLDDEF_RULE_ALLOW_LIST}})
            {
                my $strConstant = 'CFGOPTVAL_' . uc("${strOption}_${strValue}");
                $strConstant =~ s/\-/\_/g;

                $strConstantBlock .= "    ${strConstant}" . (' ' x (69 - length($strConstant) - 4)) . "=> '${strValue}',\n";
                push(@{$rhExport->{'config'}{&BLD_EXPORTTYPE_CONSTANT}}, $strConstant);
            }
        }

        foreach my $strCommand (sort(keys(%{$rhOption->{&CFGBLDDEF_RULE_COMMAND}})))
        {
            my $rhCommand = $rhOption->{&CFGBLDDEF_RULE_COMMAND}{$strCommand};

            if (defined($rhCommand->{&CFGBLDDEF_RULE_ALLOW_LIST}))
            {
                $strConstantBlock .= defined($strConstantBlock) ? "\n" : '';

                foreach my $strValue (@{$rhCommand->{&CFGBLDDEF_RULE_ALLOW_LIST}})
                {
                    my $strConstant = 'CFGOPTVAL_' . uc("${strCommand}_${strOption}_${strValue}");
                    $strConstant =~ s/\-/\_/g;

                    $strConstantBlock .= "    ${strConstant}" . (' ' x (69 - length($strConstant) - 4)) . "=> '${strValue}',\n";
                    push(@{$rhExport->{'config'}{&BLD_EXPORTTYPE_CONSTANT}}, $strConstant);
                }
            }
        }
    }

    $strConstantBlock = trim($strConstantBlock);
    $strLibC =~ s/\{\[LIBC\_CONSTANT\]\}/$strConstantBlock/g;

    # Generate export sections
    foreach my $strPath (sort(keys(%{$rhBuild})))
    {
        foreach my $strFile (sort(keys(%{$rhBuild->{$strPath}{&BLD_FILE}})))
        {
            my $rhFileConstant = $rhBuild->{$strPath}{&BLD_FILE}{$strFile}{&BLD_CONSTANT_GROUP};

            foreach my $strConstantGroup (sort(keys(%{$rhFileConstant})))
            {
                my $rhConstantGroup = $rhFileConstant->{$strConstantGroup};

                foreach my $strConstant (sort(keys(%{$rhConstantGroup->{&BLD_CONSTANT}})))
                {
                    my $rhConstant = $rhConstantGroup->{&BLD_CONSTANT}{$strConstant};

                    if ($rhConstant->{&BLD_CONSTANT_EXPORT})
                    {
                        push(@{$rhExport->{$strFile}{&BLD_EXPORTTYPE_CONSTANT}}, $strConstant);
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

        $strExportTags .= (defined($strExportTags) ? "\n" : '') . "    '${strSection}' => [qw(\n";

        if (defined($rhExportSection->{&BLD_EXPORTTYPE_CONSTANT}) && @{$rhExportSection->{&BLD_EXPORTTYPE_CONSTANT}} > 0)
        {
            $strExportTags .= formatText(join(' ', sort(@{$rhExportSection->{&BLD_EXPORTTYPE_CONSTANT}})), 132, 8) . "\n";
        }

        if (defined($rhExportSection->{&BLD_EXPORTTYPE_SUB}) && @{$rhExportSection->{&BLD_EXPORTTYPE_SUB}} > 0)
        {
            $strExportTags .= formatText(join(' ', sort(@{$rhExportSection->{&BLD_EXPORTTYPE_SUB}})), 132, 8) . "\n";
        }

        $strExportTags .= "    )],\n";
    }

    $strExportTags = trim($strExportTags);
    $strLibC =~ s/\{\[LIBC\_EXPORT\_TAGS\]\}/$strExportTags/g;

    # Generate export ok
    my $strExportOk;

    foreach my $strSection (sort(keys(%{$rhExport})))
    {
        $strExportOk .= "    \@{\$EXPORT_TAGS{'${strSection}'}},\n";
    }

    $strExportOk = trim($strExportOk);
    $strLibC =~ s/\{\[LIBC\_EXPORT\_OK\]\}/$strExportOk/g;

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

# Create C files array
my @stryCFile = qw(LibC.c);

foreach my $strFile (sort(keys(%{$oStorage->manifest('src')})))
{
    # Skip all files except .c files (including .auto.c)
    next if $strFile !~ /(?<!\.auto)\.c$/;

    push(@stryCFile, "../src/${strFile}");
}

WriteMakefile
(
    NAME => BACKREST_NAME . '::LibC',
    VERSION_FROM => 'lib/' . BACKREST_NAME . '/LibC.pm',
    AUTHOR => 'David Steele <david@pgbackrest.org>',

    CCFLAGS => join(' ', qw(
        -o $@
        -std=c99
        -D_FILE_OFFSET_BITS=64
        -funroll-loops
        -ftree-vectorize
        $(CFLAGS)
    )),

    INC => join(' ', qw(
        -I.
        -I../src
    )),

    PM => {('lib/' . BACKREST_NAME . '/' . LIB_NAME . '.pm') => ('$(INST_LIB)/' . BACKREST_NAME . '/' . LIB_NAME . '.pm')},

    C => \@stryCFile,

    OBJECT => '$(O_FILES)',
);
