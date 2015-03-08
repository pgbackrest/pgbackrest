#!/usr/bin/perl
####################################################################################################################################
# pg_backrest.pl - Simple Postgres Backup and Restore
####################################################################################################################################

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);
use Pod::Usage qw(pod2usage);
use Getopt::Long qw(GetOptions);
use XML::Checker::Parser;

use lib dirname($0) . '/../lib';
use BackRest::Utility;

####################################################################################################################################
# Usage
####################################################################################################################################

=head1 NAME

doc.pl - Generate PgBackRest documentation

=head1 SYNOPSIS

doc.pl [options] [operation]

 General Options:
   --help           display usage and exit

=cut

####################################################################################################################################
# DOC_RENDER_TAG - render a tag to another markup language
####################################################################################################################################
my $oRenderTag =
{
    'markdown' =>
    {
        'b' => ['**', '**'],
        'i' => ['_', '_'],
        'bi' => ['_**', '**_'],
        'ul' => ["\n", ''],
        'ol' => ["\n", ''],
        'li' => ['- ', "\n"],
        'id' => ['`', '`'],
        'file' => ['`', '`'],
        'path' => ['`', '`'],
        'cmd' => ['`', '`'],
        'param' => ['`', '`'],
        'setting' => ['`', '`'],
        'code' => ['`', '`'],
        'code-block' => ['```', '```'],
        'backrest' => ['PgBackRest', ''],
    },

    'html' =>
    {
        'b' => ['<b>', '</b>']
    }
};

sub doc_render_tag
{
    my $oTag = shift;
    my $strType = shift;

    my $strBuffer = "";

    my $strTag = $$oTag{name};
    my $strStart = $$oRenderTag{$strType}{$strTag}[0];
    my $strStop = $$oRenderTag{$strType}{$strTag}[1];

    if (!defined($strStart) || !defined($strStop))
    {
        confess "invalid type ${strType} or tag ${strTag}";
    }

    $strBuffer .= $strStart;

    if ($strTag eq 'li')
    {
        $strBuffer .= doc_render_text($oTag, $strType);
    }
    elsif (defined($$oTag{value}))
    {
        $strBuffer .= $$oTag{value};
    }
    elsif (defined($$oTag{children}[0]))
    {
        foreach my $oSubTag (@{doc_list($oTag)})
        {
            $strBuffer .= doc_render_tag($oSubTag, $strType);
        }

    }

    $strBuffer .= $strStop;
}

####################################################################################################################################
# DOC_RENDER_TEXT - Render a text node
####################################################################################################################################
sub doc_render_text
{
    my $oText = shift;
    my $strType = shift;

    my $strBuffer = "";

    if (defined($$oText{children}))
    {
        for (my $iIndex = 0; $iIndex < @{$$oText{children}}; $iIndex++)
        {
            if (ref(\$$oText{children}[$iIndex]) eq "SCALAR")
            {
                $strBuffer .= $$oText{children}[$iIndex];
            }
            else
            {
                $strBuffer .= doc_render_tag($$oText{children}[$iIndex], $strType);
            }
        }
    }

    return $strBuffer;
}

####################################################################################################################################
# DOC_GET - Get a node
####################################################################################################################################
sub doc_get
{
    my $oDoc = shift;
    my $strName = shift;
    my $bRequired = shift;

    my $oNode;

    for (my $iIndex = 0; $iIndex < @{$$oDoc{children}}; $iIndex++)
    {
        if ($$oDoc{children}[$iIndex]{name} eq $strName)
        {
            if (!defined($oNode))
            {
                $oNode = $$oDoc{children}[$iIndex];
            }
            else
            {
                confess "found more than one child ${strName} in node $$oDoc{name}";
            }
        }
    }

    if (!defined($oNode) && (!defined($bRequired) || $bRequired))
    {
        confess "unable to find child ${strName} in node $$oDoc{name}";
    }

    return $oNode;
}

####################################################################################################################################
# DOC_GET - Test if a node exists
####################################################################################################################################
sub doc_exists
{
    my $oDoc = shift;
    my $strName = shift;

    my $bExists = false;

    for (my $iIndex = 0; $iIndex < @{$$oDoc{children}}; $iIndex++)
    {
        if ($$oDoc{children}[$iIndex]{name} eq $strName)
        {
            return true;
        }
    }

    return false;
}

####################################################################################################################################
# DOC_LIST - Get a list of nodes
####################################################################################################################################
sub doc_list
{
    my $oDoc = shift;
    my $strName = shift;
    my $bRequired = shift;

    my @oyNode;

    for (my $iIndex = 0; $iIndex < @{$$oDoc{children}}; $iIndex++)
    {
        if (!defined($strName) || $$oDoc{children}[$iIndex]{name} eq $strName)
        {
            push(@oyNode, $$oDoc{children}[$iIndex]);
        }
    }

    if (@oyNode == 0 && (!defined($bRequired) || $bRequired))
    {
        confess "unable to find child ${strName} in node $$oDoc{name}";
    }

    return \@oyNode;
}

####################################################################################################################################
# DOC_VALUE - Get value from a node
####################################################################################################################################
sub doc_value
{
    my $oNode = shift;
    my $strDefault = shift;

    if (defined($oNode) && defined($$oNode{value}))
    {
        return $$oNode{value};
    }

    return $strDefault;
}

####################################################################################################################################
# DOC_PARSE - Parse the XML tree into something more usable
####################################################################################################################################
sub doc_parse
{
    my $strName = shift;
    my $oyNode = shift;

    my %oOut;
    my $iIndex = 0;
    my $bText = $strName eq 'text' || $strName eq 'li';

    # Store the node name
    $oOut{name} = $strName;

    if (keys($$oyNode[$iIndex]))
    {
        $oOut{param} = $$oyNode[$iIndex];
    }

    $iIndex++;

    # Look for strings and children
    while (defined($$oyNode[$iIndex]))
    {
        # Process string data
        if (ref(\$$oyNode[$iIndex]) eq 'SCALAR' && $$oyNode[$iIndex] eq '0')
        {
            $iIndex++;
            my $strBuffer = $$oyNode[$iIndex++];

            # Strip tabs, CRs, and LFs
            $strBuffer =~ s/\t|\r//g;

            # If anything is left
            if (length($strBuffer) > 0)
            {
                # If text node then create array entries for strings
                if ($bText)
                {
                    if (!defined($oOut{children}))
                    {
                        $oOut{children} = [];
                    }

                    push($oOut{children}, $strBuffer);
                }
                # Don't allow strings mixed with children
                elsif (length(trim($strBuffer)) > 0)
                {
                    if (defined($oOut{children}))
                    {
                        confess "text mixed with children in node ${strName} (spaces count)";
                    }

                    if (defined($oOut{value}))
                    {
                        confess "value is already defined in node ${strName} - this shouldn't happen";
                    }

                    # Don't allow text mixed with
                    $oOut{value} = $strBuffer;
                }
            }
        }
        # Process a child
        else
        {
            if (defined($oOut{value}) && $bText)
            {
                confess "text mixed with children in node ${strName} before child " . $$oyNode[$iIndex++] . " (spaces count)";
            }

            if (!defined($oOut{children}))
            {
                $oOut{children} = [];
            }

            push($oOut{children}, doc_parse($$oyNode[$iIndex++], $$oyNode[$iIndex++]));
        }
    }

    return \%oOut;
}

####################################################################################################################################
# DOC_SAVE - save a doc
####################################################################################################################################
sub doc_write
{
    my $strFileName = shift;
    my $strBuffer = shift;

    # Open the file
    my $hFile;
    open($hFile, '>', $strFileName)
        or confess &log(ERROR, "unable to open ${strFileName}");

    # Write the buffer
    my $iBufferOut = syswrite($hFile, $strBuffer);

    # Report any errors
    if (!defined($iBufferOut) || $iBufferOut != length($strBuffer))
    {
        confess "unable to write '${strBuffer}'" . (defined($!) ? ': ' . $! : '');
    }

    # Close the file
    close($hFile);
}

####################################################################################################################################
# Load command line parameters and config
####################################################################################################################################
my $bHelp = false;          # Display usage
my $bVersion = false;       # Display version
my $bQuiet = false;         # Sets log level to ERROR
my $strLogLevel = 'info';   # Log level for tests

GetOptions ('help' => \$bHelp,
            'version' => \$bVersion,
            'quiet' => \$bQuiet,
            'log-level=s' => \$strLogLevel)
    or pod2usage(2);

# Display version and exit if requested
if ($bHelp || $bVersion)
{
    print 'pg_backrest ' . version_get() . " documentation\n";

    if ($bHelp)
    {
        print "\n";
        pod2usage();
    }

    exit 0;
}

# Set console log level
if ($bQuiet)
{
    $strLogLevel = 'off';
}

log_level_set(undef, uc($strLogLevel));

####################################################################################################################################
# Load the doc file
####################################################################################################################################
# Initialize parser object and parse the file
my $oParser = XML::Checker::Parser->new(ErrorContext => 2, Style => 'Tree');
my $strFile = dirname($0) . '/doc.xml';
my $oTree;

eval
{
    local $XML::Checker::FAIL = sub
    {
        my $iCode = shift;

        die XML::Checker::error_string($iCode, @_);
    };

    $oTree = $oParser->parsefile(dirname($0) . '/doc.xml');
};

# Report any error that stopped parsing
if ($@)
{
    $@ =~ s/at \/.*?$//s;               # remove module line number
    die "malformed xml in '$strFile}':\n" . trim($@);
}

####################################################################################################################################
# Parse the doc
####################################################################################################################################
# my %oDocOut;

# Doc Build
#-----------------------------------------------------------------------------------------------------------------------------------
# $oDocOut{title} = $$oDocIn{param}{title};
# $oDocOut{subtitle} = $$oDocIn{param}{subtitle};
# $oDocOut{children} = [];

# my $strReadMe = "# ${strTitle} -  ${strSubTitle}";

# Intro Build
#-----------------------------------------------------------------------------------------------------------------------------------
# my $oIntroOut = {name => 'intro'};
#
# $$oIntroOut{text} = doc_get(doc_get($oDocIn, $$oIntroOut{name}), 'text');
# push($oDocOut{children}, $oIntroOut);

# Config Build
#-----------------------------------------------------------------------------------------------------------------------------------
# my $oConfigOut = {name => 'config', children => []};
# my $oConfig = doc_get($oDocIn, $$oConfigOut{name});
#
# $$oConfigOut{title} = $$oConfig{param}{title};
# $$oConfigOut{text} = doc_get($oConfig, 'text');
# push($oDocOut{children}, $oConfigOut);
#
# # Config Example List
# my $oConfigExampleOut = {name => 'config-example-list', children => []};
# my $oConfigExampleList = doc_get($oConfig, $$oConfigExampleOut{name});
#
# $$oConfigExampleOut{title} = $$oConfigExampleList{param}{title};
# $$oConfigExampleOut{text} = doc_get($oConfigExampleList, 'text', false);
# push($$oConfigExampleOut{children}, $oConfigExampleOut);

#
# #$strReadMe .= "\n\n## " . $$oConfigExampleList{param}{title};
#
# $oDocOut{config}{exampleListText} = doc_get($oConfigExampleList, 'text', false);
# my @oExampleList;
#
# # if (doc_exists($oConfigExampleList, 'text', false))
# # {
# #     $strReadMe .= "\n\n" . doc_render_text(doc_get($oConfigExampleList, 'text'), 'markdown');
# # }
#
# foreach my $oConfigExample (@{doc_list($oConfigExampleList, 'config-example')})
# {
#     my %oExampleOut;
#
#     $oExampleOut{title} = $$oConfigExample{param}{title};
#     $oExampleOut{text} = doc_get($oConfigExample, 'text', false);
#     # $strReadMe .= "\n\n### " . $$oConfigExample{param}{title} .
#     #               "\n\n" . doc_render_text(doc_get($oConfigExample, 'text'), 'markdown');
#
#     push(@oExampleList, \%oExampleOut);
# }
#
# $oDocOut{config}{exampleList} = \@oExampleList;

# # Config Section List
# my $oConfigSectionList = doc_get($oConfig, 'config-section-list');
#
# $strReadMe .= "\n\n## " . $$oConfigSectionList{param}{title};
#
# if (doc_exists($oConfigSectionList, 'text', false))
# {
#     $strReadMe .= "\n\n" . doc_render_text(doc_get($oConfigSectionList, 'text'), 'markdown');
# }
#
# foreach my $oConfigSection (@{doc_list($oConfigSectionList, 'config-section')})
# {
#     my $strConfigSectionId = $$oConfigSection{param}{id};
#
#     $strReadMe .= "\n\n#### `" . $strConfigSectionId . "` section" .
#                   "\n\n" . doc_render_text(doc_get($oConfigSection, 'text'), 'markdown');
#
#     foreach my $oConfigKey (@{doc_list($oConfigSection, 'config-key', false)})
#     {
#         my $strConfigKeyId = $$oConfigKey{param}{id};
#         my $strError = "config section ${strConfigSectionId}, key ${strConfigKeyId} requires";
#
#         my $bRequired = doc_exists($oConfigKey, 'required');
#         my $strDefault = !$bRequired ? doc_value(doc_get($oConfigKey, 'default', false)) : undef;
#         my $strAllow = doc_value(doc_get($oConfigKey, 'allow', false));
#         my $strOverride = doc_value(doc_get($oConfigKey, 'override', false));
#         my $strExample = doc_value(doc_get($oConfigKey, 'example', false));
#
#         defined($strExample) or die "${strError} example";
#
#         $strReadMe .= "\n\n##### `" . $strConfigKeyId . "` key" .
#                       "\n\n" . doc_render_text(doc_get($oConfigKey, 'text'), 'markdown') .
#                       "\n```\n" .
#                       "required: " . ($bRequired ? 'y' : 'n') . "\n" .
#                       (defined($strDefault) ? "default: ${strDefault}\n" : '') .
#                       (defined($strAllow) ? "allow: ${strAllow}\n" : '') .
#                       (defined($strOverride) ? "override: ${strOverride}\n" : '') .
#                       "example: ${strConfigKeyId}=${strExample}\n" .
#                       "```";
#     }
# }

# Release Build
#-----------------------------------------------------------------------------------------------------------------------------------
# my $oReleaseOut = {name => 'release'};
# my $oRelease = doc_get($oDocIn, 'release');
#
# $$oReleaseOut{title} = $$oRelease{param}{title};
# $$oReleaseOut{text} = doc_get($oRelease, 'text', false);
# $$oReleaseOut{children} = [];
#
# my $oReleaseList = doc_get($oRelease, 'release-list');
#
# foreach my $oReleaseVersion (@{doc_list($oReleaseList, 'release-version')})
# {
#     my %oVersionOut;
#     my @oFeatureList;
#
#     $oVersionOut{version} = $$oReleaseVersion{param}{version};
#     $oVersionOut{title} = $$oReleaseVersion{param}{title};
#     $oVersionOut{text} = doc_get($oReleaseVersion, 'text', false);
#     $oVersionOut{list} = true;
#     $oVersionOut{children} = [];
#
#     foreach my $oReleaseFeature (@{doc_list($oReleaseVersion, 'release-feature')})
#     {
#         my %oFeatureOut;
#         $oFeatureOut{text} = doc_get($oReleaseFeature, 'text');
#         push ($oVersionOut{children}, \%oFeatureOut);
#     }
#
#     push($$oReleaseOut{children}, \%oVersionOut);
# }
#
# push($oDocOut{children}, $oReleaseOut);

# Recognition Build
#-----------------------------------------------------------------------------------------------------------------------------------
# my $oRecognitionOut = {name => 'recognition'};
# my $oRecognition = doc_get($oDocIn, $$oRecognitionOut{name});
#
# $$oRecognitionOut{title} = $$oRecognition{param}{title};
# $$oRecognitionOut{text} = doc_get($oRecognition, 'text');
# push($oDocOut{children}, $oRecognitionOut);

# Build
#-----------------------------------------------------------------------------------------------------------------------------------
my $oDocIn = doc_parse(${$oTree}[0], ${$oTree}[1]);

sub doc_build
{
    my $oDoc = shift;

    # Initialize the node object
    my $oOut = {name => $$oDoc{name}, children => []};
    my $strError = "in node $$oDoc{name}";

    # Get all params
    if (defined($$oDoc{param}))
    {
        for my $strParam (keys $$oDoc{param})
        {
            $$oOut{param}{$strParam} = $$oDoc{param}{$strParam};
        }
    }

    if (defined($$oDoc{children}))
    {
        for (my $iIndex = 0; $iIndex < @{$$oDoc{children}}; $iIndex++)
        {
            my $oSub = $$oDoc{children}[$iIndex];
            my $strName = $$oSub{name};

            if ($strName eq 'text')
            {
                $$oOut{field}{text} = $oSub;
            }
            elsif (defined($$oSub{value}))
            {
                $$oOut{field}{$strName} = $$oSub{value};
            }
            elsif (!defined($$oSub{children}))
            {
                $$oOut{field}{$strName} = true;
            }
            else
            {
                push($$oOut{children}, doc_build($oSub));
            }
        }
    }

    return $oOut;
}

my $oDocOut = doc_build($oDocIn);

# Render
#-----------------------------------------------------------------------------------------------------------------------------------
sub doc_render
{
    my $oDoc = shift;
    my $strType = shift;
    my $iDepth = shift;
    my $bChildList = shift;

    my $strBuffer = "";
    my $bList = $$oDoc{name} =~ /.*-bullet-list$/;
    $bChildList = defined($bChildList) ? $bChildList : false;
    my $iChildDepth = $iDepth;

    if ($strType eq 'markdown')
    {
        if (defined($$oDoc{param}{id}))
        {
            my @stryToken = split('-', $$oDoc{name});
            my $strTitle = @stryToken == 0 ? '[unknown]' : $stryToken[@stryToken - 1];

            $strBuffer = ('#' x $iDepth) . " `$$oDoc{param}{id}` " . $strTitle;
        }

        if (defined($$oDoc{param}{title}))
        {
            $strBuffer = ('#' x $iDepth) . ' ';

            if (defined($$oDoc{param}{version}))
            {
                $strBuffer .= "v$$oDoc{param}{version}: ";
            }

            $strBuffer .= $$oDoc{param}{title};
        }

        if (defined($$oDoc{param}{subtitle}))
        {
            if (!defined($$oDoc{param}{subtitle}))
            {
                confess "subtitle not valid without title";
            }

            $strBuffer .= " - " . $$oDoc{param}{subtitle};
        }

        if ($strBuffer ne "")
        {
            $iChildDepth++;
        }

        if (defined($$oDoc{field}{text}))
        {
            if ($strBuffer ne "")
            {
                $strBuffer .= "\n\n";
            }

            if ($bChildList)
            {
                $strBuffer .= '- ';
            }

            $strBuffer .= doc_render_text($$oDoc{field}{text}, $strType);
        }

        if ($$oDoc{name} eq 'config-key')
        {
            my $strError = "config section ?, key $$oDoc{param}{id} requires";

            my $bRequired = defined($$oDoc{field}{required});
            my $strDefault = !$bRequired ? $$oDoc{field}{default} : undef;
            my $strAllow = $$oDoc{field}{allow};
            my $strOverride = $$oDoc{field}{override};
            my $strExample = $$oDoc{field}{example};

            defined($strExample) or die "${strError} example";

            $strBuffer .= "\n```\n" .
                          "required: " . ($bRequired ? 'y' : 'n') . "\n" .
                          (defined($strDefault) ? "default: ${strDefault}\n" : '') .
                          (defined($strAllow) ? "allow: ${strAllow}\n" : '') .
                          (defined($strOverride) ? "override: ${strOverride}\n" : '') .
                         "example: $$oDoc{param}{id}=${strExample}\n" .
                          "```";
        }

        if ($strBuffer ne "" && $iDepth != 1 && !$bList)
        {
            $strBuffer = "\n\n" . $strBuffer;
        }
    }
    else
    {
        confess "unknown type ${strType}";
    }

    my $bFirst = true;

    foreach my $oChild (@{$$oDoc{children}})
    {
        if ($strType eq 'markdown')
        {
        }
        else
        {
            confess "unknown type ${strType}";
        }

         $strBuffer .= doc_render($oChild, $strType, $iChildDepth, $bList);

    }

    if ($iDepth == 1)
    {
        if ($strType eq 'markdown')
        {
            $strBuffer .= "\n";
        }
        else
        {
            confess "unknown type ${strType}";
        }
    }

    return $strBuffer;
}

# Write markdown
doc_write(dirname($0) . '/../README.md', doc_render($oDocOut, 'markdown', 1));
