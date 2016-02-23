####################################################################################################################################
# DOC MANIFEST MODULE
####################################################################################################################################
package BackRestDoc::Common::DocManifest;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Cwd qw(abs_path);
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);
use Scalar::Util qw(blessed);

use lib dirname($0) . '/../lib';
use BackRest::Common::Log;
use BackRest::Common::String;
use BackRest::FileCommon;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_DOC_MANIFEST                                        => 'DocManifest';

use constant OP_DOC_MANIFEST_NEW                                    => OP_DOC_MANIFEST . '->new';
use constant OP_DOC_MANIFEST_RENDER_GET                             => OP_DOC_MANIFEST . '->renderGet';
use constant OP_DOC_MANIFEST_RENDER_LIST                            => OP_DOC_MANIFEST . '->renderList';
use constant OP_DOC_MANIFEST_RENDER_OUT_GET                         => OP_DOC_MANIFEST . '->renderOutGet';
use constant OP_DOC_MANIFEST_RENDER_OUT_LIST                        => OP_DOC_MANIFEST . '->renderOutList';
use constant OP_DOC_MANIFEST_SOURCE_GET                             => OP_DOC_MANIFEST . '->sourceGet';
use constant OP_DOC_MANIFEST_VARIABLE_LIST_PARSE                    => OP_DOC_MANIFEST . '->variableListParse';

####################################################################################################################################
# File constants
####################################################################################################################################
use constant FILE_MANIFEST                                          => 'manifest.xml';

####################################################################################################################################
# Render constants
####################################################################################################################################
use constant RENDER                                                 => 'render';
use constant RENDER_FILE                                            => 'file';

use constant RENDER_TYPE                                            => 'type';
use constant RENDER_TYPE_HTML                                       => 'html';
    push @EXPORT, qw(RENDER_TYPE_HTML);
use constant RENDER_TYPE_MARKDOWN                                   => 'markdown';
    push @EXPORT, qw(RENDER_TYPE_MARKDOWN);
use constant RENDER_TYPE_PDF                                        => 'pdf';
    push @EXPORT, qw(RENDER_TYPE_PDF);

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
        $self->{stryKeyword},
        $self->{stryRequire},
        my $oVariableOverride,
        $self->{strDocPath}
    ) =
        logDebugParam
        (
            OP_DOC_MANIFEST_NEW, \@_,
            {name => 'stryKeyword'},
            {name => 'stryRequire'},
            {name => 'oVariableOverride', required => false},
            {name => 'strDocPath', required => false},
        );

    # Set the base path if it was not passed in
    if (!defined($self->{strDocPath}))
    {
        $self->{strDocPath} = abs_path(dirname($0));
    }

    # Load the manifest
    $self->{oManifestXml} = new BackRestDoc::Common::Doc("$self->{strDocPath}/manifest.xml");

    # Iterate the sources
    $self->{oManifest} = {};

    foreach my $oSource ($self->{oManifestXml}->nodeGet('source-list')->nodeList('source'))
    {
        my $oSourceHash = {};
        my $strKey = $oSource->paramGet('key');

        logDebugMisc
        (
            $strOperation, 'load source',
            {name => 'strKey', value => $strKey}
        );

        $$oSourceHash{doc} = new BackRestDoc::Common::Doc("$self->{strDocPath}/xml/${strKey}.xml");

        # Read variables from source
        $self->variableListParse($$oSourceHash{doc}->nodeGet('variable-list', false), $oVariableOverride);

        ${$self->{oManifest}}{source}{$strKey} = $oSourceHash;
    }

    # Iterate the renderers
    foreach my $oRender ($self->{oManifestXml}->nodeGet('render-list')->nodeList('render'))
    {
        my $oRenderHash = {};
        my $strType = $oRender->paramGet(RENDER_TYPE);

        # Only one instance of each render type can be defined
        if (defined(${$self->{oManifest}}{&RENDER}{$strType}))
        {
            confess &log(ERROR, "render ${strType} has already been defined");
        }

        # Get the file param
        $${oRenderHash}{file} = $oRender->paramGet(RENDER_FILE, false);

        logDebugMisc
        (
            $strOperation, '    load render',
            {name => 'strType', value => $strType},
            {name => 'strFile', value => $${oRenderHash}{file}}
        );

        # Error if file is set and render type is not pdf
        if (defined($${oRenderHash}{file}) && $strType ne RENDER_TYPE_PDF)
        {
            confess &log(ERROR, 'only the pdf render type can have file set')
        }

        # Iterate the render sources
        foreach my $oRenderOut ($oRender->nodeList('render-source'))
        {
            my $oRenderOutHash = {};
            my $strKey = $oRenderOut->paramGet('key');
            my $strSource = $oRenderOut->paramGet('source', false, $strKey);

            $$oRenderOutHash{source} = $strSource;

            # Get the menu caption
            if (defined($oRenderOut->paramGet('menu', false)) && $strType ne RENDER_TYPE_HTML)
            {
                confess &log(ERROR, "menu is only valid with html render type");
            }

            # Get the filename
            if (defined($oRenderOut->paramGet('file', false)))
            {
                if ($strType eq RENDER_TYPE_HTML || $strType eq RENDER_TYPE_MARKDOWN)
                {
                    $$oRenderOutHash{file} = $oRenderOut->paramGet('file');
                }
                else
                {
                    confess &log(ERROR, "file is only valid with html or markdown render types");
                }
            }

            if (defined($oRenderOut->paramGet('menu', false)))
            {
                if ($strType eq RENDER_TYPE_HTML)
                {
                    $$oRenderOutHash{menu} = $oRenderOut->paramGet('menu', false);
                }
                else
                {
                    confess &log(ERROR, 'only the html render type can have menu set');
                }
            }

            logDebugMisc
            (
                $strOperation, '        load render source',
                {name => 'strKey', value => $strKey},
                {name => 'strSource', value => $strSource},
                {name => 'strMenu', value => $${oRenderOutHash}{menu}}
            );

            $${oRenderHash}{out}{$strKey} = $oRenderOutHash;
        }

        ${$self->{oManifest}}{render}{$strType} = $oRenderHash;
    }

    # Read variables from manifest
    $self->variableListParse($self->{oManifestXml}->nodeGet('variable-list', false), $oVariableOverride);

    # use Data::Dumper; confess Dumper($self->{oVariable});

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# isBackRest
#
# Until all the backrest specific code can be abstracted, this function will identify when BackRest docs are being built.
####################################################################################################################################
sub isBackRest
{
    my $self = shift;

    return($self->variableTest('project-exe', 'pg_backrest'));
}

####################################################################################################################################
# keywordMatch
#
# See if all the keywords were on the command line.
####################################################################################################################################
sub keywordMatch
{
    my $self = shift;
    my $strKeywordRequired = shift;

    if (defined($strKeywordRequired))
    {
        for my $strKeyword (split(',', $strKeywordRequired))
        {
            $strKeyword = trim($strKeyword);

            if (!grep(/^$strKeyword$/, @{$self->{stryKeyword}}))
            {
                return false;
            }
        }
    }

    return true;
}

####################################################################################################################################
# variableListParse
#
# Parse a variable list and store variables.
####################################################################################################################################
sub variableListParse
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oVariableList,
        $oVariableOverride
    ) =
        logDebugParam
        (
            OP_DOC_MANIFEST_VARIABLE_LIST_PARSE, \@_,
            {name => '$oVariableList', required => false},
            {name => '$oVariableOverride', required => false}
        );

    if (defined($oVariableList))
    {
        foreach my $oVariable ($oVariableList->nodeList('variable'))
        {
            if ($self->keywordMatch($oVariable->paramGet('keyword', false)))
            {
                my $strKey = $oVariable->paramGet('key');
                my $strValue = $oVariable->valueGet();

                if ($oVariable->paramTest('eval', 'y'))
                {
                    $strValue = eval $strValue;

                    if ($@)
                    {
                        confess &log(ERROR, "unable to evaluate ${strKey}: $@\n" . $oVariable->valueGet());
                    }
                }

                $self->variableSet($strKey, defined($$oVariableOverride{$strKey}) ? $$oVariableOverride{$strKey} : $strValue);

                logDebugMisc
                (
                    $strOperation, '    load variable',
                    {name => 'strKey', value => $strKey},
                    {name => 'strValue', value => $strValue}
                );
            }
        }
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# variableReplace
#
# Replace variables in the string.
####################################################################################################################################
sub variableReplace
{
    my $self = shift;
    my $strBuffer = shift;
    my $strType = shift;

    if (!defined($strBuffer))
    {
        return;
    }

    foreach my $strName (sort(keys(%{$self->{oVariable}})))
    {
        my $strValue = $self->{oVariable}{$strName};

        $strBuffer =~ s/\{\[$strName\]\}/$strValue/g;
    }

    if (defined($strType) && $strType eq 'latex')
    {
        $strBuffer =~ s/\\\_/\_/g;
        $strBuffer =~ s/\_/\\\_/g;
        $strBuffer =~ s/\\\#/\#/g;
        $strBuffer =~ s/\#/\\\#/g;
    }

    return $strBuffer;
}

####################################################################################################################################
# variableSet
#
# Set a variable to be replaced later.
####################################################################################################################################
sub variableSet
{
    my $self = shift;
    my $strKey = shift;
    my $strValue = shift;
    my $bForce = shift;

    if (defined(${$self->{oVariable}}{$strKey}) && (!defined($bForce) || !$bForce))
    {
        confess &log(ERROR, "${strKey} variable is already defined");
    }

    ${$self->{oVariable}}{$strKey} = $self->variableReplace($strValue);
}

####################################################################################################################################
# variableGet
#
# Get the current value of a variable.
####################################################################################################################################
sub variableGet
{
    my $self = shift;
    my $strKey = shift;

    return ${$self->{oVariable}}{$strKey};
}

####################################################################################################################################
# variableTest
#
# Test that a variable is defined or has an expected value.
####################################################################################################################################
sub variableTest
{
    my $self = shift;
    my $strKey = shift;
    my $strExpectedValue = shift;

    # Get the variable
    my $strValue = ${$self->{oVariable}}{$strKey};

    # Return false if it is not defined
    if (!defined($strValue))
    {
        return false;
    }

    # Return false if it does not equal the expected value
    if (defined($strExpectedValue) && $strValue ne $strExpectedValue)
    {
        return false;
    }

    return true;
}

####################################################################################################################################
# sourceGet
####################################################################################################################################
sub sourceGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strSource
    ) =
        logDebugParam
        (
            OP_DOC_MANIFEST_SOURCE_GET, \@_,
            {name => 'strSource', trace => true}
        );

    if (!defined(${$self->{oManifest}}{source}{$strSource}))
    {
        confess &log(ERROR, "source ${strSource} does not exist");
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oSource', value => ${$self->{oManifest}}{source}{$strSource}}
    );
}

####################################################################################################################################
# renderList
####################################################################################################################################
sub renderList
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(OP_DOC_MANIFEST_RENDER_LIST);

    # Check that the render output exists
    my @stryRender;

    if (defined(${$self->{oManifest}}{render}))
    {
        @stryRender = sort(keys(${$self->{oManifest}}{render}));
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'stryRender', value => \@stryRender}
    );
}

####################################################################################################################################
# renderGet
####################################################################################################################################
sub renderGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strType
    ) =
        logDebugParam
        (
            OP_DOC_MANIFEST_RENDER_GET, \@_,
            {name => 'strType', trace => true}
        );

    # Check that the render exists
    if (!defined(${$self->{oManifest}}{render}{$strType}))
    {
        confess &log(ERROR, "render type ${strType} does not exist");
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oRenderOut', value => ${$self->{oManifest}}{render}{$strType}}
    );
}

####################################################################################################################################
# renderOutList
####################################################################################################################################
sub renderOutList
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strType
    ) =
        logDebugParam
        (
            OP_DOC_MANIFEST_RENDER_OUT_LIST, \@_,
            {name => 'strType'}
        );

    # Check that the render output exists
    my @stryRenderOut;

    if (defined(${$self->{oManifest}}{render}{$strType}))
    {
        @stryRenderOut = sort(keys(${$self->{oManifest}}{render}{$strType}{out}));
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'stryRenderOut', value => \@stryRenderOut}
    );
}

####################################################################################################################################
# renderOutGet
####################################################################################################################################
sub renderOutGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strType,
        $strKey
    ) =
        logDebugParam
        (
            OP_DOC_MANIFEST_RENDER_OUT_GET, \@_,
            {name => 'strType', trace => true},
            {name => 'strKey', trace => true}
        );

    # use Data::Dumper; print Dumper(${$self->{oManifest}}{render});

    if (!defined(${$self->{oManifest}}{render}{$strType}{out}{$strKey}))
    {
        confess &log(ERROR, "render out ${strKey} does not exist");
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oRenderOut', value => ${$self->{oManifest}}{render}{$strType}{out}{$strKey}}
    );
}

1;
