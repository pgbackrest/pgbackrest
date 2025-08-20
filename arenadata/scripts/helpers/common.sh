function setup_environment {
    export TEST_DIR=/home/gpadmin/test_pgbackrest

    # Starting up demo cluster
    source "/usr/local/greenplum-db-devel/greenplum_path.sh"
    pushd gpdb_src/gpAux/gpdemo
    make create-demo-cluster WITH_MIRRORS=$2
    source gpdemo-env.sh
    popd

    # Creating backup and log directories for pgbackrest
    export TEST_NAME=$1
    mkdir -p "$TEST_DIR/logs/$TEST_NAME"
    mkdir -p "$TEST_DIR/$TEST_NAME"
    export DATADIR="${MASTER_DATA_DIRECTORY%*/*/*}"
    export MASTER=${DATADIR}/qddir/demoDataDir-1
    export PRIMARY1=${DATADIR}/dbfast1/demoDataDir0
    export PRIMARY2=${DATADIR}/dbfast2/demoDataDir1
    export PRIMARY3=${DATADIR}/dbfast3/demoDataDir2

    if [ "$2" = true ]; then
        export MIRROR1=${DATADIR}/dbfast_mirror1/demoDataDir0
        export MIRROR2=${DATADIR}/dbfast_mirror2/demoDataDir1
        export MIRROR3=${DATADIR}/dbfast_mirror3/demoDataDir2
    fi

    # Filling the pgbackrest.conf configuration file
    cat <<EOF > /etc/pgbackrest.conf
[seg-1]
pg1-path=$MASTER
pg1-port=$PGPORT

[seg0]
pg1-path=$PRIMARY1
pg1-port=$((PGPORT+2))

[seg1]
pg1-path=$PRIMARY2
pg1-port=$((PGPORT+3))

[seg2]
pg1-path=$PRIMARY3
pg1-port=$((PGPORT+4))

[global]
repo1-path=$TEST_DIR/$TEST_NAME
log-path=$TEST_DIR/logs/$TEST_NAME
start-fast=y
fork=GPDB
EOF

    # Initializing pgbackrest for GPDB
    for i in -1 0 1 2
    do
        PGOPTIONS="-c gp_session_role=utility" pgbackrest --stanza=seg$i stanza-create &
    done
    wait

    gpconfig -c archive_mode -v on

    gpconfig -c archive_command -v "'PGOPTIONS=\"-c gp_session_role=utility\" \
    pgbackrest --stanza=seg%c archive-push %p'" --skipvalidation

    gpstop -ar

    # pgbackrest health check
    for i in -1 0 1 2
    do
        PGOPTIONS="-c gp_session_role=utility" pgbackrest --stanza=seg$i check
    done

    export PGDATABASE=testdb
    createdb

    GP_VERSION_NUM=$(pg_config --gp_version | cut -c 11)

    if [ "$GP_VERSION_NUM" -le 6 ]; then
        psql -c "create extension gp_pitr;"
        export RESTORE_OPTIONS=""
        export PYTHON_EXTENSION_NAME="plpythonu"
    else
        export RESTORE_OPTIONS="--target-action=promote"
        export PYTHON_EXTENSION_NAME="plpython3u"
    fi
}

function dump_table {
    psql -Atc "select * from $1 order by a, b;" -o "$TEST_DIR/$TEST_NAME/$1_$2.txt"
}
