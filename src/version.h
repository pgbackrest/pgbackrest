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
Format Number -- defines format for info and manifest files as well as on-disk structure. Each info file and manifest stores the
format it was written with, so a repository may contain more than one format while older backups and archives expire.

A constant is defined for each format so that code which varies by format can be explicit about the format it applies to.
REPOSITORY_FORMAT_MIN/MAX are the range that can be read. The allow list for repo-format in build/config.yaml must be kept in sync
with MIN/MAX and its default is the format used for new repositories.
***********************************************************************************************************************************/
#define REPOSITORY_FORMAT_5                                         5
#define REPOSITORY_FORMAT_6                                         6

#define REPOSITORY_FORMAT_MIN                                       REPOSITORY_FORMAT_5
#define REPOSITORY_FORMAT_MAX                                       REPOSITORY_FORMAT_6

/***********************************************************************************************************************************
Project version components. PROJECT_VERSION and PROJECT_VERSION_NUM are automatically generated from the component parts.
***********************************************************************************************************************************/
#define PROJECT_VERSION_MAJOR                                       2
#define PROJECT_VERSION_MINOR                                       60
#define PROJECT_VERSION_PATCH                                       0
#define PROJECT_VERSION_SUFFIX                                      "dev"

#define PROJECT_VERSION                                             "2.60.0dev"
#define PROJECT_VERSION_NUM                                         2060000

#endif
