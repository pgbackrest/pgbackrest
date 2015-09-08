####################################################################################################################################
# DOC RENDER MODULE
####################################################################################################################################
package BackRestDoc::DocRender;

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
use constant OP_DOC_RENDER                                          => 'DocRender';

use constant OP_DOC_RENDER_PROCESS                                  => OP_DOC_RENDER . '->process';
use constant OP_DOC_RENDER_PROCESS_TAG                              => OP_DOC_RENDER . '->processTag';
use constant OP_DOC_RENDER_PROCESS_TEXT                             => OP_DOC_RENDER . '->processText';
use constant OP_DOC_RENDER_NEW                                      => OP_DOC_RENDER . '->new';
use constant OP_DOC_RENDER_SAVE                                     => OP_DOC_RENDER . '->save';

# use HTML::HTML5::Builder qw[:standard JQUERY];

####################################################################################################################################
# Render tags for various output types
####################################################################################################################################
my $oRenderTag =
{
    'markdown' =>
    {
        'b' => ['**', '**'],
        'i' => ['_', '_'],
        'bi' => ['_**', '**_'],
        'ul' => ["\n", ''],
        'ol' => ["\n", ''],
        'li' => ['- ', "\n"],
        'id' => ['`', '`'],
        'file' => ['`', '`'],
        'path' => ['`', '`'],
        'cmd' => ['`', '`'],
        'param' => ['`', '`'],
        'setting' => ['`', '`'],
        'code' => ['`', '`'],
        'code-block' => ['```', '```'],
        'exe' => [undef, ''],
        'backrest' => [undef, ''],
        'postgres' => ['PostgreSQL', '']
    },

    'text' =>
    {
        'b' => ['', ''],
        'i' => ['', ''],
        'bi' => ['', ''],
        'ul' => ["\n", ''],
        'ol' => ["\n", ''],
        'li' => ['* ', "\n"],
        'id' => ['', ''],
        'file' => ['', ''],
        'path' => ['', ''],
        'cmd' => ['', ''],
        'param' => ['', ''],
        'setting' => ['', ''],
        'code' => ['', ''],
        'code-block' => ['', ''],
        'exe' => [undef, ''],
        'backrest' => [undef, ''],
        'postgres' => ['PostgreSQL', '']
    },

    'html' =>
    {
        'b' => ['<b>', '</b>']
    }
};

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
        $self->{strType},
        $self->{strProjectName},
        $self->{strExeName}
    ) =
        logDebugParam
        (
            OP_DOC_RENDER_NEW, \@_,
            {name => 'strType'},
            {name => 'strProjectName'},
            {name => 'strExeName'}
        );

    $$oRenderTag{markdown}{backrest}[0] = $self->{strProjectName};
    $$oRenderTag{markdown}{exe}[0] = $self->{strExeName};
    $$oRenderTag{text}{backrest}[0] = $self->{strProjectName};
    $$oRenderTag{text}{exe}[0] = $self->{strExeName};

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
# Render the document
####################################################################################################################################
sub process
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oDoc,
        $iDepth,
        $bChildList,
    ) =
        logDebugParam
        (
            OP_DOC_RENDER_PROCESS, \@_,
            {name => 'oDoc', trace => true},
            {name => 'iDepth', default => 1, trace => true},
            {name => 'bChildList', default => true, trace => true}
        );

    my $strType = $self->{strType};
    my $strProjectName = $self->{strProjectName};

    my $strBuffer = "";
    my $bList = $oDoc->nameGet() =~ /.*-bullet-list$/;
    $bChildList = defined($bChildList) ? $bChildList : false;
    my $iChildDepth = $iDepth;

    my @stryMonth = ('January', 'February', 'March', 'April', 'May', 'June',
                     'July', 'August', 'September', 'October', 'November', 'December');

    if ($strType eq 'markdown' || $strType eq 'text')
    {
        if (defined($oDoc->paramGet('id', false)))
        {
            my @stryToken = split('-', $oDoc->nameGet());
            my $strTitle = @stryToken == 0 ? '[unknown]' : $stryToken[@stryToken - 1];

            $strBuffer = ('#' x $iDepth) . ' `' . $oDoc->paramGet('id') . '` ' . $strTitle;
        }

        if (defined($oDoc->paramGet('title', false)))
        {
            $strBuffer = ('#' x $iDepth) . ' ';

            if (defined($oDoc->paramGet('version', false)))
            {
                $strBuffer .= 'v' . $oDoc->paramGet('version') . ': ';
            }

            $strBuffer .= ($iDepth == 1 ? "${strProjectName} - " : '') . $oDoc->paramGet('title');

            if (defined($oDoc->paramGet('date', false)))
            {
                my $strDate = $oDoc->paramGet('date');

                if ($strDate !~ /^(XXXX-XX-XX)|([0-9]{4}-[0-9]{2}-[0-9]{2})$/)
                {
                    confess "invalid date ${strDate}";
                }

                if ($strDate =~ /^X/)
                {
                    $strBuffer .= "\n__No Release Date Set__";
                }
                else
                {
                    $strBuffer .= "\n__Released " . $stryMonth[(substr($strDate, 5, 2) - 1)] . ' ' .
                                  (substr($strDate, 8, 2) + 0) . ', ' . substr($strDate, 0, 4) . '__';
                }
            }
        }

        if ($strBuffer ne "")
        {
            $iChildDepth++;
        }

        if (defined($oDoc->nodeGet('summary', false)))
        {
            if ($strBuffer ne "")
            {
                $strBuffer .= "\n\n";
            }

            if ($bChildList)
            {
                $strBuffer .= '* ';
            }

            $strBuffer .= $self->processText($oDoc->nodeGet('summary')->textGet());
        }

        if (defined($oDoc->textGet(false)))
        {
            if ($strBuffer ne "")
            {
                $strBuffer .= "\n\n";
            }

            if ($bChildList)
            {
                $strBuffer .= '* ';
            }

            $strBuffer .= $self->processText($oDoc->textGet());
        }

        if ($oDoc->nameGet() eq 'config-key' || $oDoc->nameGet() eq 'option')
        {
            my $strError = 'config section ?, key ' . $oDoc->paramGet('id') . 'requires';

            my $bRequired = defined($oDoc->fieldGet('required', false)) && $oDoc->fieldGet('required');
            my $strDefault = $oDoc->fieldGet('default', false);
            my $strAllow = $oDoc->fieldGet('allow', false);
            my $strOverride = $oDoc->fieldGet('override', false);
            my $strExample = $oDoc->fieldGet('example', false);

            # !!! Temporary hack to make docs generate correctly.  This should be replace with a parameter so that it can be
            # changed based on the build.  Maybe check the exe name by default?
            if ($oDoc->paramGet('id') eq 'config')
            {
                $strDefault = '/etc/pg_backrest.conf';
            }

            if (defined($strExample))
            {
                if (index($strExample, '=') == -1)
                {
                    $strExample = "=${strExample}";
                }
                else
                {
                    $strExample = " ${strExample}";
                }

                $strExample = $oDoc->paramGet('id') . $strExample;

                if (defined($oDoc->fieldGet('cmd', false)) && $oDoc->fieldGet('cmd'))
                {
                    $strExample = '--' . $strExample;

                    if (index($oDoc->fieldGet('example'), ' ') != -1)
                    {
                        $strExample = "\"${strExample}\"";
                    }
                }
            }

            $strBuffer .= "\n```\n" .
                          ($bRequired ? "required: " . ($bRequired ? 'y' : 'n') . "\n" : '') .
                          (defined($strDefault) ? "default: ${strDefault}\n" : '') .
                          (defined($strAllow) ? "allow: ${strAllow}\n" : '') .
                          (defined($strOverride) ? "override: ${strOverride}\n" : '') .
                          (defined($strExample) ? "example: ${strExample}\n" : '') .
                          "```";
        }

        if ($strBuffer ne "" && $iDepth != 1 && !$bList)
        {
            $strBuffer = "\n\n" . $strBuffer;
        }
    }
    else
    {
        confess "unknown type ${strType}";
    }

    my $bFirst = true;

    foreach my $oChild ($oDoc->nodeList(undef, false))
    {
        if ($oChild->nameGet() ne 'summary')
        {
            if ($strType eq 'markdown' || $strType eq 'text')
            {
            }
            else
            {
                confess "unknown type ${strType}";
            }

             $strBuffer .= $self->process($oChild, $iChildDepth, $bList);
        }
    }

    if ($iDepth == 1)
    {
        if ($strType eq 'markdown' || $strType eq 'text')
        {
            $strBuffer .= "\n";
        }
        else
        {
            confess "unknown type ${strType}";
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strBuffer', value => $strBuffer, trace => true}
    );
}

####################################################################################################################################
# processTag
####################################################################################################################################
sub processTag
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oTag
    ) =
        logDebugParam
        (
            OP_DOC_RENDER_PROCESS_TAG, \@_,
            {name => 'oTag', trace => true}
        );

    my $strBuffer = "";

    my $strType = $self->{strType};
    my $strTag = $oTag->nameGet();
    my $strStart = $$oRenderTag{$strType}{$strTag}[0];
    my $strStop = $$oRenderTag{$strType}{$strTag}[1];

    if (!defined($strStart) || !defined($strStop))
    {
        confess "invalid type ${strType} or tag ${strTag}";
    }

    $strBuffer .= $strStart;

    if ($strTag eq 'li' || $strTag eq 'code-block')
    {
        $strBuffer .= $self->processText($oTag);
    }
    elsif (defined($oTag->valueGet()))
    {
        $strBuffer .= $oTag->valueGet();
    }
    else
    {
        foreach my $oSubTag ($oTag->nodeList(undef, false))
        {
            $strBuffer .= $self->processTag($oSubTag);
        }
    }

    $strBuffer .= $strStop;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strBuffer', value => $strBuffer, trace => true}
    );
}

####################################################################################################################################
# processText
####################################################################################################################################
sub processText
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oText
    ) =
        logDebugParam
        (
            OP_DOC_RENDER_PROCESS_TEXT, \@_,
            {name => 'oText', trace => true}
        );

    my $strType = $self->{strType};
    my $strBuffer = '';

    foreach my $oNode ($oText->nodeList(undef, false))
    {
        if (ref(\$oNode) eq "SCALAR")
        {
            $strBuffer .= $oNode;
        }
        else
        {
            $strBuffer .= $self->processTag($oNode);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strBuffer', value => $strBuffer, trace => true}
    );
}

####################################################################################################################################
# save
####################################################################################################################################
sub save
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFileName,
        $strBuffer
    ) =
        logDebugParam
        (
            OP_DOC_RENDER_SAVE, \@_,
            {name => 'strFileName', trace => true},
            {name => 'strBuffer', trace => true}
        );

    # Open the file
    my $hFile;
    open($hFile, '>', $strFileName)
        or confess &log(ERROR, "unable to open ${strFileName}");

    # Write the buffer
    my $iBufferOut = syswrite($hFile, $strBuffer);

    # Report any errors
    if (!defined($iBufferOut) || $iBufferOut != length($strBuffer))
    {
        confess "unable to write '${strBuffer}'" . (defined($!) ? ': ' . $! : '');
    }

    # Close the file
    close($hFile);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

1;
