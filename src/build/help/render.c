/***********************************************************************************************************************************
Render Help
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "common/compress/bz2/compress.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "common/log.h"
#include "common/type/pack.h"

#include "build/common/render.h"
#include "build/config/parse.h"
#include "build/help/parse.h"

/***********************************************************************************************************************************
Render xml as text
***********************************************************************************************************************************/
static String *
bldHlpRenderReplace(const String *const string, const char *const replace, const char *const with)
{
    String *result = strNew();

    StringList *const stringList = strLstNewSplitZ(string, replace);

    for (unsigned int stringIdx = 0; stringIdx < strLstSize(stringList); stringIdx++)
    {
        if (stringIdx != 0)
            strCatZ(result, with);

        strCat(result, strLstGet(stringList, stringIdx));
    }

    return result;
}

static String *
bldHlpRenderXmlNode(const xmlNodePtr xml)
{
    String *const result = strNew();

    for (xmlNodePtr currentNode = xml->children; currentNode != NULL; currentNode = currentNode->next)
    {
        const String *const name = STR((char *)currentNode->name);

        if (currentNode->type == XML_ELEMENT_NODE)
        {
            if (strEq(name, STRDEF("admonition")))
            {
                strCatZ(result, "NOTE: ");
                strCat(result, bldHlpRenderXmlNode(currentNode));
                strCatZ(result, "\n\n");
            }
            else if (strEq(name, STRDEF("backrest")))
                strCatZ(result, "pgBackRest");
            else if (strEq(name, STRDEF("list")))
            {
                strCat(result, bldHlpRenderXmlNode(currentNode));
                strCatChr(result, '\n');
            }
            else if (strEq(name, STRDEF("list-item")))
            {
                strCatZ(result, "* ");
                strCat(result, bldHlpRenderXmlNode(currentNode));
                strCatChr(result, '\n');
            }
            else if (strEq(name, STRDEF("p")))
            {
                strCat(result, bldHlpRenderXmlNode(currentNode));
                strCatZ(result, "\n\n");
            }
            else if (strEq(name, STRDEF("postgres")))
                strCatZ(result, "PostgreSQL");
            else if (
                strEq(name, STRDEF("id")) || strEq(name, STRDEF("br-option")) || strEq(name, STRDEF("cmd")) ||
                strEq(name, STRDEF("link")) || strEq(name, STRDEF("setting")) || strEq(name, STRDEF("pg-setting")) ||
                strEq(name, STRDEF("code")) || strEq(name, STRDEF("i")) || strEq(name, STRDEF("file")) ||
                strEq(name, STRDEF("path")) || strEq(name, STRDEF("b")) || strEq(name, STRDEF("host")) ||
                strEq(name, STRDEF("exe")) || strEq(name, STRDEF("proper")))
            {
                strCat(result, bldHlpRenderXmlNode(currentNode));
            }
            else
                THROW_FMT(FormatError, "unknown tag '%s'", strZ(name));
        }
        else if (currentNode->type == XML_TEXT_NODE)
        {
            xmlChar *content = xmlNodeGetContent(currentNode);
            String *text = strNewZ((char *)content);
            xmlFree(content);

            if (strchr(strZ(text), '\n') != NULL)
            {
                if (!strEmpty(strTrim(strDup(text))))
                    THROW_FMT(FormatError, "text '%s' is invalid", strZ(text));

                continue;
            }

            text = bldHlpRenderReplace(text, "{[dash]}", "-");

            strCat(result, text);
        }
    }

    return result;
}

static String *
bldHlpRenderXml(const XmlNode *const xml)
{
    return strTrim(bldHlpRenderXmlNode(*(const xmlNodePtr *)xml));
}

/***********************************************************************************************************************************
Render help to a pack
***********************************************************************************************************************************/
static PackWrite *
bldHlpRenderHelpAutoCPack(const BldCfg bldCfg, const BldHlp bldHlp)
{
    PackWrite *const pack = pckWriteNewP(.size = 65 * 1024);

    // Command help
    // -----------------------------------------------------------------------------------------------------------------------------
    pckWriteArrayBeginP(pack);

    for (unsigned int cmdIdx = 0; cmdIdx < lstSize(bldCfg.cmdList); cmdIdx++)
    {
        const BldCfgCommand *const cmd = lstGet(bldCfg.cmdList, cmdIdx);
        const BldHlpCommand *const cmdHlp = lstFind(bldHlp.cmdList, &cmd->name);
        CHECK(AssertError, cmdHlp != NULL, "command help is NULL");

        pckWriteBoolP(pack, cmd->internal);
        pckWriteStrP(pack, bldHlpRenderXml(cmdHlp->summary));
        pckWriteStrP(pack, bldHlpRenderXml(cmdHlp->description));
    }

    pckWriteArrayEndP(pack);

    // Option help
    // -----------------------------------------------------------------------------------------------------------------------------
    pckWriteArrayBeginP(pack);

    for (unsigned int optIdx = 0; optIdx < lstSize(bldCfg.optList); optIdx++)
    {
        const BldCfgOption *const opt = lstGet(bldCfg.optList, optIdx);
        const BldHlpOption *const optHlp = lstFind(bldHlp.optList, &opt->name);

        // Internal
        pckWriteBoolP(pack, opt->internal);

        // Section
        if (optHlp != NULL)
            pckWriteStrP(pack, optHlp->section);
        else
            pckWriteNullP(pack);

        // Summary
        if (optHlp != NULL)
            pckWriteStrP(pack, bldHlpRenderXml(optHlp->summary));
        else
            pckWriteNullP(pack);

        // Description
        if (optHlp != NULL)
            pckWriteStrP(pack, bldHlpRenderXml(optHlp->description));
        else
            pckWriteNullP(pack);

        // Deprecations
        StringList *const deprecateList = strLstNew();

        if (opt->deprecateList != NULL)
        {
            for (unsigned int deprecateIdx = 0; deprecateIdx < lstSize(opt->deprecateList); deprecateIdx++)
            {
                const BldCfgOptionDeprecate *const deprecate = lstGet(opt->deprecateList, deprecateIdx);

                if (!strEq(deprecate->name, opt->name))
                    strLstAdd(deprecateList, deprecate->name);
            }
        }

        if (!strLstEmpty(deprecateList))
        {
            pckWriteArrayBeginP(pack);

            for (unsigned int deprecateIdx = 0; deprecateIdx < strLstSize(deprecateList); deprecateIdx++)
                pckWriteStrP(pack, strLstGet(deprecateList, deprecateIdx));

            pckWriteArrayEndP(pack);
        }
        else
            pckWriteNullP(pack);

        // Command overrides
        bool found = false;

        for (unsigned int cmdIdx = 0; cmdIdx < lstSize(bldCfg.cmdList); cmdIdx++)
        {
            const BldCfgCommand *const cmd = lstGet(bldCfg.cmdList, cmdIdx);
            const BldCfgCommand *const optCmd = lstFind(opt->cmdList, &cmd->name);

            if (optCmd != NULL)
            {
                const BldHlpCommand *const cmdHlp = lstFind(bldHlp.cmdList, &cmd->name);
                CHECK(AssertError, cmdHlp != NULL, "command help is NULL");
                const BldHlpOption *const cmdOptHlp = cmdHlp->optList != NULL ? lstFind(cmdHlp->optList, &opt->name) : NULL;

                if (opt->internal != optCmd->internal || cmdOptHlp != NULL)
                {
                    if (!found)
                    {
                        pckWriteArrayBeginP(pack);
                        found = true;
                    }

                    pckWriteObjBeginP(pack, .id = cmdIdx + 1);

                    if (opt->internal != optCmd->internal)
                        pckWriteBoolP(pack, optCmd->internal, .defaultWrite = true);
                    else
                        pckWriteNullP(pack);

                    if (cmdOptHlp != NULL)
                    {
                        pckWriteStrP(pack, bldHlpRenderXml(cmdOptHlp->summary));
                        pckWriteStrP(pack, bldHlpRenderXml(cmdOptHlp->description));
                    }

                    pckWriteObjEndP(pack);
                }
            }
        }

        if (found)
            pckWriteArrayEndP(pack);
        else
            pckWriteNullP(pack);
    }

    pckWriteArrayEndP(pack);
    pckWriteEnd(pack);

    return pack;
}

/***********************************************************************************************************************************
Compress pack to a buffer
***********************************************************************************************************************************/
static Buffer *
bldHlpRenderHelpAutoCCmp(const BldCfg bldCfg, const BldHlp bldHlp)
{
    // Get pack buffer
    const Buffer *const packBuf = pckToBuf(pckWriteResult(bldHlpRenderHelpAutoCPack(bldCfg, bldHlp)));
    Buffer *const result = bufNew(bufSize(packBuf));

    // Open source/destination
    IoRead *const source = ioBufferReadNewOpen(packBuf);
    IoWrite *const destination = ioBufferWriteNew(result);

    ioFilterGroupAdd(ioWriteFilterGroup(destination), bz2CompressNew(9));
    ioWriteOpen(destination);

    // Copy data from source to destination
    Buffer *read = bufNew(bufUsed(packBuf) + 1);

    ioRead(source, read);
    ASSERT(ioReadEof(source));

    ioWrite(destination, read);
    ioWriteClose(destination);

    // Return compressed buffer
    return result;
}

/***********************************************************************************************************************************
Output buffer to a file as a byte array
***********************************************************************************************************************************/
static void
bldHlpRenderHelpAutoC(const Storage *const storageRepo, const BldCfg bldCfg, const BldHlp bldHlp)
{
    // Convert buffer to bytes
    const Buffer *const buffer = bldHlpRenderHelpAutoCCmp(bldCfg, bldHlp);

    String *const help = strCatFmt(
        strNew(),
        "%s"
        "static const unsigned char helpData[%zu] =\n"
        "{\n",
        strZ(bldHeader("help", "Help Data")), bufUsed(buffer));

    bool first = true;
    size_t lineSize = 0;
    char byteZ[4];

    for (unsigned int bufferIdx = 0; bufferIdx < bufUsed(buffer); bufferIdx++)
    {
        snprintf(byteZ, sizeof(byteZ), "%u", bufPtrConst(buffer)[bufferIdx]);

        if (strlen(byteZ) + 1 + (first ? 0 : 1) + lineSize > 128)
        {
            strCatChr(help, '\n');
            first = true;
        }

        if (first)
        {
            strCatZ(help, "    ");
            lineSize = 0;
        }

        strCatFmt(help, "%s%s,", first ? "" : " ", byteZ);

        lineSize += strlen(byteZ) + 1 + (first ? 0 : 1);
        first = false;
    }

    strCatZ(help, "\n};\n");

    // Write to storage
    bldPut(storageRepo, "src/command/help/help.auto.c.inc", BUFSTR(help));
}

/**********************************************************************************************************************************/
void
bldHlpRender(const Storage *const storageRepo, const BldCfg bldCfg, const BldHlp bldHlp)
{
    bldHlpRenderHelpAutoC(storageRepo, bldCfg, bldHlp);
}
