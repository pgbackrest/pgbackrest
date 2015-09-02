####################################################################################################################################
# DOC MODULE
####################################################################################################################################
package BackRestDoc::Doc;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);

use lib dirname($0) . '/../lib';
use BackRest::Common::Log;
use BackRest::Common::String;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_DOC                                                 => 'Doc';

use constant OP_DOC_BUILD                                           => OP_DOC . '->build';
use constant OP_DOC_NEW                                             => OP_DOC . '->new';
use constant OP_DOC_NODE_GET                                        => OP_DOC . '->nodeGet';
use constant OP_DOC_PARSE                                           => OP_DOC . '->parse';

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;       # Class name

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Assign function parameters, defaults, and log debug info
    (
        my $strOperation,
        $self->{strFileName}
    ) =
        logDebugParam
        (
            OP_DOC_NEW, \@_,
            {name => 'strFileName'}
        );

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
    my $bText = $strName eq 'text' || $strName eq 'li' || $strName eq 'code-block';

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
    my $oOut = {name => $$oDoc{name}, children => []};
    my $strError = "in node $$oDoc{name}";

    # Get all params
    if (defined($$oDoc{param}))
    {
        for my $strParam (keys $$oDoc{param})
        {
            $$oOut{param}{$strParam} = $$oDoc{param}{$strParam};
        }
    }

    if (defined($$oDoc{children}))
    {
        for (my $iIndex = 0; $iIndex < @{$$oDoc{children}}; $iIndex++)
        {
            my $oSub = $$oDoc{children}[$iIndex];
            my $strName = $$oSub{name};

            if ($strName eq 'text' || $strName eq 'code-block')
            {
                $$oOut{field}{text} = $oSub;
            }
            elsif (defined($$oSub{value}))
            {
                $$oOut{field}{$strName} = $$oSub{value};
            }
            elsif (!defined($$oSub{children}))
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
sub nodeGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oDoc,
        $strName,
        $bRequired,
    ) =
        logDebugParam
        (
            OP_DOC_NODE_GET, \@_,
            {name => 'oDoc', default => $self->{oDoc}, trace => true},
            {name => 'strName', trace => true},
            {name => 'bRequired', default => true, trace => true},
        );

    my $oNode;

    for (my $iIndex = 0; $iIndex < @{$$oDoc{children}}; $iIndex++)
    {
        if ($$oDoc{children}[$iIndex]{name} eq $strName)
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
        confess "unable to find child ${strName} in node $$oDoc{name}";
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oNode', value => $oNode, trace => true}
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
        $oDoc,
        $strName,
        $bRequired,
    ) =
        logDebugParam
        (
            OP_DOC_NODE_GET, \@_,
            {name => 'oDoc'},
            {name => 'strName', required => false, trace => true},
            {name => 'bRequired', default => true, trace => true},
        );

    my @oyNode;

    for (my $iIndex = 0; $iIndex < @{$$oDoc{children}}; $iIndex++)
    {
        if (!defined($strName) || $$oDoc{children}[$iIndex]{name} eq $strName)
        {
            push(@oyNode, $$oDoc{children}[$iIndex]);
        }
    }

    if (@oyNode == 0 && $bRequired)
    {
        confess "unable to find child ${strName} in node $$oDoc{name}";
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oyNode', value => \@oyNode, ref => true, trace => true}
    );
}

1;
