####################################################################################################################################
# DOC LATEX MODULE
####################################################################################################################################
package BackRestDoc::Latex::DocLatex;

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

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::FileCommon;
use pgBackRest::Version;

use pgBackRestTest::Common::ExecuteTest;

use BackRestDoc::Common::DocConfig;
use BackRestDoc::Common::DocManifest;
use BackRestDoc::Latex::DocLatexSection;

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
            __PACKAGE__ . '->new', \@_,
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
    my $strOperation = logDebugParam(__PACKAGE__ . '->process');

    my $oRender = $self->{oManifest}->renderGet(RENDER_TYPE_PDF);

    # Copy the logo
    copy("$self->{oManifest}{strDocPath}/resource/latex/cds-logo.eps", "$self->{strLatexPath}/logo.eps")
        or confess &log(ERROR, "unable to copy logo");

    my $strLatex = $self->{oManifest}->variableReplace(fileStringRead($self->{strPreambleFile}), 'latex') . "\n";

    # !!! Temp hack for underscores in filename
    $strLatex =~ s/pgaudit\\\_doc/pgaudit\_doc/g;

    foreach my $strPageId ($self->{oManifest}->renderOutList(RENDER_TYPE_PDF))
    {
        &log(INFO, "    render out: ${strPageId}");

        eval
        {
            my $oDocLatexSection =
                new BackRestDoc::Latex::DocLatexSection($self->{oManifest}, $strPageId, $self->{bExe});

            # Save the html page
            $strLatex .= $oDocLatexSection->process();

            return true;
        }
        or do
        {
            my $oException = $EVAL_ERROR;

            if (isException($oException) && $oException->code() == ERROR_FILE_INVALID)
            {
                my $oRenderOut = $self->{oManifest}->renderOutGet(RENDER_TYPE_HTML, $strPageId);
                $self->{oManifest}->cacheReset($$oRenderOut{source});

                my $oDocLatexSection =
                    new BackRestDoc::Latex::DocLatexSection($self->{oManifest}, $strPageId, $self->{bExe});

                # Save the html page
                $strLatex .= $oDocLatexSection->process();
            }
            else
            {
                confess $oException;
            }
        };
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
