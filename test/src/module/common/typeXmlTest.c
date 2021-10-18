/***********************************************************************************************************************************
Test Xml Types
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("xml*()"))
    {
        TEST_ERROR(xmlDocumentNewBuf(bufNewC(BOGUS_STR, strlen(BOGUS_STR))), FormatError, "invalid xml");

        XmlDocument *xmlDocument = NULL;
        TEST_ASSIGN(
            xmlDocument,
            xmlDocumentNewBuf(
                BUFSTRDEF(
                    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                    "<ListBucketResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">\n"
                    "    <Name id=\"test\">bucket</Name>\n"
                    "    <Prefix/>\n"
                    "    <KeyCount>2</KeyCount>\n"
                    "    <MaxKeys>1000</MaxKeys>\n"
                    "    <IsTruncated>false</IsTruncated>\n"
                    "    <Contents>\n"
                    "        <Key>test1.txt</Key>\n"
                    "        <LastModified>2009-10-12T17:50:30.000Z</LastModified>\n"
                    "        <ETag>&quot;fba9dede5f27731c9771645a39863328&quot;</ETag>\n"
                    "        <Size>1234</Size>\n"
                    "        <StorageClass>STANDARD</StorageClass>\n"
                    "    </Contents>\n"
                    "    <Contents>\n"
                    "        <Key>test2.txt</Key>\n"
                    "        <LastModified>2009-10-14T17:50:30.000Z</LastModified>\n"
                    "        <ETag>&quot;fba9dede5f27731c9771645a39863327&quot;</ETag>\n"
                    "        <Size>4321</Size>\n"
                    "        <StorageClass>STANDARD-IA</StorageClass>\n"
                    "    </Contents>\n"
                    "</ListBucketResult>")),
        "valid xml");

        XmlNode *rootNode = NULL;
        TEST_ASSIGN(rootNode, xmlDocumentRoot(xmlDocument), "get root node");

        XmlNode *nodeMaxKeys = NULL;
        TEST_ASSIGN(nodeMaxKeys, xmlNodeChild(rootNode, STRDEF("MaxKeys"), true), "get max keys");
        TEST_RESULT_STR_Z(xmlNodeContent(nodeMaxKeys), "1000", "    check MaxKeys content");

        TEST_RESULT_STR(xmlNodeContent(NULL), NULL, "    get null content for null node");

        TEST_RESULT_VOID(xmlNodeFree(nodeMaxKeys), "free node");
        TEST_RESULT_VOID(xmlNodeFree(NULL), "free null node");

        TEST_RESULT_UINT(xmlNodeChildTotal(rootNode, STRDEF("Contents")), 2, "Contents child total");
        TEST_RESULT_STR_Z(
            xmlNodeContent(xmlNodeChild(xmlNodeChildN(rootNode, STRDEF("Contents"), 0, true), STRDEF("Key"), true)),
            "test1.txt", "Contents index 0 Key");
        TEST_RESULT_STR_Z(
            xmlNodeContent(xmlNodeChild(xmlNodeChildN(rootNode, STRDEF("Contents"), 1, true), STRDEF("Key"), true)),
            "test2.txt", "Contents index 1 Key");

        XmlNodeList *list = NULL;
        TEST_ASSIGN(list, xmlNodeChildList(rootNode, STRDEF("Contents")), "create node list");
        TEST_RESULT_UINT(xmlNodeLstSize(list), 2, "    check size");
        TEST_RESULT_STR_Z(
            xmlNodeContent(xmlNodeChild(xmlNodeLstGet(list, 0), STRDEF("Key"), true)), "test1.txt",
            "    check Contents index 0 Key");
        TEST_RESULT_STR_Z(
            xmlNodeContent(xmlNodeChild(xmlNodeLstGet(list, 1), STRDEF("Key"), true)), "test2.txt",
            "    check Contents index 1 Key");
        TEST_RESULT_VOID(xmlNodeLstFree(list), "    free list");

        TEST_ERROR(
            xmlNodeChildN(rootNode, STRDEF("Contents"), 2, true), FormatError,
            "unable to find child 'Contents':2 in node 'ListBucketResult'");
        TEST_RESULT_PTR(xmlNodeChildN(rootNode, STRDEF("Contents"), 2, false), NULL, "get missing child without error");

        TEST_RESULT_STR(xmlNodeAttribute(rootNode, STRDEF(BOGUS_STR)), NULL, "attempt to get missing attribute");
        TEST_RESULT_STR_Z(xmlNodeAttribute(xmlNodeChild(rootNode, STRDEF("Name"), true), STRDEF("id")), "test", "get attribute");

        TEST_RESULT_VOID(xmlDocumentFree(xmlDocument), "free xmldoc");

        // Create an empty document, add data to it, and output xml
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(xmlDocument, xmlDocumentNew(STRDEF("CompleteMultipartUpload")), "new xml with root node");

        XmlNode *partNode = NULL;
        TEST_ASSIGN(partNode, xmlNodeAdd(xmlDocumentRoot(xmlDocument), STRDEF("Part")), "create part node 1");
        TEST_RESULT_VOID(xmlNodeContentSet(xmlNodeAdd(partNode, STRDEF("PartNumber")), STRDEF("1")), "set part number 1");
        TEST_RESULT_VOID(xmlNodeContentSet(xmlNodeAdd(partNode, STRDEF("ETag")), STRDEF("E1")), "set etag 1");

        TEST_ASSIGN(partNode, xmlNodeAdd(xmlDocumentRoot(xmlDocument), STRDEF("Part")), "create part node 2");
        TEST_RESULT_VOID(xmlNodeContentSet(xmlNodeAdd(partNode, STRDEF("PartNumber")), STRDEF("2")), "set part number 2");
        TEST_RESULT_VOID(xmlNodeContentSet(xmlNodeAdd(partNode, STRDEF("ETag")), STRDEF("E2")), "set etag 2");

        TEST_RESULT_STR_Z(
            strNewBuf(xmlDocumentBuf(xmlDocument)),
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<CompleteMultipartUpload>"
                "<Part><PartNumber>1</PartNumber><ETag>E1</ETag></Part>"
                "<Part><PartNumber>2</PartNumber><ETag>E2</ETag></Part>"
                "</CompleteMultipartUpload>\n",
            "get xml");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
