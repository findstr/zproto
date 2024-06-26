#include <assert.h>
#include <vector>
#include <string>
#include <sstream>
#include <unordered_set>

#include "zproto.hpp"

static std::unordered_set<struct zproto_struct *>	protocol;
static std::unordered_set<struct zproto_struct *>	defined;

struct stmt_args {
	int level;
	std::string base;
	std::vector<std::string> fields;
	std::vector<std::string> encodestm;
	std::vector<std::string> decodestm;
	std::vector<std::string> stmts;
};

static std::string tab(int n)
{
	std::stringstream ss;
	for (int i = 0; i < n; i++)
		ss << "\t";
	return ss.str();
}

static std::string inline
fill_normal(struct zproto_field *f, int level)
{
	std::string t = tab(level);
	char buff[512];
	const char *fmt =
	"%s\tcase %d:\n"
	"%s\t\treturn write(ref args, %s);\n";

	const char *afmt =
	"%s\tcase %d:\n"
	"%s\t\tDebug.Assert(args.idx >= 0);\n"
	"%s\t\tif (args.idx >= (int)%s.Length) {\n"
	"%s\t\t\targs.len = args.idx;\n"
	"%s\t\t\treturn zdll.NOFIELD;\n"
	"%s\t\t}\n"
	"%s\t\treturn write(ref args, %s[args.idx]);\n";
	if (f->isarray) {
		snprintf(buff, 512, afmt,
				t.c_str(),f->tag,
				t.c_str(),
				t.c_str(),f->name,
				t.c_str(),
				t.c_str(),
				t.c_str(),
				t.c_str(),f->name);
	} else {
		snprintf(buff, 512, fmt,
				t.c_str(), f->tag,
				t.c_str(), f->name);
	}
	return buff;
}

static std::string inline
to_blob(struct zproto_field *f, int level)
{
	char buff[512];
	const char *fmt =
	"%s\tcase %d:\n"
	"%s\t\treturn read(ref args, out %s);\n";

	const char *afmt =
	"%s\tcase %d:\n"
	"%s\t\tif (args.idx == 0)\n"
	"%s\t\t\t%s = new byte[args.len][];\n"
	"%s\t\tif (args.len == 0)\n"
	"%s\t\t\treturn 0;\n"
	"%s\t\treturn read(ref args, out %s[args.idx]);\n";

	std::string t = tab(level);
	if (f->isarray) {
		snprintf(buff, 512, afmt,
			t.c_str(), f->tag,
			t.c_str(),
			t.c_str(), f->name,
			t.c_str(),
			t.c_str(),
			t.c_str(), f->name);
	} else {
		snprintf(buff, 512, fmt,
			t.c_str(), f->tag,
			t.c_str(), f->name);
	}
	return buff;
}

static std::string inline
to_string(struct zproto_field *f, int level)
{
	char buff[512];
	const char *fmt =
	"%s\tcase %d:\n"
	"%s\t\treturn read(ref args, out %s);\n";

	const char *afmt =
	"%s\tcase %d:\n"
	"%s\t\tif (args.idx == 0)\n"
	"%s\t\t\t%s = new string[args.len];\n"
	"%s\t\tif (args.len == 0)\n"
	"%s\t\t\treturn 0;\n"
	"%s\t\treturn read(ref args, out %s[args.idx]);\n";

	std::string t = tab(level);
	if (f->isarray) {
		snprintf(buff, 512, afmt,
			t.c_str(), f->tag,
			t.c_str(),
			t.c_str(), f->name,
			t.c_str(),
			t.c_str(),
			t.c_str(), f->name);
	} else {
		snprintf(buff, 512, fmt,
			t.c_str(), f->tag,
			t.c_str(), f->name);
	}
	return buff;
}



static std::string inline
to_normal(struct zproto_field *f, const char *type, int level)
{
	char buff[512];
	const char *fmt =
	"%s\tcase %d:\n"
	"%s\t\treturn read(ref args, out %s);\n";

	const char *afmt =
	"%s\tcase %d:\n"
	"%s\t\tif (args.idx == 0)\n"
	"%s\t\t\t%s = new %s[args.len];\n"
	"%s\t\tif (args.len == 0)\n"
	"%s\t\t\treturn 0;\n"
	"%s\t\treturn read(ref args, out %s[args.idx]);\n";

	std::string t = tab(level);
	if (f->isarray) {
		snprintf(buff, 512, afmt,
			t.c_str(), f->tag,
			t.c_str(),
			t.c_str(), f->name, type,
			t.c_str(),
			t.c_str(),
			t.c_str(), f->name);
	} else {
		snprintf(buff, 512, fmt,
			t.c_str(), f->tag,
			t.c_str(), f->name);
	}
	return buff;
}

static std::string inline
fill_struct(struct zproto_field *f, int level)
{
	char buff[1024];
	std::string t = tab(level);

	const char *afmt =
	"%s\tcase %d:\n"
	"%s\t\tif (args.idx >= (int)%s.Length) {\n"
	"%s\t\t\targs.len = args.idx;\n"
	"%s\t\t\treturn zdll.NOFIELD;\n"
	"%s\t\t}\n"
	"%s\t\treturn %s[args.idx]._encode(args.buff, args.buffsz, args.sttype);\n";

	const char *fmt =
	"%s\tcase %d:\n"
	"%s\t\treturn %s._encode(args.buff, args.buffsz, args.sttype);\n";

	if (f->mapkey != NULL) {
		assert(0);
	} else if (f->isarray) {
		snprintf(buff, 1024, afmt,
				t.c_str(), f->tag,
				t.c_str(), f->name,
				t.c_str(),
				t.c_str(),
				t.c_str(),
				t.c_str(), f->name);
	} else {
		snprintf(buff, 1024, fmt,
				t.c_str(), f->tag,
				t.c_str(), f->name);
	}
	return buff;
}

static std::string inline
to_struct(struct zproto_field *f, const char *type, int level)
{
	char buff[512];

	const char *afmt =
	"%s\tcase %d:\n"
	"%s\t\tDebug.Assert(args.idx >= 0);\n"
	"%s\t\tif (args.idx == 0)\n"
	"%s\t\t\t%s = new %s[args.len];\n"
	"%s\t\tif (args.len == 0)\n"
	"%s\t\t\treturn 0;\n"
	"%s\t\t%s[args.idx] = new %s();\n"
	"%s\t\treturn %s[args.idx]._decode(args.buff, args.buffsz, args.sttype);\n";

	const char *fmt =
	"%s\tcase %d:\n"
	"%s\t\t%s = new %s();\n"
	"%s\t\treturn %s._decode(args.buff, args.buffsz, args.sttype);\n";

	std::string t = tab(level);

	if (f->mapkey != NULL) {
		assert(0);
	} else if (f->isarray) {
		snprintf(buff, 512, afmt,
				t.c_str(), f->tag,
				t.c_str(),
				t.c_str(),
				t.c_str(), f->name, type,
				t.c_str(),
				t.c_str(),
				t.c_str(), f->name, type,
				t.c_str(), f->name);
	} else {
		snprintf(buff, 512, fmt,
				t.c_str(), f->tag,
				t.c_str(), f->name, type,
				t.c_str(), f->name);
	}
	return buff;
}


static std::string inline
format_code(const char *base, const char *name, const char *qualifier, int lv)
{
	static const char *fmt =
	"%sprotected override int %s(ref zdll.args args) %s {\n"
	"%s\tswitch (args.tag) {\n";
	char buff[1024];
	std::string t = tab(lv);
	snprintf(buff, 1024, fmt, t.c_str(), name,
		qualifier, t.c_str());
	return buff;
}

static std::string inline
format_close(int level)
{
	static const char *fmt =
	"%s\tdefault:\n"
	"%s\t\treturn zdll.ERROR;\n"
	"%s\t}\n"
	"%s}\n";
	char buff[4096];
	std::string t = tab(level);
	snprintf(buff, 4096, fmt,
		t.c_str(), t.c_str(), t.c_str(), t.c_str());
	return buff;
}

static std::string inline
format_name(const char *base, int level)
{
	static const char *fmt =
	"%spublic override string _name() {\n"
	"%s\treturn \"%s\";\n"
	"%s}\n";
	char buff[1024];
	std::string t = tab(level);
	snprintf(buff, 1024, fmt, t.c_str(), t.c_str(), base, t.c_str());
	return buff;
}

static void prototype_cb(struct zproto_field *f, struct stmt_args *ud);

static void
formatst(struct zproto *z, struct zproto_struct *st, struct stmt_args &newargs)
{
	int level = newargs.level;
	std::string t1 = tab(newargs.level - 1);
	std::string t2 = tab(newargs.level);
	std::string tmp;
	char buff[4096];
	int count;
	const struct zproto_starray *sts;
	struct zproto_struct *const*start, *const*end;
	sts = (st == NULL) ? zproto_root(z) : st->child;
	if (sts != NULL) {
		start = &sts->buf[0];
		end = start + sts->count;
		while (start < end) {
			struct zproto_struct *child = *start;
			struct stmt_args nnewargs;
			nnewargs.level = newargs.level + 1;
			assert(protocol.count(child) == 0);
			assert(defined.count(child) == 0);
			nnewargs.base = newargs.base + "." + zproto_name(child);
			formatst(z, child, nnewargs);
			newargs.stmts.insert(newargs.stmts.end(),
				nnewargs.stmts.begin(),
				nnewargs.stmts.end());
			++start;
		}
	}
	for (int i = 0; i < st->fieldcount; i++) {
		prototype_cb(st->fields[i], &newargs);
	}
	//open
	if (protocol.count(st)) {
		snprintf(buff, 4096, "public class %s:wirep {\n",
			zproto_name(st));
	} else {
		snprintf(buff, 4096, "public class %s:wire {\n",
			zproto_name(st));
	}
	newargs.stmts.insert(newargs.stmts.begin(), t1 + buff);

	//field
	newargs.stmts.insert(newargs.stmts.end(),
		newargs.fields.begin(), newargs.fields.end());
	tmp = "\n";
	newargs.stmts.push_back(tmp);
	//name
	tmp = format_name(newargs.base.c_str(), level);
	newargs.stmts.push_back(tmp);
	//_encode_field
	tmp = format_code(newargs.base.c_str(), "_encode_field", "", level);
	newargs.encodestm.insert(newargs.encodestm.begin(), tmp);
	tmp = format_close(level);
	newargs.encodestm.push_back(tmp);

	newargs.stmts.insert(newargs.stmts.end(), newargs.encodestm.begin(),
			newargs.encodestm.end());
	//_decode_field
	tmp = format_code(newargs.base.c_str(), "_decode_field", "", level);
	newargs.decodestm.insert(newargs.decodestm.begin(), tmp);
	tmp = format_close(level);
	newargs.decodestm.push_back(tmp);
	newargs.stmts.insert(newargs.stmts.end(), newargs.decodestm.begin(),
			newargs.decodestm.end());
	//close
	tmp = t1 + "}\n";
	newargs.stmts.push_back(tmp);

	defined.insert(st);
	return ;
}

static void
prototype_cb(struct zproto_field *f, struct stmt_args *ud)
{
	std::string estm;
	std::string dstm;
	std::string type;
	std::string subtype;
	char buff[2048];
	struct stmt_args newargs;
	std::string t = tab(ud->level);
	newargs.level = ud->level + 1;
	if (f->isarray)
		type = "public %s[] %s;\n";
	else
		type = "public %s %s;\n";

	switch (f->type) {
	case ZPROTO_STRUCT:
		subtype = zproto_name(f->seminfo);
		estm = fill_struct(f, ud->level);
		dstm = to_struct(f, subtype.c_str(), ud->level);
		break;
	case ZPROTO_BLOB:
		subtype = "byte[]";
		estm = fill_normal(f, ud->level);
		dstm = to_blob(f, ud->level);
		break;
	case ZPROTO_STRING:
		subtype = "string";
		estm = fill_normal(f, ud->level);
		dstm = to_string(f, ud->level);
		break;
	case ZPROTO_BOOLEAN:
		subtype = "bool";
		goto gen;
		break;
	case ZPROTO_BYTE:
		subtype = "sbyte";
		goto gen;
		break;
	case ZPROTO_SHORT:
		subtype = "short";
		goto gen;
		break;
	case ZPROTO_INTEGER:
		subtype = "int";
		goto gen;
		break;
	case ZPROTO_LONG:
		subtype = "long";
		goto gen;
		break;
	case ZPROTO_UBYTE:
		subtype = "byte";
		goto gen;
		break;
	case ZPROTO_USHORT:
		subtype = "ushort";
		goto gen;
		break;
	case ZPROTO_UINTEGER:
		subtype = "uint";
		goto gen;
		break;
	case ZPROTO_ULONG:
		subtype = "ulong";
		goto gen;
		break;
	case ZPROTO_FLOAT:
		subtype = "float";
	gen:
		estm = fill_normal(f, ud->level);
		dstm = to_normal(f, subtype.c_str(), ud->level);
		break;
	default:
		break;
	}
	snprintf(buff, 2048,
		type.c_str(),
		subtype.c_str(),
		f->name);
	ud->fields.push_back(t + buff);
	ud->encodestm.push_back(estm);
	ud->decodestm.push_back(dstm);
	return ;
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
	const struct zproto_starray *sts = zproto_root(z);
	if (sts == NULL)
		return ;
	start = &sts->buf[0];
	end = start + sts->count;
	while (start < end) {
		struct stmt_args args;
		struct zproto_struct *st = *start;
		args.base = zproto_name(st);
		args.level = 1;
		formatst(z, st, args);
		dump_vecstring(fp, args.stmts);
		++start;
	}
	return ;
}

static void
wiretree(FILE *fp, const char *proto)
{
	int i;
	char buff[8];
	std::string hex = "";
	for (i = 0; proto[i]; i++) {
		snprintf(buff, 8, "\\x%x", (uint8_t)proto[i]);
		hex += buff;
	}
	fprintf(fp,
		"public class serializer:wiretree {\n\n"
		"\tprivate static serializer inst = null;\n\n"
		"\tprivate const string def = \"%s\";\n"
		"\tprivate serializer():base(def) {\n\n"
		"\t}\n\n"
		"\tpublic static serializer instance() {\n"
		"\t\tif (inst == null)\n"
		"\t\t\tinst = new serializer();\n"
		"\t\treturn inst;\n"
		"\t}\n"
		"}\n\n",
		hex.c_str()
	);
}

static const char *wirep =
"public abstract class wirep:iwirep {\n"
"\tprotected override wiretree _wiretree() {\n"
"\t\treturn serializer.instance();\n"
"\t}\n"
"}\n\n";

void
body(const char *name, std::vector<const char *> &space, const char *proto, struct zproto *z)
{
	FILE *fp;
	int count;
	struct zproto_struct *const* start, *const* end;
	std::string path = name;
	path += ".cs";
	fp = fopen(path.c_str(), "wb+");

	const struct zproto_starray *sts = zproto_root(z);
	if (sts != NULL) {
		start = &sts->buf[0];
		end = start + sts->count;
		while (start < end) {
			struct zproto_struct *st = *start;
			protocol.insert(st);
			++start;
		}
	}
	fprintf(fp,
		"using System;\n"
		"using System.Text;\n"
		"using System.Collections.Generic;\n"
		"using System.Runtime.InteropServices;\n"
		"using System.Diagnostics;\n"
		"using zprotobuf;\n");
	for (const auto p:space)
		fprintf(fp, "namespace %s {\n", p);
	fprintf(fp, "\n");
	fprintf(fp, wirep);
	dumpst(fp, z);
	wiretree(fp, proto);
	for (size_t i = 0; i < space.size(); i++)
		fprintf(fp, "}");
	fprintf(fp, "\n");
	fclose(fp);
}


