#!/usr/bin/env bash
set -exo pipefail

# Test partial restore.
# The test consists of the following steps:
#  1. Create tables before backup and filling them with data.
#  2. Create backup
#  3. Create tables after backup and fill them with data. These tables and their data are stored in WAL and must be filtered out
#  during archive-get.
#  4. Create a restore point and send the WAL to the archive.
#  5. Saving the contents of the tables on disk.
#  6. Stop and delete the cluster.
#  7. Restore only the system catalog.
#  8. Create a filter file for each segment. The filter contains both tables created before the backup and tables created after.
#  9. Stop and delete the cluster.
#  10. Restoring only a specific set of tables.
#  11. Check the correctness of the restored data.
#  12. Restore the mirrors.
#  13. Check the correctness of cluster restoration.

source /home/gpadmin/pgbackrest/arenadata/scripts/helpers/common.sh

setup_environment $(basename "${0%.sh}") true

# The test scenario starts here
psql -c "create view random_array as select array_agg((random() * 100)::int) from generate_series(1, 128)a;" &
# Step 1
# Create test tables before backup
psql -c "create table t1 (a int, b int[128]) distributed by(a);" &
psql -c "create table t2 (a int, b int[128]) distributed by(a);" &

psql -c "create table t4 (a int, b int[128]) with (appendoptimized=true) distributed by(a);" &
psql -c "create table t5 (a int, b int[128]) with (appendoptimized=true) distributed by(a);" &

psql -c "create table t7 (a int, b int[128]) with (appendoptimized=true, orientation=column) distributed by(a);" &
psql -c "create table t8 (a int, b int[128]) with (appendoptimized=true, orientation=column) distributed by(a);" &
wait

psql -c "insert into t1 select a, (select * from random_array) from generate_series(1, 100000)a;" &
psql -c "insert into t2 select a, (select * from random_array) from generate_series(1, 100000)a;" &

psql -c "insert into t4 select a, (select * from random_array) from generate_series(1, 100000)a;" &
psql -c "insert into t5 select a, (select * from random_array) from generate_series(1, 100000)a;" &

wait

# Step 2
# Create backup
for i in -1 0 1 2
do
    PGOPTIONS="-c gp_session_role=utility" pgbackrest --stanza=seg$i --type=full backup &
done
wait

# Step 3
# Create new tables and add data to existing ones.
psql -c "create table t3 (a int, b int[128]) distributed by(a);" &
# Add several checkpoints inside the transaction to test the pending delete records.
psql -c "begin; create table t6 (a int, b int[128]) with (appendoptimized=true) distributed by(a); checkpoint; commit;" &
psql -c "begin; create table t9 (a int, b int[128]) with (appendoptimized=true, orientation=column) distributed by(a); checkpoint; commit;" &
psql -c "CREATE TABLE tp1 (a int, b int, c int) DISTRIBUTED BY (a) PARTITION BY LIST (a) SUBPARTITION BY LIST (b)
  SUBPARTITION TEMPLATE ( SUBPARTITION prt1 VALUES (1)) (PARTITION ptr1 VALUES (1));" &
wait
psql -c "insert into t3 select a, (select * from random_array) from generate_series(1, 100000)a;" &
psql -c "insert into t6 select a, (select * from random_array) from generate_series(1, 100000)a;" &
psql -c "insert into t9 select a, (select * from random_array) from generate_series(1, 100000)a;" &

psql -c "insert into t1 select a, (select * from random_array) from generate_series(1, 100000)a;" &
psql -c "insert into t2 select a, (select * from random_array) from generate_series(1, 100000)a;" &
psql -c "insert into t4 select a, (select * from random_array) from generate_series(1, 100000)a;" &
psql -c "insert into t5 select a, (select * from random_array) from generate_series(1, 100000)a;" &
psql -c "insert into t7 select a, (select * from random_array) from generate_series(1, 100000)a;" &
psql -c "insert into t8 select a, (select * from random_array) from generate_series(1, 100000)a;" &
psql -c "insert into tp1 select 1, 1, a from generate_series(1, 100000)a;" &
wait

# Step 4
# Create restore point and send wal files to the archive
psql -c "select gp_create_restore_point('backup1');"
psql -c "select gp_switch_wal();"

# Step 5
dump_table t1 pre &
dump_table t3 pre &
dump_table t4 pre &
dump_table t6 pre &
dump_table t7 pre &
dump_table t9 pre &
dump_table tp1 pre &
wait

psql -c "select * from gp_segment_configuration order by dbid" -o "$TEST_DIR/$TEST_NAME/gp_segment_conf_expected.out"

# Step 6
gpstop -a
rm -rf "${MASTER:?}/"* "${PRIMARY1:?}/"* "${PRIMARY2:?}/"* "${PRIMARY3:?}/"*
rm -rf "${MIRROR1:?}/"* "${MIRROR2:?}/"* "${MIRROR3:?}/"* "$DATADIR/standby/"*

# Step 7
# Restore the cluster without data to get metadata
echo "[]" > "$TEST_DIR/$TEST_NAME/empty_filter.json"
for i in -1 0 1 2
do
    pgbackrest --stanza=seg$i --type=name --target=backup1 $RESTORE_OPTIONS --filter="$(realpath "$TEST_DIR/$TEST_NAME/empty_filter.json")" restore &
done
wait

gpstart -am
gpinitstandby -ar

# Mark mirror segments as unavailable
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

# Step 8
# Dump metadata
psql -Atc "select * from table_metadata_dump(\$\$'t1','t3','t4','t6','t7','t9','tp1','tp1_1_prt_ptr1','tp1_1_prt_ptr1_2_prt_prt1'\$\$)" -o "$TEST_DIR/$TEST_NAME/filter_seg-1.json"
psql -Atc "select table_metadata_dump(\$\$'t1','t3','t4','t6','t7','t9','tp1','tp1_1_prt_ptr1','tp1_1_prt_ptr1_2_prt_prt1'\$\$) from gp_dist_random(\$\$gp_id\$\$) where gp_segment_id = 0;" -o "$TEST_DIR/$TEST_NAME/filter_seg0.json"
psql -Atc "select table_metadata_dump(\$\$'t1','t3','t4','t6','t7','t9','tp1','tp1_1_prt_ptr1','tp1_1_prt_ptr1_2_prt_prt1'\$\$) from gp_dist_random(\$\$gp_id\$\$) where gp_segment_id = 1;" -o "$TEST_DIR/$TEST_NAME/filter_seg1.json"
psql -Atc "select table_metadata_dump(\$\$'t1','t3','t4','t6','t7','t9','tp1','tp1_1_prt_ptr1','tp1_1_prt_ptr1_2_prt_prt1'\$\$) from gp_dist_random(\$\$gp_id\$\$) where gp_segment_id = 2;" -o "$TEST_DIR/$TEST_NAME/filter_seg2.json"
cat "$TEST_DIR/$TEST_NAME/filter_seg-1.json"
cat "$TEST_DIR/$TEST_NAME/filter_seg0.json"
cat "$TEST_DIR/$TEST_NAME/filter_seg1.json"
cat "$TEST_DIR/$TEST_NAME/filter_seg2.json"

# Step 9
gpstop -a
rm -rf "${MASTER:?}/"* "${PRIMARY1:?}/"* "${PRIMARY2:?}/"* "${PRIMARY3:?}/"*

# Step 10
# Restore only tables t1, t3, t4, t6, t7, t9 and tp1
for i in -1 0 1 2
do
    pgbackrest --stanza=seg$i --type=name --target=backup1 $RESTORE_OPTIONS --filter="$(realpath "$TEST_DIR/$TEST_NAME/filter_seg$i.json")" restore &
done
wait

gpstart -am
gpinitstandby -ar

# Mark mirror segments as unavailable
PGOPTIONS="-c gp_session_role=utility" psql << EOF
set allow_system_table_mods to true;

update gp_segment_configuration
set status = case when role='m' then 'd' else status end, mode = 'n'
where content >= 0;
EOF
gpstop -ra

# Dump tables after restore
dump_table t1 after &
dump_table t3 after &
dump_table t4 after &
dump_table t6 after &
dump_table t7 after &
dump_table t9 after &
dump_table tp1 after &
wait

# Step 11
# Verify data integrity.
diff "$TEST_DIR/$TEST_NAME/t1_pre.txt" "$TEST_DIR/$TEST_NAME/t1_after.txt"
diff "$TEST_DIR/$TEST_NAME/t3_pre.txt" "$TEST_DIR/$TEST_NAME/t3_after.txt"
diff "$TEST_DIR/$TEST_NAME/t4_pre.txt" "$TEST_DIR/$TEST_NAME/t4_after.txt"
diff "$TEST_DIR/$TEST_NAME/t6_pre.txt" "$TEST_DIR/$TEST_NAME/t6_after.txt"
diff "$TEST_DIR/$TEST_NAME/t7_pre.txt" "$TEST_DIR/$TEST_NAME/t7_after.txt"
diff "$TEST_DIR/$TEST_NAME/t9_pre.txt" "$TEST_DIR/$TEST_NAME/t9_after.txt"
diff "$TEST_DIR/$TEST_NAME/tp1_pre.txt" "$TEST_DIR/$TEST_NAME/tp1_after.txt"

# Step 12
gprecoverseg -aF
gpinitstandby -as "$HOSTNAME" -S "$DATADIR/standby" -P $((PGPORT+1))

# Step 13
# Checking cluster configuration after restore
psql -c "select * from gp_segment_configuration order by dbid" -o "$TEST_DIR/$TEST_NAME/gp_segment_conf_result.out"
diff "$TEST_DIR/$TEST_NAME/gp_segment_conf_expected.out" "$TEST_DIR/$TEST_NAME/gp_segment_conf_result.out"
