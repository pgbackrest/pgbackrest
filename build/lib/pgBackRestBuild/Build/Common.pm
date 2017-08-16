####################################################################################################################################
# Build Constants and Functions
####################################################################################################################################
package pgBackRestBuild::Build::Common;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Cwd qw(abs_path);
use Exporter qw(import);
    our @EXPORT = qw();
use Storable qw(dclone);

####################################################################################################################################
# Constants
####################################################################################################################################
use constant BLD_PATH                                               => 'path';
    push @EXPORT, qw(BLD_PATH);
use constant BLD_FILE                                               => 'file';
    push @EXPORT, qw(BLD_FILE);

use constant BLD_C                                                  => 'c';
    push @EXPORT, qw(BLD_C);
use constant BLD_HEADER                                             => 'h';
    push @EXPORT, qw(BLD_HEADER);
use constant BLD_MD                                                 => 'md';
    push @EXPORT, qw(BLD_MD);

use constant BLD_SUMMARY                                            => 'summary';
    push @EXPORT, qw(BLD_SUMMARY);

use constant BLD_CONSTANT                                           => 'constant';
    push @EXPORT, qw(BLD_CONSTANT);
use constant BLD_FUNCTION                                           => 'function';
    push @EXPORT, qw(BLD_FUNCTION);

use constant BLD_PARAM                                              => 'param';
    push @EXPORT, qw(BLD_PARAM);
use constant BLD_RETURN_TYPE                                        => 'return-type';
    push @EXPORT, qw(BLD_RETURN_TYPE);
use constant BLD_RETURN_VALUE_MAP                                   => 'return-value-map';
    push @EXPORT, qw(BLD_RETURN_VALUE_MAP);

use constant BLD_TRUTH_DEFAULT                                      => 'truthDefault';
    push @EXPORT, qw(BLD_TRUTH_DEFAULT);

use constant BLD_SOURCE                                             => 'buildSource';
    push @EXPORT, qw(BLD_SOURCE);

1;
