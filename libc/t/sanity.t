####################################################################################################################################
# Sanity Tests for C Library
#
# Test to ensure the C library loads and is compiled correctly.  Unit and integration tests are performed by test/test.pl.
####################################################################################################################################
use strict;
use warnings;
use Carp;
use English '-no_match_vars';

use Cwd qw(abs_path);
use File::Basename qw(dirname);

# Set number of tests
use Test::More tests => 5;

use lib abs_path(dirname($0) . '/../../lib');

# Make sure the module loads without errors
BEGIN {use_ok('pgBackRest::LibC', qw(:debug :config :configDefine))};

require XSLoader;
XSLoader::load('pgBackRest::LibC', '999');

# UVSIZE determines the pointer and long long int size.  This needs to be 8 to indicate 64-bit types are available.
ok (libcUvSize() == 8, 'UVSIZE == 8');

# Check constant that is created dynamically
ok (CFGOPTVAL_BACKUP_TYPE_FULL eq 'full', 'auto constant valid');

# Check constant that is exported from C
ok (CFGDEF_TYPE_HASH >= 0, 'auto constant valid');

# Check a function
ok (cfgOptionName(CFGOPT_DELTA) eq 'delta', 'auto constant valid');
