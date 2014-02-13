####################################################################################################################################
# DB MODULE
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

# Command strings
has strCommandPsql => (is => 'bare');   # PSQL command

# Module variables
has strDbUser => (is => 'ro');          # Database user
has strDbHost => (is => 'ro');          # Database host
has oDbSSH => (is => 'bare');           # Database SSH object
has fVersion => (is => 'ro');           # Database version

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
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

    # !!! Need to capture error output with open3 and log it

    # Run remotely
    if ($self->is_remote())
    {
        &log(TRACE, "psql execute: remote ${strScript}");

        $strResult = $self->{oDbSSH}->capture($strCommand)
            or confess &log(ERROR, "unable to execute remote psql command '${strCommand}'");
    }
    # Else run locally
    else
    {
        &log(TRACE, "psql execute: ${strScript}");
        $strResult = capture($strCommand) or confess &log(ERROR, "unable to execute local psql command '${strCommand}'");
    }

    return $strResult;
}

####################################################################################################################################
# TABLESPACE_MAP_GET - Get the mapping between oid and tablespace name
####################################################################################################################################
sub tablespace_map_get
{
    my $self = shift;

    return data_hash_build("oid\tname\n" . $self->psql_execute(
                           "copy (select oid, spcname from pg_tablespace) to stdout"), "\t");
}

####################################################################################################################################
# VERSION_GET
####################################################################################################################################
sub version_get
{
    my $self = shift;
    
    if (defined($self->{fVersion}))
    {
        return $self->{fVersion};
    }

    $self->{fVersion} = 
        trim($self->psql_execute("copy (select (regexp_matches(split_part(version(), ' ', 2), '^[0-9]+\.[0-9]+'))[1]) to stdout"));

    &log(DEBUG, "database version is $self->{fVersion}");

    return $self->{fVersion};
}

####################################################################################################################################
# BACKUP_START
####################################################################################################################################
sub backup_start
{
    my $self = shift;
    my $strLabel = shift;

    return trim($self->psql_execute("set client_min_messages = 'warning';" . 
                                    "copy (select pg_xlogfile_name(xlog) from pg_start_backup('${strLabel}') as xlog) to stdout"));
}

####################################################################################################################################
# BACKUP_STOP
####################################################################################################################################
sub backup_stop
{
    my $self = shift;

    return trim($self->psql_execute("set client_min_messages = 'warning';" .
                                    "copy (select pg_xlogfile_name(xlog) from pg_stop_backup() as xlog) to stdout"))
}

no Moose;
  __PACKAGE__->meta->make_immutable;