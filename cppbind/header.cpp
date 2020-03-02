#include <assert.h>
#include <vector>
#include <string>
#include <sstream>
#include <unordered_set>

#include "zproto.hpp"
#include "header.h"

static std::unordered_set<struct zproto_struct *> protocol;
static std::unordered_set<struct zproto_struct *> defined;

struct prototype_args {
	int level;
	std::vector<std::string> type;
	std::vector<std::string> fields;
	std::vector<std::string> encode;
	std::vector<std::string> iters;
};

const char *funcproto = "\
%sprotected:\n\
%s\
%svirtual int _encode_field(struct zproto_args *args) const;\n\
%svirtual int _decode_field(struct zproto_args *args);\n\
%s\
%spublic:\n\
%svirtual void _reset();\n\
%s\
";

static std::string
tab(int level)
{
	std::string str;
	str.reserve(level);
	for (int i = 0; i < level; i++)
		str += "\t";
	return str;
}

static int prototype_cb(struct zproto_args *args);

static void
formatst(struct zproto *z, struct zproto_struct *st, struct prototype_args &newargs)
{
	char buff[2048];
	std::string t1 = tab(newargs.level);
	std::string t2 = tab(newargs.level - 1);
	std::string iterwrapper;
	std::string iterdefine;
	int count;
	struct zproto_struct *const*start, *const*end;
	start = zproto_child(z, st, &count);
	end = start + count;
	while (start < end) {
		struct zproto_struct *child = *start;
		assert(protocol.count(child) == 0);
		assert(defined.count(child) == 0);
		struct prototype_args nnewargs;
		nnewargs.level = newargs.level + 1;
		formatst(z, child, nnewargs);
		newargs.type.insert(newargs.type.end(),
			nnewargs.fields.begin(), nnewargs.fields.end());
		++start;
	}
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
		herit = ":public wirepimpl";
	else
		herit = ":public wire ";
	newargs.fields.insert(
		newargs.fields.begin(),
                t2 + "struct " + zproto_name(st) + herit + "{\n");
	if (newargs.iters.size()) {
		std::stringstream ss;
		ss << t1 << "union iterator_wrapper {\n";
		ss << t1 << t1 << "iterator_wrapper(){};\n";
		for (const auto &iter:newargs.iters)
			ss << t1 << t1 << iter;
		ss << t1 << "};\n";
		iterwrapper = ss.str();
		iterdefine = t1 + "mutable union iterator_wrapper _mapiterator;\n";
	}
	//member function
	char wirep_query[2048];
	if (protocol.count(st)) {
		snprintf(wirep_query, 2048,
				"%svirtual int _tag() const;\n"
				"%svirtual const char *_name() const;\n"
				"%svirtual zproto_struct *_query() const;\n"
				"%sstatic void _load(wiretree &t);\n"
				"%sprivate:\n"
				"%sstatic zproto_struct *_st;\n",
				t1.c_str(),
				t1.c_str(),
				t1.c_str(),
				t1.c_str(),
				t2.c_str(),
				t1.c_str());
	} else {
		wirep_query[0] = 0;
	}
        snprintf(buff, 2048, funcproto,
			t2.c_str(),
			iterwrapper.c_str(),
			t1.c_str(),
			t1.c_str(),
			iterdefine.c_str(),
			t2.c_str(),
			t1.c_str(),
			wirep_query);
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

static std::string
typestr(int type, struct zproto_struct *st)
{
	static struct {
		int type;
		const char *name;
	} types[] = {
		{ZPROTO_BLOB, "std::string"},
		{ZPROTO_STRING, "std::string"},
		{ZPROTO_BOOLEAN, "bool"},
		{ZPROTO_FLOAT, "float"},
		{ZPROTO_BYTE, "int8_t"},
		{ZPROTO_SHORT, "int16_t"},
		{ZPROTO_INTEGER, "int32_t"},
		{ZPROTO_LONG, "int64_t"},
		{ZPROTO_UBYTE, "uint8_t"},
		{ZPROTO_USHORT, "uint16_t"},
		{ZPROTO_UINTEGER, "uint32_t"},
		{ZPROTO_ULONG, "uint64_t"},
	};
	for (size_t i = 0; i < sizeof(types)/ sizeof(types[0]); i++) {
		if (types[i].type == type)
			return types[i].name;
	}
	assert(type == ZPROTO_STRUCT);
	return std::string("struct ") + zproto_name(st);
}

static int
find_field_type(struct zproto_args *args)
{
	struct find_field_ud *ud = (struct find_field_ud *)args->ud;
	if (ud->tag == args->tag)
		ud->type = typestr(args->type, NULL);
	return 0;
}

static int
prototype_cb(struct zproto_args *args)
{
	struct prototype_args *ud = (struct prototype_args *)args->ud;
	char buff[2048];
	std::string type;
	std::string iter;
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
		iter = "std::unordered_map<" + fud.type + ", %s>::const_iterator %s;\n";
		type = tab(ud->level) + str;
	} else if(args->idx >= 0) {
		type = tab(ud->level) + "std::vector<%s> %s%s;\n";
	} else {
		type = tab(ud->level) + "%s %s%s;\n";
		switch (args->type) {
		case ZPROTO_BOOLEAN:
			defval = " = false";
			break;
		case ZPROTO_BYTE:
		case ZPROTO_SHORT:
		case ZPROTO_LONG:
		case ZPROTO_INTEGER:
		case ZPROTO_UBYTE:
		case ZPROTO_USHORT:
		case ZPROTO_ULONG:
		case ZPROTO_UINTEGER:
			defval = " = 0";
			break;
		case ZPROTO_FLOAT:
			defval = " = 0.0f";
			break;
		}
	}
	subtype = typestr(args->type, args->sttype);
	snprintf(buff, 2048, type.c_str(), subtype.c_str(), args->name, defval.c_str());
	ud->fields.push_back(buff);
	if (iter.size()) {
		snprintf(buff, 2048, iter.c_str(), subtype.c_str(), args->name);
		ud->iters.push_back(buff);
	}
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
dumpst(FILE *fp, struct zproto *z)
{
	int count;
	struct zproto_struct *const* start, *const* end;
	start = zproto_child(z, NULL, &count);
	end = start + count;
	while (start < end) {
		struct prototype_args args;
		struct zproto_struct *st = *start;
		args.level = 1;
		formatst(z, st, args);
		dump_vecstring(fp, args.fields);
		++start;
	}
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
"struct wirepimpl:public wirep {\n"
"protected:\n"
"        virtual wiretree &_wiretree() const;\n"
"};\n\n";

void
header(const char *name, std::vector<const char *> &space, struct zproto *z)
{
	FILE *fp;
	int count;
	struct zproto_struct *const* start, *const* end;
	std::string path = name;
	path += ".hpp";
	start = zproto_child(z, NULL, &count);
	end = start + count;
	while (start < end) {
		struct zproto_struct *st = *start;
		protocol.insert(st);
		++start;
	}
	fp = fopen(path.c_str(), "wb+");
	fprintf(fp, "#ifndef __%s_h\n#define __%s_h\n", name, name);
	fprintf(fp, "#include \"zprotowire.h\"\n");
	for (const auto p:space)
		fprintf(fp, "namespace %s {\n", p);
	fprintf(fp, "%s", "\nusing namespace zprotobuf;\n\n");
	fprintf(fp, "%s", wirep);
	dumpst(fp, z);
	wiretree(fp);
	fprintf(fp, "\n");
	for (size_t i = 0; i < space.size(); i++)
		fprintf(fp, "}");
	fprintf(fp, "\n#endif\n");
	fclose(fp);
}

