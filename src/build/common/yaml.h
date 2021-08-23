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
#include "common/type/object.h"
#include "common/type/buffer.h"
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
void yamlEventCheck(YamlEvent event, YamlEventType type);

// Convert an event to a boolean (or error)
bool yamlBoolParse(YamlEvent event);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
yamlFree(Yaml *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_YAML_TYPE                                                                                                     \
    Yaml *
#define FUNCTION_LOG_YAML_FORMAT(value, buffer, bufferSize)                                                                        \
    objToLog(value, "Yaml", buffer, bufferSize)

#define FUNCTION_LOG_YAML_EVENT_TYPE                                                                                               \
    YamlEvent
#define FUNCTION_LOG_YAML_EVENT_FORMAT(value, buffer, bufferSize)                                                                  \
    objToLog(&value, "YamlEvent", buffer, bufferSize)

#endif
