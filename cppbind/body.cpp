#include <assert.h>
#include <vector>
#include <string>
#include <unordered_set>

#include "zproto.hpp"

static std::unordered_set<struct zproto_struct *>	protocol;
static std::unordered_set<struct zproto_struct *>	defined;
static std::vector<std::string> querystmts;
struct stmt_args {
	std::string base;
	std::vector<std::string> encodestm;
	std::vector<std::string> decodestm;
	std::vector<std::string> resetstm;
	std::vector<std::string> stmts;
};

static std::string inline
fill_normal(struct zproto_field *f)
{
	char buff[512];
	const char *fmt =
	"\tcase %d:\n"
	"\t\treturn _write(_args, %s);\n";

	const char *afmt =
	"\tcase %d:\n"
	"\t\tassert(_args->idx >= 0);\n"
	"\t\tif (_args->idx >= (int)%s.size()) {\n"
	"\t\t\t_args->len = _args->idx;\n"
	"\t\t\treturn ZPROTO_NOFIELD;\n"
	"\t\t}\n"
	"\t\treturn _write(_args, %s[_args->idx]);\n";

	if (f->isarray)
		snprintf(buff, 512, afmt, f->tag, f->name, f->name);
	else
		snprintf(buff, 512, fmt, f->tag, f->name);
	return buff;
}

static std::string inline
to_normal(struct zproto_field *f)
{
	char buff[512];
	const char *fmt =
	"\tcase %d:\n"
	"\t\treturn _read(_args, %s);\n";

	const char *afmt =
	"\tcase %d:\n"
	"\t\tassert(_args->idx >= 0);\n"
	"\t\tif (_args->len == 0)\n"
	"\t\t\treturn 0;\n"
	"\t\t%s.resize(_args->idx + 1);\n"
	"\t\treturn _read(_args, %s[_args->idx]);\n";

	if (f->isarray)
		snprintf(buff, 512, afmt, f->tag, f->name, f->name);
	else
		snprintf(buff, 512, fmt, f->tag, f->name);
	return buff;
}

static std::string inline
reset_normal(struct zproto_field *f)
{
	const char *fmt = "";
	char buff[1024];
	if (f->isarray) {
		fmt = "\t%s.clear();\n";
	} else {
		switch (f->type) {
		case ZPROTO_BLOB:
		case ZPROTO_STRING:
			fmt = "\t%s.clear();\n";
			break;
		case ZPROTO_BOOLEAN:
			fmt = "\t%s = false;\n";
			break;
		case ZPROTO_BYTE:
		case ZPROTO_SHORT:
		case ZPROTO_INTEGER:
		case ZPROTO_LONG:
		case ZPROTO_UBYTE:
		case ZPROTO_USHORT:
		case ZPROTO_UINTEGER:
		case ZPROTO_ULONG:
			fmt = "\t%s = 0;\n";
			break;
		case ZPROTO_FLOAT:
			fmt = "\t%s = 0.0f;\n";
			break;
		}
	}
	snprintf(buff, 1024, fmt, f->name);
	return buff;
}

static std::string inline
fill_struct(struct zproto_field *f)
{
	char buff[1024];
	const char *fmt =
	"\tcase %d:\n"
	"\t\treturn %s._encode(_args->buff, _args->buffsz, _args->sttype);\n";

	const char *afmt =
	"\tcase %d:\n"
	"\t\tif (_args->idx >= (int)%s.size()) {\n"
	"\t\t\t_args->len = _args->idx;\n"
	"\t\t\treturn ZPROTO_NOFIELD;\n"
	"\t\t}\n"
	"\t\treturn %s[_args->idx]._encode(_args->buff, _args->buffsz, _args->sttype);\n";

	const char *mfmt =
	"\tcase %d: {\n"
	"\t\tint ret;\n"
	"\t\tif (_args->idx == 0) {\n"
	"\t\t\t_mapiterator.%s = %s.begin();\n"
	"\t\t}\n"
	"\t\tif (_mapiterator.%s == %s.end()) {\n"
	"\t\t\t_args->len = _args->idx;\n"
	"\t\t\treturn ZPROTO_NOFIELD;\n"
	"\t\t}\n"
	"\t\tret = _mapiterator.%s->second._encode(_args->buff, _args->buffsz, _args->sttype);\n"
	"\t\t++_mapiterator.%s;\n"
	"\t\treturn ret;}\n";

	if (f->mapkey) {
		assert(f->isarray);
		snprintf(buff, 1024, mfmt, f->tag,
				f->name, f->name,
				f->name, f->name,
				f->name,
				f->name);
	} else if (f->isarray) {
		snprintf(buff, 1024, afmt, f->tag, f->name, f->name);
	} else {
		snprintf(buff, 1024, fmt, f->tag, f->name);
	}
	return buff;
}

static std::string inline
to_struct(struct zproto_field *f)
{
	char buff[512];
	const char *fmt =
	"\tcase %d:\n"
	"\t\treturn %s._decode(_args->buff, _args->buffsz, _args->sttype);\n";
	const char *afmt =
	"\tcase %d:\n"
	"\t\tassert(_args->idx >= 0);\n"
	"\t\tif (_args->len == 0)\n"
	"\t\t\treturn 0;\n"
	"\t\t%s.resize(_args->idx + 1);\n"
	"\t\treturn %s[_args->idx]._decode(_args->buff, _args->buffsz, _args->sttype);\n";
	const char *mfmt =
	"\tcase %d: {\n"
	"\t\tint ret;\n"
	"\t\tstruct %s _tmp;\n"
	"\t\tassert(_args->idx >= 0);\n"
	"\t\tif (_args->len == 0)\n"
	"\t\treturn 0;\n"
	"\t\tret = _tmp._decode(_args->buff, _args->buffsz, _args->sttype);\n"
	"\t\t%s[_tmp.%s] = std::move(_tmp);\n"
	"\t\treturn ret;\n"
	"\t}\n";
	if (f->mapkey) {
		assert(f->isarray);
		snprintf(buff, 512, mfmt, f->tag, zproto_name(f->seminfo), f->name, f->mapkey->name);
	} else if (f->isarray) {
		snprintf(buff, 512, afmt, f->tag, f->name, f->name);
	} else {
		snprintf(buff, 512, fmt, f->tag, f->name);
	}
	return buff;
}

static std::string inline
reset_struct(struct zproto_field *f)
{
	char buff[1024];
	if (f->isarray)
		snprintf(buff, 1024, "\t%s.clear();\n", f->name);
	else
		snprintf(buff, 1024, "\t%s._reset();\n", f->name);
	return buff;
}

static std::string inline
format_code(const char *base, const char *name, const char *qualifier)
{
	const char *fmt =
	"int\n"
	"%s::%s(struct zproto_args *_args) %s\n"
	"{\n"
	"\tswitch (_args->tag) {\n";

	char buff[1024];
	snprintf(buff, 1024, fmt, base, name, qualifier);
	return buff;
}

static std::string inline
format_close()
{
	const char *fmt =
	"\tdefault:\n"
	"\t\treturn ZPROTO_ERROR;\n"
	"\t}\n"
	"}\n";
	return fmt;
}

static std::string inline
format_reset(const char *base)
{
	const char *fmt =
	"void\n"
	"%s::_reset()\n"
	"{\n";
	char buff[1024];
	snprintf(buff, 1024, fmt, base);
	return buff;
}


static std::string inline
format_name(const char *base)
{
	const char *fmt =
	"const char *\n"
	"%s::_name() const\n"
	"{\n"
	"\treturn \"%s\";\n"
	"}\n";

	char buff[1024];
	snprintf(buff, 1024, fmt, base, base);
	return buff;
}

static std::string inline
format_tag(const char *base, int tag)
{
	const char *fmt =
	"int\n"
	"%s::_tag() const\n"
	"{\n"
	"\treturn 0x%X;\n"
	"}\n";
	char buff[1024];
	snprintf(buff, 1024, fmt, base, tag);
	return buff;
}

static std::string inline
format_query(const char *base)
{
	const char *fmt =
	"struct zproto_struct *%s::_st = nullptr;\n"
	"struct zproto_struct *\n"
	"%s::_query() const\n"
	"{\n"
	"\treturn _st;\n"
	"}\n"
	"void\n"
	"%s::_load(wiretree &t)\n"
	"{\n"
	"\t%s::_st = t.query(\"%s\");\n"
	"\tassert(%s::_st);\n"
	"}\n";
	char buff[1024];
	snprintf(buff, 1024,"\t%s::_load(*this);\n", base);
	querystmts.push_back(buff);
	snprintf(buff, 1024, fmt,
			base,
			base,
			base,
			base, base,
			base);
	return buff;
}

static int prototype_cb(struct zproto_field *f, struct stmt_args *ud);

static void
formatst(struct zproto *z, struct zproto_struct *st, struct stmt_args &newargs)
{
	struct zproto_struct *const*start, *const*end;
	const struct zproto_starray *sts;
	sts = (st == NULL) ? zproto_root(z) : st->child;
	if (sts != NULL) {
		start = &sts->buf[0];
		end = start + sts->count;
		while (start < end) {
			struct zproto_struct *child = *start;
			struct stmt_args nnewargs;
			assert(protocol.count(child) == 0);
			assert(defined.count(child) == 0);
			nnewargs.base = newargs.base;
			nnewargs.base += "::";
			nnewargs.base += zproto_name(child);
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
	std::string tmp;

	if (protocol.count(st)) {
		//_tag()
		tmp = format_tag(newargs.base.c_str(), zproto_tag(st));
		newargs.stmts.push_back(tmp);
		//_name()
		tmp = format_name(newargs.base.c_str());
		newargs.stmts.push_back(tmp);
		//_query()
		tmp = format_query(newargs.base.c_str());
		newargs.stmts.push_back(tmp);
	}
	//_reset()
	tmp = format_reset(newargs.base.c_str());
	newargs.resetstm.insert(newargs.resetstm.begin(), tmp);
	newargs.resetstm.push_back("\n}\n");
	newargs.stmts.insert(newargs.stmts.end(), newargs.resetstm.begin(),
			newargs.resetstm.end());
	//_encode_field
	tmp = format_code(newargs.base.c_str(), "_encode_field", "const");
	newargs.encodestm.insert(newargs.encodestm.begin(), tmp);
	tmp = format_close();
	newargs.encodestm.push_back(tmp);

	newargs.stmts.insert(newargs.stmts.end(), newargs.encodestm.begin(),
			newargs.encodestm.end());
	//_decode_field
	tmp = format_code(newargs.base.c_str(), "_decode_field", "");
	newargs.decodestm.insert(newargs.decodestm.begin(), tmp);
	tmp = format_close();
	newargs.decodestm.push_back(tmp);
	newargs.stmts.insert(newargs.stmts.end(), newargs.decodestm.begin(),
			newargs.decodestm.end());
	//
	defined.insert(st);
	return ;
}

static int
prototype_cb(struct zproto_field *f, struct stmt_args *ud)
{
	struct stmt_args newargs;
	std::string estm;
	std::string dstm;
	std::string rstm;

	switch (f->type) {
	case ZPROTO_STRUCT:
		estm = fill_struct(f);
		dstm = to_struct(f);
		rstm = reset_struct(f);
		break;
	case ZPROTO_BLOB:
	case ZPROTO_STRING:
	case ZPROTO_BOOLEAN:
	case ZPROTO_FLOAT:
	case ZPROTO_BYTE:
	case ZPROTO_SHORT:
	case ZPROTO_INTEGER:
	case ZPROTO_LONG:
	case ZPROTO_UBYTE:
	case ZPROTO_USHORT:
	case ZPROTO_UINTEGER:
	case ZPROTO_ULONG:
		estm = fill_normal(f);
		dstm = to_normal(f);
		rstm = reset_normal(f);
		break;
	default:
		break;
	}
	ud->encodestm.push_back(estm);
	ud->decodestm.push_back(dstm);
	ud->resetstm.push_back(rstm);
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
	struct zproto_struct *const* start, *const* end;
	const struct zproto_starray *sts;
	sts = zproto_root(z);
	if (sts == NULL)
		return ;
	start = &sts->buf[0];
	end = start + sts->count;
	while (start < end) {
		struct stmt_args args;
		struct zproto_struct *st = *start;
		args.base = zproto_name(st);
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
	std::string hex = "const char *def = \"";
	for (i = 0; proto[i]; i++) {
		snprintf(buff, 8, "\\x%x", (uint8_t)proto[i]);
		hex += buff;
	}
	hex += "\";\n";
	fprintf(fp, "%s\n", hex.c_str());
	fprintf(fp,
"serializer::serializer()\n"
"	 :wiretree(def)\n"
"{\n"
);
	dump_vecstring(fp, querystmts);
	fprintf(fp,
"}\n"
"serializer &\n"
"serializer::instance()\n"
"{\n"
"	 static serializer *inst = new serializer();\n"
"	 return *inst;\n"
"}\n");
}

static const char *wirep =
"wiretree&\n"
"wirepimpl::_wiretree() const\n"
"{\n"
"	return serializer::instance();\n"
"}\n\n";

void
body(const char *name, std::vector<const char*> &space, const char *proto, struct zproto *z)
{
	FILE *fp;
	std::string path = name;
	path += ".cc";
	fp = fopen(path.c_str(), "wb+");

	const struct zproto_starray *sts = zproto_root(z);
	if (sts != NULL) {
		struct zproto_struct *const* start, *const* end;
		start = &sts->buf[0];
		end = start + sts->count;
		while (start < end) {
			struct zproto_struct *st = *start;
			protocol.insert(st);
			++start;
		}
	}

	fprintf(fp, "#include <string.h>\n");
	fprintf(fp, "#include \"zprotowire.h\"\n");
	fprintf(fp, "#include \"%s.hpp\"\n", name);
	for (const auto p:space)
		fprintf(fp, "namespace %s {\n", p);
	fprintf(fp, "%s", "\nusing namespace zprotobuf;\n\n");
	fprintf(fp, "%s", wirep);
	dumpst(fp, z);
	wiretree(fp, proto);
	for (size_t i = 0; i < space.size(); i++)
		fprintf(fp, "}");
	fprintf(fp, "\n");
	fclose(fp);
}


