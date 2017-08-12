####################################################################################################################################
# Config Tests
####################################################################################################################################
use strict;
use warnings;
use Carp;
use English '-no_match_vars';

use Fcntl qw(O_RDONLY);

# Set number of tests
use Test::More tests => 56;

# Load the module dynamically so it does not interfere with the test above
use pgBackRest::LibC qw(:config :configRule);

# Config rule functions
ok (cfgCommandId('archive-push') eq CFGCMD_ARCHIVE_PUSH);
ok (cfgCommandId('restore') eq CFGCMD_RESTORE);

ok (cfgOptionId('target') eq CFGOPT_TARGET);
ok (cfgOptionId('log-level-console') eq CFGOPT_LOG_LEVEL_CONSOLE);

ok (cfgOptionRuleAllowList(CFGCMD_BACKUP, CFGOPT_TYPE));
ok (!cfgOptionRuleAllowList(CFGCMD_BACKUP, CFGOPT_DB_HOST));

ok (cfgOptionRuleAllowListValueTotal(CFGCMD_BACKUP, CFGOPT_TYPE) == 3);

ok (cfgOptionRuleAllowListValue(CFGCMD_BACKUP, CFGOPT_TYPE, 0) eq 'full');
ok (cfgOptionRuleAllowListValue(CFGCMD_BACKUP, CFGOPT_TYPE, 1) eq 'diff');
ok (cfgOptionRuleAllowListValue(CFGCMD_BACKUP, CFGOPT_TYPE, 2) eq 'incr');

ok (cfgOptionRuleAllowListValueValid(CFGCMD_BACKUP, CFGOPT_TYPE, 'diff'));
ok (!cfgOptionRuleAllowListValueValid(CFGCMD_BACKUP, CFGOPT_TYPE, 'bogus'));

ok (cfgOptionRuleAllowRange(CFGCMD_BACKUP, CFGOPT_COMPRESS_LEVEL));
ok (!cfgOptionRuleAllowRange(CFGCMD_BACKUP, CFGOPT_BACKUP_HOST));

ok (cfgOptionRuleAllowRangeMin(CFGCMD_BACKUP, CFGOPT_DB_TIMEOUT) == 0.1);
ok (cfgOptionRuleAllowRangeMin(CFGCMD_BACKUP, CFGOPT_COMPRESS_LEVEL) == 0);
ok (cfgOptionRuleAllowRangeMax(CFGCMD_BACKUP, CFGOPT_COMPRESS_LEVEL) == 9);

ok (cfgOptionRuleDefault(CFGCMD_BACKUP, CFGOPT_COMPRESS_LEVEL) == 6);
ok (!defined(cfgOptionRuleDefault(CFGCMD_BACKUP, CFGOPT_BACKUP_HOST)));

ok (cfgOptionRuleDepend(CFGCMD_RESTORE, CFGOPT_REPO_S3_KEY));
ok (!cfgOptionRuleDepend(CFGCMD_RESTORE, CFGOPT_TYPE));

ok (cfgOptionRuleDependOption(CFGCMD_BACKUP, CFGOPT_DB_USER) == CFGOPT_DB_HOST);

ok (cfgOptionRuleDependValueTotal(CFGCMD_RESTORE, CFGOPT_TARGET) == 3);

ok (cfgOptionRuleDependValue(CFGCMD_RESTORE, CFGOPT_TARGET, 0) eq 'name');
ok (cfgOptionRuleDependValue(CFGCMD_RESTORE, CFGOPT_TARGET, 1) eq 'time');
ok (cfgOptionRuleDependValue(CFGCMD_RESTORE, CFGOPT_TARGET, 2) eq 'xid');

ok (cfgOptionRuleDependValueValid(CFGCMD_RESTORE, CFGOPT_TARGET, 'time'));
ok (!cfgOptionRuleDependValueValid(CFGCMD_RESTORE, CFGOPT_TARGET, 'bogus'));

ok (cfgOptionRuleHint(CFGCMD_BACKUP, CFGOPT_DB1_PATH) eq 'does this stanza exist?');

ok (cfgOptionIndexTotal(CFGOPT_DB_PATH) == 2);
ok (cfgOptionIndexTotal(CFGOPT_REPO_PATH) == 1);

ok (cfgOptionRuleNameAlt(CFGOPT_DB1_HOST) eq 'db-host');
ok (cfgOptionRuleNameAlt(CFGOPT_PROCESS_MAX) eq 'thread-max');
ok (!defined(cfgOptionRuleNameAlt(CFGOPT_TYPE)));

ok (cfgOptionRuleNegate(CFGOPT_ONLINE));
ok (cfgOptionRuleNegate(CFGOPT_COMPRESS));
ok (!cfgOptionRuleNegate(CFGOPT_TYPE));

ok (cfgOptionRulePrefix(CFGOPT_DB_HOST) eq 'db');
ok (cfgOptionRulePrefix(CFGOPT_DB2_HOST) eq 'db');
ok (!defined(cfgOptionRulePrefix(CFGOPT_TYPE)));

ok (cfgOptionRuleRequired(CFGCMD_BACKUP, CFGOPT_CONFIG));
ok (!cfgOptionRuleRequired(CFGCMD_RESTORE, CFGOPT_BACKUP_HOST));

ok (cfgOptionRuleSection(CFGOPT_REPO_S3_KEY) eq 'global');
ok (!defined(cfgOptionRuleSection(CFGOPT_TYPE)));

ok (cfgOptionRuleSecure(CFGOPT_REPO_S3_KEY));
ok (!cfgOptionRuleSecure(CFGOPT_BACKUP_HOST));

ok (cfgOptionRuleType(CFGOPT_TYPE) == CFGOPTRULE_TYPE_STRING);
ok (cfgOptionRuleType(CFGOPT_COMPRESS) == CFGOPTRULE_TYPE_BOOLEAN);

ok (cfgOptionRuleValid(CFGCMD_BACKUP, CFGOPT_TYPE));
ok (!cfgOptionRuleValid(CFGCMD_INFO, CFGOPT_TYPE));

ok (cfgOptionRuleValueHash(CFGOPT_LINK_MAP));
ok (!cfgOptionRuleValueHash(CFGOPT_TYPE));

# Config functions
ok (cfgCommandName(CFGCMD_ARCHIVE_GET) eq 'archive-get');
ok (cfgCommandName(CFGCMD_BACKUP) eq 'backup');

ok (cfgOptionName(CFGOPT_TYPE) eq 'type');
ok (cfgOptionName(CFGOPT_COMPRESS_LEVEL) eq 'compress-level');
