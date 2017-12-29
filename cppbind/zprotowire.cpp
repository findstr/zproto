#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "zproto.hpp"
#include "zprotowire.h"

namespace zprotobuf {

#define BUFFSZ (64)

static int
encode_cb(struct zproto_args *arg)
{
	wire *w = (wire *)arg->ud;
	return w->_encode_proxy(arg);
}

static int
decode_cb(struct zproto_args *arg)
{
	wire *w = (wire *)arg->ud;
	return w->_decode_proxy(arg);
}

template<typename T> int
write(struct zproto_args *args, T val)
{
	if (args->buffsz < (int)sizeof(T))
		return ZPROTO_OOM;
	(*(T *)args->buff) = val;
	return sizeof(T);
}

template<typename T> int
read(struct zproto_args *args, T &val)
{
	if (args->buffsz < (int)sizeof(T))
		return ZPROTO_ERROR;
	val = (*(T *)args->buff);
	return sizeof(T);
}

int
wire::_write(struct zproto_args *args, bool val) const
{
	uint8_t b = val ? 1 : 0;
	return write(args, b);
}

int
wire::_write(struct zproto_args *args, int32_t val) const
{
	return write(args, val);
}
int
wire::_write(struct zproto_args *args, int64_t val) const
{
	return write(args, val);
}

int
wire::_write(struct zproto_args *args, float val) const
{
	return write(args, val);
}

int
wire::_write(struct zproto_args *args, const std::string &val) const
{
	if (args->buffsz < (int)val.size())
		return ZPROTO_OOM;
	memcpy(args->buff, val.c_str(), val.size());
	return val.size();
}

int
wire::_read(struct zproto_args *args, bool &val)
{
	int ret;
	uint8_t b = 0;
	ret = read(args, b);
	val = (b == 1) ? true : false;
	return ret;
}

int
wire::_read(struct zproto_args *args, int32_t &val)
{
	return read(args, val);
}

int
wire::_read(struct zproto_args *args, int64_t &val)
{
	return read(args, val);
}

int
wire::_read(struct zproto_args *args, float &val)
{
	return read(args, val);
}

int
wire::_read(struct zproto_args *args, std::string &val)
{
	val.assign((char *)args->buff, args->buffsz);
	return args->buffsz;
}

int
wire::_encode(uint8_t *buff, int buffsz, struct zproto_struct *st) const
{
	return zproto_encode(st, buff, buffsz, encode_cb, (void *)this);
}

int
wire::_decode(uint8_t *buff, int buffsz, struct zproto_struct *st)
{
	return zproto_decode(st, buff, buffsz, decode_cb, (void *)this);
}

int
wire::_encode_proxy(struct zproto_args *args) const
{
	return _encode_field(args);
}

int
wire::_decode_proxy(struct zproto_args *args)
{
	return _decode_field(args);
}

int
wire::_serialize(std::string &dat) const
{
	(void)dat;
	return ZPROTO_ERROR;
}

int
wire::_serialize(const uint8_t **data) const
{
	(void)data;
	return ZPROTO_ERROR;
}

int
wire::_serializesafe(std::string &dat, int presize) const
{
	(void)dat;
	(void)presize;
	return ZPROTO_ERROR;
}

int
wire::_parse(const std::string &dat)
{
	(void)dat;
	return ZPROTO_ERROR;
}
int
wire::_parse(const uint8_t *data, int datasz)
{
	(void)data;
	(void)datasz;
	return ZPROTO_ERROR;
}

int
wire::_tag() const
{
	return 0;
}

int
wire::_pack(const uint8_t *src, int srcsz, const uint8_t **dat)
{
	(void)src;
	(void)srcsz;
	(void)dat;
	return ZPROTO_ERROR;
}

int
wire::_pack(const uint8_t *src, int srcsz, std::string &dat)
{
	(void)src;
	(void)srcsz;
	(void)dat;
	return ZPROTO_ERROR;
}

int
wire::_unpack(const uint8_t *src, int srcsz, const uint8_t **dat)
{
	(void)src;
	(void)srcsz;
	(void)dat;
	return ZPROTO_ERROR;
}

int
wire::_unpack(const uint8_t *src, int srcsz, std::string &dat)
{
	(void)src;
	(void)srcsz;
	(void)dat;
	return ZPROTO_ERROR;
}


void
wire::_reset()
{
	//empty
}

//syntax tree

wiretree::wiretree(const char *proto)
	:protodef(proto)
{
	int err;
	z = zproto_create();
	err = zproto_parse(z, protodef);
	assert(err == 0);
	marshal.ptr = new uint8_t[BUFFSZ];
	marshal.size = BUFFSZ;
	cook.ptr = new uint8_t[BUFFSZ];
	cook.size = BUFFSZ;
}

wiretree::~wiretree()
{
	zproto_free(z);
	delete[] marshal.ptr;
	delete[] cook.ptr;
}


void
wiretree::expand(struct wiretree::buffer *buff)
{
	size_t newsz = buff->size * 2;
	uint8_t *newbuff = new uint8_t[newsz];
	delete[] buff->ptr;
	buff->size = newsz;
	buff->ptr = newbuff;
	return ;
}

struct zproto_struct *
wiretree::query(const char *name)
{
	struct zproto_struct *st;
	intptr_t k = (intptr_t)name;
	st = cache[k];
	if (st)
		return st;
	st = zproto_query(z, name);
	assert(st);
	cache[k] = st;
	return st;
}

int
wiretree::encodecheck(const wire &w)
{
	int sz;
	struct zproto_struct *st = query(w._name());
	for (;;) {
		sz = w._encode(marshal.ptr, marshal.size, st);
		if (sz == ZPROTO_ERROR)
			return sz;
		if (sz == ZPROTO_OOM) {
			expand(&marshal);
			continue;
		}
		return sz;
	}
	//never come here
	return ZPROTO_ERROR;
}

int
wiretree::cookcheck(const uint8_t *src, int size, cook_cb_t cb)
{
	for (;;) {
		int sz = cb(src, size, cook.ptr, cook.size);
		if (sz == ZPROTO_ERROR)
			return sz;
		if (sz == ZPROTO_OOM) {
			expand(&cook);
			continue;
		}
		return sz;
	}
	//never come here
	return ZPROTO_ERROR;
}

int
wiretree::cooksafe(const uint8_t *src, int size, std::string &dat,
		int presize, cook_cb_t cb)
{
	int sz;
	uint8_t *tmpbuf = new uint8_t[presize];
	for (;;) {
		sz = cb(src, size, tmpbuf, presize);
		if (sz == ZPROTO_ERROR) {
			delete[] tmpbuf;
			return sz;
		}
		if (sz == ZPROTO_OOM) {
			size_t newsz = presize * 2;
			uint8_t *newbuf = new uint8_t[newsz];
			delete[] tmpbuf;
			tmpbuf = newbuf;
			presize = newsz;
			continue;
		}
		dat.assign((char *)tmpbuf, sz);
		delete[] tmpbuf;
		return sz;
	}
	//never come here
	delete[] tmpbuf;
	return ZPROTO_ERROR;
}

int
wiretree::encode(const wire &w, std::string &dat)
{
	int sz;
	dat.clear();
	sz = encodecheck(w);
	if (sz > 0)
		dat.assign((char *)marshal.ptr, sz);
	return sz;
}

int
wiretree::encode(const wire &w, const uint8_t **dat)
{
	int sz;
	sz = encodecheck(w);
	if ((sz >= 0) && dat)
		*dat = marshal.ptr;
	return sz;
}

int
wiretree::pack(const uint8_t *src, int size, std::string &dat)
{
	int sz;
	sz = cookcheck(src, size, zproto_pack);
	if (sz > 0)
		dat.assign((char *)cook.ptr, sz);
	return sz;
}

int
wiretree::pack(const uint8_t *src, int size, const uint8_t **dat)
{
	int sz;
	sz = cookcheck(src, size, zproto_pack);
	if ((sz >= 0) && dat)
		*dat = cook.ptr;
	return sz;
}

int
wiretree::unpack(const uint8_t *src, int size, std::string &dat)
{
	int sz;
	sz = cookcheck(src, size, zproto_unpack);
	if (sz > 0)
		dat.assign((char *)cook.ptr, sz);
	return sz;
}

int
wiretree::unpack(const uint8_t *src, int size, const uint8_t **dat)
{
	int sz;
	sz = cookcheck(src, size, zproto_unpack);
	if ((sz >= 0) && dat)
		*dat = cook.ptr;
	return sz;
}

int
wiretree::encodesafe(const wire &w, std::string &dat, int presize)
{
	int sz;
	uint8_t *tmpbuf = new uint8_t[presize];
	struct zproto_struct *st = query(w._name());
	for (;;) {
		sz = w._encode(tmpbuf, presize, st);
		if (sz == ZPROTO_ERROR) {
			delete[] tmpbuf;
			return sz;
		}
		if (sz == ZPROTO_OOM) {
			size_t newsz = presize * 2;
			uint8_t *newbuf = new uint8_t[newsz];
			delete[] tmpbuf;
			tmpbuf = newbuf;
			presize = newsz;
			continue;
		}
		dat.assign((char *)tmpbuf, sz);
		delete[] tmpbuf;
		return sz;
	}
	//never come here
	delete[] tmpbuf;
	return ZPROTO_ERROR;
}

int
wiretree::packsafe(const uint8_t *src, int size, std::string &dat, int presize)
{
	return cooksafe(src, size, dat, presize, zproto_pack);
}

int
wiretree::unpacksafe(const uint8_t *src, int size, std::string &dat, int presize)
{
	return cooksafe(src, size, dat, presize, zproto_unpack);
}

int
wiretree::decode(wire &w, const std::string &dat)
{
	return decode(w, (uint8_t *)dat.data(), dat.size());
}

int
wiretree::decode(wire &w, const uint8_t *dat, size_t datasz)
{
	int sz;
	struct zproto_struct *st = query(w._name());
	sz = w._decode((uint8_t *)dat, datasz, st);
	return sz;
}

int
wiretree::tag(const wire &w)
{
	struct zproto_struct *st = query(w._name());
	return zproto_tag(st);
}

//wire protocol interface
int
iwirep::_serialize(std::string &dat) const
{
	return _wiretree().encode(*this, dat);
}

int
iwirep::_serialize(const uint8_t **data) const
{
	return _wiretree().encode(*this, data);
}

int
iwirep::_serializesafe(std::string &dat, int presize) const
{
	return _wiretree().encodesafe(*this, dat, presize);
}

int
iwirep::_parse(const std::string &dat)
{
	return _wiretree().decode(*this, dat);
}

int
iwirep::_parse(const uint8_t *data, int datasz)
{
	return _wiretree().decode(*this, data, datasz);
}

int
iwirep::_tag() const
{
	return _wiretree().tag(*this);
}

int
iwirep::_pack(const uint8_t *src, int srcsz, const uint8_t **dat)
{
	return _wiretree().pack(src, srcsz, dat);
}

int
iwirep::_pack(const uint8_t *src, int srcsz, std::string &dat)
{
	return _wiretree().pack(src, srcsz, dat);
}

int
iwirep::_unpack(const uint8_t *src, int srcsz, const uint8_t **dat)
{
	return _wiretree().unpack(src, srcsz, dat);
}

int
iwirep::_unpack(const uint8_t *src, int srcsz, std::string &dat)
{
	return _wiretree().unpack(src, srcsz, dat);
}

}
