"""Parse PostgreSQL Interface Declaration.

The versions come from postgres.yaml, but the types, defines, and functions that make up an interface are read out of the vendored
PostgreSQL headers rather than declared. A version interface is the same headers compiled again with different names, so scanning
them is what keeps the generated code in step with a header update.

Two interfaces are generated from the same declaration: the one the binary carries and the one the test harness carries. They are
the same versions and the same vendored types, so all that differs is where their macros are declared and what the functions those
macros define are called.

The vendored header is also read as a set of version branches, which is what says whether two versions would compile a function to
the same code. A function is rendered once for each distinct answer rather than once per version."""

####################################################################################################################################
import os
import re

from common.error import ToolError
from common.storage import file_read
from common.yaml import yaml_bool, yaml_load

# Defines that every interface undefines but that no vendored header declares, so they are added to the list explicitly. Neither is
# branched by the vendored header, so neither resolves to a branch of it: PG_VERSION is the version itself, and
# CATALOG_VERSION_NO_MAX is defined only for a version that has not been released.
_DEFINE_VERSION = "PG_VERSION"
_DEFINE_CATALOG_MAX = "CATALOG_VERSION_NO_MAX"
_DEFINE_EXTRA_LIST = (_DEFINE_CATALOG_MAX, _DEFINE_VERSION)

# A name in C, and the comments that are removed before looking for one so that a comment naming a type does not read as a use of it
_NAME = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
_COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)

# Comments the vendored header divides itself with, one before each entity and one before each section of related entities
_BLOCK_SEPARATOR = re.compile(r"^// -{50,}$")
_BLOCK_BANNER = re.compile(r"^/\*{50,}$")

# Branch that no version takes, which is the one guarding against a version newer than the maximum, and the branch an entity
# resolves to for a version it does not exist for
_BRANCH_MAX = object()
_BRANCH_ABSENT = object()


####################################################################################################################################
def bld_pg_version_num(version):
    """The version as the number the C compares, e.g. "9.6" becomes 90600 and "10" becomes 100000."""

    major, _, minor = version.partition(".")

    return int(major) * 10000 + (int(minor) * 100 if minor != "" else 0)


####################################################################################################################################
class BldPgVersion:
    """A supported PostgreSQL version."""

    def __init__(self, version, release):
        self.version = version
        self.release = release  # Has the version been released?


####################################################################################################################################
class BldPgEntity:
    """A type or define of the vendored header and the version branches it is declared in."""

    def __init__(self, branch_map, version_map):
        self.branch_map = branch_map  # Body the entity has under each branch, keyed by the version the branch begins at
        self.version_map = version_map  # Branch each version resolves to, or _BRANCH_ABSENT for a version it is missing from


####################################################################################################################################
class BldPgInterface:
    """An interface that is generated from the vendored headers."""

    def __init__(self, module, description, path_intern, include, prefix, path_render):
        self.module = module  # Generator that renders it, which the generated file names as what wrote it
        self.description = description  # What the generated file says it is
        self.path_intern = path_intern  # Header the interface macros are declared in
        self.include = include  # How the generated code includes that header
        self.prefix = prefix  # Prefix of the functions the macros define, e.g. "pgInterface"
        self.path_render = path_render  # Where the interface is rendered

    @property
    def type(self):
        """The struct type the interface functions are collected in, e.g. "pgInterface" becomes "PgInterface"."""

        return self.prefix[:1].upper() + self.prefix[1:]


# The interface the binary carries, which is the default since it is the one the build generates
BLD_PG_INTERFACE = BldPgInterface(
    "postgres",
    "PostgreSQL Interface",
    "src/postgres/interface/version.intern.h",
    "postgres/interface/version.intern.h",
    "pgInterface",
    "src/postgres/interface.auto.c.inc",
)

# The interface the test harness carries, which writes the files a test needs rather than reading the ones PostgreSQL wrote
BLD_PG_INTERFACE_HARNESS = BldPgInterface(
    "postgres-harness",
    "PostgreSQL Interface Harness",
    "test/src/harness/postgres/version.intern.h",
    "harness/postgres/version.intern.h",
    "hrnPgInterface",
    "test/src/harness/postgres/interface.auto.c.inc",
)


####################################################################################################################################
class BldPg:
    """The PostgreSQL interface declaration."""

    def __init__(self, pg_list, type_list, define_list, function_list, function_version, interface):
        self.pg_list = pg_list  # Supported versions, oldest first
        self.type_list = type_list  # Interface types, sorted
        self.define_list = define_list  # Interface defines, sorted
        self.function_list = function_list  # Functions defined by macros, in the order they are declared
        self.function_version = function_version  # Version whose rendering of a function each version uses, by function
        self.interface = interface  # Interface the functions were read for, which is the one that gets rendered


####################################################################################################################################
def bld_pg_version_list(path_repo):
    """Parse the supported versions, oldest first.

    Separate from the interface below because the versions are the whole of what the declaration says, so a tool that needs to know
    which versions are supported does not also scan the vendored headers for an interface it has no use for."""

    path = os.path.join(path_repo, "build/postgres.yaml")
    result = []

    for key, value in yaml_load(file_read(path), path):
        if key != "version":
            raise ToolError("unknown postgres definition '%s'" % key)

        for version in value:
            # A scalar is the version on its own, else a map naming the version and its attributes
            if isinstance(version, str):
                result.append(BldPgVersion(version, True))

                continue

            for name, detail in version:
                release = True

                for def_key, def_value in detail:
                    if def_key != "release":
                        raise ToolError("unknown postgres definition '%s'" % def_key)

                    release = yaml_bool(def_value, "version '%s' release" % name)

                result.append(BldPgVersion(name, release))

    return result


####################################################################################################################################
def _define_name(line):
    """The name a #define line declares, or None when the line does not declare one."""

    line = line.strip()

    if not line.startswith("#define"):
        return None

    token = line.split(" ")[1].strip()

    if token == "":
        raise ToolError("unable to find define -- are there extra spaces on '%s'" % line)

    # The define name may be followed by a parameter list or separated from its value by a tab
    define = token.split("(")[0] if "(" in token else token.split("\t")[0]

    return define.strip()


####################################################################################################################################
def _define_list(header):
    """Scan the defines out of a header."""

    result = []

    for line in header.split("\n"):
        define = _define_name(line)

        if define is not None and define not in result:
            result.append(define)

    return result


####################################################################################################################################
def _function_list(header):
    """Scan the interface functions out of a header, which are the macros taking the version to render one for.

    A macro that does not take a version is a helper the function macros are written with rather than a function the interface has,
    so it is not rendered. It is still followed when working out what a function depends on, since a type it names is a type the
    function that uses it reaches."""

    result = []

    for line in header.split("\n"):
        define = _define_name(line)

        if define is None or not line.strip()[len("#define ") + len(define) :].startswith("(version)"):
            continue

        if define not in result:
            result.append(define)

    return result


####################################################################################################################################
def _type_list(line_list):
    """Scan the types out of the lines of a header.

    A typedef of a struct or an enum names the type after the block rather than before it, so the scan carries on to the closing
    brace, collecting the values of an enum on the way since each is a name the interface has to undefine."""

    result = []
    scan_enum = False

    def add(value):
        if value not in result:
            result.append(value)

    for line in line_list:
        token_list = line.strip().split(" ")

        if token_list[0] == "typedef":
            # A struct or an enum is named at the end of the block, so keep scanning
            if token_list[1] in ("struct", "enum"):
                scan_enum = token_list[1] == "enum"
            else:
                add(token_list[-1].split(";")[0])
        elif token_list[0] == "}":
            add(token_list[-1].split(";")[0])
            scan_enum = False
        elif scan_enum and token_list[0] != "{":
            add(token_list[0].split(",")[0])

    return result


####################################################################################################################################
def _block_list(header):
    """Split the vendored header into the block it devotes to each entity, which it already separates them with a comment for.

    A separator that is missing merges two entities into a single block, which gives each of them the branches of both. That splits
    versions that could have shared rather than sharing versions that could not, so a header with no separators at all is read as
    one block and nothing is shared."""

    result = []
    line_list = header.split("\n")
    idx = 0

    while idx < len(line_list):
        if not _BLOCK_SEPARATOR.match(line_list[idx]):
            idx += 1
            continue

        idx += 1
        block = []

        while idx < len(line_list) and not _BLOCK_SEPARATOR.match(line_list[idx]) and not _BLOCK_BANNER.match(line_list[idx]):
            block.append(line_list[idx])
            idx += 1

        # The name comment of the next entity trails the block, so drop it
        while block and block[-1].strip().startswith("//"):
            block.pop()

        result.append(block)

    return result if result != [] else [line_list]


####################################################################################################################################
def _branch_map(block):
    """Split a block into the body it has under each branch, keyed by the version the branch begins at.

    A block declaring an entity that never varies has one body under None, which is the body every version gets."""

    result = {}
    branch = None

    for line in block:
        line_strip = line.strip()

        if line_strip.startswith("#if") and "PG_VERSION" in line_strip:
            branch = _BRANCH_MAX
        elif line_strip.startswith("#elif") and "PG_VERSION >= PG_VERSION_" in line_strip:
            branch = line_strip.split("PG_VERSION_")[-1].strip()
            branch = "9.6" if branch == "96" else branch
        elif line_strip.startswith("#endif"):
            branch = None
        else:
            result.setdefault(branch, []).append(line)

    result = {key: "\n".join(value) for key, value in result.items() if key is not _BRANCH_MAX}

    # The blank lines around a branch chain fall outside it, so an unbranched body only counts when there is nothing else
    if len(result) > 1:
        result.pop(None, None)

    return result


####################################################################################################################################
def _version_map(pg_list, branch_map):
    """The branch each version resolves to, which is the newest branch that is not newer than the version."""

    result = {}

    for pg in pg_list:
        # An entity that never varies is the same for every version
        if None in branch_map:
            result[pg.version] = None

            continue

        branch_best = _BRANCH_ABSENT

        for branch in branch_map:
            if bld_pg_version_num(branch) <= bld_pg_version_num(pg.version) and (
                branch_best is _BRANCH_ABSENT or bld_pg_version_num(branch) > bld_pg_version_num(branch_best)
            ):
                branch_best = branch

        result[pg.version] = branch_best

    return result


####################################################################################################################################
def _entity_map(header, pg_list):
    """Scan the vendored header into the entity each type and define is, which is the branches it varies by."""

    result = {}

    for block in _block_list(header):
        branch_map = _branch_map(block)
        entity = BldPgEntity(branch_map, _version_map(pg_list, branch_map))

        for name in _type_list(block) + _define_list("\n".join(block)):
            result[name] = entity

    return result


####################################################################################################################################
def _macro_map(header):
    """Scan an interface header into the text of each macro it declares.

    A macro declared inside a conditional gets the names the condition tests, since which of them is rendered depends on them. A
    macro declared more than once gets the text of every declaration, since any of them may be the one that is rendered."""

    result = {}
    condition = []
    name = None

    for line in header.split("\n"):
        line_strip = line.strip()

        # A macro body is continued with a backslash, so keep adding to it until a line does not end with one
        if name is not None:
            result[name] += "\n" + line

            if not line_strip.endswith("\\"):
                name = None

            continue

        if line_strip.startswith("#endif"):
            condition = condition[:-1]
        elif line_strip.startswith("#if"):
            condition.append(" ".join(_NAME.findall(line_strip)[1:]))
        elif _define_name(line) is not None:
            name = _define_name(line)
            result[name] = result.get(name, "") + "\n" + " ".join(condition) + "\n" + line

            if not line_strip.endswith("\\"):
                name = None

    return result


####################################################################################################################################
def _dep_list(text, name_set, macro_map, seen):
    """Names of the name set that a body reaches, following the macros of its own header so a helper does not hide a dependency."""

    result = set()

    for name in _NAME.findall(_COMMENT.sub(" ", text)):
        if name in macro_map:
            if name not in seen:
                seen.add(name)
                result |= _dep_list(macro_map[name], name_set, macro_map, seen)
        elif name in name_set:
            result.add(name)

    return result


####################################################################################################################################
def _closure(entity_map, name, version, seen):
    """Every entity a name depends on at a version, as the name and branch pairs that have to match for a function to be shared.

    A type that is unchanged may still be laid out differently because a type embedded in it changed, so following the entities a
    body reaches is what makes the comparison sound. CheckPoint changing inside an unchanged ControlFileData is the example."""

    if name in seen:
        return set()

    seen.add(name)
    branch = entity_map[name].version_map[version]
    result = {(name, branch)}

    if branch is not _BRANCH_ABSENT:
        for dep in _dep_list(entity_map[name].branch_map[branch], set(entity_map), {}, set()):
            if dep != name:
                result |= _closure(entity_map, dep, version, seen)

    return result


####################################################################################################################################
def _function_version(pg_list, entity_map, macro_map, function_list):
    """The version whose rendering of a function each version uses.

    Two versions share a rendering when every type and define the function reaches resolves to the same branch for both, since that
    is what makes the code the compiler sees the same. The oldest of them renders it, so adding a version later never renames what
    is already there."""

    name_set = set(entity_map) | {_DEFINE_VERSION, _DEFINE_CATALOG_MAX}
    result = {}

    for function in function_list:
        dep_list = _dep_list(macro_map[function], name_set, macro_map, {function})
        version_map = {}
        group = {}

        for pg in pg_list:
            signature = set()

            for dep in dep_list:
                # The two defines no vendored header declares vary by the version itself and by whether it has been released
                if dep == _DEFINE_VERSION:
                    signature.add((dep, pg.version))
                elif dep == _DEFINE_CATALOG_MAX:
                    signature.add((dep, pg.release))
                else:
                    signature |= _closure(entity_map, dep, pg.version, set())

            # The list is oldest first, so the first version with a signature is the one that renders it
            version_map[pg.version] = group.setdefault(frozenset(signature), pg.version)

        result[function] = version_map

    return result


####################################################################################################################################
def bld_pg_parse(path_repo, interface=BLD_PG_INTERFACE):
    """Parse the PostgreSQL interface declaration."""

    path_vendor = os.path.join(path_repo, "src/postgres/interface/version.vendor.h")
    header_vendor = file_read(path_vendor)
    header_intern = file_read(os.path.join(path_repo, interface.path_intern))
    pg_list = bld_pg_version_list(path_repo)

    # The interface is generated from the vendored header, so its types and defines are whatever that header declares
    type_list = sorted(_type_list(header_vendor.split("\n")))
    define_list = sorted(_define_list(header_vendor) + list(_DEFINE_EXTRA_LIST))

    # Functions are defined as macros, which each interface expands for its own version
    function_list = _function_list(header_intern)

    # A function only has to be expanded once for each set of branches it reaches, so work out which versions can share one
    function_version = _function_version(pg_list, _entity_map(header_vendor, pg_list), _macro_map(header_intern), function_list)

    return BldPg(pg_list, type_list, define_list, function_list, function_version, interface)
