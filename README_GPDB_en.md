This manual describes how to compile pgBackRest, use it to create a backup copy of the Greenplum cluster and recover from it.

# 1. Supported versions of Greenplum

Currently, only [Greenplum 6](https://docs.vmware.com/en/VMware-Greenplum/6/greenplum-database/landing-index.html) is supported.

# 2. pgBackRest compilation

- Install dependencies

The following installation commands must be executed as root.

on CentOS 7:
```
yum install git gcc openssl-devel libxml2-devel bzip2-devel libzstd-devel lz4-devel libyaml-devel zlib-devel libssh2-devel
```

on ALT Linux p10
```
apt-get update
apt-get install git gcc openssl-devel libxml2-devel bzip2-devel libzstd-devel liblz4-devel libyaml-devel zlib-devel libssh2-devel
```

- Set environment variables

The `PATH` variable should contain a path to `pg_config` of Greenplum, and `LD_LIBRARY_PATH` should contain a path to `libpq.so.5`.

```
source <GPDB_DIR>/greenplum_path.sh
```

- Download the repository
```
git clone https://github.com/arenadata/pgbackrest -b 2.50-ci
```

- Go to the source code directory
```
cd pgbackrest/src/
```

- Run the configuration script

Optimization (-O2) and vectorization of page checksum calculations will be enabled without parameters.
```
./configure
```

For debugging, it is recommended to run the command
```
CFLAGS="-O0 -g3" ./configure --disable-optimize --enable-test
```

- Build

The command to build using all available cores and without displaying executable commands
```
make -j`nproc` -s
```

As a result, an executable file named `pgbackrest` will appear in the current directory.

- Install

```
sudo make install
```
By default, the `pgbackrest` executable file will be placed in the `/usr/local/bin` directory. This is the only action that is performed during the installation phase.


# 3. Configuration
- Create directories for backup and log. For the test, let it be `/tmp/backup` and `/tmp/backup/log`.
```
mkdir -p /tmp/backup/log
```

- Create a configuration file

In the following command examples, it is assumed that the configuration file has a standard name - `/etc/pgbackrest.conf`. If you need to use another file, its name can be passed through the `--config` parameter. For **standard demo cluster** created with `DATADIRS=/tmp/gpdb` the command to create a configuration file will require superuser rights and will look like this:
```
cat <<EOF > /etc/pgbackrest.conf
[seg-1]
pg1-path=/tmp/gpdb/qddir/demoDataDir-1
pg1-port=6000

[seg0]
pg1-path=/tmp/gpdb/dbfast1/demoDataDir0
pg1-port=6002

[seg1]
pg1-path=/tmp/gpdb/dbfast2/demoDataDir1
pg1-port=6003

[seg2]
pg1-path=/tmp/gpdb/dbfast3/demoDataDir2
pg1-port=6004

[global]
repo1-path=/tmp/backup
log-path=/tmp/backup/log
start-fast=y
fork=GPDB
EOF
```

This version of pgBackRest can be used for both PostgreSQL and Greenplum backups, so you should specify in the `fork` parameter which DBMS is backed up. Description of the remaining parameters can be found in the [documentation](https://pgbackrest.org/configuration.html) or in `build/help/help.xml`.

- Create and initialize directories for the coordinator and each primary segment in which files for recovery will be stored
```
for i in -1 0 1 2
do 
    PGOPTIONS="-c gp_session_role=utility" pgbackrest --stanza=seg$i stanza-create
done
```

- Configure Greenplum
```
gpconfig -c archive_mode -v on
gpconfig -c archive_command -v "'PGOPTIONS=\"-c gp_session_role=utility\" /usr/local/bin/pgbackrest --stanza=seg%c archive-push %p'" --skipvalidation
gpstop -ar
```

- Install the gp_pitr extension

Run the query below in any client application, for example in psql.
```
create extension gp_pitr;
```
This extension is necessary to work with restore points.

- Check that the backup and the WAL archiving are configured correctly
```
for i in -1 0 1 2
do 
	PGOPTIONS="-c gp_session_role=utility" pgbackrest --stanza=seg$i check
done
```
The command should not output an error message

# 4. Backup of the Greenplum cluster

4.1 Save files from primary segments and coordinator
```
for i in -1 0 1 2
do 
    PGOPTIONS="-c gp_session_role=utility" pgbackrest --stanza=seg$i backup
done
```
If you need to stop an interrupted backup, you can do this with the query
```
select pg_stop_backup() from gp_dist_random('gp_id');
```

4.2 Сreate a named restore point

Run the query
```
select gp_create_restore_point('backup1');
```
The record of the restore point will be stored in WAL.

4.3 Archive the files that store the created restore point.

Sending to the archive is carried out by switching WALs on coordinator and segments to new files using the query
```
select gp_switch_wal();
```

# 5. Restoring the Greenplum cluster

- Stop the cluster and delete the contents of the directories of all cluster components
```
gpstop -a
rm -rf /tmp/gpdb/qddir/demoDataDir-1/* /tmp/gpdb/dbfast1/demoDataDir0/* /tmp/gpdb/dbfast2/demoDataDir1/* /tmp/gpdb/dbfast3/demoDataDir2/* /tmp/gpdb/dbfast_mirror1/demoDataDir0/* /tmp/gpdb/dbfast_mirror2/demoDataDir1/* /tmp/gpdb/dbfast_mirror3/demoDataDir2/* /tmp/gpdb/standby/*
```

- Restore the contents of the coordinator's and primary segment directories from a backup

The name of the restore point from point 4.2 is passed in the `--target` parameter.
```
for i in -1 0 1 2
do 
    pgbackrest --stanza=seg$i --type=name --target=backup1 restore
done
```

- Run only the coordinator
```
gpstart -am
```

- Remove the secondary coordinator from the cluster
```
gpinitstandby -ar
```

- Mark mirror segments as unavailable
```
PGOPTIONS="-c gp_session_role=utility" psql postgres -c "
set allow_system_table_mods to true;
update gp_segment_configuration 
 set status = case when role='m' then 'd' else status end, 
     mode = 'n'
 where content >= 0;"
```

- Restart the cluster in normal mode
```
gpstop -ar
```

- Restore mirrors by primary segments
```
gprecoverseg -aF
```

- Restore the secondary coordinator
```
gpinitstandby -as $HOSTNAME -S /tmp/gpdb/standby -P 6001
```

- Make sure that all cluster components are restored and working

Run the query
```
select * from gp_segment_configuration order by content, role;
```

One line should be output for the coordinator, backup coordinator, and each primary and mirror segment. All rows in the `status` column must have the `u` value.
