/***********************************************************************************************************************************
Harness for Fd Testing
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Install/uninstall shim
void hrnFdReadyShimInstall(void);
void hrnFdReadyShimUninstall(void);

// Use shim for one call
void hrnFdReadyShimOne(bool result);

// Use ioFdWriteInternal shim for one call - specify return value and errno
void hrnIoFdWriteInternalShimOne(ssize_t result, int errNo);
