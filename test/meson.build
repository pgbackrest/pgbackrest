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
