#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <unordered_map>

extern "C" {
#include "zproto.h"
}

// phase 2b: emit X.message.hpp — a namespace-zproto block of message<T> specializations
// (one per struct). Each carries tag()/name() (tagged structs only), byte_size(),
// write_at() (header + deltas + body, cursor-threaded into a pre-sized Buffer),
// encode() (resize once + write_at), and decode() (schema-agnostic decoder switch,
// recursing via message<E>::decode for nested structs). No encoder object, no
// virtuals, no .cc — the data structs stay clean PODs defined in X.hpp.

// fully-qualified C++ type name for each struct, e.g. "::bench::frame" or
// "::hello::world::packet::phone". Always carries its full leading scope so it
// resolves from inside namespace zproto, where message<> lives.
static std::unordered_map<struct zproto_struct *, std::string> qname;
// "::ns1::ns2::" prefix (or "::" when the schema has no namespace).
static std::string qualprefix;

// ---- scalar type tables (mirror the wire widths / decoder readers) ----

// wire width in bytes for a scalar type.
static int
width(int type)
{
	switch (type) {
	case ZPROTO_BOOLEAN:
	case ZPROTO_BYTE:
	case ZPROTO_UBYTE:
		return 1;
	case ZPROTO_SHORT:
	case ZPROTO_USHORT:
		return 2;
	case ZPROTO_INTEGER:
	case ZPROTO_UINTEGER:
	case ZPROTO_FLOAT:
		return 4;
	case ZPROTO_LONG:
	case ZPROTO_ULONG:
		return 8;
	default:
		assert(0);
		return 0;
	}
}

// runtime put_uXX primitive a scalar is written with. float reuses put_u32 but
// its argument is produced via a memcpy bit-cast by the caller.
static const char *
putfn(int type)
{
	switch (type) {
	case ZPROTO_BOOLEAN:
	case ZPROTO_BYTE:
	case ZPROTO_UBYTE:
		return "put_u8";
	case ZPROTO_SHORT:
	case ZPROTO_USHORT:
		return "put_u16";
	case ZPROTO_INTEGER:
	case ZPROTO_UINTEGER:
	case ZPROTO_FLOAT:
		return "put_u32";
	case ZPROTO_LONG:
	case ZPROTO_ULONG:
		return "put_u64";
	default:
		assert(0);
		return NULL;
	}
}

// the (uintN_t) cast applied to a scalar value before writing: signedness is
// irrelevant on the wire, the cast only reinterprets the bit pattern into the
// unsigned width put_uXX expects.
static const char *
ecast(int type)
{
	switch (type) {
	case ZPROTO_BOOLEAN:
	case ZPROTO_BYTE:
	case ZPROTO_UBYTE:
		return "(uint8_t)";
	case ZPROTO_SHORT:
	case ZPROTO_USHORT:
		return "(uint16_t)";
	case ZPROTO_INTEGER:
	case ZPROTO_UINTEGER:
		return "(uint32_t)";
	case ZPROTO_LONG:
	case ZPROTO_ULONG:
		return "(uint64_t)";
	case ZPROTO_FLOAT:
		return "";
	default:
		assert(0);
		return "";
	}
}

// decoder read method name for a scalar type.
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

// cast applied to the decoder's unsigned read so it assigns back into the
// (possibly signed) member with the same bits.
static const char *
dcast(int type)
{
	switch (type) {
	case ZPROTO_BOOLEAN:
		return "";		// handled specially: == 1
	case ZPROTO_BYTE:
		return "(int8_t)";
	case ZPROTO_UBYTE:
		return "";
	case ZPROTO_SHORT:
		return "(int16_t)";
	case ZPROTO_USHORT:
		return "";
	case ZPROTO_INTEGER:
		return "(int32_t)";
	case ZPROTO_UINTEGER:
		return "";
	case ZPROTO_LONG:
		return "(int64_t)";
	case ZPROTO_ULONG:
		return "";
	case ZPROTO_FLOAT:
		return "";
	default:
		assert(0);
		return "";
	}
}

// ---- qualified-name collection --------------------------------------------
// post-order DFS over the struct tree: records every struct's full type so a
// field referencing struct E resolves regardless of where E sits. Emission
// (dump_struct) uses the same post-order, so message<E> is always defined before
// any message<X> that names it as a field type (required: message<concrete-type> is a
// non-dependent name, so the specialization must be complete at that point).

static void
collect(struct zproto_struct *st, const std::string &base)
{
	qname[st] = qualprefix + base;
	const struct zproto_starray *sts = st->child;
	if (sts != NULL) {
		for (int i = 0; i < sts->count; i++) {
			struct zproto_struct *child = sts->buf[i];
			collect(child, base + "::" + zproto_name(child));
		}
	}
}

// ---- per-field body-size statements (summed into byte_size) ---------------

static std::string
byte_size_stmt(struct zproto_field *f)
{
	char b[1024];
	const char *fn = f->name;
	if (f->mapkey) {                                  // struct map: array of values
		snprintf(b, sizeof(b),
			"\t\tn += 4;\n"
			"\t\tfor (const auto &kv : o.%s) n += message<%s>::byte_size(kv.second);\n",
			fn, qname[f->seminfo].c_str());
	} else if (f->type == ZPROTO_STRUCT && f->isarray) {
		snprintf(b, sizeof(b),
			"\t\tn += 4;\n"
			"\t\tfor (const auto &v : o.%s) n += message<%s>::byte_size(v);\n",
			fn, qname[f->seminfo].c_str());
	} else if (f->type == ZPROTO_STRUCT) {
		snprintf(b, sizeof(b),
			"\t\tn += message<%s>::byte_size(o.%s);\n",
			qname[f->seminfo].c_str(), fn);
	} else if ((f->type == ZPROTO_STRING || f->type == ZPROTO_BLOB) && f->isarray) {
		snprintf(b, sizeof(b),
			"\t\tn += 4;\n"
			"\t\tfor (const auto &v : o.%s) n += 4 + v.size();\n", fn);
	} else if (f->isarray) {
		snprintf(b, sizeof(b),
			"\t\tn += 4 + o.%s.size() * %d;\n", fn, width(f->type));
	} else if (f->type == ZPROTO_STRING || f->type == ZPROTO_BLOB) {
		snprintf(b, sizeof(b),
			"\t\tn += 4 + o.%s.size();\n", fn);
	} else {
		snprintf(b, sizeof(b),
			"\t\tn += %d;\n", width(f->type));
	}
	return b;
}

// ---- per-field body-write statements (cursor-threaded into write_at) ------

static std::string
write_at_stmt(struct zproto_field *f)
{
	char b[2048];
	const char *fn = f->name;
	if (f->mapkey) {
		snprintf(b, sizeof(b),
			"\t\tput_u32(b + cur, (uint32_t)o.%s.size()); cur += 4;\n"
			"\t\tfor (const auto &kv : o.%s) cur = message<%s>::write_at(kv.second, buf, cur);\n",
			fn, fn, qname[f->seminfo].c_str());
	} else if (f->type == ZPROTO_STRUCT && f->isarray) {
		snprintf(b, sizeof(b),
			"\t\tput_u32(b + cur, (uint32_t)o.%s.size()); cur += 4;\n"
			"\t\tfor (const auto &v : o.%s) cur = message<%s>::write_at(v, buf, cur);\n",
			fn, fn, qname[f->seminfo].c_str());
	} else if (f->type == ZPROTO_STRUCT) {
		snprintf(b, sizeof(b),
			"\t\tcur = message<%s>::write_at(o.%s, buf, cur);\n",
			qname[f->seminfo].c_str(), fn);
	} else if ((f->type == ZPROTO_STRING || f->type == ZPROTO_BLOB) && f->isarray) {
		snprintf(b, sizeof(b),
			"\t\tput_u32(b + cur, (uint32_t)o.%s.size()); cur += 4;\n"
			"\t\tfor (const auto &v : o.%s) {\n"
			"\t\t\tput_u32(b + cur, (uint32_t)v.size()); cur += 4;\n"
			"\t\t\tstd::memcpy(b + cur, v.data(), v.size()); cur += v.size();\n"
			"\t\t}\n",
			fn, fn);
	} else if (f->isarray && f->type == ZPROTO_FLOAT) {
		snprintf(b, sizeof(b),
			"\t\tput_u32(b + cur, (uint32_t)o.%s.size()); cur += 4;\n"
			"\t\tfor (const auto &v : o.%s) { uint32_t _u; std::memcpy(&_u, &v, 4); put_u32(b + cur, _u); cur += 4; }\n",
			fn, fn);
	} else if (f->isarray) {
		snprintf(b, sizeof(b),
			"\t\tput_u32(b + cur, (uint32_t)o.%s.size()); cur += 4;\n"
			"\t\tfor (const auto &v : o.%s) { %s(b + cur, %sv); cur += %d; }\n",
			fn, fn, putfn(f->type), ecast(f->type), width(f->type));
	} else if (f->type == ZPROTO_STRING || f->type == ZPROTO_BLOB) {
		snprintf(b, sizeof(b),
			"\t\tput_u32(b + cur, (uint32_t)o.%s.size()); cur += 4;\n"
			"\t\tstd::memcpy(b + cur, o.%s.data(), o.%s.size()); cur += o.%s.size();\n",
			fn, fn, fn, fn);
	} else if (f->type == ZPROTO_FLOAT) {
		snprintf(b, sizeof(b),
			"\t\t{ uint32_t _u; std::memcpy(&_u, &o.%s, 4); put_u32(b + cur, _u); } cur += 4;\n", fn);
	} else {
		snprintf(b, sizeof(b),
			"\t\t%s(b + cur, %so.%s); cur += %d;\n",
			putfn(f->type), ecast(f->type), fn, width(f->type));
	}
	return b;
}

// ---- per-field decode case (switch shape; message<E>::decode for nested) ---

static std::string
decode_stmt(struct zproto_field *f)
{
	char b[2048];
	const char *fn = f->name;
	if (f->mapkey) {
		snprintf(b, sizeof(b),
			"\t\tcase %d: {\n"
			"\t\t\tuint32_t c = d.r_array();\n"
			"\t\t\tfor (uint32_t i = 0; i < c; i++) {\n"
			"\t\t\t\tsize_t n;\n"
			"\t\t\t\tconst uint8_t *p = d.struct_bytes(n);\n"
			"\t\t\t\tif (!d.ok)\n"
			"\t\t\t\t\tbreak;\n"
			"\t\t\t\t%s t;\n"
			"\t\t\t\tmessage<%s>::decode(t, p, n);\n"
			"\t\t\t\to.%s[t.%s] = std::move(t);\n"
			"\t\t\t}\n"
			"\t\t\tbreak;\n"
			"\t\t}\n",
			f->tag, qname[f->seminfo].c_str(), qname[f->seminfo].c_str(),
			fn, f->mapkey->name);
	} else if (f->type == ZPROTO_STRUCT && f->isarray) {
		snprintf(b, sizeof(b),
			"\t\tcase %d: {\n"
			"\t\t\tuint32_t c = d.r_array();\n"
			"\t\t\tfor (uint32_t i = 0; i < c; i++) {\n"
			"\t\t\t\tsize_t n;\n"
			"\t\t\t\tconst uint8_t *p = d.struct_bytes(n);\n"
			"\t\t\t\tif (!d.ok)\n"
			"\t\t\t\t\tbreak;\n"
			"\t\t\t\t%s t;\n"
			"\t\t\t\tmessage<%s>::decode(t, p, n);\n"
			"\t\t\t\to.%s.push_back(std::move(t));\n"
			"\t\t\t}\n"
			"\t\t\tbreak;\n"
			"\t\t}\n",
			f->tag, qname[f->seminfo].c_str(), qname[f->seminfo].c_str(), fn);
	} else if (f->type == ZPROTO_STRUCT) {
		snprintf(b, sizeof(b),
			"\t\tcase %d: {\n"
			"\t\t\tsize_t n;\n"
			"\t\t\tconst uint8_t *p = d.struct_bytes(n);\n"
			"\t\t\tmessage<%s>::decode(o.%s, p, n);\n"
			"\t\t\tbreak;\n"
			"\t\t}\n",
			f->tag, qname[f->seminfo].c_str(), fn);
	} else if ((f->type == ZPROTO_STRING || f->type == ZPROTO_BLOB) && f->isarray) {
		snprintf(b, sizeof(b),
			"\t\tcase %d: {\n"
			"\t\t\tuint32_t c = d.r_array();\n"
			"\t\t\tfor (uint32_t i = 0; i < c; i++) {\n"
			"\t\t\t\tstd::string s;\n"
			"\t\t\t\td.r_bytes(s);\n"
			"\t\t\t\tif (!d.ok)\n"
			"\t\t\t\t\tbreak;\n"
			"\t\t\t\to.%s.push_back(std::move(s));\n"
			"\t\t\t}\n"
			"\t\t\tbreak;\n"
			"\t\t}\n",
			f->tag, fn);
	} else if (f->isarray) {
		if (f->type == ZPROTO_BOOLEAN) {
			snprintf(b, sizeof(b),
				"\t\tcase %d: {\n"
				"\t\t\tuint32_t c = d.r_array();\n"
				"\t\t\tfor (uint32_t i = 0; i < c; i++) {\n"
				"\t\t\t\tuint8_t v = d.r_u8();\n"
				"\t\t\t\tif (!d.ok)\n"
				"\t\t\t\t\tbreak;\n"
				"\t\t\t\to.%s.push_back(v == 1);\n"
				"\t\t\t}\n"
				"\t\t\tbreak;\n"
				"\t\t}\n",
				f->tag, fn);
		} else {
			snprintf(b, sizeof(b),
				"\t\tcase %d: {\n"
				"\t\t\tuint32_t c = d.r_array();\n"
				"\t\t\tfor (uint32_t i = 0; i < c; i++) {\n"
				"\t\t\t\tauto v = %sd.%s();\n"
				"\t\t\t\tif (!d.ok)\n"
				"\t\t\t\t\tbreak;\n"
				"\t\t\t\to.%s.push_back(v);\n"
				"\t\t\t}\n"
				"\t\t\tbreak;\n"
				"\t\t}\n",
				f->tag, dcast(f->type), rmethod(f->type), fn);
		}
	} else if (f->type == ZPROTO_STRING || f->type == ZPROTO_BLOB) {
		snprintf(b, sizeof(b),
			"\t\tcase %d:\n"
			"\t\t\td.r_bytes(o.%s);\n"
			"\t\t\tbreak;\n",
			f->tag, fn);
	} else if (f->type == ZPROTO_BOOLEAN) {
		snprintf(b, sizeof(b),
			"\t\tcase %d:\n"
			"\t\t\to.%s = (d.r_u8() == 1);\n"
			"\t\t\tbreak;\n",
			f->tag, fn);
	} else {
		snprintf(b, sizeof(b),
			"\t\tcase %d:\n"
			"\t\t\to.%s = %sd.%s();\n"
			"\t\t\tbreak;\n",
			f->tag, fn, dcast(f->type), rmethod(f->type));
	}
	return b;
}

// emit one message<T> specialization: tag/name (tagged structs only) + byte_size +
// write_at + encode + decode.
static void
emit_struct(FILE *fp, struct zproto_struct *st)
{
	const std::string &Q = qname[st];
	int fc = st->fieldcount;
	// byte_size only reads o when some field is variable-size (string/blob/
	// array/struct); fixed-scalar structs leave the param unnamed (matches the
	// hand-written prototype and keeps production builds warning-free).
	bool byte_uses_o = false;
	for (int i = 0; i < fc; i++) {
		struct zproto_field *f = st->fields[i];
		if (f->type == ZPROTO_STRING || f->type == ZPROTO_BLOB ||
		    f->isarray || f->type == ZPROTO_STRUCT)
			byte_uses_o = true;
	}
	bool body_uses_o = (fc > 0);   // write_at / decode touch o on every field

	fprintf(fp, "template<> struct message<%s> {\n", Q.c_str());
	if (st->tag != 0) {
		fprintf(fp, "\tstatic constexpr int tag() { return 0x%X; }\n", zproto_tag(st));
		fprintf(fp, "\tstatic constexpr const char *name() { return \"%s\"; }\n", zproto_name(st));
	}

	// byte_size: full message size incl. the 4-byte datasize prefix.
	//   6 + fc*2 = datasize(4) + tagcount(2) + fc delta slots.
	fprintf(fp, "\tstatic size_t byte_size(const %s &%s) {\n", Q.c_str(), byte_uses_o ? "o" : "");
	fprintf(fp, "\t\tsize_t n = 6 + %d * 2;\n", fc);
	for (int i = 0; i < fc; i++)
		fprintf(fp, "%s", byte_size_stmt(st->fields[i]).c_str());
	fprintf(fp, "\t\treturn n;\n");
	fprintf(fp, "\t}\n");

	// write_at: [u32 datasize][u16 tagcount][fc*u16 delta][body], then patch
	// datasize. deltas are precomputed (basetag-1 is the implicit prev for the
	// first field, so its delta is always 0).
	fprintf(fp, "\tstatic size_t write_at(const %s &%s, Buffer &buf, size_t cur) {\n", Q.c_str(), body_uses_o ? "o" : "");
	fprintf(fp, "\t\tchar *b = buf.data();\n");
	fprintf(fp, "\t\tsize_t start = cur;\n");
	fprintf(fp, "\t\tcur += 4;\n");
	fprintf(fp, "\t\tput_u16(b + cur, (uint16_t)%d); cur += 2;\n", fc);
	for (int i = 0; i < fc; i++) {
		int prev = (i == 0) ? (st->basetag - 1) : st->fields[i - 1]->tag;
		int delta = st->fields[i]->tag - prev - 1;
		fprintf(fp, "\t\tput_u16(b + cur, (uint16_t)%d); cur += 2;\n", delta);
	}
	for (int i = 0; i < fc; i++)
		fprintf(fp, "%s", write_at_stmt(st->fields[i]).c_str());
	fprintf(fp, "\t\tput_u32(b + start, (uint32_t)(cur - start - 4));\n");
	fprintf(fp, "\t\treturn cur;\n");
	fprintf(fp, "\t}\n");

	// encode: resize once to the exact size, then write_at at the tail.
	fprintf(fp, "\tstatic void encode(const %s &o, Buffer &buf) {\n", Q.c_str());
	fprintf(fp, "\t\tsize_t off = buf.size();\n");
	fprintf(fp, "\t\tbuf.resize(off + byte_size(o));\n");
	fprintf(fp, "\t\twrite_at(o, buf, off);\n");
	fprintf(fp, "\t}\n");

	// decode: drive the schema-agnostic decoder; switch on the recovered tag.
	// unknown tag -> stop (forward-compat).
	fprintf(fp, "\tstatic void decode(%s &%s, const uint8_t *data, size_t sz) {\n", Q.c_str(), body_uses_o ? "o" : "");
	fprintf(fp, "\t\tdecoder d(data, sz, %d);\n", st->basetag);
	fprintf(fp, "\t\tfor (int tag; d.next(tag); ) {\n");
	fprintf(fp, "\t\t\tswitch (tag) {\n");
	for (int i = 0; i < fc; i++)
		fprintf(fp, "%s", decode_stmt(st->fields[i]).c_str());
	fprintf(fp, "\t\t\tdefault:\n");
	fprintf(fp, "\t\t\t\treturn;\n");
	fprintf(fp, "\t\t\t}\n");
	fprintf(fp, "\t\t}\n");
	fprintf(fp, "\t}\n");

	fprintf(fp, "};\n\n");
}

// emit every struct's specialization in post-order (children before parent),
// matching header.cpp's nesting so a referenced message<E> precedes message<X>.
static void
dump_struct(FILE *fp, struct zproto_struct *st)
{
	const struct zproto_starray *sts = st->child;
	if (sts != NULL) {
		for (int i = 0; i < sts->count; i++)
			dump_struct(fp, sts->buf[i]);
	}
	emit_struct(fp, st);
}

void
body(const char *name, std::vector<const char *> &space, const char * /*proto*/, struct zproto *z)
{
	FILE *fp;
	std::string path = name;
	path += ".message.hpp";
	fp = fopen(path.c_str(), "wb+");
	if (fp == NULL) {
		perror(path.c_str());
		return;
	}

	// namespace prefix for fully-qualified type names. the leading "struct " is an
	// elaborated-type-specifier: it forces type lookup so a field that shares its
	// name with a nested struct type (e.g. schema `.phone:phone[home]` -> a member
	// named phone and a nested struct phone) resolves to the type, not the member.
	std::string ns;
	for (const auto p : space) {
		ns += "::";
		ns += p;
	}
	qualprefix = ns.empty() ? "struct ::" : ("struct " + ns + "::");

	// collect qualified names for every struct (roots + nested).
	const struct zproto_starray *sts = zproto_root(z);
	if (sts != NULL) {
		for (int i = 0; i < sts->count; i++)
			collect(sts->buf[i], zproto_name(sts->buf[i]));
	}

	fprintf(fp, "#pragma once\n");
	fprintf(fp, "#include \"%s.hpp\"\n", name);
	fprintf(fp, "#include \"zproto.hpp\"\n");
	fprintf(fp, "#include <cstring>\n");
	fprintf(fp, "#include <utility>\n");
	fprintf(fp, "\n");
	fprintf(fp, "namespace zproto {\n\n");
	if (sts != NULL) {
		for (int i = 0; i < sts->count; i++)
			dump_struct(fp, sts->buf[i]);
	}
	fprintf(fp, "}  // namespace zproto\n");
	fclose(fp);
}
