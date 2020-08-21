```mermaid
graph TD
archiveinfo[Read archive.info]-->archiveinfoexist{Exists?}
archiveinfoexist-->|no|logarchiveinfoerr[Log Error]
archiveinfoexist-->|yes|archiveinfovalid{Valid?}
archiveinfovalid-->|no|logarchiveinfoerr
archiveinfovalid-->|yes|loadarchiveinfo[Load jobdata->archiveInfo]
logarchiveinfoerr-->backupinfo[Read backup.info]
loadarchiveinfo-->backupinfo
backupinfo-->backupinfoexist{Exists?}
backupinfoexist-->|no|logbackupinfoerr[Log Error]
backupinfoexist-->|yes|backupinfovalid{Valid?}
backupinfovalid-->|no|logbackupinfoerr
loadbackupinfo-->process(-process-)
backupinfovalid-->|yes|loadbackupinfo[Load jobdata->backupInfo]
logbackupinfoerr --> done(-END-)
```
