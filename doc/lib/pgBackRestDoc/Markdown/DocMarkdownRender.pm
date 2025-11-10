####################################################################################################################################
# DOC MARKDOWN RENDER MODULE
####################################################################################################################################
package pgBackRestDoc::Markdown::DocMarkdownRender;
use parent 'pgBackRestDoc::Common::DocExecute';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Data::Dumper;
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);
use File::Copy;
use Storable qw(dclone);

use pgBackRestDoc::Common::DocManifest;
use pgBackRestDoc::Common::Log;
use pgBackRestDoc::Common::String;

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
    my $self = $class->SUPER::new(RENDER_TYPE_MARKDOWN, $oManifest, $strRenderOutKey, $bExe);
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

    # Initialize page
    my $strMarkdown = "# " . $oPage->paramGet('title');

    if (defined($oPage->paramGet('subtitle', false)))
    {
        $strMarkdown .= ' <br/> ' . $oPage->paramGet('subtitle') . '';
    }

    # Render sections
    foreach my $oSection ($oPage->nodeList('section'))
    {
        $strMarkdown = trim($strMarkdown) . "\n\n" . $self->sectionProcess($oSection, 1);
    }

    $strMarkdown .= "\n";

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strMarkdown', value => $strMarkdown, trace => true}
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
        $iDepth
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->sectionProcess', \@_,
            {name => 'oSection'},
            {name => 'iDepth'}
        );

    if ($oSection->paramGet('log'))
    {
        &log(INFO, ('    ' x ($iDepth + 1)) . 'process section: ' . $oSection->paramGet('path'));
    }

    if ($iDepth > 3)
    {
        confess &log(ASSERT, "section depth of ${iDepth} exceeds maximum");
    }

    my $strMarkdown = '#' . ('#' x $iDepth) . ' ' . $self->processText($oSection->nodeGet('title')->textGet());

    my $strLastChild = undef;

    foreach my $oChild ($oSection->nodeList())
    {
        &log(DEBUG, ('    ' x ($iDepth + 2)) . 'process child ' . $oChild->nameGet());

        # Execute a command
        if ($oChild->nameGet() eq 'execute-list')
        {
            my $bShow = $oChild->paramTest('show', 'n') ? false : true;
            my $bFirst = true;
            my $strHostName = $self->{oManifest}->variableReplace($oChild->paramGet('host'));
            my $bOutput = false;

            if ($bShow)
            {
                $strMarkdown .=
                    "\n\n${strHostName} => " . $self->processText($oChild->nodeGet('title')->textGet()) .
                    "\n```\n";
            }

            foreach my $oExecute ($oChild->nodeList('execute'))
            {
                my $bExeShow = !$oExecute->paramTest('show', 'n');
                my $bExeExpectedError = defined($oExecute->paramGet('err-expect', false));

                if ($bOutput)
                {
                    confess &log(ERROR, "only the last command can have output");
                }

                my ($strCommand, $strOutput) = $self->execute(
                    $oSection, $strHostName, $oExecute, {iIndent => $iDepth + 3, bShow => $bShow && $bExeShow});

                if ($bShow && $bExeShow)
                {
                    # Add continuation chars and proper spacing
                    $strCommand =~ s/\n/\n   /smg;

                    $strMarkdown .= "${strCommand}\n";

                    my $strHighLight = $self->{oManifest}->variableReplace($oExecute->fieldGet('exe-highlight', false));
                    my $bHighLightFound = false;

                    if (defined($strOutput))
                    {
                        $strMarkdown .= "\n--- output ---\n\n";

                        if ($oExecute->fieldTest('exe-highlight-type', 'error'))
                        {
                            $bExeExpectedError = true;
                        }

                        foreach my $strLine (split("\n", $strOutput))
                        {
                            my $bHighLight = defined($strHighLight) && $strLine =~ /$strHighLight/;

                            if ($bHighLight)
                            {
                                $strMarkdown .= $bExeExpectedError ? "ERR" : "-->";
                            }
                            else
                            {
                                $strMarkdown .= "   ";
                            }

                            $strMarkdown .= " ${strLine}\n";

                            $bHighLightFound = $bHighLightFound ? true : $bHighLight ? true : false;
                        }

                        $bFirst = true;
                    }

                    if ($self->{bExe} && $self->isRequired($oSection) && defined($strHighLight) && !$bHighLightFound)
                    {
                        confess &log(ERROR, "unable to find a match for highlight: ${strHighLight}");
                    }
                }

                $bFirst = false;
            }

            $strMarkdown .= "```";
        }
        # Add code block
        elsif ($oChild->nameGet() eq 'code-block')
        {
            if ($oChild->paramTest('title'))
            {
                if (defined($strLastChild) && $strLastChild ne 'code-block')
                {
                    $strMarkdown .= "\n";
                }

                $strMarkdown .= "\n_" . $oChild->paramGet('title') . "_:";
            }

            $strMarkdown .= "\n```";

            if ($oChild->paramTest('type'))
            {
                $strMarkdown .= $oChild->paramGet('type');
            }

            $strMarkdown .= "\n" . trim($oChild->valueGet()) . "\n```";
        }
        # Add descriptive text
        elsif ($oChild->nameGet() eq 'p')
        {
            if (defined($strLastChild) && $strLastChild ne 'code-block' && $strLastChild ne 'table')
            {
                $strMarkdown .= "\n";
            }

            $strMarkdown .= "\n" . $self->processText($oChild->textGet());
        }
        # Add a list
        elsif ($oChild->nameGet() eq 'list')
        {
            foreach my $oListItem ($oChild->nodeList())
            {
                $strMarkdown .= "\n\n- " . $self->processText($oListItem->textGet());
            }
        }
        # Add a subsection
        elsif ($oChild->nameGet() eq 'section')
        {
            $strMarkdown = trim($strMarkdown) . "\n\n" . $self->sectionProcess($oChild, $iDepth + 1);
        }
        elsif ($oChild->nameGet() eq 'table')
        {
            my $oTableTitle;
            if ($oChild->nodeTest('title'))
            {
                $oTableTitle = $oChild->nodeGet('title');
            }

            my $oHeader;
            my @oyColumn;

            if ($oChild->nodeTest('table-header'))
            {
                $oHeader = $oChild->nodeGet('table-header');
                @oyColumn = $oHeader->nodeList('table-column');
            }

            if (defined($oTableTitle))
            {
                # Print the label (e.g. Table 1:) in front of the title if one exists
                $strMarkdown .= "\n\n**" . ($oTableTitle->paramTest('label') ?
                    ($oTableTitle->paramGet('label') . ': ' . $self->processText($oTableTitle->textGet())) :
                    $self->processText($oTableTitle->textGet())) . "**\n\n";
            }
            else
            {
                $strMarkdown .= "\n\n";
            }

            my $strHeaderText = "| ";
            my $strHeaderIndicator = "| ";

            for (my $iColCellIdx = 0; $iColCellIdx < @oyColumn; $iColCellIdx++)
            {
                my $strAlign = $oyColumn[$iColCellIdx]->paramGet("align", false, 'left');

                $strHeaderText .= $self->processText($oyColumn[$iColCellIdx]->textGet()) .
                    (($iColCellIdx < @oyColumn - 1) ? " | " : " |\n");
                $strHeaderIndicator .= ($strAlign eq 'left' || $strAlign eq 'center') ? ":---" : "---";
                $strHeaderIndicator .= ($strAlign eq 'right' || $strAlign eq 'center') ? "---:" : "";
                $strHeaderIndicator .= ($iColCellIdx < @oyColumn - 1) ? " | " : " |\n";
            }

            # Markdown requires a table header so if not provided then create an empty header row and default the column alignment
            # left by using the number of columns in the 1st row
            if (!defined($oHeader))
            {
                my @oyRow = $oChild->nodeGet('table-data')->nodeList('table-row');
                foreach my $oRowCell ($oyRow[0]->nodeList('table-cell'))
                {
                    $strHeaderText .= "     | ";
                    $strHeaderIndicator .= ":--- | ";
                }
                $strHeaderText .= "\n";
                $strHeaderIndicator .= "\n";
            }

            $strMarkdown .= (defined($strHeaderText) ? $strHeaderText : '') . $strHeaderIndicator;

            # Build the rows
            foreach my $oRow ($oChild->nodeGet('table-data')->nodeList('table-row'))
            {
                my @oRowCellList = $oRow->nodeList('table-cell');
                $strMarkdown .= "| ";

                for (my $iRowCellIdx = 0; $iRowCellIdx < @oRowCellList; $iRowCellIdx++)
                {
                    my $oRowCell = $oRowCellList[$iRowCellIdx];

                    $strMarkdown .= $self->processText($oRowCell->textGet()) .
                        (($iRowCellIdx < @oRowCellList -1) ? " | " : " |\n");
                }
            }
        }
        # Add an admonition (e.g. NOTE, WARNING, etc)
        elsif ($oChild->nameGet() eq 'admonition')
        {
            $strMarkdown .= "\n> **" . uc($oChild->paramGet('type')) . ":** " . $self->processText($oChild->textGet());
        }
         # Check if the child can be processed by a parent
        else
        {
            $self->sectionChildProcess($oSection, $oChild, $iDepth + 1);
        }

        $strLastChild = $oChild->nameGet();
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strMarkdown', value => $strMarkdown, trace => true}
    );
}

1;
