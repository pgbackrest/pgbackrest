-- An example of monitoring pgBackRest from within PostgresSQL
--
-- Use copy to export data from the pgBackRest info command into the jsonb
-- type so it can be queried directly by PostgresSQL.

-- Create monitor schema
create schema monitor;

-- Get pgBackRest info in JSON format
create function monitor.pgbackrest_info()
    returns jsonb AS $$
declare
    data jsonb;
begin
    -- Create a temp table to hold the JSON data
    create temp table temp_pgbackrest_data (data jsonb);

    -- Copy data into the table directory from the pgBackRest into command
    copy temp_pgbackrest_data (data)
        from program
            'pgbackrest --output=json info | tr ''\n'' '' ''' (format text);

    select temp_pgbackrest_data.data
      into data
      from temp_pgbackrest_data;

    drop table temp_pgbackrest_data;

    return data;
end $$ language plpgsql;
