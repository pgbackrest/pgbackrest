####################################################################################################################################
# DOC MANIFEST MODULE
####################################################################################################################################
package pgBackRestDoc::Common::DocManifest;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Cwd qw(abs_path);
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);
use JSON::PP;

use pgBackRestDoc::Common::Log;
use pgBackRestDoc::Common::String;

####################################################################################################################################
# File constants
####################################################################################################################################
use constant FILE_MANIFEST                                          => 'manifest.xml';

####################################################################################################################################
# Render constants
####################################################################################################################################
use constant RENDER                                                 => 'render';
use constant RENDER_COMPACT                                         => 'compact';
    push @EXPORT, qw(RENDER_COMPACT);
use constant RENDER_FILE                                            => 'file';
use constant RENDER_MENU                                            => 'menu';
    push @EXPORT, qw(RENDER_MENU);
use constant RENDER_PRETTY                                          => 'pretty';
    push @EXPORT, qw(RENDER_PRETTY);

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
        $self->{oStorage},
        $self->{stryRequire},
        $self->{stryInclude},
        $self->{stryExclude},
        $self->{rhKeyVariableOverride},
        my $rhVariableOverride,
        $self->{strDocPath},
        $self->{bDeploy},
        $self->{bCacheOnly},
        $self->{bPre},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'oStorage'},
            {name => 'stryRequire'},
            {name => 'stryInclude'},
            {name => 'stryExclude'},
            {name => 'rhKeyVariableOverride', required => false},
            {name => 'rhVariableOverride', required => false},
            {name => 'strDocPath', required => false},
            {name => 'bDeploy', required => false},
            {name => 'bCacheOnly', required => false},
            {name => 'bPre', required => false, default => false},
        );

    # Set the bin path
    $self->{strBinPath} = abs_path(dirname($0));

    # Set the base path if it was not passed in
    if (!defined($self->{strDocPath}))
    {
        $self->{strDocPath} = $self->{strBinPath};
    }

    # Set cache file names
    $self->{strExeCacheLocal} = $self->{strDocPath} . "/output/exe.cache";
    $self->{strExeCacheDeploy} = $self->{strDocPath} . "/resource/exe.cache";

    # Load the manifest
    $self->{oManifestXml} = new pgBackRestDoc::Common::Doc("$self->{strDocPath}/manifest.xml");

    # Iterate the sources
    $self->{oManifest} = {};

    foreach my $oSource ($self->{oManifestXml}->nodeGet('source-list')->nodeList('source'))
    {
        my $oSourceHash = {};
        my $strKey = $oSource->paramGet('key');
        my $strSourceType = $oSource->paramGet('type', false);

        logDebugMisc
        (
            $strOperation, 'load source',
            {name => 'strKey', value => $strKey},
            {name => 'strSourceType', value => $strSourceType}
        );

        # Skip sources in exclude list
        if (grep(/^$strKey$/, @{$self->{stryExclude}}))
        {
            next;
        }

        $$oSourceHash{doc} = new pgBackRestDoc::Common::Doc("$self->{strDocPath}/xml/${strKey}.xml");

        # Read variables from source
        $self->variableListParse($$oSourceHash{doc}->nodeGet('variable-list', false), $rhVariableOverride);

        ${$self->{oManifest}}{source}{$strKey} = $oSourceHash;
        ${$self->{oManifest}}{source}{$strKey}{strSourceType} = $strSourceType;
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
        $${oRenderHash}{&RENDER_COMPACT} = $oRender->paramGet(RENDER_COMPACT, false, 'n') eq 'y' ? true : false;
        $${oRenderHash}{&RENDER_PRETTY} = $oRender->paramGet(RENDER_PRETTY, false, 'n') eq 'y' ? true : false;
        $${oRenderHash}{&RENDER_MENU} = false;

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

            # Skip sources in exclude list
            if (grep(/^$strSource$/, @{$self->{stryExclude}}))
            {
                next;
            }

            # Skip sources not in include list
            if (@{$self->{stryInclude}} > 0 && !grep(/^$strSource$/, @{$self->{stryInclude}}))
            {
                next;
            }

            # Preserve natural order
            push(@{$${oRenderHash}{stryOrder}}, $strKey);

            $$oRenderOutHash{source} = $strSource;

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

            # Get the menu caption
            if (defined($oRenderOut->paramGet('menu', false)) && $strType ne RENDER_TYPE_HTML)
            {
                confess &log(ERROR, "menu is only valid with html render type");
            }

            if (defined($oRenderOut->paramGet('menu', false)))
            {
                $${oRenderHash}{&RENDER_MENU} = true;

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

    # Set the doc path variable
    $self->variableSet('doc-path', $self->{strDocPath});

    # Read variables from manifest
    $self->variableListParse($self->{oManifestXml}->nodeGet('variable-list', false), $rhVariableOverride);

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

    return($self->variableTest('project-exe', 'pgbackrest'));
}

####################################################################################################################################
# Evaluate the if condition for a node
####################################################################################################################################
sub evaluateIf
{
    my $self = shift;
    my $oNode = shift;

    my $bIf = true;

    # Evaluate if condition
    if (defined($oNode->paramGet('if', false)))
    {
        my $strIf = $self->variableReplace($oNode->paramGet('if'));

        # In this case we really do want to evaluate the contents and not treat it as a literal
        $bIf = eval($strIf);

        # Error if the eval failed
        if ($@)
        {
            confess &log(ERROR, "unable to evaluate '${strIf}': $@");
        }
    }

    return $bIf;
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
        $rhVariableOverride
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->variableListParse', \@_,
            {name => '$oVariableList', required => false},
            {name => '$rhVariableOverride', required => false}
        );

    if (defined($oVariableList))
    {
        foreach my $oVariable ($oVariableList->nodeList('variable'))
        {
            if ($self->evaluateIf($oVariable))
            {
                my $strKey = $oVariable->paramGet('key');
                my $strValue = $self->variableReplace($oVariable->valueGet());

                if ($oVariable->paramTest('eval', 'y'))
                {
                    # In this case we really do want to evaluate the contents of strValue and not treat it as a literal.
                    $strValue = eval($strValue);

                    if ($@)
                    {
                        confess &log(ERROR, "unable to evaluate ${strKey}: $@\n" . $oVariable->valueGet());
                    }
                }

                $self->variableSet($strKey, defined($rhVariableOverride->{$strKey}) ? $rhVariableOverride->{$strKey} : $strValue);

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
# Get list of source documents
####################################################################################################################################
sub sourceList
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->sourceList');

    # Check that sources exist
    my @strySource;

    if (defined(${$self->{oManifest}}{source}))
    {
        @strySource = sort(keys(%{${$self->{oManifest}}{source}}));
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strySource', value => \@strySource}
    );
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
            __PACKAGE__ . '->sourceGet', \@_,
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
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->renderList');

    # Check that the render output exists
    my @stryRender;

    if (defined(${$self->{oManifest}}{render}))
    {
        @stryRender = sort(keys(%{${$self->{oManifest}}{render}}));
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
            __PACKAGE__ . '->renderGet', \@_,
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
            __PACKAGE__ . '->renderOutList', \@_,
            {name => 'strType'}
        );

    # Check that the render output exists
    my @stryRenderOut;

    if (defined(${$self->{oManifest}}{render}{$strType}))
    {
        @stryRenderOut = sort(keys(%{${$self->{oManifest}}{render}{$strType}{out}}));
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
        $strKey,
        $bIgnoreMissing,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->renderOutGet', \@_,
            {name => 'strType', trace => true},
            {name => 'strKey', trace => true},
            {name => 'bIgnoreMissing', default => false, trace => true},
        );

    if (!defined(${$self->{oManifest}}{render}{$strType}{out}{$strKey}) && !$bIgnoreMissing)
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

####################################################################################################################################
# cacheKey
####################################################################################################################################
sub cacheKey
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->cacheKey');

    # Generate a cache key from the variable override
    my $strVariableKey = JSON::PP->new()->canonical()->allow_nonref()->encode($self->{rhKeyVariableOverride});

    if ($strVariableKey eq '{}')
    {
        $strVariableKey = 'default';
    }

    my $strRequire = defined($self->{stryRequire}) && @{$self->{stryRequire}} > 0 ?
        join("\n", @{$self->{stryRequire}}) : 'all';

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strVariableKey', value => $strVariableKey},
        {name => 'strRequire', value => $strRequire},
    );
}

####################################################################################################################################
# cacheRead
####################################################################################################################################
sub cacheRead
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->cacheRead');

    $self->{hCache} = undef;

    my $strCacheFile = $self->{bDeploy} ? $self->{strExeCacheDeploy} : $self->{strExeCacheLocal};

    if (!$self->storage()->exists($strCacheFile) && !$self->{bDeploy})
    {
        $strCacheFile = $self->{strExeCacheDeploy};
    }

    if ($self->storage()->exists($strCacheFile))
    {
        my ($strCacheKey, $strRequire) = $self->cacheKey();
        my $oJSON = JSON::PP->new()->allow_nonref();
        $self->{hCache} = $oJSON->decode(${$self->storage()->get($strCacheFile)});

        foreach my $strSource (sort(keys(%{${$self->{oManifest}}{source}})))
        {
            my $hSource = ${$self->{oManifest}}{source}{$strSource};

            if (defined(${$self->{hCache}}{$strCacheKey}{$strRequire}{$strSource}))
            {
                $$hSource{hyCache} = ${$self->{hCache}}{$strCacheKey}{$strRequire}{$strSource};
                &log(DETAIL, "cache load $strSource (key = ${strCacheKey}, require = ${strRequire})");
            }
        }
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# cacheWrite
####################################################################################################################################
sub cacheWrite
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->cacheWrite');

    my $strCacheFile = $self->{bDeploy} ? $self->{strExeCacheDeploy} : $self->{strExeCacheLocal};
    my ($strCacheKey, $strRequire) = $self->cacheKey();

    foreach my $strSource (sort(keys(%{${$self->{oManifest}}{source}})))
    {
        my $hSource = ${$self->{oManifest}}{source}{$strSource};

        if (defined($$hSource{hyCache}))
        {
            ${$self->{hCache}}{$strCacheKey}{$strRequire}{$strSource} = $$hSource{hyCache};
            &log(DETAIL, "cache load $strSource (key = ${strCacheKey}, require = ${strRequire})");
        }
    }

    if (defined($self->{hCache}))
    {
        my $oJSON = JSON::PP->new()->canonical()->allow_nonref()->pretty();
        $self->storage()->put($strCacheFile, $oJSON->encode($self->{hCache}));
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# cacheReset
####################################################################################################################################
sub cacheReset
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
            __PACKAGE__ . '->cacheReset', \@_,
            {name => 'strSource', trace => true}
        );

    if ($self->{bCacheOnly})
    {
        confess &log(ERROR, 'Cache reset disabled by --cache-only option');
    }

    &log(WARN, "Cache will be reset for source ${strSource} and rendering retried automatically");
    delete(${$self->{oManifest}}{source}{$strSource}{hyCache});

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# Getters
####################################################################################################################################
sub storage {shift->{oStorage}};

1;
