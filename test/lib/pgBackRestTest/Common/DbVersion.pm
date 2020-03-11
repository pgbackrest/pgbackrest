####################################################################################################################################
# DB VERSION MODULE
####################################################################################################################################
package pgBackRestTest::Common::DbVersion;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT =  qw();

use pgBackRestDoc::Common::Log;

####################################################################################################################################
# PostgreSQL version numbers
####################################################################################################################################
use constant PG_VERSION_83                                          => '8.3';
    push @EXPORT, qw(PG_VERSION_83);
use constant PG_VERSION_84                                          => '8.4';
    push @EXPORT, qw(PG_VERSION_84);
use constant PG_VERSION_90                                          => '9.0';
    push @EXPORT, qw(PG_VERSION_90);
use constant PG_VERSION_91                                          => '9.1';
    push @EXPORT, qw(PG_VERSION_91);
use constant PG_VERSION_92                                          => '9.2';
    push @EXPORT, qw(PG_VERSION_92);
use constant PG_VERSION_93                                          => '9.3';
    push @EXPORT, qw(PG_VERSION_93);
use constant PG_VERSION_94                                          => '9.4';
    push @EXPORT, qw(PG_VERSION_94);
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

use constant PG_VERSION_APPLICATION_NAME                            => PG_VERSION_90;
    push @EXPORT, qw(PG_VERSION_APPLICATION_NAME);
use constant PG_VERSION_HOT_STANDBY                                 => PG_VERSION_91;
    push @EXPORT, qw(PG_VERSION_HOT_STANDBY);
use constant PG_VERSION_BACKUP_STANDBY                              => PG_VERSION_92;
    push @EXPORT, qw(PG_VERSION_BACKUP_STANDBY);

####################################################################################################################################
# versionSupport
#
# Returns an array of the supported Postgres versions.
####################################################################################################################################
sub versionSupport
{
    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->versionSupport');

    my @strySupportVersion = (PG_VERSION_83, PG_VERSION_84, PG_VERSION_90, PG_VERSION_91, PG_VERSION_92, PG_VERSION_93,
                              PG_VERSION_94, PG_VERSION_95, PG_VERSION_96, PG_VERSION_10, PG_VERSION_11, PG_VERSION_12);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strySupportVersion', value => \@strySupportVersion}
    );
}

push @EXPORT, qw(versionSupport);

1;
