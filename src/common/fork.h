/***********************************************************************************************************************************
Fork Handler
***********************************************************************************************************************************/
#ifndef COMMON_FORK_H
#define COMMON_FORK_H

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Fork a new process and throw an error if it fails
int forkSafe(void);

// Detach a forked process so it can continue running after the parent process has exited. This is not a typical daemon startup
// because the parent process may continue to run and perform work for some time.
void forkDetach(void);

#endif
