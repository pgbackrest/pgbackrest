/***********************************************************************************************************************************
Info Command
***********************************************************************************************************************************/
#ifndef COMMAND_INFO_INFO_H
#define COMMAND_INFO_INFO_H

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Render info and output to stdout
FN_EXTERN void cmdInfo(void);

// Render backrest text version (in PROJECT_VERSION style) as 6-digits number format
FN_EXTERN const char *versionNumRender(const char *const versionStr);

#endif
