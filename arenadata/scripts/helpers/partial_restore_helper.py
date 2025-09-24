    # This code is used in partial restore integration tests.
    # We use a single indentation because the code will be passed to psql as it is.
    import json

    database_info = plpy.execute('select oid, datname from pg_database where datname = current_database();')[0]

    tables = plpy.execute("""
        select
            pgc.oid,
            pgn.nspname,
            pgc.relname,
            pgc.relfilenode,
            segrelid,
            visimaprelid
        from
            pg_class pgc
        inner join
            pg_namespace pgn on pgn.oid = pgc.relnamespace
        left join
            pg_appendonly pap on pgc.oid = pap.relid
        left join
            pg_inherits inh on inh.inhrelid = pgc.oid
        where pgc.relkind in ('r', 'p')
            and pgc.oid >= 16384
            and pgc.relname in ({});
        """.format(to_dump))

    databases = [{
        "database": database_info['datname'],
        "dbOid": database_info['oid'],
        "tables": []
    }]

    database = databases[0]

    for table_info in tables:
        database['tables'].append({
            "tablefqn": table_info['relname'],
            "tableoid": table_info['oid'],
            "relfilenode": table_info['relfilenode'],
        })
        if table_info['segrelid'] is not None:
            seg_info = plpy.execute('select relname, relfilenode from pg_class where oid = {};'.format(table_info['segrelid']))[0]
            database['tables'].append({
                "tablefqn": seg_info['relname'],
                "tableoid": table_info['segrelid'],
                "relfilenode": seg_info['relfilenode'],
            })
        if table_info['visimaprelid'] is not None:
            visimap_info = plpy.execute('select relname, relfilenode from pg_class where oid = {};'.format(table_info['visimaprelid']))[0]
            database['tables'].append({
                "tablefqn": visimap_info['relname'],
                "tableoid": table_info['visimaprelid'],
                "relfilenode": visimap_info['relfilenode'],
            })
            index_info = plpy.execute('select pc.oid, relname, relfilenode from pg_index join pg_class pc on pc.oid = indexrelid where indrelid = {};'.format(table_info['visimaprelid']))[0]
            database['tables'].append({
                "tablefqn": index_info['relname'],
                "tableoid": index_info['oid'],
                "relfilenode": index_info['relfilenode'],
            })

    return json.dumps(databases)
