####################################################################################################################################
# Sanity Tests for C Library
#
# Basic test to ensure the C library loads and has been compiled correctly.
####################################################################################################################################
use strict;
use warnings;
use Carp;
use English '-no_match_vars';

# Set number of tests
use Test::More tests => 2;

# Make sure the module loads without errors
BEGIN {use_ok('pgBackRest::LibC')};

# Load the module dynamically so it does not interfere with the test above
require pgBackRest::LibC;
pgBackRest::LibC->import(qw(:debug));

# UVSIZE determines the pointer and long long int size.  This needs to be 8 to indicate 64-bit types are available.
ok (&UVSIZE == 8, 'UVSIZE == 8');
