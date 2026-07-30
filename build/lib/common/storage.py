"""File System Helpers.

Thin wrappers over the standard library that report the affected path in the error, since a bare OSError from deep in a generated
build says nothing about which file the tool was working on."""

####################################################################################################################################
import os
import re

from common.error import ToolError


####################################################################################################################################
def file_read(path, ignore_missing=False):
    """Read a file as text.

    Return None instead of raising when the file is missing and ignore_missing is set."""

    try:
        with open(path, "r") as file:
            return file.read()
    except FileNotFoundError:
        if ignore_missing:
            return None

        raise ToolError("unable to open file '%s' for read" % path)
    except OSError as error:
        raise ToolError("unable to open file '%s' for read: %s" % (path, error.strerror))


####################################################################################################################################
def file_write(path, content):
    """Write a file as text, or as bytes when that is what was given, creating parent paths as needed."""

    path_create(os.path.dirname(path))

    try:
        with open(path, "wb" if isinstance(content, bytes) else "w") as file:
            file.write(content)
    except OSError as error:
        raise ToolError("unable to open file '%s' for write: %s" % (path, error.strerror))


####################################################################################################################################
def file_write_differs(path, content):
    """Write a file only when the content differs.

    An unchanged generated file then keeps its timestamp and does not trigger a rebuild."""

    if file_read(path, ignore_missing=True) != content:
        file_write(path, content)


####################################################################################################################################
def file_remove(path, error_on_missing=False):
    """Remove a file.

    Missing is not an error unless error_on_missing is set."""

    try:
        os.remove(path)
    except FileNotFoundError:
        if error_on_missing:
            raise ToolError("unable to remove missing file '%s'" % path)
    except OSError as error:
        raise ToolError("unable to remove file '%s': %s" % (path, error.strerror))


####################################################################################################################################
def path_create(path, mode=0o750):
    """Create a path and any missing parents.

    An existing path is not an error."""

    if path == "" or os.path.isdir(path):
        return

    try:
        os.makedirs(path, mode=mode, exist_ok=True)
    except OSError as error:
        raise ToolError("unable to create path '%s': %s" % (path, error.strerror))


####################################################################################################################################
def path_list(path, expression=None, error_on_missing=False):
    """List the names in a path, sorted, optionally filtered by a regular expression.

    A missing path returns an empty list unless error_on_missing is set, since a path listed here is often legitimately absent."""

    try:
        result = sorted(os.listdir(path))
    except FileNotFoundError:
        if error_on_missing:
            raise ToolError("unable to list missing path '%s'" % path)

        return []
    except OSError as error:
        raise ToolError("unable to list path '%s': %s" % (path, error.strerror))

    if expression is not None:
        regexp = re.compile(expression)
        result = [name for name in result if regexp.search(name)]

    return result


####################################################################################################################################
def path_list_recurse(path):
    """List files in a path recursively, relative to the path and sorted.

    Directories are not included."""

    result = []

    for root, pathList, fileList in os.walk(path):
        pathList.sort()

        for name in sorted(fileList):
            result.append(os.path.relpath(os.path.join(root, name), path))

    return sorted(result)
