####################################################################################################################################
# Sanity Tests for C Library
#
# Test to ensure the C library loads and is compiled correctly.  Unit and integration tests are performed by test/test.pl.
####################################################################################################################################
use strict;
use warnings;
use Carp;
use English '-no_match_vars';

# Set number of tests
use Test::More tests => 4;

# Make sure the module loads without errors
BEGIN {use_ok('pgBackRest::LibC', qw(:debug :config :configDefine))};

# UVSIZE determines the pointer and long long int size.  This needs to be 8 to indicate 64-bit types are available.
ok (&UVSIZE == 8, 'UVSIZE == 8');

# Check constant that is created dynamically
ok (CFGOPTVAL_BACKUP_TYPE_FULL eq 'full', 'auto constant valid');

# Check constant that is exported from C
ok (CFGDEF_TYPE_HASH >= 0, 'auto constant valid');
