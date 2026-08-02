"""Build News.

Dates the news and lists the most recent items on the index. A news item says when it was posted and nothing else about its date, so
the date the reader sees on the news and the list of recent items on the index both come from the news itself rather than being
written again, and out of step, wherever they appear."""

####################################################################################################################################
import re

from common.date import date_render
from common.error import ToolError
from common.xml import (
    xml_document_new,
    xml_node_add,
    xml_node_attribute,
    xml_node_child,
    xml_node_child_add,
    xml_node_child_list,
    xml_node_child_replace,
    xml_node_content_add,
    xml_node_insert,
    xml_node_text_add,
)

# Page the news is on, which is what the index points at for each item it lists
_PAGE = "news"

# A date the way it sorts, which is how a news item says when it was posted
_DATE_EXP = re.compile(r"^[0-9]{4}-[0-9]{2}-[0-9]{2}$")


####################################################################################################################################
def _date(item):
    """The date a news item was posted, the way it sorts."""

    date = xml_node_attribute(item, "date", True)

    if _DATE_EXP.match(date) is None:
        raise ToolError("news '%s' has invalid date '%s'" % (xml_node_attribute(item, "id", True), date))

    return date


####################################################################################################################################
def _item_list(news):
    """Every news item, most recent first, which is the order the news is written in."""

    result = xml_node_child_list(news, "section")

    # An item that is out of order would be missed by the index, which lists the items the news begins with
    for prior, item in zip(result, result[1:]):
        if _date(item) > _date(prior):
            raise ToolError(
                "news '%s' is out of order, since the news is written most recent first" % xml_node_attribute(item, "id", True)
            )

    return result


####################################################################################################################################
def _index_recurse(node, item_list):
    """Replace wherever a document asks for the news with the items it asks for."""

    # The children are taken first because replacing one removes it
    for child in list(node):
        if child.tag != "news-list":
            _index_recurse(child, item_list)

            continue

        total = int(xml_node_attribute(child, "total", True))

        # The items are built in a node of their own so they can take the place of the node that asked for them
        replace = xml_document_new(child.tag)

        for item in item_list[:total]:
            text = xml_node_text_add(xml_node_add(replace, "p"))

            xml_node_content_add(xml_node_add(text, "b"), date_render(_date(item)))
            xml_node_content_add(text, " - ")

            # An item is named the way the news names it, so a title that is changed there is changed here as well
            link = xml_node_add(text, "link", {"page": _PAGE, "section": "/%s" % xml_node_attribute(item, "id", True)})
            xml_node_child_add(link, xml_node_child(item, "title", True))

        xml_node_child_replace(node, child, replace)


####################################################################################################################################
def news_render(news):
    """Date every news item and return the news."""

    for item in _item_list(news):
        title = xml_node_child(item, "title", True)

        # The date goes just after the title, which is where a reader looks for it
        node = xml_node_insert(item, list(item).index(title) + 1, "p")

        xml_node_content_add(xml_node_add(xml_node_text_add(node), "b"), date_render(_date(item)))

    return news


####################################################################################################################################
def news_index_render(index, news):
    """List the most recent news items where the index asks for them and return the index."""

    _index_recurse(index, _item_list(news))

    return index
