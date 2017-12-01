#include <assert.h>
#include <vector>
#include <string>
#include <unordered_set>

#include "zproto.hpp"
#include "header.h"

static std::unordered_set<struct zproto_struct *> protocol;
static std::unordered_set<struct zproto_struct *> defined;

struct prototype_args {
	int level;
	int hasmap = 0;
	std::vector<std::string> type;
	std::vector<std::string> fields;
	std::vector<std::string> encode;
};

const char *funcproto = "\
%sprotected:\n\
%svirtual int _encode_field(struct zproto_args *args) const;\n\
%svirtual int _decode_field(struct zproto_args *args);\n\
%s\
%spublic:\n\
%svirtual const char *_name() const;\n\
";

static std::string
tab(int level)
{
	std::string str;
	level *= 8;
	str.reserve(level);
	for (int i = 0; i < level; i++)
		str += " ";
	return str;
}

static int prototype_cb(struct zproto_args *args);

static void
formatst(struct zproto_struct *st, struct prototype_args &newargs)
{
	std::string t1 = tab(newargs.level);
	std::string t2 = tab(newargs.level - 1);
	std::string mapbuf;
	char buff[2048];

	zproto_travel(st, prototype_cb, &newargs);
	//subtype
	newargs.fields.insert(
		newargs.fields.begin(),
		newargs.type.begin(),
		newargs.type.end()
	);
	//open
	const char *herit;
	if (protocol.count(st))
		herit = ":public wirep ";
	else
		herit = ":public wire ";
	newargs.fields.insert(
		newargs.fields.begin(),
                t2 + "struct " + zproto_name(st) + herit + "{\n");
	if (newargs.hasmap)
		mapbuf = t1 + "mutable std::vector<const void *>maptoarray;\n";
	//member function
        snprintf(buff, 2048, funcproto,
			t2.c_str(),
			t1.c_str(), t1.c_str(),
			mapbuf.c_str(),
			t2.c_str(),
			t1.c_str());
	newargs.fields.push_back(buff);

	//close
	newargs.fields.insert(
		newargs.fields.end(),
		t2 + "};\n"
		);
	defined.insert(st);
	return ;
}

struct find_field_ud {
	int tag;
	std::string type;
};

static int
find_field_type(struct zproto_args *args)
{
	struct find_field_ud *ud = (struct find_field_ud *)args->ud;
	if (ud->tag == args->tag) {
		switch (args->type) {
		case ZPROTO_STRING:
			ud->type = "std::string";
			break;
		case ZPROTO_BOOLEAN:
			ud->type = "uint8_t";
			break;
		case ZPROTO_INTEGER:
			ud->type = "uint32_t";
			break;
		case ZPROTO_FLOAT:
			ud->type = "float";
			break;
		default:
			assert(!"unsupport key type of std::map");
			break;
		}
	}
	return 0;
}

static int
prototype_cb(struct zproto_args *args)
{
	struct prototype_args *ud = (struct prototype_args *)args->ud;
	struct prototype_args newargs;
	char buff[2048];
	std::string type;
	std::string subtype;
	std::string defval;
	if (args->maptag) {
		std::string str;
		assert(args->idx >= 0);
		struct find_field_ud fud;
		fud.tag = args->maptag;
		zproto_travel(args->sttype, find_field_type, &fud);
		assert(fud.type != "");
		str = "std::unordered_map<" + fud.type + ", %s> %s%s;\n";
		type = tab(ud->level) + str;
		ud->hasmap = 1;
	} else if(args->idx >= 0) {
		type = tab(ud->level) + "std::vector<%s> %s%s;\n";
	} else {
		type = tab(ud->level) + "%s %s%s;\n";
		switch (args->type) {
		case ZPROTO_BOOLEAN:
			defval = " = false";
			break;
		case ZPROTO_INTEGER:
			defval = " = 0";
			break;
		case ZPROTO_FLOAT:
			defval = " = 0.0f";
			break;
		}
	}

	switch (args->type) {
	case ZPROTO_STRUCT:
		if (protocol.count(args->sttype) == 0 && defined.count(args->sttype) == 0) { //protocol define
			newargs.level = ud->level + 1;
			formatst(args->sttype, newargs);
			ud->type.insert(ud->type.end(), newargs.fields.begin(), newargs.fields.end());
		}
		subtype = std::string("struct ") + zproto_name(args->sttype);
		break;
	case ZPROTO_STRING:
		subtype = "std::string";
		break;
	case ZPROTO_BOOLEAN:
		subtype = "bool";
		break;
	case ZPROTO_INTEGER:
		subtype = "uint32_t";
		break;
	case ZPROTO_FLOAT:
		subtype = "float";
		break;
	default:
		break;
	}
	snprintf(buff, 2048, type.c_str(), subtype.c_str(),args->name, defval.c_str());
	type = buff;
	ud->fields.push_back(type);
	return 0;
}

static void
dump_vecstring(FILE *fp, const std::vector<std::string> &tbl)
{
	for (const auto &str:tbl)
		fprintf(fp, "%s", str.c_str());
	return;
}

static void
dumpst(FILE *fp, struct zproto *z, struct zproto_struct *st)
{
	struct prototype_args args;
	struct zproto_struct *nxt = zproto_next(z, st);
	if (st == NULL)
		return;
	dumpst(fp, z, nxt);
	args.level = 1;
	args.fields.clear();
	formatst(st, args);
	dump_vecstring(fp, args.fields);
}

static void
wiretree(FILE *fp)
{
	fprintf(fp, "%s",
"class serializer:public wiretree {\n"
"\tserializer();\n"
"public:\n"
"\tstatic serializer &instance();\n"
"};\n");
}
const char *wirep =
"struct wirep:public wire {\n"
"public:\n"
"        virtual int _serialize(std::string &dat) const;\n"
"        virtual int _serializesafe(std::string &dat, int presize = 1024) const;\n"
"        virtual int _serialize(const uint8_t **data) const;\n"
"        virtual int _parse(const std::string &dat);\n"
"        virtual int _parse(const uint8_t *data, int datasz);\n"
"	 virtual int _tag() const;\n"
"};\n\n";

void
header(const char *name, std::vector<const char *> &space, struct zproto *z)
{
	FILE *fp;
	std::string path = name;
	path += ".hpp";
	struct zproto_struct *st = NULL;
	for (;;) {
		st = zproto_next(z, st);
		if (st == NULL)
			break;
		protocol.insert(st);
	}
	st = zproto_next(z, NULL);
	fp = fopen(path.c_str(), "wb+");
	fprintf(fp, "#ifndef __%s_h\n#define __%s_h\n", name, name);
	fprintf(fp, "#include \"zprotowire.h\"\n");
	for (const auto p:space)
		fprintf(fp, "namespace %s {\n", p);
	fprintf(fp, "\nusing namespace zprotobuf;\n\n");
	fprintf(fp, wirep);
	dumpst(fp, z, st);
	wiretree(fp);
	fprintf(fp, "\n");
	for (size_t i = 0; i < space.size(); i++)
		fprintf(fp, "}");
	fprintf(fp, "\n#endif\n");
	fclose(fp);
}

