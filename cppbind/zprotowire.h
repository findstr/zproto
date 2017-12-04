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
        virtual int _serialize(std::string &dat) const;
        virtual int _serialize(const uint8_t **data) const;
        virtual int _serializesafe(std::string &dat, int presize = 1024) const;
        virtual int _parse(const std::string &dat);
        virtual int _parse(const uint8_t *data, int datasz);
	virtual int _tag() const;
protected:
	int _write(struct zproto_args *args, uint8_t val) const;
	int _write(struct zproto_args *args, uint32_t val) const;
	int _write(struct zproto_args *args, uint64_t val) const;
	int _write(struct zproto_args *args, float val) const;
	int _write(struct zproto_args *args, const std::string &val) const;

	int _read(struct zproto_args *args, uint8_t &val);
	int _read(struct zproto_args *args, uint32_t &val);
	int _read(struct zproto_args *args, uint64_t &val);
	int _read(struct zproto_args *args, float &val);
	int _read(struct zproto_args *args, std::string &val);
protected:
	virtual int _encode_field(struct zproto_args *args) const = 0;
	virtual int _decode_field(struct zproto_args *args) = 0;
public:
	virtual const char *_name() const = 0;
};

class wiretree {
public:
	wiretree(const char *proto);
	~wiretree();
	int encode(const wire &w, std::string &dat);
	int encode(const wire &w, const uint8_t **data);
	int encodesafe(const wire &w, std::string &dat, int presize = 1024);
	int decode(wire &w, const std::string &dat);
	int decode(wire &w, const uint8_t *dat, size_t datasz);
	int tag(const wire &w);
private:
	void expand();
	int encodecheck(const wire &w);
	struct zproto_struct *query(const char *name);
private:
	const char *protodef;
	struct zproto *z;
	std::unordered_map<intptr_t, struct zproto_struct *> cache;
	uint8_t *buff;
	int	buffsz;
};


};


#endif

