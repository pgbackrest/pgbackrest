####################################################################################################################################
# COMMON EXCEPTION MODULE
####################################################################################################################################
package BackRest::Common::Exception;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess longmess);

use Exporter qw(import);
    our @EXPORT = qw();

####################################################################################################################################
# Exception codes
####################################################################################################################################
use constant ERROR_ASSERT                                           => 100;
    push @EXPORT, qw(ERROR_ASSERT);
use constant ERROR_CHECKSUM                                         => 101;
    push @EXPORT, qw(ERROR_CHECKSUM);
use constant ERROR_CONFIG                                           => 102;
    push @EXPORT, qw(ERROR_CONFIG);
use constant ERROR_FILE_INVALID                                     => 103;
    push @EXPORT, qw(ERROR_FILE_INVALID);
use constant ERROR_FORMAT                                           => 104;
    push @EXPORT, qw(ERROR_FORMAT);
use constant ERROR_COMMAND_REQUIRED                                 => 105;
    push @EXPORT, qw(ERROR_COMMAND_REQUIRED);
use constant ERROR_OPTION_INVALID                                   => 106;
    push @EXPORT, qw(ERROR_OPTION_INVALID);
use constant ERROR_OPTION_INVALID_VALUE                             => 107;
    push @EXPORT, qw(ERROR_OPTION_INVALID_VALUE);
use constant ERROR_OPTION_INVALID_RANGE                             => 108;
    push @EXPORT, qw(ERROR_OPTION_INVALID_RANGE);
use constant ERROR_OPTION_INVALID_PAIR                              => 109;
    push @EXPORT, qw(ERROR_OPTION_INVALID_PAIR);
use constant ERROR_OPTION_DUPLICATE_KEY                             => 110;
    push @EXPORT, qw(ERROR_OPTION_DUPLICATE_KEY);
use constant ERROR_OPTION_NEGATE                                    => 111;
    push @EXPORT, qw(ERROR_OPTION_NEGATE);
use constant ERROR_OPTION_REQUIRED                                  => 112;
    push @EXPORT, qw(ERROR_OPTION_REQUIRED);
use constant ERROR_POSTMASTER_RUNNING                               => 113;
    push @EXPORT, qw(ERROR_POSTMASTER_RUNNING);
use constant ERROR_PROTOCOL                                         => 114;
    push @EXPORT, qw(ERROR_PROTOCOL);
use constant ERROR_RESTORE_PATH_NOT_EMPTY                           => 115;
    push @EXPORT, qw(ERROR_RESTORE_PATH_NOT_EMPTY);
use constant ERROR_FILE_OPEN                                        => 116;
    push @EXPORT, qw(ERROR_FILE_OPEN);
use constant ERROR_FILE_READ                                        => 117;
    push @EXPORT, qw(ERROR_FILE_READ);
use constant ERROR_PARAM_REQUIRED                                   => 118;
    push @EXPORT, qw(ERROR_PARAM_REQUIRED);
use constant ERROR_ARCHIVE_MISMATCH                                 => 119;
    push @EXPORT, qw(ERROR_ARCHIVE_MISMATCH);
use constant ERROR_ARCHIVE_DUPLICATE                                => 120;
    push @EXPORT, qw(ERROR_ARCHIVE_DUPLICATE);
use constant ERROR_VERSION_NOT_SUPPORTED                            => 121;
    push @EXPORT, qw(ERROR_VERSION_NOT_SUPPORTED);
use constant ERROR_PATH_CREATE                                      => 122;
    push @EXPORT, qw(ERROR_PATH_CREATE);
use constant ERROR_COMMAND_INVALID                                  => 123;
    push @EXPORT, qw(ERROR_COMMAND_INVALID);
use constant ERROR_HOST_CONNECT                                     => 124;
    push @EXPORT, qw(ERROR_HOST_CONNECT);
use constant ERROR_LOCK_ACQUIRE                                     => 125;
    push @EXPORT, qw(ERROR_LOCK_ACQUIRE);
use constant ERROR_BACKUP_MISMATCH                                  => 126;
    push @EXPORT, qw(ERROR_BACKUP_MISMATCH);
use constant ERROR_FILE_SYNC                                        => 127;
    push @EXPORT, qw(ERROR_FILE_SYNC);
use constant ERROR_PATH_OPEN                                        => 128;
    push @EXPORT, qw(ERROR_PATH_OPEN);
use constant ERROR_PATH_SYNC                                        => 129;
    push @EXPORT, qw(ERROR_PATH_SYNC);
use constant ERROR_FILE_MISSING                                     => 130;
    push @EXPORT, qw(ERROR_FILE_MISSING);
use constant ERROR_DB_CONNECT                                       => 131;
    push @EXPORT, qw(ERROR_DB_CONNECT);
use constant ERROR_DB_QUERY                                         => 132;
    push @EXPORT, qw(ERROR_DB_QUERY);
use constant ERROR_DB_MISMATCH                                      => 133;
    push @EXPORT, qw(ERROR_DB_MISMATCH);
use constant ERROR_DB_TIMEOUT                                       => 134;
    push @EXPORT, qw(ERROR_DB_TIMEOUT);
use constant ERROR_FILE_REMOVE                                      => 135;
    push @EXPORT, qw(ERROR_FILE_REMOVE);
use constant ERROR_PATH_REMOVE                                      => 136;
    push @EXPORT, qw(ERROR_PATH_REMOVE);

use constant ERROR_UNKNOWN                                          => 199;
    push @EXPORT, qw(ERROR_UNKNOWN);

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;       # Class name
    my $iCode = shift;       # Error code
    my $strMessage = shift;  # ErrorMessage
    my $strTrace = shift;    # Stack trace

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Initialize exception
    $self->{iCode} = $iCode;
    $self->{strMessage} = $strMessage;
    $self->{strTrace} = $strTrace;

    return $self;
}

####################################################################################################################################
# CODE
####################################################################################################################################
sub code
{
    my $self = shift;

    return $self->{iCode};
}

####################################################################################################################################
# MESSAGE
####################################################################################################################################
sub message
{
    my $self = shift;

    return $self->{strMessage};
}

####################################################################################################################################
# TRACE
####################################################################################################################################
sub trace
{
    my $self = shift;

    return $self->{strTrace};
}

1;
