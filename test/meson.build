####################################################################################################################################
# Smoke test
####################################################################################################################################
# Test a built pgbackrest through a backup/restore cycle against every PostgreSQL found on the system. Shipped in the distribution
# so packagers can verify a build. Fails when there is no PostgreSQL to test against.
test(
    'smoke',
    python,
    args: [files('smoke.py'), '--pgbackrest', pgbackrest_exe],
    suite: 'smoke',
    timeout: 1800,
)

####################################################################################################################################
# Fuzz targets
####################################################################################################################################
# Link each target in fuzz with the objects of the pgbackrest binary, except main.c, and the fuzzing engine. The sources must be
# compiled with the instrumentation flags of the engine, e.g. -fsanitize=fuzzer-no-link.
if get_option('fuzz-engine') != ''
    fuzz_src = []

    foreach src : src_pgbackrest
        if src != 'main.c'
            fuzz_src += src
        endif
    endforeach

    fuzz_object = pgbackrest_exe.extract_objects(fuzz_src)

    foreach fuzz : ['ini', 'json', 'pack']
        executable(
            'fuzz-' + fuzz,
            'fuzz/fuzz.c',
            'fuzz/' + fuzz + '.c',
            objects: fuzz_object,
            include_directories: include_directories('../src'),
            dependencies: [
                lib_backtrace,
                lib_bz2,
                lib_openssl,
                lib_lz4,
                lib_systemd,
                lib_pq,
                lib_socket,
                lib_ssh2,
                lib_xml,
                lib_z,
                lib_zstd,
            ],
            link_args: get_option('fuzz-engine').split(),
        )
    endforeach
endif
