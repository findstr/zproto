#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <string>
#include <vector>

#include "zproto.hpp"

// ============ path / namespace helpers (from proto.cpp) ============

static void
strip_ext(char *path)
{
	char *p = strstr(path, ".zproto");
	if (p != NULL)
		*p = 0;
}

static std::string
remove_dot(char *path)
{
	char *p = path;
	for (;;) {
		p = strchr(path, '.');
		if (p == NULL)
			break;
		*p = '_';
		p++;
	}
	return path;
}

static void
name_space(char *path, std::vector<const char *> &space)
{
	char *p;
	for (;;) {
		p = strchr(path, '.');
		space.push_back(path);
		if (p != NULL)
			*p = 0;
		else
			break;
		path = p + 1;
	}
}

static char *
base_name(char *path)
{
	char *slash = strrchr(path, '/');
	return slash == NULL ? path : slash + 1;
}

// ============ identifier helpers (from rust.cpp) ============

static bool
has_children(struct zproto_struct *st)
{
	return st->child != NULL && st->child->count > 0;
}

static bool
unsupported_map_key_type(int type)
{
	return type == ZPROTO_FLOAT || type == ZPROTO_BLOB || type == ZPROTO_STRUCT;
}

static bool
is_ident_start(char c)
{
	return (c == '_') || isalpha((unsigned char)c);
}

static bool
is_ident_continue(char c)
{
	return (c == '_') || isalnum((unsigned char)c);
}

static bool
valid_ident(const char *name)
{
	if (name == NULL || name[0] == 0 || !is_ident_start(name[0]))
		return false;
	for (const char *p = name + 1; *p; ++p) {
		if (!is_ident_continue(*p))
			return false;
	}
	return true;
}

static bool
is_keyword(const std::string &name)
{
	static const char *keywords[] = {
		"as", "break", "const", "continue", "crate", "else", "enum",
		"extern", "false", "fn", "for", "if", "impl", "in", "let",
		"loop", "match", "mod", "move", "mut", "pub", "ref", "return",
		"self", "Self", "static", "struct", "super", "trait", "true",
		"type", "unsafe", "use", "where", "while", "async", "await",
		"dyn", "abstract", "become", "box", "do", "final", "macro",
		"override", "priv", "try", "typeof", "unsized", "virtual", "yield",
	};
	for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
		if (name == keywords[i])
			return true;
	}
	return false;
}

static std::string
raw_ident(const char *name)
{
	std::string n = name;
	if (is_keyword(n))
		return "r#" + n;
	return n;
}

static std::string
camel(const char *name)
{
	std::string out;
	bool upper = true;
	for (const char *p = name; *p; ++p) {
		if (*p == '_') {
			upper = true;
			continue;
		}
		if (upper) {
			out += (char)toupper((unsigned char)*p);
			upper = false;
		} else {
			out += *p;
		}
	}
	return out.empty() ? std::string(name) : out;
}

// ============ type mapping (from rust.cpp) ============

static std::string
scalar_type(int type)
{
	switch (type) {
	case ZPROTO_BOOLEAN: return "bool";
	case ZPROTO_BYTE: return "i8";
	case ZPROTO_UBYTE: return "u8";
	case ZPROTO_SHORT: return "i16";
	case ZPROTO_USHORT: return "u16";
	case ZPROTO_INTEGER: return "i32";
	case ZPROTO_UINTEGER: return "u32";
	case ZPROTO_LONG: return "i64";
	case ZPROTO_ULONG: return "u64";
	case ZPROTO_FLOAT: return "f32";
	case ZPROTO_STRING: return "String";
	case ZPROTO_BLOB: return "Vec<u8>";
	default:
		assert(0);
		return "";
	}
}

static std::string
type_name(struct zproto_field *f)
{
	if (f->type == ZPROTO_STRUCT)
		return camel(zproto_name(f->seminfo));
	return scalar_type(f->type);
}

static std::string
field_type(struct zproto_field *f)
{
	std::string base = type_name(f);
	if (f->mapkey != NULL) {
		std::string key = scalar_type(f->mapkey->type);
		return "HashMap<" + key + ", " + base + ">";
	}
	if (f->isarray) {
		return "Vec<" + base + ">";
	}
	return base;
}

static bool
needs_hashmap(struct zproto *z)
{
	const struct zproto_starray *roots = zproto_root(z);
	if (roots == NULL)
		return false;
	for (int i = 0; i < roots->count; i++) {
		struct zproto_struct *st = roots->buf[i];
		for (int j = 0; j < st->fieldcount; j++) {
			if (st->fields[j]->mapkey != NULL)
				return true;
		}
	}
	return false;
}

static const char *
write_method(int type)
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
		return "";
	}
}

static const char *
read_method(int type)
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
		return "";
	}
}

static std::string
encode_scalar_expr(int type, const std::string &expr)
{
	switch (type) {
	case ZPROTO_BOOLEAN:
		return expr + " as u8";
	case ZPROTO_BYTE:
		return expr + " as u8";
	case ZPROTO_UBYTE:
		return expr;
	case ZPROTO_SHORT:
		return expr + " as u16";
	case ZPROTO_USHORT:
		return expr;
	case ZPROTO_INTEGER:
		return expr + " as u32";
	case ZPROTO_UINTEGER:
		return expr;
	case ZPROTO_LONG:
		return expr + " as u64";
	case ZPROTO_ULONG:
		return expr;
	case ZPROTO_FLOAT:
		return expr;
	default:
		assert(0);
		return expr;
	}
}

static std::string
decode_scalar_expr(int type, const std::string &reader)
{
	switch (type) {
	case ZPROTO_BOOLEAN:
		return reader + "? == 1";
	case ZPROTO_BYTE:
		return reader + "? as i8";
	case ZPROTO_UBYTE:
		return reader + "?";
	case ZPROTO_SHORT:
		return reader + "? as i16";
	case ZPROTO_USHORT:
		return reader + "?";
	case ZPROTO_INTEGER:
		return reader + "? as i32";
	case ZPROTO_UINTEGER:
		return reader + "?";
	case ZPROTO_LONG:
		return reader + "? as i64";
	case ZPROTO_ULONG:
		return reader + "?";
	case ZPROTO_FLOAT:
		return reader + "?";
	default:
		assert(0);
		return reader + "?";
	}
}

// ============ validation (from rust.cpp) ============

static int
validate_field(struct zproto_field *f)
{
	if (!valid_ident(f->name)) {
		fprintf(stderr, "invalid Rust field identifier: %s\n", f->name);
		return -1;
	}
	if (f->mapkey != NULL && unsupported_map_key_type(f->mapkey->type)) {
		fprintf(stderr, "unsupported map key type for field %s\n", f->name);
		return -1;
	}
	return 0;
}

static int
validate_struct(struct zproto_struct *st)
{
	if (!valid_ident(zproto_name(st))) {
		fprintf(stderr, "invalid Rust struct identifier: %s\n", zproto_name(st));
		return -1;
	}
	if (has_children(st)) {
		fprintf(stderr, "nested struct is unsupported in zproto-gen-rust: %s\n", zproto_name(st));
		return -1;
	}
	for (int i = 0; i < st->fieldcount; i++) {
		if (validate_field(st->fields[i]) != 0)
			return -1;
	}
	return 0;
}

static int
validate(struct zproto *z)
{
	const struct zproto_starray *roots = zproto_root(z);
	if (roots == NULL)
		return 0;
	for (int i = 0; i < roots->count; i++) {
		if (validate_struct(roots->buf[i]) != 0)
			return -1;
	}
	return 0;
}

// ============ code emission (from rust.cpp) ============

static void emit_struct(FILE *fp, struct zproto_struct *st);

static void
emit_field_encode(FILE *fp, struct zproto_field *f)
{
	std::string name = raw_ident(f->name);
	if (f->mapkey != NULL) {
		fprintf(fp, "\t\te.present(out, %d);\n", f->tag);
		fprintf(fp, "\t\te.w_array(out, self.%s.len() as u32);\n", name.c_str());
		fprintf(fp, "\t\tfor value in self.%s.values() {\n", name.c_str());
		fprintf(fp, "\t\t\tvalue.encode_to(out)?;\n");
		fprintf(fp, "\t\t}\n");
		return;
	}
	if (f->type == ZPROTO_STRUCT && f->isarray) {
		fprintf(fp, "\t\te.present(out, %d);\n", f->tag);
		fprintf(fp, "\t\te.w_array(out, self.%s.len() as u32);\n", name.c_str());
		fprintf(fp, "\t\tfor value in &self.%s {\n", name.c_str());
		fprintf(fp, "\t\t\tvalue.encode_to(out)?;\n");
		fprintf(fp, "\t\t}\n");
		return;
	}
	if (f->type == ZPROTO_STRUCT) {
		fprintf(fp, "\t\te.present(out, %d);\n", f->tag);
		fprintf(fp, "\t\tself.%s.encode_to(out)?;\n", name.c_str());
		return;
	}
	if ((f->type == ZPROTO_STRING || f->type == ZPROTO_BLOB) && f->isarray) {
		fprintf(fp, "\t\te.present(out, %d);\n", f->tag);
		fprintf(fp, "\t\te.w_array(out, self.%s.len() as u32);\n", name.c_str());
		fprintf(fp, "\t\tfor value in &self.%s {\n", name.c_str());
		if (f->type == ZPROTO_STRING)
			fprintf(fp, "\t\t\te.w_string(out, value);\n");
		else
			fprintf(fp, "\t\t\te.w_bytes(out, value);\n");
		fprintf(fp, "\t\t}\n");
		return;
	}
	if (f->isarray) {
		fprintf(fp, "\t\te.present(out, %d);\n", f->tag);
		fprintf(fp, "\t\te.w_array(out, self.%s.len() as u32);\n", name.c_str());
		fprintf(fp, "\t\tfor value in &self.%s {\n", name.c_str());
		std::string expr = encode_scalar_expr(f->type, "*value");
		fprintf(fp, "\t\t\te.%s(out, %s);\n", write_method(f->type), expr.c_str());
		fprintf(fp, "\t\t}\n");
		return;
	}
	if (f->type == ZPROTO_STRING) {
		fprintf(fp, "\t\te.present(out, %d);\n", f->tag);
		fprintf(fp, "\t\te.w_string(out, &self.%s);\n", name.c_str());
		return;
	}
	if (f->type == ZPROTO_BLOB) {
		fprintf(fp, "\t\te.present(out, %d);\n", f->tag);
		fprintf(fp, "\t\te.w_bytes(out, &self.%s);\n", name.c_str());
		return;
	}
	fprintf(fp, "\t\te.present(out, %d);\n", f->tag);
	std::string expr = encode_scalar_expr(f->type, "self." + name);
	fprintf(fp, "\t\te.%s(out, %s);\n", write_method(f->type), expr.c_str());
}

static void
emit_field_decode(FILE *fp, struct zproto_field *f)
{
	std::string name = raw_ident(f->name);
	if (f->mapkey != NULL) {
		std::string stype = camel(zproto_name(f->seminfo));
		std::string keyname = raw_ident(f->mapkey->name);
		fprintf(fp, "\t\t\t%d => {\n", f->tag);
		fprintf(fp, "\t\t\t\tlet count = d.r_array()?;\n");
		fprintf(fp, "\t\t\t\tfor _ in 0..count {\n");
		fprintf(fp, "\t\t\t\t\tlet mut value = %s::default();\n", stype.c_str());
		fprintf(fp, "\t\t\t\t\tvalue.decode_from(d.struct_bytes()?)?;\n");
		if (f->mapkey->type == ZPROTO_STRING || f->mapkey->type == ZPROTO_BLOB)
			fprintf(fp, "\t\t\t\t\tlet key = value.%s.clone();\n", keyname.c_str());
		else
			fprintf(fp, "\t\t\t\t\tlet key = value.%s;\n", keyname.c_str());
		fprintf(fp, "\t\t\t\t\tself.%s.insert(key, value);\n", name.c_str());
		fprintf(fp, "\t\t\t\t}\n");
		fprintf(fp, "\t\t\t}\n");
		return;
	}
	if (f->type == ZPROTO_STRUCT && f->isarray) {
		std::string stype = camel(zproto_name(f->seminfo));
		fprintf(fp, "\t\t\t%d => {\n", f->tag);
		fprintf(fp, "\t\t\t\tlet count = d.r_array()? as usize;\n");
		fprintf(fp, "\t\t\t\tself.%s.clear();\n", name.c_str());
		fprintf(fp, "\t\t\t\tfor _ in 0..count {\n");
		fprintf(fp, "\t\t\t\t\tlet mut value = %s::default();\n", stype.c_str());
		fprintf(fp, "\t\t\t\t\tvalue.decode_from(d.struct_bytes()?)?;\n");
		fprintf(fp, "\t\t\t\t\tself.%s.push(value);\n", name.c_str());
		fprintf(fp, "\t\t\t\t}\n");
		fprintf(fp, "\t\t\t}\n");
		return;
	}
	if (f->type == ZPROTO_STRUCT) {
		fprintf(fp, "\t\t\t%d => {\n", f->tag);
		fprintf(fp, "\t\t\t\tself.%s.decode_from(d.struct_bytes()?)?;\n", name.c_str());
		fprintf(fp, "\t\t\t}\n");
		return;
	}
	if ((f->type == ZPROTO_STRING || f->type == ZPROTO_BLOB) && f->isarray) {
		fprintf(fp, "\t\t\t%d => {\n", f->tag);
		fprintf(fp, "\t\t\t\tlet count = d.r_array()? as usize;\n");
		fprintf(fp, "\t\t\t\tself.%s.clear();\n", name.c_str());
		fprintf(fp, "\t\t\t\tfor _ in 0..count {\n");
		if (f->type == ZPROTO_STRING)
			fprintf(fp, "\t\t\t\t\tself.%s.push(d.r_string()?);\n", name.c_str());
		else
			fprintf(fp, "\t\t\t\t\tself.%s.push(d.r_bytes()?);\n", name.c_str());
		fprintf(fp, "\t\t\t\t}\n");
		fprintf(fp, "\t\t\t}\n");
		return;
	}
	if (f->isarray) {
		fprintf(fp, "\t\t\t%d => {\n", f->tag);
		fprintf(fp, "\t\t\t\tlet count = d.r_array()? as usize;\n");
		fprintf(fp, "\t\t\t\tself.%s.clear();\n", name.c_str());
		fprintf(fp, "\t\t\t\tfor _ in 0..count {\n");
		std::string reader = std::string("d.") + read_method(f->type) + "()";
		std::string expr = decode_scalar_expr(f->type, reader);
		fprintf(fp, "\t\t\t\t\tself.%s.push(%s);\n", name.c_str(), expr.c_str());
		fprintf(fp, "\t\t\t\t}\n");
		fprintf(fp, "\t\t\t}\n");
		return;
	}
	if (f->type == ZPROTO_STRING) {
		fprintf(fp, "\t\t\t%d => self.%s = d.r_string()?,\n", f->tag, name.c_str());
		return;
	}
	if (f->type == ZPROTO_BLOB) {
		fprintf(fp, "\t\t\t%d => self.%s = d.r_bytes()?,\n", f->tag, name.c_str());
		return;
	}
	std::string reader = std::string("d.") + read_method(f->type) + "()";
	std::string expr = decode_scalar_expr(f->type, reader);
	fprintf(fp, "\t\t\t%d => self.%s = %s,\n", f->tag, name.c_str(), expr.c_str());
}

static void
emit_struct(FILE *fp, struct zproto_struct *st)
{
	std::string name = camel(zproto_name(st));
	fprintf(fp, "#[derive(Debug, Clone, Default, PartialEq)]\n");
	fprintf(fp, "pub struct %s {\n", name.c_str());
	for (int i = 0; i < st->fieldcount; i++) {
		struct zproto_field *f = st->fields[i];
		fprintf(fp, "\tpub %s: %s,\n", raw_ident(f->name).c_str(), field_type(f).c_str());
	}
	fprintf(fp, "}\n\n");

	fprintf(fp, "impl Message for %s {\n", name.c_str());
	fprintf(fp, "\tconst TAG: i32 = 0x%X;\n", zproto_tag(st));
	fprintf(fp, "\tconst NAME: &'static str = \"%s\";\n\n", zproto_name(st));
	fprintf(fp, "\tfn reset(&mut self) {\n");
	fprintf(fp, "\t\t*self = Self::default();\n");
	fprintf(fp, "\t}\n\n");
	fprintf(fp, "\tfn encode_to(&self, out: &mut Vec<u8>) -> Result<usize, zproto::Error> {\n");
	fprintf(fp, "\t\tlet start = out.len();\n");
	fprintf(fp, "\t\tlet mut e = zproto::Encoder::new(out, %d, %d);\n", st->basetag, st->fieldcount);
	for (int i = 0; i < st->fieldcount; i++)
		emit_field_encode(fp, st->fields[i]);
	fprintf(fp, "\t\te.finish(out)?;\n");
	fprintf(fp, "\t\tOk(out.len() - start)\n");
	fprintf(fp, "\t}\n\n");
	fprintf(fp, "\tfn decode_from(&mut self, data: &[u8]) -> Result<usize, zproto::Error> {\n");
	fprintf(fp, "\t\tself.reset();\n");
	fprintf(fp, "\t\tlet mut d = zproto::Decoder::new(data, %d)?;\n", st->basetag);
	fprintf(fp, "\t\twhile let Some(tag) = d.next()? {\n");
	fprintf(fp, "\t\t\tmatch tag {\n");
	for (int i = 0; i < st->fieldcount; i++)
		emit_field_decode(fp, st->fields[i]);
	fprintf(fp, "\t\t\t\t_ => return Ok(d.size()),\n");
	fprintf(fp, "\t\t\t}\n");
	fprintf(fp, "\t\t}\n");
	fprintf(fp, "\t\tOk(d.size())\n");
	fprintf(fp, "\t}\n");
	fprintf(fp, "}\n\n");
}

static void
emit_modules_open(FILE *fp, const std::vector<const char *> &space)
{
	for (const auto p : space)
		fprintf(fp, "pub mod %s {\n", raw_ident(p).c_str());
}

static void
emit_modules_close(FILE *fp, const std::vector<const char *> &space)
{
	for (size_t i = 0; i < space.size(); i++)
		fprintf(fp, "}\n");
}

static void
emit_all(FILE *fp, const std::vector<const char *> &space, struct zproto *z)
{
	emit_modules_open(fp, space);
	fprintf(fp, "use ::zproto::{self, Message};\n");
	if (needs_hashmap(z))
		fprintf(fp, "use std::collections::HashMap;\n");
	fprintf(fp, "\n");
	const struct zproto_starray *roots = zproto_root(z);
	if (roots != NULL) {
		for (int i = 0; i < roots->count; i++)
			emit_struct(fp, roots->buf[i]);
	}
	emit_modules_close(fp, space);
}

// ============ output path resolution (new) ============

static std::string
resolve_output(const std::string &name, const char *out_arg)
{
	std::string filename = name + ".rs";
	if (out_arg == NULL)
		return filename;
	std::string out = out_arg;
	if (!out.empty() && (out.back() == '/' || out.back() == '\\'))
		return out + filename;
	struct stat st;
	if (stat(out.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
		return out + "/" + filename;
	return out;
}

// ============ main ============

int
main(int argc, char *argv[])
{
	const char *schema_path = NULL;
	const char *out_arg = NULL;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--out") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr, "--out requires argument\n");
				return 1;
			}
			out_arg = argv[++i];
		} else if (schema_path == NULL) {
			schema_path = argv[i];
		} else {
			fprintf(stderr, "unexpected argument: %s\n", argv[i]);
			return 1;
		}
	}
	if (schema_path == NULL) {
		fprintf(stderr, "USAGE: %s <schema.zproto> [--out <path>]\n", argv[0]);
		return 1;
	}

	struct stat st;
	int err = stat(schema_path, &st);
	if (err < 0) {
		perror("file noexist");
		return 1;
	}

	char *proto = new char[st.st_size + 1];
	FILE *fp = fopen(schema_path, "rb");
	assert(fp);
	err = fread(proto, st.st_size, 1, fp);
	fclose(fp);
	if (err == 0) {
		delete []proto;
		return 1;
	}
	proto[st.st_size] = 0;

	struct zproto_parser parser;
	err = zproto_parse(&parser, proto);
	if (err < 0) {
		fprintf(stderr, "%s", parser.error);
		delete []proto;
		return 1;
	}

	char *schema_name = strdup(base_name((char *)schema_path));
	strip_ext(schema_name);
	char *space_buf = strdup(schema_name);
	std::string name = remove_dot(schema_name);
	std::vector<const char *> space;
	name_space(space_buf, space);

	int rc = 1;
	if (validate(parser.z) != 0)
		goto cleanup;

	{
		std::string output_path = resolve_output(name, out_arg);
		FILE *out_fp = fopen(output_path.c_str(), "wb+");
		if (out_fp == NULL) {
			perror(output_path.c_str());
			goto cleanup;
		}
		fprintf(out_fp, "// generated by zproto-gen-rust\n");
		emit_all(out_fp, space, parser.z);
		fclose(out_fp);
		rc = 0;
	}

cleanup:
	free(space_buf);
	free(schema_name);
	delete []proto;
	zproto_free(parser.z);
	return rc;
}
