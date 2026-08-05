"""Test Build News.

The news and the index are written out and the result checked as the xml that comes out, since what is built here is part of the
document before a renderer ever sees it."""

####################################################################################################################################
import xml.etree.ElementTree as etree

from harness.test import *

from command.build.news import *
from common.error import *
from common.xml import xml_document_parse

NEWS = """<doc title="News">
    <section id="tarball" date="2026-07-20">
        <title>New Distribution Tarball</title>

        <p>Every release includes a tarball.</p>
    </section>

    <section id="release-2-59-0" date="2026-07-20">
        <title><backrest/> 2.59.0 Released</title>

        <p>The release is out.</p>
    </section>

    <section id="continue" date="2026-05-18">
        <title><backrest/> Will Continue!</title>

        <p>The project continues.</p>
    </section>

    <section id="maintenance" date="2026-05-04">
        <title>Maintenance Update</title>

        <p>An update on maintenance.</p>
    </section>
</doc>
"""


####################################################################################################################################
def _news(content=NEWS):
    """Parse the news."""

    return xml_document_parse(content, "news.xml")


####################################################################################################################################
def _index(content):
    """Parse an index."""

    return xml_document_parse("<doc>%s</doc>" % content, "index.xml")


####################################################################################################################################
def test_news_render():
    """Every news item is dated from the date it was posted, which it says once."""

    news = news_render(_news())
    item = news[0]

    # The date goes just after the title, which is where a reader looks for it
    assert_equal([child.tag for child in item], ["title", "p", "p"])
    assert_equal(etree.tostring(item[1], encoding="unicode"), "<p><b>July 20, 2026</b></p>")

    # Every item is dated rather than only the first
    assert_equal([item.find("p/b").text for item in news], ["July 20, 2026", "July 20, 2026", "May 18, 2026", "May 4, 2026"])


####################################################################################################################################
def test_news_index_render():
    """The index lists the most recent news items, named and dated the way the news names and dates them."""

    index = news_index_render(_index('<section id="news"><title>News</title><news-list total="3"/></section>'), _news())

    assert_equal(
        etree.tostring(index[0], encoding="unicode"),
        '<section id="news"><title>News</title>'
        '<p><b>July 20, 2026</b> - <link page="news" section="/tarball">New Distribution Tarball</link></p>'
        '<p><b>July 20, 2026</b> - <link page="news" section="/release-2-59-0"><backrest /> 2.59.0 Released</link></p>'
        '<p><b>May 18, 2026</b> - <link page="news" section="/continue"><backrest /> Will Continue!</link></p>'
        "</section>",
    )

    # An index that asks for more items than the news holds gets what there is
    index = news_index_render(_index('<news-list total="99"/>'), _news())

    assert_equal(len(index), 4)

    # An index that does not ask for the news is left as it was
    index = news_index_render(_index("<p>No news here.</p>"), _news())

    assert_equal(etree.tostring(index, encoding="unicode"), "<doc><p>No news here.</p></doc>")


####################################################################################################################################
def test_news_date_error():
    """A date that is not written the way it sorts is reported, since it would otherwise be shown as some other date."""

    with assert_raises(ToolError) as raised:
        news_render(_news(NEWS.replace('date="2026-05-18"', 'date="May 18, 2026"')))

    assert_equal(str(raised.exception), "news 'continue' has invalid date 'May 18, 2026'")

    # A news item with no date at all cannot be dated
    with assert_raises(ToolError) as raised:
        news_index_render(_index('<news-list total="1"/>'), _news(NEWS.replace(' date="2026-07-20"', "", 1)))

    assert_equal(str(raised.exception), "unable to find attribute 'date' in node 'section'")


####################################################################################################################################
def test_news_order_error():
    """A news item that is out of order is reported, since the index lists the items the news begins with."""

    with assert_raises(ToolError) as raised:
        news_render(_news(NEWS.replace('date="2026-05-18"', 'date="2026-08-18"')))

    assert_equal(str(raised.exception), "news 'continue' is out of order, since the news is written most recent first")
