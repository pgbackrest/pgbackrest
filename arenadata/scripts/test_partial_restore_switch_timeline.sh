#!/usr/bin/env bash
set -exo pipefail

# Test partial restore with a timeline change between backup and restore point.
# The test consists of the following steps:
#  1. Create tables before backup and filling them with data.
#  2. Create backup
#  3. Create tables after backup and fill them with data. These tables and their data are stored in WAL and must be filtered out
#  during archive-get.
#  4. Seg0 went down
#  5. Seg0 went back
#  6. Seg2 went down
#  7. Create a restore point and send the WAL to the archive.
#  8. Saving the contents of the tables on disk.
#  9. Stop and delete the cluster.
#  10. Restore only the system catalog.
#  11. Create a filter file for each segment. The filter contains both tables created before the backup and tables created after.
#  12. Stop and delete the cluster.
#  13. Restoring only a specific set of tables.
#  14. Check the correctness of the restored data.
#  15. Restore the mirrors.
#  16. Check the correctness of cluster restoration.

source /home/gpadmin/pgbackrest/arenadata/scripts/helpers/common.sh

setup_environment $(basename "${0%.sh}") true

# The test scenario starts here
psql -c "create view random_array as select array_agg((random() * 100)::int) from generate_series(1, 128)a;" &
# Step 1
# Create test tables
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

psql -c "select * from gp_segment_configuration order by dbid" -o "$TEST_DIR/$TEST_NAME/gp_segment_conf_expected.out"

# Step 3
# Create new tables and add data to existing ones.
psql -c "create table t3 (a int, b int[128]) distributed by(a);" &
psql -c "create table t6 (a int, b int[128]) with (appendoptimized=true) distributed by(a);" &
psql -c "create table t9 (a int, b int[128]) with (appendoptimized=true, orientation=column) distributed by(a);" &
wait
psql -c "insert into t3 select a, (select * from random_array) from generate_series(1, 100000)a;"

# Step 4
# seg0 went down
kill -9 $(ps aux | grep dbfast1/demoDataDir0 | grep -v grep | awk '{print $2}')
# Update pgbackrest.conf to archive the WAL from the mirror.
sudo sed -i 's/dbfast1\/demoDataDir0/dbfast_mirror1\/demoDataDir0/g' /etc/pgbackrest.conf
sudo sed -i "s/$((PGPORT+2))/$((PGPORT+5))/g" /etc/pgbackrest.conf
psql -c "select gp_request_fts_probe_scan();"

psql -c "insert into t6 select a, (select * from random_array) from generate_series(1, 100000)a;" &
psql -c "insert into t9 select a, (select * from random_array) from generate_series(1, 100000)a;" &
wait

# Step 5
# seg0 went back
gprecoverseg -a
# Wait for sync
while [[ "$(psql -Atc "select mode from gp_segment_configuration where dbid = 5;")" != "s" ]]; do
    sleep 1
done
gprecoverseg -ar

sudo sed -i 's/dbfast_mirror1\/demoDataDir0/dbfast1\/demoDataDir0/g' /etc/pgbackrest.conf
sudo sed -i "s/$((PGPORT+5))/$((PGPORT+2))/g" /etc/pgbackrest.conf
psql -c "select gp_request_fts_probe_scan();"

psql -c "insert into t1 select a, (select * from random_array) from generate_series(1, 100000)a;" &
psql -c "insert into t2 select a, (select * from random_array) from generate_series(1, 100000)a;" &
psql -c "insert into t4 select a, (select * from random_array) from generate_series(1, 100000)a;" &
wait

# Step 6
# seg2 went down
kill -9 $(ps aux | grep dbfast2/demoDataDir1 | grep -v grep | awk '{print $2}')
# Update pgbackrest.conf to archive the WAL from the mirror and restore to mirror data directory.
sudo sed -i 's/dbfast2\/demoDataDir1/dbfast_mirror2\/demoDataDir1/g' /etc/pgbackrest.conf
sudo sed -i "s/$((PGPORT+3))/$((PGPORT+6))/g" /etc/pgbackrest.conf
psql -c "select gp_request_fts_probe_scan();"

psql -c "insert into t5 select a, (select * from random_array) from generate_series(1, 100000)a;" &
psql -c "insert into t7 select a, (select * from random_array) from generate_series(1, 100000)a;" &
psql -c "insert into t8 select a, (select * from random_array) from generate_series(1, 100000)a;" &
wait

# Step 7
# Create restore point and send wal files to the archive
psql -c "select gp_create_restore_point('backup1');"
psql -c "select gp_switch_wal();"

# Step 8
dump_table t1 pre &
dump_table t3 pre &
dump_table t4 pre &
dump_table t6 pre &
dump_table t7 pre &
dump_table t9 pre &
wait

# Step 9
gpstop -a
rm -rf "${MASTER:?}/"* "${PRIMARY1:?}/"* "${PRIMARY2:?}/"* "${PRIMARY3:?}/"*
rm -rf "${MIRROR1:?}/"* "${MIRROR2:?}/"* "${MIRROR3:?}/"* "$DATADIR/standby/"*

# Step 10
# Restore the cluster without data to get metadata
echo "[]" > "$TEST_DIR/$TEST_NAME/empty_filter.json"
for i in -1 0 1 2
do
    # seg0 has a 3 timeline because it switched between the primary and the mirror twice.
    # seg1 has a 2 timeline because it switched between the primary and the mirror once.
    # The other segments have a 1 timeline since they did not switch between the primary and the mirror.
    target_timeline=1

    if [ $i == 0 ]; then
      target_timeline=3
    fi

    if [ $i == 1 ]; then
      target_timeline=2
    fi

    pgbackrest \
     --stanza=seg$i \
     --type=name \
     --target=backup1 \
     $RESTORE_OPTIONS \
     --filter="$(realpath "$TEST_DIR/$TEST_NAME/empty_filter.json")" \
     --target-timeline=$target_timeline \
     restore &
done
wait

# The backup contains a primary, but we are restoring this segment as a mirror. Therefore, we set the correct dbid and port.
echo "gp_dbid=6" > "$MIRROR2/internal.auto.conf"
echo "port=$((PGPORT+6))" >> "$MIRROR2/postgresql.conf"
gpstart -am
gpinitstandby -ar

# Mark mirror segments as unavailable
PGOPTIONS="-c gp_session_role=utility" psql << EOF
SET allow_system_table_mods to true;

UPDATE gp_segment_configuration
SET status = CASE WHEN role='m' THEN 'd' ELSE status END, mode = 'n'
WHERE content >= 0;
EOF
gpstop -ra
# Prevent sending the WAL to the archive while receiving the metadata of the tables
gpconfig -r archive_command

psql -c "create language $PYTHON_EXTENSION_NAME;"
psql -c "create or replace function table_metadata_dump (to_dump text) returns text as \$\$
$(cat /home/gpadmin/pgbackrest/arenadata/scripts/helpers/partial_restore_helper.py)
\$\$ language $PYTHON_EXTENSION_NAME"

# Step 11
# Dump metadata
psql -Atc "select * from table_metadata_dump(\$\$'t1','t3','t4','t6','t7','t9'\$\$)" -o "$TEST_DIR/$TEST_NAME/filter_seg-1.json"
psql -Atc "select table_metadata_dump(\$\$'t1','t3','t4','t6','t7','t9'\$\$) from gp_dist_random(\$\$gp_id\$\$) where gp_segment_id = 0;" -o "$TEST_DIR/$TEST_NAME/filter_seg0.json"
psql -Atc "select table_metadata_dump(\$\$'t1','t3','t4','t6','t7','t9'\$\$) from gp_dist_random(\$\$gp_id\$\$) where gp_segment_id = 1;" -o "$TEST_DIR/$TEST_NAME/filter_seg1.json"
psql -Atc "select table_metadata_dump(\$\$'t1','t3','t4','t6','t7','t9'\$\$) from gp_dist_random(\$\$gp_id\$\$) where gp_segment_id = 2;" -o "$TEST_DIR/$TEST_NAME/filter_seg2.json"

# Step 12
gpstop -a
rm -rf "${MASTER:?}/"* "${PRIMARY1:?}/"* "${PRIMARY2:?}/"* "${PRIMARY3:?}/"*
rm -rf "${MIRROR1:?}/"* "${MIRROR2:?}/"* "${MIRROR3:?}/"* "$DATADIR/standby/"*

# Step 13
# Restore only tables t1, t3, t4, t6, t7 and t9
for i in -1 0 1 2
do
    # seg0 has a 3 timeline because it switched between the primary and the mirror twice.
    # seg1 has a 2 timeline because it switched between the primary and the mirror once.
    # The other segments have a 1 timeline since they did not switch between the primary and the mirror.
    target_timeline=1

    if [ $i == 0 ]; then
      target_timeline=3
    fi

    if [ $i == 1 ]; then
      target_timeline=2
    fi

    pgbackrest \
     --stanza=seg$i \
     --type=name \
     --target=backup1 \
     $RESTORE_OPTIONS \
     --filter="$(realpath "$TEST_DIR/$TEST_NAME/filter_seg$i.json")" \
     --target-timeline=$target_timeline \
     restore &
done
wait

# The backup contains a primary, but we are restoring this segment as a mirror. Therefore, we set the correct dbid and port.
echo "gp_dbid=6" > "$MIRROR2/internal.auto.conf"
echo "port=$((PGPORT+6))" >> "$MIRROR2/postgresql.conf"
gpstart -am
gpinitstandby -ar

# Mark mirror segments as unavailable
PGOPTIONS="-c gp_session_role=utility" psql << EOF
SET allow_system_table_mods to true;

UPDATE gp_segment_configuration
SET status = CASE WHEN role='m' THEN 'd' ELSE status END, mode = 'n'
WHERE content >= 0;
EOF
gpstop -ra

# Dump tables after restore
dump_table t1 after &
dump_table t3 after &
dump_table t4 after &
dump_table t6 after &
dump_table t7 after &
dump_table t9 after &
wait

# Step 14
# Verify data integrity.
diff "$TEST_DIR/$TEST_NAME/t1_pre.txt" "$TEST_DIR/$TEST_NAME/t1_after.txt"
diff "$TEST_DIR/$TEST_NAME/t3_pre.txt" "$TEST_DIR/$TEST_NAME/t3_after.txt"
diff "$TEST_DIR/$TEST_NAME/t4_pre.txt" "$TEST_DIR/$TEST_NAME/t4_after.txt"
diff "$TEST_DIR/$TEST_NAME/t6_pre.txt" "$TEST_DIR/$TEST_NAME/t6_after.txt"
diff "$TEST_DIR/$TEST_NAME/t7_pre.txt" "$TEST_DIR/$TEST_NAME/t7_after.txt"
diff "$TEST_DIR/$TEST_NAME/t9_pre.txt" "$TEST_DIR/$TEST_NAME/t9_after.txt"

# Step 15
gprecoverseg -aF
gprecoverseg -ar
gpinitstandby -as "$HOSTNAME" -S "$DATADIR/standby" -P $((PGPORT+1))

# Step 16
# Checking cluster configuration after restore
psql -c "select * from gp_segment_configuration order by dbid" -o "$TEST_DIR/$TEST_NAME/gp_segment_conf_result.out"
diff "$TEST_DIR/$TEST_NAME/gp_segment_conf_expected.out" "$TEST_DIR/$TEST_NAME/gp_segment_conf_result.out"
