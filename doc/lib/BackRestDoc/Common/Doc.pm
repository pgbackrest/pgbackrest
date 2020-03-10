####################################################################################################################################
# DOC MODULE
####################################################################################################################################
package BackRestDoc::Common::Doc;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Cwd qw(abs_path);
use File::Basename qw(dirname);
use Scalar::Util qw(blessed);
use XML::Checker::Parser;

use BackRestDoc::Common::Log;
use BackRestDoc::Common::String;

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
        my $strSgmlSearchPath,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strFileName', required => false},
            {name => 'strSgmlSearchPath', required => false},
        );

    # Load the doc from a file if one has been defined
    if (defined($self->{strFileName}))
    {
        my $oParser = XML::Checker::Parser->new(ErrorContext => 2, Style => 'Tree');
        $oParser->set_sgml_search_path(
            defined($strSgmlSearchPath) ? $strSgmlSearchPath : dirname(dirname(abs_path($0))) . '/doc/xml/dtd');

        my $oTree;

        eval
        {
            local $XML::Checker::FAIL = sub
            {
                my $iCode = shift;

                die XML::Checker::error_string($iCode, @_);
            };

            $oTree = $oParser->parsefile($self->{strFileName});

            return true;
        }
        # Report any error that stopped parsing
        or do
        {
            my $strException = $EVAL_ERROR;
            $strException =~ s/at \/.*?$//s;               # remove module line number
            die "malformed xml in '$self->{strFileName}':\n" . trim($strException);
        };

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
            __PACKAGE__ . '->parse', \@_,
            {name => 'strName', trace => true},
            {name => 'oyNode', trace => true}
        );

    my %oOut;
    my $iIndex = 0;
    my $bText = $strName eq 'text' || $strName eq 'li' || $strName eq 'p' || $strName eq 'title' ||
                $strName eq 'summary' || $strName eq 'table-cell' || $strName eq 'table-column' || $strName eq 'list-item' ||
                $strName eq 'admonition';

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
            __PACKAGE__ . '->build', \@_,
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
        $$oDoc{name} eq 'table-cell' || $$oDoc{name} eq 'table-column' || $$oDoc{name} eq 'list-item' ||
        $$oDoc{name} eq 'admonition')
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
            elsif ((defined($$oSub{value}) && !defined($$oSub{param})) && $strName ne 'code-block')
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
# nodeGetById
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
            __PACKAGE__ . 'nodeGetById', \@_,
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
            __PACKAGE__ . '->nodeBless', \@_,
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
            __PACKAGE__ . '->nodeList', \@_,
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
            __PACKAGE__ . '->nodeRemove', \@_,
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
        confess &log(ERROR, "child was not found in node, could not be removed");
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# nodeReplace
#
# Replace a child node with one or more child nodes.
####################################################################################################################################
sub nodeReplace
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oChildRemove,
        $oyChildReplace,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->nodeReplace', \@_,
            {name => 'oChildRemove', trace => true},
            {name => 'oChildReplace', trace => true},
        );

    my $bReplace = false;
    my $iReplaceIdx = undef;
    my $iReplaceTotal = undef;
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
            splice(@{$$oDoc{children}}, $iIndex, 0, @{$oyChildReplace});

            $iReplaceIdx = $iIndex;
            $iReplaceTotal = scalar(@{$oyChildReplace});
            $bReplace = true;
            last;
        }
    }

    if (!$bReplace)
    {
        confess &log(ERROR, "child was not found in node, could not be replaced");
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iReplaceIdx', value => $iReplaceIdx, trace => true},
        {name => 'iReplaceTotal', value => $iReplaceTotal, trace => true},
    );
}

####################################################################################################################################
# nameGet
####################################################################################################################################
sub nameGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my $strOperation = logDebugParam(__PACKAGE__ . '->nameGet');

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
    my $strOperation = logDebugParam(__PACKAGE__ . '->valueGet');

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
    my $strOperation = logDebugParam(__PACKAGE__ . '->valueSet');

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
            __PACKAGE__ . '->paramGet', \@_,
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
            __PACKAGE__ . '->paramTest', \@_,
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
            __PACKAGE__ . '->paramSet', \@_,
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
