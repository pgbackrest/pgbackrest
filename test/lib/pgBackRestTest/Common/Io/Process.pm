####################################################################################################################################
# Process Execution, Management, and IO
####################################################################################################################################
package pgBackRestTest::Common::Io::Process;
use parent 'pgBackRestTest::Common::Io::Filter';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use IPC::Open3 qw(open3);
use POSIX qw(:sys_wait_h);
use Symbol 'gensym';

use pgBackRestDoc::Common::Exception;
use pgBackRestDoc::Common::Log;

use pgBackRestTest::Common::Io::Buffered;
use pgBackRestTest::Common::Wait;

####################################################################################################################################
# Amount of time to attempt to retrieve errors when a process terminates unexpectedly
####################################################################################################################################
use constant IO_ERROR_TIMEOUT                                                => 5;

####################################################################################################################################
# new - use open3 to run the command and get the io handles
####################################################################################################################################
sub new
{
    my $class = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oParent,
        $strCommand,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'oParent', trace => true},
            {name => 'strCommand', trace => true},
        );

    # Bless with new class
    my $self = $class->SUPER::new($oParent);
    bless $self, $class;

    # Use open3 to run the command
    my ($iProcessId, $fhRead, $fhWrite, $fhReadError);
    $fhReadError = gensym;

    $iProcessId = IPC::Open3::open3($fhWrite, $fhRead, $fhReadError, $strCommand);

    # Set handles
    $self->handleReadSet($fhRead);
    $self->handleWriteSet($fhWrite);

    # Set variables
    $self->{iProcessId} = $iProcessId;
    $self->{fhReadError} = $fhReadError;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# error - handle errors
####################################################################################################################################
sub error
{
    my $self = shift;
    my $iCode = shift;
    my $strMessage = shift;
    my $strDetail = shift;
    my $bClose = shift;

    if (defined($self->{iProcessId}))
    {
        my $oWait = waitInit(defined($iCode) ? IO_ERROR_TIMEOUT : 0);

        do
        {
            # Check the result
            my $iResult = waitpid($self->{iProcessId}, $bClose ? 0 : WNOHANG);

            # Error if the process exited unexpectedly
            if ($iResult != 0)
            {
                # Get the exit status
                $self->{iExitStatus} = $iResult == -1 ? 255 : ${^CHILD_ERROR_NATIVE} >> 8;

                # Drain the stderr stream
                my $strError;
                my $oIoError = new pgBackRestTest::Common::Io::Buffered(
                    new pgBackRestTest::Common::Io::Handle($self->id(), $self->{fhReadError}), 5, $self->bufferMax());

                while (defined(my $strLine = $oIoError->readLine(true, false)))
                {
                    $strError .= (defined($strError) ? "\n" : '') . $strLine;
                }

                delete($self->{iProcessId});

                if ((!$bClose && $self->{iExitStatus} != 0) || defined($strError))
                {
                    my $iErrorCode =
                        $self->{iExitStatus} >= ERROR_MINIMUM && $self->{iExitStatus} <= ERROR_MAXIMUM ?
                            $self->{iExitStatus} : ERROR_FILE_READ;

                    logErrorResult(
                        $iErrorCode, $self->id() . ' terminated unexpectedly' .
                            ($self->{iExitStatus} != 255 ? sprintf(' [%03d]', $self->{iExitStatus}) : ''),
                        $strError);
                }
            }
        }
        while (waitMore($oWait));

        if (defined($iCode))
        {
            $self->parent()->error($iCode, $strMessage, $strDetail);
        }
    }
    else
    {
        confess &log(ASSERT, 'cannot call error() after process has been closed');
    }
}

####################################################################################################################################
# Get process id
####################################################################################################################################
sub processId
{
    my $self = shift;

    return $self->{iProcessId};
}

####################################################################################################################################
# Get exit status (after close() is called)
####################################################################################################################################
sub exitStatus
{
    my $self = shift;

    return $self->{iExitStatus};
}

####################################################################################################################################
# writeLine - check for error before writing line
####################################################################################################################################
sub writeLine
{
    my $self = shift;
    my $strBuffer = shift;

    # Check if the process has exited abnormally (doesn't seem like we should need this, but the next syswrite does a hard
    # abort if the remote process has already closed)
    $self->error();

    return $self->parent()->writeLine($strBuffer);
}

####################################################################################################################################
# close - check if the process terminated on error
####################################################################################################################################
sub close
{
    my $self = shift;

    if (defined($self->{iProcessId}))
    {
        $self->error(undef, undef, undef, true);

        # Class parent close
        $self->parent()->close();
    }

    return true;
}

1;
