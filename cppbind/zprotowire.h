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
protected:
	virtual int _encode_field(struct zproto_args *args) const = 0;
	virtual int _decode_field(struct zproto_args *args) = 0;
	mutable std::unordered_map<int, const void *> maptoarray;
public:
	virtual const char *_name() const = 0;
};

class wiretree {
public:
	wiretree(const char *proto);
	~wiretree();
	int encode(const wire &w, std::string &dat);
	int decode(wire &w, const std::string &dat);
private:
	void expand();
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

