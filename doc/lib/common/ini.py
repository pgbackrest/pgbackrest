"""Ini Rendering.

Writes the configuration file the documentation shows and installs on a host. An option that may be given more than once is a list
and is written once per value, which is how a repository or a stanza with several of something is configured."""

####################################################################################################################################


####################################################################################################################################
def ini_render(section_map):
    """Render sections and their options as an ini file, in the order a reader would look for them."""

    result = ""

    for section in sorted(section_map):
        # A blank line between sections, which is how the file reads when a person writes it
        if result != "":
            result += "\n"

        result += "[%s]\n" % section

        for key in sorted(section_map[section]):
            value = section_map[section][key]

            for one in value if isinstance(value, list) else [value]:
                result += "%s=%s\n" % (key, one)

    return result
