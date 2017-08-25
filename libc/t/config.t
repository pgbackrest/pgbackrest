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

ok (cfgRuleOptionAllowList(CFGCMD_BACKUP, CFGOPT_TYPE));
ok (!cfgRuleOptionAllowList(CFGCMD_BACKUP, CFGOPT_DB_HOST));

ok (cfgRuleOptionAllowListValueTotal(CFGCMD_BACKUP, CFGOPT_TYPE) == 3);

ok (cfgRuleOptionAllowListValue(CFGCMD_BACKUP, CFGOPT_TYPE, 0) eq 'full');
ok (cfgRuleOptionAllowListValue(CFGCMD_BACKUP, CFGOPT_TYPE, 1) eq 'diff');
ok (cfgRuleOptionAllowListValue(CFGCMD_BACKUP, CFGOPT_TYPE, 2) eq 'incr');

ok (cfgRuleOptionAllowListValueValid(CFGCMD_BACKUP, CFGOPT_TYPE, 'diff'));
ok (!cfgRuleOptionAllowListValueValid(CFGCMD_BACKUP, CFGOPT_TYPE, 'bogus'));

ok (cfgRuleOptionAllowRange(CFGCMD_BACKUP, CFGOPT_COMPRESS_LEVEL));
ok (!cfgRuleOptionAllowRange(CFGCMD_BACKUP, CFGOPT_BACKUP_HOST));

ok (cfgRuleOptionAllowRangeMin(CFGCMD_BACKUP, CFGOPT_DB_TIMEOUT) == 0.1);
ok (cfgRuleOptionAllowRangeMin(CFGCMD_BACKUP, CFGOPT_COMPRESS_LEVEL) == 0);
ok (cfgRuleOptionAllowRangeMax(CFGCMD_BACKUP, CFGOPT_COMPRESS_LEVEL) == 9);

ok (cfgRuleOptionDefault(CFGCMD_BACKUP, CFGOPT_COMPRESS_LEVEL) == 6);
ok (!defined(cfgRuleOptionDefault(CFGCMD_BACKUP, CFGOPT_BACKUP_HOST)));

ok (cfgRuleOptionDepend(CFGCMD_RESTORE, CFGOPT_REPO_S3_KEY));
ok (!cfgRuleOptionDepend(CFGCMD_RESTORE, CFGOPT_TYPE));

ok (cfgRuleOptionDependOption(CFGCMD_BACKUP, CFGOPT_DB_USER) == CFGOPT_DB_HOST);

ok (cfgRuleOptionDependValueTotal(CFGCMD_RESTORE, CFGOPT_TARGET) == 3);

ok (cfgRuleOptionDependValue(CFGCMD_RESTORE, CFGOPT_TARGET, 0) eq 'name');
ok (cfgRuleOptionDependValue(CFGCMD_RESTORE, CFGOPT_TARGET, 1) eq 'time');
ok (cfgRuleOptionDependValue(CFGCMD_RESTORE, CFGOPT_TARGET, 2) eq 'xid');

ok (cfgRuleOptionDependValueValid(CFGCMD_RESTORE, CFGOPT_TARGET, 'time'));
ok (!cfgRuleOptionDependValueValid(CFGCMD_RESTORE, CFGOPT_TARGET, 'bogus'));

ok (cfgRuleOptionHint(CFGCMD_BACKUP, CFGOPT_DB1_PATH) eq 'does this stanza exist?');

ok (cfgOptionIndexTotal(CFGOPT_DB_PATH) == 2);
ok (cfgOptionIndexTotal(CFGOPT_REPO_PATH) == 1);

ok (cfgRuleOptionNameAlt(CFGOPT_DB1_HOST) eq 'db-host');
ok (cfgRuleOptionNameAlt(CFGOPT_PROCESS_MAX) eq 'thread-max');
ok (!defined(cfgRuleOptionNameAlt(CFGOPT_TYPE)));

ok (cfgRuleOptionNegate(CFGOPT_ONLINE));
ok (cfgRuleOptionNegate(CFGOPT_COMPRESS));
ok (!cfgRuleOptionNegate(CFGOPT_TYPE));

ok (cfgRuleOptionPrefix(CFGOPT_DB_HOST) eq 'db');
ok (cfgRuleOptionPrefix(CFGOPT_DB2_HOST) eq 'db');
ok (!defined(cfgRuleOptionPrefix(CFGOPT_TYPE)));

ok (cfgRuleOptionRequired(CFGCMD_BACKUP, CFGOPT_CONFIG));
ok (!cfgRuleOptionRequired(CFGCMD_RESTORE, CFGOPT_BACKUP_HOST));

ok (cfgRuleOptionSection(CFGOPT_REPO_S3_KEY) eq 'global');
ok (!defined(cfgRuleOptionSection(CFGOPT_TYPE)));

ok (cfgRuleOptionSecure(CFGOPT_REPO_S3_KEY));
ok (!cfgRuleOptionSecure(CFGOPT_BACKUP_HOST));

ok (cfgRuleOptionType(CFGOPT_TYPE) == CFGOPTDEF_TYPE_STRING);
ok (cfgRuleOptionType(CFGOPT_COMPRESS) == CFGOPTDEF_TYPE_BOOLEAN);

ok (cfgRuleOptionValid(CFGCMD_BACKUP, CFGOPT_TYPE));
ok (!cfgRuleOptionValid(CFGCMD_INFO, CFGOPT_TYPE));

ok (cfgRuleOptionValueHash(CFGOPT_LINK_MAP));
ok (!cfgRuleOptionValueHash(CFGOPT_TYPE));

# Config functions
ok (cfgCommandName(CFGCMD_ARCHIVE_GET) eq 'archive-get');
ok (cfgCommandName(CFGCMD_BACKUP) eq 'backup');

ok (cfgOptionName(CFGOPT_TYPE) eq 'type');
ok (cfgOptionName(CFGOPT_COMPRESS_LEVEL) eq 'compress-level');
