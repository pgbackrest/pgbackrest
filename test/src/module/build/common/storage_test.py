"""Test File System Helpers.

The error other than missing is provoked with a path where a file is expected, since that is reachable without changing permissions
and the tests must behave the same when run as root."""

####################################################################################################################################
import os
import tempfile

from harness.test import *

from common.error import *
from common.storage import *


####################################################################################################################################
def test_file_read_write():
    """A file that is written is read back and parent paths are created as needed."""

    with tempfile.TemporaryDirectory() as path:
        path_file = os.path.join(path, "sub/dir/file.txt")

        file_write(path_file, "content")

        assert_equal(file_read(path_file), "content")
        assert_true(os.path.isdir(os.path.join(path, "sub/dir")))

        # An existing file is replaced
        file_write(path_file, "replaced")

        assert_equal(file_read(path_file), "replaced")

        # Bytes are written as bytes, which is how the raw help data is written for the C test to load
        file_write(path_file, b"\x00\x01raw")

        with open(path_file, "rb") as file:
            assert_equal(file.read(), b"\x00\x01raw")


####################################################################################################################################
def test_file_read_error():
    """A file that cannot be read is an error unless it is merely missing and that is allowed."""

    with tempfile.TemporaryDirectory() as path:
        path_file = os.path.join(path, "missing.txt")

        with assert_raises(ToolError) as error:
            file_read(path_file)

        assert_equal(str(error.exception), "unable to open file '%s' for read" % path_file)

        # Missing is reported by returning nothing when the caller expects it, e.g. a generated file that is not there yet
        assert_is_none(file_read(path_file, ignore_missing=True))

        # Anything else reports what the system said, and is an error however the caller asked for it
        with assert_raises(ToolError) as error:
            file_read(path, ignore_missing=True)

        assert_equal(str(error.exception), "unable to open file '%s' for read: Is a directory" % path)


####################################################################################################################################
def test_file_write_error():
    """A file that cannot be written reports what the system said."""

    with tempfile.TemporaryDirectory() as path:
        with assert_raises(ToolError) as error:
            file_write(path, "content")

        assert_equal(str(error.exception), "unable to open file '%s' for write: Is a directory" % path)


####################################################################################################################################
def test_file_write_differs():
    """A file is written only when the content differs so an unchanged file does not trigger a rebuild."""

    with tempfile.TemporaryDirectory() as path:
        path_file = os.path.join(path, "file.txt")

        # Missing counts as different
        file_write_differs(path_file, "content")

        assert_equal(file_read(path_file), "content")

        # Content that matches leaves the file alone, which only the timestamp can show since the content is the same either way
        os.utime(path_file, (0, 0))
        file_write_differs(path_file, "content")

        assert_equal(os.stat(path_file).st_mtime, 0)

        # Content that differs is written
        file_write_differs(path_file, "changed")

        assert_equal(file_read(path_file), "changed")
        assert_not_equal(os.stat(path_file).st_mtime, 0)


####################################################################################################################################
def test_file_remove():
    """A file is removed and missing is only an error when the caller asks for it."""

    with tempfile.TemporaryDirectory() as path:
        path_file = os.path.join(path, "file.txt")

        file_write(path_file, "content")
        file_remove(path_file)

        assert_false(os.path.exists(path_file))

        # Missing is not an error by default since a file that is gone is what the caller wanted
        assert_is_none(file_remove(path_file))

        with assert_raises(ToolError) as error:
            file_remove(path_file, error_on_missing=True)

        assert_equal(str(error.exception), "unable to remove missing file '%s'" % path_file)

        # Anything else reports what the system said
        with assert_raises(ToolError) as error:
            file_remove(path)

        assert_equal(str(error.exception), "unable to remove file '%s': Is a directory" % path)


####################################################################################################################################
def test_path_create():
    """Missing parents are created and an existing path is not an error."""

    with tempfile.TemporaryDirectory() as path:
        path_sub = os.path.join(path, "sub/dir")

        path_create(path_sub)

        assert_true(os.path.isdir(path_sub))
        assert_is_none(path_create(path_sub))

        # An empty path is where creation stops, e.g. a file written with no path in the name
        assert_is_none(path_create(""))

        # A path that cannot be created reports what the system said
        path_file = os.path.join(path, "file.txt")
        file_write(path_file, "content")

        with assert_raises(ToolError) as error:
            path_create(os.path.join(path_file, "sub"))

        assert_equal(str(error.exception), "unable to create path '%s': Not a directory" % os.path.join(path_file, "sub"))


####################################################################################################################################
def test_path_list():
    """Names are listed sorted and can be filtered."""

    with tempfile.TemporaryDirectory() as path:
        for name in ("c.txt", "a.txt", "b.json"):
            file_write(os.path.join(path, name), "content")

        path_create(os.path.join(path, "sub"))

        # Paths are listed along with files
        assert_equal(path_list(path), ["a.txt", "b.json", "c.txt", "sub"])
        assert_equal(path_list(path, expression=r"\.txt$"), ["a.txt", "c.txt"])

        # A missing path is empty rather than an error, since a path listed here is often legitimately absent
        path_missing = os.path.join(path, "missing")

        assert_equal(path_list(path_missing), [])

        with assert_raises(ToolError) as error:
            path_list(path_missing, error_on_missing=True)

        assert_equal(str(error.exception), "unable to list missing path '%s'" % path_missing)

        # Anything else reports what the system said
        path_file = os.path.join(path, "a.txt")

        with assert_raises(ToolError) as error:
            path_list(path_file)

        assert_equal(str(error.exception), "unable to list path '%s': Not a directory" % path_file)


####################################################################################################################################
def test_path_list_recurse():
    """Files are listed recursively and relative to the path, with the paths themselves left out."""

    with tempfile.TemporaryDirectory() as path:
        file_write(os.path.join(path, "b.txt"), "content")
        file_write(os.path.join(path, "sub/a.txt"), "content")
        file_write(os.path.join(path, "sub/deep/c.txt"), "content")
        path_create(os.path.join(path, "empty"))

        assert_equal(path_list_recurse(path), ["b.txt", "sub/a.txt", "sub/deep/c.txt"])
