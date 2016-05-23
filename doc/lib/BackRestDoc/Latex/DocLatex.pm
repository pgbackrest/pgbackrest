####################################################################################################################################
# DOC LATEX MODULE
####################################################################################################################################
package BackRestDoc::Latex::DocLatex;

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
use BackRestDoc::Latex::DocLatexSection;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_DOC_LATEX                                           => 'DocLatex';

use constant OP_DOC_LATEX_NEW                                       => OP_DOC_LATEX . '->new';
use constant OP_DOC_LATEX_PROCESS                                   => OP_DOC_LATEX . '->process';

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
        $self->{strLatexPath},
        $self->{strPreambleFile},
        $self->{bExe}
    ) =
        logDebugParam
        (
            OP_DOC_LATEX_NEW, \@_,
            {name => 'oManifest'},
            {name => 'strXmlPath'},
            {name => 'strLatexPath'},
            {name => 'strPreambleFile'},
            {name => 'bExe'}
        );

    # Remove the current html path if it exists
    if (-e $self->{strLatexPath})
    {
        executeTest("rm -rf $self->{strLatexPath}/*");
    }
    # Else create the html path
    else
    {
        mkdir($self->{strLatexPath})
            or confess &log(ERROR, "unable to create path $self->{strLatexPath}");
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
    my $strOperation = logDebugParam(OP_DOC_LATEX_PROCESS);

    my $oRender = $self->{oManifest}->renderGet(RENDER_TYPE_PDF);

    # Copy the logo
    copy('doc/resource/latex/cds-logo.eps', "$self->{strLatexPath}/logo.eps")
        or confess &log(ERROR, "unable to copy logo");

    my $strLatex = $self->{oManifest}->variableReplace(fileStringRead($self->{strPreambleFile}), 'latex') . "\n";

    foreach my $strPageId ($self->{oManifest}->renderOutList(RENDER_TYPE_PDF))
    {
        &log(INFO, "    render out: ${strPageId}");

        my $oDocLatexSection =
            new BackRestDoc::Latex::DocLatexSection($self->{oManifest}, $strPageId, $self->{bExe});

        # Save the html page
        $strLatex .= $oDocLatexSection->process();
    }

    $strLatex .= "\n% " . ('-' x 130) . "\n% End document\n% " . ('-' x 130) . "\n\\end{document}\n";

    my $strLatexFileName = $self->{oManifest}->variableReplace("$self->{strLatexPath}/" . $$oRender{file} . '.tex');

    fileStringWrite($strLatexFileName, $strLatex, false);

    executeTest("pdflatex -output-directory=$self->{strLatexPath} -shell-escape $strLatexFileName",
                {bSuppressStdErr => true});
    executeTest("pdflatex -output-directory=$self->{strLatexPath} -shell-escape $strLatexFileName",
                {bSuppressStdErr => true});

    # Return from function and log return values if any
    logDebugReturn($strOperation);
}

1;
