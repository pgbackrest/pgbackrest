####################################################################################################################################
# DOC HTML SITE MODULE
####################################################################################################################################
package pgBackRestDoc::Html::DocHtmlSite;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Data::Dumper;
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);
use File::Copy;
use POSIX qw(strftime);
use Storable qw(dclone);

use pgBackRest::Version;

use pgBackRestTest::Common::ExecuteTest;

use pgBackRestDoc::Common::DocConfig;
use pgBackRestDoc::Common::DocManifest;
use pgBackRestDoc::Common::Exception;
use pgBackRestDoc::Common::Log;
use pgBackRestDoc::Common::String;
use pgBackRestDoc::Html::DocHtmlPage;

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
        $self->{oManifest},
        $self->{strXmlPath},
        $self->{strHtmlPath},
        $self->{strCssFile},
        $self->{strFaviconFile},
        $self->{strProjectLogoFile},
        $self->{bExe}
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'oManifest'},
            {name => 'strXmlPath'},
            {name => 'strHtmlPath'},
            {name => 'strCssFile'},
            {name => 'strFaviconFile', required => false},
            {name => 'strProjectLogoFile', required => false},
            {name => 'bExe'}
        );

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

    # Get render options
    my $oRender = $self->{oManifest}->renderGet(RENDER_TYPE_HTML);

    my $bMenu = $$oRender{&RENDER_MENU};
    my $bPretty = $$oRender{&RENDER_PRETTY};
    my $bCompact = $$oRender{&RENDER_COMPACT};

    if (!$bCompact)
    {
        # Copy the css file
        my $strCssFileDestination = "$self->{strHtmlPath}/default.css";
        copy($self->{strCssFile}, $strCssFileDestination)
            or confess &log(ERROR, "unable to copy $self->{strCssFile} to ${strCssFileDestination}");

        # Copy the favicon file
        if (defined($self->{strFaviconFile}))
        {
            my $strFaviconFileDestination = "$self->{strHtmlPath}/" . $self->{oManifest}->variableGet('project-favicon');
            copy($self->{strFaviconFile}, $strFaviconFileDestination)
                or confess &log(ERROR, "unable to copy $self->{strFaviconFile} to ${strFaviconFileDestination}");
        }

        # Copy the project logo file
        if (defined($self->{strProjectLogoFile}))
        {
            my $strProjectLogoFileDestination = "$self->{strHtmlPath}/" . $self->{oManifest}->variableGet('project-logo');
            copy($self->{strProjectLogoFile}, $strProjectLogoFileDestination)
                or confess &log(ERROR, "unable to copy $self->{strProjectLogoFile} to ${strProjectLogoFileDestination}");
        }
    }

    foreach my $strPageId ($self->{oManifest}->renderOutList(RENDER_TYPE_HTML))
    {
        &log(INFO, "    render out: ${strPageId}");

        my $strHtml;
        my $oRenderOut = $self->{oManifest}->renderOutGet(RENDER_TYPE_HTML, $strPageId);

        eval
        {
            $strHtml = $self->{oManifest}->variableReplace(
                new pgBackRestDoc::Html::DocHtmlPage(
                    $self->{oManifest}, $strPageId, $bMenu, $self->{bExe}, $bCompact,
                    ${$self->{oManifest}->storage()->get($self->{strCssFile})}, $bPretty)->process());

            return true;
        }
        or do
        {
            my $oException = $@;

            if (exceptionCode($oException) == ERROR_FILE_INVALID)
            {
                my $oRenderOut = $self->{oManifest}->renderOutGet(RENDER_TYPE_HTML, $strPageId);
                $self->{oManifest}->cacheReset($$oRenderOut{source});

                $strHtml = $self->{oManifest}->variableReplace(
                    new pgBackRestDoc::Html::DocHtmlPage(
                        $self->{oManifest}, $strPageId, $bMenu, $self->{bExe}, $bCompact,
                        ${$self->{oManifest}->storage()->get($self->{strCssFile})}, $bPretty)->process());
            }
            else
            {
                confess $oException;
            }
        };

        # Save the html page
        my $strFile = "$self->{strHtmlPath}/" . (defined($$oRenderOut{file}) ? $$oRenderOut{file} : "${strPageId}.html");
        $self->{oManifest}->storage()->put($strFile, $strHtml);
    }

    # Return from function and log return values if any
    logDebugReturn($strOperation);
}

1;
