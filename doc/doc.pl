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
use BackRest::Config;

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
        'postgres' => ['PostgreSQL', '']
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
    print 'pg_backrest ' . version_get() . " doc builder\n";

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
# Build the document from xml
####################################################################################################################################
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

####################################################################################################################################
# Build commands pulled from the code
####################################################################################################################################
# Get the option rules
my $oOptionRule = optionRuleGet();
my %oOptionFound;

sub doc_out_get
{
    my $oNode = shift;
    my $strName = shift;
    my $bRequired = shift;

    foreach my $oChild (@{$$oNode{children}})
    {
        if ($$oChild{name} eq $strName)
        {
            return $oChild;
        }
    }

    if (!defined($bRequired) || $bRequired)
    {
        confess "unable to find child node '${strName}' in node '$$oNode{name}'";
    }

    return undef;
}

sub doc_option_list_process
{
    my $oOptionListOut = shift;
    my $strOperation = shift;

    foreach my $oOptionOut (@{$$oOptionListOut{children}})
    {
        my $strOption = $$oOptionOut{param}{id};

        # if (defined($oOptionFound{$strOption}))
        # {
        #     confess "option ${strOption} has already been found";
        # }

        if ($strOption eq 'help' || $strOption eq 'version')
        {
            next;
        }

        $oOptionFound{$strOption} = true;

        if (!defined($$oOptionRule{$strOption}{&OPTION_RULE_TYPE}))
        {
            confess "unable to find option $strOption";
        }

        $$oOptionOut{field}{default} = optionDefault($strOption, $strOperation);

        if (defined($$oOptionOut{field}{default}))
        {
            $$oOptionOut{field}{required} = false;

            if ($$oOptionRule{$strOption}{&OPTION_RULE_TYPE} eq &OPTION_TYPE_BOOLEAN)
            {
                $$oOptionOut{field}{default} = $$oOptionOut{field}{default} ? 'y' : 'n';
            }
        }
        else
        {
            $$oOptionOut{field}{required} = optionRequired($strOption, $strOperation);
        }

        if (defined($strOperation))
        {
            $$oOptionOut{field}{cmd} = true;
        }

        if ($strOption eq 'cmd-remote')
        {
            $$oOptionOut{field}{default} = 'same as local';
        }

        # &log(INFO, "operation " . (defined($strOperation) ? $strOperation : '[undef]') .
        #            ", option ${strOption}, required $$oOptionOut{field}{required}" .
        #            ", default " . (defined($$oOptionOut{field}{default}) ? $$oOptionOut{field}{default} : 'undef'));
    }
}

# Ouput general options
my $oOperationGeneralOptionListOut = doc_out_get(doc_out_get(doc_out_get($oDocOut, 'operation'), 'operation-general'), 'option-list');
doc_option_list_process($oOperationGeneralOptionListOut);

# Ouput commands
my $oCommandListOut = doc_out_get(doc_out_get($oDocOut, 'operation'), 'command-list');

foreach my $oCommandOut (@{$$oCommandListOut{children}})
{
    my $strOperation = $$oCommandOut{param}{id};

    my $oOptionListOut = doc_out_get($oCommandOut, 'option-list', false);

    if (defined($oOptionListOut))
    {
        doc_option_list_process($oOptionListOut, $strOperation);
    }

    my $oExampleListOut = doc_out_get($oCommandOut, 'command-example-list');

    foreach my $oExampleOut (@{$$oExampleListOut{children}})
    {
        if (defined($$oExampleOut{param}{title}))
        {
            $$oExampleOut{param}{title} = 'Example: ' . $$oExampleOut{param}{title};
        }
        else
        {
            $$oExampleOut{param}{title} = 'Example';
        }
    }

    # $$oExampleListOut{param}{title} = 'Examples';
}

# Ouput config section
my $oConfigSectionListOut = doc_out_get(doc_out_get($oDocOut, 'config'), 'config-section-list');

foreach my $oConfigSectionOut (@{$$oConfigSectionListOut{children}})
{
    my $oOptionListOut = doc_out_get($oConfigSectionOut, 'config-key-list', false);

    if (defined($oOptionListOut))
    {
        doc_option_list_process($oOptionListOut);
    }
}

# Mark undocumented features as processed
$oOptionFound{'no-fork'} = true;
$oOptionFound{'test'} = true;
$oOptionFound{'test-delay'} = true;

# Make sure all options were processed
foreach my $strOption (sort(keys($oOptionRule)))
{
    if (!defined($oOptionFound{$strOption}))
    {
        confess "option ${strOption} was not found";
    }
}

####################################################################################################################################
# Render the document
####################################################################################################################################
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

        if ($$oDoc{name} eq 'config-key' || $$oDoc{name} eq 'option')
        {
            my $strError = "config section ?, key $$oDoc{param}{id} requires";

            my $bRequired = defined($$oDoc{field}{required}) && $$oDoc{field}{required};
            my $strDefault = $$oDoc{field}{default};
            my $strAllow = $$oDoc{field}{allow};
            my $strOverride = $$oDoc{field}{override};
            my $strExample = $$oDoc{field}{example};

            if (defined($strExample))
            {
                if (index($strExample, '=') == -1)
                {
                    $strExample = "=${strExample}";
                }
                else
                {
                    $strExample = " ${strExample}";
                }

                $strExample = "$$oDoc{param}{id}${strExample}";

                if (defined($$oDoc{field}{cmd}) && $$oDoc{field}{cmd})
                {
                    $strExample = '--' . $strExample;

                    if (index($$oDoc{field}{example}, ' ') != -1)
                    {
                        $strExample = "\"${strExample}\"";
                    }
                }
            }

            $strBuffer .= "\n```\n" .
                          "required: " . ($bRequired ? 'y' : 'n') . "\n" .
                          (defined($strDefault) ? "default: ${strDefault}\n" : '') .
                          (defined($strAllow) ? "allow: ${strAllow}\n" : '') .
                          (defined($strOverride) ? "override: ${strOverride}\n" : '') .
                          (defined($strExample) ? "example: ${strExample}\n" : '') .
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
