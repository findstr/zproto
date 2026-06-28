#ifndef __bench_h
#define __bench_h
#include "zprotowire.h"
namespace bench {

using namespace zprotobuf;

struct vec3:public wirep{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
public:
	virtual int _encode(std::string &) const;
	virtual int _decode(const uint8_t *, size_t);
	virtual void _reset();
	virtual int _tag() const;
	virtual const char *_name() const;
};
struct item:public wirep{
	uint32_t id = 0;
	uint32_t count = 0;
public:
	virtual int _encode(std::string &) const;
	virtual int _decode(const uint8_t *, size_t);
	virtual void _reset();
	virtual int _tag() const;
	virtual const char *_name() const;
};
struct playerlevel:public wirep{
	uint64_t userid = 0;
	uint32_t level = 0;
public:
	virtual int _encode(std::string &) const;
	virtual int _decode(const uint8_t *, size_t);
	virtual void _reset();
	virtual int _tag() const;
	virtual const char *_name() const;
};
struct attr:public wirep{
	std::string key;
	std::string val;
public:
	virtual int _encode(std::string &) const;
	virtual int _decode(const uint8_t *, size_t);
	virtual void _reset();
	virtual int _tag() const;
	virtual const char *_name() const;
};
struct nest:public wirep{
	int32_t x = 0;
	int32_t y = 0;
public:
	virtual int _encode(std::string &) const;
	virtual int _decode(const uint8_t *, size_t);
	virtual void _reset();
	virtual int _tag() const;
	virtual const char *_name() const;
};
struct mapi:public wirep{
	int32_t k = 0;
	int32_t v = 0;
public:
	virtual int _encode(std::string &) const;
	virtual int _decode(const uint8_t *, size_t);
	virtual void _reset();
	virtual int _tag() const;
	virtual const char *_name() const;
};
struct mapf:public wirep{
	int32_t k = 0;
	float fv = 0.0f;
public:
	virtual int _encode(std::string &) const;
	virtual int _decode(const uint8_t *, size_t);
	virtual void _reset();
	virtual int _tag() const;
	virtual const char *_name() const;
};
struct heartbeat:public wirep{
	uint32_t seq = 0;
	uint32_t ack = 0;
public:
	virtual int _encode(std::string &) const;
	virtual int _decode(const uint8_t *, size_t);
	virtual void _reset();
	virtual int _tag() const;
	virtual const char *_name() const;
};
struct frame:public wirep{
	uint64_t eid = 0;
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float yaw = 0.0f;
	uint32_t seq = 0;
public:
	virtual int _encode(std::string &) const;
	virtual int _decode(const uint8_t *, size_t);
	virtual void _reset();
	virtual int _tag() const;
	virtual const char *_name() const;
};
struct login:public wirep{
	uint64_t userid = 0;
	std::string token;
	uint32_t version = 0;
	uint8_t platform = 0;
public:
	virtual int _encode(std::string &) const;
	virtual int _decode(const uint8_t *, size_t);
	virtual void _reset();
	virtual int _tag() const;
	virtual const char *_name() const;
};
struct chat:public wirep{
	uint64_t from = 0;
	uint64_t to = 0;
	std::string text;
	uint64_t ts = 0;
	std::vector<struct attr> attrs;
public:
	virtual int _encode(std::string &) const;
	virtual int _decode(const uint8_t *, size_t);
	virtual void _reset();
	virtual int _tag() const;
	virtual const char *_name() const;
};
struct snapshot:public wirep{
	uint64_t userid = 0;
	struct vec3 pos;
	int32_t hp = 0;
	int32_t mp = 0;
	std::vector<struct item> inventory;
	std::vector<int32_t> buffs;
	std::vector<bool> flags;
	std::vector<struct playerlevel> friends;
	uint32_t level = 0;
public:
	virtual int _encode(std::string &) const;
	virtual int _decode(const uint8_t *, size_t);
	virtual void _reset();
	virtual int _tag() const;
	virtual const char *_name() const;
};
struct alltypes:public wirep{
	bool b = false;
	int8_t i8 = 0;
	uint8_t u8 = 0;
	int16_t i16 = 0;
	uint16_t u16 = 0;
	int32_t i32 = 0;
	uint32_t u32 = 0;
	int64_t i64 = 0;
	uint64_t u64 = 0;
	float f = 0.0f;
	std::string s;
	std::string bl;
	std::vector<int32_t> ai32;
	std::vector<std::string> as;
	std::vector<std::string> abl;
	std::vector<bool> abool;
	struct nest nest_n;
	std::vector<struct nest> nest_na;
	std::vector<struct mapi> m_int;
	std::vector<struct mapf> m_float;
	std::vector<int32_t> aempty;
	std::vector<int32_t> asingle;
public:
	virtual int _encode(std::string &) const;
	virtual int _decode(const uint8_t *, size_t);
	virtual void _reset();
	virtual int _tag() const;
	virtual const char *_name() const;
};

}
#endif
