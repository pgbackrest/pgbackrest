####################################################################################################################################
# DOC HTML SITE MODULE
####################################################################################################################################
package BackRestDoc::Html::DocHtmlSite;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Data::Dumper;
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);
use File::Copy;
use POSIX qw(strftime);
use Storable qw(dclone);

use lib dirname($0) . '/../lib';
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::FileCommon;
use pgBackRest::Version;

use lib dirname($0) . '/../test/lib';
use pgBackRestTest::Common::ExecuteTest;

use BackRestDoc::Common::DocConfig;
use BackRestDoc::Common::DocManifest;
use BackRestDoc::Html::DocHtmlPage;

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

    # Remove the current html path if it exists
    if (-e $self->{strHtmlPath})
    {
        executeTest("rm -rf $self->{strHtmlPath}/*");
    }
    # Else create the html path
    else
    {
        mkdir($self->{strHtmlPath})
            or confess &log(ERROR, "unable to create path $self->{strHtmlPath}");
    }

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

    foreach my $strPageId ($self->{oManifest}->renderOutList(RENDER_TYPE_HTML))
    {
        &log(INFO, "    render out: ${strPageId}");

        # Save the html page
        fileStringWrite("$self->{strHtmlPath}/${strPageId}.html",
                        $self->{oManifest}->variableReplace((new BackRestDoc::Html::DocHtmlPage($self->{oManifest},
                            $strPageId, $self->{bExe}))->process()),
                        false);
    }

    # Return from function and log return values if any
    logDebugReturn($strOperation);
}

1;
