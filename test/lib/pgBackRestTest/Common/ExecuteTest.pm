####################################################################################################################################
# ExecuteTest.pm - Module to execute external commands
####################################################################################################################################
package pgBackRestTest::Common::ExecuteTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use IO::Select;
use IPC::Open3;
use POSIX ':sys_wait_h';
use Symbol 'gensym';

use pgBackRest::Common::Io::Handle;
use pgBackRest::Common::Io::Buffered;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;

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
        $self->{strCommand},
        my $oParam
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strCommand'},
            {name => 'oParam', required => false}
        );

    # Load params
    foreach my $strParam (sort(keys(%$oParam)))
    {
        $self->{$strParam} = $$oParam{$strParam};
    }

    # Set defaults
    $self->{bSuppressError} = defined($self->{bSuppressError}) ? $self->{bSuppressError} : false;
    $self->{bSuppressStdErr} = defined($self->{bSuppressStdErr}) ? $self->{bSuppressStdErr} : false;
    $self->{bShowOutput} = defined($self->{bShowOutput}) ? $self->{bShowOutput} : false;
    $self->{bShowOutputAsync} = defined($self->{bShowOutputAsync}) ? $self->{bShowOutputAsync} : false;
    $self->{iExpectedExitStatus} = defined($self->{iExpectedExitStatus}) ? $self->{iExpectedExitStatus} : 0;
    $self->{iRetrySeconds} = defined($self->{iRetrySeconds}) ? $self->{iRetrySeconds} : undef;
    $self->{bLogOutput} = defined($self->{bLogOutput}) ? $self->{bLogOutput} : true;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# begin
####################################################################################################################################
sub begin
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->begin');

    $self->{strErrorLog} = '';
    $self->{strOutLog} = '';

    if (defined($self->{oTestLog}))
    {
        if (defined($self->{strComment}))
        {
            $self->{strFullLog} = $self->{oTestLog}->regExpAll($self->{strComment}) . "\n";
        }

        $self->{strFullLog} .= '> ' . $self->{oTestLog}->regExpAll($self->{strCommand}) . "\n" . ('-' x '132') . "\n";
    }

    &log(DETAIL, "executing command: $self->{strCommand}");

    # Execute the command
    $self->{hError} = gensym;

    $self->{pId} = open3(undef, $self->{hOut}, $self->{hError}, $self->{strCommand});

    # Create select objects
    $self->{oIo} = new pgBackRest::Common::Io::Buffered(new pgBackRest::Common::Io::Handle('exec test', $self->{hOut}), 120, 65536);

    if (!defined($self->{hError}))
    {
        confess 'STDERR handle is undefined';
    }

    return logDebugReturn($strOperation);
}

####################################################################################################################################
# endRetry
####################################################################################################################################
sub endRetry
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strTest,
        $bWait
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->endRetry', \@_,
            {name => 'strTest', required => false, trace => true},
            {name => 'bWait', required => false, default => true, trace => true}
        );

    # Drain the output and error streams and look for test points
    while(waitpid($self->{pId}, WNOHANG) == 0)
    {
        my $bFound = false;

        # Drain the stdout stream and look for test points
        while (defined(my $strLine = $self->{oIo}->readLine(true)))
        {
            $self->{strOutLog} .= "${strLine}\n";
            $bFound = true;

            if ($self->{bShowOutputAsync})
            {
                syswrite(*STDOUT, "    ${strLine}\n")
            }

            if (defined($strTest) && testCheck($strLine, $strTest))
            {
                &log(DEBUG, "Found test ${strTest}");
                return true;
            }
        }

        if (!$bWait)
        {
            return;
        }

        if (!$bFound)
        {
            waitHiRes(.05);
        }
    }

    # Check the exit status
    my $iExitStatus = ${^CHILD_ERROR_NATIVE} >> 8;

    # Drain the stdout stream
    while (defined(my $strLine = $self->{oIo}->readLine(true)))
    {
        $self->{strOutLog} .= "${strLine}\n";

        if ($self->{bShowOutputAsync})
        {
            syswrite(*STDOUT, "    ${strLine}\n")
        }
    }

    # Drain the stderr stream
    my $oIoError = new pgBackRest::Common::Io::Buffered(new pgBackRest::Common::Io::Handle('exec test', $self->{hError}), 0, 65536);

    while (defined(my $strLine = $oIoError->readLine(true, false)))
    {
        $self->{strErrorLog} .= "${strLine}\n";
    }

    # Pass the log to the LogTest object
    if (defined($self->{oLogTest}))
    {
        if (defined($self->{strErrorLog}) && $self->{strErrorLog} ne '')
        {
            $self->{strOutLog} .= "STDERR:\n" . $self->{strErrorLog};
        }

        $self->{oLogTest}->logAdd($self->{strCommand}, $self->{strComment}, $self->{bLogOutput} ? $self->{strOutLog} : undef);
    }

    # If an error was expected then return success if that error occurred
    if ($self->{iExpectedExitStatus} != 0 && $iExitStatus == $self->{iExpectedExitStatus})
    {
        return $iExitStatus;
    }

    if ($iExitStatus != 0 || ($self->{iExpectedExitStatus} != 0 && $iExitStatus != $self->{iExpectedExitStatus}))
    {
        if ($self->{bSuppressError})
        {
            &log(DEBUG, "suppressed error was ${iExitStatus}");
            $self->{strErrorLog} = '';
        }
        else
        {
            if (defined($self->{iRetrySeconds}))
            {
                $self->{bRetry} = true;
                return;
            }
            else
            {
                confess &log(ERROR, "command '$self->{strCommand}' returned " . $iExitStatus .
                             ($self->{iExpectedExitStatus} != 0 ? ", but $self->{iExpectedExitStatus} was expected" : '') . "\n" .
                             ($self->{strOutLog} ne '' ? "STDOUT (last 10,000 characters):\n" . substr($self->{strOutLog},
                                 length($self->{strOutLog}) > 10000 ? length($self->{strOutLog}) - 10000 : 0) : '') .
                             ($self->{strErrorLog} ne '' ? "STDERR:\n$self->{strErrorLog}" : ''));
            }
        }
    }

    if ($self->{strErrorLog} ne '' && !$self->{bSuppressStdErr} && !$self->{bSuppressError})
    {
        confess &log(ERROR, "STDOUT:\n$self->{strOutLog}\n\noutput found on STDERR:\n$self->{strErrorLog}");
    }

    if ($self->{bShowOutput})
    {
        print "output:\n$self->{strOutLog}\n";
    }

    if (defined($strTest))
    {
        confess &log(ASSERT, "test point ${strTest} was not found");
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iExitStatus', value => $iExitStatus, trace => true}
    );
}

####################################################################################################################################
# end
####################################################################################################################################
sub end
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strTest,
        $bWait
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->end', \@_,
            {name => 'strTest', required => false, trace => true},
            {name => 'bWait', required => false, default => true, trace => true}
        );

    # If retry is not defined then run endRetry() one time
    my $iExitStatus;

    if (!defined($self->{iRetrySeconds}))
    {
        $iExitStatus = $self->endRetry($strTest, $bWait);
    }
    # Else loop until success or timeout
    else
    {
        my $oWait = waitInit($self->{iRetrySeconds});

        do
        {
            $self->{bRetry} = false;
            $self->begin();
            $iExitStatus = $self->endRetry($strTest, $bWait);

            if ($self->{bRetry})
            {
                &log(TRACE, 'error executing statement - retry');
            }
        }
        while ($self->{bRetry} && waitMore($oWait));

        if ($self->{bRetry})
        {
            $self->begin();
            $iExitStatus = $self->endRetry($strTest, $bWait);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iExitStatus', value => $iExitStatus, trace => true}
    );
}

####################################################################################################################################
# executeTest
####################################################################################################################################
sub executeTest
{
    my $strCommand = shift;
    my $oParam = shift;
    my $strTest = shift;

    my $oExec = new pgBackRestTest::Common::ExecuteTest($strCommand, $oParam);
    $oExec->begin();

    if (defined($strTest))
    {
        $oExec->end($strTest);
    }

    $oExec->end();

    return $oExec->{strOutLog};
}

push @EXPORT, qw(executeTest);

1;
