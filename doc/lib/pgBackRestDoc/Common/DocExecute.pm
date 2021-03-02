####################################################################################################################################
# DOC EXECUTE MODULE
####################################################################################################################################
package pgBackRestDoc::Common::DocExecute;
use parent 'pgBackRestDoc::Common::DocRender';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Cwd qw(abs_path);
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);
use Storable qw(dclone);

use pgBackRestBuild::Config::Data;

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::HostTest;
use pgBackRestTest::Common::HostGroupTest;

use pgBackRestDoc::Common::DocManifest;
use pgBackRestDoc::Common::Exception;
use pgBackRestDoc::Common::Ini;
use pgBackRestDoc::Common::Log;
use pgBackRestDoc::Common::String;
use pgBackRestDoc::ProjectInfo;

####################################################################################################################################
# User that's building the docs
####################################################################################################################################
use constant DOC_USER                                              => getpwuid($UID) eq 'root' ? 'ubuntu' : getpwuid($UID) . '';

####################################################################################################################################
# Generate indexed defines
####################################################################################################################################
my $rhConfigDefineIndex = cfgDefine();

foreach my $strKey (sort(keys(%{$rhConfigDefineIndex})))
{
    # Build options for all possible db configurations
    if (defined($rhConfigDefineIndex->{$strKey}{&CFGDEF_PREFIX}) &&
        $rhConfigDefineIndex->{$strKey}{&CFGDEF_PREFIX} eq CFGDEF_PREFIX_PG)
    {
        my $strPrefix = $rhConfigDefineIndex->{$strKey}{&CFGDEF_PREFIX};

        for (my $iIndex = 1; $iIndex <= CFGDEF_INDEX_PG; $iIndex++)
        {
            my $strKeyNew = "${strPrefix}${iIndex}" . substr($strKey, length($strPrefix));

            $rhConfigDefineIndex->{$strKeyNew} = dclone($rhConfigDefineIndex->{$strKey});

            $rhConfigDefineIndex->{$strKeyNew}{&CFGDEF_INDEX_TOTAL} = CFGDEF_INDEX_PG;
            $rhConfigDefineIndex->{$strKeyNew}{&CFGDEF_INDEX} = $iIndex - 1;

            # Options indexed > 1 are never required
            if ($iIndex != 1)
            {
                $rhConfigDefineIndex->{$strKeyNew}{&CFGDEF_REQUIRED} = false;
            }

            if (defined($rhConfigDefineIndex->{$strKeyNew}{&CFGDEF_DEPEND}) &&
                defined($rhConfigDefineIndex->{$strKeyNew}{&CFGDEF_DEPEND}{&CFGDEF_DEPEND_OPTION}))
            {
                $rhConfigDefineIndex->{$strKeyNew}{&CFGDEF_DEPEND}{&CFGDEF_DEPEND_OPTION} =
                    "${strPrefix}${iIndex}" .
                    substr(
                        $rhConfigDefineIndex->{$strKeyNew}{&CFGDEF_DEPEND}{&CFGDEF_DEPEND_OPTION},
                        length($strPrefix));
            }
        }

        delete($rhConfigDefineIndex->{$strKey});
    }
    else
    {
        $rhConfigDefineIndex->{$strKey}{&CFGDEF_INDEX} = 0;
    }
}

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;       # Class name

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strType,
        $oManifest,
        $strRenderOutKey,
        $bExe
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strType'},
            {name => 'oManifest'},
            {name => 'strRenderOutKey'},
            {name => 'bExe'}
        );

    # Create the class hash
    my $self = $class->SUPER::new($strType, $oManifest, $bExe, $strRenderOutKey);
    bless $self, $class;

    if (defined($self->{oSource}{hyCache}))
    {
        $self->{bCache} = true;
        $self->{iCacheIdx} = 0;
    }
    else
    {
        $self->{bCache} = false;
    }

    $self->{bExe} = $bExe;

    $self->{iCmdLineLen} = $self->{oDoc}->paramGet('cmd-line-len', false, 80);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# executeKey
#
# Get a unique key for the execution step to determine if the cache is valid.
####################################################################################################################################
sub executeKey
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strHostName,
        $oCommand,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->executeKey', \@_,
            {name => 'strHostName', trace => true},
            {name => 'oCommand', trace => true},
        );

    # Add user to command
    my $bUserForce = $oCommand->paramTest('user-force', 'y') ? true : false;
    my $strCommand = $self->{oManifest}->variableReplace(trim($oCommand->fieldGet('exe-cmd')));
    my $strUser = $self->{oManifest}->variableReplace($oCommand->paramGet('user', false, DOC_USER));
    $strCommand = ($strUser eq DOC_USER || $bUserForce ? '' : ('sudo ' . ($strUser eq 'root' ? '' : "-u $strUser "))) . $strCommand;

    # Format and split command
    $strCommand =~ s/[ ]*\n[ ]*/ \\\n    /smg;
    $strCommand =~ s/ \\\@ \\//smg;
    my @stryCommand = split("\n", $strCommand);

    my $hCacheKey =
    {
        host => $strHostName,
        cmd => \@stryCommand,
        output => JSON::PP::false,
    };

    $$hCacheKey{'run-as-user'} = $bUserForce ? $strUser : undef;

    if (defined($oCommand->fieldGet('exe-cmd-extra', false)))
    {
        $$hCacheKey{'cmd-extra'} = $self->{oManifest}->variableReplace($oCommand->fieldGet('exe-cmd-extra'));
    }

    if (defined($oCommand->paramGet('err-expect', false)))
    {
        $$hCacheKey{'err-expect'} = $oCommand->paramGet('err-expect');
    }

    if ($oCommand->paramTest('output', 'y') || $oCommand->paramTest('show', 'y') || $oCommand->paramTest('variable-key'))
    {
        $$hCacheKey{'output'} = JSON::PP::true;
    }

    $$hCacheKey{'load-env'} = $oCommand->paramTest('load-env', 'n') ? JSON::PP::false : JSON::PP::true;
    $$hCacheKey{'bash-wrap'} = $oCommand->paramTest('bash-wrap', 'n') ? JSON::PP::false : JSON::PP::true;

    if (defined($oCommand->fieldGet('exe-highlight', false)))
    {
        $$hCacheKey{'output'} = JSON::PP::true;
        $$hCacheKey{highlight}{'filter'} = $oCommand->paramTest('filter', 'n') ? JSON::PP::false : JSON::PP::true;
        $$hCacheKey{highlight}{'filter-context'} = $oCommand->paramGet('filter-context', false, 2);

        my @stryHighlight;
        $stryHighlight[0] = $self->{oManifest}->variableReplace($oCommand->fieldGet('exe-highlight'));

        $$hCacheKey{highlight}{list} = \@stryHighlight;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'hExecuteKey', value => $hCacheKey, trace => true}
    );
}

####################################################################################################################################
# execute
####################################################################################################################################
sub execute
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oSection,
        $strHostName,
        $oCommand,
        $iIndent,
        $bCache,
        $bShow,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->execute', \@_,
            {name => 'oSection'},
            {name => 'strHostName'},
            {name => 'oCommand'},
            {name => 'iIndent', optional => true, default => 1},
            {name => 'bCache', optional => true, default => true},
            {name => 'bShow', optional => true, default => true},
        );

    # Working variables
    my $hCacheKey = $self->executeKey($strHostName, $oCommand);
    my $strCommand = join("\n", @{$$hCacheKey{cmd}});
    my $strOutput;

    if ($bShow && $self->{bExe} && $self->isRequired($oSection))
    {
        # Make sure that no lines are greater than 80 chars
        foreach my $strLine (split("\n", $strCommand))
        {
            if (length(trim($strLine)) > $self->{iCmdLineLen})
            {
                confess &log(ERROR,
                    "command has a line > $self->{iCmdLineLen} characters:\n${strCommand}\noffending line: ${strLine}");
            }
        }
    }

    &log(DEBUG, ('    ' x $iIndent) . "execute: $strCommand");

    if ($self->{oManifest}->variableReplace($oCommand->paramGet('skip', false, 'n')) ne 'y')
    {
        if ($self->{bExe} && $self->isRequired($oSection))
        {
            my ($bCacheHit, $strCacheType, $hCacheKey, $hCacheValue) = $self->cachePop('exe', $hCacheKey);

            if ($bCacheHit)
            {
                $strOutput = defined($$hCacheValue{output}) ? join("\n", @{$$hCacheValue{output}}) : undef;
            }
            else
            {
                # Check that the host is valid
                my $oHost = $self->{host}{$strHostName};

                if (!defined($oHost))
                {
                    confess &log(ERROR, "cannot execute on host ${strHostName} because the host does not exist");
                }

                my $oExec = $oHost->execute(
                    $strCommand . (defined($$hCacheKey{'cmd-extra'}) ? ' ' . $$hCacheKey{'cmd-extra'} : ''),
                    {iExpectedExitStatus => $$hCacheKey{'err-expect'},
                     bSuppressError => $oCommand->paramTest('err-suppress', 'y'),
                     iRetrySeconds => $oCommand->paramGet('retry', false)}, $hCacheKey->{'run-as-user'},
                     {bLoadEnv => $hCacheKey->{'load-env'}, bBashWrap => $hCacheKey->{'bash-wrap'}});
                $oExec->begin();
                $oExec->end();

                if (defined($oExec->{strOutLog}) && $oExec->{strOutLog} ne '')
                {
                    $strOutput = $oExec->{strOutLog};

                    # Trim off extra linefeeds before and after
                    $strOutput =~ s/^\n+|\n$//g;
                }

                if (defined($$hCacheKey{'err-expect'}) && defined($oExec->{strErrorLog}) && $oExec->{strErrorLog} ne '')
                {
                    $strOutput .= $oExec->{strErrorLog};
                }

                if ($$hCacheKey{output} && defined($$hCacheKey{highlight}) && $$hCacheKey{highlight}{filter} && defined($strOutput))
                {
                    my $strHighLight = @{$$hCacheKey{highlight}{list}}[0];

                    if (!defined($strHighLight))
                    {
                        confess &log(ERROR, 'filter requires highlight definition: ' . $strCommand);
                    }

                    my $iFilterContext = $$hCacheKey{highlight}{'filter-context'};

                    my @stryOutput = split("\n", $strOutput);
                    undef($strOutput);
                    # my $iFiltered = 0;
                    my $iLastOutput = -1;

                    for (my $iIndex = 0; $iIndex < @stryOutput; $iIndex++)
                    {
                        if ($stryOutput[$iIndex] =~ /$strHighLight/)
                        {
                            # Determine the first line to output
                            my $iFilterFirst = $iIndex - $iFilterContext;

                            # Don't go past the beginning
                            $iFilterFirst = $iFilterFirst < 0 ? 0 : $iFilterFirst;

                            # Don't repeat lines that have already been output
                            $iFilterFirst  = $iFilterFirst <= $iLastOutput ? $iLastOutput + 1 : $iFilterFirst;

                            # Determine the last line to output
                            my $iFilterLast = $iIndex + $iFilterContext;

                            # Don't got past the end
                            $iFilterLast = $iFilterLast >= @stryOutput ? @stryOutput -1 : $iFilterLast;

                            # Mark filtered lines if any
                            if ($iFilterFirst > $iLastOutput + 1)
                            {
                                my $iFiltered = $iFilterFirst - ($iLastOutput + 1);

                                if ($iFiltered > 1)
                                {
                                    $strOutput .= (defined($strOutput) ? "\n" : '') .
                                                  "       [filtered ${iFiltered} lines of output]";
                                }
                                else
                                {
                                    $iFilterFirst -= 1;
                                }
                            }

                            # Output the lines
                            for (my $iOutputIndex = $iFilterFirst; $iOutputIndex <= $iFilterLast; $iOutputIndex++)
                            {
                                    $strOutput .= (defined($strOutput) ? "\n" : '') . $stryOutput[$iOutputIndex];
                            }

                            $iLastOutput = $iFilterLast;
                        }
                    }

                    if (@stryOutput - 1 > $iLastOutput + 1)
                    {
                        my $iFiltered = (@stryOutput - 1) - ($iLastOutput + 1);

                        if ($iFiltered > 1)
                        {
                            $strOutput .= (defined($strOutput) ? "\n" : '') .
                                          "       [filtered ${iFiltered} lines of output]";
                        }
                        else
                        {
                            $strOutput .= (defined($strOutput) ? "\n" : '') . $stryOutput[-1];
                        }
                    }
                }

                if (!$$hCacheKey{output})
                {
                    $strOutput = undef;
                }

                if (defined($strOutput))
                {
                    my @stryOutput = split("\n", $strOutput);
                    $$hCacheValue{output} = \@stryOutput;
                }

                if ($bCache)
                {
                    $self->cachePush($strCacheType, $hCacheKey, $hCacheValue);
                }
            }

            # Output is assigned to a var
            if ($oCommand->paramTest('variable-key'))
            {
                $self->{oManifest}->variableSet($oCommand->paramGet('variable-key'), trim($strOutput), true);
            }
        }
        elsif ($$hCacheKey{output})
        {
            $strOutput = 'Output suppressed for testing';
        }
    }

    # Default variable output when it was not set by execution
    if ($oCommand->paramTest('variable-key') && !defined($self->{oManifest}->variableGet($oCommand->paramGet('variable-key'))))
    {
        $self->{oManifest}->variableSet($oCommand->paramGet('variable-key'), '[Test Variable]', true);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strCommand', value => $strCommand, trace => true},
        {name => 'strOutput', value => $strOutput, trace => true}
    );
}


####################################################################################################################################
# configKey
####################################################################################################################################
sub configKey
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oConfig,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->hostKey', \@_,
            {name => 'oConfig', trace => true},
        );

    my $hCacheKey =
    {
        host => $self->{oManifest}->variableReplace($oConfig->paramGet('host')),
        file => $self->{oManifest}->variableReplace($oConfig->paramGet('file')),
    };

    if ($oConfig->paramTest('reset', 'y'))
    {
        $$hCacheKey{reset} = JSON::PP::true;
    }

    # Add all options to the key
    my $strOptionTag = $oConfig->nameGet() eq 'backrest-config' ? 'backrest-config-option' : 'postgres-config-option';

    foreach my $oOption ($oConfig->nodeList($strOptionTag))
    {
        my $hOption = {};

        if ($oOption->paramTest('remove', 'y'))
        {
            $$hOption{remove} = JSON::PP::true;
        }

        if (defined($oOption->valueGet(false)))
        {
            $$hOption{value} = $self->{oManifest}->variableReplace($oOption->valueGet());
        }

        my $strKey = $self->{oManifest}->variableReplace($oOption->paramGet('key'));

        if ($oConfig->nameGet() eq 'backrest-config')
        {
            my $strSection = $self->{oManifest}->variableReplace($oOption->paramGet('section'));

            $$hCacheKey{option}{$strSection}{$strKey} = $hOption;
        }
        else
        {
            $$hCacheKey{option}{$strKey} = $hOption;
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'hCacheKey', value => $hCacheKey, trace => true}
    );
}

####################################################################################################################################
# backrestConfig
####################################################################################################################################
sub backrestConfig
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oSection,
        $oConfig,
        $iDepth
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->backrestConfig', \@_,
            {name => 'oSection'},
            {name => 'oConfig'},
            {name => 'iDepth'}
        );

    # Working variables
    my $hCacheKey = $self->configKey($oConfig);
    my $strFile = $$hCacheKey{file};
    my $strConfig = undef;

    &log(DEBUG, ('    ' x $iDepth) . 'process backrest config: ' . $$hCacheKey{file});

    if ($self->{bExe} && $self->isRequired($oSection))
    {
        my ($bCacheHit, $strCacheType, $hCacheKey, $hCacheValue) = $self->cachePop('cfg-' . PROJECT_EXE, $hCacheKey);

        if ($bCacheHit)
        {
            $strConfig = defined($$hCacheValue{config}) ? join("\n", @{$$hCacheValue{config}}) : undef;
        }
        else
        {
            # Check that the host is valid
            my $strHostName = $self->{oManifest}->variableReplace($oConfig->paramGet('host'));
            my $oHost = $self->{host}{$strHostName};

            if (!defined($oHost))
            {
                confess &log(ERROR, "cannot configure backrest on host ${strHostName} because the host does not exist");
            }

            # Reset all options
            if ($oConfig->paramTest('reset', 'y'))
            {
                delete(${$self->{config}}{$strHostName}{$$hCacheKey{file}})
            }

            foreach my $oOption ($oConfig->nodeList('backrest-config-option'))
            {
                my $strSection = $self->{oManifest}->variableReplace($oOption->paramGet('section'));
                my $strKey = $self->{oManifest}->variableReplace($oOption->paramGet('key'));
                my $strValue;

                if (!$oOption->paramTest('remove', 'y'))
                {
                    $strValue = $self->{oManifest}->variableReplace(trim($oOption->valueGet(false)));
                }

                if (!defined($strValue))
                {
                    delete(${$self->{config}}{$strHostName}{$$hCacheKey{file}}{$strSection}{$strKey});

                    if (keys(%{${$self->{config}}{$strHostName}{$$hCacheKey{file}}{$strSection}}) == 0)
                    {
                        delete(${$self->{config}}{$strHostName}{$$hCacheKey{file}}{$strSection});
                    }

                    &log(DEBUG, ('    ' x ($iDepth + 1)) . "reset ${strSection}->${strKey}");
                }
                else
                {
                    # Make sure the specified option exists
                    # ??? This is too simplistic to handle new indexed options.  The check below works for now but it would be good
                    # ??? to bring back more sophisticated checking in the future.
                    # if (!defined($rhConfigDefineIndex->{$strKey}))
                    # {
                    #     confess &log(ERROR, "option ${strKey} does not exist");
                    # }

                    # If this option is a hash and the value is already set then append to the array
                    if (defined($rhConfigDefineIndex->{$strKey}) &&
                        $rhConfigDefineIndex->{$strKey}{&CFGDEF_TYPE} eq CFGDEF_TYPE_HASH &&
                        defined(${$self->{config}}{$strHostName}{$$hCacheKey{file}}{$strSection}{$strKey}))
                    {
                        my @oValue = ();
                        my $strHashValue = ${$self->{config}}{$strHostName}{$$hCacheKey{file}}{$strSection}{$strKey};

                        # If there is only one key/value
                        if (ref(\$strHashValue) eq 'SCALAR')
                        {
                            push(@oValue, $strHashValue);
                        }
                        # Else if there is an array of values
                        else
                        {
                            @oValue = @{$strHashValue};
                        }

                        push(@oValue, $strValue);
                        ${$self->{config}}{$strHostName}{$$hCacheKey{file}}{$strSection}{$strKey} = \@oValue;
                    }
                    # else just set the value
                    else
                    {
                        ${$self->{config}}{$strHostName}{$$hCacheKey{file}}{$strSection}{$strKey} = $strValue;
                    }

                    &log(DEBUG, ('    ' x ($iDepth + 1)) . "set ${strSection}->${strKey} = ${strValue}");
                }
            }

            my $strLocalFile = '/home/' . DOC_USER . '/data/pgbackrest.conf';

            # Save the ini file
            $self->{oManifest}->storage()->put($strLocalFile, iniRender($self->{config}{$strHostName}{$$hCacheKey{file}}, true));

            $oHost->copyTo(
                $strLocalFile, $$hCacheKey{file},
                $self->{oManifest}->variableReplace($oConfig->paramGet('owner', false, 'postgres:postgres')), '640');

            # Remove the log-console-stderr option before pushing into the cache
            # ??? This is not very pretty and should be replaced with a general way to hide config options
            my $oConfigClean = dclone($self->{config}{$strHostName}{$$hCacheKey{file}});
            delete($$oConfigClean{&CFGDEF_SECTION_GLOBAL}{&CFGOPT_LOG_LEVEL_STDERR});
            delete($$oConfigClean{&CFGDEF_SECTION_GLOBAL}{&CFGOPT_LOG_TIMESTAMP});

            # Don't show repo1-storage-host option. Since Azure behaves differently with Azurite (which we use for local testing)
            # and the actual service we can't just fake /etc/hosts like we do for S3.
            delete($$oConfigClean{&CFGDEF_SECTION_GLOBAL}{'repo1-storage-host'});

            if (keys(%{$$oConfigClean{&CFGDEF_SECTION_GLOBAL}}) == 0)
            {
                delete($$oConfigClean{&CFGDEF_SECTION_GLOBAL});
            }

            $self->{oManifest}->storage()->put("${strLocalFile}.clean", iniRender($oConfigClean, true));

            # Push config file into the cache
            $strConfig = ${$self->{oManifest}->storage()->get("${strLocalFile}.clean")};

            my @stryConfig = undef;

            if (trim($strConfig) ne '')
            {
                @stryConfig = split("\n", $strConfig);
            }

            $$hCacheValue{config} = \@stryConfig;
            $self->cachePush($strCacheType, $hCacheKey, $hCacheValue);
        }
    }
    else
    {
        $strConfig = 'Config suppressed for testing';
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strFile', value => $strFile, trace => true},
        {name => 'strConfig', value => $strConfig, trace => true},
        {name => 'bShow', value => $oConfig->paramTest('show', 'n') ? false : true, trace => true}
    );
}

####################################################################################################################################
# postgresConfig
####################################################################################################################################
sub postgresConfig
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oSection,
        $oConfig,
        $iDepth
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->postgresConfig', \@_,
            {name => 'oSection'},
            {name => 'oConfig'},
            {name => 'iDepth'}
        );

    # Working variables
    my $hCacheKey = $self->configKey($oConfig);
    my $strFile = $$hCacheKey{file};
    my $strConfig;

    if ($self->{bExe} && $self->isRequired($oSection))
    {
        my ($bCacheHit, $strCacheType, $hCacheKey, $hCacheValue) = $self->cachePop('cfg-postgresql', $hCacheKey);

        if ($bCacheHit)
        {
            $strConfig = defined($$hCacheValue{config}) ? join("\n", @{$$hCacheValue{config}}) : undef;
        }
        else
        {
            # Check that the host is valid
            my $strHostName = $self->{oManifest}->variableReplace($oConfig->paramGet('host'));
            my $oHost = $self->{host}{$strHostName};

            if (!defined($oHost))
            {
                confess &log(ERROR, "cannot configure postgres on host ${strHostName} because the host does not exist");
            }

            my $strLocalFile = '/home/' . DOC_USER . '/data/postgresql.conf';
            $oHost->copyFrom($$hCacheKey{file}, $strLocalFile);

            if (!defined(${$self->{'pg-config'}}{$strHostName}{$$hCacheKey{file}}{base}) && $self->{bExe})
            {
                ${$self->{'pg-config'}}{$strHostName}{$$hCacheKey{file}}{base} =
                    ${$self->{oManifest}->storage()->get($strLocalFile)};
            }

            my $oConfigHash = $self->{'pg-config'}{$strHostName}{$$hCacheKey{file}};
            my $oConfigHashNew;

            if (!defined($$oConfigHash{old}))
            {
                $oConfigHashNew = {};
                $$oConfigHash{old} = {}
            }
            else
            {
                $oConfigHashNew = dclone($$oConfigHash{old});
            }

            &log(DEBUG, ('    ' x $iDepth) . 'process postgres config: ' . $$hCacheKey{file});

            foreach my $oOption ($oConfig->nodeList('postgres-config-option'))
            {
                my $strKey = $oOption->paramGet('key');
                my $strValue = $self->{oManifest}->variableReplace(trim($oOption->valueGet()));

                if ($strValue eq '')
                {
                    delete($$oConfigHashNew{$strKey});

                    &log(DEBUG, ('    ' x ($iDepth + 1)) . "reset ${strKey}");
                }
                else
                {
                    $$oConfigHashNew{$strKey} = $strValue;
                    &log(DEBUG, ('    ' x ($iDepth + 1)) . "set ${strKey} = ${strValue}");
                }
            }

            # Generate config text
            foreach my $strKey (sort(keys(%$oConfigHashNew)))
            {
                if (defined($strConfig))
                {
                    $strConfig .= "\n";
                }

                $strConfig .= "${strKey} = $$oConfigHashNew{$strKey}";
            }

            # Save the conf file
            if ($self->{bExe})
            {
                $self->{oManifest}->storage()->put($strLocalFile, $$oConfigHash{base} .
                                (defined($strConfig) ? "\n# pgBackRest Configuration\n${strConfig}\n" : ''));

                $oHost->copyTo($strLocalFile, $$hCacheKey{file}, 'postgres:postgres', '640');
            }

            $$oConfigHash{old} = $oConfigHashNew;

            my @stryConfig = undef;

            if (trim($strConfig) ne '')
            {
                @stryConfig = split("\n", $strConfig);
            }

            $$hCacheValue{config} = \@stryConfig;
            $self->cachePush($strCacheType, $hCacheKey, $hCacheValue);
        }
    }
    else
    {
        $strConfig = 'Config suppressed for testing';
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strFile', value => $strFile, trace => true},
        {name => 'strConfig', value => $strConfig, trace => true},
        {name => 'bShow', value => $oConfig->paramTest('show', 'n') ? false : true, trace => true}
    );
}

####################################################################################################################################
# hostKey
####################################################################################################################################
sub hostKey
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oHost,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->hostKey', \@_,
            {name => 'oHost', trace => true},
        );

    my $hCacheKey =
    {
        name => $self->{oManifest}->variableReplace($oHost->paramGet('name')),
        image => $self->{oManifest}->variableReplace($oHost->paramGet('image')),
    };

    if (defined($oHost->paramGet('id', false)))
    {
        $hCacheKey->{id} = $self->{oManifest}->variableReplace($oHost->paramGet('id'));
    }
    else
    {
        $hCacheKey->{id} = $hCacheKey->{name};
    }

    if (defined($oHost->paramGet('option', false)))
    {
        $$hCacheKey{option} = $self->{oManifest}->variableReplace($oHost->paramGet('option'));
    }

    if (defined($oHost->paramGet('param', false)))
    {
        $$hCacheKey{param} = $self->{oManifest}->variableReplace($oHost->paramGet('param'));
    }

    if (defined($oHost->paramGet('os', false)))
    {
        $$hCacheKey{os} = $self->{oManifest}->variableReplace($oHost->paramGet('os'));
    }

    $$hCacheKey{'update-hosts'} = $oHost->paramTest('update-hosts', 'n') ? JSON::PP::false : JSON::PP::true;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'hCacheKey', value => $hCacheKey, trace => true}
    );
}

####################################################################################################################################
# cachePop
####################################################################################################################################
sub cachePop
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strCacheType,
        $hCacheKey,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->hostKey', \@_,
            {name => 'strCacheType', trace => true},
            {name => 'hCacheKey', trace => true},
        );

    my $bCacheHit = false;
    my $oCacheValue = undef;

    if ($self->{bCache})
    {
        my $oJSON = JSON::PP->new()->canonical()->allow_nonref();
        # &log(WARN, "checking cache for\ncurrent key: " . $oJSON->encode($hCacheKey));

        my $hCache = ${$self->{oSource}{hyCache}}[$self->{iCacheIdx}];

        if (!defined($hCache))
        {
            confess &log(ERROR, 'unable to get index from cache', ERROR_FILE_INVALID);
        }

        if (!defined($$hCache{key}))
        {
            confess &log(ERROR, 'unable to get key from cache', ERROR_FILE_INVALID);
        }

        if (!defined($$hCache{type}))
        {
            confess &log(ERROR, 'unable to get type from cache', ERROR_FILE_INVALID);
        }

        if ($$hCache{type} ne $strCacheType)
        {
            confess &log(ERROR, 'types do not match, cache is invalid', ERROR_FILE_INVALID);
        }

        if ($oJSON->encode($$hCache{key}) ne $oJSON->encode($hCacheKey))
        {
            confess &log(ERROR,
                "keys at index $self->{iCacheIdx} do not match, cache is invalid." .
                "\n  cache key: " . $oJSON->encode($$hCache{key}) .
                "\ncurrent key: " . $oJSON->encode($hCacheKey), ERROR_FILE_INVALID);
        }

        $bCacheHit = true;
        $oCacheValue = $$hCache{value};
        $self->{iCacheIdx}++;
    }
    else
    {
        if ($self->{oManifest}{bCacheOnly})
        {
            confess &log(ERROR, 'Cache only operation forced by --cache-only option');
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bCacheHit', value => $bCacheHit, trace => true},
        {name => 'strCacheType', value => $strCacheType, trace => true},
        {name => 'hCacheKey', value => $hCacheKey, trace => true},
        {name => 'oCacheValue', value => $oCacheValue, trace => true},
    );
}

####################################################################################################################################
# cachePush
####################################################################################################################################
sub cachePush
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strType,
        $hCacheKey,
        $oCacheValue,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->hostKey', \@_,
            {name => 'strType', trace => true},
            {name => 'hCacheKey', trace => true},
            {name => 'oCacheValue', required => false, trace => true},
        );

    if ($self->{bCache})
    {
        confess &log(ASSERT, "cachePush should not be called when cache is already present");
    }

    # Create the cache entry
    my $hCache =
    {
        key => $hCacheKey,
        type => $strType,
    };

    if (defined($oCacheValue))
    {
        $$hCache{value} = $oCacheValue;
    }

    push @{$self->{oSource}{hyCache}}, $hCache;

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# sectionChildProcesss
####################################################################################################################################
sub sectionChildProcess
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oSection,
        $oChild,
        $iDepth
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->sectionChildProcess', \@_,
            {name => 'oSection'},
            {name => 'oChild'},
            {name => 'iDepth'}
        );

    &log(DEBUG, ('    ' x ($iDepth + 1)) . 'process child: ' . $oChild->nameGet());

    # Execute a command
    if ($oChild->nameGet() eq 'host-add')
    {
        if ($self->{bExe} && $self->isRequired($oSection))
        {
            my ($bCacheHit, $strCacheType, $hCacheKey, $hCacheValue) = $self->cachePop('host', $self->hostKey($oChild));

            if ($bCacheHit)
            {
                $self->{oManifest}->variableSet('host-' . $hCacheKey->{id} . '-ip', $hCacheValue->{ip}, true);
            }
            else
            {
                if (defined($self->{host}{$$hCacheKey{name}}))
                {
                    confess &log(ERROR, 'cannot add host ${strName} because the host already exists');
                }

                executeTest("rm -rf ~/data/$$hCacheKey{name}");
                executeTest("mkdir -p ~/data/$$hCacheKey{name}/etc");

                my $strHost = $hCacheKey->{name};
                my $strImage = $hCacheKey->{image};
                my $strHostUser = $self->{oManifest}->variableReplace($oChild->paramGet('user'));

                # Determine if a pre-built image should be created
                if (defined($self->preExecute($strHost)))
                {
                    my $strPreImage = "${strImage}-${strHost}";
                    my $strFrom = $strImage;

                    &log(INFO, "Build vm '${strPreImage}' from '${strFrom}'");

                    my $strCommandList;

                    # Add all pre commands
                    foreach my $oExecute ($self->preExecute($strHost))
                    {
                        my $hExecuteKey = $self->executeKey($strHost, $oExecute);

                        my $strCommand =
                            join("\n", @{$hExecuteKey->{cmd}}) .
                            (defined($hExecuteKey->{'cmd-extra'}) ? ' ' . $hExecuteKey->{'cmd-extra'} : '');
                        $strCommand =~ s/'/'\\''/g;

                        $strCommand =
                            "sudo -u ${strHostUser}" .
                            ($hCacheKey->{'bash-wrap'} ?
                                " bash" . ($hCacheKey->{'load-env'} ? ' -l' : '') . " -c '${strCommand}'" : " ${strCommand}");

                        if (defined($strCommandList))
                        {
                            $strCommandList .= "\n";
                        }

                        $strCommandList .= "RUN ${strCommand}";

                        &log(DETAIL, "    Pre command $strCommand");
                    }

                    # Build container
                    my $strDockerfile = $self->{oManifest}{strDocPath} . "/output/doc-host.dockerfile";

                    $self->{oManifest}{oStorage}->put(
                        $strDockerfile,
                        "FROM ${strFrom}\n\n" . trim($self->{oManifest}->variableReplace($strCommandList)) . "\n");
                    executeTest("docker build -f ${strDockerfile} -t ${strPreImage} " . $self->{oManifest}{oStorage}->pathGet());

                    # Use the pre-built image
                    $strImage = $strPreImage;
                }

                my $strHostRepoPath = dirname(dirname(abs_path($0)));

                # Replace host repo path in mounts with if present
                my $strMount = undef;

                if (defined($oChild->paramGet('mount', false)))
                {
                    $strMount = $self->{oManifest}->variableReplace($oChild->paramGet('mount'));
                    $strMount =~ s/\{\[host\-repo\-path\]\}/${strHostRepoPath}/g;
                }

                # Replace host repo mount in params if present
                my $strOption = $$hCacheKey{option};

                if (defined($strOption))
                {
                    $strOption =~ s/\{\[host\-repo\-path\]\}/${strHostRepoPath}/g;
                }

                my $oHost = new pgBackRestTest::Common::HostTest(
                    $$hCacheKey{name}, "doc-$$hCacheKey{name}", $strImage, $strHostUser, $$hCacheKey{os},
                    defined($strMount) ? [$strMount] : undef, $strOption, $$hCacheKey{param}, $$hCacheKey{'update-hosts'});

                $self->{host}{$$hCacheKey{name}} = $oHost;
                $self->{oManifest}->variableSet('host-' . $hCacheKey->{id} . '-ip', $oHost->{strIP}, true);
                $$hCacheValue{ip} = $oHost->{strIP};

                # Add to the host group
                my $oHostGroup = hostGroupGet();
                $oHostGroup->hostAdd($oHost);

                # Execute initialize commands
                foreach my $oExecute ($oChild->nodeList('execute', false))
                {
                    $self->execute(
                        $oSection, $$hCacheKey{name}, $oExecute, {iIndent => $iDepth + 1, bCache => false, bShow => false});
                }

                $self->cachePush($strCacheType, $hCacheKey, $hCacheValue);
            }
        }
    }
    # Skip children that have already been processed and error on others
    elsif ($oChild->nameGet() ne 'title')
    {
        confess &log(ASSERT, 'unable to process child type ' . $oChild->nameGet());
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

1;
