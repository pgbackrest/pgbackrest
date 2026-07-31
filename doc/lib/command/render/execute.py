"""Document Execution.

Runs what the user guide says to run. A document is a sequence of commands against hosts, so building it is running them in order and
keeping what they wrote, which is what a reader sees.

Running them takes long enough that the result is cached. A cache entry is keyed by everything that decides what the command would be,
so replaying the cache checks as it goes that the document still describes the same run. When a key no longer matches, the cache no
longer describes the document and the whole document is built again.

Configuration is handled here rather than in a renderer because a configuration file is built up across a document: a section adds an
option and a later section shows the file with everything added so far."""

####################################################################################################################################
import copy
import json
import os
import re

from common.error import ToolError
from common.exec import exec_result
from common.ini import ini_render
from common.log import *
from common.storage import file_read, file_write
from common.user import user_name
from common.xml import (
    xml_node_attribute,
    xml_node_child_list,
    xml_node_field,
)
from command.render.host import Host, HostGroup
from command.render.render import DocRender

# Where a command that is split over more than one line breaks, and what it looks like when it does
_CMD_SPLIT_EXP = re.compile(r"[ ]*\\?[ ]*\n[ ]*")
_CMD_CONTINUE = " \\\n    "

# How long a line of a command may be before it is too wide to read
_CMD_LINE_LEN = 80

# Options that are set so the build can watch what happens but are not part of what the documentation is showing
_CONFIG_SECTION_GLOBAL = "global"
_CONFIG_HIDE_LIST = ("log-level-stderr", "log-timestamp")

# Linefeeds around what a command wrote, which are how the output ends rather than part of it
_OUTPUT_TRIM_EXP = re.compile(r"^\n+|\n$")

# What is shown in place of output when the commands are not being run
_SUPPRESS_OUTPUT = "Output suppressed for testing"
_SUPPRESS_CONFIG = "Config suppressed for testing"


####################################################################################################################################
def doc_user():
    """The user building the documentation, which is the user a command runs as unless it says otherwise."""

    return "ubuntu" if user_name() == "root" else user_name()


####################################################################################################################################
class CacheInvalidError(ToolError):
    """The cache no longer describes the document, so the document has to be built again."""


####################################################################################################################################
class DocExecute(DocRender):
    """Renders a document, running the commands it holds."""

    def __init__(self, type, manifest, key, exe):
        super().__init__(type, manifest, exe, key)

        self.cache = self.source.cache_list is not None
        self.cache_idx = 0

        self.host_map = {}
        self.config_map = {}
        self.pg_config_map = {}

        self.cmd_line_len = int(xml_node_attribute(self.root, "cmd-line-len") or _CMD_LINE_LEN)

    ################################################################################################################################
    def _replace(self, string):
        """Replace every variable in a string."""

        return self.manifest.var_store.replace_str(string)

    ################################################################################################################################
    def execute_key(self, host_name, command):
        """Everything that decides what a command would be, which is what the cache is keyed by."""

        # A command runs as the user building the documentation unless it says otherwise, and a user it names is reached with sudo
        # unless the command is showing what running as that user looks like
        user_force = xml_node_attribute(command, "user-force") == "y"
        user = self._replace(xml_node_attribute(command, "user") or doc_user())
        cmd = self._replace(xml_node_field(command, "exe-cmd", True).strip())

        if user != doc_user() and not user_force:
            cmd = "sudo %s%s" % ("" if user == "root" else "-u %s " % user, cmd)

        # A command written over more than one line is shown that way, with a backslash where it breaks
        cmd = _CMD_SPLIT_EXP.sub(lambda _: _CMD_CONTINUE, cmd)
        cmd = cmd.replace(" \\@ \\", "")

        key = {
            "host": host_name,
            "cmd": cmd.split("\n"),
            "output": False,
            "run-as-user": user if user_force else None,
        }

        extra = xml_node_field(command, "exe-cmd-extra")

        if extra is not None:
            key["cmd-extra"] = self._replace(extra)

        err_expect = xml_node_attribute(command, "err-expect")

        if err_expect is not None:
            key["err-expect"] = err_expect

        # Output is kept when the documentation shows it or takes a variable from it
        if (
            xml_node_attribute(command, "output") == "y"
            or xml_node_attribute(command, "show") == "y"
            or xml_node_attribute(command, "variable-key") is not None
        ):
            key["output"] = True

        key["load-env"] = xml_node_attribute(command, "load-env") != "n"
        key["bash-wrap"] = xml_node_attribute(command, "bash-wrap") != "n"

        highlight = xml_node_field(command, "exe-highlight")

        if highlight is not None:
            key["output"] = True
            key["highlight"] = {
                "filter": xml_node_attribute(command, "filter") != "n",
                "filter-context": xml_node_attribute(command, "filter-context") or 2,
                "list": [self._replace(highlight)],
            }

        return key

    ################################################################################################################################
    def _output_filter(self, output, highlight, filter_context):
        """Keep the lines of the output that matter and say how many were dropped.

        A command can write a great deal and the documentation is showing one thing in it, so what is kept is the lines that match
        with a little either side."""

        line_list = output.split("\n")
        expression = re.compile(highlight)
        result = None
        last = -1

        def add(text):
            return text if result is None else result + "\n" + text

        for idx, line in enumerate(line_list):
            if expression.search(line) is None:
                continue

            # Do not go back past the start, or repeat lines that have already been kept
            first = max(idx - filter_context, 0)
            first = last + 1 if first <= last else first
            end = min(idx + filter_context, len(line_list) - 1)

            if first > last + 1:
                dropped = first - (last + 1)

                # One dropped line is not worth saying so, and takes as much room to say as to show
                if dropped > 1:
                    result = add("       [filtered %d lines of output]" % dropped)
                else:
                    first -= 1

            for keep in range(first, end + 1):
                result = add(line_list[keep])

            last = end

        dropped = (len(line_list) - 1) - (last + 1)

        if dropped > 0:
            result = add("       [filtered %d lines of output]" % dropped if dropped > 1 else line_list[-1])

        return result

    ################################################################################################################################
    def execute(self, section, host_name, command, indent=1, cache=True, show=True):
        """Run a command and return what was run and what it wrote."""

        key = self.execute_key(host_name, command)
        cmd = "\n".join(key["cmd"])
        output = None

        if show and self.exe and self.is_required(section):
            # A line that is too wide to read is an error, since the documentation is showing it to be read. A command may raise the
            # limit for itself when it holds something that cannot be broken, e.g. a url.
            line_len = int(xml_node_attribute(command, "cmd-line-len") or self.cmd_line_len)

            for line in cmd.split("\n"):
                if len(line.strip()) > line_len:
                    raise ToolError("command has a line > %d characters:\n%s\noffending line: %s" % (line_len, cmd, line))

        log(DEBUG, "    " * indent + "execute: %s" % cmd)

        if self._replace(xml_node_attribute(command, "skip") or "n") != "y":
            if self.exe and self.is_required(section):
                hit, cache_type, key, value = self.cache_pop("exe", key)

                if hit:
                    output = None if value.get("output") is None else "\n".join(value["output"])
                else:
                    output = self._execute_run(host_name, command, key, cmd)

                    if cache:
                        self.cache_push(cache_type, key, None if output is None else {"output": output.split("\n")})

                # A command the documentation takes a variable from sets it from what the command wrote
                if xml_node_attribute(command, "variable-key") is not None:
                    self.manifest.var_store.set(xml_node_attribute(command, "variable-key"), (output or "").strip())
            elif key["output"]:
                output = _SUPPRESS_OUTPUT

        # A variable the documentation takes from a command still has to have a value when the command was not run
        variable_key = xml_node_attribute(command, "variable-key")

        if variable_key is not None and self.manifest.var_store.get(variable_key) is None:
            self.manifest.var_store.set(variable_key, "[Test Variable]")

        return cmd, output

    ################################################################################################################################
    def _execute_run(self, host_name, command, key, cmd):
        """Run a command on a host and return what it wrote."""

        host = self.host_map.get(host_name)

        if host is None:
            raise ToolError("cannot execute on host %s because the host does not exist" % host_name)

        status, output, error = host.execute(
            cmd + ("" if key.get("cmd-extra") is None else " " + key["cmd-extra"]),
            user=key["run-as-user"],
            load_env=key["load-env"],
            bash_wrap=key["bash-wrap"],
            status_expect=int(key["err-expect"]) if key.get("err-expect") is not None else 0,
            suppress_error=xml_node_attribute(command, "err-suppress") == "y",
            retry=int(xml_node_attribute(command, "retry")) if xml_node_attribute(command, "retry") is not None else None,
        )

        result = _OUTPUT_TRIM_EXP.sub("", output) if output != "" else None

        # A command that is expected to fail is showing the error, so the error is part of what it wrote
        if key.get("err-expect") is not None and error != "":
            result = (result or "") + error

        if key["output"] and key.get("highlight") is not None and key["highlight"]["filter"] and result is not None:
            result = self._output_filter(result, key["highlight"]["list"][0], int(key["highlight"]["filter-context"]))

        return result if key["output"] else None

    ################################################################################################################################
    def config_key(self, config):
        """Everything that decides what a configuration change would be."""

        key = {
            "host": self._replace(xml_node_attribute(config, "host", True)),
            "file": self._replace(xml_node_attribute(config, "file", True)),
        }

        if xml_node_attribute(config, "reset") == "y":
            key["reset"] = True

        backrest = config.tag == "backrest-config"
        option_map = {}

        for option in xml_node_child_list(config, "%s-option" % config.tag):
            entry = {}

            if xml_node_attribute(option, "remove") == "y":
                entry["remove"] = True

            if option.text is not None and option.text.strip() != "":
                entry["value"] = self._replace(option.text)

            name = self._replace(xml_node_attribute(option, "key", True))

            if backrest:
                option_map.setdefault(self._replace(xml_node_attribute(option, "section", True)), {})[name] = entry
            else:
                option_map[name] = entry

        key["option"] = option_map

        return key

    ################################################################################################################################
    def backrest_config(self, section, config, depth):
        """Apply a change to the configuration and return the file, what is in it, and whether to show it."""

        key = self.config_key(config)
        file = key["file"]
        result = None

        log(DEBUG, "    " * depth + "process backrest config: %s" % file)

        if self.exe and self.is_required(section):
            hit, cache_type, key, value = self.cache_pop("cfg-pgbackrest", key)

            if hit:
                result = None if value.get("config") is None else "\n".join(value["config"])
            else:
                result = self._backrest_config_apply(config, key, depth)
                self.cache_push(cache_type, key, {"config": None if result is None else result.split("\n")})
        else:
            result = _SUPPRESS_CONFIG

        return file, result, xml_node_attribute(config, "show") != "n"

    ################################################################################################################################
    def _backrest_config_apply(self, config, key, depth):
        """Apply a change to the configuration, install it on the host, and return what a reader should see."""

        host_name = self._replace(xml_node_attribute(config, "host", True))
        host = self.host_map.get(host_name)

        if host is None:
            raise ToolError("cannot configure backrest on host %s because the host does not exist" % host_name)

        file_map = self.config_map.setdefault(host_name, {})

        if xml_node_attribute(config, "reset") == "y":
            file_map.pop(key["file"], None)

        section_map = file_map.setdefault(key["file"], {})

        for option in xml_node_child_list(config, "backrest-config-option"):
            option_section = self._replace(xml_node_attribute(option, "section", True))
            name = self._replace(xml_node_attribute(option, "key", True))
            value = None

            if xml_node_attribute(option, "remove") != "y":
                value = self._replace((option.text or "").strip())

            if value is None or value == "":
                section_map.get(option_section, {}).pop(name, None)

                if option_section in section_map and len(section_map[option_section]) == 0:
                    del section_map[option_section]

                log(DEBUG, "    " * (depth + 1) + "reset %s->%s" % (option_section, name))
            else:
                current = section_map.setdefault(option_section, {})

                # An option that may be given more than once adds to what is already there rather than replacing it
                if xml_node_attribute(option, "multi") == "y" and name in current:
                    existing = current[name]
                    current[name] = (existing if isinstance(existing, list) else [existing]) + [value]
                else:
                    current[name] = value

                log(DEBUG, "    " * (depth + 1) + "set %s->%s = %s" % (option_section, name, value))

        path_local = os.path.join(self.manifest.path_doc, "output/pgbackrest.conf")

        file_write(path_local, ini_render(section_map))
        host.copy_to(path_local, key["file"], self._replace(xml_node_attribute(config, "owner") or "postgres:postgres"), "640")

        # What a reader sees leaves out the options that are only there so the build can watch what happens
        clean = copy.deepcopy(section_map)

        for name in _CONFIG_HIDE_LIST:
            clean.get(_CONFIG_SECTION_GLOBAL, {}).pop(name, None)

        if _CONFIG_SECTION_GLOBAL in clean and len(clean[_CONFIG_SECTION_GLOBAL]) == 0:
            del clean[_CONFIG_SECTION_GLOBAL]

        result = ini_render(clean)

        return result if result.strip() != "" else None

    ################################################################################################################################
    def postgres_config(self, section, config, depth):
        """Apply a change to the PostgreSQL configuration and return the file, what was added, and whether to show it."""

        key = self.config_key(config)
        file = key["file"]
        result = None

        if self.exe and self.is_required(section):
            hit, cache_type, key, value = self.cache_pop("cfg-postgresql", key)

            if hit:
                result = None if value.get("config") is None else "\n".join(value["config"])
            else:
                result = self._postgres_config_apply(config, key, depth)
                self.cache_push(cache_type, key, {"config": None if result is None else result.split("\n")})
        else:
            result = _SUPPRESS_CONFIG

        return file, result, xml_node_attribute(config, "show") != "n"

    ################################################################################################################################
    def _postgres_config_apply(self, config, key, depth):
        """Apply a change to the PostgreSQL configuration, install it on the host, and return what was added."""

        host_name = self._replace(xml_node_attribute(config, "host", True))
        host = self.host_map.get(host_name)

        if host is None:
            raise ToolError("cannot configure postgres on host %s because the host does not exist" % host_name)

        path_local = os.path.join(self.manifest.path_doc, "output/postgresql.conf")
        host.copy_from(key["file"], path_local)

        file_map = self.pg_config_map.setdefault(host_name, {}).setdefault(key["file"], {})

        # The file as it was before the documentation added anything, which everything added is appended to
        if "base" not in file_map:
            file_map["base"] = file_read(path_local)

        option_map = dict(file_map.get("old", {}))

        log(DEBUG, "    " * depth + "process postgres config: %s" % key["file"])

        for option in xml_node_child_list(config, "postgres-config-option"):
            name = xml_node_attribute(option, "key", True)
            value = self._replace((option.text or "").strip())

            if value == "":
                option_map.pop(name, None)

                log(DEBUG, "    " * (depth + 1) + "reset %s" % name)
            else:
                option_map[name] = value

                log(DEBUG, "    " * (depth + 1) + "set %s = %s" % (name, value))

        result = "\n".join("%s = %s" % (name, option_map[name]) for name in sorted(option_map))

        file_write(path_local, file_map["base"] + ("\n# pgBackRest Configuration\n%s\n" % result if result != "" else ""))
        host.copy_to(path_local, key["file"], "postgres:postgres", "640")

        file_map["old"] = option_map

        return result if result.strip() != "" else None

    ################################################################################################################################
    def host_key(self, host):
        """Everything that decides what a host would be."""

        key = {
            "name": self._replace(xml_node_attribute(host, "name", True)),
            "image": self._replace(xml_node_attribute(host, "image", True)),
        }

        key["id"] = self._replace(xml_node_attribute(host, "id")) if xml_node_attribute(host, "id") is not None else key["name"]

        for name in ("option", "param", "os"):
            if xml_node_attribute(host, name) is not None:
                key[name] = self._replace(xml_node_attribute(host, name))

        key["update-hosts"] = xml_node_attribute(host, "update-hosts") != "n"

        return key

    ################################################################################################################################
    def cache_pop(self, cache_type, key):
        """Take the next entry from the cache and check that it describes what is about to happen."""

        if not self.cache:
            if self.manifest.cache_only:
                raise ToolError("cache only operation forced by --cache-only option")

            return False, cache_type, key, None

        if self.cache_idx >= len(self.source.cache_list):
            raise CacheInvalidError("unable to get index from cache")

        entry = self.source.cache_list[self.cache_idx]

        if "key" not in entry or "type" not in entry:
            raise CacheInvalidError("unable to get key or type from cache")

        if entry["type"] != cache_type:
            raise CacheInvalidError("types do not match, cache is invalid")

        if _canonical(entry["key"]) != _canonical(key):
            raise CacheInvalidError(
                "keys at index %d do not match, cache is invalid.\n  cache key: %s\ncurrent key: %s"
                % (self.cache_idx, _canonical(entry["key"]), _canonical(key))
            )

        self.cache_idx += 1

        return True, cache_type, key, entry.get("value", {})

    ################################################################################################################################
    def cache_push(self, cache_type, key, value):
        """Add an entry to the cache."""

        if self.cache:
            raise ToolError("cache push should not be called when the cache is already present")

        entry = {"key": key, "type": cache_type}

        if value is not None:
            entry["value"] = value

        if self.source.cache_list is None:
            self.source.cache_list = []

        self.source.cache_list.append(entry)

    ################################################################################################################################
    def section_child_process(self, section, child, depth):
        """Handle what a section holds that every renderer handles the same way, which is starting a host."""

        log(DEBUG, "    " * (depth + 1) + "process child: %s" % child.tag)

        if child.tag == "host-add":
            if self.exe and self.is_required(section):
                hit, cache_type, key, value = self.cache_pop("host", self.host_key(child))

                if hit:
                    self.manifest.var_store.set("host-%s-ip" % key["id"], value["ip"])
                else:
                    self.cache_push(cache_type, key, {"ip": self._host_add(section, child, key, depth)})
        elif child.tag != "title":
            raise ToolError("unable to process child type %s" % child.tag)

    ################################################################################################################################
    def _host_add(self, section, child, key, depth):
        """Start a host and run the commands that set it up, and return its address."""

        if key["name"] in self.host_map:
            raise ToolError("cannot add host %s because the host already exists" % key["name"])

        path_data = os.path.join(os.environ["HOME"], "data", key["name"])

        exec_result("rm -rf %s" % path_data)
        exec_result("mkdir -p %s/etc" % path_data)

        image = key["image"]
        host_user = self._replace(xml_node_attribute(child, "user", True))

        # Commands marked pre are run while the image is built rather than against the host, so a rebuild of the documentation does
        # not run them again
        if len(self.pre_execute(key["name"])) > 0:
            image = self._image_pre_build(key, host_user)

        # The repository is mounted from where it is on the host running the build, which is not where it is in this container
        path_repo = os.path.dirname(self.manifest.path_doc)
        mount = xml_node_attribute(child, "mount")
        mount = None if mount is None else self._replace(mount).replace("{[host-repo-path]}", path_repo)
        option = None if key.get("option") is None else key["option"].replace("{[host-repo-path]}", path_repo)

        host = Host(
            key["name"],
            "doc-%s" % key["name"],
            image,
            host_user,
            [mount] if mount is not None else None,
            option,
            key.get("param"),
            key["update-hosts"],
        )

        self.host_map[key["name"]] = host
        self.manifest.var_store.set("host-%s-ip" % key["id"], host.ip)

        HOST_GROUP.host_add(host)

        for execute in xml_node_child_list(child, "execute"):
            self.execute(section, key["name"], execute, indent=depth + 1, cache=False, show=False)

        return host.ip

    ################################################################################################################################
    def _image_pre_build(self, key, host_user):
        """Build an image with the commands marked pre already run, and return what it is called."""

        image = "%s-%s" % (key["image"], key["name"])

        log(INFO, "Build vm '%s' from '%s'" % (image, key["image"]))

        command_list = []

        for execute in self.pre_execute(key["name"]):
            execute_key = self.execute_key(key["name"], execute)
            command = "\n".join(execute_key["cmd"]) + (
                "" if execute_key.get("cmd-extra") is None else " " + execute_key["cmd-extra"]
            )
            command = command.replace("'", "'\\''")

            command = "sudo -u %s%s" % (
                host_user,
                (
                    " bash%s -c '%s'" % (" -l" if execute_key["load-env"] else "", command)
                    if execute_key["bash-wrap"]
                    else " " + command
                ),
            )

            command_list.append("RUN %s" % command)

            log(DETAIL, "    Pre command %s" % command)

        path_docker_file = os.path.join(self.manifest.path_doc, "output/doc-host.dockerfile")

        file_write(path_docker_file, "FROM %s\n\n%s\n" % (key["image"], self._replace("\n".join(command_list)).strip()))

        exec_result("docker build -f %s -t %s %s" % (path_docker_file, image, self.manifest.path_doc), suppress_stderr=True)

        return image


####################################################################################################################################
# Every host of the build, so a host that is added learns about the ones that are already up
HOST_GROUP = HostGroup()


####################################################################################################################################
def _canonical(value):
    """Render a cache key the one way it is written, so two keys are compared by what they hold rather than by how they were built."""

    return json.dumps(value, sort_keys=True, separators=(",", ":"))
