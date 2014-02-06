####################################################################################################################################
# FILE MODULE
####################################################################################################################################
package pg_backrest_db;

use Moose;
use strict;
use warnings;
use Carp;
use Net::OpenSSH;
use File::Basename;
use IPC::System::Simple qw(capture);

use lib dirname($0);
use pg_backrest_utility;

#use Exporter qw(import);
#our @EXPORT = qw(PATH_DB PATH_DB_ABSOLUTE PATH_BACKUP PATH_BACKUP_ABSOLUTE PATH_BACKUP_CLUSTER PATH_BACKUP_TMP PATH_BACKUP_ARCHIVE);

# Command strings
has strCommandPsql => (is => 'bare');   # PSQL command

# Module variables
has strDbUser => (is => 'ro');          # Database user
has strDbHost => (is => 'ro');          # Database host
has oDbSSH => (is => 'bare');           # Database SSH object

sub BUILD
{
    my $self = shift;
    
    # Connect SSH object if db host is defined
    if (defined($self->{strDbHost}))
    {
        &log(INFO, "connecting to database ssh host $self->{strDbHost}");

        # !!! This could be improved by redirecting stderr to a file to get a better error message
        $self->{oDbSSH} = Net::OpenSSH->new($self->{strDbHost}, master_stderr_discard => true, user => $self->{strDbUser});
        $self->{oDbSSH}->error and confess &log(ERROR, "unable to connect to $self->{strDbHost}: " . $self->{oDbSSH}->error);
    }
}

####################################################################################################################################
# IS_REMOTE
#
# Determine whether database operations are remote.
####################################################################################################################################
sub is_remote
{
    my $self = shift;
    
    # If the SSH object is defined then db is remote
    return defined($self->{oDbSSH}) ? true : false;
}

####################################################################################################################################
# PSQL_EXECUTE
####################################################################################################################################
sub psql_execute
{
    my $self = shift;
    my $strScript = shift;  # psql script to execute
    
    # Get the user-defined command for psql
    my $strCommand = $self->{strCommandPsql} . " -c \"${strScript}\" postgres";
    my $strResult;

    # Run remotely
    if ($self->is_remote())
    {
        &log(DEBUG, "        psql execute: remote ${strScript}");

        $strResult = $self->{oDbSSH}->capture($strCommand)
            or confess &log(ERROR, "unable to execute remote psql command '${strCommand}'");
    }
    # Else run locally
    else
    {
        &log(DEBUG, "        psql execute: ${strScript}");
        $strResult = capture($strCommand) or confess &log(ERROR, "unable to execute local psql command '${strCommand}'");
    }
    
    return $strResult;
}

no Moose;
  __PACKAGE__->meta->make_immutable;