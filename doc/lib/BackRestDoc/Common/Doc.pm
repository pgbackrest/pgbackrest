####################################################################################################################################
# DOC MODULE
####################################################################################################################################
package BackRestDoc::Common::Doc;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

# use Exporter qw(import);
#     our @EXPORT = qw();
use File::Basename qw(dirname);
use Scalar::Util qw(blessed);

use lib dirname($0) . '/../lib';
use BackRest::Common::Log;
use BackRest::Common::String;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_DOC                                                 => 'Doc';

use constant OP_DOC_BUILD                                           => OP_DOC . '->build';
use constant OP_DOC_NAME_GET                                        => OP_DOC . '->nameGet';
use constant OP_DOC_NEW                                             => OP_DOC . '->new';
use constant OP_DOC_NODE_BLESS                                      => OP_DOC . '->nodeBless';
use constant OP_DOC_NODE_GET                                        => OP_DOC . '->nodeGet';
use constant OP_DOC_NODE_LIST                                       => OP_DOC . '->nodeList';
use constant OP_DOC_PARAM_GET                                       => OP_DOC . '->paramGet';
use constant OP_DOC_PARAM_SET                                       => OP_DOC . '->paramSet';
use constant OP_DOC_PARSE                                           => OP_DOC . '->parse';
use constant OP_DOC_VALUE_GET                                       => OP_DOC . '->valueGet';
use constant OP_DOC_VALUE_SET                                       => OP_DOC . '->valueSet';

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
        $self->{strFileName}
    ) =
        logDebugParam
        (
            OP_DOC_NEW, \@_,
            {name => 'strFileName', required => false}
        );

    # Load the doc from a file if one has been defined
    if (defined($self->{strFileName}))
    {
        my $oParser = XML::Checker::Parser->new(ErrorContext => 2, Style => 'Tree');
        $oParser->set_sgml_search_path(dirname($self->{strFileName}) . '/dtd');

        my $oTree;

        eval
        {
            local $XML::Checker::FAIL = sub
            {
                my $iCode = shift;

                die XML::Checker::error_string($iCode, @_);
            };

            $oTree = $oParser->parsefile($self->{strFileName});
        };

        # Report any error that stopped parsing
        if ($@)
        {
            $@ =~ s/at \/.*?$//s;               # remove module line number
            die "malformed xml in '$self->{strFileName}':\n" . trim($@);
        }

        # Parse and build the doc
        $self->{oDoc} = $self->build($self->parse(${$oTree}[0], ${$oTree}[1]));

    }
    # Else create a blank doc
    else
    {
        $self->{oDoc} = {name => 'doc', children => []};
    }

    $self->{strName} = 'root';

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# parse
#
# Parse the xml doc into a more usable hash and array structure.
####################################################################################################################################
sub parse
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strName,
        $oyNode
    ) =
        logDebugParam
        (
            OP_DOC_PARSE, \@_,
            {name => 'strName', trace => true},
            {name => 'oyNode', trace => true}
        );

    my %oOut;
    my $iIndex = 0;
    my $bText = $strName eq 'text' || $strName eq 'li' || $strName eq 'code-block' || $strName eq 'p' || $strName eq 'title' ||
                $strName eq 'summary';

    # Store the node name
    $oOut{name} = $strName;

    if (keys($$oyNode[$iIndex]))
    {
        $oOut{param} = $$oyNode[$iIndex];
    }

    $iIndex++;

    # Look for strings and children
    while (defined($$oyNode[$iIndex]))
    {
        # Process string data
        if (ref(\$$oyNode[$iIndex]) eq 'SCALAR' && $$oyNode[$iIndex] eq '0')
        {
            $iIndex++;
            my $strBuffer = $$oyNode[$iIndex++];

            # Strip tabs, CRs, and LFs
            $strBuffer =~ s/\t|\r//g;

            # If anything is left
            if (length($strBuffer) > 0)
            {
                # If text node then create array entries for strings
                if ($bText)
                {
                    if (!defined($oOut{children}))
                    {
                        $oOut{children} = [];
                    }

                    push($oOut{children}, $strBuffer);
                }
                # Don't allow strings mixed with children
                elsif (length(trim($strBuffer)) > 0)
                {
                    if (defined($oOut{children}))
                    {
                        confess "text mixed with children in node ${strName} (spaces count)";
                    }

                    if (defined($oOut{value}))
                    {
                        confess "value is already defined in node ${strName} - this shouldn't happen";
                    }

                    # Don't allow text mixed with
                    $oOut{value} = $strBuffer;
                }
            }
        }
        # Process a child
        else
        {
            if (defined($oOut{value}) && $bText)
            {
                confess "text mixed with children in node ${strName} before child " . $$oyNode[$iIndex++] . " (spaces count)";
            }

            if (!defined($oOut{children}))
            {
                $oOut{children} = [];
            }

            push($oOut{children}, $self->parse($$oyNode[$iIndex++], $$oyNode[$iIndex++]));
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oDoc', value => \%oOut, trace => true}
    );
}

####################################################################################################################################
# build
#
# Restructure the doc to make walking it easier.
####################################################################################################################################
sub build
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oDoc
    ) =
        logDebugParam
        (
            OP_DOC_BUILD, \@_,
            {name => 'oDoc', trace => true}
        );

    # Initialize the node object
    my $oOut = {name => $$oDoc{name}, children => [], value => $$oDoc{value}};
    my $strError = "in node $$oDoc{name}";

    # Get all params
    if (defined($$oDoc{param}))
    {
        for my $strParam (keys $$oDoc{param})
        {
            $$oOut{param}{$strParam} = $$oDoc{param}{$strParam};
        }
    }

    if ($$oDoc{name} eq 'p' || $$oDoc{name} eq 'title' || $$oDoc{name} eq 'summary')
    {
        $$oOut{field}{text} = $oDoc;
    }
    elsif (defined($$oDoc{children}))
    {
        for (my $iIndex = 0; $iIndex < @{$$oDoc{children}}; $iIndex++)
        {
            my $oSub = $$oDoc{children}[$iIndex];
            my $strName = $$oSub{name};

            if ($strName eq 'text' || $strName eq 'code-block')
            {
                $$oOut{field}{text} = $oSub;
            }
            elsif (defined($$oSub{value}) && !defined($$oSub{param}))
            {
                $$oOut{field}{$strName} = $$oSub{value};
            }
            elsif (!defined($$oSub{children}) && !defined($$oSub{value}) && !defined($$oSub{param}))
            {
                $$oOut{field}{$strName} = true;
            }
            else
            {
                push($$oOut{children}, $self->build($oSub));
            }
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oDoc', value => $oOut, trace => true}
    );
}

####################################################################################################################################
# nodeGet
#
# Return a node by name - error if more than one exists
####################################################################################################################################
sub nodeGetById
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strName,
        $strId,
        $bRequired,
    ) =
        logDebugParam
        (
            OP_DOC_NODE_GET, \@_,
            {name => 'strName', trace => true},
            {name => 'strId', required => false, trace => true},
            {name => 'bRequired', default => true, trace => true}
        );

    my $oDoc = $self->{oDoc};
    my $oNode;

    for (my $iIndex = 0; $iIndex < @{$$oDoc{children}}; $iIndex++)
    {
        if ((defined($strName) && $$oDoc{children}[$iIndex]{name} eq $strName) &&
            (!defined($strId) || $$oDoc{children}[$iIndex]{param}{id} eq $strId))
        {
            if (!defined($oNode))
            {
                $oNode = $$oDoc{children}[$iIndex];
            }
            else
            {
                confess "found more than one child ${strName} in node $$oDoc{name}";
            }
        }
    }

    if (!defined($oNode) && $bRequired)
    {
        confess "unable to find child ${strName}" . (defined($strId) ? " (${strId})" : '') . " in node $$oDoc{name}";
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oNodeDoc', value => $self->nodeBless($oNode), trace => true}
    );
}

####################################################################################################################################
# nodeGet
#
# Return a node by name - error if more than one exists
####################################################################################################################################
sub nodeGet
{
    my $self = shift;

    return $self->nodeGetById(shift, undef, shift);
}

####################################################################################################################################
# nodeAdd
#
# Add a node to to the current doc's child list
####################################################################################################################################
sub nodeAdd
{
    my $self = shift;
    my $strName = shift;
    my $strValue = shift;
    my $oParam = shift;
    my $oField = shift;

    my $oDoc = $self->{oDoc};
    my $oNode = {name => $strName, value => $strValue, param => $oParam, field => $oField};

    push(@{$$oDoc{children}}, $oNode);

    return $self->nodeBless($oNode);
}

####################################################################################################################################
# nodeBless
#
# Make a new Doc object from a node.
####################################################################################################################################
sub nodeBless
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oNode
    ) =
        logDebugParam
        (
            OP_DOC_NODE_BLESS, \@_,
            {name => 'oNode', required => false, trace => true}
        );

    my $oDoc;

    if (defined($oNode))
    {
        $oDoc = {};
        bless $oDoc, $self->{strClass};

        $oDoc->{strClass} = $self->{strClass};
        $oDoc->{strName} = $$oNode{name};
        $oDoc->{oDoc} = $oNode;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oDoc', value => $oDoc, trace => true}
    );
}

####################################################################################################################################
# nodeList
#
# Get a list of nodes.
####################################################################################################################################
sub nodeList
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strName,
        $bRequired,
    ) =
        logDebugParam
        (
            OP_DOC_NODE_LIST, \@_,
            {name => 'strName', required => false, trace => true},
            {name => 'bRequired', default => true, trace => true},
        );

    my $oDoc = $self->{oDoc};
    my @oyNode;

    if (defined($$oDoc{children}))
    {
        for (my $iIndex = 0; $iIndex < @{$$oDoc{children}}; $iIndex++)
        {
            if (!defined($strName) || $$oDoc{children}[$iIndex]{name} eq $strName)
            {
                if (ref(\$$oDoc{children}[$iIndex]) eq "SCALAR")
                {
                    push(@oyNode, $$oDoc{children}[$iIndex]);
                }
                else
                {
                    push(@oyNode, $self->nodeBless($$oDoc{children}[$iIndex]));
                }
            }
        }
    }

    if (@oyNode == 0 && $bRequired)
    {
        confess 'unable to find ' . (defined($strName) ? "children named '${strName}'" : 'any children') . " in node $$oDoc{name}";
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oyNode', value => \@oyNode, trace => true}
    );
}

####################################################################################################################################
# nameGet
####################################################################################################################################
sub nameGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my $strOperation = logDebugParam(OP_DOC_NAME_GET);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strName', value => ${$self->{oDoc}}{name}, trace => true}
    );
}

####################################################################################################################################
# valueGet
####################################################################################################################################
sub valueGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my $strOperation = logDebugParam(OP_DOC_VALUE_GET);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strValue', value => ${$self->{oDoc}}{value}, trace => true}
    );
}

####################################################################################################################################
# valueSet
####################################################################################################################################
sub valueSet
{
    my $self = shift;
    my $strValue = shift;

    # Assign function parameters, defaults, and log debug info
    my $strOperation = logDebugParam(OP_DOC_VALUE_SET);

    # Set the value
    ${$self->{oDoc}}{value} = $strValue;

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# paramGet
#
# Get a parameter from a node.
####################################################################################################################################
sub paramGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strName,
        $bRequired,
        $strType
    ) =
        logDebugParam
        (
            OP_DOC_PARAM_GET, \@_,
            {name => 'strName', trace => true},
            {name => 'bRequired', default => true, trace => true},
            {name => 'strType', default => 'param', trace => true}
        );

    my $strValue = ${$self->{oDoc}}{$strType}{$strName};

    if (!defined($strValue) && $bRequired)
    {
        confess "${strType} '${strName}' in required in node '$self->{strName}'";
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strValue', value => $strValue, trace => true}
    );
}

####################################################################################################################################
# paramSet
#
# Set a parameter in a node.
####################################################################################################################################
sub paramSet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strName,
        $strValue,
        $strType
    ) =
        logDebugParam
        (
            OP_DOC_PARAM_SET, \@_,
            {name => 'strName', trace => true},
            {name => 'strValue', trace => true},
            {name => 'strType', default => 'param', trace => true}
        );

    ${$self->{oDoc}}{$strType}{$strName} = $strValue;

    # Return from function and log return values if any
    logDebugReturn($strOperation);
}

####################################################################################################################################
# fieldGet
#
# Get a field from a node.
####################################################################################################################################
sub fieldGet
{
    my $self = shift;

    return $self->paramGet(shift, shift, 'field');
}

####################################################################################################################################
# textGet
#
# Get a field from a node.
####################################################################################################################################
sub textGet
{
    my $self = shift;

    return $self->nodeBless($self->paramGet('text', shift, 'field'));
}

####################################################################################################################################
# textSet
#
# Get a field from a node.
####################################################################################################################################
sub textSet
{
    my $self = shift;
    my $oText = shift;

    if (blessed($oText) && $oText->isa('BackRestDoc::Common::Doc'))
    {
        $oText = $oText->{oDoc};
    }
    elsif (ref($oText) ne 'HASH')
    {
        $oText = {name => 'text', children => [$oText]};
    }

    return $self->paramSet('text', $oText, 'field');
}

####################################################################################################################################
# fieldSet
#
# Set a parameter in a node.
####################################################################################################################################
sub fieldSet
{
    my $self = shift;

    $self->paramSet(shift, shift, 'field');
}

1;
