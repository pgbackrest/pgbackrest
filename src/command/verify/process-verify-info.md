```mermaid
graph TD
archiveinfo[Read archive.info] -->archiveinfoexist{Exists?}
archiveinfoexist --> |no|logarchiveinfoerr[Log Error]
archiveinfoexist -->|yes| archiveinfovalid{Valid?}
archiveinfovalid --> |no|logarchiveinfoerr
logarchiveinfoerr -->backupinfo[Read backup.info]
loadarchiveinfo -->backupinfo
backupinfo-->backupinfoexist{Exists?}
backupinfoexist --> |no| logbackupinfoerr[Log Error]
backupinfoexist -->|yes| backupinfovalid{Valid?}
backupinfovalid --> |no| logbackupinfoerr
loadarchiveinfo --> process[process]
loadbackupinfo --> process
archiveinfovalid --> |yes|loadarchiveinfo[Load jobdata->archiveInfo]
backupinfovalid --> |yes|loadbackupinfo[Load jobdata->backupInfo]
logbackupinfoerr --> done(-END-)
```
