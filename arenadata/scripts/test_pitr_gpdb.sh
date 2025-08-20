#!/usr/bin/env bash
set -exo pipefail

source /home/gpadmin/pgbackrest/arenadata/scripts/helpers/common.sh

setup_environment $(basename "${0%.sh}") true

# The test scenario starts here
# Creating initial dataset
export PGDATABASE=gpdb_pitr_database
createdb
psql -c "CREATE TABLE t1 AS SELECT id, 'text'||id AS text FROM generate_series(1,30) id DISTRIBUTED BY (id);"

# Saving cluster configuration status
psql -c "SELECT * FROM gp_segment_configuration ORDER BY dbid" -o "$TEST_DIR/$TEST_NAME/gp_segment_conf_expected.out"

# Creating full backup on master and seg0
for i in -1 0
do 
    PGOPTIONS="-c gp_session_role=utility" pgbackrest --stanza=seg$i --type=full backup &
done
wait

# Checking the presence of first backup
function check_backup(){
    segment_backup_dir=$TEST_DIR/$TEST_NAME/backup/seg$1
    current_date=$(date +%Y%m%d)
        
    if [[ -z $(find "$segment_backup_dir" -maxdepth 1 -type d -name "${current_date}-??????F" -not -empty ) ]];
    then
        echo "The backup directory for segment $1 was not found" > /dev/null && exit 1
    fi
}
for i in -1 0
do 
   check_backup $i
done

# Creating additional table
psql -c "CREATE TABLE t2 AS SELECT id, 'text'||id AS text FROM generate_series(1,30) id DISTRIBUTED BY (id);"

psql -c "SELECT * FROM t2 ORDER BY id;" \
-o "$TEST_DIR/$TEST_NAME/t2_rows_original.out"

# Filling the first table with more data at seg0 after the first backup
psql -c "INSERT INTO t1 SELECT 3, 'text'||i FROM generate_series(1,10) i;"

psql -c "SELECT * FROM t1 ORDER BY id;" \
-o "$TEST_DIR/$TEST_NAME/t1_rows_original.out"

# Creating full backup on seg1 and seg2
for i in 1 2
do 
    PGOPTIONS="-c gp_session_role=utility" pgbackrest --stanza=seg$i --type=full backup &
done
wait

# Checking the presence of second backup
for i in 1 2
do 
   check_backup $i
done

# Determine version
GP_VERSION_NUM=$(pg_config --gp_version | cut -c 11)

# GPDB 7 doesn't need gp_pitr extension, but needs --target-action=promote
if [ $GP_VERSION_NUM -le 6 ]; then
    psql -c "create extension gp_pitr;"
    RESTORE_OPTIONS=""
else
    RESTORE_OPTIONS="--target-action=promote"
fi
# Creating a distributed restore point..."
psql -c "select gp_create_restore_point('test_pitr');"
psql -c "select gp_switch_wal();"

# Simulating disaster
psql -c "drop table t1;"
psql -c "truncate table t2;"

gpstop -a
rm -rf "${MASTER:?}/"* "${PRIMARY1:?}/"* "${PRIMARY2:?}/"* "${PRIMARY3:?}/"*
rm -rf "${MIRROR1:?}/"* "${MIRROR2:?}/"* "${MIRROR3:?}/"* "$DATADIR/standby/"*

# Restoring cluster
for i in -1 0 1 2
do 
    pgbackrest --stanza=seg$i --type=name --target=test_pitr $RESTORE_OPTIONS restore &
done
wait

# Configuring mirrors after primary restore
gpstart -am
gpinitstandby -ar

PGOPTIONS="-c gp_session_role=utility" psql << EOF
SET allow_system_table_mods to true;
UPDATE gp_segment_configuration
SET status = CASE WHEN role='m' THEN 'd' ELSE status END, mode = 'n'
WHERE content >= 0;
EOF

gpstop -ar
gprecoverseg -aF
gpinitstandby -as "$HOSTNAME" -S "$DATADIR/standby" -P $((PGPORT+1))

# Checking cluster configuration after restore
psql -c "SELECT * FROM gp_segment_configuration ORDER BY dbid" -o "$TEST_DIR/$TEST_NAME/gp_segment_conf_result.out"

diff "$TEST_DIR/$TEST_NAME/gp_segment_conf_expected.out" "$TEST_DIR/$TEST_NAME/gp_segment_conf_result.out"

# Checking data integrity
psql -c "SELECT * FROM t1 ORDER BY id;" \
-o "$TEST_DIR/$TEST_NAME/t1_rows_restored.out"

psql -c "SELECT * FROM t2 ORDER BY id;" \
-o "$TEST_DIR/$TEST_NAME/t2_rows_restored.out"

diff "$TEST_DIR/$TEST_NAME/t1_rows_original.out" "$TEST_DIR/$TEST_NAME/t1_rows_restored.out"

diff "$TEST_DIR/$TEST_NAME/t2_rows_original.out" "$TEST_DIR/$TEST_NAME/t2_rows_restored.out"
