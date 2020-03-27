####################################################################################################################################
# DOC HTML PAGE MODULE
####################################################################################################################################
package pgBackRestDoc::Html::DocHtmlPage;
use parent 'pgBackRestDoc::Common::DocExecute';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Data::Dumper;
use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRestDoc::Common::DocConfig;
use pgBackRestDoc::Common::DocManifest;
use pgBackRestDoc::Common::DocRender;
use pgBackRestDoc::Html::DocHtmlBuilder;
use pgBackRestDoc::Html::DocHtmlElement;
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
        $bMenu,
        $bExe,
        $bCompact,
        $strCss,
        $bPretty,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'oManifest'},
            {name => 'strRenderOutKey'},
            {name => 'bMenu'},
            {name => 'bExe'},
            {name => 'bCompact'},
            {name => 'strCss'},
            {name => 'bPretty'},
        );

    # Create the class hash
    my $self = $class->SUPER::new(RENDER_TYPE_HTML, $oManifest, $strRenderOutKey, $bExe);
    bless $self, $class;

    $self->{bMenu} = $bMenu;
    $self->{bCompact} = $bCompact;
    $self->{strCss} = $strCss;
    $self->{bPretty} = $bPretty;

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
    my $oRender = $self->{oManifest}->renderGet(RENDER_TYPE_HTML);

    # Initialize page
    my $strTitle = $oPage->paramGet('title');
    my $strSubTitle = $oPage->paramGet('subtitle', false);

    my $oHtmlBuilder = new pgBackRestDoc::Html::DocHtmlBuilder(
        $self->{oManifest}->variableReplace('{[project]}' . (defined($self->{oManifest}->variableGet('project-tagline')) ?
            ' - ' . $self->{oManifest}->variableGet('project-tagline') : '')),
        $self->{oManifest}->variableReplace($strTitle . (defined($strSubTitle) ? " - ${strSubTitle}" : '')),
        $self->{oManifest}->variableGet('project-favicon'),
        $self->{oManifest}->variableGet('project-logo'),
        $self->{oManifest}->variableReplace(trim($self->{oDoc}->fieldGet('description'))),
        $self->{bPretty},
        $self->{bCompact},
        $self->{bCompact} ? $self->{strCss} : undef);

    # Generate header
    my $oPageHeader = $oHtmlBuilder->bodyGet()->addNew(HTML_DIV, 'page-header');

    # add the logo to the header
    if (defined($self->{oManifest}->variableGet('html-logo')))
    {
        $oPageHeader->
            addNew(HTML_DIV, 'page-header-logo',
                   {strContent =>"{[html-logo]}"});
    }

    $oPageHeader->
        addNew(HTML_DIV, 'page-header-title',
               {strContent => $strTitle});

    if (defined($strSubTitle))
    {
        $oPageHeader->
            addNew(HTML_DIV, 'page-header-subtitle',
                   {strContent => $strSubTitle});
    }

    # Generate menu
    if ($self->{bMenu})
    {
        my $oMenuBody = $oHtmlBuilder->bodyGet()->addNew(HTML_DIV, 'page-menu')->addNew(HTML_DIV, 'menu-body');

        # Get the menu in the order listed in the manifest.xml
        foreach my $strRenderOutKey (@{${$oRender}{stryOrder}})
        {
            # Do not output the menu item for the page the user is on (e.g. on Command page, the Command menu item will not appear)
            if ($strRenderOutKey ne $self->{strRenderOutKey})
            {
                my $oRenderOut = $self->{oManifest}->renderOutGet(RENDER_TYPE_HTML, $strRenderOutKey);

                if (defined($$oRenderOut{menu}))
                {
                    $oMenuBody->addNew(HTML_DIV, 'menu')->addNew(
                        HTML_A, 'menu-link',
                        {strContent => $$oRenderOut{menu},
                            strRef => $strRenderOutKey eq 'index' ? '{[project-url-root]}' : "${strRenderOutKey}.html"});
                }
            }
        }
    }

    # Generate table of contents
    my $oPageTocBody;

    if ($self->{bToc})
    {
        my $oPageToc = $oHtmlBuilder->bodyGet()->addNew(HTML_DIV, 'page-toc');

        $oPageToc->addNew(HTML_DIV, 'page-toc-header')->addNew(HTML_DIV, 'page-toc-title', {strContent => "Table of Contents"});

        $oPageTocBody = $oPageToc->
            addNew(HTML_DIV, 'page-toc-body');
    }

    # Generate body
    my $oPageBody = $oHtmlBuilder->bodyGet()->addNew(HTML_DIV, 'page-body');
    my $iSectionNo = 1;

    # Render sections
    foreach my $oSection ($oPage->nodeList('section'))
    {
        my ($oChildSectionElement, $oChildSectionTocElement) =
            $self->sectionProcess($oSection, undef, "${iSectionNo}", 1);

        $oPageBody->add($oChildSectionElement);

        if (defined($oPageTocBody) && defined($oChildSectionTocElement))
        {
            $oPageTocBody->add($oChildSectionTocElement);
        }

        $iSectionNo++;
    }

    my $oPageFooter = $oHtmlBuilder->bodyGet()->
        addNew(HTML_DIV, 'page-footer',
               {strContent => '{[html-footer]}'});

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strHtml', value => $oHtmlBuilder->htmlGet(), trace => true}
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
        $strAnchor,
        $strSectionNo,
        $iDepth
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->sectionProcess', \@_,
            {name => 'oSection'},
            {name => 'strAnchor', required => false},
            {name => 'strSectionNo'},
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

    # Working variables
    $strAnchor =
        ($oSection->paramTest(XML_SECTION_PARAM_ANCHOR, XML_SECTION_PARAM_ANCHOR_VALUE_NOINHERIT) ? '' :
            (defined($strAnchor) ? "${strAnchor}/" : '')) .
        $oSection->paramGet('id');

    # Create the section toc element
    my $oSectionTocElement = new pgBackRestDoc::Html::DocHtmlElement(HTML_DIV, "section${iDepth}-toc");

    # Create the section element
    my $oSectionElement = new pgBackRestDoc::Html::DocHtmlElement(HTML_DIV, "section${iDepth}");

    # Add the section anchor
    $oSectionElement->addNew(HTML_A, undef, {strId => $strAnchor});

    # Add the section title to section and toc
    my $oSectionHeaderElement = $oSectionElement->addNew(HTML_DIV, "section${iDepth}-header");
    my $strSectionTitle = $self->processText($oSection->nodeGet('title')->textGet());

    if ($self->{bTocNumber})
    {
        $oSectionHeaderElement->addNew(HTML_DIV, "section${iDepth}-number", {strContent => $strSectionNo});
    }

    $oSectionHeaderElement->addNew(HTML_DIV, "section${iDepth}-title", {strContent => $strSectionTitle});

    if ($self->{bTocNumber})
    {
        $oSectionTocElement->addNew(HTML_DIV, "section${iDepth}-toc-number", {strContent => $strSectionNo});
    }

    my $oTocSectionTitleElement = $oSectionTocElement->addNew(HTML_DIV, "section${iDepth}-toc-title");

    $oTocSectionTitleElement->addNew(
        HTML_A, undef,
        {strContent => $strSectionTitle, strRef => "#${strAnchor}"});

    # Add the section intro if it exists
    if (defined($oSection->textGet(false)))
    {
        $oSectionElement->
            addNew(HTML_DIV, "section-intro",
                   {strContent => $self->processText($oSection->textGet())});
    }

    # Add the section body
    my $oSectionBodyElement = $oSectionElement->addNew(HTML_DIV, "section-body");

    # Process each child
    my $iSectionNo = 1;

    foreach my $oChild ($oSection->nodeList())
    {
        &log(DEBUG, ('    ' x ($iDepth + 2)) . 'process child ' . $oChild->nameGet());

        # Execute a command
        if ($oChild->nameGet() eq 'execute-list')
        {
            my $bShow = $oChild->paramTest('show', 'n') ? false : true;
            my $oExecuteBodyElement;
            my $bFirst = true;
            my $strHostName = $self->{oManifest}->variableReplace($oChild->paramGet('host'));

            if ($bShow)
            {
                my $oSectionBodyExecute = $oSectionBodyElement->addNew(HTML_DIV, "execute");
                $oSectionBodyExecute->
                    addNew(HTML_DIV, "execute-title",
                           {strContent => "<span class=\"host\">${strHostName}</span> <b>&#x21d2;</b> " .
                                          $self->processText($oChild->nodeGet('title')->textGet())});
                $oExecuteBodyElement = $oSectionBodyExecute->addNew(HTML_DIV, "execute-body");
            }

            foreach my $oExecute ($oChild->nodeList('execute'))
            {
                my $bExeShow = !$oExecute->paramTest('show', 'n');
                my $bExeExpectedError = defined($oExecute->paramGet('err-expect', false));

                my ($strCommand, $strOutput) = $self->execute(
                    $oSection, $strHostName, $oExecute, {iIndent => $iDepth + 3, bShow => $bShow && $bExeShow});

                if ($bShow && $bExeShow)
                {
                    # Add continuation chars and proper spacing
                    $strCommand =~ s/\n/\n   /smg;

                    $oExecuteBodyElement->
                        addNew(HTML_PRE, "execute-body-cmd",
                               {strContent => $strCommand, bPre => true});

                    my $strHighLight = $self->{oManifest}->variableReplace($oExecute->fieldGet('exe-highlight', false));
                    my $bHighLightFound = false;

                    if (defined($strOutput))
                    {
                        my $bHighLightOld;
                        my $strHighLightOutput;

                        if ($oExecute->fieldTest('exe-highlight-type', 'error'))
                        {
                            $bExeExpectedError = true;
                        }

                        foreach my $strLine (split("\n", $strOutput))
                        {
                            my $bHighLight = defined($strHighLight) && $strLine =~ /$strHighLight/;

                            if (defined($bHighLightOld) && $bHighLight != $bHighLightOld)
                            {
                                $oExecuteBodyElement->
                                    addNew(HTML_PRE, 'execute-body-output' .
                                           ($bHighLightOld ? '-highlight' . ($bExeExpectedError ? '-error' : '') : ''),
                                           {strContent => $strHighLightOutput, bPre => true});

                                undef($strHighLightOutput);
                            }

                            $strHighLightOutput .= (defined($strHighLightOutput) ? "\n" : '') . $strLine;
                            $bHighLightOld = $bHighLight;

                            $bHighLightFound = $bHighLightFound ? true : $bHighLight ? true : false;
                        }

                        if (defined($bHighLightOld))
                        {
                            $oExecuteBodyElement->
                                addNew(HTML_PRE, 'execute-body-output' .
                                       ($bHighLightOld ? '-highlight' . ($bExeExpectedError ? '-error' : '') : ''),
                                       {strContent => $strHighLightOutput, bPre => true});
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
        }
        # Add code block
        elsif ($oChild->nameGet() eq 'code-block')
        {
            my $strValue = $oChild->valueGet();

            # Trim linefeeds from the beginning and all whitespace from the end
            $strValue =~ s/^\n+|\s+$//g;

            # Find the line with the fewest leading spaces
            my $iSpaceMin = undef;

            foreach my $strLine (split("\n", $strValue))
            {
                $strLine =~ s/\s+$//;

                my $iSpaceMinTemp = length($strLine) - length(trim($strLine));

                if (!defined($iSpaceMin) || $iSpaceMinTemp < $iSpaceMin)
                {
                    $iSpaceMin = $iSpaceMinTemp;
                }
            }

            # Replace the leading spaces
            $strValue =~ s/^( ){$iSpaceMin}//smg;

            $oSectionBodyElement->addNew(
                HTML_PRE, 'code-block', {strContent => $strValue, bPre => true});
        }
        # Add table
        elsif ($oChild->nameGet() eq 'table')
        {
            my $oTableTitle;
            if ($oChild->nodeTest('title'))
            {
                $oTableTitle = $oChild->nodeGet('title');
            }

            my $oTableElement = $oSectionBodyElement->addNew(HTML_TABLE, 'table');
            my @oyColumn;

            # If there is a title element then add it as the caption for the table
            if (defined($oTableTitle))
            {
                # Print the label (e.g. Table 1:) in front of the title if one exists
                my $strTableTitle = $oTableTitle->paramTest('label') ?
                    ($oTableTitle->paramGet('label') . ': '. $self->processText($oTableTitle->textGet())) :
                    $self->processText($oTableTitle->textGet());

                $oTableElement->addNew(HTML_TABLE_CAPTION, 'table-caption', {strContent => $strTableTitle});
            }

            # Build the header
            if ($oChild->nodeTest('table-header'))
            {
                my $oHeader = $oChild->nodeGet('table-header');
                @oyColumn = $oHeader->nodeList('table-column');

                my $oHeaderRowElement = $oTableElement->addNew(HTML_TR, 'table-header-row');

                foreach my $oColumn (@oyColumn)
                {
                    # Each column can have different alignment properties - if not set, then default to align left
                    my $strAlign = $oColumn->paramGet("align", false, 'left');
                    my $bFill = $oColumn->paramTest('fill', 'y');

                    $oHeaderRowElement->addNew(
                        HTML_TH,
                        "table-header-${strAlign}" . ($bFill ? " table-header-fill" : ""),
                        {strContent => $self->processText($oColumn->textGet())});
                }
            }

            # Build the rows
            foreach my $oRow ($oChild->nodeGet('table-data')->nodeList('table-row'))
            {
                my $oRowElement = $oTableElement->addNew(HTML_TR, 'table-row');
                my @oRowCellList = $oRow->nodeList('table-cell');

                for (my $iRowCellIdx = 0; $iRowCellIdx < @oRowCellList; $iRowCellIdx++)
                {
                    my $oRowCell = $oRowCellList[$iRowCellIdx];

                    # If a header row was defined, then get the column alignment, else default to left
                    my $strAlign = @oyColumn > 0 ? $oyColumn[$iRowCellIdx]->paramGet("align", false, 'left') : 'left';

                    $oRowElement->addNew(
                        HTML_TD, "table-data-${strAlign}", {strContent => $self->processText($oRowCell->textGet())});
                }
            }
        }
        # Add descriptive text
        elsif ($oChild->nameGet() eq 'p')
        {
            $oSectionBodyElement->
                addNew(HTML_DIV, 'section-body-text',
                       {strContent => $self->processText($oChild->textGet())});
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

            $oSectionBodyElement->
                addNew(HTML_DIV, 'section-body-text',
                       {strContent => $self->processText($oDescription)});
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

            $oSectionBodyElement->
                addNew(HTML_DIV, 'section-body-text',
                       {strContent => $self->processText($oDescription)});
        }
        # Add/remove backrest config options
        elsif ($oChild->nameGet() eq 'backrest-config')
        {
            my $oConfigElement = $self->backrestConfigProcess($oSection, $oChild, $iDepth + 3);

            if (defined($oConfigElement))
            {
                $oSectionBodyElement->add($oConfigElement);
            }
        }
        # Add/remove postgres config options
        elsif ($oChild->nameGet() eq 'postgres-config')
        {
            my $oConfigElement = $self->postgresConfigProcess($oSection, $oChild, $iDepth + 3);

            if (defined($oConfigElement))
            {
                $oSectionBodyElement->add($oConfigElement);
            }
        }
        # Add a list
        elsif ($oChild->nameGet() eq 'list')
        {
            my $oList = $oSectionBodyElement->addNew(HTML_UL, 'list-unordered');

            foreach my $oListItem ($oChild->nodeList())
            {
                $oList->addNew(HTML_LI, 'list-unordered', {strContent => $self->processText($oListItem->textGet())});
            }
        }
        # Add a subtitle
        elsif ($oChild->nameGet() eq 'subtitle')
        {
            $oSectionBodyElement->
                addNew(HTML_DIV, "section${iDepth}-subtitle",
                       {strContent => $self->processText($oChild->textGet())});
        }
        # Add a subsubtitle
        elsif ($oChild->nameGet() eq 'subsubtitle')
        {
            $oSectionBodyElement->
                addNew(HTML_DIV, "section${iDepth}-subsubtitle",
                       {strContent => $self->processText($oChild->textGet())});
        }
        # Add a subsection
        elsif ($oChild->nameGet() eq 'section')
        {
            my ($oChildSectionElement, $oChildSectionTocElement) =
                $self->sectionProcess($oChild, $strAnchor, "${strSectionNo}.${iSectionNo}", $iDepth + 1);

            $oSectionBodyElement->add($oChildSectionElement);

            if (defined($oChildSectionTocElement))
            {
                $oSectionTocElement->add($oChildSectionTocElement);
            }

            $iSectionNo++;
        }
        # Add an admonition (e.g. NOTE, WARNING, etc)
        elsif ($oChild->nameGet() eq 'admonition')
        {
            my $oAdmonition = $oSectionBodyElement->addNew(HTML_DIV, 'admonition');
            $oAdmonition->addNew(HTML_DIV, $oChild->paramGet('type'), {strContent => uc($oChild->paramGet('type')) . ": "});
            $oAdmonition->addNew(HTML_DIV, $oChild->paramGet('type') . '-text',
                {strContent => $self->processText($oChild->textGet())});
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
        {name => 'oSectionElement', value => $oSectionElement, trace => true},
        {name => 'oSectionTocElement', value => $oSection->paramTest('toc', 'n') ? undef : $oSectionTocElement, trace => true}
    );
}

####################################################################################################################################
# backrestConfigProcess
####################################################################################################################################
sub backrestConfigProcess
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
            __PACKAGE__ . '->backrestConfigProcess', \@_,
            {name => 'oSection'},
            {name => 'oConfig'},
            {name => 'iDepth'}
        );

    # Generate the config
    my $oConfigElement;
    my ($strFile, $strConfig, $bShow) = $self->backrestConfig($oSection, $oConfig, $iDepth);

    if ($bShow)
    {
        my $strHostName = $self->{oManifest}->variableReplace($oConfig->paramGet('host'));

        # Render the config
        $oConfigElement = new pgBackRestDoc::Html::DocHtmlElement(HTML_DIV, "config");

        $oConfigElement->
            addNew(HTML_DIV, "config-title",
                   {strContent => "<span class=\"host\">${strHostName}</span>:<span class=\"file\">${strFile}</span>" .
                                  " <b>&#x21d2;</b> " . $self->processText($oConfig->nodeGet('title')->textGet())});

        my $oConfigBodyElement = $oConfigElement->addNew(HTML_DIV, "config-body");
        #
        # $oConfigBodyElement->
        #     addNew(HTML_DIV, "config-body-title",
        #            {strContent => "${strFile}:"});

        $oConfigBodyElement->
            addNew(HTML_DIV, "config-body-output",
                   {strContent => $strConfig});
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oConfigElement', value => $oConfigElement, trace => true}
    );
}

####################################################################################################################################
# postgresConfigProcess
####################################################################################################################################
sub postgresConfigProcess
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
            __PACKAGE__ . '->postgresConfigProcess', \@_,
            {name => 'oSection'},
            {name => 'oConfig'},
            {name => 'iDepth'}
        );

    # Generate the config
    my $oConfigElement;
    my ($strFile, $strConfig, $bShow) = $self->postgresConfig($oSection, $oConfig, $iDepth);

    if ($bShow)
    {
        # Render the config
        my $strHostName = $self->{oManifest}->variableReplace($oConfig->paramGet('host'));
        $oConfigElement = new pgBackRestDoc::Html::DocHtmlElement(HTML_DIV, "config");

        $oConfigElement->
            addNew(HTML_DIV, "config-title",
                   {strContent => "<span class=\"host\">${strHostName}</span>:<span class=\"file\">${strFile}</span>" .
                                  " <b>&#x21d2;</b> " . $self->processText($oConfig->nodeGet('title')->textGet())});

        my $oConfigBodyElement = $oConfigElement->addNew(HTML_DIV, "config-body");

        # $oConfigBodyElement->
        #     addNew(HTML_DIV, "config-body-title",
        #            {strContent => "append to ${strFile}:"});

        $oConfigBodyElement->
            addNew(HTML_DIV, "config-body-output",
                   {strContent => defined($strConfig) ? $strConfig : '<No PgBackRest Settings>'});

        $oConfig->fieldSet('actual-config', $strConfig);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oConfigElement', value => $oConfigElement, trace => true}
    );
}

1;
