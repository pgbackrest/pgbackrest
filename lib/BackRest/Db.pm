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
            my $strDbSocketPath = optionGet(OPTION_DB_SOCKET_PATH, false);

            if (defined($strDbSocketPath) && $strDbSocketPath !~ /^\//)
            {
                confess &log(ERROR, "'${strDbSocketPath}' is not valid for '" . OPTION_DB_SOCKET_PATH . "' option:" .
                                    " path must be absolute", ERROR_OPTION_INVALID_VALUE);
            }

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

    # Return data from the cache if it exists
    if (defined($self->{info}{$strDbPath}))
    {
        return $self->{info}{$strDbPath}{fDbVersion},
               $self->{info}{$strDbPath}{iControlVersion},
               $self->{info}{$strDbPath}{iCatalogVersion},
               $self->{info}{$strDbPath}{ullDbSysId};
    }

    # Database info
    my $iCatalogVersion;
    my $iControlVersion;
    my $ullDbSysId;
    my $fDbVersion;

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

        $fDbVersion = $stryToken[0];
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
            $fDbVersion = '9.4';
        }
        # Leave 9.5 catalog version out until it stabilizes (then move 9.5 to the top of the list)
        elsif ($iControlVersion == 942) # && $iCatalogVersion == 201505311)
        {
            $fDbVersion = '9.5';
        }
        elsif ($iControlVersion == 937 && $iCatalogVersion == 201306121)
        {
            $fDbVersion = '9.3';
        }
        elsif ($iControlVersion == 922 && $iCatalogVersion == 201204301)
        {
            $fDbVersion = '9.2';
        }
        elsif ($iControlVersion == 903 && $iCatalogVersion == 201105231)
        {
            $fDbVersion = '9.1';
        }
        elsif ($iControlVersion == 903 && $iCatalogVersion == 201008051)
        {
            $fDbVersion = '9.0';
        }
        elsif ($iControlVersion == 843 && $iCatalogVersion == 200904091)
        {
            $fDbVersion = '8.4';
        }
        elsif ($iControlVersion == 833 && $iCatalogVersion == 200711281)
        {
            $fDbVersion = '8.3';
        }
        elsif ($iControlVersion == 822 && $iCatalogVersion == 200611241)
        {
            $fDbVersion = '8.2';
        }
        elsif ($iControlVersion == 812 && $iCatalogVersion == 200510211)
        {
            $fDbVersion = '8.1';
        }
        elsif ($iControlVersion == 74 && $iCatalogVersion == 200411041)
        {
            $fDbVersion = '8.0';
        }
        else
        {
            confess &log(ERROR, "unexpected control version = ${iControlVersion} and catalog version = ${iCatalogVersion}" .
                                ' (unsupported PostgreSQL version?)',
                         ERROR_VERSION_NOT_SUPPORTED);
        }
    }

    # Store data in the cache
    $self->{info}{$strDbPath}{fDbVersion} = $fDbVersion;
    $self->{info}{$strDbPath}{iControlVersion} = $iControlVersion;
    $self->{info}{$strDbPath}{iCatalogVersion} = $iCatalogVersion;
    $self->{info}{$strDbPath}{ullDbSysId} = $ullDbSysId;

    &log(DEBUG, OP_DB_INFO . "=>: dbVersion = ${fDbVersion}, controlVersion = ${iControlVersion}" .
                ", catalogVersion = ${iCatalogVersion}, dbSysId = ${ullDbSysId}");

    return $fDbVersion, $iControlVersion, $iCatalogVersion, $ullDbSysId;
}

####################################################################################################################################
# versionGet
####################################################################################################################################
sub versionGet
{
    my $self = shift;

    # Get data from the cache if possible
    if (defined($self->{fVersion}) && defined($self->{strDbPath}))
    {
        return $self->{fVersion}, $self->{strDbPath};
    }

    # Get version and db-path from
    ($self->{fVersion}, $self->{strDbPath}) = split("\t", trim(
        $self->executeSql("select (regexp_matches(split_part(version(), ' ', 2), '^[0-9]+\.[0-9]+'))[1], setting" .
                          " from pg_settings where name = 'data_directory'")));

    my $strVersionSupport = versionSupport();

    if ($self->{fVersion} < ${$strVersionSupport}[0])
    {
        confess &log(ERROR, "unsupported Postgres version ${$strVersionSupport}[0]", ERROR_VERSION_NOT_SUPPORTED);
    }

    logDebug(OP_DB_VERSION_GET, DEBUG_RESULT, {dbVersion => $self->{fVersion}});

    return $self->{fVersion}, $self->{strDbPath};
}

####################################################################################################################################
# BACKUP_START
####################################################################################################################################
sub backup_start
{
    my $self = shift;
    my $oFile = shift;
    my $strDbPath = shift;
    my $strLabel = shift;
    my $bStartFast = shift;

    # Get the version from the control file
    my ($fDbVersion) = $self->info($oFile, $strDbPath);

    # Get version and db path from the database
    my ($fCompareDbVersion, $strCompareDbPath) = $self->versionGet();

    # Error if they are not identical
    if (!($fDbVersion == $fCompareDbVersion && $strDbPath eq $strCompareDbPath))
    {
        confess &log(ERROR, "version '${fCompareDbVersion}' and db-path '${strCompareDbPath}' queried from cluster does not match" .
                            " version '${fDbVersion}' and db-path '${strDbPath}' read from '${strDbPath}/global/pg_control'\n" .
                            "HINT: the db-path and db-port settings likely reference different clusters", ERROR_DB_MISMATCH);
    }

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
