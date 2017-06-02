####################################################################################################################################
# COMMON INI MODULE
####################################################################################################################################
package pgBackRest::Common::Ini;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Digest::SHA;
use Exporter qw(import);
    our @EXPORT = qw();
use Fcntl qw(:mode O_WRONLY O_CREAT O_TRUNC);
use File::Basename qw(dirname basename);
use IO::Handle;
use JSON::PP;
use Storable qw(dclone);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Version;

####################################################################################################################################
# Boolean constants
####################################################################################################################################
use constant INI_TRUE                                               => JSON::PP::true;
    push @EXPORT, qw(INI_TRUE);
use constant INI_FALSE                                              => JSON::PP::false;
    push @EXPORT, qw(INI_FALSE);

####################################################################################################################################
# Ini control constants
####################################################################################################################################
use constant INI_SECTION_BACKREST                                   => 'backrest';
    push @EXPORT, qw(INI_SECTION_BACKREST);

use constant INI_KEY_CHECKSUM                                       => 'backrest-checksum';
    push @EXPORT, qw(INI_KEY_CHECKSUM);
use constant INI_KEY_FORMAT                                         => 'backrest-format';
    push @EXPORT, qw(INI_KEY_FORMAT);
use constant INI_KEY_SEQUENCE                                       => 'backrest-sequence';
    push @EXPORT, qw(INI_KEY_SEQUENCE);
use constant INI_KEY_VERSION                                        => 'backrest-version';
    push @EXPORT, qw(INI_KEY_VERSION);

####################################################################################################################################
# Ini file copy extension
####################################################################################################################################
use constant INI_COPY_EXT                                           => '.copy';
    push @EXPORT, qw(INI_COPY_EXT);

####################################################################################################################################
# Ini sort orders
####################################################################################################################################
use constant INI_SORT_FORWARD                                       => 'forward';
    push @EXPORT, qw(INI_SORT_FORWARD);
use constant INI_SORT_REVERSE                                       => 'reverse';
    push @EXPORT, qw(INI_SORT_REVERSE);
use constant INI_SORT_NONE                                          => 'none';
    push @EXPORT, qw(INI_SORT_NONE);

####################################################################################################################################
# new()
####################################################################################################################################
sub new
{
    my $class = shift;                  # Class name

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Load Storage::Helper module
    require pgBackRest::Storage::Helper;
    pgBackRest::Storage::Helper->import();

    # Assign function parameters, defaults, and log debug info
    (
        my $strOperation,
        $self->{strFileName},
        my $bLoad,
        my $strContent,
        $self->{bMainOnly},
        $self->{oStorage},
        $self->{iInitFormat},
        $self->{strInitVersion},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strFileName', trace => true},
            {name => 'bLoad', optional => true, default => true, trace => true},
            {name => 'strContent', optional => true, trace => true},
            {name => 'bMainOnly', optional => true, default => false, trace => true},
            {name => 'oStorage', optional => true, default => storageLocal(), trace => true},
            {name => 'iInitFormat', optional => true, default => BACKREST_FORMAT, trace => true},
            {name => 'strInitVersion', optional => true, default => BACKREST_VERSION, trace => true},
        );

    # Set variables
    $self->{oContent} = {};

    # Set changed to false
    $self->{bModified} = false;

    # Set exists to false
    $self->{bExists} = false;

    # Load the file if requested
    if ($bLoad)
    {
        $self->load();
    }
    # Load from a string if provided
    elsif (defined($strContent))
    {
        $self->{oContent} = iniParse($strContent);
        $self->headerCheck();
    }
    # Else initialize
    else
    {
        $self->{oContent}{&INI_SECTION_BACKREST}{&INI_KEY_SEQUENCE} = 0;
        $self->numericSet(INI_SECTION_BACKREST, INI_KEY_FORMAT, undef, $self->{iInitFormat});
        $self->set(INI_SECTION_BACKREST, INI_KEY_VERSION, undef, $self->{strInitVersion});
    }

    return $self;
}

####################################################################################################################################
# load() - load the ini.
####################################################################################################################################
sub load
{
    my $self = shift;

    # Load the copy if required
    my $rstrContentCopy;

    if (!$self->{bMainOnly})
    {
        $rstrContentCopy = $self->{oStorage}->get(
            $self->{oStorage}->openRead($self->{strFileName} . INI_COPY_EXT, {bIgnoreMissing => true}));
    }

    # Main is always loaded
    my $rstrContent = $self->{oStorage}->get($self->{oStorage}->openRead($self->{strFileName}, {bIgnoreMissing => true}));

    # If both exist then select an authoritative version
    if (defined($rstrContent) && defined($rstrContentCopy))
    {
        my $oContentCopy = iniParse($$rstrContentCopy, {bIgnoreInvalid => true});
        my $oContent = iniParse($$rstrContent, {bIgnoreInvalid => true});

        # If both files parse then select an authoritative version
        if (defined($oContent) && defined($oContentCopy))
        {
            $self->{oContent} = dclone($oContentCopy);
            my $bContentCopyHeader = $self->headerCheck({bIgnoreInvalid => true});
            $self->{oContent} = dclone($oContent);
            my $bContentHeader = $self->headerCheck({bIgnoreInvalid => true});

            # If both headers are valid then select an authoritative version
            if ($bContentHeader && $bContentCopyHeader)
            {
                # If both files have the same checksum then they are identical
                if ($oContent->{&INI_SECTION_BACKREST}{&INI_KEY_CHECKSUM} eq
                    $oContentCopy->{&INI_SECTION_BACKREST}{&INI_KEY_CHECKSUM})
                {
                    $self->{oContent} = $oContent;
                }
                # Else use the sequence to determine the authoritative version
                else
                {
                    # Use main if it has the largest sequence
                    if ($oContent->{&INI_SECTION_BACKREST}{&INI_KEY_SEQUENCE} >
                        $oContentCopy->{&INI_SECTION_BACKREST}{&INI_KEY_SEQUENCE})
                    {
                        $self->{oContent} = $oContent;
                    }
                    # Else use copy if it has the largest sequence
                    elsif ($oContent->{&INI_SECTION_BACKREST}{&INI_KEY_SEQUENCE} <
                           $oContentCopy->{&INI_SECTION_BACKREST}{&INI_KEY_SEQUENCE})
                    {
                        $self->{oContent} = $oContentCopy;
                    }
                    else
                    {
                        confess &log(ERROR,
                            "$self->{strFileName} and $self->{strFileName}" . INI_COPY_EXT .
                            ' have different checksums but the same sequence number, likely due to corruption', ERROR_CONFIG);
                    }
                }
            }
            # If neither header is valid then error on main
            elsif (!$bContentHeader && !$bContentCopyHeader)
            {
                $self->{oContent} = $oContent;
            }
            # Else only one header is valid and must be considered authoritative
            else
            {
                if (!$bContentHeader)
                {
                    $self->{oContent} = $oContentCopy;
                }
            }
        }
        # If neither parses then error on main
        elsif (!defined($oContent) && !defined($oContentCopy))
        {
            iniParse($$rstrContent);
        }
        # Else only one parses and must considered authoritative as long as the header is valid
        else
        {
            if (defined($oContent))
            {
                $self->{oContent} = $oContent;
            }
            else
            {
                $self->{oContent} = $oContentCopy;
            }
        }
    }
    # If neither exists then error
    elsif (!defined($rstrContent) && !defined($rstrContentCopy))
    {
        confess &log(ERROR, "unable to open $self->{strFileName} or $self->{strFileName}" . INI_COPY_EXT, ERROR_FILE_MISSING);
    }
    # Else only one exists and must considered authoritative as long as it parses and has a valid header
    else
    {
        if (defined($rstrContent))
        {
            $self->{oContent} = iniParse($$rstrContent);
        }
        else
        {
            $self->{oContent} = iniParse($$rstrContentCopy);
        }
    }

    $self->headerCheck();
    $self->{bExists} = true;
}

####################################################################################################################################
# headerCheck() - check that version and checksum in header are as expected
####################################################################################################################################
sub headerCheck
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $bIgnoreInvalid,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->headerCheck', \@_,
            {name => 'bIgnoreInvalid', optional => true, default => false, trace => true},
        );

    # Eval so exceptions can be ignored on bIgnoreInvalid
    my $bValid = true;

    eval
    {
        # Make sure that the sequence is an integer >= 1
        my $iSequence = $self->get(INI_SECTION_BACKREST, INI_KEY_SEQUENCE, undef, false);

        eval
        {
            # Error if not defined
            if (!defined($iSequence))
            {
                return false;
            }

            # Error if not a number
            $iSequence += 0;

            # Error if < 1 or not an integer
            if ($iSequence < 1 || $iSequence != int($iSequence))
            {
                return false;
            }

            return true;
        }
        or do
        {
            confess &log(ERROR,
                "invalid sequence in '$self->{strFileName}', expected integer >= 1 but found " .
                    (defined($iSequence) ? "'${iSequence}'" : '[undef]'),
                ERROR_CHECKSUM);
        };

        # Make sure the ini is valid by testing checksum
        my $strChecksum = $self->get(INI_SECTION_BACKREST, INI_KEY_CHECKSUM, undef, false);
        my $strTestChecksum = $self->hash();

        if (!defined($strChecksum) || $strChecksum ne $strTestChecksum)
        {
            confess &log(ERROR,
                "invalid checksum in '$self->{strFileName}', expected '${strTestChecksum}' but found " .
                    (defined($strChecksum) ? "'${strChecksum}'" : '[undef]'),
                ERROR_CHECKSUM);
        }

        # Make sure that the format is current, otherwise error
        my $iFormat = $self->get(INI_SECTION_BACKREST, INI_KEY_FORMAT, undef, false, 0);

        if ($iFormat != $self->{iInitFormat})
        {
            confess &log(ERROR,
                "invalid format in '$self->{strFileName}', expected $self->{iInitFormat} but found ${iFormat}", ERROR_FORMAT);
        }

        # Check if the version has changed
        if (!$self->test(INI_SECTION_BACKREST, INI_KEY_VERSION, undef, $self->{strInitVersion}))
        {
            $self->set(INI_SECTION_BACKREST, INI_KEY_VERSION, undef, $self->{strInitVersion});
        }

        return true;
    }
    or do
    {
        # Confess the error if it should not be ignored
        if (!$bIgnoreInvalid)
        {
            confess $EVAL_ERROR;
        }

        # Return false when errors are ignored
        $bValid = false;
    };

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bValid', value => $bValid, trace => true}
    );
}

####################################################################################################################################
# iniParse() - parse from standard INI format to a hash.
####################################################################################################################################
push @EXPORT, qw(iniParse);

sub iniParse
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strContent,
        $bRelaxed,
        $bIgnoreInvalid,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::iniParse', \@_,
            {name => 'strContent', required => false, trace => true},
            {name => 'bRelaxed', optional => true, default => false, trace => true},
            {name => 'bIgnoreInvalid', optional => true, default => false, trace => true},
        );

    # Ini content
    my $oContent = undef;
    my $strSection;

    # Create the JSON object
    my $oJSON = JSON::PP->new()->allow_nonref();

    # Eval so exceptions can be ignored on bIgnoreInvalid
    eval
    {
        # Read the INI file
        foreach my $strLine (split("\n", defined($strContent) ? $strContent : ''))
        {
            $strLine = trim($strLine);

            # Skip lines that are blank or comments
            if ($strLine ne '' && $strLine !~ '^[ ]*#.*')
            {
                # Get the section
                if (index($strLine, '[') == 0)
                {
                    $strSection = substr($strLine, 1, length($strLine) - 2);
                }
                else
                {
                    if (!defined($strSection))
                    {
                        confess &log(ERROR, "key/value pair '${strLine}' found outside of a section", ERROR_CONFIG);
                    }

                    # Get key and value
                    my $iIndex = index($strLine, '=');

                    if ($iIndex == -1)
                    {
                        confess &log(ERROR, "unable to find '=' in '${strLine}'", ERROR_CONFIG);
                    }

                    my $strKey = substr($strLine, 0, $iIndex);
                    my $strValue = substr($strLine, $iIndex + 1);

                    # If relaxed then read the value directly
                    if ($bRelaxed)
                    {
                        if (defined($oContent->{$strSection}{$strKey}))
                        {
                            if (ref($oContent->{$strSection}{$strKey}) ne 'ARRAY')
                            {
                                $oContent->{$strSection}{$strKey} = [$oContent->{$strSection}{$strKey}];
                            }

                            push(@{$oContent->{$strSection}{$strKey}}, $strValue);
                        }
                        else
                        {
                            $oContent->{$strSection}{$strKey} = $strValue;
                        }
                    }
                    # Else read the value as stricter JSON
                    else
                    {
                        ${$oContent}{$strSection}{$strKey} = $oJSON->decode($strValue);
                    }
                }
            }
        }

        # Error if the file is empty
        if (!($bRelaxed || defined($oContent)))
        {
            confess &log(ERROR, 'no key/value pairs found', ERROR_CONFIG);
        }

        return true;
    }
    or do
    {
        # Confess the error if it should not be ignored
        if (!$bIgnoreInvalid)
        {
            confess $EVAL_ERROR;
        }

        # Undef content when errors are ignored
        undef($oContent);
    };

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oContent', value => $oContent, trace => true}
    );
}

####################################################################################################################################
# save() - save the file.
####################################################################################################################################
sub save
{
    my $self = shift;

    # Save only if modified
    if ($self->{bModified})
    {
        # Calculate the hash
        $self->hash();

        # Save the file
        $self->{oStorage}->put($self->{strFileName}, iniRender($self->{oContent}));
        $self->{oStorage}->put($self->{strFileName} . INI_COPY_EXT, iniRender($self->{oContent}));
        $self->{bModified} = false;

        # Indicate the file now exists
        $self->{bExists} = true;

        # File was saved
        return true;
    }

    # File was not saved
    return false;
}

####################################################################################################################################
# saveCopy - save only a copy of the file.
####################################################################################################################################
sub saveCopy
{
    my $self = shift;

    if ($self->{oStorage}->exists($self->{strFileName}))
    {
        confess &log(ASSERT, "cannot save copy only when '$self->{strFileName}' exists");
    }

    $self->hash();
    $self->{oStorage}->put($self->{strFileName} . INI_COPY_EXT, iniRender($self->{oContent}));
}

####################################################################################################################################
# iniRender() - render hash to standard INI format.
####################################################################################################################################
push @EXPORT, qw(iniRender);

sub iniRender
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oContent,
        $bRelaxed,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::iniRender', \@_,
            {name => 'oContent', trace => true},
            {name => 'bRelaxed', default => false, trace => true},
        );

    # Open the ini file for writing
    my $strContent = '';
    my $bFirst = true;

    # Create the JSON object canonical so that fields are alpha ordered to pass unit tests
    my $oJSON = JSON::PP->new()->canonical()->allow_nonref();

    # Write the INI file
    foreach my $strSection (sort(keys(%$oContent)))
    {
        # Add a linefeed between sections
        if (!$bFirst)
        {
            $strContent .= "\n";
        }

        # Write the section
        $strContent .= "[${strSection}]\n";

        # Iterate through all keys in the section
        foreach my $strKey (sort(keys(%{$oContent->{$strSection}})))
        {
            # If the value is a hash then convert it to JSON, otherwise store as is
            my $strValue = ${$oContent}{$strSection}{$strKey};

            # If relaxed then store as old-style config
            if ($bRelaxed)
            {
                # If the value is an array then save each element to a separate key/value pair
                if (ref($strValue) eq 'ARRAY')
                {
                    foreach my $strArrayValue (@{$strValue})
                    {
                        $strContent .= "${strKey}=${strArrayValue}\n";
                    }
                }
                # Else write a standard key/value pair
                else
                {
                    $strContent .= "${strKey}=${strValue}\n";
                }
            }
            # Else write as stricter JSON
            else
            {
                $strContent .= "${strKey}=" . $oJSON->encode($strValue) . "\n";
            }
        }

        $bFirst = false;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strContent', value => $strContent, trace => true}
    );
}

####################################################################################################################################
# hash() - generate hash for the manifest.
####################################################################################################################################
sub hash
{
    my $self = shift;

    # Remove the old checksum and save the sequence
    my $iSequence = $self->{oContent}{&INI_SECTION_BACKREST}{&INI_KEY_SEQUENCE};
    delete($self->{oContent}{&INI_SECTION_BACKREST}{&INI_KEY_CHECKSUM});
    delete($self->{oContent}{&INI_SECTION_BACKREST}{&INI_KEY_SEQUENCE});

    my $oSHA = Digest::SHA->new('sha1');
    my $oJSON = JSON::PP->new()->canonical()->allow_nonref();
    $oSHA->add($oJSON->encode($self->{oContent}));

    # Set the new checksum
    $self->{oContent}{&INI_SECTION_BACKREST}{&INI_KEY_CHECKSUM} = $oSHA->hexdigest();
    $self->{oContent}{&INI_SECTION_BACKREST}{&INI_KEY_SEQUENCE} = $iSequence + 0;

    return $self->{oContent}{&INI_SECTION_BACKREST}{&INI_KEY_CHECKSUM};
}

####################################################################################################################################
# get() - get a value.
####################################################################################################################################
sub get
{
    my $self = shift;
    my $strSection = shift;
    my $strKey = shift;
    my $strSubKey = shift;
    my $bRequired = shift;
    my $oDefault = shift;

    # Parameter constraints
    if (!defined($strSection))
    {
        confess &log(ASSERT, 'strSection is required');
    }

    if (defined($strSubKey) && !defined($strKey))
    {
        confess &log(ASSERT, "strKey is required when strSubKey '${strSubKey}' is requested");
    }

    # Get the result
    my $oResult = $self->{oContent}->{$strSection};

    if (defined($strKey) && defined($oResult))
    {
        $oResult = $oResult->{$strKey};

        if (defined($strSubKey) && defined($oResult))
        {
            $oResult = $oResult->{$strSubKey};
        }
    }

    # When result is not defined
    if (!defined($oResult))
    {
        # Error if a result is required
        if (!defined($bRequired) || $bRequired)
        {
            confess &log(ASSERT, "strSection '$strSection'" . (defined($strKey) ? ", strKey '$strKey'" : '') .
                                  (defined($strSubKey) ? ", strSubKey '$strSubKey'" : '') . ' is required but not defined');
        }

        # Return default if specified
        if (defined($oDefault))
        {
            return $oDefault;
        }
    }

    return $oResult
}

####################################################################################################################################
# boolGet() - get a boolean value.
####################################################################################################################################
sub boolGet
{
    my $self = shift;
    my $strSection = shift;
    my $strKey = shift;
    my $strSubKey = shift;
    my $bRequired = shift;
    my $bDefault = shift;

    return $self->get(
        $strSection, $strKey, $strSubKey, $bRequired,
        defined($bDefault) ? ($bDefault ? INI_TRUE : INI_FALSE) : undef) ? true : false;
}

####################################################################################################################################
# numericGet() - get a numeric value.
####################################################################################################################################
sub numericGet
{
    my $self = shift;
    my $strSection = shift;
    my $strKey = shift;
    my $strSubKey = shift;
    my $bRequired = shift;
    my $nDefault = shift;

    return $self->get($strSection, $strKey, $strSubKey, $bRequired, defined($nDefault) ? $nDefault + 0 : undef) + 0;
}

####################################################################################################################################
# set - set a value.
####################################################################################################################################
sub set
{
    my $self = shift;
    my $strSection = shift;
    my $strKey = shift;
    my $strSubKey = shift;
    my $oValue = shift;

    # Parameter constraints
    if (!(defined($strSection) && defined($strKey)))
    {
        confess &log(ASSERT, 'strSection and strKey are required');
    }

    my $oCurrentValue;

    if (defined($strSubKey))
    {
        $oCurrentValue = \$self->{oContent}{$strSection}{$strKey}{$strSubKey};
    }
    else
    {
        $oCurrentValue = \$self->{oContent}{$strSection}{$strKey};
    }

    if (!defined($$oCurrentValue) ||
        defined($oCurrentValue) != defined($oValue) ||
        ${dclone($oCurrentValue)} ne ${dclone(\$oValue)})
    {
        $$oCurrentValue = $oValue;

        if (!$self->{bModified})
        {
            $self->{bModified} = true;
            $self->{oContent}{&INI_SECTION_BACKREST}{&INI_KEY_SEQUENCE}++;
        }

        return true;
    }

    return false;
}

####################################################################################################################################
# boolSet - set a boolean value.
####################################################################################################################################
sub boolSet
{
    my $self = shift;
    my $strSection = shift;
    my $strKey = shift;
    my $strSubKey = shift;
    my $bValue = shift;

    $self->set($strSection, $strKey, $strSubKey, $bValue ? INI_TRUE : INI_FALSE);
}

####################################################################################################################################
# numericSet - set a numeric value.
####################################################################################################################################
sub numericSet
{
    my $self = shift;
    my $strSection = shift;
    my $strKey = shift;
    my $strSubKey = shift;
    my $nValue = shift;

    $self->set($strSection, $strKey, $strSubKey, defined($nValue) ? $nValue + 0 : undef);
}

####################################################################################################################################
# remove - remove a value.
####################################################################################################################################
sub remove
{
    my $self = shift;
    my $strSection = shift;
    my $strKey = shift;
    my $strSubKey = shift;

    # Test if the value exists
    if ($self->test($strSection, $strKey, $strSubKey))
    {
        # Remove a subkey
        if (defined($strSubKey))
        {
            delete($self->{oContent}{$strSection}{$strKey}{$strSubKey});
        }

        # Remove a key
        if (defined($strKey))
        {
            if (!defined($strSubKey))
            {
                delete($self->{oContent}{$strSection}{$strKey});
            }

            # Remove the section if it is now empty
            if (keys(%{$self->{oContent}{$strSection}}) == 0)
            {
                delete($self->{oContent}{$strSection});
            }
        }

        # Remove a section
        if (!defined($strKey))
        {
            delete($self->{oContent}{$strSection});
        }

        # Record changes
        if (!$self->{bModified})
        {
            $self->{bModified} = true;
            $self->{oContent}{&INI_SECTION_BACKREST}{&INI_KEY_SEQUENCE}++;
        }

        return true;
    }

    return false;
}

####################################################################################################################################
# keys - get the list of keys in a section.
####################################################################################################################################
sub keys
{
    my $self = shift;
    my $strSection = shift;
    my $strSortOrder = shift;

    if ($self->test($strSection))
    {
        if (!defined($strSortOrder) || $strSortOrder eq INI_SORT_FORWARD)
        {
            return (sort(keys(%{$self->get($strSection)})));
        }
        elsif ($strSortOrder eq INI_SORT_REVERSE)
        {
            return (sort {$b cmp $a} (keys(%{$self->get($strSection)})));
        }
        elsif ($strSortOrder eq INI_SORT_NONE)
        {
            return (keys(%{$self->get($strSection)}));
        }
        else
        {
            confess &log(ASSERT, "invalid strSortOrder '${strSortOrder}'");
        }
    }

    my @stryEmptyArray;
    return @stryEmptyArray;
}

####################################################################################################################################
# test - test a value.
#
# Test a value to see if it equals the supplied test value.  If no test value is given, tests that the section, key, or subkey
# is defined.
####################################################################################################################################
sub test
{
    my $self = shift;
    my $strSection = shift;
    my $strValue = shift;
    my $strSubValue = shift;
    my $strTest = shift;

    # Get the value
    my $strResult = $self->get($strSection, $strValue, $strSubValue, false);

    # Is there a result
    if (defined($strResult))
    {
        # Is there a value to test against?
        if (defined($strTest))
        {
            return $strResult eq $strTest ? true : false;
        }

        return true;
    }

    return false;
}

####################################################################################################################################
# boolTest - test a boolean value, see test().
####################################################################################################################################
sub boolTest
{
    my $self = shift;
    my $strSection = shift;
    my $strValue = shift;
    my $strSubValue = shift;
    my $bTest = shift;

    return $self->test($strSection, $strValue, $strSubValue, defined($bTest) ? ($bTest ? INI_TRUE : INI_FALSE) : undef);
}

####################################################################################################################################
# Properties.
####################################################################################################################################
sub modified {shift->{bModified}}                                   # Has the data been modified since last load/save?
sub exists {shift->{bExists}}                                       # Is the data persisted to file?

1;
