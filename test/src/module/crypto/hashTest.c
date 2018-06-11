/***********************************************************************************************************************************
Test Cryptographic Hashes
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("cryptoHashNew(), cryptoHashProcess*(), cryptoHashStr(), and cryptoHashFree()"))
    {
        CryptoHash *hash = NULL;

        TEST_ERROR(cryptoHashNew(strNew(BOGUS_STR)), AssertError, "unable to load hash 'BOGUS'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(hash, cryptoHashNew(strNew(HASH_TYPE_SHA1)), "create sha1 hash");
        TEST_RESULT_VOID(cryptoHashFree(hash), "    free hash");
        TEST_RESULT_VOID(cryptoHashFree(NULL), "    free null hash");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(hash, cryptoHashNew(strNew(HASH_TYPE_SHA1)), "create sha1 hash");
        TEST_RESULT_STR(strPtr(cryptoHashHex(hash)), "da39a3ee5e6b4b0d3255bfef95601890afd80709", "    check empty hash");
        TEST_RESULT_STR(strPtr(cryptoHashHex(hash)), "da39a3ee5e6b4b0d3255bfef95601890afd80709", "    check empty hash again");
        TEST_RESULT_VOID(cryptoHashFree(hash), "    free hash");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(hash, cryptoHashNew(strNew(HASH_TYPE_SHA1)), "create sha1 hash");
        TEST_RESULT_VOID(cryptoHashProcessC(hash, (const unsigned char *)"1", 1), "    add 1");
        TEST_RESULT_VOID(cryptoHashProcessStr(hash, strNew("2")), "    add 2");
        TEST_RESULT_VOID(cryptoHashProcess(hash, bufNewStr(strNew("3"))), "    add 3");
        TEST_RESULT_VOID(cryptoHashProcess(hash, bufNewStr(strNew("4"))), "    add 4");
        TEST_RESULT_VOID(cryptoHashProcess(hash, bufNewStr(strNew("5"))), "    add 5");

        TEST_RESULT_STR(strPtr(cryptoHashHex(hash)), "8cb2237d0679ca88db6464eac60da96345513964", "    check small hash");
        TEST_RESULT_VOID(cryptoHashFree(hash), "    free hash");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(hash, cryptoHashNew(strNew(HASH_TYPE_MD5)), "create md5 hash");
        TEST_RESULT_STR(strPtr(cryptoHashHex(hash)), "d41d8cd98f00b204e9800998ecf8427e", "    check empty hash");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(hash, cryptoHashNew(strNew(HASH_TYPE_SHA256)), "create sha256 hash");
        TEST_RESULT_STR(
            strPtr(cryptoHashHex(hash)), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
            "    check empty hash");
    }

    // *****************************************************************************************************************************
    if (testBegin("cryptoHashOne*()"))
    {
        TEST_RESULT_STR(
            strPtr(cryptoHashOne(strNew(HASH_TYPE_SHA1), bufNewStr(strNew("12345")))), "8cb2237d0679ca88db6464eac60da96345513964",
            "    check small hash");
        TEST_RESULT_STR(
            strPtr(cryptoHashOneStr(strNew(HASH_TYPE_SHA1), strNew("12345"))), "8cb2237d0679ca88db6464eac60da96345513964",
            "    check small hash");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
