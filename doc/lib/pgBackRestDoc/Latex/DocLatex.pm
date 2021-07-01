####################################################################################################################################
# DOC LATEX MODULE
####################################################################################################################################
package pgBackRestDoc::Latex::DocLatex;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Cwd qw(abs_path);
use Data::Dumper;
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname basename);
use File::Copy;
use POSIX qw(strftime);
use Storable qw(dclone);

use pgBackRestTest::Common::ExecuteTest;

use pgBackRestDoc::Common::DocConfig;
use pgBackRestDoc::Common::DocManifest;
use pgBackRestDoc::Common::Exception;
use pgBackRestDoc::Common::Log;
use pgBackRestDoc::Common::String;
use pgBackRestDoc::Latex::DocLatexSection;
use pgBackRestDoc::ProjectInfo;

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

    my $strLogo = $self->{oManifest}->variableGet('pdf-resource-logo');

    if (!defined($strLogo))
    {
        $strLogo = 'blank.eps';
    }

    my ($strExt) = $strLogo =~ /(\.[^.]+)$/;
    my $strLogoPath = defined($self->{oManifest}->variableGet('pdf-resource-path')) ?
        $self->{oManifest}->variableGet('pdf-resource-path') :
        "$self->{oManifest}{strDocPath}/resource/latex/";

    # Copy the logo
    copy($strLogoPath . $strLogo, "$self->{strLatexPath}/logo$strExt")
        or confess &log(ERROR, "unable to copy logo");

    my $strLatex = $self->{oManifest}->variableReplace(
        ${$self->{oManifest}->storage()->get($self->{strPreambleFile})}, 'latex') . "\n";

    # ??? Temp hack for underscores in filename
    $strLatex =~ s/pgaudit\\\_doc/pgaudit\_doc/g;

    # Process the sources in the order listed in the manifest.xml
    foreach my $strPageId (@{${$self->{oManifest}->renderGet(RENDER_TYPE_PDF)}{stryOrder}})
    {
        &log(INFO, "    render out: ${strPageId}");

        eval
        {
            my $oDocLatexSection =
                new pgBackRestDoc::Latex::DocLatexSection($self->{oManifest}, $strPageId, $self->{bExe});

            # Save the html page
            $strLatex .= $oDocLatexSection->process();

            return true;
        }
        or do
        {
            my $oException = $EVAL_ERROR;

            if (exceptionCode($oException) == ERROR_FILE_INVALID)
            {
                my $oRenderOut = $self->{oManifest}->renderOutGet(RENDER_TYPE_HTML, $strPageId);
                $self->{oManifest}->cacheReset($$oRenderOut{source});

                my $oDocLatexSection =
                    new pgBackRestDoc::Latex::DocLatexSection($self->{oManifest}, $strPageId, $self->{bExe});

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

    # Get base name of output file to use for processing
    (my $strLatexFileBase = basename($$oRender{file})) =~ s/\.[^.]+$//;
    $strLatexFileBase = $self->{oManifest}->variableReplace($strLatexFileBase);

    # Name of latex file to use for output and processing
    my $strLatexFileName = $self->{oManifest}->variableReplace("$self->{strLatexPath}/" . $strLatexFileBase . '.tex');

    # Output latex and build PDF
    $self->{oManifest}->storage()->put($strLatexFileName, $strLatex);

    executeTest("pdflatex -output-directory=$self->{strLatexPath} -shell-escape $strLatexFileName",
                {bSuppressStdErr => true});
    executeTest("pdflatex -output-directory=$self->{strLatexPath} -shell-escape $strLatexFileName",
                {bSuppressStdErr => true});

    # Determine path of output file
    my $strLatexOutputName = $oRender->{file};

    if ($strLatexOutputName !~ /^\//)
    {
        $strLatexOutputName = abs_path($self->{strLatexPath} . "/" . $oRender->{file});
    }

    # Copy pdf file if it is not already in the correct place
    if ($strLatexOutputName ne "$self->{strLatexPath}/" . $strLatexFileBase . '.pdf')
    {
        copy("$self->{strLatexPath}/" . $strLatexFileBase . '.pdf', $strLatexOutputName)
            or confess &log(ERROR, "unable to copy pdf to " . $strLatexOutputName);
    }

    # Return from function and log return values if any
    logDebugReturn($strOperation);
}

1;
