####################################################################################################################################
# DB MODULE
####################################################################################################################################
package BackRest::Db;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use DBI;
use Exporter qw(import);
use Fcntl qw(O_RDONLY);
use File::Basename qw(dirname);

use lib dirname($0);
use BackRest::Config;
use BackRest::Exception;
use BackRest::File;
use BackRest::Utility;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_DB                                                  => 'Db';

use constant OP_DB_NEW                                              => OP_DB . "->new";
    our @EXPORT = qw(OP_DB_NEW);
use constant OP_DB_INFO                                             => OP_DB . "->info";
    push @EXPORT, qw(OP_DB_INFO);
use constant OP_DB_EXECUTE_SQL                                      => OP_DB . "->executeSql";
    push @EXPORT, qw(OP_DB_EXECUTE_SQL);
use constant OP_DB_VERSION_GET                                      => OP_DB . "->versionGet";

####################################################################################################################################
# Postmaster process Id file
####################################################################################################################################
use constant FILE_POSTMASTER_PID                                    => 'postmaster.pid';
    push @EXPORT, qw(FILE_POSTMASTER_PID);

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;          # Class name

    # Create the class hash
    my $self = {};
    bless $self, $class;

    return $self;
}

####################################################################################################################################
# DESTRUCTOR
####################################################################################################################################
sub DESTROY
{
    my $self = shift;

    if (defined($self->{hDb}))
    {
        $self->{hDb}->disconnect();
        undef($self->{hDb});
    }
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
# executeSql
####################################################################################################################################
sub executeSql
{
    my $self = shift;
    my $strScript = shift;  # psql script to execute (must be on a single line)

    logDebug(OP_DB_EXECUTE_SQL, DEBUG_CALL, undef, {isRemote => optionRemoteTypeTest(DB), script => \$strScript});

    # Get the user-defined command for psql
    my $strResult;

    # Run remotely
    if (optionRemoteTypeTest(DB))
    {
        # Build param hash
        my %oParamHash;

        $oParamHash{'script'} = $strScript;

        # Execute the command
        $strResult = protocolGet()->cmdExecute(OP_DB_EXECUTE_SQL, \%oParamHash, true);
    }
    # Else run locally
    else
    {
        if (!defined($self->{hDb}))
        {
            # Connect to the db
            my $strDbName = 'postgres';
            my $strDbUser = getpwuid($<);
            my $strDbUri = "dbi:Pg:dbname=${strDbName};port=" . optionGet(OPTION_DB_PORT) .
                           (optionTest(OPTION_DB_SOCKET_PATH) ? ';host=' . optionGet(OPTION_DB_SOCKET_PATH) : '');

            logDebug(OP_DB_EXECUTE_SQL, DEBUG_MISC, 'db connect', {strDbUri => $strDbUri, strDbUser => $strDbUser});

            $self->{hDb} = DBI->connect($strDbUri, $strDbUser, undef, {AutoCommit => 1, RaiseError => 0, PrintError => 0});

            if (!$self->{hDb})
            {
                confess &log(ERROR, $DBI::errstr, ERROR_DB_CONNECT);
            }
        }

        # Execute the query
        my $hStatement = $self->{hDb}->prepare($strScript)
            or confess &log(ERROR, $DBI::errstr, ERROR_DB_QUERY);

        $hStatement->execute() or
            confess &log(ERROR, $DBI::errstr . ":\n${strScript}", ERROR_DB_QUERY);

        # Get rows and return them
        my @stryArray;

        do
        {
            @stryArray = $hStatement->fetchrow_array;

            if (!@stryArray && $hStatement->err)
            {
                confess &log(ERROR, $DBI::errstr . ":\n${strScript}", ERROR_DB_QUERY);
            }

            $strResult = (defined($strResult) ? "${strResult}\n" : '') . join("\t", @stryArray);
        }
        while (@stryArray);
    }

    logDebug(OP_DB_EXECUTE_SQL, DEBUG_RESULT, undef, {strResult => $strResult});

    return $strResult;
}

####################################################################################################################################
# TABLESPACE_MAP_GET - Get the mapping between oid and tablespace name
####################################################################################################################################
sub tablespace_map_get
{
    my $self = shift;

    my $oHashRef = {};

    data_hash_build($oHashRef, "oid\tname\n" . $self->executeSql(
                    'select oid, spcname from pg_tablespace'), "\t");

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

    logDebug(OP_DB_INFO, DEBUG_CALL, undef, {isRemote => $oFile->is_remote(PATH_DB_ABSOLUTE), dbPath => $strDbPath});

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
        &log(TRACE, OP_DB_INFO . ": remote (" . $oFile->{oProtocol}->commandParamString(\%oParamHash) . ')');

        # Execute the command
        my $strResult = $oFile->{oProtocol}->cmdExecute(OP_DB_INFO, \%oParamHash, true);

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
        # Leave 9.5 catalog version out until it stabilizes (then move 9.5 to the top of the list)
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
# versionGet
####################################################################################################################################
sub versionGet
{
    my $self = shift;

    if (defined($self->{fVersion}))
    {
        return $self->{fVersion};
    }

    $self->{fVersion} =
        trim($self->executeSql("select (regexp_matches(split_part(version(), ' ', 2), '^[0-9]+\.[0-9]+'))[1]"));

    my $strVersionSupport = versionSupport();

    if ($self->{fVersion} < ${$strVersionSupport}[0])
    {
        confess &log(ERROR, "unsupported Postgres version ${$strVersionSupport}[0]", ERROR_VERSION_NOT_SUPPORTED);
    }

    logDebug(OP_DB_VERSION_GET, DEBUG_RESULT, {dbVersion => $self->{fVersion}});

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

    $self->versionGet();

    # Only allow start-fast option for version >= 8.4
    if ($self->{fVersion} < 8.4 && $bStartFast)
    {
        &log(WARN, 'start-fast option is only available in PostgreSQL >= 8.4');
        $bStartFast = false;
    }

    &log(INFO, "executing pg_start_backup() with label \"${strLabel}\": backup will begin after " .
               ($bStartFast ? "the requested immediate checkpoint" : "the next regular checkpoint") . " completes");

    my @stryField = split("\t", trim(
        $self->executeSql("select to_char(current_timestamp, 'YYYY-MM-DD HH24:MI:SS.US TZ'), " .
                          "pg_xlogfile_name(xlog) from pg_start_backup('${strLabel}'" .
                          ($bStartFast ? ', true' : '') . ') as xlog')));

    return $stryField[1], $stryField[0];
}

####################################################################################################################################
# BACKUP_STOP
####################################################################################################################################
sub backup_stop
{
    my $self = shift;

    &log(INFO, 'executing pg_stop_backup() and waiting for all WAL segments to be archived');

    my @stryField = split("\t",
        trim($self->executeSql("select to_char(clock_timestamp(), 'YYYY-MM-DD HH24:MI:SS.US TZ')," .
                               " pg_xlogfile_name(xlog) from pg_stop_backup() as xlog")));

    return $stryField[1], $stryField[0];
}

1;
