/***********************************************************************************************************************************
Documentation Script

Three things that a page reads better with and works without. A browser that runs no script loses them and loses nothing it needs:
the contents are links and work as links, every block that can scroll is already reachable from the keyboard, and the button that
copies a config file is not shown until there is a script to make it work.

The first marks the section being read in the contents beside the text, so a long page says where the reader is rather than only
where they can go. The section being read is the last one that begins above the menu bar, which is what a reader means by the
section they are in. An observer would report the sections in view instead, which is not the same thing once a section is taller
than the window.

The second takes the focus stop back off the blocks that turned out to fit, which is most of them.
***********************************************************************************************************************************/
"use strict";

(function()
{
    var TOC_LEVEL_LIST = [".section1-toc", ".section2-toc", ".section3-toc"];

    // What scrolls is the element that sticks rather than the column holding it
    var column = document.querySelector(".page-toc");
    var toc = document.querySelector(".page-toc-inner") || column;
    var bar = document.querySelector(".page-menu");

    if (toc === null)
        return;

    /*******************************************************************************************************************************
    The link in the contents for each section of this page, by the id it points at
    *******************************************************************************************************************************/
    var linkMap = Object.create(null);
    var linkList = toc.querySelectorAll("a[href^='#']");

    for (var linkIdx = 0; linkIdx < linkList.length; linkIdx++)
    {
        var id = decodeURIComponent(linkList[linkIdx].getAttribute("href").substring(1));

        if (id !== "")
            linkMap[id] = linkList[linkIdx];
    }

    /*******************************************************************************************************************************
    The anchors those links point at, in the order they appear on the page
    *******************************************************************************************************************************/
    var anchorList = [];
    var anchorAll = document.querySelectorAll(".page-body a[id]");

    for (var anchorIdx = 0; anchorIdx < anchorAll.length; anchorIdx++)
    {
        if (anchorAll[anchorIdx].id in linkMap)
            anchorList.push(anchorAll[anchorIdx]);
    }

    if (anchorList.length === 0)
        return;

    /*******************************************************************************************************************************
    Mark a link and the links of the sections holding it
    *******************************************************************************************************************************/
    function markAdd(link)
    {
        link.classList.add("toc-active");

        // Walk out through the sections holding this one, marking each so the path to the section being read is visible
        var node = link.closest(TOC_LEVEL_LIST.join(", "));

        while (node !== null && node.parentElement !== null)
        {
            node = node.parentElement.closest(TOC_LEVEL_LIST.join(", "));

            if (node !== null)
            {
                var parent = node.querySelector(":scope > [class$='-toc-title'] > a");

                if (parent !== null)
                    parent.classList.add("toc-active-parent");
            }
        }
    }

    /*******************************************************************************************************************************
    Hold the contents to the space between where they sit and the bottom of the window

    The style can only say how tall they are once they have come to rest under the menu bar. Until then they sit lower than that,
    by however much of the page header is still on screen, and the style has no way to know how much that is. Left alone the last
    of the contents would hang below the window until the page was scrolled far enough to bring them to rest.
    *******************************************************************************************************************************/
    function fit()
    {
        // Only what sticks is held to the window. Above the text rather than beside it the contents are as tall as they are, and
        // a height set here would leave the rest of them hanging over the text, since nothing is scrolling them there.
        if (window.getComputedStyle(toc).position !== "sticky")
        {
            toc.style.maxHeight = "";

            return;
        }

        // Measured from the column rather than from what sticks inside it, since the height being set here moves that
        var barBox = bar === null ? null : bar.getBoundingClientRect();
        var top = Math.max(barBox === null ? 0 : barBox.height, column.getBoundingClientRect().top);

        toc.style.maxHeight = Math.max(0, window.innerHeight - top) + "px";
    }

    /*******************************************************************************************************************************
    Keep the mark in view when the contents are a column of their own and taller than the space they are given
    *******************************************************************************************************************************/
    function markShow(link)
    {
        if (toc.scrollHeight <= toc.clientHeight)
            return;

        var linkBox = link.getBoundingClientRect();
        var tocBox = toc.getBoundingClientRect();

        // Scroll the contents rather than the page, and leave the mark a third of the way down rather than against an edge
        if (linkBox.top < tocBox.top || linkBox.bottom > tocBox.bottom)
            toc.scrollTop += linkBox.top - tocBox.top - toc.clientHeight / 3;
    }

    /*******************************************************************************************************************************
    Find the section being read and mark it
    *******************************************************************************************************************************/
    var active = null;
    var pending = false;

    function mark()
    {
        pending = false;

        fit();

        // A link into the page leaves the section it points at this far down, clear of the menu bar over the top of the text. The
        // same distance decides which section is being read, so following a link marks what it arrived at rather than what sits
        // above it.
        var edge = parseFloat(window.getComputedStyle(anchorList[0]).scrollMarginTop) + 1;
        var found = anchorList[0];

        for (var idx = 0; idx < anchorList.length; idx++)
        {
            if (anchorList[idx].getBoundingClientRect().top > edge)
                break;

            found = anchorList[idx];
        }

        // The end of the page cannot be scrolled past, so the last section is marked once the end is reached however tall it is
        if (window.innerHeight + window.scrollY >= document.documentElement.scrollHeight - 2)
            found = anchorList[anchorList.length - 1];

        if (found === active)
            return;

        active = found;

        var markList = toc.querySelectorAll(".toc-active, .toc-active-parent");

        for (var markIdx = 0; markIdx < markList.length; markIdx++)
            markList[markIdx].classList.remove("toc-active", "toc-active-parent");

        markAdd(linkMap[found.id]);
        markShow(linkMap[found.id]);
    }

    /*******************************************************************************************************************************
    Run on a frame rather than on every event, since scrolling reports far more often than a page can be drawn
    *******************************************************************************************************************************/
    function schedule()
    {
        if (pending)
            return;

        pending = true;
        window.requestAnimationFrame(mark);
    }

    window.addEventListener("scroll", schedule, {passive: true});
    window.addEventListener("resize", schedule, {passive: true});

    mark();
})();

/***********************************************************************************************************************************
Blocks that scroll

A block that scrolls sideways has to be reachable from the keyboard, so the page is written with every one of them focusable. Most
of them fit at most widths and never scroll, and a focus stop on a block that does not scroll is a stop that does nothing, so the
ones that fit give theirs back. How wide a block needs to be is a question only the browser can answer, and the answer changes
with the window, so it is asked again whenever the window changes.
***********************************************************************************************************************************/
(function()
{
    // Captured once, since this is what removes the attribute the selector matches and the list must not shrink as it goes
    var blockList = document.querySelectorAll("pre[tabindex], .execute-body-output[tabindex], .config-body-output[tabindex]");

    if (blockList.length === 0)
        return;

    var pending = false;

    /*******************************************************************************************************************************
    Give the stop to the blocks that scroll and take it back from the blocks that do not
    *******************************************************************************************************************************/
    function reach()
    {
        pending = false;

        for (var idx = 0; idx < blockList.length; idx++)
        {
            // A pixel of slack, since a block that fits can still measure a fraction wider than the room it has
            if (blockList[idx].scrollWidth > blockList[idx].clientWidth + 1)
                blockList[idx].setAttribute("tabindex", "0");
            else
                blockList[idx].removeAttribute("tabindex");
        }
    }

    /*******************************************************************************************************************************
    Run on a frame rather than on every event, since a window being dragged reports far more often than a page can be drawn
    *******************************************************************************************************************************/
    function schedule()
    {
        if (pending)
            return;

        pending = true;
        window.requestAnimationFrame(reach);
    }

    window.addEventListener("resize", schedule, {passive: true});

    reach();
})();

/***********************************************************************************************************************************
Copying a config file

What is shown of a config file is what a section changed it to, so the lines the section took out are on the page beside the lines
it put in, and the marks that say which is which are written by the style rather than by the page. None of that is the file, so the
button hands over the lines that are in the file and leaves the rest of it behind.

The page is written with the button and the style keeps it hidden, so it is shown here rather than written here. A browser that
cannot copy never shows it at all, since a button that does nothing is worse than no button.
***********************************************************************************************************************************/
(function()
{
    // How long the button says what happened before it goes back to saying what it does when it is pressed
    var SAID_TIME = 2000;

    var buttonList = document.querySelectorAll(".config-copy");

    if (buttonList.length === 0 || navigator.clipboard === undefined)
        return;

    /*******************************************************************************************************************************
    The config file of the block a button is in, which is the lines that are in it
    *******************************************************************************************************************************/
    function configText(button)
    {
        var lineList = button.closest(".config").querySelectorAll(".config-line, .config-line-add");
        var result = "";

        for (var idx = 0; idx < lineList.length; idx++)
            result += lineList[idx].textContent + "\n";

        return result;
    }

    /*******************************************************************************************************************************
    Make a button work and show it
    *******************************************************************************************************************************/
    function ready(button)
    {
        var timeout = null;

        function reset()
        {
            button.classList.remove("config-copy-done", "config-copy-fail");
            timeout = null;
        }

        function said(state)
        {
            if (timeout !== null)
                window.clearTimeout(timeout);

            reset();

            button.classList.add(state);
            timeout = window.setTimeout(reset, SAID_TIME);
        }

        function done()
        {
            said("config-copy-done");
        }

        // A browser can refuse to copy, e.g. when the page is not the tab the reader is on, and saying nothing would read as done
        function fail()
        {
            said("config-copy-fail");
        }

        function copy()
        {
            navigator.clipboard.writeText(configText(button)).then(done, fail);
        }

        button.addEventListener("click", copy);
        button.classList.add("config-copy-ready");
    }

    for (var buttonIdx = 0; buttonIdx < buttonList.length; buttonIdx++)
        ready(buttonList[buttonIdx]);
})();
