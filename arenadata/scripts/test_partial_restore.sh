#!/usr/bin/env bash
set -exo pipefail

function dump_table {
    psql -Atc "select * from $1 order by a, b;" -o "$PGBACKREST_TEST_DIR/$TEST_NAME/$1_$2.txt"
}

PGBACKREST_TEST_DIR=/home/gpadmin/test_pgbackrest

# Starting up demo cluster
source "/usr/local/greenplum-db-devel/greenplum_path.sh"
pushd gpdb_src/gpAux/gpdemo
make create-demo-cluster WITH_MIRRORS=true
source gpdemo-env.sh
popd

# Creating backup and log directories for pgbackrest
TEST_NAME=$(basename "${0%.sh}")
mkdir -p "$PGBACKREST_TEST_DIR/logs/$TEST_NAME"
mkdir -p "$PGBACKREST_TEST_DIR/$TEST_NAME"
DATADIR="${MASTER_DATA_DIRECTORY%*/*/*}"
MASTER=${DATADIR}/qddir/demoDataDir-1
PRIMARY1=${DATADIR}/dbfast1/demoDataDir0
PRIMARY2=${DATADIR}/dbfast2/demoDataDir1
PRIMARY3=${DATADIR}/dbfast3/demoDataDir2
MIRROR1=${DATADIR}/dbfast_mirror1/demoDataDir0
MIRROR2=${DATADIR}/dbfast_mirror2/demoDataDir1
MIRROR3=${DATADIR}/dbfast_mirror3/demoDataDir2

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
repo1-path=$PGBACKREST_TEST_DIR/$TEST_NAME
log-path=$PGBACKREST_TEST_DIR/logs/$TEST_NAME
start-fast=y
fork=GPDB
EOF

# Initializing pgbackrest for GPDB
for i in -1 0 1 2
do
    PGOPTIONS="-c gp_session_role=utility" pgbackrest --stanza=seg$i stanza-create
done

gpconfig -c archive_mode -v on

gpconfig -c archive_command -v "'PGOPTIONS=\"-c gp_session_role=utility\" \
pgbackrest --stanza=seg%c archive-push %p'" --skipvalidation

gpstop -ar

# pgbackrest health check
for i in -1 0 1 2
do
    PGOPTIONS="-c gp_session_role=utility" pgbackrest --stanza=seg$i check
done

# The test scenario starts here
export PGDATABASE=testdb
createdb
psql -c "create view random_array as select array_agg((random() * 100)::int) from generate_series(1, 128)a;"
# Сreate test tables
psql -c "create table t1 (a int, b int[128]) distributed by(a);"
psql -c "create table t2 (a int, b int[128]) distributed by(a);"

psql -c "create table t4 (a int, b int[128]) with (appendoptimized=true) distributed by(a);"
psql -c "create table t5 (a int, b int[128]) with (appendoptimized=true) distributed by(a);"

psql -c "create table t7 (a int, b int[128]) with (appendoptimized=true, orientation=column) distributed by(a);"
psql -c "create table t8 (a int, b int[128]) with (appendoptimized=true, orientation=column) distributed by(a);"

psql -c "insert into t1 select a, (select * from random_array) from generate_series(1, 100000)a;"
psql -c "insert into t2 select a, (select * from random_array) from generate_series(1, 100000)a;"

psql -c "insert into t4 select a, (select * from random_array) from generate_series(1, 100000)a;"
psql -c "insert into t5 select a, (select * from random_array) from generate_series(1, 100000)a;"

# Сreate backup
for i in -1 0 1 2
do
    PGOPTIONS="-c gp_session_role=utility" pgbackrest --stanza=seg$i --type=full backup
done

# Create new tables and add data to existing ones.
psql -c "create table t3 (a int, b int[128]) distributed by(a);"
psql -c "create table t6 (a int, b int[128]) with (appendoptimized=true) distributed by(a);"
psql -c "create table t9 (a int, b int[128]) with (appendoptimized=true, orientation=column) distributed by(a);"
psql -c "insert into t3 select a, (select * from random_array) from generate_series(1, 100000)a;"
psql -c "insert into t6 select a, (select * from random_array) from generate_series(1, 100000)a;"
psql -c "insert into t9 select a, (select * from random_array) from generate_series(1, 100000)a;"

psql -c "insert into t1 select a, (select * from random_array) from generate_series(1, 100000)a;"
psql -c "insert into t2 select a, (select * from random_array) from generate_series(1, 100000)a;"
psql -c "insert into t4 select a, (select * from random_array) from generate_series(1, 100000)a;"
psql -c "insert into t5 select a, (select * from random_array) from generate_series(1, 100000)a;"
psql -c "insert into t7 select a, (select * from random_array) from generate_series(1, 100000)a;"
psql -c "insert into t8 select a, (select * from random_array) from generate_series(1, 100000)a;"

GP_VERSION_NUM=$(pg_config --gp_version | cut -c 11)

if [ "$GP_VERSION_NUM" -le 6 ]; then
    psql -c "create extension gp_pitr;"
    RESTORE_OPTIONS=""
    PYTHON_EXTENSION_NAME="plpythonu"
else
    RESTORE_OPTIONS="--target-action=promote"
    PYTHON_EXTENSION_NAME="plpython3u"
fi

# Create restore point and send wal files to the archive
psql -c "select gp_create_restore_point('backup1');"
psql -c "select gp_switch_wal();"

dump_table t1 pre
dump_table t3 pre
dump_table t4 pre
dump_table t6 pre
dump_table t7 pre
dump_table t9 pre

psql -c "select * from gp_segment_configuration order by dbid" -o "$PGBACKREST_TEST_DIR/$TEST_NAME/gp_segment_conf_expected.out"

gpstop -a
rm -rf "${MASTER:?}/"* "${PRIMARY1:?}/"* "${PRIMARY2:?}/"* "${PRIMARY3:?}/"*
rm -rf "${MIRROR1:?}/"* "${MIRROR2:?}/"* "${MIRROR3:?}/"* "$DATADIR/standby/"*

# Restore the cluster without data to get metadata
echo "[]" >> "$PGBACKREST_TEST_DIR/$TEST_NAME/empty_filter.json"
for i in -1 0 1 2
do
    pgbackrest --stanza=seg$i --type=name --target=backup1 $RESTORE_OPTIONS --filter="$(realpath "$PGBACKREST_TEST_DIR/$TEST_NAME/empty_filter.json")" restore
done

gpstart -am
gpinitstandby -ar

PGOPTIONS="-c gp_session_role=utility" psql << EOF
set allow_system_table_mods to true;

update gp_segment_configuration
set status = case when role='m' then 'd' else status end, mode = 'n'
where content >= 0;
EOF
gpstop -ra
# Prevent sending the WAL to the archive while receiving the metadata of the tables
gpconfig -r archive_command

psql -c "create language $PYTHON_EXTENSION_NAME;"
psql -c "create or replace function table_metadata_dump (to_dump text) returns text as \$\$
$(cat /home/gpadmin/pgbackrest/arenadata/scripts/helpers/partial_restore_helper.py)
\$\$ language $PYTHON_EXTENSION_NAME"

# Dump metadata
psql -Atc "select * from table_metadata_dump(\$\$'t1','t3','t4','t6','t7','t9'\$\$)" -o "$PGBACKREST_TEST_DIR/$TEST_NAME/filter_seg-1.json"
psql -Atc "select table_metadata_dump(\$\$'t1','t3','t4','t6','t7','t9'\$\$) from gp_dist_random(\$\$gp_id\$\$) where gp_segment_id = 0;" -o "$PGBACKREST_TEST_DIR/$TEST_NAME/filter_seg0.json"
psql -Atc "select table_metadata_dump(\$\$'t1','t3','t4','t6','t7','t9'\$\$) from gp_dist_random(\$\$gp_id\$\$) where gp_segment_id = 1;" -o "$PGBACKREST_TEST_DIR/$TEST_NAME/filter_seg1.json"
psql -Atc "select table_metadata_dump(\$\$'t1','t3','t4','t6','t7','t9'\$\$) from gp_dist_random(\$\$gp_id\$\$) where gp_segment_id = 2;" -o "$PGBACKREST_TEST_DIR/$TEST_NAME/filter_seg2.json"

gpstop -a
rm -rf "${MASTER:?}/"* "${PRIMARY1:?}/"* "${PRIMARY2:?}/"* "${PRIMARY3:?}/"*

# Restore only tables t1, t3, t3 and t6
for i in -1 0 1 2
do
    pgbackrest --stanza=seg$i --type=name --target=backup1 $RESTORE_OPTIONS --filter="$(realpath "$PGBACKREST_TEST_DIR/$TEST_NAME/filter_seg$i.json")" restore
done
gpstart -am
gpinitstandby -ar
PGOPTIONS="-c gp_session_role=utility" psql << EOF
set allow_system_table_mods to true;

update gp_segment_configuration
set status = case when role='m' then 'd' else status end, mode = 'n'
where content >= 0;
EOF
gpstop -ra

# Dump tables after restore
dump_table t1 after
dump_table t3 after
dump_table t4 after
dump_table t6 after
dump_table t7 after
dump_table t9 after

# Verify data integrity.
diff "$PGBACKREST_TEST_DIR/$TEST_NAME/t1_pre.txt" "$PGBACKREST_TEST_DIR/$TEST_NAME/t1_after.txt"
diff "$PGBACKREST_TEST_DIR/$TEST_NAME/t3_pre.txt" "$PGBACKREST_TEST_DIR/$TEST_NAME/t3_after.txt"
diff "$PGBACKREST_TEST_DIR/$TEST_NAME/t4_pre.txt" "$PGBACKREST_TEST_DIR/$TEST_NAME/t4_after.txt"
diff "$PGBACKREST_TEST_DIR/$TEST_NAME/t6_pre.txt" "$PGBACKREST_TEST_DIR/$TEST_NAME/t6_after.txt"
diff "$PGBACKREST_TEST_DIR/$TEST_NAME/t7_pre.txt" "$PGBACKREST_TEST_DIR/$TEST_NAME/t7_after.txt"
diff "$PGBACKREST_TEST_DIR/$TEST_NAME/t9_pre.txt" "$PGBACKREST_TEST_DIR/$TEST_NAME/t9_after.txt"

gprecoverseg -aF
gpinitstandby -as "$HOSTNAME" -S "$DATADIR/standby" -P $((PGPORT+1))

# Checking cluster configuration after restore
psql -c "select * from gp_segment_configuration order by dbid" -o "$PGBACKREST_TEST_DIR/$TEST_NAME/gp_segment_conf_result.out"
diff "$PGBACKREST_TEST_DIR/$TEST_NAME/gp_segment_conf_expected.out" "$PGBACKREST_TEST_DIR/$TEST_NAME/gp_segment_conf_result.out"
