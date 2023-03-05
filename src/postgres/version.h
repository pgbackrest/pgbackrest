/***********************************************************************************************************************************
PostgreSQL Version Constants
***********************************************************************************************************************************/
#ifndef POSTGRES_VERSION_H
#define POSTGRES_VERSION_H

/***********************************************************************************************************************************
PostgreSQL name
***********************************************************************************************************************************/
#define PG_NAME                                                     "PostgreSQL"

/***********************************************************************************************************************************
PostgreSQL version constants
***********************************************************************************************************************************/
#define PG_VERSION_93                                                90300
#define PG_VERSION_94                                                90400
#define PG_VERSION_95                                                90500
#define PG_VERSION_96                                                90600
#define PG_VERSION_10                                               100000
#define PG_VERSION_11                                               110000
#define PG_VERSION_12                                               120000
#define PG_VERSION_13                                               130000
#define PG_VERSION_14                                               140000
#define PG_VERSION_15                                               150000

#define PG_VERSION_MAX                                              PG_VERSION_15

/***********************************************************************************************************************************
Version where various PostgreSQL capabilities were introduced
***********************************************************************************************************************************/
// tablespace_map is created during backup
#define PG_VERSION_TABLESPACE_MAP                                   PG_VERSION_95

// recovery target action supported
#define PG_VERSION_RECOVERY_TARGET_ACTION                           PG_VERSION_95

// parallel query supported
#define PG_VERSION_PARALLEL_QUERY                                   PG_VERSION_96

// xlog was renamed to wal
#define PG_VERSION_WAL_RENAME                                       PG_VERSION_10

// recovery settings are implemented as GUCs (recovery.conf is no longer valid)
#define PG_VERSION_RECOVERY_GUC                                     PG_VERSION_12

/***********************************************************************************************************************************
PostgreSQL version string constants for use in error messages
***********************************************************************************************************************************/
#define PG_VERSION_93_STR                                            "9.3"
#define PG_VERSION_94_STR                                            "9.4"
#define PG_VERSION_95_STR                                            "9.5"
#define PG_VERSION_96_STR                                            "9.6"
#define PG_VERSION_10_STR                                            "10"
#define PG_VERSION_11_STR                                            "11"
#define PG_VERSION_12_STR                                            "12"
#define PG_VERSION_13_STR                                            "13"
#define PG_VERSION_14_STR                                            "14"
#define PG_VERSION_15_STR                                            "15"

#endif
