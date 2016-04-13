####################################################################################################################################
# DOC RENDER MODULE
####################################################################################################################################
package BackRestDoc::Common::DocRender;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);

use lib dirname($0) . '/../lib';
use BackRest::Common::Log;
use BackRest::Common::String;

use BackRestDoc::Common::DocManifest;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_DOC_RENDER                                          => 'DocRender';

use constant OP_DOC_RENDER_PROCESS                                  => OP_DOC_RENDER . '->process';
use constant OP_DOC_RENDER_PROCESS_TAG                              => OP_DOC_RENDER . '->processTag';
use constant OP_DOC_RENDER_PROCESS_TEXT                             => OP_DOC_RENDER . '->processText';
use constant OP_DOC_RENDER_NEW                                      => OP_DOC_RENDER . '->new';
use constant OP_DOC_RENDER_SAVE                                     => OP_DOC_RENDER . '->save';

####################################################################################################################################
# Render tags for various output types
####################################################################################################################################
my $oRenderTag =
{
    'markdown' =>
    {
        'quote' => ['"', '"'],
        'b' => ['**', '**'],
        'i' => ['_', '_'],
        # 'bi' => ['_**', '**_'],
        'ul' => ["\n", "\n"],
        'ol' => ["\n", "\n"],
        'li' => ['- ', "\n"],
        'id' => ['`', '`'],
        'file' => ['`', '`'],
        'path' => ['`', '`'],
        'cmd' => ['`', '`'],
        'param' => ['`', '`'],
        'setting' => ['`', '`'],
        'code' => ['`', '`'],
        # 'code-block' => ['```', '```'],
        # 'exe' => [undef, ''],
        'backrest' => [undef, ''],
        'postgres' => ['PostgreSQL', '']
    },

    'text' =>
    {
        'quote' => ['"', '"'],
        'b' => ['', ''],
        'i' => ['', ''],
        # 'bi' => ['', ''],
        'ul' => ["\n", "\n"],
        'ol' => ["\n", "\n"],
        'li' => ['* ', "\n"],
        'id' => ['', ''],
        'file' => ['', ''],
        'path' => ['', ''],
        'cmd' => ['', ''],
        'br-option' => ['', ''],
        'param' => ['', ''],
        'setting' => ['', ''],
        'code' => ['', ''],
        'code-block' => ['', ''],
        'exe' => [undef, ''],
        'backrest' => [undef, ''],
        'postgres' => ['PostgreSQL', '']
    },

    'latex' =>
    {
        'quote' => ['``', '"'],
        'b' => ['\textbf{', '}'],
        'i' => ['\textit{', '}'],
        # 'bi' => ['', ''],
        # 'ul' => ["\n", "\n"],
        # 'ol' => ["\n", "\n"],
        # 'li' => ['* ', "\n"],
        'id' => ['\textnormal{\texttt{', '}}'],
        'host' => ['\textnormal{\textbf{', '}}'],
        'file' => ['\textnormal{\texttt{', '}}'],
        'path' => ['\textnormal{\texttt{', '}}'],
        'cmd' => ['\textnormal{\texttt{', "}}"],
        'user' => ['\textnormal{\texttt{', '}}'],
        'br-option' => ['', ''],
        # 'param' => ['\texttt{', '}'],
        # 'setting' => ['\texttt{', '}'],
        'br-option' => ['\textnormal{\texttt{', '}}'],
        'br-setting' => ['\textnormal{\texttt{', '}}'],
        'pg-option' => ['\textnormal{\texttt{', '}}'],
        'pg-setting' => ['\textnormal{\texttt{', '}}'],
        'code' => ['\textnormal{\texttt{', '}}'],
        # 'code' => ['\texttt{', '}'],
        # 'code-block' => ['', ''],
        # 'exe' => [undef, ''],
        'backrest' => [undef, ''],
        'postgres' => ['PostgreSQL', '']
    },

    'html' =>
    {
        'quote' => ['<q>', '</q>'],
        'b' => ['<b>', '</b>'],
        'i' => ['<i>', '</i>'],
        # 'bi' => ['<i><b>', '</b></i>'],
        'ul' => ['<ul>', '</ul>'],
        'ol' => ['<ol>', '</ol>'],
        'li' => ['<li>', '</li>'],
        'id' => ['<span class="id">', '</span>'],
        'host' => ['<span class="host">', '</span>'],
        'file' => ['<span class="file">', '</span>'],
        'path' => ['<span class="path">', '</span>'],
        'cmd' => ['<span class="cmd">', '</span>'],
        'user' => ['<span class="user">', '</span>'],
        'br-option' => ['<span class="br-option">', '</span>'],
        'br-setting' => ['<span class="br-setting">', '</span>'],
        'pg-option' => ['<span class="pg-option">', '</span>'],
        'pg-setting' => ['<span class="pg-setting">', '</span>'],
        'code' => ['<span class="id">', '</span>'],
        'code-block' => ['<code-block>', '</code-block>'],
        'exe' => [undef, ''],
        'setting' => ['<span class="br-setting">', '</span>'], # ??? This will need to be fixed
        'backrest' => [undef, ''],
        'postgres' => ['<span class="postgres">PostgreSQL</span>', '']
    }
};

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;       # Class name

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Assign function parameters, defaults, and log debug info
    (
        my $strOperation,
        $self->{strType},
        $self->{oManifest},
        $self->{strRenderOutKey},
    ) =
        logDebugParam
        (
            OP_DOC_RENDER_NEW, \@_,
            {name => 'strType'},
            {name => 'oManifest'},
            {name => 'strRenderOutKey', required => false}
        );

    # Initialize project tags
    $$oRenderTag{markdown}{backrest}[0] = "{[project]}";
    $$oRenderTag{markdown}{exe}[0] = "{[project-exe]}";

    $$oRenderTag{text}{backrest}[0] = "{[project]}";
    $$oRenderTag{text}{exe}[0] = "{[project-exe]}";

    $$oRenderTag{latex}{backrest}[0] = "{[project]}";
    $$oRenderTag{latex}{exe}[0] = "\\textnormal\{\\texttt\{[project-exe]}}\}\}";

    $$oRenderTag{html}{backrest}[0] = "<span class=\"backrest\">{[project]}</span>";
    $$oRenderTag{html}{exe}[0] = "<span class=\"file\">{[project-exe]}</span>";

    if (defined($self->{strRenderOutKey}))
    {
        # Copy page data to self
        my $oRenderOut = $self->{oManifest}->renderOutGet($self->{strType} eq 'latex' ? 'pdf' : $self->{strType}, $self->{strRenderOutKey});

        # Get the reference if this is the backrest project
        if ($self->{oManifest}->isBackRest())
        {
            $self->{oReference} = new BackRestDoc::Common::DocConfig(${$self->{oManifest}->sourceGet('reference')}{doc}, $self);
        }

        if (defined($$oRenderOut{source}) && $$oRenderOut{source} eq 'reference')
        {
            if ($self->{strRenderOutKey} eq 'configuration')
            {
                $self->{oDoc} = $self->{oReference}->helpConfigDocGet();
            }
            elsif ($self->{strRenderOutKey} eq 'command')
            {
                $self->{oDoc} = $self->{oReference}->helpCommandDocGet();
            }
            else
            {
                confess &log(ERROR, "cannot render $self->{strRenderOutKey} from source $$oRenderOut{source}");
            }
        }
        else
        {
            $self->{oDoc} = ${$self->{oManifest}->sourceGet($self->{strRenderOutKey})}{doc};
        }
    }

    if (defined($self->{strRenderOutKey}))
    {
        # Build the doc
        $self->build($self->{oDoc});

        # Get required sections
        foreach my $strPath (@{$self->{oManifest}->{stryRequire}})
        {
            if (defined(${$self->{oSection}}{$strPath}))
            {
                $self->required($strPath);
            }
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# variableReplace
#
# Replace variables in the string.
####################################################################################################################################
sub variableReplace
{
    my $self = shift;

    return $self->{oManifest}->variableReplace(shift, $self->{strType});
}

####################################################################################################################################
# variableSet
#
# Set a variable to be replaced later.
####################################################################################################################################
sub variableSet
{
    my $self = shift;

    return $self->{oManifest}->variableSet(shift, shift);
}

####################################################################################################################################
# variableGet
#
# Get the current value of a variable.
####################################################################################################################################
sub variableGet
{
    my $self = shift;

    return $self->{oManifest}->variableGet(shift);
}

####################################################################################################################################
# build
#
# Build the section map and perform keyword matching.
####################################################################################################################################
sub build
{
    my $self = shift;
    my $oNode = shift;
    my $oParent = shift;
    my $strPath = shift;

    # &log(INFO, "        node " . $oNode->nameGet());

    my $strName = $oNode->nameGet();

    if (defined($oParent))
    {
        if (!$self->{oManifest}->keywordMatch($oNode->paramGet('keyword', false)))
        {
            my $strDescription;

            if (defined($oNode->nodeGet('title', false)))
            {
                $strDescription = $self->processText($oNode->nodeGet('title')->textGet());
            }

            &log(DEBUG, "            filtered ${strName}" . (defined($strDescription) ? ": ${strDescription}" : ''));

            $oParent->nodeRemove($oNode);
        }
    }
    else
    {
            &log(DEBUG, '        build document');
            $self->{oSection} = {};
    }

    if ($strName eq 'section')
    {
        if (defined($strPath))
        {
            $oNode->paramSet('path-parent', $strPath);
        }

        $strPath .= '/' . $oNode->paramGet('id');

        &log(DEBUG, "            path ${strPath}");
        ${$self->{oSection}}{$strPath} = $oNode;
        $oNode->paramSet('path', $strPath);
    }

    # Iterate all nodes
    foreach my $oChild ($oNode->nodeList(undef, false))
    {
        $self->build($oChild, $oNode, $strPath);
    }
}

####################################################################################################################################
# required
#
# Build a list of required sections
####################################################################################################################################
sub required
{
    my $self = shift;
    my $strPath = shift;
    my $bDepend = shift;

    # If node is not found that means the path is invalid
    my $oNode = ${$self->{oSection}}{$strPath};

    if (!defined($oNode))
    {
        confess &log(ERROR, "invalid path ${strPath}");
    }

    # Only add sections that are listed dependencies
    if (!defined($bDepend) || $bDepend)
    {
        # Match section and all child sections
        foreach my $strChildPath (sort(keys($self->{oSection})))
        {
            if ($strChildPath =~ /^$strPath$/ || $strChildPath =~ /^$strPath\/.*$/)
            {
                &log(INFO, "        require section: ${strChildPath}");

                ${$self->{oSectionRequired}}{$strChildPath} = true;
            }
        }
    }

    # Get the path of the current section's parent
    my $strParentPath = $oNode->paramGet('path-parent', false);

    if ($oNode->paramTest('depend'))
    {
        foreach my $strDepend (split(',', $oNode->paramGet('depend')))
        {
            if ($strDepend !~ /^\//)
            {
                if (!defined($strParentPath))
                {
                    $strDepend = "/${strDepend}";
                }
                else
                {
                    $strDepend = "${strParentPath}/${strDepend}";
                }
            }

            $self->required($strDepend, true);
        }
    }
    elsif (defined($strParentPath))
    {
        $self->required($strParentPath, false);
    }
}

####################################################################################################################################
# isRequired
#
# Is it required to execute the section statements?
####################################################################################################################################
sub isRequired
{
    my $self = shift;
    my $oSection = shift;

    if (!defined($self->{oSectionRequired}))
    {
        return true;
    }

    my $strPath = $oSection->paramGet('path');

    defined(${$self->{oSectionRequired}}{$strPath}) ? true : false;
}

####################################################################################################################################
# process
#
# Render the document
####################################################################################################################################
sub process
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oDoc,
        $iDepth,
        $bChildList,
    ) =
        logDebugParam
        (
            OP_DOC_RENDER_PROCESS, \@_,
            {name => 'oDoc', trace => true},
            {name => 'iDepth', default => 1, trace => true},
            {name => 'bChildList', default => true, trace => true}
        );

    my $strType = $self->{strType};

    my $strBuffer = "";
    my $bList = $oDoc->nameGet() =~ /.*-bullet-list$/;
    $bChildList = defined($bChildList) ? $bChildList : false;
    my $iChildDepth = $iDepth;

    my @stryMonth = ('January', 'February', 'March', 'April', 'May', 'June',
                     'July', 'August', 'September', 'October', 'November', 'December');

    if ($strType eq 'markdown' || $strType eq 'text')
    {
        if (defined($oDoc->paramGet('id', false)))
        {
            my @stryToken = split('-', $oDoc->nameGet());
            my $strTitle = @stryToken == 0 ? '[unknown]' : $stryToken[@stryToken - 1];

            $strBuffer = ('#' x $iDepth) . ' `' . $oDoc->paramGet('id') . '` ' . $strTitle;
        }

        # Try to get the title param from the element (??? this is the old style and should be removed)
        my $strTitle = $oDoc->paramGet('title', false);

        if (!defined($strTitle))
        {
            $strTitle = $oDoc->paramGet('subtitle', false);
        }

        # If not found then get the title element
        if (!defined($strTitle) && defined($oDoc->nodeGet('title', false)))
        {
            $strTitle = $self->processText($oDoc->nodeGet('title')->textGet());
        }

        if (defined($strTitle))
        {
            $strBuffer = ('#' x $iDepth) . ' ';

            if (defined($oDoc->paramGet('version', false)))
            {
                $strBuffer .= 'v' . $oDoc->paramGet('version') . ': ';
            }

            $strBuffer .= ($iDepth == 1 ? "{[project]}<br/>" : '') . $strTitle;

            if (defined($oDoc->paramGet('date', false)))
            {
                my $strDate = $oDoc->paramGet('date');

                if ($strDate !~ /^(XXXX-XX-XX)|([0-9]{4}-[0-9]{2}-[0-9]{2})$/)
                {
                    confess "invalid date ${strDate}";
                }

                if ($strDate =~ /^X/)
                {
                    $strBuffer .= "\n__No Release Date Set__";
                }
                else
                {
                    $strBuffer .= "\n__Released " . $stryMonth[(substr($strDate, 5, 2) - 1)] . ' ' .
                                  (substr($strDate, 8, 2) + 0) . ', ' . substr($strDate, 0, 4) . '__';
                }
            }
        }

        if ($strBuffer ne "")
        {
            $iChildDepth++;
        }

        if (defined($oDoc->nodeGet('summary', false)))
        {
            if ($strBuffer ne "")
            {
                $strBuffer .= "\n\n";
            }

            if ($bChildList)
            {
                $strBuffer .= '* ';
            }

            $strBuffer .= $self->processText($oDoc->nodeGet('summary')->textGet());
        }

        if (defined($oDoc->textGet(false)))
        {
            if ($strBuffer ne "")
            {
                $strBuffer .= "\n\n";
            }

            if ($bChildList)
            {
                $strBuffer .= '* ';
            }

            $strBuffer .= $self->processText($oDoc->textGet());
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

    foreach my $oChild ($oDoc->nodeList(undef, false))
    {
        if ($oChild->nameGet() ne 'summary' && $oChild->nameGet() ne 'title')
        {
            if ($strType eq 'markdown' || $strType eq 'text')
            {
            }
            else
            {
                confess "unknown type ${strType}";
            }

             $strBuffer .= $self->process($oChild, $iChildDepth, $bList);
        }
    }

    if ($iDepth == 1)
    {
        if ($strType eq 'markdown' || $strType eq 'text')
        {
            $strBuffer .= "\n";
        }
        else
        {
            confess "unknown type ${strType}";
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strBuffer', value => $strBuffer, trace => true}
    );
}

####################################################################################################################################
# processTag
####################################################################################################################################
sub processTag
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oTag
    ) =
        logDebugParam
        (
            OP_DOC_RENDER_PROCESS_TAG, \@_,
            {name => 'oTag', trace => true}
        );

    my $strBuffer = "";

    my $strType = $self->{strType};
    my $strTag = $oTag->nameGet();

    if (!defined($strTag))
    {
        use Data::Dumper;
        confess Dumper($oTag);
    }

    if ($strTag eq 'link')
    {
        my $strUrl = $oTag->paramGet('url', false);

        if ($strType eq 'markdown')
        {
            if (!defined($strUrl))
            {
                $strUrl = '{[backrest-url-base]}/' . $oTag->paramGet('page');
            }

            $strBuffer = '[' . $oTag->valueGet() . '](' . $strUrl . ')';
        }
        elsif ($strType eq 'html')
        {
            if (!defined($strUrl))
            {
                $strUrl = $oTag->paramGet('page', false);

                if (!defined($strUrl))
                {
                    $strUrl = '#' . substr($oTag->paramGet('section'), 1);
                }
            }

            $strBuffer = '<a href="' . $strUrl . '">' . $oTag->valueGet() . '</a>';
        }
        elsif ($strType eq 'latex')
        {
            if (!defined($strUrl))
            {
                $strUrl = $oTag->paramGet('page', false);

                if (!defined($strUrl))
                {
                    $strUrl = $oTag->paramGet('section');
                }

                $strBuffer = "\\hyperref[$strUrl]{" . $oTag->valueGet() . "}";
            }
            else
            {
                $strBuffer = "\\href{$strUrl}{" . $oTag->valueGet() . "}";
            }
        }
        else
        {
            confess "tag link not valid for type ${strType}";
        }
    }
    else
    {
        my $strStart = $$oRenderTag{$strType}{$strTag}[0];
        my $strStop = $$oRenderTag{$strType}{$strTag}[1];

        if (!defined($strStart) || !defined($strStop))
        {
            confess "invalid type ${strType} or tag ${strTag}";
        }

        $strBuffer .= $strStart;

        if ($strTag eq 'p' || $strTag eq 'title' || $strTag eq 'li' || $strTag eq 'code-block' || $strTag eq 'summary')
        {
            $strBuffer .= $self->processText($oTag);
        }
        elsif (defined($oTag->valueGet()))
        {
            $strBuffer .= $oTag->valueGet();
        }
        else
        {
            foreach my $oSubTag ($oTag->nodeList(undef, false))
            {
                $strBuffer .= $self->processTag($oSubTag);
            }
        }

        $strBuffer .= $strStop;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strBuffer', value => $strBuffer, trace => true}
    );
}

####################################################################################################################################
# processText
####################################################################################################################################
sub processText
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oText
    ) =
        logDebugParam
        (
            OP_DOC_RENDER_PROCESS_TEXT, \@_,
            {name => 'oText', trace => true}
        );

    my $strType = $self->{strType};
    my $strBuffer = '';

    foreach my $oNode ($oText->nodeList(undef, false))
    {
        if (ref(\$oNode) eq "SCALAR")
        {
            if ($oNode =~ /\"/)
            {
                confess &log(ERROR, "unable to process quotes in string (use <quote> instead):\n${oNode}");
            }

            $strBuffer .= $oNode;
        }
        else
        {
            $strBuffer .= $self->processTag($oNode);
        }
    }
    #
    # if ($strType eq 'html')
    # {
    #         # $strBuffer =~ s/^\s+|\s+$//g;
    #
    #     $strBuffer =~ s/\n/\<br\/\>\n/g;
    # }

    # if ($strType eq 'markdown')
    # {
            # $strBuffer =~ s/^\s+|\s+$//g;

        $strBuffer =~ s/ +/ /g;
        $strBuffer =~ s/^ //smg;
    # }

    if ($strType eq 'latex')
    {
        $strBuffer =~ s/\&mdash\;/---/g;
        $strBuffer =~ s/\&lt\;/\</g;
        $strBuffer =~ s/\<\=/\$\\leq\$/g;
        $strBuffer =~ s/\>\=/\$\\geq\$/g;
        # $strBuffer =~ s/\_/\\_/g;
    }

    $strBuffer = $self->variableReplace($strBuffer);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strBuffer', value => $strBuffer, trace => true}
    );
}

####################################################################################################################################
# save
####################################################################################################################################
sub save
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFileName,
        $strBuffer
    ) =
        logDebugParam
        (
            OP_DOC_RENDER_SAVE, \@_,
            {name => 'strFileName', trace => true},
            {name => 'strBuffer', trace => true}
        );

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

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

1;
