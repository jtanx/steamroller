#include <cstdio>
#include <filesystem>

#include <libxml/parser.h>
#include <libxml/xmlsave.h>

enum class NodeTag
{
	HAS_REFS = 1
};

static void XMLCDECL LIBXML_ATTR_FORMAT(2, 3) ErrorHandler(void* ctx, const char* msg, ...)
{
	xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr)ctx;
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
	std::string ret;

	va_start(args, msg);
	int length = std::min(vsnprintf(nullptr, 0, msg, args), 65535);
	va_end(args);

	if (length > 0)
	{
		va_start(args, msg);
		ret.resize(length + 1);
		vsnprintf(&ret[0], length + 1, msg, args);
		va_end(args);
	}

	xmlParserError(ctx, "%s", ret.c_str());
}

static xmlParserInputPtr LoggingEntityLoader(const char* url, const char* id, xmlParserCtxtPtr ctx)
{
	xmlParserInputPtr ret = xmlNoNetExternalEntityLoader(url, id, ctx);
	if (ret && url)
	{
		// could also do relative, just pick a standard
		//static std::filesystem::path sPwd = std::filesystem::current_path();
		//auto resolved = std::filesystem::relative(url, sPwd);
		auto resolved = std::filesystem::canonical(url);
		printf("Loaded: %s\n", resolved.c_str());
	}

	return ret;
}

int main(int argc, char* argv[])
{
	LIBXML_TEST_VERSION

	if (argc != 3)
	{
		fprintf(stderr, "Usage: %s input output\n", argv[0]);
		return 1;
	}

	xmlSetExternalEntityLoader(LoggingEntityLoader);
	xmlLineNumbersDefault(1);

	xmlParserCtxtPtr parserCtxt = xmlNewParserCtxt();
	parserCtxt->sax->warning = ErrorHandler;
	parserCtxt->sax->error = ErrorHandler;
	parserCtxt->sax->comment = [](void*, const xmlChar*) {};
	parserCtxt->sax->getEntity = [](void* ctx, const xmlChar* name) {
		xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr)ctx;
		if (ctxt && ctxt->node)
		{
			ctxt->node->_private = reinterpret_cast<void*>(NodeTag::HAS_REFS);
		}
		return xmlSAX2GetEntity(ctx, name);
	};
	parserCtxt->sax->endElementNs = [](void* ctx, const xmlChar* localname, const xmlChar* prefix, const xmlChar* URI) {
		// The XML_PARSE_NOBLANKS option doesn't work well on elements that
		// include entity references, so this works around that
		xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr)ctx;
		if (ctxt && ctxt->node->_private == reinterpret_cast<void*>(NodeTag::HAS_REFS) &&
			ctxt->node->children != ctxt->node->last)
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

	xmlDocPtr doc = xmlCtxtReadFile(parserCtxt, argv[1], nullptr,
		XML_PARSE_DTDLOAD | XML_PARSE_NOBLANKS | XML_PARSE_NOENT | XML_PARSE_NSCLEAN);
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

	xmlNodePtr root = xmlDocGetRootElement(doc);
	xmlNodePtr comment = xmlNewComment(BAD_CAST " STEAMROLLED ");
	xmlAddPrevSibling(root, comment);

	xmlSaveCtxtPtr saveCtxt = xmlSaveToFilename(argv[2], "utf-8", XML_SAVE_FORMAT);
	int ret = xmlSaveDoc(saveCtxt, doc);

	xmlSaveClose(saveCtxt);
	xmlFreeDoc(doc);
	xmlCleanupParser();

	if (ret == -1)
	{
		fprintf(stderr, "failed to serialise xml to %s\n", argv[2]);
		return 1;
	}

	return 0;
}