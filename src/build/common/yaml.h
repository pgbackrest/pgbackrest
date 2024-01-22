/***********************************************************************************************************************************
Yaml Handler
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_YAML_H
#define COMMON_TYPE_YAML_H

#include <limits.h>

/***********************************************************************************************************************************
Yaml object
***********************************************************************************************************************************/
typedef struct Yaml Yaml;

#include "common/memContext.h"
#include "common/type/buffer.h"
#include "common/type/object.h"
#include "common/type/stringId.h"

/***********************************************************************************************************************************
Yaml event type
***********************************************************************************************************************************/
typedef enum
{
    yamlEventTypeNone = STRID5("none", 0x2b9ee0),
    yamlEventTypeStreamBegin = STRID5("stream-begin", 0x724e516da12ca930),
    yamlEventTypeStreamEnd = STRID5("stream-end", 0x8e2eda12ca930),
    yamlEventTypeDocBegin = STRID5("doc-begin", 0xe49ca2d8de40),
    yamlEventTypeDocEnd = STRID5("doc-end", 0x11c5d8de40),
    yamlEventTypeAlias = STRID5("alias", 0x130a5810),
    yamlEventTypeScalar = STRID5("scalar", 0x241604730),
    yamlEventTypeSeqBegin = STRID5("seq-begin", 0xe49ca2dc4b30),
    yamlEventTypeSeqEnd = STRID5("seq-end", 0x11c5dc4b30),
    yamlEventTypeMapBegin = STRID5("map-begin", 0xe49ca2dc02d0),
    yamlEventTypeMapEnd = STRID5("map-end", 0x11c5dc02d0),
} YamlEventType;

/***********************************************************************************************************************************
Yaml event
***********************************************************************************************************************************/
typedef struct YamlEvent
{
    YamlEventType type;                                             // Type (e.g. scalar)
    const String *value;                                            // Value, when type is scalar
    size_t line;                                                    // Parse line
    size_t column;                                                  // Parse column
} YamlEvent;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
Yaml *yamlNew(const Buffer *const buffer);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Get next event from doc
YamlEvent yamlEventNext(Yaml *this);

// Get next event from doc and check the type
YamlEvent yamlEventNextCheck(Yaml *this, YamlEventType type);

// Check the event type
YamlEvent yamlEventCheck(YamlEvent event, YamlEventType type);

// Peek at the next event
YamlEvent yamlEventPeek(Yaml *this);

// Get next scalar
FN_INLINE_ALWAYS YamlEvent
yamlScalarNext(Yaml *const this)
{
    return yamlEventNextCheck(this, yamlEventTypeScalar);
}

// Check scalar
void yamlScalarCheck(YamlEvent event, const String *value);

FN_INLINE_ALWAYS void
yamlScalarCheckZ(const YamlEvent event, const char *const value)
{
    yamlScalarCheck(event, STR(value));
}

void yamlScalarNextCheck(Yaml *this, const String *value);

FN_INLINE_ALWAYS void
yamlScalarNextCheckZ(Yaml *const this, const char *const value)
{
    yamlScalarNextCheck(this, STR(value));
}

// Convert an event to a boolean (or error)
bool yamlBoolParse(YamlEvent event);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
yamlFree(Yaml *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Helper macros for iterating sequences and maps
***********************************************************************************************************************************/
#define YAML_ITER_BEGIN(yamlParam, eventBegin)                                                                                     \
    do                                                                                                                             \
    {                                                                                                                              \
        Yaml *const YAML_ITER_yaml = yaml;                                                                                         \
        yamlEventNextCheck(YAML_ITER_yaml, eventBegin);                                                                            \
                                                                                                                                   \
        do                                                                                                                         \
        {

#define YAML_ITER_END(eventEnd)                                                                                                    \
        }                                                                                                                          \
        while (yamlEventPeek(YAML_ITER_yaml).type != eventEnd);                                                                    \
                                                                                                                                   \
        yamlEventNextCheck(YAML_ITER_yaml, eventEnd);                                                                              \
    }                                                                                                                              \
    while (0)

#define YAML_SEQ_BEGIN(yaml)                                                                                                       \
    YAML_ITER_BEGIN(yaml, yamlEventTypeSeqBegin)

#define YAML_SEQ_END()                                                                                                             \
    YAML_ITER_END(yamlEventTypeSeqEnd);

#define YAML_MAP_BEGIN(yaml)                                                                                                       \
    YAML_ITER_BEGIN(yaml, yamlEventTypeMapBegin)

#define YAML_MAP_END()                                                                                                             \
    YAML_ITER_END(yamlEventTypeMapEnd);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_YAML_TYPE                                                                                                     \
    Yaml *
#define FUNCTION_LOG_YAML_FORMAT(value, buffer, bufferSize)                                                                        \
    objNameToLog(value, "Yaml", buffer, bufferSize)

#define FUNCTION_LOG_YAML_EVENT_TYPE                                                                                               \
    YamlEvent
#define FUNCTION_LOG_YAML_EVENT_FORMAT(value, buffer, bufferSize)                                                                  \
    objNameToLog(&value, "YamlEvent", buffer, bufferSize)

#endif
