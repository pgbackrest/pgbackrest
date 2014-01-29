####################################################################################################################################
# FILE_HASH_GET - get the sha1 hash for a file
####################################################################################################################################
sub file_hash_get
{
    my $strPathType = shift;
    my $strFile = shift;
    
    if (!defined($strCommandChecksum))
    {
        confess &log(ASSERT, "\$strCommandChecksum not defined");
    }
    
    my $strPath = path_get($strPathType, $strFile);
    my $strCommand;
    
    if (-e $strPath)
    {
        $strCommand = $strCommandChecksum;
        $strCommand =~ s/\%file\%/$strFile/g;
    }
    elsif (-e $strPath . ".gz")
    {
        $strCommand = $strCommandDecompress;
        $strCommand =~ s/\%file\%/${strPath}/g;
        $strCommand .= " | " . $strCommandChecksum;
        $strCommand =~ s/\%file\%//g;
    }
    else
    {
        confess &log(ASSERT, "unable to find $strPath(.gz) for checksum");
    }
    
    return trim(capture($strCommand)) or confess &log(ERROR, "unable to checksum ${strPath}");
}

1;