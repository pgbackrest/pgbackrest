/***********************************************************************************************************************************
Version Numbers and Names
***********************************************************************************************************************************/
#ifndef VERSION_H
#define VERSION_H

/***********************************************************************************************************************************
Official name of the project
***********************************************************************************************************************************/
#define PROJECT_NAME                                                "pgBackRest"

/***********************************************************************************************************************************
Standard binary name
***********************************************************************************************************************************/
#define PROJECT_BIN                                                 "pgbackrest"

/***********************************************************************************************************************************
Config file name. The path will vary based on configuration.
***********************************************************************************************************************************/
#define PROJECT_CONFIG_FILE                                         PROJECT_BIN ".conf"

/***********************************************************************************************************************************
Config include path name. The parent path will vary based on configuration.
***********************************************************************************************************************************/
#define PROJECT_CONFIG_INCLUDE_PATH                                 "conf.d"

/***********************************************************************************************************************************
Format Number -- defines format for info and manifest files as well as on-disk structure. If this number changes then the repository
will be invalid unless migration functions are written.
***********************************************************************************************************************************/
#define REPOSITORY_FORMAT                                           5

/***********************************************************************************************************************************
Software version
***********************************************************************************************************************************/
#define PROJECT_VERSION                                             "2.54.2dev"

#endif
