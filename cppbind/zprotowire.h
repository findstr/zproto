#ifndef _ZPROTO_WIRE_H
#define _ZPROTO_WIRE_H

#include <assert.h>
#include <stdint.h>
#include <vector>
#include <string>
#include <unordered_map>
#include "zproto.hpp"

namespace zprotobuf {

class wire {
public:
	int _encode(uint8_t *buff, int buffsz, struct zproto_struct *st) const;
	int _decode(uint8_t *buff, int buffsz, struct zproto_struct *st);
	int _encode_proxy(struct zproto_args *args) const;
	int _decode_proxy(struct zproto_args *args);
public:
	virtual void _reset();
protected:
	int _write(struct zproto_args *args, bool val) const;
	int _write(struct zproto_args *args, int8_t val) const;
	int _write(struct zproto_args *args, int16_t val) const;
	int _write(struct zproto_args *args, int32_t val) const;
	int _write(struct zproto_args *args, int64_t val) const;
	int _write(struct zproto_args *args, float val) const;
	int _write(struct zproto_args *args, const std::string &val) const;

	int _read(struct zproto_args *args, bool &val);
	int _read(struct zproto_args *args, int8_t &val);
	int _read(struct zproto_args *args, int16_t &val);
	int _read(struct zproto_args *args, int32_t &val);
	int _read(struct zproto_args *args, int64_t &val);
	int _read(struct zproto_args *args, float &val);
	int _read(struct zproto_args *args, std::string &val);
protected:
	virtual int _encode_field(struct zproto_args *args) const = 0;
	virtual int _decode_field(struct zproto_args *args) = 0;
};

class wirep;

class wiretree {
public:
	wiretree(const char *proto);
	~wiretree();
	int encode(const wirep &w, std::string &dat);
	int encode(const wirep &w, const uint8_t **data);
	int pack(const uint8_t *src, int size, std::string &dat);
	int pack(const uint8_t *src, int size, const uint8_t **dat);
	int unpack(const uint8_t *src, int size, std::string &dat);
	int unpack(const uint8_t *src, int size, const uint8_t **dat);
	int encodesafe(const wirep &w, std::string &dat, int presize = 1024);
	int packsafe(const uint8_t *src, int size, std::string &dat,
			int presize = 1024);
	int unpacksafe(const uint8_t *src, int size, std::string &dat,
			int presize = 1024);
	int decode(wirep &w, const std::string &dat);
	int decode(wirep &w, const uint8_t *dat, size_t datasz);
	struct zproto_struct *query(const char *name);
private:
	int encodecheck(const wirep &w);
	typedef int (*cook_cb_t)(const uint8_t *src, int srcsz,
			uint8_t *dst, int dstsz);
	int cookcheck(const uint8_t *src, int size, cook_cb_t cb);
	int cooksafe(const uint8_t *src, int size, std::string &dat,
		int presize, cook_cb_t cb);
private:
	const char *protodef;
	struct zproto *z;
};

class wirep : public wire {
public:
	virtual int _serialize(std::string &dat) const;
	virtual int _serialize(const uint8_t **dat) const;
	virtual int _serializesafe(std::string &dat, int presize = 1024) const;
	virtual int _parse(const std::string &dat);
	virtual int _parse(const uint8_t *dat, int datsz);
	virtual int _pack(const uint8_t *src, int srcsz, const uint8_t **dat);
	virtual int _pack(const uint8_t *src, int srcsz, std::string &dat);
	virtual int _unpack(const uint8_t *src, int srcsz, const uint8_t **dat);
	virtual int _unpack(const uint8_t *src, int srcsz, std::string &dat);
	virtual int _tag() const = 0;
	virtual const char *_name() const = 0;
	virtual struct zproto_struct *_query() const = 0;
protected:
	virtual wiretree &_wiretree() const = 0;
};



};


#endif

