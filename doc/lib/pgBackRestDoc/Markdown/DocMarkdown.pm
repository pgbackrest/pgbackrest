####################################################################################################################################
# DOC MARKDOWN MODULE
####################################################################################################################################
package pgBackRestDoc::Markdown::DocMarkdown;

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

use pgBackRest::Version;

use pgBackRestTest::Common::ExecuteTest;

use pgBackRestDoc::Common::DocConfig;
use pgBackRestDoc::Common::DocManifest;
use pgBackRestDoc::Common::Log;
use pgBackRestDoc::Common::String;
use pgBackRestDoc::Markdown::DocMarkdownRender;

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
        $self->{strMarkdownPath},
        $self->{bExe}
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'oManifest'},
            {name => 'strXmlPath'},
            {name => 'strMarkdownPath'},
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

    foreach my $strRenderOutId ($self->{oManifest}->renderOutList(RENDER_TYPE_MARKDOWN))
    {
        my $oRenderOut = $self->{oManifest}->renderOutGet(RENDER_TYPE_MARKDOWN, $strRenderOutId);
        my $strFile = "$self->{strMarkdownPath}/" . (defined($$oRenderOut{file}) ? $$oRenderOut{file} : "${strRenderOutId}.md");

        &log(INFO, "    render out: ${strRenderOutId}");

        # Save the html page
        $self->{oManifest}->storage()->put(
            $strFile, $self->{oManifest}->variableReplace((new pgBackRestDoc::Markdown::DocMarkdownRender($self->{oManifest},
            $strRenderOutId, $self->{bExe}))->process()));
    }

    # Return from function and log return values if any
    logDebugReturn($strOperation);
}

1;
