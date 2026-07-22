/***********************************************************************************************************************************
Harness for Stack Trace Testing
***********************************************************************************************************************************/
#ifndef TEST_HARNESS_STACK_TRACE_H
#define TEST_HARNESS_STACK_TRACE_H

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Install/uninstall shim to return true from stackTraceBackCallback() to indicate that no backtrace data was found
#ifdef HAVE_LIBBACKTRACE
void hrnStackTraceBackShimInstall(void);
void hrnStackTraceBackShimUninstall(void);
#endif

#endif
