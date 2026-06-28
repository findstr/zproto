#include <string.h>
#include "zprotowire.h"
#include "bench.hpp"
namespace bench {

using namespace zprotobuf;

int
vec3::_tag() const
{
	return 0x0;
}
const char *
vec3::_name() const
{
	return "vec3";
}
void
vec3::_reset()
{
	this->x = 0.0f;
	this->y = 0.0f;
	this->z = 0.0f;

}
int
vec3::_encode(std::string &out) const
{
	zprotobuf::encoder e(out, 1, 3);
	e.present(1);
	e.w_f32(this->x);
	e.present(2);
	e.w_f32(this->y);
	e.present(3);
	e.w_f32(this->z);
	return e.finish();
}
int
vec3::_decode(const uint8_t *data, size_t sz)
{
	zprotobuf::decoder d(data, sz, 1);
	for (int tag; d.next(tag); ) {
		switch (tag) {
		case 1:
			this->x = d.r_f32();
			break;
		case 2:
			this->y = d.r_f32();
			break;
		case 3:
			this->z = d.r_f32();
			break;
		default:
			return 0;
		}
	}
	return (int)d.size();
}
int
item::_tag() const
{
	return 0x0;
}
const char *
item::_name() const
{
	return "item";
}
void
item::_reset()
{
	this->id = 0;
	this->count = 0;

}
int
item::_encode(std::string &out) const
{
	zprotobuf::encoder e(out, 1, 2);
	e.present(1);
	e.w_u32((uint32_t)this->id);
	e.present(2);
	e.w_u32((uint32_t)this->count);
	return e.finish();
}
int
item::_decode(const uint8_t *data, size_t sz)
{
	zprotobuf::decoder d(data, sz, 1);
	for (int tag; d.next(tag); ) {
		switch (tag) {
		case 1:
			this->id = d.r_u32();
			break;
		case 2:
			this->count = d.r_u32();
			break;
		default:
			return 0;
		}
	}
	return (int)d.size();
}
int
playerlevel::_tag() const
{
	return 0x0;
}
const char *
playerlevel::_name() const
{
	return "playerlevel";
}
void
playerlevel::_reset()
{
	this->userid = 0;
	this->level = 0;

}
int
playerlevel::_encode(std::string &out) const
{
	zprotobuf::encoder e(out, 1, 2);
	e.present(1);
	e.w_u64((uint64_t)this->userid);
	e.present(2);
	e.w_u32((uint32_t)this->level);
	return e.finish();
}
int
playerlevel::_decode(const uint8_t *data, size_t sz)
{
	zprotobuf::decoder d(data, sz, 1);
	for (int tag; d.next(tag); ) {
		switch (tag) {
		case 1:
			this->userid = d.r_u64();
			break;
		case 2:
			this->level = d.r_u32();
			break;
		default:
			return 0;
		}
	}
	return (int)d.size();
}
int
attr::_tag() const
{
	return 0x0;
}
const char *
attr::_name() const
{
	return "attr";
}
void
attr::_reset()
{
	this->key.clear();
	this->val.clear();

}
int
attr::_encode(std::string &out) const
{
	zprotobuf::encoder e(out, 1, 2);
	e.present(1);
	e.w_bytes(this->key.data(), this->key.size());
	e.present(2);
	e.w_bytes(this->val.data(), this->val.size());
	return e.finish();
}
int
attr::_decode(const uint8_t *data, size_t sz)
{
	zprotobuf::decoder d(data, sz, 1);
	for (int tag; d.next(tag); ) {
		switch (tag) {
		case 1:
			d.r_bytes(this->key);
			break;
		case 2:
			d.r_bytes(this->val);
			break;
		default:
			return 0;
		}
	}
	return (int)d.size();
}
int
nest::_tag() const
{
	return 0x0;
}
const char *
nest::_name() const
{
	return "nest";
}
void
nest::_reset()
{
	this->x = 0;
	this->y = 0;

}
int
nest::_encode(std::string &out) const
{
	zprotobuf::encoder e(out, 1, 2);
	e.present(1);
	e.w_u32((uint32_t)this->x);
	e.present(2);
	e.w_u32((uint32_t)this->y);
	return e.finish();
}
int
nest::_decode(const uint8_t *data, size_t sz)
{
	zprotobuf::decoder d(data, sz, 1);
	for (int tag; d.next(tag); ) {
		switch (tag) {
		case 1:
			this->x = (int32_t)d.r_u32();
			break;
		case 2:
			this->y = (int32_t)d.r_u32();
			break;
		default:
			return 0;
		}
	}
	return (int)d.size();
}
int
mapi::_tag() const
{
	return 0x0;
}
const char *
mapi::_name() const
{
	return "mapi";
}
void
mapi::_reset()
{
	this->k = 0;
	this->v = 0;

}
int
mapi::_encode(std::string &out) const
{
	zprotobuf::encoder e(out, 1, 2);
	e.present(1);
	e.w_u32((uint32_t)this->k);
	e.present(2);
	e.w_u32((uint32_t)this->v);
	return e.finish();
}
int
mapi::_decode(const uint8_t *data, size_t sz)
{
	zprotobuf::decoder d(data, sz, 1);
	for (int tag; d.next(tag); ) {
		switch (tag) {
		case 1:
			this->k = (int32_t)d.r_u32();
			break;
		case 2:
			this->v = (int32_t)d.r_u32();
			break;
		default:
			return 0;
		}
	}
	return (int)d.size();
}
int
mapf::_tag() const
{
	return 0x0;
}
const char *
mapf::_name() const
{
	return "mapf";
}
void
mapf::_reset()
{
	this->k = 0;
	this->fv = 0.0f;

}
int
mapf::_encode(std::string &out) const
{
	zprotobuf::encoder e(out, 1, 2);
	e.present(1);
	e.w_u32((uint32_t)this->k);
	e.present(2);
	e.w_f32(this->fv);
	return e.finish();
}
int
mapf::_decode(const uint8_t *data, size_t sz)
{
	zprotobuf::decoder d(data, sz, 1);
	for (int tag; d.next(tag); ) {
		switch (tag) {
		case 1:
			this->k = (int32_t)d.r_u32();
			break;
		case 2:
			this->fv = d.r_f32();
			break;
		default:
			return 0;
		}
	}
	return (int)d.size();
}
int
heartbeat::_tag() const
{
	return 0x1;
}
const char *
heartbeat::_name() const
{
	return "heartbeat";
}
void
heartbeat::_reset()
{
	this->seq = 0;
	this->ack = 0;

}
int
heartbeat::_encode(std::string &out) const
{
	zprotobuf::encoder e(out, 1, 2);
	e.present(1);
	e.w_u32((uint32_t)this->seq);
	e.present(2);
	e.w_u32((uint32_t)this->ack);
	return e.finish();
}
int
heartbeat::_decode(const uint8_t *data, size_t sz)
{
	zprotobuf::decoder d(data, sz, 1);
	for (int tag; d.next(tag); ) {
		switch (tag) {
		case 1:
			this->seq = d.r_u32();
			break;
		case 2:
			this->ack = d.r_u32();
			break;
		default:
			return 0;
		}
	}
	return (int)d.size();
}
int
frame::_tag() const
{
	return 0x10;
}
const char *
frame::_name() const
{
	return "frame";
}
void
frame::_reset()
{
	this->eid = 0;
	this->x = 0.0f;
	this->y = 0.0f;
	this->z = 0.0f;
	this->yaw = 0.0f;
	this->seq = 0;

}
int
frame::_encode(std::string &out) const
{
	zprotobuf::encoder e(out, 1, 6);
	e.present(1);
	e.w_u64((uint64_t)this->eid);
	e.present(2);
	e.w_f32(this->x);
	e.present(3);
	e.w_f32(this->y);
	e.present(4);
	e.w_f32(this->z);
	e.present(5);
	e.w_f32(this->yaw);
	e.present(6);
	e.w_u32((uint32_t)this->seq);
	return e.finish();
}
int
frame::_decode(const uint8_t *data, size_t sz)
{
	zprotobuf::decoder d(data, sz, 1);
	for (int tag; d.next(tag); ) {
		switch (tag) {
		case 1:
			this->eid = d.r_u64();
			break;
		case 2:
			this->x = d.r_f32();
			break;
		case 3:
			this->y = d.r_f32();
			break;
		case 4:
			this->z = d.r_f32();
			break;
		case 5:
			this->yaw = d.r_f32();
			break;
		case 6:
			this->seq = d.r_u32();
			break;
		default:
			return 0;
		}
	}
	return (int)d.size();
}
int
login::_tag() const
{
	return 0x20;
}
const char *
login::_name() const
{
	return "login";
}
void
login::_reset()
{
	this->userid = 0;
	this->token.clear();
	this->version = 0;
	this->platform = 0;

}
int
login::_encode(std::string &out) const
{
	zprotobuf::encoder e(out, 1, 4);
	e.present(1);
	e.w_u64((uint64_t)this->userid);
	e.present(2);
	e.w_bytes(this->token.data(), this->token.size());
	e.present(3);
	e.w_u32((uint32_t)this->version);
	e.present(4);
	e.w_u8((uint8_t)this->platform);
	return e.finish();
}
int
login::_decode(const uint8_t *data, size_t sz)
{
	zprotobuf::decoder d(data, sz, 1);
	for (int tag; d.next(tag); ) {
		switch (tag) {
		case 1:
			this->userid = d.r_u64();
			break;
		case 2:
			d.r_bytes(this->token);
			break;
		case 3:
			this->version = d.r_u32();
			break;
		case 4:
			this->platform = d.r_u8();
			break;
		default:
			return 0;
		}
	}
	return (int)d.size();
}
int
chat::_tag() const
{
	return 0x30;
}
const char *
chat::_name() const
{
	return "chat";
}
void
chat::_reset()
{
	this->from = 0;
	this->to = 0;
	this->text.clear();
	this->ts = 0;
	this->attrs.clear();

}
int
chat::_encode(std::string &out) const
{
	zprotobuf::encoder e(out, 1, 5);
	e.present(1);
	e.w_u64((uint64_t)this->from);
	e.present(2);
	e.w_u64((uint64_t)this->to);
	e.present(3);
	e.w_bytes(this->text.data(), this->text.size());
	e.present(4);
	e.w_u64((uint64_t)this->ts);
	e.present(5);
	e.w_array((uint32_t)this->attrs.size());
	for (const auto &v : this->attrs) {
		v._encode(out);
	}
	return e.finish();
}
int
chat::_decode(const uint8_t *data, size_t sz)
{
	zprotobuf::decoder d(data, sz, 1);
	for (int tag; d.next(tag); ) {
		switch (tag) {
		case 1:
			this->from = d.r_u64();
			break;
		case 2:
			this->to = d.r_u64();
			break;
		case 3:
			d.r_bytes(this->text);
			break;
		case 4:
			this->ts = d.r_u64();
			break;
		case 5: {
			uint32_t c = d.r_array();
			this->attrs.resize(c);
			for (uint32_t i = 0; i < c; i++) {
				size_t n;
				const uint8_t *p = d.struct_bytes(n);
				this->attrs[i]._decode(p, n);
			}
			break;
		}
		default:
			return 0;
		}
	}
	return (int)d.size();
}
int
snapshot::_tag() const
{
	return 0x40;
}
const char *
snapshot::_name() const
{
	return "snapshot";
}
void
snapshot::_reset()
{
	this->userid = 0;
	this->pos._reset();
	this->hp = 0;
	this->mp = 0;
	this->inventory.clear();
	this->buffs.clear();
	this->flags.clear();
	this->friends.clear();
	this->level = 0;

}
int
snapshot::_encode(std::string &out) const
{
	zprotobuf::encoder e(out, 1, 9);
	e.present(1);
	e.w_u64((uint64_t)this->userid);
	e.present(2);
	this->pos._encode(out);
	e.present(3);
	e.w_u32((uint32_t)this->hp);
	e.present(4);
	e.w_u32((uint32_t)this->mp);
	e.present(5);
	e.w_array((uint32_t)this->inventory.size());
	for (const auto &v : this->inventory) {
		v._encode(out);
	}
	e.present(6);
	e.w_array((uint32_t)this->buffs.size());
	for (const auto &v : this->buffs) {
		e.w_u32((uint32_t)v);
	}
	e.present(7);
	e.w_array((uint32_t)this->flags.size());
	for (const auto &v : this->flags) {
		e.w_u8((uint8_t)v);
	}
	e.present(8);
	e.w_array((uint32_t)this->friends.size());
	for (const auto &v : this->friends) {
		v._encode(out);
	}
	e.present(9);
	e.w_u32((uint32_t)this->level);
	return e.finish();
}
int
snapshot::_decode(const uint8_t *data, size_t sz)
{
	zprotobuf::decoder d(data, sz, 1);
	for (int tag; d.next(tag); ) {
		switch (tag) {
		case 1:
			this->userid = d.r_u64();
			break;
		case 2: {
			size_t n;
			const uint8_t *p = d.struct_bytes(n);
			this->pos._decode(p, n);
			break;
		}
		case 3:
			this->hp = (int32_t)d.r_u32();
			break;
		case 4:
			this->mp = (int32_t)d.r_u32();
			break;
		case 5: {
			uint32_t c = d.r_array();
			this->inventory.resize(c);
			for (uint32_t i = 0; i < c; i++) {
				size_t n;
				const uint8_t *p = d.struct_bytes(n);
				this->inventory[i]._decode(p, n);
			}
			break;
		}
		case 6: {
			uint32_t c = d.r_array();
			this->buffs.resize(c);
			for (uint32_t i = 0; i < c; i++) {
				this->buffs[i] = (int32_t)d.r_u32();
			}
			break;
		}
		case 7: {
			uint32_t c = d.r_array();
			this->flags.resize(c);
			for (uint32_t i = 0; i < c; i++) {
				this->flags[i] = (d.r_u8() == 1);
			}
			break;
		}
		case 8: {
			uint32_t c = d.r_array();
			this->friends.resize(c);
			for (uint32_t i = 0; i < c; i++) {
				size_t n;
				const uint8_t *p = d.struct_bytes(n);
				this->friends[i]._decode(p, n);
			}
			break;
		}
		case 9:
			this->level = d.r_u32();
			break;
		default:
			return 0;
		}
	}
	return (int)d.size();
}
int
alltypes::_tag() const
{
	return 0xFF;
}
const char *
alltypes::_name() const
{
	return "alltypes";
}
void
alltypes::_reset()
{
	this->b = false;
	this->i8 = 0;
	this->u8 = 0;
	this->i16 = 0;
	this->u16 = 0;
	this->i32 = 0;
	this->u32 = 0;
	this->i64 = 0;
	this->u64 = 0;
	this->f = 0.0f;
	this->s.clear();
	this->bl.clear();
	this->ai32.clear();
	this->as.clear();
	this->abl.clear();
	this->abool.clear();
	this->nest_n._reset();
	this->nest_na.clear();
	this->m_int.clear();
	this->m_float.clear();
	this->aempty.clear();
	this->asingle.clear();

}
int
alltypes::_encode(std::string &out) const
{
	zprotobuf::encoder e(out, 1, 22);
	e.present(1);
	e.w_u8((uint8_t)this->b);
	e.present(2);
	e.w_u8((uint8_t)this->i8);
	e.present(3);
	e.w_u8((uint8_t)this->u8);
	e.present(4);
	e.w_u16((uint16_t)this->i16);
	e.present(5);
	e.w_u16((uint16_t)this->u16);
	e.present(6);
	e.w_u32((uint32_t)this->i32);
	e.present(7);
	e.w_u32((uint32_t)this->u32);
	e.present(8);
	e.w_u64((uint64_t)this->i64);
	e.present(9);
	e.w_u64((uint64_t)this->u64);
	e.present(10);
	e.w_f32(this->f);
	e.present(11);
	e.w_bytes(this->s.data(), this->s.size());
	e.present(12);
	e.w_bytes(this->bl.data(), this->bl.size());
	e.present(13);
	e.w_array((uint32_t)this->ai32.size());
	for (const auto &v : this->ai32) {
		e.w_u32((uint32_t)v);
	}
	e.present(14);
	e.w_array((uint32_t)this->as.size());
	for (const auto &v : this->as) {
		e.w_bytes(v.data(), v.size());
	}
	e.present(15);
	e.w_array((uint32_t)this->abl.size());
	for (const auto &v : this->abl) {
		e.w_bytes(v.data(), v.size());
	}
	e.present(16);
	e.w_array((uint32_t)this->abool.size());
	for (const auto &v : this->abool) {
		e.w_u8((uint8_t)v);
	}
	e.present(17);
	this->nest_n._encode(out);
	e.present(18);
	e.w_array((uint32_t)this->nest_na.size());
	for (const auto &v : this->nest_na) {
		v._encode(out);
	}
	e.present(19);
	e.w_array((uint32_t)this->m_int.size());
	for (const auto &v : this->m_int) {
		v._encode(out);
	}
	e.present(20);
	e.w_array((uint32_t)this->m_float.size());
	for (const auto &v : this->m_float) {
		v._encode(out);
	}
	e.present(21);
	e.w_array((uint32_t)this->aempty.size());
	for (const auto &v : this->aempty) {
		e.w_u32((uint32_t)v);
	}
	e.present(22);
	e.w_array((uint32_t)this->asingle.size());
	for (const auto &v : this->asingle) {
		e.w_u32((uint32_t)v);
	}
	return e.finish();
}
int
alltypes::_decode(const uint8_t *data, size_t sz)
{
	zprotobuf::decoder d(data, sz, 1);
	for (int tag; d.next(tag); ) {
		switch (tag) {
		case 1:
			this->b = (d.r_u8() == 1);
			break;
		case 2:
			this->i8 = (int8_t)d.r_u8();
			break;
		case 3:
			this->u8 = d.r_u8();
			break;
		case 4:
			this->i16 = (int16_t)d.r_u16();
			break;
		case 5:
			this->u16 = d.r_u16();
			break;
		case 6:
			this->i32 = (int32_t)d.r_u32();
			break;
		case 7:
			this->u32 = d.r_u32();
			break;
		case 8:
			this->i64 = (int64_t)d.r_u64();
			break;
		case 9:
			this->u64 = d.r_u64();
			break;
		case 10:
			this->f = d.r_f32();
			break;
		case 11:
			d.r_bytes(this->s);
			break;
		case 12:
			d.r_bytes(this->bl);
			break;
		case 13: {
			uint32_t c = d.r_array();
			this->ai32.resize(c);
			for (uint32_t i = 0; i < c; i++) {
				this->ai32[i] = (int32_t)d.r_u32();
			}
			break;
		}
		case 14: {
			uint32_t c = d.r_array();
			this->as.resize(c);
			for (uint32_t i = 0; i < c; i++) {
				d.r_bytes(this->as[i]);
			}
			break;
		}
		case 15: {
			uint32_t c = d.r_array();
			this->abl.resize(c);
			for (uint32_t i = 0; i < c; i++) {
				d.r_bytes(this->abl[i]);
			}
			break;
		}
		case 16: {
			uint32_t c = d.r_array();
			this->abool.resize(c);
			for (uint32_t i = 0; i < c; i++) {
				this->abool[i] = (d.r_u8() == 1);
			}
			break;
		}
		case 17: {
			size_t n;
			const uint8_t *p = d.struct_bytes(n);
			this->nest_n._decode(p, n);
			break;
		}
		case 18: {
			uint32_t c = d.r_array();
			this->nest_na.resize(c);
			for (uint32_t i = 0; i < c; i++) {
				size_t n;
				const uint8_t *p = d.struct_bytes(n);
				this->nest_na[i]._decode(p, n);
			}
			break;
		}
		case 19: {
			uint32_t c = d.r_array();
			this->m_int.resize(c);
			for (uint32_t i = 0; i < c; i++) {
				size_t n;
				const uint8_t *p = d.struct_bytes(n);
				this->m_int[i]._decode(p, n);
			}
			break;
		}
		case 20: {
			uint32_t c = d.r_array();
			this->m_float.resize(c);
			for (uint32_t i = 0; i < c; i++) {
				size_t n;
				const uint8_t *p = d.struct_bytes(n);
				this->m_float[i]._decode(p, n);
			}
			break;
		}
		case 21: {
			uint32_t c = d.r_array();
			this->aempty.resize(c);
			for (uint32_t i = 0; i < c; i++) {
				this->aempty[i] = (int32_t)d.r_u32();
			}
			break;
		}
		case 22: {
			uint32_t c = d.r_array();
			this->asingle.resize(c);
			for (uint32_t i = 0; i < c; i++) {
				this->asingle[i] = (int32_t)d.r_u32();
			}
			break;
		}
		default:
			return 0;
		}
	}
	return (int)d.size();
}
}
