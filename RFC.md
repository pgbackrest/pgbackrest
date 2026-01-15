# RFC: Externalize Relation File Inclusion List for Table Access Methods

## Status
Draft - Seeking Feedback

## Authors
- Supabase Team

## Summary

This RFC proposes externalizing the hardcoded relation file pattern matching ("inclusion list") in pgBackRest to support alternative PostgreSQL Table Access Methods (TAMs) such as OrioleDB. Currently, pgBackRest uses a hardcoded regular expression to identify PostgreSQL relation files, which assumes the default heap storage format with specific fork types (`_fsm`, `_vm`, `_init`). Alternative TAMs may use different file naming conventions, additional fork types, or different storage layouts that are not recognized by the current implementation.

## Background

### Current Implementation

pgBackRest identifies relation files using the following hardcoded pattern in `src/info/manifest.c:1281`:

```c
#define RELATION_EXP "[0-9]+(_(fsm|vm)){0,1}(\\.[0-9]+){0,1}"
```

This pattern matches:
- `16384` - Main heap relation
- `16384_fsm` - Free Space Map fork
- `16384_vm` - Visibility Map fork
- `16384.1` - Second segment of main relation (for files > 1GB)
- `16384_fsm.1` - Second segment of FSM fork

Additionally, the code recognizes:
- `_init` suffix - Marks unlogged relations (excluded from backup)
- Temp relations with `t[0-9]+_` prefix (excluded from backup)

### Where This Pattern Is Used

1. **Manifest Building** (`src/info/manifest.c:1441`): Identifies relation files for special handling
2. **Unlogged Relation Detection** (`src/info/manifest.c:1483-1485`): Checks for `_init` forks
3. **Temp Relation Filtering** (`src/info/manifest.c:1351`): Excludes temporary tables
4. **Page Checksum Validation** (`src/command/backup/pageChecksum.c`): Applies PostgreSQL page checksums to relation files

### Current Database Path Pattern

Relation files are expected in these locations (`src/info/manifest.c:1282-1283`):

```c
#define DB_PATH_EXP \
    "(pg_data/(global|base/[0-9]+)|pg_tblspc/[0-9]+/%s/[0-9]+)"
```

- `pg_data/global/` - System catalog and shared relations
- `pg_data/base/[DBOID]/` - Per-database relations
- `pg_tblspc/[TBLSPC_OID]/PG_VERSION_CATALOG/[DBOID]/` - Tablespace relations

## Problem Statement

### Alternative Table Access Methods

PostgreSQL's Table Access Method (TAM) API (introduced in PostgreSQL 12) allows custom storage engines. Examples include:

- **OrioleDB**: A modern storage engine with undo logging, row-level WAL, and different file structures
- **Zedstore**: Columnar storage engine
- **Citus Columnar**: Columnar storage for analytics
- **Custom TAMs**: Any organization can implement their own storage engine

### Why Current Approach Fails

Alternative TAMs may:

1. **Use different file naming conventions**
   - Additional fork types beyond `fsm`/`vm`/`init`
   - Different segment size boundaries
   - Non-numeric file identifiers or prefixes

2. **Have different storage layouts**
   - Files in different directories
   - Additional metadata files with specific naming patterns
   - Different unlogged/temp relation markers

3. **Require different page validation**
   - Non-standard page formats
   - Different or no page checksums
   - Alternative data integrity mechanisms

### Example: OrioleDB

OrioleDB uses its own storage format and may have:
- Different file extensions or suffixes for its data files
- UNDO log files that need backup
- Index files with different naming patterns
- Files that shouldn't have PostgreSQL page checksum validation applied

Without externalizing the inclusion list, OrioleDB users cannot get proper backups of their data.

## Proposed Solution

### Option 1: Configuration-Based Inclusion Pattern (Recommended)

Add a new configuration option to specify additional relation file patterns.

#### New Configuration Option

```ini
# pgbackrest.conf
[global]
relation-pattern=_oriole
relation-pattern=_undo
relation-pattern=_custom_fork
```

#### Implementation

```c
// Extended pattern building
#define RELATION_EXP_BASE    "[0-9]+"
#define RELATION_EXP_DEFAULT "(_(fsm|vm))"

// Build dynamic pattern from config
String *buildRelationExp(const StringList *additionalForks) {
    String *forkPattern = strNewZ("(_(fsm|vm");

    for (unsigned int i = 0; i < strLstSize(additionalForks); i++) {
        strCatFmt(forkPattern, "|%s", strZ(strLstGet(additionalForks, i)));
    }

    strCatZ(forkPattern, ")){0,1}(\\.[0-9]+){0,1}");
    return strNewFmt(RELATION_EXP_BASE "%s", strZ(forkPattern));
}
```

#### Pros
- Backward compatible (default behavior unchanged)
- Simple to configure
- Minimal code changes
- Users can add TAM-specific patterns without code changes

#### Cons
- Limited to fork-style patterns
- May not handle completely different naming schemes

### Option 2: Full Pattern Override

Allow complete replacement of the relation pattern.

#### Configuration

```ini
[global]
# Override entire relation pattern (advanced)
relation-pattern-override=[0-9]+(_(fsm|vm|oriole|undo)){0,1}(\.[0-9]+){0,1}
```

#### Pros
- Maximum flexibility
- Supports any naming convention

#### Cons
- Risk of misconfiguration breaking backups
- Users must understand regex
- Harder to validate correctness

### Option 3: TAM Plugin System

Create a plugin interface for TAM-specific handling.

#### Plugin Interface

```c
typedef struct TamPlugin {
    const char *name;                           // e.g., "orioledb"
    const char *relationPattern;                // File matching pattern
    const char *unloggedMarker;                 // e.g., "_init" or "_oriole_init"
    bool (*validatePage)(const Buffer *page);   // Custom page validation
    const char **excludePatterns;               // Files to exclude
} TamPlugin;
```

#### Configuration

```ini
[global]
tam-plugin=/usr/lib/pgbackrest/orioledb.so
```

#### Pros
- Most flexible solution
- Clean separation of concerns
- TAM maintainers can provide their own plugins

#### Cons
- Significant implementation effort
- Plugin ABI stability concerns
- Deployment complexity

### Option 4: Pattern Include File

Read additional patterns from an external file.

#### Configuration

```ini
[global]
relation-pattern-file=/etc/pgbackrest/relation-patterns.conf
```

#### Pattern File Format

```
# /etc/pgbackrest/relation-patterns.conf
# Additional fork types to include in backups
fork:_oriole
fork:_undo
fork:_custom

# Additional file patterns (full regex)
pattern:oriole_[0-9]+\.dat

# Exclude from page checksum validation
no-checksum:_oriole
no-checksum:_undo
```

#### Pros
- Flexible without code changes
- Easy to version control patterns
- Can be shared across configurations

#### Cons
- Another file to manage
- Parsing complexity

## Recommendation

We recommend **Option 1 (Configuration-Based Inclusion Pattern)** as the initial implementation, with the following considerations:

1. **Start simple**: Add `relation-fork` option for additional fork types
2. **Provide escape hatch**: Add `relation-pattern-override` for advanced use cases
3. **Page checksum control**: Add `relation-no-checksum` for forks that shouldn't be validated

### Proposed Configuration Options

| Option | Type | Description |
|--------|------|-------------|
| `relation-fork` | list | Additional fork suffixes to recognize (e.g., `oriole`, `undo`) |
| `relation-pattern` | list | Additional complete file patterns (advanced) |
| `relation-no-checksum` | list | Fork types to exclude from page checksum validation |

### Example Configuration

```ini
[global]
# For OrioleDB support
relation-fork=oriole
relation-fork=undo

# Disable page checksum for OrioleDB files (uses different format)
relation-no-checksum=oriole
relation-no-checksum=undo
```

## Implementation Plan

### Phase 1: Core Pattern Externalization

1. Add `relation-fork` configuration option
2. Modify `RELATION_EXP` building to be dynamic
3. Update `manifestNewBuild()` to accept additional fork patterns
4. Add tests for custom fork patterns

### Phase 2: Page Checksum Control

1. Add `relation-no-checksum` configuration option
2. Modify page checksum filter to respect exclusions
3. Update manifest to track checksum-excluded files
4. Add tests for checksum exclusion

### Phase 3: Documentation and Validation

1. Document new options in help.xml
2. Add user guide section for TAM support
3. Add validation for pattern syntax
4. Provide example configurations for known TAMs

## Compatibility Considerations

### Backward Compatibility

- Default behavior remains unchanged
- Existing configurations continue to work
- New options are purely additive

### Forward Compatibility

- Backups with custom patterns can be restored on older pgBackRest versions (files are still backed up, just not specially handled)
- Pattern configuration is stored in backup manifest for reference

### PostgreSQL Version Compatibility

- TAM API available from PostgreSQL 12+
- Feature should be available regardless of PostgreSQL version
- Documentation should note TAM API version requirements

## Security Considerations

1. **Pattern validation**: Validate regex patterns to prevent ReDoS attacks
2. **File access**: Patterns should not allow access outside PGDATA
3. **Configuration trust**: Pattern files should have restricted permissions

## Testing Strategy

1. **Unit tests**: Pattern matching with various fork types
2. **Integration tests**: Backup/restore with custom patterns
3. **Performance tests**: Ensure pattern matching doesn't regress
4. **Edge cases**: Invalid patterns, empty lists, overlapping patterns

## Open Questions

1. Should we provide official patterns for known TAMs (OrioleDB, Zedstore)?
2. How should we handle TAMs that use completely different directory structures?
3. Should pattern configuration be per-stanza or global?
4. How do we validate that a TAM backup is complete/consistent?

## References

- [PostgreSQL Table Access Method API](https://www.postgresql.org/docs/current/tableam.html)
- [OrioleDB](https://github.com/orioledb/orioledb)
- [pgBackRest Manifest Implementation](src/info/manifest.c)
- [pgBackRest Page Checksum Handling](src/command/backup/pageChecksum.c)

## Feedback Requested

We welcome feedback on:

1. **Preferred approach**: Which option best balances flexibility and simplicity?
2. **Configuration naming**: Are the proposed option names clear?
3. **Missing use cases**: Are there TAM requirements we haven't considered?
4. **Implementation priority**: Which phases are most critical?

Please provide comments and suggestions on this RFC.
