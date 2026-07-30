"""Test Yaml Handler.

What matters here is what the loader does differently from a normal yaml load, since the generators depend on all three: scalars stay
text, mappings keep their order, and a repeated key keeps both values."""

####################################################################################################################################
from harness.test import *

from common.error import *
from common.yaml import *


####################################################################################################################################
def test_yaml_load():
    """Every scalar is loaded as the text it was written as."""

    value = yaml_load("version: 9.6\ncode: 011\nfatal: true\nempty:\n", "test.yaml")

    assert_equal([(key, value) for key, value in value], [("version", "9.6"), ("code", "011"), ("fatal", "true"), ("empty", "")])

    # A sequence is a list and a mapping is not, so a range written as a pair can be told from a range written per map
    value = yaml_load("range: [1, 2]\nmap:\n  - key: value\n", "test.yaml")
    pair_list = list(value)

    assert_equal(pair_list[0][1], ["1", "2"])
    assert_is_instance(pair_list[1][1][0], YamlMap)


####################################################################################################################################
def test_yaml_load_order():
    """A mapping keeps the order it was written in and every one of a repeated key."""

    value = yaml_load("b: 1\na: 2\nb: 3\n", "test.yaml")

    assert_equal([(key, value) for key, value in value], [("b", "1"), ("a", "2"), ("b", "3")])
    assert_equal(len(value), 3)


####################################################################################################################################
def test_yaml_load_error():
    """A file that is not yaml is reported with the name of the file, since the error is otherwise only a line number."""

    with assert_raises(ToolError) as error:
        yaml_load("key: [\n", "test.yaml")

    assert_in("unable to parse 'test.yaml':", str(error.exception))


####################################################################################################################################
def test_yaml_bool():
    """A boolean must be written as true or false rather than any of the other spellings yaml allows."""

    assert_true(yaml_bool("true", "key"))
    assert_false(yaml_bool("false", "key"))

    with assert_raises(ToolError) as error:
        yaml_bool("yes", "key")

    assert_equal(str(error.exception), "invalid boolean 'yes' for key")


####################################################################################################################################
def test_yaml_map_empty():
    """A key that is a name in a list has an empty map under it and nothing else."""

    yaml_map_empty(list(yaml_load("key: {}\n", "test.yaml"))[0][1], "key")

    # A key with nothing under it is a scalar rather than an empty map, so it does not count
    for content in ("key:\n", "key: value\n", "key:\n  other: value\n"):
        with assert_raises(ToolError) as error:
            yaml_map_empty(list(yaml_load(content, "test.yaml"))[0][1], "key")

        assert_equal(str(error.exception), "key must be an empty map")
