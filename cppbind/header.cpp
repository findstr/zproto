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
	std::vector<std::string> type;
	std::vector<std::string> fields;
};

static std::string
tab(int level)
{
	std::string str;
	str.reserve(level);
	for (int i = 0; i < level; i++)
		str += "\t";
	return str;
}

static int prototype_cb(struct zproto_field *f, struct prototype_args &ud);

// emit a clean POD struct: just fields, no base class, no virtuals, no zproto dep.
static void
formatst(struct zproto *z, struct zproto_struct *st, struct prototype_args &newargs)
{
	std::string t2 = tab(newargs.level - 1);
	struct zproto_struct *const *start, *const *end;
	const struct zproto_starray *sts;
	sts = (st == NULL) ? zproto_root(z) : st->child;
	if (sts != NULL) {
		start = &sts->buf[0];
		end = start + sts->count;
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
	}
	for (int i = 0; i < st->fieldcount; i++) {
		prototype_cb(st->fields[i], newargs);
	}
	// nested helper structs first
	newargs.fields.insert(
		newargs.fields.begin(),
		newargs.type.begin(),
		newargs.type.end()
	);
	// open: clean POD struct (no inheritance)
	newargs.fields.insert(
		newargs.fields.begin(),
		t2 + "struct " + zproto_name(st) + " {\n"
	);
	// close
	newargs.fields.insert(
		newargs.fields.end(),
		t2 + "};\n"
	);
	defined.insert(st);
	return;
}

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
	for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
		if (types[i].type == type)
			return types[i].name;
	}
	assert(type == ZPROTO_STRUCT);
	return std::string("struct ") + zproto_name(st);
}

static int
prototype_cb(struct zproto_field *f, struct prototype_args &ud)
{
	char buff[2048];
	std::string type;
	std::string subtype;
	std::string defval;
	if (f->mapkey != NULL) {
		assert(f->isarray);
		struct find_field_ud { int tag; std::string type; } fud;
		fud.tag = f->mapkey->tag;
		fud.type = typestr(f->mapkey->type, NULL);
		assert(fud.type != "");
		type = tab(ud.level) + "std::unordered_map<" + fud.type + ", %s> %s%s;\n";
	} else if (f->isarray) {
		type = tab(ud.level) + "std::vector<%s> %s%s;\n";
	} else {
		type = tab(ud.level) + "%s %s%s;\n";
		switch (f->type) {
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
	subtype = typestr(f->type, f->seminfo);
	snprintf(buff, 2048, type.c_str(), subtype.c_str(), f->name, defval.c_str());
	ud.fields.push_back(buff);
	return 0;
}

static void
dump_vecstring(FILE *fp, const std::vector<std::string> &tbl)
{
	for (const auto &str : tbl)
		fprintf(fp, "%s", str.c_str());
	return;
}

static void
dumpst(FILE *fp, struct zproto *z)
{
	struct zproto_struct *const *start, *const *end;
	const struct zproto_starray *sts;
	sts = zproto_root(z);
	if (sts != NULL) {
		start = &sts->buf[0];
		end = start + sts->count;
		while (start < end) {
			struct prototype_args args;
			struct zproto_struct *st = *start;
			args.level = 1;
			formatst(z, st, args);
			dump_vecstring(fp, args.fields);
			++start;
		}
	}
}

// emit X.hpp: #pragma once + zproto-free includes + clean POD structs.
void
header(const char *name, std::vector<const char *> &space, struct zproto *z)
{
	FILE *fp;
	struct zproto_struct *const *start, *const *end;
	std::string path = name;
	path += ".hpp";
	const struct zproto_starray *sts;
	sts = zproto_root(z);
	if (sts != NULL) {
		start = &sts->buf[0];
		end = start + sts->count;
		while (start < end) {
			struct zproto_struct *st = *start;
			protocol.insert(st);
			++start;
		}
	}
	fp = fopen(path.c_str(), "wb+");
	fprintf(fp, "#pragma once\n");
	fprintf(fp, "#include <cstdint>\n");
	fprintf(fp, "#include <string>\n");
	fprintf(fp, "#include <vector>\n");
	fprintf(fp, "#include <unordered_map>\n");
	for (const auto p : space)
		fprintf(fp, "namespace %s {\n", p);
	fprintf(fp, "\n");
	dumpst(fp, z);
	fprintf(fp, "\n");
	for (size_t i = 0; i < space.size(); i++)
		fprintf(fp, "}");
	fprintf(fp, "\n");
	fclose(fp);
}
