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

// Use fdWrite shim for one call - specify return value and errno
void hrnFdWriteShimOne(ssize_t result, int errnoValue);
