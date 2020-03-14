#include <cstdio>
#include <cstring>
#include <filesystem>

#include <libxml/parser.h>
#include <libxml/xmlsave.h>

enum class NodeTag
{
	HAS_REFS = 1
};

static void XMLCDECL LIBXML_ATTR_FORMAT(2, 3) ErrorHandler(void* ctx, const char* msg, ...)
{
	auto ctxt = static_cast<xmlParserCtxtPtr>(ctx);
	if (ctxt)
	{
		// ignore these
		if (ctxt->errNo == XML_NS_ERR_UNDEFINED_NAMESPACE)
		{
			return;
		}
		ctxt->wellFormed = 0;
	}

	// workaround to xmlParserError not coming in a va_list variant
	va_list args;
	char ret[4096];

	va_start(args, msg);
	int rv = vsnprintf(ret, sizeof(ret), msg, args);
	va_end(args);

	if (rv < 0)
	{
		perror("formatting error failed");
		ret[0] = '\0';
	}

	xmlParserError(ctx, "%s", ret);
}

static xmlParserInputPtr LoggingEntityLoader(const char* url, const char* id, xmlParserCtxtPtr ctx)
{
	xmlParserInputPtr ret = xmlNoNetExternalEntityLoader(url, id, ctx);
	if (ret && url)
	{
		try
		{
			// could also do relative, just pick a standard
			//static std::filesystem::path sPwd = std::filesystem::current_path();
			//auto resolved = std::filesystem::relative(url, sPwd);
			auto resolved = std::filesystem::canonical(url);
			printf("Loaded: %s\n", resolved.c_str());
		}
		catch (const std::exception& e)
		{
			ctx->sax->error(ctx, "Failed to resolve external entity '%s': %s\n", url, e.what());
		}
	}

	return ret;
}

int main(int argc, char* argv[])
{
	LIBXML_TEST_VERSION

	const char *comment, *input, *output;
	bool quiet = argc == 5;

	if (argc < 4 || argc > 5 || (argc == 5 && strcmp(argv[1], "--quiet")))
	{
		fprintf(stderr, "Usage: %s [--quiet] comment input output\n", argv[0]);
		return 1;
	}
	else if (quiet)
	{
		comment = argv[2], input = argv[3], output = argv[4];
	}
	else
	{
		comment = argv[1], input = argv[2], output = argv[3];
		xmlSetExternalEntityLoader(LoggingEntityLoader);
	}
	xmlLineNumbersDefault(1);

	xmlParserCtxtPtr parserCtxt = xmlNewParserCtxt();
	parserCtxt->sax->warning = ErrorHandler;
	parserCtxt->sax->error = ErrorHandler;
	parserCtxt->sax->comment = nullptr;
	parserCtxt->sax->getEntity = [](void* ctx, const xmlChar* name) {
		auto ctxt = static_cast<xmlParserCtxtPtr>(ctx);
		if (ctxt && ctxt->node)
		{
			ctxt->node->_private = reinterpret_cast<void*>(NodeTag::HAS_REFS);
		}
		return xmlSAX2GetEntity(ctx, name);
	};
	parserCtxt->sax->endElementNs = [](void* ctx, const xmlChar* localname, const xmlChar* prefix, const xmlChar* URI) {
		// The XML_PARSE_NOBLANKS option doesn't work well on elements that
		// include entity references, so this works around that
		// This also removes element content where the content is only blanks
		auto ctxt = static_cast<xmlParserCtxtPtr>(ctx);
		if (ctxt && (ctxt->node->children == ctxt->node->last ||
						ctxt->node->_private == reinterpret_cast<void*>(NodeTag::HAS_REFS)))
		{
			xmlNodePtr cur = ctxt->node->children;
			while (cur != nullptr)
			{
				if (xmlIsBlankNode(cur))
				{
					xmlNodePtr temp = cur;
					cur = cur->next;
					xmlUnlinkNode(temp);
					xmlFreeNode(temp);
				}
				else
				{
					cur = cur->next;
				}
			}
		}
		xmlSAX2EndElementNs(ctx, localname, prefix, URI);
	};

	// Could also enable XML_PARSE_NSCLEAN
	xmlDocPtr doc = xmlCtxtReadFile(parserCtxt, input, nullptr,
		XML_PARSE_DTDLOAD | XML_PARSE_NONET | XML_PARSE_NOBLANKS | XML_PARSE_NOENT);
	xmlFreeParserCtxt(parserCtxt);

	if (doc == nullptr)
	{
		fprintf(stderr, "failed to parse: %s\n", input);
		return 1;
	}

	xmlDtdPtr dtd = xmlGetIntSubset(doc);
	if (dtd != nullptr)
	{
		xmlUnlinkNode(reinterpret_cast<xmlNodePtr>(dtd));
		xmlFreeDtd(dtd);
	}

	xmlNodePtr root = xmlDocGetRootElement(doc);
	xmlNodePtr commentNode = xmlNewComment(BAD_CAST comment);
	xmlAddPrevSibling(root, commentNode);

	xmlSaveCtxtPtr saveCtxt = xmlSaveToFilename(output, "utf-8", XML_SAVE_FORMAT);
	int ret = xmlSaveDoc(saveCtxt, doc);

	xmlSaveClose(saveCtxt);
	xmlFreeDoc(doc);
	xmlCleanupParser();

	if (ret == -1)
	{
		fprintf(stderr, "failed to serialise xml to %s\n", output);
		return 1;
	}

	return 0;
}