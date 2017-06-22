####################################################################################################################################
# S3 Test Environment
####################################################################################################################################
package pgBackRestTest::Env::S3EnvTest;
use parent 'pgBackRestTest::Common::RunTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Storage::S3::Driver;

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Common::VmTest;

####################################################################################################################################
# initS3
####################################################################################################################################
sub initS3
{
    my $self = shift;

    my ($strBucket, $strEndPoint, $strRegion, $strAccessKeyId, $strSecretAccessKey) =
        ('pgbackrest-dev', 's3.amazonaws.com', 'us-east-1', 'accessKey1', 'verySecretKey1');

    my $strS3ServerPath = $self->testPath() . '/s3server';
    my $strS3ServerDataPath = "${strS3ServerPath}/data";
    my $strS3ServerMetaPath = "${strS3ServerPath}/meta";
    my $strS3ServerLogFile = "${strS3ServerPath}/server.log";
    storageTest()->pathCreate($strS3ServerDataPath, {bCreateParent => true});
    storageTest()->pathCreate($strS3ServerMetaPath, {bCreateParent => true});

    $self->{strS3Command} = 'export PYTHONWARNINGS="ignore" && aws s3 --no-verify-ssl';

    # Make sure the cert is visible
    executeTest('sudo chmod o+r,o+x /root /root/scalitys3 && sudo chmod o+r /root/scalitys3/ca.crt');

    executeTest("echo '127.0.0.1 ${strBucket}.${strEndPoint} ${strEndPoint}' | sudo tee -a /etc/hosts");
    executeTest('sudo sed -i "s/logLevel\"\: \"info\"/logLevel\"\: \"trace\"/" /root/scalitys3/config.json');
    executeTest("sudo npm start --prefix /root/scalitys3 > ${strS3ServerLogFile} 2>&1 &");
    executeTest("tail -f ${strS3ServerLogFile} | grep -m 1 \"server started\"");
    executeTest("$self->{strS3Command} mb s3://pgbackrest-dev");

    # Test variables
    my $strFile = 'file.txt';
    my $strFileContent = 'TESTDATA';

    # Initialize the driver
    return new pgBackRest::Storage::S3::Driver(
        $strBucket, $strEndPoint, $strRegion, $strAccessKeyId, $strSecretAccessKey,
        {strCaFile => $self->vm() eq VM_CO7 ? '/root/scalitys3/ca.crt' : undef,
            bVerifySsl => $self->vm() eq VM_U16 ? false : undef, lBufferMax => 1048576});
}

1;
