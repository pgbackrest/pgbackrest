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
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::FileCommon;

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
use constant OP_DOC_NODE_REMOVE                                     => OP_DOC . '->nodeRemove';
use constant OP_DOC_PARAM_GET                                       => OP_DOC . '->paramGet';
use constant OP_DOC_PARAM_SET                                       => OP_DOC . '->paramSet';
use constant OP_DOC_PARAM_TEST                                      => OP_DOC . '->paramSet';
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
        $self->{strFileName},
        my $bCached
    ) =
        logDebugParam
        (
            OP_DOC_NEW, \@_,
            {name => 'strFileName', required => false},
            {name => 'bCached', default => false}
        );

    # Load the doc from a file if one has been defined
    if (defined($self->{strFileName}))
    {
        if ($bCached)
        {
            $self->oDoc = XMLin(fileStringRead($self->{strFileName}));
        }
        else
        {
            my $oParser = XML::Checker::Parser->new(ErrorContext => 2, Style => 'Tree');

            if (-e dirname($0) . '/dtd')
            {
                $oParser->set_sgml_search_path(dirname($0) . '/dtd')
            }
            else
            {
                $oParser->set_sgml_search_path(dirname($0) . '/xml/dtd');
            }

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
    my $bText = $strName eq 'text' || $strName eq 'li' || $strName eq 'p' || $strName eq 'title' ||
                $strName eq 'summary' || $strName eq 'table-cell' || $strName eq 'table-column';

    # Store the node name
    $oOut{name} = $strName;

    if (keys(%{$$oyNode[$iIndex]}))
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

                    push(@{$oOut{children}}, $strBuffer);
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

            push(@{$oOut{children}}, $self->parse($$oyNode[$iIndex++], $$oyNode[$iIndex++]));
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
        for my $strParam (keys %{$$oDoc{param}})
        {
            $$oOut{param}{$strParam} = $$oDoc{param}{$strParam};
        }
    }

    if ($$oDoc{name} eq 'p' || $$oDoc{name} eq 'title' || $$oDoc{name} eq 'summary' ||
        $$oDoc{name} eq 'table-cell' || $$oDoc{name} eq 'table-column')
    {
        $$oOut{field}{text} = $oDoc;
    }
    elsif (defined($$oDoc{children}))
    {
        for (my $iIndex = 0; $iIndex < @{$$oDoc{children}}; $iIndex++)
        {
            my $oSub = $$oDoc{children}[$iIndex];
            my $strName = $$oSub{name};

            if ($strName eq 'text')
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
                push(@{$$oOut{children}}, $self->build($oSub));
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
# nodeTest
#
# Test that a node exists
####################################################################################################################################
sub nodeTest
{
    my $self = shift;

    return defined($self->nodeGetById(shift, undef, false));
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
# nodeRemove
#
# Remove a child node.
####################################################################################################################################
sub nodeRemove
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oChildRemove
    ) =
        logDebugParam
        (
            OP_DOC_NODE_REMOVE, \@_,
            {name => 'oChildRemove', required => false, trace => true}
        );

    my $bRemove = false;
    my $oDoc = $self->{oDoc};

    # Error if there are no children
    if (!defined($$oDoc{children}))
    {
        confess &log(ERROR, "node has no children");
    }

    for (my $iIndex = 0; $iIndex < @{$$oDoc{children}}; $iIndex++)
    {
        if ($$oDoc{children}[$iIndex] == $oChildRemove->{oDoc})
        {
            splice(@{$$oDoc{children}}, $iIndex, 1);
            $bRemove = true;
            last;
        }
    }

    if (!$bRemove)
    {
        confess &log(ERROR, "child was not found in node");
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
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
        $strDefault,
        $strType
    ) =
        logDebugParam
        (
            OP_DOC_PARAM_GET, \@_,
            {name => 'strName', trace => true},
            {name => 'bRequired', default => true, trace => true},
            {name => 'strDefault', required => false, trace => true},
            {name => 'strType', default => 'param', trace => true}
        );

    my $strValue = ${$self->{oDoc}}{$strType}{$strName};

    if (!defined($strValue))
    {
        if ($bRequired)
        {
            confess "${strType} '${strName}' is required in node '$self->{strName}'";
        }

        $strValue = $strDefault;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strValue', value => $strValue, trace => true}
    );
}

####################################################################################################################################
# paramTest
#
# Test that a parameter exists or has a certain value.
####################################################################################################################################
sub paramTest
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strName,
        $strExpectedValue,
        $strType
    ) =
        logDebugParam
        (
            OP_DOC_PARAM_TEST, \@_,
            {name => 'strName', trace => true},
            {name => 'strExpectedValue', required => false, trace => true},
            {name => 'strType', default => 'param', trace => true}
        );

    my $bResult = true;
    my $strValue = $self->paramGet($strName, false, undef, $strType);

    if (!defined($strValue))
    {
        $bResult = false;
    }
    elsif (defined($strExpectedValue) && $strValue ne $strExpectedValue)
    {
        $bResult = false;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bResult', value => $bResult, trace => true}
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
            {name => 'strValue', required => false, trace => true},
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

    return $self->paramGet(shift, shift, shift, 'field');
}

####################################################################################################################################
# fieldTest
#
# Test if a field exists.
####################################################################################################################################
sub fieldTest
{
    my $self = shift;

    return $self->paramTest(shift, shift, 'field');
}

####################################################################################################################################
# textGet
#
# Get a field from a node.
####################################################################################################################################
sub textGet
{
    my $self = shift;

    return $self->nodeBless($self->paramGet('text', shift, shift, 'field'));
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
