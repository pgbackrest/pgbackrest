enum
{
//    RM_XLOG_ID, defined in header with common definitions
    RM_XACT_ID =            1,
    RM_SMGR_ID =            2,
    RM_CLOG_ID =            3,
    RM_DBASE_ID =           4,
    RM_TBLSPC_ID =          5,
    RM_MULTIXACT_ID =       6,
    RM_RELMAP_ID =          7,
    RM_STANDBY_ID =         8,
    RM_HEAP2_ID =           9,
    RM_HEAP_ID =           10,
    RM_BTREE_ID =          11,
    RM_HASH_ID =           12,
    RM_GIN_ID =            13,
    RM_GIST_ID =           14,
    RM_SEQ_ID =            15,
    RM_SPGIST_ID =         16,
    RM_BITMAP_ID =         17,
    RM_DISTRIBUTEDLOG_ID = 18,
    RM_APPEND_ONLY_ID =    19,
};

enum
{
    // xlog
    XLOG_CHECKPOINT_SHUTDOWN =  0x00,
    XLOG_CHECKPOINT_ONLINE =    0x10,
//    XLOG_NOOP =                 0x20, defined in header with common definitions
    XLOG_NEXTOID =              0x30,
//    XLOG_SWITCH =               0x40, defined in header with common definitions
    XLOG_BACKUP_END =           0x50,
    XLOG_PARAMETER_CHANGE =     0x60,
    XLOG_RESTORE_POINT =        0x70,
    XLOG_FPW_CHANGE =           0x80,
    XLOG_END_OF_RECOVERY =      0x90,
    XLOG_FPI =                  0xA0,
    XLOG_NEXTRELFILENODE =      0xB0,
    XLOG_OVERWRITE_CONTRECORD = 0xC0,

// storage
    XLOG_SMGR_CREATE =    0x10,
    XLOG_SMGR_TRUNCATE =  0x20,

// heap
    XLOG_HEAP2_REWRITE =      0x00,
    XLOG_HEAP2_CLEAN =        0x10,
    XLOG_HEAP2_FREEZE_PAGE =  0x20,
    XLOG_HEAP2_CLEANUP_INFO = 0x30,
    XLOG_HEAP2_VISIBLE =      0x40,
    XLOG_HEAP2_MULTI_INSERT = 0x50,
    XLOG_HEAP2_LOCK_UPDATED = 0x60,
    XLOG_HEAP2_NEW_CID =      0x70,

    XLOG_HEAP_INSERT =        0x00,
    XLOG_HEAP_DELETE =        0x10,
    XLOG_HEAP_UPDATE =        0x20,
    XLOG_HEAP_MOVE =          0x30,
    XLOG_HEAP_HOT_UPDATE =    0x40,
    XLOG_HEAP_NEWPAGE =       0x50,
    XLOG_HEAP_LOCK =          0x60,
    XLOG_HEAP_INPLACE =       0x70,
    XLOG_HEAP_INIT_PAGE =     0x80,

// btree
    XLOG_BTREE_INSERT_LEAF =        0x00,
    XLOG_BTREE_INSERT_UPPER =       0x10,
    XLOG_BTREE_INSERT_META =        0x20,
    XLOG_BTREE_SPLIT_L =            0x30,
    XLOG_BTREE_SPLIT_R =            0x40,
    XLOG_BTREE_SPLIT_L_ROOT =       0x50,
    XLOG_BTREE_SPLIT_R_ROOT =       0x60,
    XLOG_BTREE_DELETE =             0x70,
    XLOG_BTREE_UNLINK_PAGE =        0x80,
    XLOG_BTREE_UNLINK_PAGE_META =   0x90,
    XLOG_BTREE_NEWROOT =            0xA0,
    XLOG_BTREE_MARK_PAGE_HALFDEAD = 0xB0,
    XLOG_BTREE_VACUUM =             0xC0,
    XLOG_BTREE_REUSE_PAGE =         0xD0,

// gin
    XLOG_GIN_CREATE_INDEX =          0x00,
    XLOG_GIN_CREATE_PTREE =          0x10,
    XLOG_GIN_INSERT =                0x20,
    XLOG_GIN_SPLIT =                 0x30,
    XLOG_GIN_VACUUM_PAGE =           0x40,
    XLOG_GIN_VACUUM_DATA_LEAF_PAGE = 0x90,
    XLOG_GIN_DELETE_PAGE =           0x50,
    XLOG_GIN_UPDATE_META_PAGE =      0x60,
    XLOG_GIN_INSERT_LISTPAGE =       0x70,
    XLOG_GIN_DELETE_LISTPAGE =       0x80,

// gist
    XLOG_GIST_PAGE_UPDATE =  0x00,
    XLOG_GIST_PAGE_SPLIT =   0x30,
    XLOG_GIST_CREATE_INDEX = 0x50,

// sequence
    XLOG_SEQ_LOG = 0x00,

// spgist
    XLOG_SPGIST_CREATE_INDEX =    0x00,
    XLOG_SPGIST_ADD_LEAF =        0x10,
    XLOG_SPGIST_MOVE_LEAFS =      0x20,
    XLOG_SPGIST_ADD_NODE =        0x30,
    XLOG_SPGIST_SPLIT_TUPLE =     0x40,
    XLOG_SPGIST_PICKSPLIT =       0x50,
    XLOG_SPGIST_VACUUM_LEAF =     0x60,
    XLOG_SPGIST_VACUUM_ROOT =     0x70,
    XLOG_SPGIST_VACUUM_REDIRECT = 0x80,

// bitmap
    XLOG_BITMAP_INSERT_LOVITEM =          0x20,
    XLOG_BITMAP_INSERT_META =             0x30,
    XLOG_BITMAP_INSERT_BITMAP_LASTWORDS = 0x40,

    XLOG_BITMAP_INSERT_WORDS = 0x50,

    XLOG_BITMAP_UPDATEWORD =  0x70,
    XLOG_BITMAP_UPDATEWORDS = 0x80,

// appendonly
    XLOG_APPENDONLY_INSERT =   0x00,
    XLOG_APPENDONLY_TRUNCATE = 0x10,
};
