####################################################################################################################################
# DOC HTML BUILDER MODULE
####################################################################################################################################
package BackRestDoc::Html::DocHtmlBuilder;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);
use File::Copy;

use lib dirname($0) . '/../lib';
use BackRest::Common::Log;
use BackRest::Common::String;

use BackRestDoc::Html::DocHtmlElement;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_DOC_HTML_BUILDER                                    => 'DocHtmlBuilder';

use constant OP_DOC_HTML_BUILDER_NEW                                => OP_DOC_HTML_BUILDER . '->new';
use constant OP_DOC_HTML_BUILDER_HTML_GET                           => OP_DOC_HTML_BUILDER . '->htmlGet';
use constant OP_DOC_HTML_BUILDER_HTML_RENDER                        => OP_DOC_HTML_BUILDER . '->htmlRender';

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
        $self->{strName},
        $self->{strTitle}
    ) =
        logDebugParam
        (
            OP_DOC_HTML_BUILDER_NEW, \@_,
            {name => 'strName'},
            {name => 'strTitle'}
        );

    $self->{oBody} = new BackRestDoc::Html::DocHtmlElement(HTML_BODY);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# indent
#
# Indent html
####################################################################################################################################
sub indent
{
    my $self = shift;
    my $iDepth = shift;

    return ('  ' x $iDepth);
}

####################################################################################################################################
# lf
#
# Add a linefeed.
####################################################################################################################################
sub lf
{
    my $self = shift;

    return "\n";
}

####################################################################################################################################
# bodyGet
#
# Get the body element.
####################################################################################################################################
sub bodyGet
{
    my $self = shift;

    return $self->{oBody};
}

####################################################################################################################################
# htmlRender
#
# Render each html element.
####################################################################################################################################
sub htmlRender
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my (
        $strOperation,
        $oElement,
        $iDepth
    ) =
        logDebugParam
        (
            OP_DOC_HTML_BUILDER_HTML_RENDER, \@_,
            {name => 'oElement'},
            {name => 'iDepth'}
        );

    # Build the header
    my $strHtml =
        $self->indent($iDepth) . "<$oElement->{strType}" .
            (defined($oElement->{strClass}) ? " class=\"$oElement->{strClass}\"": '') .
            (defined($oElement->{strRef}) ? " href=\"$oElement->{strRef}\"": '') .
            (defined($oElement->{strId}) ? " id=\"$oElement->{strId}\"": '') . '>';

    if (defined($oElement->{strContent}))
    {
        $oElement->{strContent} =~ s/\n/\<br\/>\n/g;
        $strHtml .= $self->lf() . trim($oElement->{strContent}) . $self->lf() . $self->indent($iDepth);
    }
    else
    {
        if (!($oElement->{strType} eq HTML_A && @{$oElement->{oyElement}} == 0))
        {
            $strHtml .= $self->lf();
        }

        foreach my $oChildElement (@{$oElement->{oyElement}})
        {
            $strHtml .= $self->htmlRender($oChildElement, $iDepth + 1);
        }

        if (!($oElement->{strType} eq HTML_A && @{$oElement->{oyElement}} == 0))
        {
            $strHtml .= $self->indent($iDepth);
        }
    }

    $strHtml .= "</$oElement->{strType}>" . $self->lf();

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strHtml', value => $strHtml, trace => true}
    );
}

####################################################################################################################################
# htmlGet
#
# Generate the HTML.
####################################################################################################################################
sub htmlGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(OP_DOC_HTML_BUILDER_HTML_GET);

    # Build the header
    my $strHtml =
        $self->indent(0) . "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\"" .
                           " \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">" . $self->lf() .
        $self->indent(0) . "<html xmlns=\"http://www.w3.org/1999/xhtml\">" . $self->lf() .
        $self->indent(0) . "<head>" . $self->lf() .
            $self->indent(1) . "<title>$self->{strTitle}</title>" . $self->lf() .
            $self->indent(1) . "<link rel=\"stylesheet\" href=\"default.css\" type=\"text/css\"></link>" . $self->lf() .
            # $self->indent(1) . "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"></meta>" . $self->lf() .
            $self->indent(1) . "<meta http-equiv=\"Content-Type\" content=\"text/html;charset=utf-8\"></meta>" . $self->lf() .
            $self->indent(1) . "<meta name=\"og:site_name\" content=\"$self->{strName}\"></meta>" . $self->lf() .
            $self->indent(1) . "<meta name=\"og:title\" content=\"$self->{strTitle}\"></meta>" . $self->lf() .
            $self->indent(1) . "<meta name=\"og:image\" content=\"favicon.png\"></meta>" . $self->lf() .
        $self->indent(0) . "</head>" . $self->lf();

    $strHtml .= $self->htmlRender($self->bodyGet(), 0);

    # Complete the html
    $strHtml .=
        $self->indent(0) . "</html>" . $self->lf();

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strHtml', value => $strHtml, trace => true}
    );
}

1;
