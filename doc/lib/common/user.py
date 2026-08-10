"""User and Group.

The user the tool runs as. A test container is built with this user so files created inside it are owned by the user on the host,
and the generated test.c is given the same names so a test can check ownership against what it will find at runtime. The
documentation build runs the commands it shows as this user, so what a reader sees is what they would type."""

####################################################################################################################################
import grp
import os
import pwd


####################################################################################################################################
def user_id():
    """Id of the user the tests run as."""

    return os.getuid()


####################################################################################################################################
def user_name():
    """Name of the user the tests run as."""

    return pwd.getpwuid(user_id()).pw_name


####################################################################################################################################
def group_id():
    """Id of the group the tests run as."""

    return os.getgid()


####################################################################################################################################
def group_name():
    """Name of the group the tests run as."""

    return grp.getgrgid(group_id()).gr_name
