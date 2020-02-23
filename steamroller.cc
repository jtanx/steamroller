#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlerror.h>
#include <libxml/xmlsave.h>

#include <cstdio>
#include <filesystem>

static void XMLCDECL LIBXML_ATTR_FORMAT(2,3) ErrorHandler(void *ctx, const char *msg, ...)
{
	xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
	xmlParserInputPtr input = nullptr;
	xmlParserInputPtr cur = nullptr;

	if (ctxt) {
		input = ctxt->input;
		if (input && input->filename && (ctxt->inputNr > 1))
		{
			cur = input;
			input = ctxt->inputTab[ctxt->inputNr - 1];
		}
		xmlParserPrintFileInfo(input);
	}

	va_list args;
	va_start(args, msg);

	fprintf(stderr, "error: ");
	vfprintf(stderr, msg, args);

	va_end(args);

	if (ctxt)
	{
		xmlParserPrintFileContext(input);
		if (cur)
		{
			xmlParserPrintFileInfo(cur);
			xmlGenericError(xmlGenericErrorContext, "\n");
			xmlParserPrintFileContext(cur);
		}
		// Make it error out, not sure of a better way
		ctxt->wellFormed = 0;
	}
}

static xmlParserInputPtr LoggingEntityLoader(const char* url, const char* id, xmlParserCtxtPtr ctx)
{
	xmlParserInputPtr ret = xmlNoNetExternalEntityLoader(url, id, ctx);
	if (ret && url)
	{
		// could also do relative, just pick a standard
		auto resolved = std::filesystem::canonical(url);
		printf("Loaded: %s\n", resolved.c_str());
	}

	return ret;
}

static void StripTree(xmlNodePtr node)
{
	while (node)
	{
		if (node->type == XML_COMMENT_NODE)
		{
			xmlNodePtr old = node;
			node = node->next;
			xmlUnlinkNode(old);
			xmlFreeNode(old);
		}
		else
		{
			StripTree(node->children);
			node = node->next;
		}
	}
}

int main(int argc, char* argv[])
{
	LIBXML_TEST_VERSION

	if (argc != 3)
	{
		fprintf(stderr, "Usage: %s input output", argv[0]);
		return 1;
	}

	xmlSetExternalEntityLoader(LoggingEntityLoader);
	xmlLineNumbersDefault(1);

	xmlParserCtxtPtr parserCtxt = xmlNewParserCtxt();
	// parserCtxt->sax->error = ErrorHandler;
	parserCtxt->sax->warning = ErrorHandler;
	xmlDocPtr doc = xmlCtxtReadFile(parserCtxt, argv[1], "utf-8",
		XML_PARSE_DTDLOAD | XML_PARSE_NOENT | XML_PARSE_NOBLANKS | XML_PARSE_NSCLEAN);
	xmlFreeParserCtxt(parserCtxt);

	if (doc == nullptr)
	{
		fprintf(stderr, "failed to parse: %s\n", argv[1]);
		return 1;
	}

	xmlDtdPtr dtd = xmlGetIntSubset(doc);
	if (dtd != nullptr)
	{
		xmlUnlinkNode((xmlNodePtr)dtd);
		xmlFreeDtd(dtd);
	}

	StripTree(xmlDocGetRootElement(doc));

	FILE* fp = fopen(argv[2], "wb");
	if (fp == nullptr)
	{
		perror("failed to open output file");
		return 1;
	}

	fputs("<!-- STEAMROLLED -->\n", fp);
	int ret = xmlDocFormatDump(fp, doc, 1);

	xmlCleanupParser();
	fclose(fp);

	if (ret == -1)
	{
		fprintf(stderr, "failed to serialise xml to %s\n", argv[2]);
		return 1;
	}

	return 0;
}