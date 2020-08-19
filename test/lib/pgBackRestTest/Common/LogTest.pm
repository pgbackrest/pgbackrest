####################################################################################################################################
# LogTest.pm - Capture the output of commands to compare them with an expected version
####################################################################################################################################
package pgBackRestTest::Common::LogTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Cwd qw(abs_path);
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);

use pgBackRestDoc::Common::Log;
use pgBackRestDoc::ProjectInfo;

use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::ExecuteTest;

####################################################################################################################################
# Expect log cache for supplemental files so they are only added to expect logs when they have changed
####################################################################################################################################
my $hyExpectCache = undef;

####################################################################################################################################
# new
####################################################################################################################################
sub new
{
    my $class = shift;          # Class name

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Assign function parameters, defaults, and log debug info
    (
        my $strOperation,
        $self->{strModule},
        $self->{strTest},
        $self->{iRun},
        $self->{bForce},
        $self->{strComment},
        $self->{strCommandMain},
        $self->{strPgSqlBin},
        $self->{strTestPath},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strModule', trace => true},
            {name => 'strTest', trace => true},
            {name => 'iRun', trace => true},
            {name => 'bForce', trace => true},
            {name => 'strComment', trace => true},
            {name => 'strCommandMain', trace => true},
            {name => 'strPgSqlBin', required => false, trace => true},
            {name => 'strTestPath', trace => true},
        );

    # Initialize the test log
    $self->{strLog} = 'run ' . sprintf('%03d', $self->{iRun}) . ' - ' . $self->{strComment};
    $self->{strLog} .= "\n" . ('=' x length($self->{strLog})) . "\n";

    # Initialize the filename
    $self->{strFileName} = sprintf("expect/$self->{strModule}-$self->{strTest}-%03d.log", $self->{iRun});

    # Initialize the replacement hash
    $self->{oReplaceHash} = {};

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self, trace => true}
    );
}

####################################################################################################################################
# logAdd
####################################################################################################################################
sub logAdd
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strCommand,
        $strComment,
        $strLog
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->logAdd', \@_,
            {name => 'strCommand', trace => true},
            {name => 'strComment', required => false, trace => true},
            {name => 'strLog', required => false, trace => true}
        );

    $self->{strLog} .= "\n";

    if (defined($strComment))
    {
        $self->{strLog} .= $self->regExpReplaceAll($strComment) . "\n";
    }

    $self->{strLog} .= '> ' . $self->regExpReplaceAll($strCommand) . "\n" . ('-' x '132') . "\n";

    # Make sure there is a log before trying to output it
    if (defined($strLog))
    {
        # Do replacements on each line of the log
        foreach my $strLine (split("\n", $strLog))
        {
            # Remove timestamp from log line
            $strLine =~ s/^[0-9]{4}-[0-1][0-9]-[0-3][0-9] [0-2][0-9]:[0-6][0-9]:[0-6][0-9]\.[0-9]{3} //;

            # Don't include test points in the final log
            if ($strLine !~ /^  TEST/)
            {
                # Remove spaces that provided padding for the timetamp
                $strLine =~ s/^                            //;

                # Remove any final CRs
                $strLine =~ s/\r$//;

                # Do other replacements
                $strLine = $self->regExpReplaceAll($strLine);

                # Add to the log
                $self->{strLog} .= "${strLine}\n";
            }
        }
    }

    # Return from function and log return values if any
    logDebugReturn($strOperation);
}

####################################################################################################################################
# supplementalAdd
####################################################################################################################################
sub supplementalAdd
{
    my $self = shift;
    my $strFileName = shift;
    my $strComment = shift;
    my $strContent = shift;

    # Get content if it is not defined
    if (!defined($strContent))
    {
        open(my $hFile, '<', $strFileName)
            or confess &log(ERROR, "unable to open ${strFileName} for appending to test log");

        while (my $strLine = readline($hFile))
        {
            $strContent .= $strLine;
        }

        close($hFile);
    }

    # Only ouput when the content of this file has changed
    if (!defined($hyExpectCache->{$strFileName}) || $hyExpectCache->{$strFileName} ne $strContent)
    {
        # $hyExpectCache->{$strFileName} = $strContent;

        my $strHeader = "+ supplemental file: " . $self->regExpReplaceAll($strFileName);

        if (defined($strComment))
        {
            $self->{strLog} .= "\n" . $self->regExpReplaceAll($strComment) . "\n" . ('=' x '132') . "\n";
        }

        $self->{strLog} .= "\n${strHeader}\n" . ('-' x length($strHeader)) . "\n";

        if (length($strContent) > 0)
        {
            foreach my $strLine (split("\n", $strContent))
            {
                $self->{strLog} .= $self->regExpReplaceAll($strLine) . "\n";
            }
        }
    }
}

####################################################################################################################################
# logWrite
####################################################################################################################################
sub logWrite
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strBasePath,
        $strTestPath,
        $strFileName
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->logWrite', \@_,
            {name => 'strBasePath', trace => true},
            {name => 'strTestPath', trace => true},
            {name => 'strFileName', default => $self->{strFileName}, trace => true}
        );

    my $strReferenceLogFile = "${strBasePath}/test/${strFileName}";
    my $strTestLogFile;

    if ($self->{bForce})
    {
        $strTestLogFile = $strReferenceLogFile;
    }
    else
    {
        my $strTestLogPath = "${strTestPath}/expect";

        if (!-e $strTestLogPath)
        {
            mkdir($strTestLogPath, 0750) or
                confess "unable to create expect log path ${strTestLogPath}";
        }

        $strTestLogFile = "${strTestPath}/${strFileName}";
    }

    open(my $hFile, '>', $strTestLogFile)
        or confess "unable to open expect log file '${strTestLogFile}': $!";

    syswrite($hFile, $self->{strLog})
        or confess "unable to write expect log file '${strTestLogFile}': $!";

    close($hFile);

    if (!$self->{bForce})
    {
        executeTest("diff ${strReferenceLogFile} ${strTestLogFile}");
    }

    # Return from function and log return values if any
    logDebugReturn($strOperation);
}

####################################################################################################################################
# regExpReplace
####################################################################################################################################
sub regExpReplace
{
    my $self = shift;
    my $strLine = shift;
    my $strType = shift;
    my $strExpression = shift;
    my $strToken = shift;
    my $bIndex = shift;

    my @stryReplace = ($strLine =~ /$strExpression/g);

    foreach my $strReplace (@stryReplace)
    {
        my $iIndex;
        my $strTypeReplacement;
        my $strReplacement;

        if (!defined($bIndex) || $bIndex)
        {
            if (defined($strToken))
            {
                my @stryReplacement = ($strReplace =~ /$strToken/g);

                if (@stryReplacement != 1)
                {
                    my $strError = "'${strToken}'";

                    if (@stryReplacement == 0)
                    {
                        confess &log(ASSERT, $strError . " is not a sub-regexp of '${strExpression}' or" .
                                             " matches " . @stryReplacement . " times on {[${strReplace}]}");
                    }

                    confess &log(
                        ASSERT, $strError . " matches '${strExpression}' " . @stryReplacement . " times on '${strReplace}': " .
                        join(',', @stryReplacement));
                }

                $strReplacement = $stryReplacement[0];
            }
            else
            {
                $strReplacement = $strReplace;
            }

            if (defined($strType))
            {
                if (defined(${$self->{oReplaceHash}}{$strType}{$strReplacement}))
                {
                    $iIndex = ${$self->{oReplaceHash}}{$strType}{$strReplacement}{index};
                }
                else
                {
                    if (!defined(${$self->{oReplaceHash}}{$strType}{index}))
                    {
                        ${$self->{oReplaceHash}}{$strType}{index} = 1;
                    }

                    $iIndex = ${$self->{oReplaceHash}}{$strType}{index}++;
                    ${$self->{oReplaceHash}}{$strType}{$strReplacement}{index} = $iIndex;
                }
            }
        }

        $strTypeReplacement = defined($strType) ? "[${strType}" . (defined($iIndex) ? "-${iIndex}" : '') . ']' : '';

        if (defined($strToken))
        {
            $strReplacement = $strReplace;
            $strReplacement =~ s/$strToken/$strTypeReplacement/;
        }
        else
        {
            $strReplacement = $strTypeReplacement;
        }

        $strReplace =~ s/\//\\\//g;
        $strReplace =~ s/\+/\\\+/g;
        $strLine =~ s/$strReplace/$strReplacement/g;
    }

    return $strLine;
}

####################################################################################################################################
# regExpReplaceAll
#
# Replaces dynamic test output so that the expected test output can be verified against actual test output.
####################################################################################################################################
sub regExpReplaceAll
{
    my $self = shift;
    my $strLine = shift;

    my $strBinPath = dirname(dirname(abs_path($0))) . '/bin';

    # Replace the exe path/file with sh ' prepended
    my $strLineBefore = $strLine;
    $strLine =~ s/bash -l -c '$self->{strCommandMain}/[BACKREST-BIN]/g;

    # Replace the final ' if the previous expression made changes
    if ($strLine ne $strLineBefore)
    {
        $strLine =~ s/'$//g;
    }

    # Replace the exe path/file with sh ' prepended
    $strLine =~ s/$self->{strCommandMain}/[BACKREST-BIN]/g;

    # Replace the test path
    $strLine =~ s/$self->{strTestPath}/[TEST_PATH]/g;

    # Replace the pgsql path (if exists)
    if (defined($self->{strPgSqlBin}))
    {
        $strLine =~ s/$self->{strPgSqlBin}/[PGSQL_BIN_PATH]/g;
    }

    $strLine = $self->regExpReplace($strLine, 'BACKREST-NAME-VERSION', '^' . PROJECT_NAME . ' ' . PROJECT_VERSION,
                                                undef, false);
    $strLine = $self->regExpReplace(
        $strLine, 'BACKREST-VERSION', ' begin ' . PROJECT_VERSION . '\:', PROJECT_VERSION, false);

    $strLine = $self->regExpReplace($strLine, undef, '^docker exec -u [a-z]* test-[0-9]+\-', 'test-[0-9]+\-', false);
    $strLine = $self->regExpReplace($strLine, 'CONTAINER-EXEC', '^docker exec -u [a-z]*', '^docker exec -u [a-z]*', false);

    $strLine = $self->regExpReplace($strLine, 'PROCESS-ID', 'sent term signal to process [0-9]+', '[0-9]+$', false);
    $strLine = $self->regExpReplace($strLine, 'YEAR', 'backup\.history\/20[0-9]{2}', '20[0-9]{2}$');

    $strLine = $self->regExpReplace($strLine, 'BACKUP-INCR', '[0-9]{8}\-[0-9]{6}F\_[0-9]{8}\-[0-9]{6}I');
    $strLine = $self->regExpReplace($strLine, 'BACKUP-DIFF', '[0-9]{8}\-[0-9]{6}F\_[0-9]{8}\-[0-9]{6}D');
    $strLine = $self->regExpReplace($strLine, 'BACKUP-FULL', '[0-9]{8}\-[0-9]{6}F');

    $strLine = $self->regExpReplace(
        $strLine, 'BACKUP-EXPR', 'strExpression \= \_[0-9]{8}\-[0-9]{6}', '\_[0-9]{8}\-[0-9]{6}$', false);

    if ($strLine =~ /^P00 DETAIL\: statistics\: /)
    {
        $strLine = 'P00 DETAIL: statistics: STATISTICS'
    }

    $strLine = $self->regExpReplace($strLine, 'GROUP', 'strGroup = [^ \n,\[\]]+', '[^ \n,\[\]]+$');
    $strLine = $self->regExpReplace($strLine, 'GROUP', 'unknown group in backup manifest mapped to \'[^\']+', '[^\']+$');
    $strLine = $self->regExpReplace($strLine, 'GROUP', 'group"[ ]{0,1}:[ ]{0,1}"[^"]+', '[^"]+$');
    $strLine = $self->regExpReplace($strLine, 'GROUP', 'group=\"[^"]+', '[^"]+$');
    $strLine = $self->regExpReplace($strLine, 'GROUP', 'group for .* was not mapped to a name\, set to .+$', '[^ ]+$');
    $strLine = $self->regExpReplace($strLine, 'GROUP', 'set ownership [^\:]+:[^ ]+', '[^\:]+$');
    $strLine = $self->regExpReplace($strLine, 'GROUP', TEST_USER . '\, ' . TEST_GROUP, '[^ ]+$');

    $strLine = $self->regExpReplace($strLine, 'USER', 'strRemoteHostUser = [^ \n,\[\]]+', '[^ \n,\[\]]+$');
    $strLine = $self->regExpReplace($strLine, 'USER', 'unknown user in backup manifest mapped to \'[^\']+', '[^\']+$');
    $strLine = $self->regExpReplace($strLine, 'USER', 'strUser = [^ \n,\[\]]+', '[^ \n,\[\]]+$');
    $strLine = $self->regExpReplace($strLine, 'USER', 'user"[ ]{0,1}:[ ]{0,1}"[^"]+', '[^"]+$');
    $strLine = $self->regExpReplace($strLine, 'USER', 'user=\"[^"]+', '[^"]+$');
    $strLine = $self->regExpReplace($strLine, 'USER', '^pg-host-user=.+$', '[^=]+$');
    $strLine = $self->regExpReplace($strLine, 'USER', 'user for .* was not mapped to a name\, set to .+$', '[^ ]+$');
    $strLine = $self->regExpReplace($strLine, 'USER', 'set ownership [^\:]+', '[^ ]+$');
    $strLine = $self->regExpReplace($strLine, 'USER', 'cannot be used for restore\, set to .+$', '[^ ]+$');
    $strLine = $self->regExpReplace($strLine, 'USER', '-user=[a-z0-9_]+', '[^=]+$');
    $strLine = $self->regExpReplace($strLine, 'USER', '[^ ]+\@db\-primary', '^[^\@]+');
    $strLine = $self->regExpReplace($strLine, 'USER', '[\( ]{1}' . TEST_USER . '[\,\)]{1}', TEST_USER);

    $strLine = $self->regExpReplace($strLine, 'CIPHER-PASS', '^cipher\-pass\=.+$', '[^\=]+$');

    $strLine = $self->regExpReplace($strLine, 'PORT', 'db[1-9]{0,1}-port=[0-9]+', '[0-9]+$');

    # Replace year when it falls on a single line when executing ls -1R
    $strLine = $self->regExpReplace($strLine, 'YEAR', '^20[0-9]{2}$');

    # Replace year when it is the last part of a path when executing ls -1R
    $strLine = $self->regExpReplace($strLine, 'YEAR', 'history\/20[0-9]{2}\:$', '20[0-9]{2}');

    my $strTimestampRegExp = "[0-9]{4}-[0-1][0-9]-[0-3][0-9] [0-2][0-9]:[0-6][0-9]:[0-6][0-9]";

    $strLine = $self->regExpReplace($strLine, 'TS_PATH', "PG\\_[0-9]\\.[0-9]\\_[0-9]{9}");
    my $strProjectVersion = PROJECT_VERSION;
    $strProjectVersion =~ s/\./\\./g;
    $strLine = $self->regExpReplace($strLine, 'VERSION',
        "version[\"]{0,1}[ ]{0,1}[\:\=)]{1}[ ]{0,1}[\"]{0,1}" . $strProjectVersion, $strProjectVersion . '$');

    $strLine = $self->regExpReplace($strLine, 'TIMESTAMP', 'timestamp"[ ]{0,1}:[ ]{0,1}[0-9]{10}','[0-9]{10}$');

    $strLine = $self->regExpReplace($strLine, 'TIMESTAMP',
        "timestamp-[a-z-]+[\"]{0,1}[ ]{0,1}[\:\=)]{1}[ ]{0,1}[\"]{0,1}[0-9]{10}", '[0-9]{10}$', false);
    $strLine = $self->regExpReplace($strLine, 'TIMESTAMP', "start\" : [0-9]{10}", '[0-9]{10}$', false);
    $strLine = $self->regExpReplace($strLine, 'TIMESTAMP', "stop\" : [0-9]{10}", '[0-9]{10}$', false);
    $strLine = $self->regExpReplace($strLine, 'TIMESTAMP', TEST_GROUP . '\, [0-9]{10}', '[0-9]{10}$', false);
    $strLine = $self->regExpReplace(
        $strLine, 'TIMESTAMP', 'generated by pgBackRest restore on .*$',
        '[0-9]{4}\-[0-9]{2}\-[0-9]{2} [0-9]{2}\:[0-9]{2}\:[0-9]{2}$', false);
    $strLine = $self->regExpReplace($strLine, 'SIZE', "size\"[ ]{0,1}:[ ]{0,1}[0-9]+", '[0-9]+$', false);
    $strLine = $self->regExpReplace($strLine, 'DELTA', "delta\"[ ]{0,1}:[ ]{0,1}[0-9]+", '[0-9]+$', false);
    $strLine = $self->regExpReplace(
        $strLine, 'TIMESTAMP-STR', "timestamp start\/stop: $strTimestampRegExp / $strTimestampRegExp",
        "${strTimestampRegExp} / ${strTimestampRegExp}\$", false);
    $strLine = $self->regExpReplace($strLine, 'CHECKSUM', 'checksum=[\"]{0,1}[0-f]{40}', '[0-f]{40}$', false);

    $strLine = $self->regExpReplace($strLine, 'LOG-LEVEL-FILE', 'log-level-file=[a-z]+', '[a-z]+$', false);
    $strLine = $self->regExpReplace($strLine, 'LOG-SUBPROCESS', 'log-subprocess=[a-z]+', '[a-z]+$', false);
    $strLine = $self->regExpReplace($strLine, '', ' --log-subprocess', undef, false);
    $strLine = $self->regExpReplace($strLine, '', ' --no-log-subprocess', undef, false);
    $strLine = $self->regExpReplace($strLine, 'BUFFER-SIZE', 'buffer-size=[0-9a-z]+', '[0-9a-z]+$', false);

    $strLine = $self->regExpReplace($strLine, 'REMOTE-PROCESS-TERMINATED-MESSAGE',
        'remote process terminated.*: (ssh.*|no output from terminated process)$',
        '(ssh.*|no output from terminated process)$', false);

    # Full test time-based recovery
    $strLine = $self->regExpReplace($strLine, 'TIMESTAMP-TARGET', "\\, target \\'.*UTC", "[^\\']+UTC\$");
    $strLine = $self->regExpReplace($strLine, 'TIMESTAMP-TARGET', " \\-\\-target\\=\\\".*UTC", "[^\\\"]+UTC\$");
    $strLine = $self->regExpReplace($strLine, 'TIMESTAMP-TARGET', "^recovery_target_time \\= \\'.*UTC", "[^\\']+UTC\$");

    $strLine = $self->regExpReplace($strLine, 'TIMESTAMP-TARGET', "\\, target \\'.*\\+00", "[^\\']+\\+00\$");
    $strLine = $self->regExpReplace($strLine, 'TIMESTAMP-TARGET', " \\-\\-target\\=\\\".*\\+00", "[^\\\"]+\\+00\$");
    $strLine = $self->regExpReplace($strLine, 'TIMESTAMP-TARGET', "^recovery_target_time \\= \\'.*\\+00", "[^\\']+\\+00\$");

    # Full test xid-based recovery (this expressions only work when time-based expressions above have already been applied
    $strLine = $self->regExpReplace($strLine, 'XID-TARGET', "\\, target \\'[0-9]+", "[0-9]+\$");
    $strLine = $self->regExpReplace($strLine, 'XID-TARGET', " \\-\\-target\\=\\\"[0-9]+", "[0-9]+\$");
    $strLine = $self->regExpReplace($strLine, 'XID-TARGET', "^recovery_target_xid \\= \\'[0-9]+", "[0-9]+\$");

    $strLine = $self->regExpReplace(
        $strLine, 'MODIFICATION-TIME', '(' . (substr(time(), 0, 5) - 1) . '[0-9]{5}|' . substr(time(), 0, 5) . '[0-9]{5})',
        '^[0-9]{10}$');

    return $strLine;
}

1;
