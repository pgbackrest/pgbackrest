####################################################################################################################################
# DOC HTML PAGE MODULE
####################################################################################################################################
package BackRestDoc::Html::DocHtmlPage;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Data::Dumper;
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);
use File::Copy;
use Storable qw(dclone);

use lib dirname($0) . '/../lib';
use BackRest::Common::Ini;
use BackRest::Common::Log;
use BackRest::Common::String;
use BackRest::Config::ConfigHelp;
use BackRest::FileCommon;

use lib dirname($0) . '/../test/lib';
use BackRestTest::Common::ExecuteTest;

use BackRestDoc::Html::DocHtmlBuilder;
use BackRestDoc::Html::DocHtmlElement;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_DOC_HTML_PAGE                                       => 'DocHtmlPage';

use constant OP_DOC_HTML_PAGE_BACKREST_CONFIG_PROCESS               => OP_DOC_HTML_PAGE . '->backrestConfigProcess';
use constant OP_DOC_HTML_PAGE_EXECUTE                               => OP_DOC_HTML_PAGE . '->execute';
use constant OP_DOC_HTML_PAGE_NEW                                   => OP_DOC_HTML_PAGE . '->new';
use constant OP_DOC_HTML_PAGE_POSTGRES_CONFIG_PROCESS               => OP_DOC_HTML_PAGE . '->postgresConfigProcess';
use constant OP_DOC_HTML_PAGE_PROCESS                               => OP_DOC_HTML_PAGE . '->process';
use constant OP_DOC_HTML_PAGE_SECTION_PROCESS                       => OP_DOC_HTML_PAGE . '->sectionProcess';

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;       # Class name

    # Create the class hash
    my $self = {};
    bless $self, $class;

    $self->{strClass} = $class;

    # Assign function parameters, defaults, and log debug info
    (
        my $strOperation,
        $self->{oSite},
        $self->{strPageId},
        $self->{bExe}
    ) =
        logDebugParam
        (
            OP_DOC_HTML_PAGE_NEW, \@_,
            {name => 'oSite'},
            {name => 'strPageId'},
            {name => 'bExe', default => true}
        );
    #
    # use Data::Dumper;
    # confess Dumper(${$self->{oSite}->{oSite}}{common}{oRender});

    # Copy page data to self
    $self->{oPage} = ${$self->{oSite}->{oSite}}{page}{$self->{strPageId}};
    $self->{oDoc} = ${$self->{oPage}}{'oDoc'};
    $self->{oRender} = ${$self->{oSite}->{oSite}}{common}{oRender};
    $self->{oReference} = ${$self->{oSite}->{oSite}}{common}{oReference};

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# execute
####################################################################################################################################
sub execute
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oCommand,
        $iIndent
    ) =
        logDebugParam
        (
            OP_DOC_HTML_PAGE_EXECUTE, \@_,
            {name => 'oCommand'},
            {name => 'iIndent', default => 1}
        );

    # Working variables
    my $strOutput;

    # Command variables
    my $strCommand = trim($oCommand->fieldGet('exe-cmd'));
    my $strUser = $oCommand->fieldGet('exe-user', false);
    my $bSuppressError = defined($oCommand->fieldGet('exe-err-suppress', false)) ? $oCommand->fieldGet('exe-err-suppress') : false;
    my $bSuppressStdErr = defined($oCommand->fieldGet('exe-err-suppress-stderr', false)) ?
                              $oCommand->fieldGet('exe-err-suppress-stderr') : false;
    my $bExeSkip = defined($oCommand->fieldGet('exe-skip', false)) ? $oCommand->fieldGet('exe-skip') : false;
    my $bExeOutput = defined($oCommand->fieldGet('exe-output', false)) ? $oCommand->fieldGet('exe-output') : false;
    my $bExeRetry = defined($oCommand->fieldGet('exe-retry', false)) ? $oCommand->fieldGet('exe-retry') : false;
    my $strExeVar = defined($oCommand->fieldGet('exe-var', false)) ? $oCommand->fieldGet('exe-var') : undef;
    my $iExeExpectedError = defined($oCommand->fieldGet('exe-err-expect', false)) ? $oCommand->fieldGet('exe-err-expect') : undef;

    if ($bExeRetry)
    {
        sleep(1);
    }

    $strUser = defined($strUser) ? $strUser : 'postgres';
    $strCommand = $self->{oSite}->variableReplace(
        ($strUser eq 'vagrant' ? '' : 'sudo ' . ($strUser eq 'root' ? '' : "-u ${strUser} ")) . $strCommand);

    &log(INFO, ('    ' x $iIndent) . "execute: $strCommand");

    if (!$bExeSkip)
    {
        if ($self->{bExe})
        {
            my $oExec = new BackRestTest::Common::ExecuteTest($strCommand,
                                                              {bSuppressError => $bSuppressError,
                                                               bSuppressStdErr => $bSuppressStdErr,
                                                               iExpectedExitStatus => $iExeExpectedError});
            $oExec->begin();
            $oExec->end();

            if ($bExeOutput && defined($oExec->{strOutLog}) && $oExec->{strOutLog} ne '')
            {
                $strOutput = trim($oExec->{strOutLog});
                $strOutput =~ s/^[0-9]{4}-[0-1][0-9]-[0-3][0-9] [0-2][0-9]:[0-6][0-9]:[0-6][0-9]\.[0-9]{3} T[0-9]{2} //smg;
            }

            if (defined($strExeVar))
            {
                $self->{oSite}->variableSet($strExeVar, trim($oExec->{strOutLog}));
            }

            if (defined($iExeExpectedError))
            {
                $strOutput .= trim($oExec->{strErrorLog});
            }
        }
        elsif ($bExeOutput)
        {
            $strOutput = 'Output suppressed for testing';
        }
    }

    if (defined($strExeVar) && !defined($self->{oSite}->variableGet($strExeVar)))
    {
        $self->{oSite}->variableSet($strExeVar, '[Unset Variable]');
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => '$strCommand', value => $strCommand, trace => true},
        {name => '$strOutput', value => $strOutput, trace => true}
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
    my $strOperation = logDebugParam(OP_DOC_HTML_PAGE_PROCESS);

    # Working variables
    my $oPage = $self->{oDoc};

    # Initialize page
    my $strTitle = ${$self->{oRender}}{strProjectName} .
                   (defined($oPage->paramGet('title', false)) ? ' ' . $oPage->paramGet('title') : '');
    my $strSubTitle = $oPage->paramGet('subtitle', false);

    my $oHtmlBuilder = new BackRestDoc::Html::DocHtmlBuilder("${$self->{oRender}}{strProjectName} - Reliable PostgreSQL Backup",
                                                             $strTitle . (defined($strSubTitle) ? " - ${strSubTitle}" : ''));

    # Execute cleanup commands
    if (defined($self->{oDoc}->nodeGet('cleanup', false)))
    {
        &log(INFO, "do cleanup");

        foreach my $oExecute ($oPage->nodeGet('cleanup')->nodeList('execute'))
        {
            $self->execute($oExecute);
        }
    }

    # Generate header
    my $oPageHeader = $oHtmlBuilder->bodyGet()->addNew(HTML_DIV, 'page-header');

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
    my $oMenuBody = $oHtmlBuilder->bodyGet()->addNew(HTML_DIV, 'page-menu')->addNew(HTML_DIV, 'menu-body');

    if ($self->{strPageId} ne 'index')
    {
        my $oPage = ${$self->{oSite}->{oSite}}{page}{'index'};

        $oMenuBody->
            addNew(HTML_DIV, 'menu')->
                addNew(HTML_A, 'menu-link', {strContent => $$oPage{strMenuTitle}, strRef => '{[backrest-url-root]}'});
    }

    foreach my $strPageId(sort(keys(${$self->{oSite}->{oSite}}{page})))
    {
        if ($strPageId ne $self->{strPageId} && $strPageId ne 'index')
        {
            my $oPage = ${$self->{oSite}->{oSite}}{page}{$strPageId};

            $oMenuBody->
                addNew(HTML_DIV, 'menu')->
                    addNew(HTML_A, 'menu-link', {strContent => $$oPage{strMenuTitle}, strRef => "${strPageId}.html"});
        }
    }

    # Generate table of contents
    my $oPageTocBody;

    if (!defined($oPage->paramGet('toc', false)) || $oPage->paramGet('toc') eq 'y')
    {
        my $oPageToc = $oHtmlBuilder->bodyGet()->addNew(HTML_DIV, 'page-toc');

        $oPageToc->
            addNew(HTML_DIV, 'page-toc-title',
                   {strContent => "Table of Contents"});

        $oPageTocBody = $oPageToc->
            addNew(HTML_DIV, 'page-toc-body');
    }

    # Generate body
    my $oPageBody = $oHtmlBuilder->bodyGet()->addNew(HTML_DIV, 'page-body');

    # Render sections
    foreach my $oSection ($oPage->nodeList('section'))
    {
        my ($oChildSectionElement, $oChildSectionTocElement) =
            $self->sectionProcess($oSection, undef, 1);

        $oPageBody->add($oChildSectionElement);

        if (defined($oPageTocBody))
        {
            $oPageTocBody->add($oChildSectionTocElement);
        }
    }

    my $oPageFooter = $oHtmlBuilder->bodyGet()->
        addNew(HTML_DIV, 'page-footer',
               {strContent => ${$self->{oSite}->{oSite}}{common}{strFooter}});

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
        $iDepth
    ) =
        logDebugParam
        (
            OP_DOC_HTML_PAGE_SECTION_PROCESS, \@_,
            {name => 'oSection'},
            {name => 'strAnchor', required => false},
            {name => 'iDepth'}
        );

    &log(INFO, ('    ' x ($iDepth - 1)) . 'process section: ' . $oSection->paramGet('id'));

    if ($iDepth > 3)
    {
        confess &log(ASSERT, "section depth of ${iDepth} exceeds maximum");
    }

    # Working variables
    $strAnchor = (defined($strAnchor) ? "${strAnchor}-" : '') . $oSection->paramGet('id');
    my $oRender = $self->{oRender};

    # Create the section toc element
    my $oSectionTocElement = new BackRestDoc::Html::DocHtmlElement(HTML_DIV, "section${iDepth}-toc");

    # Create the section element
    my $oSectionElement = new BackRestDoc::Html::DocHtmlElement(HTML_DIV, "section${iDepth}");

    # Add the section anchor
    $oSectionElement->addNew(HTML_A, undef, {strId => $strAnchor});

    # Add the section title to section and toc
    my $strSectionTitle = $oRender->processText($oSection->nodeGet('title')->textGet());

    $oSectionElement->
        addNew(HTML_DIV, "section${iDepth}-title",
               {strContent => $strSectionTitle});

    my $oTocSectionTitleElement = $oSectionTocElement->
        addNew(HTML_DIV, "section${iDepth}-toc-title");

    $oTocSectionTitleElement->
        addNew(HTML_A, undef,
               {strContent => $strSectionTitle, strRef => "#${strAnchor}"});

    # Add the section intro if it exists
    if (defined($oSection->textGet(false)))
    {
        $oSectionElement->
            addNew(HTML_DIV, "section-intro",
                   {strContent => $oRender->processText($oSection->textGet())});
    }

    # Add the section body
    my $oSectionBodyElement = $oSectionElement->addNew(HTML_DIV, "section-body");

    # Process each child
    my $oSectionBodyExe;

    foreach my $oChild ($oSection->nodeList())
    {
        &log(INFO, ('    ' x $iDepth) . 'process child ' . $oChild->nameGet());

        # Execute a command
        if ($oChild->nameGet() eq 'execute-list')
        {
            my $oSectionBodyExecute = $oSectionBodyElement->addNew(HTML_DIV, "execute");
            my $bFirst = true;

            $oSectionBodyExecute->
                addNew(HTML_DIV, "execute-title",
                       {strContent => $oRender->processText($oChild->nodeGet('title')->textGet()) . ':'});

            my $oExecuteBodyElement = $oSectionBodyExecute->addNew(HTML_DIV, "execute-body");

            foreach my $oExecute ($oChild->nodeList('execute'))
            {
                my $bExeShow = defined($oExecute->fieldGet('exe-no-show', false)) ? false : true;
                my $bExeExpectedError = defined($oExecute->fieldGet('exe-err-expect', false)) ? true : false;
                my ($strCommand, $strOutput) = $self->execute($oExecute, $iDepth + 1);

                if ($bExeShow)
                {
                    $oExecuteBodyElement->
                        addNew(HTML_DIV, "execute-body-cmd" . ($bFirst ? '-first' : ''),
                               {strContent => $strCommand});

                    if (defined($strOutput))
                    {
                        my $strHighLight = $self->{oSite}->variableReplace($oExecute->fieldGet('exe-highlight', false));
                        my $bHighLightOld;
                        my $bHighLightFound = false;
                        my $strHighLightOutput;

                        foreach my $strLine (split("\n", $strOutput))
                        {
                            my $bHighLight = defined($strHighLight) && $strLine =~ /$strHighLight/;

                            if (defined($bHighLightOld) && $bHighLight != $bHighLightOld)
                            {
                                $oExecuteBodyElement->
                                    addNew(HTML_DIV, 'execute-body-output' . ($bHighLightOld ? '-highlight' : '') .
                                           ($bExeExpectedError ? '-error' : ''), {strContent => $strHighLightOutput});

                                undef($strHighLightOutput);
                            }

                            $strHighLightOutput .= "${strLine}\n";
                            $bHighLightOld = $bHighLight;

                            $bHighLightFound = $bHighLightFound ? true : $bHighLight ? true : false;
                        }

                        if (defined($bHighLightOld))
                        {
                            $oExecuteBodyElement->
                                addNew(HTML_DIV, 'execute-body-output' . ($bHighLightOld ? '-highlight' : ''),
                                       {strContent => $strHighLightOutput});

                            undef($strHighLightOutput);
                        }

                        if ($self->{bExe} && defined($strHighLight) && !$bHighLightFound)
                        {
                            confess &log(ERROR, "unable to find a match for highlight: ${strHighLight}");
                        }

                        $bFirst = true;
                    }
                }

                $bFirst = false;
            }
        }
        # Add code block
        elsif ($oChild->nameGet() eq 'code-block')
        {
            $oSectionBodyElement->
                addNew(HTML_DIV, 'code-block',
                       {strContent => $oChild->valueGet()});
        }
        # Add descriptive text
        elsif ($oChild->nameGet() eq 'p')
        {
            $oSectionBodyElement->
                addNew(HTML_DIV, 'section-body-text',
                       {strContent => $oRender->processText($oChild->textGet())});
        }
        # Add option descriptive text
        elsif ($oChild->nameGet() eq 'option-description')
        {
            my $strOption = $oChild->paramGet("key");
            my $oDescription = ${$self->{oReference}->{oConfigHash}}{&CONFIG_HELP_OPTION}{$strOption}{&CONFIG_HELP_DESCRIPTION};

            if (!defined($oDescription))
            {
                confess &log(ERROR, "unable to find ${strOption} option in sections - try adding command?");
            }

            $oSectionBodyElement->
                addNew(HTML_DIV, 'section-body-text',
                       {strContent => $oRender->processText($oDescription)});
        }
        # Add/remove backrest config options
        elsif ($oChild->nameGet() eq 'backrest-config')
        {
            $oSectionBodyElement->add($self->backrestConfigProcess($oChild, $iDepth));
        }
        # Add/remove postgres config options
        elsif ($oChild->nameGet() eq 'postgres-config')
        {
            $oSectionBodyElement->add($self->postgresConfigProcess($oChild, $iDepth));
        }
        # Add a subsection
        elsif ($oChild->nameGet() eq 'section')
        {
            my ($oChildSectionElement, $oChildSectionTocElement) =
                $self->sectionProcess($oChild, $strAnchor, $iDepth + 1);

            $oSectionBodyElement->add($oChildSectionElement);
            $oSectionTocElement->add($oChildSectionTocElement);
        }
        # Skip children that have already been processed and error on others
        elsif ($oChild->nameGet() ne 'title')
        {
            confess &log(ASSERT, 'unable to find child type ' . $oChild->nameGet());
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oSectionElement', value => $oSectionElement, trace => true},
        {name => 'oSectionTocElement', value => $oSectionTocElement, trace => true}
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
        $oConfig,
        $iDepth
    ) =
        logDebugParam
        (
            OP_DOC_HTML_PAGE_BACKREST_CONFIG_PROCESS, \@_,
            {name => 'oConfig'},
            {name => 'iDepth'}
        );

    # Get filename
    my $strFile = $self->{oSite}->variableReplace($oConfig->paramGet('file'));

    &log(INFO, ('    ' x $iDepth) . 'process backrest config: ' . $strFile);

    foreach my $oOption ($oConfig->nodeList('backrest-config-option'))
    {
        my $strSection = $oOption->fieldGet('backrest-config-option-section');
        my $strKey = $oOption->fieldGet('backrest-config-option-key');
        my $strValue = $self->{oSite}->variableReplace(trim($oOption->fieldGet('backrest-config-option-value'), false));

        if (!defined($strValue))
        {
            delete(${$self->{config}}{$strFile}{$strSection}{$strKey});

            if (keys(${$self->{config}}{$strFile}{$strSection}) == 0)
            {
                delete(${$self->{config}}{$strFile}{$strSection});
            }

            &log(INFO, ('    ' x ($iDepth + 1)) . "reset ${strSection}->${strKey}");
        }
        else
        {
            ${$self->{config}}{$strFile}{$strSection}{$strKey} = $strValue;
            &log(INFO, ('    ' x ($iDepth + 1)) . "set ${strSection}->${strKey} = ${strValue}");
        }
    }

    # Save the ini file
    executeTest("sudo chmod 777 $strFile", {bSuppressError => true});
    iniSave($strFile, $self->{config}{$strFile}, true);

    # Generate config element
    my $oConfigElement = new BackRestDoc::Html::DocHtmlElement(HTML_DIV, "config");

    $oConfigElement->
        addNew(HTML_DIV, "config-title",
               {strContent => $self->{oRender}->processText($oConfig->nodeGet('title')->textGet()) . ':'});

    my $oConfigBodyElement = $oConfigElement->addNew(HTML_DIV, "config-body");

    $oConfigBodyElement->
        addNew(HTML_DIV, "config-body-title",
               {strContent => "${strFile}:"});

    $oConfigBodyElement->
        addNew(HTML_DIV, "config-body-output",
               {strContent => fileStringRead($strFile)});

    executeTest("sudo chown postgres:postgres $strFile");
    executeTest("sudo chmod 640 $strFile");

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
        $oConfig,
        $iDepth
    ) =
        logDebugParam
        (
            OP_DOC_HTML_PAGE_POSTGRES_CONFIG_PROCESS, \@_,
            {name => 'oConfig'},
            {name => 'iDepth'}
        );

    # Get filename
    my $strFile = $self->{oSite}->variableReplace($oConfig->paramGet('file'));

    if (!defined(${$self->{'pg-config'}}{$strFile}{base}))
    {
        ${$self->{'pg-config'}}{$strFile}{base} = fileStringRead($strFile);
    }

    my $oConfigHash = $self->{'pg-config'}{$strFile};
    my $oConfigHashNew;

    if (!defined($$oConfigHash{old}))
    {
        $oConfigHashNew = {};
        $$oConfigHash{old} = {}
    }
    else
    {
        $oConfigHashNew = dclone($$oConfigHash{old});
    }

    &log(INFO, ('    ' x $iDepth) . 'process postgres config: ' . $strFile);

    foreach my $oOption ($oConfig->nodeList('postgres-config-option'))
    {
        my $strKey = $oOption->paramGet('key');
        my $strValue = $self->{oSite}->variableReplace(trim($oOption->valueGet()));

        if ($strValue eq '')
        {
            delete($$oConfigHashNew{$strKey});

            &log(INFO, ('    ' x ($iDepth + 1)) . "reset ${strKey}");
        }
        else
        {
            $$oConfigHashNew{$strKey} = $strValue;
            &log(INFO, ('    ' x ($iDepth + 1)) . "set ${strKey} = ${strValue}");
        }
    }

    # Generate config text
    my $strConfig;

    foreach my $strKey (sort(keys(%$oConfigHashNew)))
    {
        if (defined($strConfig))
        {
            $strConfig .= "\n";
        }

        $strConfig .= "${strKey} = $$oConfigHashNew{$strKey}";
    }

    # Save the conf file
    executeTest("sudo chown vagrant $strFile");

    fileStringWrite($strFile, $$oConfigHash{base} .
                    (defined($strConfig) ? "\n# pgBackRest Configuration\n${strConfig}" : ''));

    executeTest("sudo chown postgres $strFile");

    # Generate config element
    my $oConfigElement = new BackRestDoc::Html::DocHtmlElement(HTML_DIV, "config");

    $oConfigElement->
        addNew(HTML_DIV, "config-title",
               {strContent => $self->{oRender}->processText($oConfig->nodeGet('title')->textGet()) . ':'});

    my $oConfigBodyElement = $oConfigElement->addNew(HTML_DIV, "config-body");

    $oConfigBodyElement->
        addNew(HTML_DIV, "config-body-title",
               {strContent => "append to ${strFile}:"});

    $oConfigBodyElement->
        addNew(HTML_DIV, "config-body-output",
               {strContent => defined($strConfig) ? $strConfig : '<No PgBackRest Settings>'});

    $$oConfigHash{old} = $oConfigHashNew;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oConfigElement', value => $oConfigElement, trace => true}
    );
}

1;
