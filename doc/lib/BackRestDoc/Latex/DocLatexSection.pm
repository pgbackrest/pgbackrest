####################################################################################################################################
# DOC LATEX SECTION MODULE
####################################################################################################################################
package BackRestDoc::Latex::DocLatexSection;
use parent 'BackRestDoc::Common::DocExecute';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();

use BackRestDoc::Common::DocConfig;
use BackRestDoc::Common::DocManifest;
use BackRestDoc::Common::Log;
use BackRestDoc::Common::String;

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;       # Class name

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oManifest,
        $strRenderOutKey,
        $bExe
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'oManifest'},
            {name => 'strRenderOutKey'},
            {name => 'bExe'}
        );

    # Create the class hash
    my $self = $class->SUPER::new('latex', $oManifest, $strRenderOutKey, $bExe);
    bless $self, $class;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# process
#
# Generate the site html
####################################################################################################################################
sub process
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my $strOperation = logDebugParam(__PACKAGE__ . '->process');

    # Working variables
    my $oPage = $self->{oDoc};
    my $strLatex;

    # Initialize page
    my $strTitle = $oPage->paramGet('title');
    my $strSubTitle = $oPage->paramGet('subtitle', false);

    # Render sections
    foreach my $oSection ($oPage->nodeList('section'))
    {
        $strLatex .= (defined($strLatex) ? "\n" : '') . $self->sectionProcess($oSection, undef, 1);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strHtml', value => $strLatex, trace => true}
    );
}

####################################################################################################################################
# sectionProcess
####################################################################################################################################
sub sectionProcess
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oSection,
        $strSection,
        $iDepth
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->sectionRender', \@_,
            {name => 'oSection'},
            {name => 'strSection', required => false},
            {name => 'iDepth'}
        );

    if ($oSection->paramGet('log'))
    {
        &log(INFO, ('    ' x ($iDepth + 1)) . 'process section: ' . $oSection->paramGet('path'));
    }

    # Create section type
    my $strSectionTitle = $self->processText($oSection->nodeGet('title')->textGet());
    $strSection .= (defined($strSection) ? ', ' : '') . "'${strSectionTitle}' " . ('Sub' x ($iDepth - 1)) . "Section";

    # Create section comment
    my $strLatex =
        "% ${strSection}\n% " . ('-' x 130) . "\n";

    # Exclude from table of contents if requested
    if ($iDepth <= 3 && $oSection->paramTest('toc', 'n'))
    {
        $strLatex .= '\\addtocontents{toc}{\\protect\\setcounter{tocdepth}{' . ($iDepth - 1) . "}}\n";
    }

    # Create section name
    $strLatex .= '\\';

    if ($iDepth <= 3)
    {
        $strLatex .= ($iDepth > 1 ? ('sub' x ($iDepth - 1)) : '') . "section";
    }
    elsif ($iDepth == 4)
    {
        $strLatex .= 'paragraph';
    }
    else
    {
        confess &log(ASSERT, "section depth of ${iDepth} exceeds maximum");
    }

    $strLatex .= "\{${strSectionTitle}\}\\label{" . $oSection->paramGet('path', false) . "}\n";

    # Reset table of contents numbering if the section was excluded
    if ($iDepth <= 3 && $oSection->paramTest('toc', 'n'))
    {
        $strLatex .= '\\addtocontents{toc}{\\protect\\setcounter{tocdepth}{' . $iDepth . "}}\n";
    }

    foreach my $oChild ($oSection->nodeList())
    {
        &log(DEBUG, ('    ' x ($iDepth + 2)) . 'process child ' . $oChild->nameGet());

        # Execute a command
        if ($oChild->nameGet() eq 'execute-list')
        {
            my $bShow = $oChild->paramTest('show', 'n') ? false : true;
            my $strHostName = $self->{oManifest}->variableReplace($oChild->paramGet('host'));

            if ($bShow)
            {
                $strLatex .=
                    "\n\\begin\{lstlisting\}[title=\{\\textnormal{\\textbf\{${strHostName}}} --- " .
                    $self->processText($oChild->nodeGet('title')->textGet()) . "}]\n";
            }

            foreach my $oExecute ($oChild->nodeList('execute'))
            {
                my $bExeShow = !$oExecute->paramTest('show', 'n');
                my ($strCommand, $strOutput) = $self->execute(
                    $oSection, $self->{oManifest}->variableReplace($oChild->paramGet('host')), $oExecute,
                    {iIndent => $iDepth + 3, bShow => $bShow && $bExeShow});

                if ($bShow && $bExeShow)
                {
                    $strLatex .= "${strCommand}\n";

                    if (defined($strOutput))
                    {
                        $strLatex .= "\nOutput:\n\n${strOutput}\n";
                    }
                }
            }

            if ($bShow)
            {
                $strLatex .=
                    "\\end{lstlisting}\n";
            }
        }
        # Add code block
        elsif ($oChild->nameGet() eq 'code-block')
        {
            my $strTitle = $self->{oManifest}->variableReplace($oChild->paramGet("title", false), 'latex');

            if (defined($strTitle) && $strTitle eq '')
            {
                undef($strTitle)
            }

            # Begin the code listing
            if (!defined($strTitle))
            {
                $strLatex .=
                    "\\vspace{.75em}\n";
            }

            $strLatex .=
                "\\begin\{lstlisting\}";

            # Add the title if one is provided
            if (defined($strTitle))
            {
                $strLatex .= "[title=\{${strTitle}:\}]";
            }

            # End the code listing
            $strLatex .=
                "\n" .
                trim($oChild->valueGet()) . "\n" .
                "\\end{lstlisting}\n";
        }
        # Add table
        elsif ($oChild->nameGet() eq 'table')
        {
            my $oHeader;
            my @oyColumn;

            if ($oChild->nodeTest('table-header'))
            {
                $oHeader = $oChild->nodeGet('table-header');
                @oyColumn = $oHeader->nodeList('table-column');
            }

            my $strWidth =
                '{' . (defined($oHeader) && $oHeader->paramTest('width') ? ($oHeader->paramGet('width') / 100) .
                '\textwidth' : '\textwidth') . '}';

            # Build the table
            $strLatex .= "\\vspace{1em}\\newline\n\\begin{table}\n\\begin{tabularx}${strWidth}{|";

            # Build the table header
            foreach my $oColumn (@oyColumn)
            {
                my $strAlignCode;
                my $strAlign = $oColumn->paramGet("align", false);

                # If fill is specified then use X or the custom designed alignments in the preamble to fill and justify the columns.
                if ($oColumn->paramTest('fill') && $oColumn->paramGet('fill', false) eq 'y')
                {
                    if (!defined($strAlign) || $strAlign eq 'left')
                    {
                        $strAlignCode = 'X';
                    }
                    elsif ($strAlign eq 'right')
                    {
                        $strAlignCode = 'R';
                    }
                    elsif ($strAlign eq 'center')
                    {
                        $strAlignCode = 'C';
                    }
                    else
                    {
                        confess &log(ERROR, "align '${strAlign}' not valid when fill=y");
                    }
                }
                else
                {
                    if (!defined($strAlign) || $strAlign eq 'left')
                    {
                        $strAlignCode = 'l';
                    }
                    elsif ($strAlign eq 'center')
                    {
                        $strAlignCode = 'c';
                    }
                    elsif ($strAlign eq 'right')
                    {
                        $strAlignCode = 'r';
                    }
                    else
                    {
                        confess &log(ERROR, "align '${strAlign}' not valid");
                    }
                }

                # $strLatex .= 'p{' . $oColumn->paramGet("width") . '} | ';
                $strLatex .= $strAlignCode . ' | ';
            }

            # If table-header not provided then default the column alignment and fill by using the number of columns in the 1st row
            if (!defined($oHeader))
            {
                my @oyRow = $oChild->nodeGet('table-data')->nodeList('table-row');
                foreach my $oRowCell ($oyRow[0]->nodeList('table-cell'))
                {
                    $strLatex .= 'X|';
                }
            }

            $strLatex .= "}\n";

            my $strLine;

            if (defined($oHeader))
            {
                $strLatex .= "\\hline";
                $strLatex .= "\\rowcolor{ltgray}\n";

                foreach my $oColumn (@oyColumn)
                {
                    $strLine .= (defined($strLine) ? ' & ' : '') . '\textbf{' . $self->processText($oColumn->textGet()) . '}';
                }

                $strLatex .= "${strLine}\\\\";
            }

            # Build the rows
            foreach my $oRow ($oChild->nodeGet('table-data')->nodeList('table-row'))
            {
                $strLatex .= "\\hline\n";
                undef($strLine);

                foreach my $oRowCell ($oRow->nodeList('table-cell'))
                {
                    $strLine .= (defined($strLine) ? ' & ' : '') . $self->processText($oRowCell->textGet());
                }

                $strLatex .= "${strLine}\\\\";
            }

            $strLatex .= "\\hline\n\\end{tabularx}\n";

            # If there is a title for the table, add it. Ignore the label since LaTex will automatically generate numbered labels.
            # e.g. Table 1:
            if ($oChild->nodeGet("title", false))
            {
                $strLatex .= "\\caption{" . $self->processText($oChild->nodeGet("title")->textGet()) . "}\n";
            }

            $strLatex .= "\\end{table}\n";
        }
        # Add descriptive text
        elsif ($oChild->nameGet() eq 'p')
        {
            $strLatex .= "\n" . $self->processText($oChild->textGet()) . "\n";
        }
        # Add option descriptive text
        elsif ($oChild->nameGet() eq 'option-description')
        {
            my $strOption = $oChild->paramGet("key");
            my $oDescription = ${$self->{oReference}->{oConfigHash}}{&CONFIG_HELP_OPTION}{$strOption}{&CONFIG_HELP_DESCRIPTION};

            if (!defined($oDescription))
            {
                confess &log(ERROR, "unable to find ${strOption} option in sections - try adding option?");
            }

            $strLatex .= "\n" . $self->processText($oDescription) . "\n";
        }
        # Add cmd descriptive text
        elsif ($oChild->nameGet() eq 'cmd-description')
        {
            my $strCommand = $oChild->paramGet("key");
            my $oDescription = ${$self->{oReference}->{oConfigHash}}{&CONFIG_HELP_COMMAND}{$strCommand}{&CONFIG_HELP_DESCRIPTION};

            if (!defined($oDescription))
            {
                confess &log(ERROR, "unable to find ${strCommand} command in sections - try adding command?");
            }

            $strLatex .= "\n" . $self->processText($oDescription) . "\n";
        }
        # Add a list
        elsif ($oChild->nameGet() eq 'list')
        {
            $strLatex .= "\n\\begin{itemize}";

            foreach my $oListItem ($oChild->nodeList())
            {
                $strLatex .= "\n    \\item " . $self->processText($oListItem->textGet());
            }

            $strLatex .= "\n\\end{itemize}";
        }
        # Add/remove config options
        elsif ($oChild->nameGet() eq 'backrest-config' || $oChild->nameGet() eq 'postgres-config')
        {
            $strLatex .= $self->configProcess($oSection, $oChild, $iDepth + 3);
        }
        # Add a subsection
        elsif ($oChild->nameGet() eq 'section')
        {
            $strLatex .= "\n" . $self->sectionProcess($oChild, $strSection, $iDepth + 1);
        }
        # Add an admonition (e.g. NOTE, WARNING, etc)
        elsif ($oChild->nameGet() eq 'admonition')
        {
            $strLatex .= "\n\\begin{leftbar}";
            $strLatex .= "\n\\textit{\\textbf{" . uc($oChild->paramGet('type')) . ": }";
            $strLatex .= $self->processText($oChild->textGet()) . "}";
            $strLatex .= "\n\\end{leftbar}\n";
        }
        # Check if the child can be processed by a parent
        else
        {
            $self->sectionChildProcess($oSection, $oChild, $iDepth + 1);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strSection', value => $strLatex, trace => true}
    );
}

####################################################################################################################################
# configProcess
####################################################################################################################################
sub configProcess
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oSection,
        $oConfig,
        $iDepth
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->configProcess', \@_,
            {name => 'oSection'},
            {name => 'oConfig'},
            {name => 'iDepth'}
        );

    # Working variables
    my $strLatex = '';
    my $strFile;
    my $strConfig;
    my $bShow = true;

    # Generate the config
    if ($oConfig->nameGet() eq 'backrest-config')
    {
        ($strFile, $strConfig, $bShow) = $self->backrestConfig($oSection, $oConfig, $iDepth);
    }
    else
    {
        ($strFile, $strConfig, $bShow) = $self->postgresConfig($oSection, $oConfig, $iDepth);
    }

    if ($bShow)
    {
        my $strHostName = $self->{oManifest}->variableReplace($oConfig->paramGet('host'));

        # Replace _ in filename
        $strFile = $self->variableReplace($strFile);

        # Render the config
        $strLatex =
            "\n\\begin\{lstlisting\}[title=\{\\textnormal{\\textbf\{${strHostName}}}:\\textnormal{\\texttt\{${strFile}}} --- " .
            $self->processText($oConfig->nodeGet('title')->textGet()) . "}]\n" .
            (defined($strConfig) ? $strConfig : '') .
            "\\end{lstlisting}\n";
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strConfig', value => $strLatex, trace => true}
    );
}

1;
