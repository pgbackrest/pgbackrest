/***********************************************************************************************************************************
Test Storage File
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    // *****************************************************************************************************************************
    if (testBegin("storageFileNew() and storageFileFree()"))
    {
        MemContext *memContext = NULL;
        StorageFile *file = NULL;

        MEM_CONTEXT_NEW_BEGIN("TestFile")
        {
            memContext = MEM_CONTEXT_NEW();
            TEST_ASSIGN(file, storageFileNew((Storage *)1, strNew("file"), storageFileTypeRead, (void *)2), "new file");
        }
        MEM_CONTEXT_NEW_END();

        TEST_RESULT_PTR(storageFileData(file), (void *)2, "    check data");
        TEST_RESULT_PTR(file->memContext, memContext, "    check mem context");
        TEST_RESULT_STR(strPtr(storageFileName(file)), "file", "    check name");
        TEST_RESULT_INT(file->type, storageFileTypeRead, "    check type");
        TEST_RESULT_PTR(storageFileStorage(file), (Storage *)1, "    check storage");

        TEST_RESULT_VOID(storageFileFree(file), "free file");
        TEST_RESULT_VOID(storageFileFree(NULL), "free null file");
    }
}
