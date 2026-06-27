#include <assert.h>
#include <vector>
#include <string>
#include <unordered_set>

#include "zproto.hpp"

static std::unordered_set<struct zproto_struct *>	protocol;
static std::unordered_set<struct zproto_struct *>	defined;
struct stmt_args {
	std::string base;
	std::vector<std::string> encodestm;
	std::vector<std::string> decodestm;
	std::vector<std::string> resetstm;
	std::vector<std::string> stmts;
};

// map a scalar type id to its encoder write method name
static const char *
wmethod(int type)
{
	switch (type) {
	case ZPROTO_BOOLEAN:
	case ZPROTO_BYTE:
	case ZPROTO_UBYTE:
		return "w_u8";
	case ZPROTO_SHORT:
	case ZPROTO_USHORT:
		return "w_u16";
	case ZPROTO_INTEGER:
	case ZPROTO_UINTEGER:
		return "w_u32";
	case ZPROTO_LONG:
	case ZPROTO_ULONG:
		return "w_u64";
	case ZPROTO_FLOAT:
		return "w_f32";
	default:
		assert(0);
		return NULL;
	}
}

// map a scalar type id to its decoder read method name
static const char *
rmethod(int type)
{
	switch (type) {
	case ZPROTO_BOOLEAN:
	case ZPROTO_BYTE:
	case ZPROTO_UBYTE:
		return "r_u8";
	case ZPROTO_SHORT:
	case ZPROTO_USHORT:
		return "r_u16";
	case ZPROTO_INTEGER:
	case ZPROTO_UINTEGER:
		return "r_u32";
	case ZPROTO_LONG:
	case ZPROTO_ULONG:
		return "r_u64";
	case ZPROTO_FLOAT:
		return "r_f32";
	default:
		assert(0);
		return NULL;
	}
}

// the (T) cast wrapper applied to the value when encoding a scalar.
// signedness is irrelevant on the wire: the cast reinterprets the bit
// pattern into the unsigned width the encoder expects.
static const char *
ecast(int type)
{
	switch (type) {
	case ZPROTO_BOOLEAN:	return "(uint8_t)";
	case ZPROTO_BYTE:	return "(uint8_t)";
	case ZPROTO_UBYTE:	return "(uint8_t)";
	case ZPROTO_SHORT:	return "(uint16_t)";
	case ZPROTO_USHORT:	return "(uint16_t)";
	case ZPROTO_INTEGER:	return "(uint32_t)";
	case ZPROTO_UINTEGER:	return "(uint32_t)";
	case ZPROTO_LONG:	return "(uint64_t)";
	case ZPROTO_ULONG:	return "(uint64_t)";
	case ZPROTO_FLOAT:	return "";
	default:
		assert(0);
		return "";
	}
}

// the (T) cast applied to the decoder's unsigned read result so it
// assigns back into the (possibly signed) member with the same bits.
static const char *
dcast(int type)
{
	switch (type) {
	case ZPROTO_BOOLEAN:	return "";	// handled specially: == 1
	case ZPROTO_BYTE:	return "(int8_t)";
	case ZPROTO_UBYTE:	return "";
	case ZPROTO_SHORT:	return "(int16_t)";
	case ZPROTO_USHORT:	return "";
	case ZPROTO_INTEGER:	return "(int32_t)";
	case ZPROTO_UINTEGER:	return "";
	case ZPROTO_LONG:	return "(int64_t)";
	case ZPROTO_ULONG:	return "";
	case ZPROTO_FLOAT:	return "";
	default:
		assert(0);
		return "";
	}
}

// returns the encode statement(s) for one field, emitted in
// declaration order. Drives the schema-agnostic encoder directly.
static std::string
encode_stmt(struct zproto_field *f)
{
	char b[1024];
	if (f->mapkey) {                         // struct map: array of struct values
		snprintf(b, sizeof(b),
			"\te.present(%d);\n"
			"\te.w_array((uint32_t)this->%s.size());\n"
			"\tfor (const auto &kv : this->%s) {\n"
			"\t\tkv.second._encode(out);\n"
			"\t}\n",
			f->tag, f->name, f->name);
	} else if (f->type == ZPROTO_STRUCT && f->isarray) {
		snprintf(b, sizeof(b),
			"\te.present(%d);\n"
			"\te.w_array((uint32_t)this->%s.size());\n"
			"\tfor (const auto &v : this->%s) {\n"
			"\t\tv._encode(out);\n"
			"\t}\n",
			f->tag, f->name, f->name);
	} else if (f->type == ZPROTO_STRUCT) {
		snprintf(b, sizeof(b),
			"\te.present(%d);\n"
			"\tthis->%s._encode(out);\n",
			f->tag, f->name);
	} else if ((f->type == ZPROTO_STRING || f->type == ZPROTO_BLOB) && f->isarray) {
		snprintf(b, sizeof(b),
			"\te.present(%d);\n"
			"\te.w_array((uint32_t)this->%s.size());\n"
			"\tfor (const auto &v : this->%s) {\n"
			"\t\te.w_bytes(v.data(), v.size());\n"
			"\t}\n",
			f->tag, f->name, f->name);
	} else if (f->isarray) {
		const char *w = wmethod(f->type);
		const char *c = ecast(f->type);
		snprintf(b, sizeof(b),
			"\te.present(%d);\n"
			"\te.w_array((uint32_t)this->%s.size());\n"
			"\tfor (const auto &v : this->%s) {\n"
			"\t\te.%s(%sv);\n"
			"\t}\n",
			f->tag, f->name, f->name, w, c);
	} else if (f->type == ZPROTO_STRING || f->type == ZPROTO_BLOB) {
		snprintf(b, sizeof(b),
			"\te.present(%d);\n"
			"\te.w_bytes(this->%s.data(), this->%s.size());\n",
			f->tag, f->name, f->name);
	} else {
		const char *w = wmethod(f->type);
		const char *c = ecast(f->type);
		snprintf(b, sizeof(b),
			"\te.present(%d);\n"
			"\te.%s(%sthis->%s);\n",
			f->tag, w, c, f->name);
	}
	return b;
}


// returns the "case TAG: ...; break;" decode statement for one field.
// Mirrors the reference shape: scalar / array / string / struct / map.
static std::string
decode_stmt(struct zproto_field *f)
{
	char b[1024];
	if (f->mapkey) {                         // struct map
		snprintf(b, sizeof(b),
			"\t\tcase %d: {\n"
			"\t\t\tuint32_t c = d.r_array();\n"
			"\t\t\tfor (uint32_t i = 0; i < c; i++) {\n"
			"\t\t\t\tsize_t n;\n"
			"\t\t\t\tconst uint8_t *p = d.struct_bytes(n);\n"
			"\t\t\t\tstruct %s t;\n"
			"\t\t\t\tt._decode(p, n);\n"
			"\t\t\t\tthis->%s[t.%s] = std::move(t);\n"
			"\t\t\t}\n"
			"\t\t\tbreak;\n"
			"\t\t}\n",
			f->tag, zproto_name(f->seminfo), f->name, f->mapkey->name);
	} else if (f->type == ZPROTO_STRUCT && f->isarray) {
		snprintf(b, sizeof(b),
			"\t\tcase %d: {\n"
			"\t\t\tuint32_t c = d.r_array();\n"
			"\t\t\tthis->%s.resize(c);\n"
			"\t\t\tfor (uint32_t i = 0; i < c; i++) {\n"
			"\t\t\t\tsize_t n;\n"
			"\t\t\t\tconst uint8_t *p = d.struct_bytes(n);\n"
			"\t\t\t\tthis->%s[i]._decode(p, n);\n"
			"\t\t\t}\n"
			"\t\t\tbreak;\n"
			"\t\t}\n",
			f->tag, f->name, f->name);
	} else if (f->type == ZPROTO_STRUCT) {
		snprintf(b, sizeof(b),
			"\t\tcase %d: {\n"
			"\t\t\tsize_t n;\n"
			"\t\t\tconst uint8_t *p = d.struct_bytes(n);\n"
			"\t\t\tthis->%s._decode(p, n);\n"
			"\t\t\tbreak;\n"
			"\t\t}\n",
			f->tag, f->name);
	} else if ((f->type == ZPROTO_STRING || f->type == ZPROTO_BLOB) && f->isarray) {
		snprintf(b, sizeof(b),
			"\t\tcase %d: {\n"
			"\t\t\tuint32_t c = d.r_array();\n"
			"\t\t\tthis->%s.resize(c);\n"
			"\t\t\tfor (uint32_t i = 0; i < c; i++) {\n"
			"\t\t\t\td.r_bytes(this->%s[i]);\n"
			"\t\t\t}\n"
			"\t\t\tbreak;\n"
			"\t\t}\n",
			f->tag, f->name, f->name);
	} else if (f->isarray) {
		const char *w = rmethod(f->type);
		if (f->type == ZPROTO_BOOLEAN) {
			snprintf(b, sizeof(b),
				"\t\tcase %d: {\n"
				"\t\t\tuint32_t c = d.r_array();\n"
				"\t\t\tthis->%s.resize(c);\n"
				"\t\t\tfor (uint32_t i = 0; i < c; i++) {\n"
				"\t\t\t\tthis->%s[i] = (d.r_u8() == 1);\n"
				"\t\t\t}\n"
				"\t\t\tbreak;\n"
				"\t\t}\n",
				f->tag, f->name, f->name);
		} else {
			const char *c = dcast(f->type);
			snprintf(b, sizeof(b),
				"\t\tcase %d: {\n"
				"\t\t\tuint32_t c = d.r_array();\n"
				"\t\t\tthis->%s.resize(c);\n"
				"\t\t\tfor (uint32_t i = 0; i < c; i++) {\n"
				"\t\t\t\tthis->%s[i] = %sd.%s();\n"
				"\t\t\t}\n"
				"\t\t\tbreak;\n"
				"\t\t}\n",
				f->tag, f->name, f->name, c, w);
		}
	} else if (f->type == ZPROTO_STRING || f->type == ZPROTO_BLOB) {
		snprintf(b, sizeof(b),
			"\t\tcase %d:\n"
			"\t\t\td.r_bytes(this->%s);\n"
			"\t\t\tbreak;\n",
			f->tag, f->name);
	} else if (f->type == ZPROTO_BOOLEAN) {
		snprintf(b, sizeof(b),
			"\t\tcase %d:\n"
			"\t\t\tthis->%s = (d.r_u8() == 1);\n"
			"\t\t\tbreak;\n",
			f->tag, f->name);
	} else {
		const char *w = rmethod(f->type);
		const char *c = dcast(f->type);
		snprintf(b, sizeof(b),
			"\t\tcase %d:\n"
			"\t\t\tthis->%s = %sd.%s();\n"
			"\t\t\tbreak;\n",
			f->tag, f->name, c, w);
	}
	return b;
}

static std::string inline
reset_normal(struct zproto_field *f)
{
	const char *fmt = "";
	char buff[1024];
	if (f->isarray) {
		fmt = "\tthis->%s.clear();\n";
	} else {
		switch (f->type) {
		case ZPROTO_BLOB:
		case ZPROTO_STRING:
			fmt = "\tthis->%s.clear();\n";
			break;
		case ZPROTO_BOOLEAN:
			fmt = "\tthis->%s = false;\n";
			break;
		case ZPROTO_BYTE:
		case ZPROTO_SHORT:
		case ZPROTO_INTEGER:
		case ZPROTO_LONG:
		case ZPROTO_UBYTE:
		case ZPROTO_USHORT:
		case ZPROTO_UINTEGER:
		case ZPROTO_ULONG:
			fmt = "\tthis->%s = 0;\n";
			break;
		case ZPROTO_FLOAT:
			fmt = "\tthis->%s = 0.0f;\n";
			break;
		}
	}
	snprintf(buff, 1024, fmt, f->name);
	return buff;
}

static std::string inline
reset_struct(struct zproto_field *f)
{
	char buff[1024];
	if (f->isarray)
		snprintf(buff, 1024, "\tthis->%s.clear();\n", f->name);
	else
		snprintf(buff, 1024, "\tthis->%s._reset();\n", f->name);
	return buff;
}

// _encode(std::string &) wrapper: construct an encoder over the output
// string, drive each present field in declaration order, then finish().
static std::string inline
format_encode(const char *base, int basetag, int fieldcount)
{
	const char *fmt =
	"int\n"
	"%s::_encode(std::string &out) const\n"
	"{\n"
	"\tzprotobuf::encoder e(out, %d, %d);\n";
	char buff[256];
	snprintf(buff, sizeof(buff), fmt, base, basetag, fieldcount);
	return buff;
}

static std::string inline
format_encode_close()
{
	return "\treturn e.finish();\n}\n";
}

// _decode(const uint8_t *, size_t) wrapper: construct a decoder, loop
// next(tag), switch on tag reading each field, default stops.
static std::string inline
format_decode(const char *base, int basetag)
{
	const char *fmt =
	"int\n"
	"%s::_decode(const uint8_t *data, size_t sz)\n"
	"{\n"
	"\tzprotobuf::decoder d(data, sz, %d);\n"
	"\tfor (int tag; d.next(tag); ) {\n"
	"\t\tswitch (tag) {\n";
	char buff[256];
	snprintf(buff, sizeof(buff), fmt, base, basetag);
	return buff;
}

static std::string inline
format_decode_close()
{
	// unknown tag -> return 0 (forward-compat stop); full message ->
	// d.size() (SIZEOF_LEN + datasize, matching zproto_decode's return).
	return "\t\tdefault:\n\t\t\treturn 0;\n\t\t}\n\t}\n\treturn (int)d.size();\n}\n";
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
	}
	//_reset()
	tmp = format_reset(newargs.base.c_str());
	newargs.resetstm.insert(newargs.resetstm.begin(), tmp);
	newargs.resetstm.push_back("\n}\n");
	newargs.stmts.insert(newargs.stmts.end(), newargs.resetstm.begin(),
			newargs.resetstm.end());
	//_encode(std::string &)
	tmp = format_encode(newargs.base.c_str(), st->basetag, st->fieldcount);
	newargs.encodestm.insert(newargs.encodestm.begin(), tmp);
	newargs.stmts.insert(newargs.stmts.end(), newargs.encodestm.begin(),
			newargs.encodestm.end());
	tmp = format_encode_close();
	newargs.stmts.push_back(tmp);
	//_decode(const uint8_t *, size_t)
	tmp = format_decode(newargs.base.c_str(), st->basetag);
	newargs.decodestm.insert(newargs.decodestm.begin(), tmp);
	newargs.stmts.insert(newargs.stmts.end(), newargs.decodestm.begin(),
			newargs.decodestm.end());
	tmp = format_decode_close();
	newargs.stmts.push_back(tmp);
	//
	defined.insert(st);
	return ;
}

static int
prototype_cb(struct zproto_field *f, struct stmt_args *ud)
{
	std::string estm;
	std::string dstm;
	std::string rstm;

	switch (f->type) {
	case ZPROTO_STRUCT:
		estm = encode_stmt(f);
		dstm = decode_stmt(f);
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
		estm = encode_stmt(f);
		dstm = decode_stmt(f);
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

void
body(const char *name, std::vector<const char*> &space, const char * /*proto*/, struct zproto *z)
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
	dumpst(fp, z);
	for (size_t i = 0; i < space.size(); i++)
		fprintf(fp, "}");
	fprintf(fp, "\n");
	fclose(fp);
}
