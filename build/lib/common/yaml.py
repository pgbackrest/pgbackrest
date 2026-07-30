"""Yaml Handler.

Loads a declaration the way the generators need to read it, which is not the way yaml is usually loaded:

- Every scalar stays the text it was written as. Yaml would otherwise type a scalar by how it looks, so a version written as 9.6 would
  arrive as a float and render as 9.6000000000000005, and a code written as 011 would arrive as the octal 9.

- A mapping keeps its pairs rather than becoming a dict, because a key may be repeated and the repetition is meaningful. The option
  command list uses it to add several roles or exclude several commands, and a dict would silently keep only the last one.

- A mapping keeps the order it was written in, which yaml guarantees for sequences only. Order matters because it decides the order of
  the generated code, so the loader is what makes the declaration order the record."""

####################################################################################################################################
import yaml

from common.error import ToolError, check


####################################################################################################################################
class YamlMap:
    """A yaml mapping, as the list of key/value pairs it was written as."""

    def __init__(self, pair_list):
        self.pair_list = pair_list

    def __iter__(self):
        return iter(self.pair_list)

    def __len__(self):
        return len(self.pair_list)


####################################################################################################################################
class _Loader(yaml.SafeLoader):
    """A safe loader with no implicit typing and no dicts."""


####################################################################################################################################
def _construct_map(loader, node):
    """Construct a mapping as its pairs, deeply, so a nested collection is complete rather than left to be filled in later."""

    return YamlMap(
        [(loader.construct_object(key, deep=True), loader.construct_object(value, deep=True)) for key, value in node.value]
    )


# Removing the implicit resolvers leaves plain scalars with the default string tag. An explicitly tagged scalar, e.g. !!int 1, is
# still typed, and there is none of that in the declarations.
_Loader.yaml_implicit_resolvers = {}
_Loader.add_constructor("tag:yaml.org,2002:map", _construct_map)


####################################################################################################################################
def yaml_load(content, path):
    """Load yaml from text, naming the file in any error since the message is otherwise only a line number."""

    try:
        return yaml.load(content, Loader=_Loader)
    except yaml.YAMLError as error:
        raise ToolError("unable to parse '%s': %s" % (path, error))


####################################################################################################################################
def yaml_bool(value, name):
    """Parse a boolean, which must be written as true or false rather than any of the other spellings yaml allows."""

    check(value in ("true", "false"), "invalid boolean '%s' for %s" % (value, name))

    return value == "true"


####################################################################################################################################
def yaml_map_empty(value, name):
    """Check that a mapping is empty, i.e. the key is a name in a list rather than something with a definition."""

    check(isinstance(value, YamlMap) and len(value) == 0, "%s must be an empty map" % name)
