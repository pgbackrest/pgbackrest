####################################################################################################################################
# DOC HTML ELEMENT MODULE
####################################################################################################################################
package BackRestDoc::Html::DocHtmlElement;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);
use File::Copy;
use Scalar::Util qw(blessed);

use lib dirname($0) . '/../lib';
use pgBackRest::Common::Log;

####################################################################################################################################
# Html Element Types
####################################################################################################################################
use constant HTML_A                                                 => 'a';
    push @EXPORT, qw(HTML_A);
use constant HTML_BODY                                              => 'body';
    push @EXPORT, qw(HTML_BODY);
use constant HTML_PRE                                               => 'pre';
    push @EXPORT, qw(HTML_PRE);
use constant HTML_DIV                                               => 'div';
    push @EXPORT, qw(HTML_DIV);
use constant HTML_SPAN                                              => 'span';
    push @EXPORT, qw(HTML_SPAN);
use constant HTML_UL                                                => 'ul';
    push @EXPORT, qw(HTML_UL);
use constant HTML_LI                                                => 'li';
    push @EXPORT, qw(HTML_LI);

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
        $self->{strType},
        $self->{strClass},
        my $oParam
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strType', trace => true},
            {name => 'strClass', required => false, trace => true},
            {name => 'oParam', required => false, trace => true}
        );

    $self->{oyElement} = [];
    $self->{strContent} = $$oParam{strContent};
    $self->{strId} = $$oParam{strId};
    $self->{strRef} = $$oParam{strRef};
    $self->{bPre} = $$oParam{bPre};

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# addNew
#
# Create a new element and add it.
####################################################################################################################################
sub addNew
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my (
        $strOperation,
        $strType,
        $strClass,
        $oParam
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->addNew', \@_,
            {name => 'strType', trace => true},
            {name => 'strClass', required => false, trace => true},
            {name => 'oParam', required => false, trace => true}
        );

    my $oElement = new BackRestDoc::Html::DocHtmlElement($strType, $strClass, $oParam);

    $self->add($oElement);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oElement', value => $oElement, trace => true}
    );
}

####################################################################################################################################
# add
#
# Add an element.
####################################################################################################################################
sub add
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my (
        $strOperation,
        $oElement
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->add', \@_,
            {name => 'oElement', trace => true}
        );

    if (!(blessed($oElement) && $oElement->isa('BackRestDoc::Html::DocHtmlElement')))
    {
        confess &log(ASSERT, 'oElement must be a valid element object');
    }

    push(@{$self->{oyElement}}, $oElement);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oElement', value => $oElement, trace => true}
    );
}

1;
