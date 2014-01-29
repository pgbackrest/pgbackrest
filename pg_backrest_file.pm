package BrFile;

use strict;
use Exporter;
use vars qw($VERSION @EXPORT);

$VERSION     = 1.00;
@EXPORT      = qw(func1);

sub func1  { return reverse @_  }
sub func2  { return map{ uc }@_ }

1;