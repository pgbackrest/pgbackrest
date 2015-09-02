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
        $self->{oDoc},
        $self->{strType},
        $self->{strProjectName},
        $self->{strExeName}
    ) =
        logDebugParam
        (
            OP_DOC_RENDER_NEW, \@_,
            {name => 'oDoc'},
            {name => 'strType'},
            {name => 'strProjectName'},
            {name => 'strExeName'}
        );

    $$oRenderTag{markdown}{backrest}[0] = $self->{strProjectName};
    $$oRenderTag{markdown}{exe}[0] = $self->{strExeName};

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
            {name => 'oDoc', default => $self->{oDoc}->{oDoc}, trace => true},
            {name => 'iDepth', default => 1, trace => true},
            {name => 'bChildList', default => true, trace => true}
        );

    my $strType = $self->{strType};
    my $strProjectName = $self->{strProjectName};

    my $strBuffer = "";
    my $bList = $$oDoc{name} =~ /.*-bullet-list$/;
    $bChildList = defined($bChildList) ? $bChildList : false;
    my $iChildDepth = $iDepth;

    my @stryMonth = ('January', 'February', 'March', 'April', 'May', 'June',
                     'July', 'August', 'September', 'October', 'November', 'December');

    if ($strType eq 'markdown')
    {
        if (defined($$oDoc{param}{id}))
        {
            my @stryToken = split('-', $$oDoc{name});
            my $strTitle = @stryToken == 0 ? '[unknown]' : $stryToken[@stryToken - 1];

            $strBuffer = ('#' x $iDepth) . " `$$oDoc{param}{id}` " . $strTitle;
        }

        if (defined($$oDoc{param}{title}))
        {
            $strBuffer = ('#' x $iDepth) . ' ';

            if (defined($$oDoc{param}{version}))
            {
                $strBuffer .= "v$$oDoc{param}{version}: ";
            }

            $strBuffer .= ($iDepth == 1 ? "${strProjectName} - " : '') . $$oDoc{param}{title};

            if (defined($$oDoc{param}{date}))
            {
                my $strDate = $$oDoc{param}{date};

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

        if (defined($$oDoc{field}{text}))
        {
            if ($strBuffer ne "")
            {
                $strBuffer .= "\n\n";
            }

            if ($bChildList)
            {
                $strBuffer .= '* ';
            }

            $strBuffer .= $self->processText($$oDoc{field}{text});
        }

        if ($$oDoc{name} eq 'config-key' || $$oDoc{name} eq 'option')
        {
            my $strError = "config section ?, key $$oDoc{param}{id} requires";

            my $bRequired = defined($$oDoc{field}{required}) && $$oDoc{field}{required};
            my $strDefault = $$oDoc{field}{default};
            my $strAllow = $$oDoc{field}{allow};
            my $strOverride = $$oDoc{field}{override};
            my $strExample = $$oDoc{field}{example};

            # !!! Temporary hack to make docs generate correctly.  This should be replace with a parameter so that it can be
            # changed based on the build.  Maybe check the exe name by default?
            if ($$oDoc{param}{id} eq 'config')
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

                $strExample = "$$oDoc{param}{id}${strExample}";

                if (defined($$oDoc{field}{cmd}) && $$oDoc{field}{cmd})
                {
                    $strExample = '--' . $strExample;

                    if (index($$oDoc{field}{example}, ' ') != -1)
                    {
                        $strExample = "\"${strExample}\"";
                    }
                }
            }

            $strBuffer .= "\n```\n" .
                          "required: " . ($bRequired ? 'y' : 'n') . "\n" .
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

    foreach my $oChild (@{$$oDoc{children}})
    {
        if ($strType eq 'markdown')
        {
        }
        else
        {
            confess "unknown type ${strType}";
        }

         $strBuffer .= $self->process($oChild, $iChildDepth, $bList);
    }

    if ($iDepth == 1)
    {
        if ($strType eq 'markdown')
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
    my $strTag = $$oTag{name};
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
    elsif (defined($$oTag{value}))
    {
        $strBuffer .= $$oTag{value};
    }
    elsif (defined($$oTag{children}[0]))
    {
        confess "GOT HERE" if !defined($self->{oDoc});

        foreach my $oSubTag (@{$self->{oDoc}->nodeList($oTag)})
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
    my $strBuffer = "";

    if (defined($$oText{children}))
    {
        for (my $iIndex = 0; $iIndex < @{$$oText{children}}; $iIndex++)
        {
            if (ref(\$$oText{children}[$iIndex]) eq "SCALAR")
            {
                $strBuffer .= $$oText{children}[$iIndex];
            }
            else
            {
                $strBuffer .= $self->processTag($$oText{children}[$iIndex]);
            }
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
