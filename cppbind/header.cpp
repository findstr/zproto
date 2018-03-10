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
	level *= 8;
	str.reserve(level);
	for (int i = 0; i < level; i++)
		str += " ";
	return str;
}

static int prototype_cb(struct zproto_args *args);

static void
formatst(struct zproto *z, struct zproto_struct *st, struct prototype_args &newargs)
{
	std::string t1 = tab(newargs.level);
	std::string t2 = tab(newargs.level - 1);
	std::string iterwrapper;
	std::string iterdefine;
	char buff[2048];
	struct zproto_struct *child;
	for (child = zproto_child(z, st); child; child = zproto_next(z, child)) {
		assert(protocol.count(child) == 0);
		assert(defined.count(child) == 0);
		struct prototype_args nnewargs;
		nnewargs.level = newargs.level + 1;
		formatst(z, child, nnewargs);
		newargs.type.insert(newargs.type.end(), nnewargs.fields.begin(), nnewargs.fields.end());
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
			ud->type = "bool";
			break;
		case ZPROTO_INTEGER:
			ud->type = "int32_t";
			break;
		case ZPROTO_LONG:
			ud->type = "int64_t";
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
		case ZPROTO_LONG:
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
		subtype = std::string("struct ") + zproto_name(args->sttype);
		break;
	case ZPROTO_STRING:
		subtype = "std::string";
		break;
	case ZPROTO_BOOLEAN:
		subtype = "bool";
		break;
	case ZPROTO_INTEGER:
		subtype = "int32_t";
		break;
	case ZPROTO_LONG:
		subtype = "int64_t";
		break;
	case ZPROTO_FLOAT:
		subtype = "float";
		break;
	default:
		break;
	}
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
dumpst(FILE *fp, struct zproto *z, struct zproto_struct *st)
{
	struct prototype_args args;
	struct zproto_struct *nxt = zproto_next(z, st);
	if (st == NULL)
		return;
	args.level = 1;
	args.fields.clear();
	formatst(z, st, args);
	dump_vecstring(fp, args.fields);
	dumpst(fp, z, nxt);
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
	fprintf(fp, "%s", "\nusing namespace zprotobuf;\n\n");
	fprintf(fp, "%s", wirep);
	dumpst(fp, z, st);
	wiretree(fp);
	fprintf(fp, "\n");
	for (size_t i = 0; i < space.size(); i++)
		fprintf(fp, "}");
	fprintf(fp, "\n#endif\n");
	fclose(fp);
}

