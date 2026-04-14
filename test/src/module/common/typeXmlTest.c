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
        TEST_ERROR(
            xmlDocumentNewBuf(bufNewC(BOGUS_STR, strlen(BOGUS_STR))), FormatError, "invalid xml in 5 byte(s):\nBOGUS");

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
        TEST_RESULT_STR_Z(xmlNodeName(rootNode), "ListBucketResult", "root node name");

        XmlNode *nodeMaxKeys = NULL;
        TEST_ASSIGN(nodeMaxKeys, xmlNodeChild(rootNode, STRDEF("MaxKeys"), true), "get max keys");
        TEST_RESULT_STR_Z(xmlNodeContent(nodeMaxKeys), "1000", "    check MaxKeys content");

        TEST_RESULT_STR(xmlNodeContent(NULL), NULL, "    get null content for null node");

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

        StringList *nameList = strLstNew();
        strLstAddZ(nameList, "IsTruncated");
        strLstAddZ(nameList, "Contents");

        TEST_ASSIGN(list, xmlNodeChildListMulti(rootNode, nameList), "create node multi list");
        TEST_RESULT_STR_Z(
            xmlNodeContent(xmlNodeLstGet(list, 0)), "false", "check IsTruncated index 0 Contents");
        TEST_RESULT_STR_Z(
            xmlNodeContent(xmlNodeChild(xmlNodeLstGet(list, 1), STRDEF("Key"), true)), "test1.txt", "check Contents index 1 Key");
        TEST_RESULT_STR_Z(
            xmlNodeContent(xmlNodeChild(xmlNodeLstGet(list, 2), STRDEF("Key"), true)), "test2.txt", "check Contents index 2 Key");

        TEST_ASSIGN(list, xmlNodeChildListAll(rootNode, false), "create node all list");
        TEST_RESULT_STR_Z(xmlNodeName(xmlNodeLstGet(list, 0)), "Name", "check node 0 name");
        TEST_RESULT_STR_Z(xmlNodeName(xmlNodeLstGet(list, 1)), "Prefix", "check node 1 name");

        TEST_ERROR(
            xmlNodeChildN(rootNode, STRDEF("Contents"), 2, true), FormatError,
            "unable to find child 'Contents':2 in node 'ListBucketResult'");
        TEST_RESULT_PTR(xmlNodeChildN(rootNode, STRDEF("Contents"), 2, false), NULL, "get missing child without error");

        TEST_RESULT_STR(xmlNodeAttribute(rootNode, STRDEF(BOGUS_STR)), NULL, "attempt to get missing attribute");
        TEST_RESULT_STR_Z(xmlNodeAttribute(xmlNodeChild(rootNode, STRDEF("Name"), true), STRDEF("id")), "test", "get attribute");

        TEST_RESULT_VOID(xmlDocumentFree(xmlDocument), "free xmldoc");

        // Create an empty document, add data to it, and output xml
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(xmlDocument, xmlDocumentNewP(STRDEF("CompleteMultipartUpload")), "new xml with root node");

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

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("xmlNodeAttributeList()");

        xmlDocument = xmlDocumentNewBuf(
            BUFSTRDEF(
                "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                "<root>\n"
                "    <node id=\"n1\" class=\"test\" title=\"hello\">original</node>\n"
                "    <child id=\"c1\">text1</child>\n"
                "    <child id=\"c2\">text2</child>\n"
                "    <empty/>\n"
                "</root>"));

        XmlNode *const node = xmlNodeChild(xmlDocumentRoot(xmlDocument), STRDEF("node"), true);
        StringList *const attrList = xmlNodeAttributeList(node);
        TEST_RESULT_UINT(strLstSize(attrList), 3, "three attributes");
        TEST_RESULT_STR_Z(strLstGet(attrList, 0), "id", "first attribute");
        TEST_RESULT_STR_Z(strLstGet(attrList, 1), "class", "second attribute");
        TEST_RESULT_STR_Z(strLstGet(attrList, 2), "title", "third attribute");

        XmlNode *const emptyNode = xmlNodeChild(xmlDocumentRoot(xmlDocument), STRDEF("empty"), true);
        TEST_RESULT_UINT(strLstSize(xmlNodeAttributeList(emptyNode)), 0, "no attributes");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("xmlNodeAttributeRemove()");

        TEST_RESULT_STR_Z(xmlNodeAttribute(node, STRDEF("class")), "test", "class exists");
        xmlNodeAttributeRemove(node, STRDEF("class"));
        TEST_RESULT_STR(xmlNodeAttribute(node, STRDEF("class")), NULL, "class removed");
        TEST_RESULT_STR_Z(xmlNodeAttribute(node, STRDEF("id")), "n1", "id unchanged");
        TEST_RESULT_STR_Z(xmlNodeAttribute(node, STRDEF("title")), "hello", "title unchanged");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("xmlNodeTextSet()");

        XmlNode *const textNode = xmlNodeLstGet(xmlNodeChildListAll(node, true), 0);
        TEST_RESULT_STR_Z(xmlNodeContent(textNode), "original", "original content");
        TEST_RESULT_VOID(xmlNodeTextSet(textNode, STRDEF("replaced")), "set text content");
        TEST_RESULT_STR_Z(xmlNodeContent(textNode), "replaced", "replaced content");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("xmlNodeDup()");

        XmlNode *const original = xmlNodeChild(xmlDocumentRoot(xmlDocument), STRDEF("child"), true);
        XmlNode *const dup = xmlNodeDup(original);
        TEST_RESULT_STR_Z(xmlNodeAttribute(dup, STRDEF("id")), "c1", "dup has attribute");
        TEST_RESULT_STR_Z(xmlNodeContent(dup), "text1", "dup has content");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("xmlNodeRemove()");

        list = xmlNodeChildList(xmlDocumentRoot(xmlDocument), STRDEF("child"));
        TEST_RESULT_UINT(xmlNodeLstSize(list), 2, "two children before remove");

        xmlNodeRemove(xmlNodeChildN(xmlDocumentRoot(xmlDocument), STRDEF("child"), 0, true));

        list = xmlNodeChildList(xmlDocumentRoot(xmlDocument), STRDEF("child"));
        TEST_RESULT_UINT(xmlNodeLstSize(list), 1, "one child after remove");
        TEST_RESULT_STR_Z(xmlNodeAttribute(xmlNodeLstGet(list, 0), STRDEF("id")), "c2", "remaining child is c2");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("copy xml between documents");

        XmlDocument *xmlDocument2 = NULL;

        TEST_ASSIGN(
            xmlDocument, xmlDocumentNewP(STRDEF("doc1"), .dtdName = STRDEF("doc"), .dtdFile = STRDEF("doc.dtd")),
            "new xml with dtd");
        TEST_ASSIGN(
            xmlDocument2,
            xmlDocumentNewBuf(
                BUFSTRDEF(
                    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                    "<doc2>\n"
                    "    <!-- comment -->\n"
                    "    text55\n"
                    "    <name id=\"id55\">name55</name>\n"
                    "</doc2>")),
            "valid xml");

        TEST_RESULT_VOID(xmlNodeChildAdd(xmlDocumentRoot(xmlDocument), xmlDocumentRoot(xmlDocument2)), "copy xml");
        TEST_RESULT_STR_Z(
            strNewBuf(xmlDocumentBuf(xmlDocument)),
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE doc SYSTEM \"doc.dtd\">\n"
            "<doc1>\n"
            "    \n"
            "    text55\n"
            "    <name id=\"id55\">name55</name>\n"
            "</doc1>\n",
            "get xml");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("insert xml between documents");

        TEST_ASSIGN(
            xmlDocument,
            xmlDocumentNewBuf(
                BUFSTRDEF(
                    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                    "<doc>\n"
                    "    <replace/>\n"
                    "</doc>")),
            "destination xml");
        TEST_ASSIGN(
            xmlDocument2,
            xmlDocumentNewBuf(
                BUFSTRDEF(
                    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                    "<doc2>\n"
                    "    <!-- comment -->\n"
                    "    text55\n"
                    "    <name id=\"id55\">name55</name>\n"
                    "</doc2>")),
            "source xml");

        TEST_RESULT_VOID(
            xmlNodeChildReplace(xmlNodeChild(xmlDocumentRoot(xmlDocument), STRDEF("replace"), true), xmlDocumentRoot(xmlDocument2)),
            "insert xml");
        TEST_RESULT_STR_Z(
            strNewBuf(xmlDocumentBuf(xmlDocument)),
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<doc>\n"
            "    \n"
            "    \n"
            "    text55\n"
            "    <name id=\"id55\">name55</name>\n"
            "\n"
            "</doc>\n",
            "get xml");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
