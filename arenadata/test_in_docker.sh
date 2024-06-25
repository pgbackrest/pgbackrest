#!/usr/bin/env bash
set -eo pipefail

LOG_EXTENSION=${LOG_EXTENSION:-"pg_log"}
ARCH=${ARCH:-"x86-64"}

function install_pgbackrest() {
    pushd /home/gpadmin/pgbackrest/src
    ./configure --enable-test
    make -j"$(nproc)" -s
    make install
    make clean
    popd
}

function stop_and_clear_gpdb() {
    source gpdb_src/gpAux/gpdemo/gpdemo-env.sh
    DATADIR="${MASTER_DATA_DIRECTORY%*/*/*}"
    # Attempt to stop the Greenplum Database cluster gracefully
    echo "Attempting to stop GPDB cluster gracefully..."
    if ! su - gpadmin -c "
        export MASTER_DATA_DIRECTORY=$MASTER_DATA_DIRECTORY
        source /usr/local/greenplum-db-devel/greenplum_path.sh &&
        gpstop -af "; then
        # In case gpstop didn't work, force stop all postgres processes
        # This approach is a catch-all attempt in case the cluster is in a
        # very broken state
        echo "Graceful stop failed. Attempting to force stop any \
        remaining postgres processes..."
        pkill -9 postgres
    fi
    #Waiting for all Postgres processes to terminate...
    while pgrep postgres > /dev/null; do
        #Postgres processes are still running. Waiting...
        sleep 1
    done

    # Now that all processes are ensured to be stopped,
    # clear the data directories.
    echo "Clearing GPDB data directories..."
    rm -rf "${DATADIR:?}/"*
}

function save_gpdb_logs() {
    rm -f "$1/"*.gz
    find gpdb_src/gpAux/gpdemo/datadirs/ -name "${LOG_EXTENSION}" \
    -type d -exec tar -rf "$1/${ARCH}_${LOG_EXTENSION}.tar" "{}" \;
    if [ -f "$1/${ARCH}_${LOG_EXTENSION}.tar" ]; then
        gzip -9 "$1/${ARCH}_${LOG_EXTENSION}.tar"
    fi
    tar -czf "$1/${ARCH}_gpAdminLogs.tar.gz" gpAdminLogs/
    tar -czf "$1/${ARCH}_gpAux.tar.gz" gpdb_src/gpAux/gpdemo/datadirs/gpAdminLogs/
}

function run_tests() {
    SUCCESS_COUNT=0
    FAILURE_COUNT=0
    tests_dir=$1
    for test_script in "$tests_dir"/*.sh; do
        TEST_NAME=$(basename "${test_script%.sh}")
        echo "Running test: $TEST_NAME"
        if su - gpadmin -c "bash $test_script"; then
             SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
        else
            echo "Test failed $TEST_NAME"
            FAILURE_COUNT=$((FAILURE_COUNT + 1))
        fi
        rm -rf "/home/gpadmin/test_pgbackrest/$TEST_NAME"
        save_gpdb_logs "/home/gpadmin/test_pgbackrest/logs/$TEST_NAME"
        chmod o+r "/home/gpadmin/test_pgbackrest/logs/$TEST_NAME/"*
        stop_and_clear_gpdb
    done

    echo "======================="
    echo "Tests run: $((SUCCESS_COUNT + FAILURE_COUNT))"
    echo "Passed: $SUCCESS_COUNT"
    echo "Failed: $FAILURE_COUNT"

    [ $FAILURE_COUNT -gt 0 ] && exit 1 || exit 0
}

# configure GPDB
source gpdb_src/concourse/scripts/common.bash
install_and_configure_gpdb
gpdb_src/concourse/scripts/setup_gpadmin_user.bash
# configure pgbackrest
install_pgbackrest
touch /etc/pgbackrest.conf
chmod o+w /etc/pgbackrest.conf
run_tests /home/gpadmin/pgbackrest/arenadata/scripts
