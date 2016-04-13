####################################################################################################################################
# DOC EXECUTE MODULE
####################################################################################################################################
package BackRestDoc::Common::DocExecute;
use parent 'BackRestDoc::Common::DocRender';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);
use Storable qw(dclone);

use lib dirname($0) . '/../lib';
use BackRest::Common::Ini;
use BackRest::Common::Log;
use BackRest::Common::String;
use BackRest::FileCommon;

use lib dirname($0) . '/../test/lib';
use BackRestTest::Common::ExecuteTest;
use BackRestTest::Common::HostTest;

use BackRestDoc::Common::DocManifest;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_DOC_EXECUTE                                         => 'DocExecute';

use constant OP_DOC_EXECUTE_BACKREST_CONFIG                         => OP_DOC_EXECUTE . '->backrestConfig';
use constant OP_DOC_EXECUTE_EXECUTE                                 => OP_DOC_EXECUTE . '->execute';
use constant OP_DOC_EXECUTE_NEW                                     => OP_DOC_EXECUTE . '->new';
use constant OP_DOC_EXECUTE_POSTGRES_CONFIG                         => OP_DOC_EXECUTE . '->postresConfig';
use constant OP_DOC_EXECUTE_SECTION_CHILD_PROCESS                   => OP_DOC_EXECUTE . '->sectionChildProcess';

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
            OP_DOC_EXECUTE_NEW, \@_,
            {name => 'strType'},
            {name => 'oManifest'},
            {name => 'strRenderOutKey'},
            {name => 'bExe'}
        );

    # Create the class hash
    my $self = $class->SUPER::new($strType, $oManifest, $strRenderOutKey);
    bless $self, $class;

    $self->{bExe} = $bExe;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
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
        $iIndent
    ) =
        logDebugParam
        (
            OP_DOC_EXECUTE_EXECUTE, \@_,
            {name => 'oSection'},
            {name => 'strHostName'},
            {name => 'oCommand'},
            {name => 'iIndent', default => 1}
        );

    # Working variables
    my $strCommand;
    my $strOutput;

    if ($oCommand->fieldTest('actual-command'))
    {
        $strCommand = $oCommand->fieldGet('actual-command');
        $strOutput = $oCommand->fieldGet('actual-output', false);
    }
    else
    {
        # Command variables
        $strCommand = trim($oCommand->fieldGet('exe-cmd'));
        my $strUser = $self->{oManifest}->variableReplace($oCommand->paramGet('user', false, 'postgres'));
        my $bExeOutput = $oCommand->paramTest('output', 'y');
        my $strVariableKey = $oCommand->paramGet('variable-key', false);
        my $iExeExpectedError = $oCommand->paramGet('err-expect', false);

        $strCommand = $self->{oManifest}->variableReplace(
            ($strUser eq 'vagrant' ? '' :
                ('sudo ' . ($strUser eq 'root' ? '' : "-u ${strUser} "))) . $strCommand);

        # Add continuation chars and proper spacing
        $strCommand =~ s/[ ]*\n[ ]*/ \\\n    /smg;

        if (!$oCommand->paramTest('show', 'n') && $self->{bExe} && $self->isRequired($oSection))
        {
            # Make sure that no lines are greater than 80 chars
            foreach my $strLine (split("\n", $strCommand))
            {
                if (length(trim($strLine)) > 80)
                {
                    confess &log(ERROR, "command has a line > 80 characters:\n${strCommand}\noffending line: ${strLine}");
                }
            }
        }

        &log(DEBUG, ('    ' x $iIndent) . "execute: $strCommand");

        if (!$oCommand->paramTest('skip', 'y'))
        {
            if ($self->{bExe} && $self->isRequired($oSection))
            {
                # Check that the host is valid
                my $oHost = $self->{host}{$strHostName};

                if (!defined($oHost))
                {
                    confess &log(ERROR, "cannot execute on host ${strHostName} because the host does not exist");
                }

                my $oExec = $oHost->execute($strCommand,
                                            {iExpectedExitStatus => $iExeExpectedError,
                                             bSuppressError => $oCommand->paramTest('err-suppress', 'y'),
                                             iRetrySeconds => $oCommand->paramGet('retry', false)});
                $oExec->begin();
                $oExec->end();

                if ($bExeOutput && defined($oExec->{strOutLog}) && $oExec->{strOutLog} ne '')
                {
                    $strOutput = $oExec->{strOutLog};

                    # Trim off extra linefeeds before and after
                    $strOutput =~ s/^\n+|\n$//g;

                    if ($strCommand =~ / pg\_backrest /)
                    {
                        $strOutput =~ s/^                             //smg;
                        $strOutput =~ s/^[0-9]{4}-[0-1][0-9]-[0-3][0-9] [0-2][0-9]:[0-6][0-9]:[0-6][0-9]\.[0-9]{3} T[0-9]{2} //smg;
                    }
                }

                if (defined($iExeExpectedError))
                {
                    $strOutput .= trim($oExec->{strErrorLog});
                }

                # Output is assigned to a var
                if (defined($strVariableKey))
                {
                    $self->{oManifest}->variableSet($strVariableKey, trim($oExec->{strOutLog}));
                }
                elsif (!$oCommand->paramTest('filter', 'n') && $bExeOutput && defined($strOutput))
                {
                    my $strHighLight = $self->{oManifest}->variableReplace($oCommand->fieldGet('exe-highlight', false));

                    if (!defined($strHighLight))
                    {
                        confess &log(ERROR, 'filter requires highlight definition: ' . $strCommand);
                    }

                    my $iFilterContext = $oCommand->paramGet('filter-context', false, 2);

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
                            $strOutput .= (defined($strOutput) ? "\n" : '') . $stryOutput[@stryOutput - 1];
                        }
                    }
                }
            }
            elsif ($bExeOutput)
            {
                $strOutput = 'Output suppressed for testing';
            }
        }

        if (defined($strVariableKey) && !defined($self->{oManifest}->variableGet($strVariableKey)))
        {
            $self->{oManifest}->variableSet($strVariableKey, '[Test Variable]');
        }

        $oCommand->fieldSet('actual-command', $strCommand);
        $oCommand->fieldSet('actual-output', $strOutput);
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
            OP_DOC_EXECUTE_BACKREST_CONFIG, \@_,
            {name => 'oSection'},
            {name => 'oConfig'},
            {name => 'iDepth'}
        );

    # Working variables
    my $strFile;
    my $strConfig;

    if ($oConfig->fieldTest('actual-file'))
    {
        $strFile = $oConfig->fieldGet('actual-file');
        $strConfig = $oConfig->fieldGet('actual-config');
    }
    else
    {
        # Get filename
        $strFile = $self->{oManifest}->variableReplace($oConfig->paramGet('file'));

        &log(DEBUG, ('    ' x $iDepth) . 'process backrest config: ' . $strFile);

        if ($self->{bExe} && $self->isRequired($oSection))
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
                delete(${$self->{config}}{$strHostName}{$strFile})
            }

            foreach my $oOption ($oConfig->nodeList('backrest-config-option'))
            {
                my $strSection = $oOption->paramGet('section');
                my $strKey = $oOption->paramGet('key');
                my $strValue;

                if (!$oOption->paramTest('remove', 'y'))
                {
                    $strValue = $self->{oManifest}->variableReplace(trim($oOption->valueGet(false)));
                }

                if (!defined($strValue))
                {
                    delete(${$self->{config}}{$strHostName}{$strFile}{$strSection}{$strKey});

                    if (keys(${$self->{config}}{$strHostName}{$strFile}{$strSection}) == 0)
                    {
                        delete(${$self->{config}}{$strHostName}{$strFile}{$strSection});
                    }

                    &log(DEBUG, ('    ' x ($iDepth + 1)) . "reset ${strSection}->${strKey}");
                }
                else
                {
                    ${$self->{config}}{$strHostName}{$strFile}{$strSection}{$strKey} = $strValue;
                    &log(DEBUG, ('    ' x ($iDepth + 1)) . "set ${strSection}->${strKey} = ${strValue}");
                }
            }

            my $strLocalFile = "/home/vagrant/data/db-master/etc/pg_backrest.conf";

            # Save the ini file
            iniSave($strLocalFile, $self->{config}{$strHostName}{$strFile}, true);

            $strConfig = fileStringRead($strLocalFile);

            $oHost->copyTo($strLocalFile, $strFile, $oConfig->paramGet('owner', false, 'postgres:postgres'), '640');
        }
        else
        {
            $strConfig = 'Config suppressed for testing';
        }

        $oConfig->fieldSet('actual-file', $strFile);
        $oConfig->fieldSet('actual-config', $strConfig);
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
            OP_DOC_EXECUTE_POSTGRES_CONFIG, \@_,
            {name => 'oSection'},
            {name => 'oConfig'},
            {name => 'iDepth'}
        );

    # Working variables
    my $strFile;
    my $strConfig;

    if ($oConfig->fieldTest('actual-file'))
    {
        $strFile = $oConfig->fieldGet('actual-file');
        $strConfig = $oConfig->fieldGet('actual-config');
    }
    else
    {
        # Get filename
        $strFile = $self->{oManifest}->variableReplace($oConfig->paramGet('file'));

        if ($self->{bExe} && $self->isRequired($oSection))
        {
            # Check that the host is valid
            my $strHostName = $self->{oManifest}->variableReplace($oConfig->paramGet('host'));
            my $oHost = $self->{host}{$strHostName};

            if (!defined($oHost))
            {
                confess &log(ERROR, "cannot configure postgres on host ${strHostName} because the host does not exist");
            }

            my $strLocalFile = '/home/vagrant/data/db-master/etc/postgresql.conf';
            $oHost->copyFrom($strFile, $strLocalFile);

            if (!defined(${$self->{'pg-config'}}{$strHostName}{$strFile}{base}) && $self->{bExe})
            {
                ${$self->{'pg-config'}}{$strHostName}{$strFile}{base} = fileStringRead($strLocalFile);
            }

            my $oConfigHash = $self->{'pg-config'}{$strHostName}{$strFile};
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

            &log(DEBUG, ('    ' x $iDepth) . 'process postgres config: ' . $strFile);

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
                fileStringWrite($strLocalFile, $$oConfigHash{base} .
                                (defined($strConfig) ? "\n# pgBackRest Configuration\n${strConfig}\n" : ''));

                $oHost->copyTo($strLocalFile, $strFile, 'postgres:postgres', '640');
            }

            $$oConfigHash{old} = $oConfigHashNew;
        }
        else
        {
            $strConfig = 'Config suppressed for testing';
        }

        $oConfig->fieldSet('actual-file', $strFile);
        $oConfig->fieldSet('actual-config', $strConfig);
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
            OP_DOC_EXECUTE_SECTION_CHILD_PROCESS, \@_,
            {name => 'oSection'},
            {name => 'oChild'},
            {name => 'iDepth'}
        );

    &log(DEBUG, ('    ' x ($iDepth + 1)) . 'process child: ' . $oChild->nameGet());

    # Execute a command
    if ($oChild->nameGet() eq 'host-add')
    {
        if ($self->{bExe} && $self->isRequired($oSection) && !$oChild->paramTest('created', true))
        {
            my $strName = $self->{oManifest}->variableReplace($oChild->paramGet('name'));
            my $strUser = $self->{oManifest}->variableReplace($oChild->paramGet('user'));
            my $strImage = $self->{oManifest}->variableReplace($oChild->paramGet('image'));
            my $strOS = $self->{oManifest}->variableReplace($oChild->paramGet('os', false));
            my $strMount = $self->{oManifest}->variableReplace($oChild->paramGet('mount', false));

            if (defined($self->{host}{$strName}))
            {
                confess &log(ERROR, 'cannot add host ${strName} because the host already exists');
            }

            my $oHost = new BackRestTest::Common::HostTest($strName, $strImage, $strUser, $strOS, $strMount);
            $self->{host}{$strName} = $oHost;
            $self->{oManifest}->variableSet("host-${strName}-ip", $oHost->{strIP});

            # Execute cleanup commands
            foreach my $oExecute ($oChild->nodeList('execute'))
            {
                $self->execute($oSection, $strName, $oExecute, $iDepth + 1);
            }

            $oHost->executeSimple("sh -c 'echo \"\" >> /etc/hosts\'", undef, 'root');
            $oHost->executeSimple("sh -c 'echo \"# Test Hosts\" >> /etc/hosts'", undef, 'root');

            # Add all other host IPs to this host
            foreach my $strOtherHostName (sort(keys($self->{host})))
            {
                if ($strOtherHostName ne $strName)
                {
                    my $oOtherHost = $self->{host}{$strOtherHostName};

                    $oHost->executeSimple("sh -c 'echo \"$oOtherHost->{strIP} ${strOtherHostName}\" >> /etc/hosts'", undef, 'root');
                }
            }

            # Add this host IP to all other hosts
            foreach my $strOtherHostName (sort(keys($self->{host})))
            {
                if ($strOtherHostName ne $strName)
                {
                    my $oOtherHost = $self->{host}{$strOtherHostName};

                    $oOtherHost->executeSimple("sh -c 'echo \"$oHost->{strIP} ${strName}\" >> /etc/hosts'", undef, 'root');
                }
            }

            $oChild->paramSet('created', true);
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
