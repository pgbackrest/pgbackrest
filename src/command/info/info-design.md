INFO COMMAND - MULTI-REPO  (cshang remove)

PROBLEM TO SOLVE:
The info command requires the repo to be specified resulting in the user needing to run the command multiple times: one for each configured repository. However, it is not considered useful if the command is simply automatically displayed for each repository. What is needed is to identify which repository the data is located and to do so in a way that is meaningful to the user.

REQUIREMENTS:
1. Ability for the user to run the info command without having to specify the repository.
2. Output the data (JSON and Text) in such a way that the user can easily determine the location (repository) of the most recent backup.
3. Identify the WAL (currently min/max) repo location.
4. Backups, in the text output, should still be displayed from oldest to newest.

USE SECTION 8 MONITORING SECTION
want to know the wal across the repos and what the wal looks like - per stanza - none of the data can cross stanzas
database version and system id - maybe use instead of db-id
don't want to group by dbid - instead say this is the 9.4 database and this is the 9.5 database
want to be able to pull the data out without a complicated
- MOST IMPORTANT: when was last successful backup and what is the last wal (is archiving up to date) - also now important to know which repo it comes from.


CURRENT TEXT OUTPUT: See https://github.com/pgbackrest/pgbackrest/pull/1227#issuecomment-735945748  (also duplicated below)

POSSIBLE COALLATED OUTPUT:

stanza: demo
    status: ok
    cipher: mixed
        repo1:
            status: ok
            cipher: none
        repo2:
            status: ok
            cipher: aes-256-cbc

    db (prior)
        wal archive min/max (repo1: 12-1): 000000010000000000000003/00000001000000000000000A

        full backup: 20201125-191243F
            timestamp start/stop: 2020-11-25 19:12:43 / 2020-11-25 19:12:56
            wal start/stop: 000000010000000000000003 / 000000010000000000000003
            database size: 23.4MB, backup size: 23.4MB
            repository id: 1, size: 2.8MB, repository backup size: 2.8MB

        incr backup: 20201125-191243F_20201125-191352I
            timestamp start/stop: 2020-11-25 19:13:52 / 2020-11-25 19:13:55
            wal start/stop: 000000010000000000000006 / 000000010000000000000006
            database size: 23.4MB, backup size: 2.3MB
            repository id: 1, size: 2.8MB, repository backup size: 234.6KB
            backup reference list: 20201125-191243F

    db (current)
        wal archive min/max (repo1: 13-2): 000000010000000000000012/000000010000000000000015
        wal archive min/max (repo2: 13-1): 000000010000000000000013/000000010000000000000015

        full backup: 20201130-172658F
            timestamp start/stop: 2020-11-30 17:26:58 / 2020-11-30 17:27:08
            wal start/stop: 000000010000000000000012 / 000000010000000000000012
            database size: 23.3MB, backup size: 23.3MB
            repository id: 1, size: 2.9MB, repository backup size: 2.9MB

        full backup: 20201130-172717F
            timestamp start/stop: 2020-11-30 17:27:17 / 2020-11-30 17:27:28
            wal start/stop: 000000010000000000000013 / 000000010000000000000013
            database size: 23.3MB, backup size: 23.3MB
            repository id: 2, size: 2.9MB, repository backup size: 2.9MB

        diff backup: 20201130-172658F_20201130-174420D
            timestamp start/stop: 2020-11-30 17:44:20 / 2020-11-30 17:44:22
            wal start/stop: 000000010000000000000015 / 000000010000000000000015
            database size: 23.3MB, backup size: 8.3KB
            repository id: 1, size: 2.9MB, repository backup size: 433B
            backup reference list: 20201130-172658F


CONSIDERATIONS:
1. A stanza may not exist on all repositories so how to report that? Currently the stanza section contains "status" and "cipher" which can vary for each repo.
    1.1 Maybe have this section as a summary such that if a stanza is not-OK on one or more repos, then "status" will not be OK and if "cipher" is different then indicate as "mixed" (vs none or aes-256-cbc) (maybe can use mixed for the status too?)
    2.2 In addition, or instead of, the summary display the stanza status and cipher per repo. Possibly only display if the status or cipher is "mixed".

2. There can be different db history ids (db-id) on each repo, meaning WAL XYZ is under archive directory 13-2 (db-id=2) on repo 1 but 13-1 (db-id=1) on repo2. From past experience, having the history ids differ between the archive.info and backup.info files in the repo caused problems especially when there were multiple histories with the same system and PG version. Although the archive.info and backup.info will be the same on a repo, it may differ between repos so we may want to avoid that.
    2.1 How to keep the repo histories in sync?
    2.2 When to identify when repos are not in-sync? (i.e. which commands would be responsible?)
    2.3 How to fix out-of-sync repos?

3. There should be some indication of the archive on each repo. The JSON array for the archive information (below) which is per stanza could be enhanced to add in a repository id but is that helpful to the user?
"archive" : [
      {
        "database" : {
          "id" : 1
        },
        "id" : "9.4-1",
        "max" : "000000020000000000000007",
        "min" : "000000010000000000000002"
      }
    ],

4. Should backups be displayed in order regardless of repository or per repository?

EXAMPLE https://github.com/pgbackrest/pgbackrest/pull/1227#issuecomment-735945748:

> pgbackrest info --repo=1

stanza: demo
    status: ok
    cipher: none

    db (prior)
        wal archive min/max (12-1): 000000010000000000000003/00000001000000000000000A

        full backup: 20201125-191243F
            timestamp start/stop: 2020-11-25 19:12:43 / 2020-11-25 19:12:56
            wal start/stop: 000000010000000000000003 / 000000010000000000000003
            database size: 23.4MB, backup size: 23.4MB
            repository size: 2.8MB, repository backup size: 2.8MB

        incr backup: 20201125-191243F_20201125-191352I
            timestamp start/stop: 2020-11-25 19:13:52 / 2020-11-25 19:13:55
            wal start/stop: 000000010000000000000006 / 000000010000000000000006
            database size: 23.4MB, backup size: 2.3MB
            repository size: 2.8MB, repository backup size: 234.6KB
            backup reference list: 20201125-191243F

    db (current)
        wal archive min/max (13-2): 000000010000000000000012/000000010000000000000015

        full backup: 20201130-172658F
            timestamp start/stop: 2020-11-30 17:26:58 / 2020-11-30 17:27:08
            wal start/stop: 000000010000000000000012 / 000000010000000000000012
            database size: 23.3MB, backup size: 23.3MB
            repository size: 2.9MB, repository backup size: 2.9MB

        diff backup: 20201130-172658F_20201130-174420D
            timestamp start/stop: 2020-11-30 17:44:20 / 2020-11-30 17:44:22
            wal start/stop: 000000010000000000000015 / 000000010000000000000015
            database size: 23.3MB, backup size: 8.3KB
            repository size: 2.9MB, repository backup size: 433B
            backup reference list: 20201130-172658F

> pgbackrest info --repo=2

stanza: demo
    status: ok
    cipher: aes-256-cbc

    db (current)
        wal archive min/max (13-1): 000000010000000000000013/000000010000000000000015

        full backup: 20201130-172717F
            timestamp start/stop: 2020-11-30 17:27:17 / 2020-11-30 17:27:28
            wal start/stop: 000000010000000000000013 / 000000010000000000000013
            database size: 23.3MB, backup size: 23.3MB
            repository size: 2.9MB, repository backup size: 2.9MB


COMMANDS
If we are requiring db-ids to be the same across repos, where would/should we catch a discrepancy and what action should be taken? Is fixing a mismatch even possible? Probably not automatically - will need to be done manually.

                            OPERATES OVER               CATCH DISCREPANCY               ACTION
ARCHIVE_GET                 single repo (always)        No
ARCHIVE_PUSH                all repos                   Maybe                           Should BUT too time-consuming?
BACKUP                      single repo (may change?)   Maybe                           Error/Warn? Should backup be allowed?
CHECK                       all repos                   Yes                             Error
EXPIRE                      single repo (always)        Maybe                           Error/Warn? Should expire be allowed?
INFO                        single and all repos        Yes                             Error
REPO_CREATE                 single repo                 No                              I think this only creates the repo path?
REPO_GET                    single repo                 No
REPO_LS                     single repo                 No
REPO_PUT                    single repo                 No
REPO_RM                     single repo                 Maybe                           Can this remove info files & archive paths?
RESTORE                     single repo (always)        Maybe                           But if do check, only warn since must restore
STANZA_CREATE               all repos (may change?)     Yes                             Special case b/c it must compare/copy the
                                                                                        same backup/archive info file to all repos.
                                                                                        But if stanza exists and current Db data in
                                                                                        the info files matches the current db system,
                                                                                        version, AND NOW "master info file" db-id,
                                                                                        then OK even if older history missing?
STANZA_DELETE               single repo (always)        Yes                             Error/Warn but remove the stanza from the repo
STANZA_UPGRADE              all repos                   Yes                             This definitely must check and error
VERIFY                      all repos (may change?)     Yes                             Log error



QUESTIONS:
1) Where would the repo-id go here?
2) What if the stanza exists on more than one repo? We would need an array under the status OR have a repo[] array and have the status under each of those. But if there were an option to say, display this for repo=1, how to pick that out? Maybe just duplicate the information such that we have a whole other stanza section

    "["
        "{"
            "\"archive\":[],"  <-- this is missing if the repo is empty and the --stanza=stanza1 is passed
            "\"backup\":[],"
            "\"db\":[],"
            "\"name\":\"stanza1\","
            "\"status\":{"
                "\"code\":1,"
                "\"lock\":{\"backup\":{\"held\":false}},"
                "\"message\":\"missing stanza path\""
                "}"
        "}"
    "]"

Maybe have:
    "["
        "{"
            "\"backup\":[],"
            "\"db\":[],"
            "\"name\":\"stanza1\","
            "\"repo\":1,"                     <-- or repo=0 - maybe it is the internal array index?
            "\"status\":{"
                "\"code\":1,"
                "\"lock\":{\"backup\":{\"held\":false}},"
                "\"message\":\"missing stanza path\""
                "}"
        "}",
        "{"
            "\"backup\":[],"
            "\"db\":[],"
            "\"name\":\"stanza1\","
            "\"repo\":2,"                     <-- or repo=1 - maybe it is the internal array index?
            "\"status\":{"
                "\"code\":1,"
                "\"lock\":{\"backup\":{\"held\":false}},"
                "\"message\":\"missing stanza path\""
                "}"
        "}"
    "]"

BUT If look through repos, we get instead: [{"backup":[],"db":[],"name":"stanza1","status":{"code":1,"lock":{"backup":{"held":false}},"message":"missing stanza path"}}]
[{"backup":[],"db":[],"name":"stanza1","status":{"code":1,"lock":{"backup":{"held":false}},"message":"missing stanza path"}}]

AND WHAT WE NEED IS to replace the ][ with a comma:
[{"backup":[],"db":[],"name":"stanza1","status":{"code":1,"lock":{"backup":{"held":false}},"message":"missing stanza path"}},
{"backup":[],"db":[],"name":"stanza1","status":{"code":1,"lock":{"backup":{"held":false}},"message":"missing stanza path"}}]


BUT ABOVE IS NOT SUFFICIENT TO JUST PUT REPO ID, SO what if repo array (discussed with David 12/2 and we agreed to the following):
        "{"
            "\"archive\":[],"               <-- need repo-id
            "\"backup\":[],"                <-- need repo-id
            "\"db\":[],"                    <-- need repo-id
            "\"name\":\"stanza1\","
            "\"cipher\":\"none\","
            "\"repo\":[         <--- repo array which should contain repo id, the cipher, stanza status (==> path? (NOT now))
                key             <--- key from config (so not index)
                cipher
                status {
                    code,
                    msg
                }
            ],"
            "\"status\":{"
                "\"code\":1,"                   <-- 5 = mixed
                "\"lock\":{\"backup\":{\"held\":false}},"   <-- this is NOT per repo
                "\"message\":\"missing stanza path\""  <-- some message about why the status is mixed
                "}"
        "}"

PROBLEM:
1) Need to have the stanza as an outer loop but need to get the stanza list from the repo when the --stanza is not specified.
2) Ordering the backups in the list so that the last is the most current across all repos. We only need the backup.info file current section from each.

* Need to identify what stanzas exist. Call out to each repo to get a list of stanzas?
* Need to identify which repos each stanza exists on - but since need to connect to the repo to get a list, we'll know.
* May need to add some logic to backup section to, what, sort the list somehow or only add to the end when greater than the last. But the trick is inserting in between - can't sort or insert "in-between" with KVs. Maybe we mimic the code in manifest where we have a list that is sorted by the name (first field), so we add, add, add then sort by label when done.
* Need to sort the last archived WAL by prior and current - For example, below the last archived WAL on 12-1 was 000000020000000000000009 but the last archived WAL on 13-2 is 00000001000000000000000D. The question is, if we revert, then 12-3 becomes the new DB and then I assume we're start with timeline 3 so it will be highest. But it should not matter since the last archive in the json array will be the current db. But only for that repo. so we may have 13-1 and 13-2 but 13-2 might be on repo1 and 13-1 on repo 2 and we might have different WAL, so 13-1, if we're looking at repos in order, then repo2 will be the last repo looked at but the WAL may be newer on repo1.

{
    name = stanza1
    repo =
    [1, cipher, status {code, message}],
    [2, cipher, status {code, message}],


Right now, the comment in the stanzaInfoList is incorrect. Although we go from index last to first, the first is the newest so we're actually getting the db ids from oldest first to newest.
printf("HISTORY ID %u, PGIDX: %u\n", pgData.id, pgIdx); fflush(stdout);

    HISTORY ID 1, PGIDX: 2
    HISTORY ID 2, PGIDX: 1
    HISTORY ID 3, PGIDX: 0

USING THIS DATA

REPO1:
stanza: demo
    status: ok
    cipher: none

    db (prior)
        wal archive min/max (12-1): 000000010000000000000001/000000020000000000000008

        full backup: 20201203-144849F
            timestamp start/stop: 2020-12-03 14:48:49 / 2020-12-03 14:49:01
            wal start/stop: 000000010000000000000002 / 000000010000000000000002
            database size: 23.4MB, backup size: 23.4MB
            repository size: 2.8MB, repository backup size: 2.8MB

        diff backup: 20201203-144849F_20201203-144903D
            timestamp start/stop: 2020-12-03 14:49:03 / 2020-12-03 14:49:04
            wal start/stop: 000000010000000000000003 / 000000010000000000000003
            database size: 23.4MB, backup size: 8.3KB
            repository size: 2.8MB, repository backup size: 426B
            backup reference list: 20201203-144849F

        incr backup: 20201203-144849F_20201203-145849I
            timestamp start/stop: 2020-12-03 14:58:49 / 2020-12-03 14:58:51
            wal start/stop: 000000020000000000000005 / 000000020000000000000005
            database size: 23.4MB, backup size: 8.5KB
            repository size: 2.8MB, repository backup size: 605B
            backup reference list: 20201203-144849F, 20201203-144849F_20201203-144903D

    db (prior)
        wal archive min/max (13-2): 00000001000000000000000C/00000001000000000000000D

    db (current)
        wal archive min/max (12-3): 000000030000000000000008/000000030000000000000009  <-- changed to be ahead of 12-2 on repo2
REPO2
stanza: demo
    status: error (no valid backups)
    cipher: none

    db (prior)
        wal archive min/max (13-1): 00000001000000000000000C/00000001000000000000000D

    db (current)
        wal archive min/max (12-2): 000000030000000000000008/000000030000000000000008

ARCHIVE.INFO
repo1:
[db]
db-id=3
db-system-id=6902042121635098774
db-version="12"

[db:history]
1={"db-id":6902042121635098774,"db-version":"12"}
2={"db-id":6902048649867809661,"db-version":"13"}
3={"db-id":6902042121635098774,"db-version":"12"}

repo2:
[db]
db-id=2
db-system-id=6902042121635098774
db-version="12"

[db:history]
1={"db-id":6902048649867809661,"db-version":"13"}
2={"db-id":6902042121635098774,"db-version":"12"}

RESULT SHOULD BE:
stanza1:
    archive-id: 9.4-1 on repo2 must be the last in the archive list because it's WAL value for the 9.4 sys-id/version is the same as 9.4-1 and 9.4-3 on repo1. So the sort has to be system-id, version, WAL

FOR EACH REPO (repoIdx = 0..total or whatever):
    !!! PROBLEM is getting the repoKey for the repoIdx
    Read the stanza list - but there may be different stanzas on different repos so fill a stanzaList where listAdd(stanzaList, &stanzaInfo) and stanzaInfo would be a
    typedef struct StanzaInfo
    {
        const String *name;                                             // Stanza Name
        const StringList *repoKey? or repoIdx?;                         // List of repositories where this stanza is located
        cons List *dbList     <-- maybe have dbList in this stanza structure
    } StanzaInfo;
ROF

FOR EACH STANZA
    FOR EACH REPO (repoIdx = 0..total or whatever)

        Read archive.info
        FOR EACH archive.info history from oldest to newest
            Add [repoKey, repoIdx, db-id, system-id, version] to dbList for the stanza
            dbList: <-- does this need to be sorted? Can it be if the name to sort by is not unique?

                [repoKey=1, repoIdx=0, id=1, system-id=6902042121635098774, version=12]
                [repoKey=1, repoIdx=0, id=2, system-id=6902048649867809661, version=13]
                [repoKey=1, repoIdx=0, id=3, system-id=6902042121635098774, version=12]
                --- end repo1
                [repoKey=2, repoIdx=1, id=1, system-id=6902048649867809661, version=13]
                [repoKey=2, repoIdx=1, id=2, system-id=6902042121635098774, version=12]
                -- end repo2

            Get minWAL / maxWAL
            Add [archive-id, repoIdx, db-id, system-id, version, minWAL, maxWAL] to archiveList
            archiveList:
                [archive-id=12-1, repoIdx=0, db-id=1, system-id=6902042121635098774, version=12, minWAL=000000010000000000000001, maxWAL=000000020000000000000008]
                [archive-id=13-2, repoIdx=0, db-id=2, system-id=6902048649867809661, version=13, minWAL=00000001000000000000000C, maxWAL=00000001000000000000000D]
                [archive-id=12-3, repoIdx=0, db-id=1, system-id=6902042121635098774, version=12, minWAL=000000030000000000000008, maxWAL=000000030000000000000009]
                [archive-id=13-1, repoIdx=1, db-id=1, system-id=6902048649867809661, version=13, minWAL=00000001000000000000000C, maxWAL=00000001000000000000000D]
                [archive-id=12-2, repoIdx=1, db-id=2, system-id=6902042121635098774, version=12, minWAL=000000030000000000000008, maxWAL=000000030000000000000008]


[
  {
    archive: [
      {
        database: {
          id: 1,
          repoKey: 2           <-- repo key is added to the database and will need to be used to find the correct database information from the json db section
        },
        id: 13-1,
        max: 00000001000000000000000D,
        min: 00000001000000000000000C
      }
      {
        database: {
          id: 2,
          repoKey: 1
        },
        id: 13-2,
        max: 00000001000000000000000D,
        min: 00000001000000000000000C
      },
      {
        database: {
          id: 1,
          repoKey: 1
        },
        id: 12-1,       <-- This is considered a "prior" database in our current display but is grouped here because it has same system-id/version as current db even though it was created before the above 13-2 (repo1)
        max: 000000020000000000000008,
        min: 000000010000000000000001
      },
      {
        database: {
            id: 2,
            repoKey: 2
        },
        id: 12-2,
        max: 000000030000000000000008,    <--- repo 2 is behind repo1 but how will we know if only displaying the max over same system-id and PG version?
        min: 000000030000000000000008
      },
      {
        database: {
          id: 3,
          repoKey: 1
        },
        id: 12-3,              <-- this is has been ordered so this is the last in the archive list b/c the system-id/version = current db and WAL is max
        max: 000000030000000000000009,
        min: 000000030000000000000008
      }
    ],
    backup: [   <-- Although not displayed in this example, the backups can be interleaved because a backup on repo1 may be later than one on repo2
      {
        archive: {
          start: 000000010000000000000002,
          stop: 000000010000000000000002
        },
        backrest: {
          format: 5,
          version: 2.31dev
        },
        database: {
          id: 1,
          repoKey: 1
        },
        info: {
          delta: 24529261,
          repository: {
            delta: 2931569,
            size: 2931569
          },
          size: 24529261
        },
        label: 20201203-144849F,
        prior: null,
        reference: null,
        timestamp: {
          start: 1607006929,
          stop: 1607006941
        },
        type: full
      },
      {
        archive: {
          start: 000000010000000000000003,
          stop: 000000010000000000000003
        },
        backrest: {
          format: 5,
          version: 2.31dev
        },
        database: {
          id: 1,
          repoKey: 1
        },
        info: {
          delta: 8457,
          repository: {
            delta: 426,
            size: 2931567
          },
          size: 24529261
        },
        label: 20201203-144849F_20201203-144903D,
        prior: 20201203-144849F,
        reference: [
          20201203-144849F
        ],
        timestamp: {
          start: 1607006943,
          stop: 1607006944
        },
        type: diff
      },
      {
        archive: {
          start: 000000020000000000000005,
          stop: 000000020000000000000005
        },
        backrest: {
          format: 5,
          version: 2.31dev
        },
        database: {
          id: 1,
          repoKey: 1
        },
        info: {
          delta: 8678,
          repository: {
            delta: 605,
            size: 2931671
          },
          size: 24529402
        },
        label: 20201203-144849F_20201203-145849I,
        prior: 20201203-144849F_20201203-144903D,
        reference: [
          20201203-144849F,
          20201203-144849F_20201203-144903D
        ],
        timestamp: {
          start: 1607007529,
          stop: 1607007531
        },
        type: incr
      }
    ],
    cipher: none,  <-- this is now the overall cipher, so could be "mixed" if, say, repo1 is encrypted but repo2 is not
    db: [
      {
        id: 1,
        system-id: 6902042121635099000,
        version: 12,
        repoKey: 1
      },
      {
        id: 2,
        system-id: 6902048649867810000,
        version: 13,
        repoKey: 1
      },
      {
        id: 3,
        system-id: 6902042121635099000,
        version: 12,
        repoKey: 1
      },
      {
        id: 1,
        system-id: 6902048649867810000,
        version: 13,
        repoKey: 2
      },
      {
        id: 2,
        system-id: 6902042121635099000,
        version: 12,
        repoKey: 2
      }
    ],
    name: demo,
    repo: [
        {
            idx: 0,
            key: 1,
            cipher: none
            status : {
                code: 0,
                message: ok
            }
        },
        {
            idx: 1,
            key: 2,
            cipher: none
            status : {
                code: 2,
                message: no valid backups
            }
        },
    ],
    status: {
      code: 5,   <-- since different statuses on each repo, the overall status code represents "mixed"
      lock: {
        backup: {
          held: false
        }
      },
      message: mixed <-- since different statuses on each repo, the overall status is "mixed"
    }
  }
]

---- Above output currently with modified code to display stanza data for each repo
[
  {
    "archive": [
      {
        "database": {
          "id": 1
        },
        "id": "12-1",
        "max": "000000020000000000000008",
        "min": "000000010000000000000001"
      },
      {
        "database": {
          "id": 2
        },
        "id": "13-2",
        "max": "00000001000000000000000D",
        "min": "00000001000000000000000C"
      },
      {
        "database": {
          "id": 3
        },
        "id": "12-3",
        "max": "000000030000000000000009",
        "min": "000000030000000000000009"
      }
    ],
    "backup": [
      {
        "archive": {
          "start": "000000010000000000000002",
          "stop": "000000010000000000000002"
        },
        "backrest": {
          "format": 5,
          "version": "2.31dev"
        },
        "database": {
          "id": 1
        },
        "info": {
          "delta": 24529261,
          "repository": {
            "delta": 2931569,
            "size": 2931569
          },
          "size": 24529261
        },
        "label": "20201203-144849F",
        "prior": null,
        "reference": null,
        "timestamp": {
          "start": 1607006929,
          "stop": 1607006941
        },
        "type": "full"
      },
      {
        "archive": {
          "start": "000000010000000000000003",
          "stop": "000000010000000000000003"
        },
        "backrest": {
          "format": 5,
          "version": "2.31dev"
        },
        "database": {
          "id": 1
        },
        "info": {
          "delta": 8457,
          "repository": {
            "delta": 426,
            "size": 2931567
          },
          "size": 24529261
        },
        "label": "20201203-144849F_20201203-144903D",
        "prior": "20201203-144849F",
        "reference": [
          "20201203-144849F"
        ],
        "timestamp": {
          "start": 1607006943,
          "stop": 1607006944
        },
        "type": "diff"
      },
      {
        "archive": {
          "start": "000000020000000000000005",
          "stop": "000000020000000000000005"
        },
        "backrest": {
          "format": 5,
          "version": "2.31dev"
        },
        "database": {
          "id": 1
        },
        "info": {
          "delta": 8678,
          "repository": {
            "delta": 605,
            "size": 2931671
          },
          "size": 24529402
        },
        "label": "20201203-144849F_20201203-145849I",
        "prior": "20201203-144849F_20201203-144903D",
        "reference": [
          "20201203-144849F",
          "20201203-144849F_20201203-144903D"
        ],
        "timestamp": {
          "start": 1607007529,
          "stop": 1607007531
        },
        "type": "incr"
      }
    ],
    "cipher": "none",
    "db": [
      {
        "id": 1,
        "system-id": 6902042121635099000,
        "version": "12"
      },
      {
        "id": 2,
        "system-id": 6902048649867810000,
        "version": "13"
      },
      {
        "id": 3,
        "system-id": 6902042121635099000,
        "version": "12"
      }
    ],
    "name": "demo",
    "status": {
      "code": 0,
      "lock": {
        "backup": {
          "held": false
        }
      },
      "message": "ok"
    }
  },
  {
    "archive": [
      {
        "database": {
          "id": 1
        },
        "id": "13-1",
        "max": "00000001000000000000000D",
        "min": "00000001000000000000000C"
      },
      {
        "database": {
          "id": 2
        },
        "id": "12-2",
        "max": "000000030000000000000009",
        "min": "000000030000000000000009"
      }
    ],
    "backup": [],
    "cipher": "none",
    "db": [
      {
        "id": 1,
        "system-id": 6902048649867810000,
        "version": "13"
      },
      {
        "id": 2,
        "system-id": 6902042121635099000,
        "version": "12"
      }
    ],
    "name": "demo",
    "status": {
      "code": 2,
      "lock": {
        "backup": {
          "held": false
        }
      },
      "message": "no valid backups"
    }
  }
]
---------------

This is before multi-repo - single stanza, upgraded so 2 archive & db ids
[
  {
    "archive": [
      {
        "database": {
          "id": 1
        },
        "id": "12-1",
        "max": "000000020000000000000009",
        "min": "000000010000000000000002"
      },
      {
        "database": {
          "id": 2
        },
        "id": "13-2",
        "max": "00000001000000000000000D",
        "min": "00000001000000000000000D"
      }
    ],
    "backup": [
      {
        "archive": {
          "start": "000000010000000000000002",
          "stop": "000000010000000000000002"
        },
        "backrest": {
          "format": 5,
          "version": "2.31dev"
        },
        "database": {
          "id": 1
        },
        "info": {
          "delta": 24529261,
          "repository": {
            "delta": 2931568,
            "size": 2931568
          },
          "size": 24529261
        },
        "label": "20201202-005755F",
        "prior": null,
        "reference": null,
        "timestamp": {
          "start": 1606870675,
          "stop": 1606870689
        },
        "type": "full"
      },
      {
        "archive": {
          "start": "000000010000000000000003",
          "stop": "000000010000000000000003"
        },
        "backrest": {
          "format": 5,
          "version": "2.31dev"
        },
        "database": {
          "id": 1
        },
        "info": {
          "delta": 8457,
          "repository": {
            "delta": 427,
            "size": 2931568
          },
          "size": 24529261
        },
        "label": "20201202-005755F_20201202-005811D",
        "prior": "20201202-005755F",
        "reference": [
          "20201202-005755F"
        ],
        "timestamp": {
          "start": 1606870691,
          "stop": 1606870692
        },
        "type": "diff"
      },
      {
        "archive": {
          "start": "000000020000000000000007",
          "stop": "000000020000000000000007"
        },
        "backrest": {
          "format": 5,
          "version": "2.31dev"
        },
        "database": {
          "id": 1
        },
        "info": {
          "delta": 3326438,
          "repository": {
            "delta": 376658,
            "size": 2932842
          },
          "size": 24537594
        },
        "label": "20201202-005755F_20201202-161614I",
        "prior": "20201202-005755F_20201202-005811D",
        "reference": [
          "20201202-005755F",
          "20201202-005755F_20201202-005811D"
        ],
        "timestamp": {
          "start": 1606925774,
          "stop": 1606925782
        },
        "type": "incr"
      },
      {
        "archive": {
          "start": "00000001000000000000000D",
          "stop": "00000001000000000000000D"
        },
        "backrest": {
          "format": 5,
          "version": "2.31dev"
        },
        "database": {
          "id": 2
        },
        "info": {
          "delta": 24308077,
          "repository": {
            "delta": 2984999,
            "size": 2984999
          },
          "size": 24308077
        },
        "label": "20201202-162241F",
        "prior": null,
        "reference": null,
        "timestamp": {
          "start": 1606926161,
          "stop": 1606926179
        },
        "type": "full"
      }
    ],
    "cipher": "none",
    "db": [
      {
        "id": 1,
        "system-id": 6901456909226529000,
        "version": "12"
      },
      {
        "id": 2,
        "system-id": 6901694283921744000,
        "version": "13"
      }
    ],
    "name": "demo",
    "status": {
      "code": 0,
      "lock": {
        "backup": {
          "held": false
        }
      },
      "message": "ok"
    }
  }
]

Text output for the above:

stanza: demo
    status: ok
    cipher: none

    db (prior)
        wal archive min/max (12-1): 000000010000000000000002/000000020000000000000009

        full backup: 20201202-005755F
            timestamp start/stop: 2020-12-02 00:57:55 / 2020-12-02 00:58:09
            wal start/stop: 000000010000000000000002 / 000000010000000000000002
            database size: 23.4MB, backup size: 23.4MB
            repository size: 2.8MB, repository backup size: 2.8MB

        diff backup: 20201202-005755F_20201202-005811D
            timestamp start/stop: 2020-12-02 00:58:11 / 2020-12-02 00:58:12
            wal start/stop: 000000010000000000000003 / 000000010000000000000003
            database size: 23.4MB, backup size: 8.3KB
            repository size: 2.8MB, repository backup size: 427B
            backup reference list: 20201202-005755F

        incr backup: 20201202-005755F_20201202-161614I
            timestamp start/stop: 2020-12-02 16:16:14 / 2020-12-02 16:16:22
            wal start/stop: 000000020000000000000007 / 000000020000000000000007
            database size: 23.4MB, backup size: 3.2MB
            repository size: 2.8MB, repository backup size: 367.8KB
            backup reference list: 20201202-005755F, 20201202-005755F_20201202-005811D

    db (current)
        wal archive min/max (13-2): 00000001000000000000000D/00000001000000000000000D

        full backup: 20201202-162241F
            timestamp start/stop: 2020-12-02 16:22:41 / 2020-12-02 16:22:59
            wal start/stop: 00000001000000000000000D / 00000001000000000000000D
            database size: 23.2MB, backup size: 23.2MB
            repository size: 2.8MB, repository backup size: 2.8MB
