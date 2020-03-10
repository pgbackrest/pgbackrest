####################################################################################################################################
# HostGroupTest.pm - Encapsulate a group of docker containers for testing
####################################################################################################################################
package pgBackRestTest::Common::HostGroupTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Cwd qw(abs_path);
use Exporter qw(import);
    our @EXPORT = qw();

use BackRestDoc::Common::Log;
use BackRestDoc::Common::String;

use pgBackRestTest::Common::ExecuteTest;

####################################################################################################################################
# Global host group variable
####################################################################################################################################
my $oHostGroup;

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
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->new');

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self, trace => true}
    );
}

####################################################################################################################################
# hostAdd
####################################################################################################################################
sub hostAdd
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oHost,
        $rstryHostName,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->hostAdd', \@_,
            {name => 'oHost'},
            {name => 'rstryHostName', optional => true},
        );

    $self->{host}{$oHost->{strName}} = $oHost;

    if ($oHost->hostUpdateGet())
    {
        $oHost->executeSimple("echo \"\" >> /etc/hosts", undef, 'root', {bLoadEnv => false});
        $oHost->executeSimple("echo \"# Test Hosts\" >> /etc/hosts", undef, 'root', {bLoadEnv => false});
    }

    my $strHostList = $oHost->{strName} . (defined($rstryHostName) ? ' ' . join(' ', @{$rstryHostName}) : '');

    # Iterate hosts to add IP mappings
    foreach my $strOtherHostName (sort(keys(%{$self->{host}})))
    {
        my $oOtherHost = $self->{host}{$strOtherHostName};

        if ($strOtherHostName ne $oHost->{strName})
        {
            # Add this host IP to all hosts
            if ($oOtherHost->hostUpdateGet())
            {
                $oOtherHost->executeSimple(
                    "echo \"$oHost->{strIP} ${strHostList}\" >> /etc/hosts", undef, 'root', {bLoadEnv => false});
            }

            # Add all other host IPs to this host
            if ($oHost->hostUpdateGet())
            {
                $oHost->executeSimple(
                    "echo \"$oOtherHost->{strIP} ${strOtherHostName}\" >> /etc/hosts", undef, 'root', {bLoadEnv => false});
            }
        }
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# hostGet
####################################################################################################################################
sub hostGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strName,
        $bIgnoreMissing,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->hostGet', \@_,
            {name => 'strName', trace => true},
            {name => 'bIgnoreMissing', default => false, trace => true},
        );

    my $oHost = $self->{host}{$strName};

    if (!defined($oHost) && !$bIgnoreMissing)
    {
        confess &log(ERROR, "host ${strName} does not exist");
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oHost', value => $oHost}
    );
}

####################################################################################################################################
# removeAll
####################################################################################################################################
sub removeAll
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->removeAll');

    my $iTotal = 0;

    foreach my $strHostName (sort(keys(%{$self->{host}})))
    {
        ${$self->{host}}{$strHostName}->remove();
        delete($self->{host}{$strHostName});

        $iTotal++;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iTotal', value => $iTotal}
    );
}

####################################################################################################################################
# hostGroupGet
#
# Get the global host group object.
####################################################################################################################################
sub hostGroupGet
{
    if (!defined($oHostGroup))
    {
        $oHostGroup = new pgBackRestTest::Common::HostGroupTest();
    }

    return $oHostGroup;
}

push @EXPORT, qw(hostGroupGet);

1;
