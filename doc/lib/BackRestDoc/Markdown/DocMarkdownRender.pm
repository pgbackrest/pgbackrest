####################################################################################################################################
# DOC MARKDOWN RENDER MODULE
####################################################################################################################################
package BackRestDoc::Markdown::DocMarkdownRender;
use parent 'BackRestDoc::Common::DocExecute';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Data::Dumper;
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);
use File::Copy;
use Storable qw(dclone);

use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Config::ConfigHelp;

use BackRestDoc::Common::DocManifest;

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

    # my $oHtmlBuilder = new BackRestDoc::Html::DocHtmlBuilder("{[project]} - Reliable PostgreSQL Backup",
    #                                                          $strTitle . (defined($strSubTitle) ? " - ${strSubTitle}" : ''),
    #                                                          $self->{bPretty});
    #
    # # Generate header
    # my $oPageHeader = $oHtmlBuilder->bodyGet()->addNew(HTML_DIV, 'page-header');
    #
    # $oPageHeader->
    #     addNew(HTML_DIV, 'page-header-title',
    #            {strContent => $strTitle});
    #
    # if (defined($strSubTitle))
    # {
    #     $oPageHeader->
    #         addNew(HTML_DIV, 'page-header-subtitle',
    #                {strContent => $strSubTitle});
    # }
    #
    # # Generate menu
    # my $oMenuBody = $oHtmlBuilder->bodyGet()->addNew(HTML_DIV, 'page-menu')->addNew(HTML_DIV, 'menu-body');
    #
    # if ($self->{strRenderOutKey} ne 'index')
    # {
    #     my $oRenderOut = $self->{oManifest}->renderOutGet(RENDER_TYPE_HTML, 'index');
    #
    #     $oMenuBody->
    #         addNew(HTML_DIV, 'menu')->
    #             addNew(HTML_A, 'menu-link', {strContent => $$oRenderOut{menu}, strRef => '{[project-url-root]}'});
    # }
    #
    # foreach my $strRenderOutKey ($self->{oManifest}->renderOutList(RENDER_TYPE_HTML))
    # {
    #     if ($strRenderOutKey ne $self->{strRenderOutKey} && $strRenderOutKey ne 'index')
    #     {
    #         my $oRenderOut = $self->{oManifest}->renderOutGet(RENDER_TYPE_HTML, $strRenderOutKey);
    #
    #         $oMenuBody->
    #             addNew(HTML_DIV, 'menu')->
    #                 addNew(HTML_A, 'menu-link', {strContent => $$oRenderOut{menu}, strRef => "${strRenderOutKey}.html"});
    #     }
    # }
    #
    # # Generate table of contents
    # my $oPageTocBody;
    #
    # if (!defined($oPage->paramGet('toc', false)) || $oPage->paramGet('toc') eq 'y')
    # {
    #     my $oPageToc = $oHtmlBuilder->bodyGet()->addNew(HTML_DIV, 'page-toc');
    #
    #     $oPageToc->
    #         addNew(HTML_DIV, 'page-toc-title',
    #                {strContent => "Table of Contents"});
    #
    #     $oPageTocBody = $oPageToc->
    #         addNew(HTML_DIV, 'page-toc-body');
    # }
    #
    # # Generate body
    # my $oPageBody = $oHtmlBuilder->bodyGet()->addNew(HTML_DIV, 'page-body');

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

    &log($iDepth == 1 ? INFO : DEBUG, ('    ' x ($iDepth + 1)) . 'process section: ' . $oSection->paramGet('id'));

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
            # my $oSectionBodyExecute = $oSectionBodyElement->addNew(HTML_DIV, "execute");
            # my $bFirst = true;
            # my $strHostName = $self->{oManifest}->variableReplace($oChild->paramGet('host'));
            #
            # $oSectionBodyExecute->
            #     addNew(HTML_DIV, "execute-title",
            #            {strContent => "<span class=\"host\">${strHostName}</span> <b>&#x21d2;</b> " .
            #                           $self->processText($oChild->nodeGet('title')->textGet())});
            #
            # my $oExecuteBodyElement = $oSectionBodyExecute->addNew(HTML_DIV, "execute-body");
            #
            # foreach my $oExecute ($oChild->nodeList('execute'))
            # {
            #     my $bExeShow = !$oExecute->paramTest('show', 'n');
            #     my $bExeExpectedError = defined($oExecute->paramGet('err-expect', false));
            #
            #     my ($strCommand, $strOutput) = $self->execute($oSection, $strHostName, $oExecute, $iDepth + 3);
            #
            #     if ($bExeShow)
            #     {
            #         # Add continuation chars and proper spacing
            #         $strCommand =~ s/\n/\n   /smg;
            #
            #         $oExecuteBodyElement->
            #             addNew(HTML_PRE, "execute-body-cmd",
            #                    {strContent => $strCommand, bPre => true});
            #
            #         my $strHighLight = $self->{oManifest}->variableReplace($oExecute->fieldGet('exe-highlight', false));
            #         my $bHighLightFound = false;
            #
            #         if (defined($strOutput))
            #         {
            #             my $bHighLightOld;
            #             my $strHighLightOutput;
            #
            #             if ($oExecute->fieldTest('exe-highlight-type', 'error'))
            #             {
            #                 $bExeExpectedError = true;
            #             }
            #
            #             foreach my $strLine (split("\n", $strOutput))
            #             {
            #                 my $bHighLight = defined($strHighLight) && $strLine =~ /$strHighLight/;
            #
            #                 if (defined($bHighLightOld) && $bHighLight != $bHighLightOld)
            #                 {
            #                     $oExecuteBodyElement->
            #                         addNew(HTML_PRE, 'execute-body-output' .
            #                                ($bHighLightOld ? '-highlight' . ($bExeExpectedError ? '-error' : '') : ''),
            #                                {strContent => $strHighLightOutput, bPre => true});
            #
            #                     undef($strHighLightOutput);
            #                 }
            #
            #                 $strHighLightOutput .= (defined($strHighLightOutput) ? "\n" : '') . $strLine;
            #                 $bHighLightOld = $bHighLight;
            #
            #                 $bHighLightFound = $bHighLightFound ? true : $bHighLight ? true : false;
            #             }
            #
            #             if (defined($bHighLightOld))
            #             {
            #                 $oExecuteBodyElement->
            #                     addNew(HTML_PRE, 'execute-body-output' .
            #                            ($bHighLightOld ? '-highlight' . ($bExeExpectedError ? '-error' : '') : ''),
            #                            {strContent => $strHighLightOutput, bPre => true});
            #             }
            #
            #             $bFirst = true;
            #         }
            #
            #         if ($self->{bExe} && $self->isRequired($oSection) && defined($strHighLight) && !$bHighLightFound)
            #         {
            #             confess &log(ERROR, "unable to find a match for highlight: ${strHighLight}");
            #         }
            #     }
            #
            #     $bFirst = false;
            # }
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

            $strMarkdown .= "\n```\n" . trim($oChild->valueGet()) . "\n```";
        }
        # Add descriptive text
        elsif ($oChild->nameGet() eq 'p')
        {
            if (defined($strLastChild) && $strLastChild ne 'code-block')
            {
                $strMarkdown .= "\n";
            }

            $strMarkdown .= "\n" . $self->processText($oChild->textGet());
        }
        # Add option descriptive text
        elsif ($oChild->nameGet() eq 'option-description')
        {
            # my $strOption = $oChild->paramGet("key");
            # my $oDescription = ${$self->{oReference}->{oConfigHash}}{&CONFIG_HELP_OPTION}{$strOption}{&CONFIG_HELP_DESCRIPTION};
            #
            # if (!defined($oDescription))
            # {
            #     confess &log(ERROR, "unable to find ${strOption} option in sections - try adding command?");
            # }
            #
            # $oSectionBodyElement->
            #     addNew(HTML_DIV, 'section-body-text',
            #            {strContent => $self->processText($oDescription)});
        }
        # Add/remove backrest config options
        elsif ($oChild->nameGet() eq 'backrest-config')
        {
            # my $oConfigElement = $self->backrestConfigProcess($oSection, $oChild, $iDepth + 3);
            #
            # if (defined($oConfigElement))
            # {
            #     $oSectionBodyElement->add($oConfigElement);
            # }
        }
        # Add/remove postgres config options
        elsif ($oChild->nameGet() eq 'postgres-config')
        {
            # my $oConfigElement = $self->postgresConfigProcess($oSection, $oChild, $iDepth + 3);
            #
            # if (defined($oConfigElement))
            # {
            #     $oSectionBodyElement->add($oConfigElement);
            # }
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
        # Check if the child can be processed by a parent
        else
        {
            # $self->sectionChildProcess($oSection, $oChild, $iDepth + 1);
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

    # # Generate the config
    # my $oConfigElement;
    # my ($strFile, $strConfig, $bShow) = $self->backrestConfig($oSection, $oConfig, $iDepth);
    #
    # if ($bShow)
    # {
    #     my $strHostName = $self->{oManifest}->variableReplace($oConfig->paramGet('host'));
    #
    #     # Render the config
    #     $oConfigElement = new BackRestDoc::Html::DocHtmlElement(HTML_DIV, "config");
    #
    #     $oConfigElement->
    #         addNew(HTML_DIV, "config-title",
    #                {strContent => "<span class=\"host\">${strHostName}</span>:<span class=\"file\">${strFile}</span>" .
    #                               " <b>&#x21d2;</b> " . $self->processText($oConfig->nodeGet('title')->textGet())});
    #
    #     my $oConfigBodyElement = $oConfigElement->addNew(HTML_DIV, "config-body");
    #     #
    #     # $oConfigBodyElement->
    #     #     addNew(HTML_DIV, "config-body-title",
    #     #            {strContent => "${strFile}:"});
    #
    #     $oConfigBodyElement->
    #         addNew(HTML_DIV, "config-body-output",
    #                {strContent => $strConfig});
    # }
    #
    # # Return from function and log return values if any
    # return logDebugReturn
    # (
    #     $strOperation,
    #     {name => 'oConfigElement', value => $oConfigElement, trace => true}
    # );
}

####################################################################################################################################
# postgresConfigProcess
####################################################################################################################################
sub postgresConfigProcess
{
    my $self = shift;

    # # Assign function parameters, defaults, and log debug info
    # my
    # (
    #     $strOperation,
    #     $oSection,
    #     $oConfig,
    #     $iDepth
    # ) =
    #     logDebugParam
    #     (
    #         __PACKAGE__ . '->postgresConfigProcess', \@_,
    #         {name => 'oSection'},
    #         {name => 'oConfig'},
    #         {name => 'iDepth'}
    #     );
    #
    # # Generate the config
    # my $oConfigElement;
    # my ($strFile, $strConfig, $bShow) = $self->postgresConfig($oSection, $oConfig, $iDepth);
    #
    # if ($bShow)
    # {
    #     # Render the config
    #     my $strHostName = $self->{oManifest}->variableReplace($oConfig->paramGet('host'));
    #     $oConfigElement = new BackRestDoc::Html::DocHtmlElement(HTML_DIV, "config");
    #
    #     $oConfigElement->
    #         addNew(HTML_DIV, "config-title",
    #                {strContent => "<span class=\"host\">${strHostName}</span>:<span class=\"file\">${strFile}</span>" .
    #                               " <b>&#x21d2;</b> " . $self->processText($oConfig->nodeGet('title')->textGet())});
    #
    #     my $oConfigBodyElement = $oConfigElement->addNew(HTML_DIV, "config-body");
    #
    #     # $oConfigBodyElement->
    #     #     addNew(HTML_DIV, "config-body-title",
    #     #            {strContent => "append to ${strFile}:"});
    #
    #     $oConfigBodyElement->
    #         addNew(HTML_DIV, "config-body-output",
    #                {strContent => defined($strConfig) ? $strConfig : '<No PgBackRest Settings>'});
    #
    #     $oConfig->fieldSet('actual-config', $strConfig);
    # }
    #
    # # Return from function and log return values if any
    # return logDebugReturn
    # (
    #     $strOperation,
    #     {name => 'oConfigElement', value => $oConfigElement, trace => true}
    # );
}

1;
