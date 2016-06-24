####################################################################################################################################
# ParamTest.pm - Allows parameters to be added to any object
####################################################################################################################################
package pgBackRestTest::Common::ParamTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use pgBackRest::Common::Log;

####################################################################################################################################
# paramSet
####################################################################################################################################
sub paramSet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strKey,
        $strValue,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->paramSet', \@_,
            {name => 'strKey', trace => true},
            {name => 'strValue', trace => true},
        );

    $self->{param}{$strKey} = $strValue;

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# paramGet
####################################################################################################################################
sub paramGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strKey,
        $bRequired,
        $strDefault,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->paramGet', \@_,
            {name => 'strKey', trace => true},
            {name => 'bRequired', default => true, trace => true},
            {name => 'strDefault', required => false, trace => true},
        );

    my $strValue = $self->{param}{$strKey};

    if (!defined($strValue))
    {
        if ($bRequired)
        {
            confess &log(ERROR, "param '${strKey}' is required");
        }

        $strValue = $strDefault;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strValue', value => $strValue, trace => true}
    );
}

####################################################################################################################################
# paramTest
####################################################################################################################################
sub paramTest
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strKey,
        $strTestValue,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->paramTest', \@_,
            {name => 'strKey', trace => true},
            {name => 'strTestValue', required => false, trace => true},
        );

    my $strValue = $self->paramGet($strKey, false);

    my $bResult =
        !defined($strTestValue) && defined($strValue) || defined($strTestValue) && $strTestValue eq $strValue ? true : false;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bResult', value => $bResult, trace => true}
    );
}

1;
