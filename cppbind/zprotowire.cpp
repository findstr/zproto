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
	if (args->buffsz < sizeof(T))
		return ZPROTO_OOM;
	(*(T *)args->buff) = val;
	return sizeof(T);
}

template<typename T> int
read(struct zproto_args *args, T &val)
{
	if (args->buffsz < sizeof(T))
		return ZPROTO_ERROR;
	val = (*(T *)args->buff);
	return sizeof(T);
}

int
wire::_write(struct zproto_args *args, uint8_t val) const
{
	return write(args, val);
}

int
wire::_write(struct zproto_args *args, uint32_t val) const
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
wire::_read(struct zproto_args *args, uint8_t &val)
{
	return read(args, val);
}

int
wire::_read(struct zproto_args *args, uint32_t &val)
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
	return 0;
}

int
wire::_serialize(const uint8_t **data) const
{
	(void)data;
	return 0;
}

int
wire::_serializesafe(std::string &dat, int presize) const
{
	(void)dat;
	(void)presize;
	return 0;
}

int
wire::_parse(const std::string &dat)
{
	(void)dat;
	return 0;
}
int
wire::_parse(const uint8_t *data, int datasz)
{
	(void)data;
	(void)datasz;
	return 0;
}

int
wire::_tag() const
{
	return 0;
}


//syntax tree

wiretree::wiretree(const char *proto)
	:protodef(proto)
{
	int err;
	z = zproto_create();
	err = zproto_parse(z, protodef);
	assert(err == 0);
	buff = new uint8_t[BUFFSZ];
	buffsz = BUFFSZ;
}

wiretree::~wiretree()
{
	zproto_free(z);
	delete[] buff;
}


void
wiretree::expand()
{
	size_t newsz = buffsz * 2;
	uint8_t *newbuff = new uint8_t[newsz];
	delete[] buff;
	buffsz = newsz;
	buff = newbuff;
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
		sz = w._encode(buff, buffsz, st);
		if (sz == ZPROTO_ERROR)
			return sz;
		if (sz == ZPROTO_OOM) {
			expand();
			continue;
		}
		return sz;
	}
	//never come here
	return ZPROTO_ERROR;
}

int
wiretree::encode(const wire &w, const uint8_t **data)
{
	int sz;
	sz = encodecheck(w);
	if ((sz >= 0) && data)
		*data = buff;
	return sz;
}

int
wiretree::encode(const wire &w, std::string &dat)
{
	int sz;
	dat.clear();
	sz = encodecheck(w);
	if (sz > 0)
		dat.assign((char *)buff, sz);
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

}
