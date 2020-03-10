####################################################################################################################################
# DOC HTML BUILDER MODULE
####################################################################################################################################
package BackRestDoc::Html::DocHtmlBuilder;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();

use BackRestDoc::Common::Log;
use BackRestDoc::Common::String;
use BackRestDoc::Html::DocHtmlElement;

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
        $self->{strTitle},
        $self->{strFavicon},
        $self->{strLogo},
        $self->{strDescription},
        $self->{bPretty},
        $self->{bCompact},
        $self->{strCss},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strName'},
            {name => 'strTitle'},
            {name => 'strFavicon', required => false},
            {name => 'strLogo', required => false},
            {name => 'strDescription', required => false},
            {name => 'bPretty', default => false},
            {name => 'bCompact', default => false},
            {name => 'strCss', required => false},
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

    return $self->{bPretty} ? ('  ' x $iDepth) : '';
}

####################################################################################################################################
# lf
#
# Add a linefeed.
####################################################################################################################################
sub lf
{
    my $self = shift;

    return $self->{bPretty} ? "\n" : '';
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
            __PACKAGE__ . '->htmlRender', \@_,
            {name => 'oElement', trace => true},
            {name => 'iDepth', trace => true}
        );

    # Build the header
    my $strHtml =
        $self->indent($iDepth) . "<$oElement->{strType}" .
            (defined($oElement->{strClass}) ? " class=\"$oElement->{strClass}\"": '') .
            (defined($oElement->{strRef}) ? " href=\"$oElement->{strRef}\"": '') .
            (defined($oElement->{strId}) ? " id=\"$oElement->{strId}\"": '') .
            (defined($oElement->{strExtra}) ? " $oElement->{strExtra}": '') . '>';

    if (defined($oElement->{strContent}))
    {
        if (!defined($oElement->{bPre}) || !$oElement->{bPre})
        {
            $oElement->{strContent} =~ s/\n/\<br\/>\n/g;
            $oElement->{strContent} = trim($oElement->{strContent});
            $strHtml .= $self->lf();
        }
        else
        {
            $oElement->{strContent} =~ s/\&/\&amp\;/g;
        }

        $strHtml .= $oElement->{strContent};

        if (!defined($oElement->{bPre}) || !$oElement->{bPre})
        {
            $strHtml .= $self->lf() . $self->indent($iDepth);
        }
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
# escape
#
# Generate the HTML.
####################################################################################################################################
sub escape
{
    my $self = shift;
    my $strBuffer = shift;

    $strBuffer =~ s/\&/\&amp\;/g;
    $strBuffer =~ s/\</\&lt\;/g;

    return $strBuffer;
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
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->htmlGet');

    # Build the header
    my $strHtml =
        $self->indent(0) . "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\"" .
                           " \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">" . $self->lf() .
        $self->indent(0) . "<html xmlns=\"http://www.w3.org/1999/xhtml\">" . $self->lf() .
        $self->indent(0) . "<head>" . $self->lf() .
            $self->indent(1) . '<title>' . $self->escape($self->{strTitle}) . '</title>' . $self->lf() .
            $self->indent(1) . "<meta http-equiv=\"Content-Type\" content=\"text/html;charset=utf-8\"></meta>" . $self->lf();

    if (!$self->{bCompact})
    {
        $strHtml .=
            # $self->indent(1) . "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"></meta>" . $self->lf() .
            $self->indent(1) .
                '<meta property="og:site_name" content="' . $self->escape($self->{strName}) . '"></meta>' . $self->lf() .
            $self->indent(1) .
                '<meta property="og:title" content="' . $self->escape($self->{strTitle}) . '"></meta>' . $self->lf() .
            $self->indent(1) . '<meta property="og:type" content="website"></meta>' . $self->lf();

        if (defined($self->{strFavicon}))
        {
            $strHtml .=
                $self->indent(1) . "<link rel=\"icon\" href=\"$self->{strFavicon}\" type=\"image/png\"></link>" . $self->lf();
        }

        if (defined($self->{strLogo}))
        {
            $strHtml .=
                $self->indent(1) . "<meta property=\"og:image:type\" content=\"image/png\"></meta>" . $self->lf() .
                $self->indent(1) . "<meta property=\"og:image\" content=\"{[backrest-url-base]}/$self->{strLogo}\"></meta>" .
                $self->lf();
        }

        if (defined($self->{strDescription}))
        {
            $strHtml .=
                $self->indent(1) .
                    '<meta name="description" content="' . $self->escape($self->{strDescription}) . '"></meta>' . $self->lf() .
                $self->indent(1) .
                    '<meta property="og:description" content="' . $self->escape($self->{strDescription}) . '"></meta>' . $self->lf();
        }
    }

    if (defined($self->{strCss}))
    {
        my $strCss = $self->{strCss};

        if (!$self->{bPretty})
        {
            $strCss =~ s/^\s+//mg;
            $strCss =~ s/\n//g;
            $strCss =~ s/\/\*.*?\*\///g;
        }

        $strHtml .=
            $self->indent(1) . '<style type="text/css">' . $self->lf() . trim($strCss) . $self->lf() .
            $self->indent(1) . '</style>' . $self->lf();
    }
    else
    {
        $strHtml .=
            $self->indent(1) . "<link rel=\"stylesheet\" href=\"default.css\" type=\"text/css\"></link>" . $self->lf();
    }

    $strHtml .=
        $self->indent(0) . "</head>" . $self->lf() .
        $self->htmlRender($self->bodyGet(), 0);

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
