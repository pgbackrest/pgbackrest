####################################################################################################################################
# DB MODULE
####################################################################################################################################
package BackRest::Db;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
use Fcntl qw(O_RDONLY);
use File::Basename;
use IPC::System::Simple qw(capture);
use Net::OpenSSH;

use lib dirname($0);
use BackRest::Config;
use BackRest::Exception;
use BackRest::File;
use BackRest::Utility;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_DB                 => 'Db';

use constant OP_DB_INFO             => OP_DB . "->info";            our @EXPORT = qw(OP_DB_INFO);

####################################################################################################################################
# Postmaster process Id file
####################################################################################################################################
use constant FILE_POSTMASTER_PID => 'postmaster.pid';               push @EXPORT, qw(FILE_POSTMASTER_PID);

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;          # Class name
    my $bStartStop = shift;     # Will start/stop be called?
    my $strDbPath = shift;      # Database path
    my $strCommandPsql = shift; # PSQL command
    my $strDbHost = shift;      # Database host name
    my $strDbUser = shift;      # Database user name (generally postgres)

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Initialize variables
    $self->{bStartStop} = $bStartStop;
    $self->{strDbPath} = $strDbPath;
    $self->{strCommandPsql} = $strCommandPsql;
    $self->{strDbHost} = $strDbHost;
    $self->{strDbUser} = $strDbUser;

    # Connect SSH object if db host is defined
    if ($self->{bStartStop} && defined($self->{strDbHost}) && !defined($self->{oDbSSH}))
    {
        my $strOptionSSHRequestTTY = 'RequestTTY=yes';

        &log(TRACE, "connecting to database ssh host $self->{strDbHost}");

        # !!! This could be improved by redirecting stderr to a file to get a better error message
        $self->{oDbSSH} = Net::OpenSSH->new($self->{strDbHost}, user => $self->{strDbUser},
                              master_opts => [-o => $strOptionSSHRequestTTY]);
        $self->{oDbSSH}->error and confess &log(ERROR, "unable to connect to $self->{strDbHost}: " . $self->{oDbSSH}->error);
    }

    return $self;
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
# versionSupport
#
# Returns an array of the supported Postgres versions.
####################################################################################################################################
sub versionSupport
{
    my @strySupportVersion = ('8.3', '8.4', '9.0', '9.1', '9.2', '9.3', '9.4', '9.5');

    return \@strySupportVersion;
}

push @EXPORT, qw(versionSupport);

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

    my $oHashRef = {};

    data_hash_build($oHashRef, "oid\tname\n" . $self->psql_execute(
                    'copy (select oid, spcname from pg_tablespace) to stdout'), "\t");

    return $oHashRef;
}

####################################################################################################################################
# info
####################################################################################################################################
sub info
{
    my $self = shift;
    my $oFile = shift;
    my $strDbPath = shift;

    # Set operation and debug strings
    &log(DEBUG, OP_DB_INFO . "(): isRemote = " . ($oFile->is_remote(PATH_DB_ABSOLUTE) ? 'true' : 'false') .
                ", dbPath = ${strDbPath}");

    # Database info
    my $iCatalogVersion;
    my $iControlVersion;
    my $ullDbSysId;
    my $strDbVersion;

    if ($oFile->is_remote(PATH_DB_ABSOLUTE))
    {
        # Build param hash
        my %oParamHash;

        $oParamHash{'db-path'} = ${strDbPath};

        # Output remote trace info
        &log(TRACE, OP_DB_INFO . ": remote (" . $oFile->{oRemote}->command_param_string(\%oParamHash) . ')');

        # Execute the command
        my $strResult = $oFile->{oRemote}->command_execute(OP_DB_INFO, \%oParamHash, true);

        # Split the result into return values
        my @stryToken = split(/\t/, $strResult);

        $strDbVersion = $stryToken[0];
        $iControlVersion = $stryToken[1];
        $iCatalogVersion = $stryToken[2];
        $ullDbSysId = $stryToken[3];
    }
    else
    {
        # Open the control file
        my $strControlFile = "${strDbPath}/global/pg_control";
        my $hFile;
        my $tBlock;

        sysopen($hFile, $strControlFile, O_RDONLY)
            or confess &log(ERROR, "unable to open ${strControlFile}", ERROR_FILE_OPEN);

        # Read system identifier
        sysread($hFile, $tBlock, 8) == 8
            or confess &log(ERROR, "unable to read database system identifier");

        $ullDbSysId = unpack('Q', $tBlock);

        # Read control version
        sysread($hFile, $tBlock, 4) == 4
            or confess &log(ERROR, "unable to read control version");

        $iControlVersion = unpack('L', $tBlock);

        # Read catalog version
        sysread($hFile, $tBlock, 4) == 4
            or confess &log(ERROR, "unable to read catalog version");

        $iCatalogVersion = unpack('L', $tBlock);

        # Close the control file
        close($hFile);

        # Make sure the control version is supported
        if ($iControlVersion == 942 && $iCatalogVersion == 201409291)
        {
            $strDbVersion = '9.4';
        }
        # Leave 9.5 catalog version out until it stabilizes (then move 9.5 to the top of if list)
        elsif ($iControlVersion == 942) # && $iCatalogVersion == 201505311)
        {
            $strDbVersion = '9.5';
        }
        elsif ($iControlVersion == 937 && $iCatalogVersion == 201306121)
        {
            $strDbVersion = '9.3';
        }
        elsif ($iControlVersion == 922 && $iCatalogVersion == 201204301)
        {
            $strDbVersion = '9.2';
        }
        elsif ($iControlVersion == 903 && $iCatalogVersion == 201105231)
        {
            $strDbVersion = '9.1';
        }
        elsif ($iControlVersion == 903 && $iCatalogVersion == 201008051)
        {
            $strDbVersion = '9.0';
        }
        elsif ($iControlVersion == 843 && $iCatalogVersion == 200904091)
        {
            $strDbVersion = '8.4';
        }
        elsif ($iControlVersion == 833 && $iCatalogVersion == 200711281)
        {
            $strDbVersion = '8.3';
        }
        elsif ($iControlVersion == 822 && $iCatalogVersion == 200611241)
        {
            $strDbVersion = '8.2';
        }
        elsif ($iControlVersion == 812 && $iCatalogVersion == 200510211)
        {
            $strDbVersion = '8.1';
        }
        elsif ($iControlVersion == 74 && $iCatalogVersion == 200411041)
        {
            $strDbVersion = '8.0';
        }
        else
        {
            confess &log(ERROR, "unexpected control version = ${iControlVersion} and catalog version = ${iCatalogVersion}" .
                                ' (unsupported PostgreSQL version?)',
                         ERROR_VERSION_NOT_SUPPORTED);
        }
    }

    &log(DEBUG, OP_DB_INFO . "=>: dbVersion = ${strDbVersion}, controlVersion = ${iControlVersion}" .
                ", catalogVersion = ${iCatalogVersion}, dbSysId = ${ullDbSysId}");

    return $strDbVersion, $iControlVersion, $iCatalogVersion, $ullDbSysId;
}

####################################################################################################################################
# DB_VERSION_GET
####################################################################################################################################
sub db_version_get
{
    my $self = shift;

    if (defined($self->{fVersion}))
    {
        return $self->{fVersion};
    }

    $self->{fVersion} =
        trim($self->psql_execute("copy (select (regexp_matches(split_part(version(), ' ', 2), '^[0-9]+\.[0-9]+'))[1]) to stdout"));

    &log(DEBUG, "database version is $self->{fVersion}");

    my $strVersionSupport = versionSupport();

    if ($self->{fVersion} < ${$strVersionSupport}[0])
    {
        confess &log(ERROR, "unsupported Postgres version ${$strVersionSupport}[0]", ERROR_VERSION_NOT_SUPPORTED);
    }

    return $self->{fVersion};
}

####################################################################################################################################
# BACKUP_START
####################################################################################################################################
sub backup_start
{
    my $self = shift;
    my $strLabel = shift;
    my $bStartFast = shift;

    $self->db_version_get();

    if ($self->{fVersion} < 8.4 && $bStartFast)
    {
        &log(WARN, 'start-fast option is only available in PostgreSQL >= 8.4');
        $bStartFast = false;
    }

    &log(INFO, "executing pg_start_backup() with label \"${strLabel}\": backup will begin after " .
               ($bStartFast ? "the requested immediate checkpoint" : "the next regular checkpoint") . " completes");

    my @stryField = split("\t", trim($self->psql_execute("set client_min_messages = 'warning';" .
                                    "copy (select to_char(current_timestamp, 'YYYY-MM-DD HH24:MI:SS.US TZ'), " .
                                    "pg_xlogfile_name(xlog) from pg_start_backup('${strLabel}'" .
                                    ($bStartFast ? ', true' : '') . ') as xlog) to stdout')));

    return $stryField[1], $stryField[0];
}

####################################################################################################################################
# BACKUP_STOP
####################################################################################################################################
sub backup_stop
{
    my $self = shift;

    &log(INFO, 'executing pg_stop_backup() and waiting for all WAL segments to be archived');

    my @stryField = split("\t", trim($self->psql_execute("set client_min_messages = 'warning';" .
                                    "copy (select to_char(clock_timestamp(), 'YYYY-MM-DD HH24:MI:SS.US TZ'), pg_xlogfile_name(xlog) from pg_stop_backup() as xlog) to stdout")));

    return $stryField[1], $stryField[0];
}

1;
