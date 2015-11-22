####################################################################################################################################
# HostTest.pm - Encapsulate a docker host for testing
####################################################################################################################################
package BackRestTest::Common::HostTest;

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

use lib dirname($0) . '/../lib';
use BackRest::Common::Log;
use BackRest::Common::String;

use BackRestTest::Common::ExecuteTest;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_HOST_TEST                                           => 'LogTest';

use constant OP_HOST_TEST_COPY_FROM                                 => OP_HOST_TEST . "->copyFrom";
use constant OP_HOST_TEST_COPY_TO                                   => OP_HOST_TEST . "->copyTo";
use constant OP_HOST_TEST_EXECUTE                                   => OP_HOST_TEST . "->execute";
use constant OP_HOST_TEST_EXECUTE_SIMPLE                            => OP_HOST_TEST . "->executeSimple";
use constant OP_HOST_TEST_NEW                                       => OP_HOST_TEST . "->new";

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
        $self->{strName},
        $self->{strImage},
        $self->{strUser},
        $self->{strOS},
        $self->{strMount}
    ) =
        logDebugParam
        (
            OP_HOST_TEST_NEW, \@_,
            {name => 'strName', trace => true},
            {name => 'strImage', trace => true},
            {name => 'strUser', trace => true},
            {name => 'strOS', trace => true},
            {name => 'strMount', trace => true}
        );

    executeTest("docker kill $self->{strName}", {bSuppressError => true});
    executeTest("docker rm $self->{strName}", {bSuppressError => true});

    executeTest("rm -rf ~/data/$self->{strName}");
    executeTest("mkdir -p ~/data/$self->{strName}/etc");

    executeTest("docker run -itd -h $self->{strName} --name=$self->{strName} " .
                (defined($self->{strMount}) ? "-v $self->{strMount} " : '') .
                "$self->{strImage}");

    $self->{strIP} = trim(executeTest("docker inspect --format '\{\{ .NetworkSettings.IPAddress \}\}' $self->{strName}"));
    $self->{bActive} = true;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self, trace => true}
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
        $strCommand,
        $oParam,
        $strUser
    ) =
        logDebugParam
        (
            OP_HOST_TEST_EXECUTE, \@_,
            {name => 'strCommand'},
            {name => 'oParam', required=> false},
            {name => 'strUser', required => false}
        );

    # Set the user
    if (!defined($strUser))
    {
        $strUser = $self->{strUser};
    }

    my $oExec = new BackRestTest::Common::ExecuteTest(
        'docker exec ' . ($strUser eq 'root' ? "-u ${strUser} " : '') . "$self->{strName} ${strCommand}" , $oParam);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oExec', value => $oExec, trace => true}
    );
}

####################################################################################################################################
# executeSimple
####################################################################################################################################
sub executeSimple
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strCommand,
        $oParam,
        $strUser
    ) =
        logDebugParam
        (
            OP_HOST_TEST_EXECUTE_SIMPLE, \@_,
            {name => 'strCommand', trace => true},
            {name => 'oParam', required=> false, trace => true},
            {name => 'strUser', required => false, trace => true}
        );

    my $oExec = $self->execute($strCommand, $oParam, $strUser);
    $oExec->begin();
    $oExec->end();

    return $oExec->{strOutLog};
}

####################################################################################################################################
# copyTo
####################################################################################################################################
sub copyTo
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strSource,
        $strDestination,
        $strOwner,
        $strMode
    ) =
        logDebugParam
        (
            OP_HOST_TEST_COPY_TO, \@_,
            {name => 'strSource'},
            {name => 'strDestination'},
            {name => 'strOwner', required => false},
            {name => 'strMode', required => false}
        );

    executeTest("docker cp ${strSource} $self->{strName}:${strDestination}");

    if (defined($strOwner))
    {
        $self->executeSimple("chown ${strOwner} ${strDestination}", undef, 'root');
    }

    if (defined($strMode))
    {
        $self->executeSimple("chmod ${strMode} ${strDestination}", undef, 'root');
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# copyFrom
####################################################################################################################################
sub copyFrom
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strSource,
        $strDestination
    ) =
        logDebugParam
        (
            OP_HOST_TEST_COPY_FROM, \@_,
            {name => 'strSource'},
            {name => 'strDestination'}
        );

    executeTest("docker cp $self->{strName}:${strSource} ${strDestination}");

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

1;
