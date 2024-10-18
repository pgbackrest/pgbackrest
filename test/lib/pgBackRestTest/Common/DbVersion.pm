####################################################################################################################################
# DB VERSION MODULE
####################################################################################################################################
package pgBackRestTest::Common::DbVersion;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRestDoc::Common::Log;

####################################################################################################################################
# PostgreSQL version numbers
####################################################################################################################################
use constant PG_VERSION_95                                          => '9.5';
    push @EXPORT, qw(PG_VERSION_95);
use constant PG_VERSION_96                                          => '9.6';
    push @EXPORT, qw(PG_VERSION_96);
use constant PG_VERSION_10                                          => '10';
    push @EXPORT, qw(PG_VERSION_10);
use constant PG_VERSION_11                                          => '11';
    push @EXPORT, qw(PG_VERSION_11);
use constant PG_VERSION_12                                          => '12';
    push @EXPORT, qw(PG_VERSION_12);
use constant PG_VERSION_13                                          => '13';
    push @EXPORT, qw(PG_VERSION_13);
use constant PG_VERSION_14                                          => '14';
    push @EXPORT, qw(PG_VERSION_14);
use constant PG_VERSION_15                                          => '15';
    push @EXPORT, qw(PG_VERSION_15);
use constant PG_VERSION_16                                          => '16';
    push @EXPORT, qw(PG_VERSION_16);
use constant PG_VERSION_17                                          => '17';
    push @EXPORT, qw(PG_VERSION_17);

####################################################################################################################################
# versionSupport
#
# Returns an array of the supported Postgres versions.
####################################################################################################################################
sub versionSupport
{
    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->versionSupport');

    my @strySupportVersion = (PG_VERSION_95, PG_VERSION_96, PG_VERSION_10, PG_VERSION_11, PG_VERSION_12, PG_VERSION_13,
                              PG_VERSION_14, PG_VERSION_15, PG_VERSION_16, PG_VERSION_17);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strySupportVersion', value => \@strySupportVersion}
    );
}

push @EXPORT, qw(versionSupport);

1;
