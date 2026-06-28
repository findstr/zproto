#pragma once
#include "wire.hpp"
#include <unordered_map>
#include <cstdint>

// router-style prototype dispatch: type-erased binding keyed by tag.
// Replaces the old unique_ptr<wirep> + virtual _decode/_reset prototype pattern:
// the "vtable" is a small fn-pointer ops struct on the binding, NOT on each data type.

using handler_cb = uint32_t (*)(void *);

namespace zproto {
template<class T> inline void decode_void(void *p, const uint8_t *b, size_t n) {
	wire<T>::decode(*static_cast<T *>(p), b, n);
}
template<class T> inline void destroy_void(void *p) { delete static_cast<T *>(p); }
}

struct binding {
	void *ptr = nullptr;                                       // owned prototype instance
	void (*decode)(void *, const uint8_t *, size_t) = nullptr; // = &decode_void<T>
	void (*destroy)(void *) = nullptr;                          // = &destroy_void<T>
	handler_cb cb = nullptr;

	binding() = default;
	binding(void *p, void (*d)(void *, const uint8_t *, size_t), void (*dst)(void *), handler_cb c)
		: ptr(p), decode(d), destroy(dst), cb(c) {}
	binding(const binding &) = delete;
	binding &operator=(const binding &) = delete;
	binding(binding &&o) noexcept
		: ptr(o.ptr), decode(o.decode), destroy(o.destroy), cb(o.cb) { o.ptr = nullptr; }
	binding &operator=(binding &&o) noexcept {
		if (this != &o) {
			if (ptr && destroy) destroy(ptr);
			ptr = o.ptr; decode = o.decode; destroy = o.destroy; cb = o.cb; o.ptr = nullptr;
		}
		return *this;
	}
	~binding() { if (ptr && destroy) destroy(ptr); }
};

inline std::unordered_map<int, binding> &router_table() {
	static std::unordered_map<int, binding> t;
	return t;
}

// register a prototype + handler (typed at the call site).
template<class T> void reg(const T &proto, handler_cb cb) {
	int tag = zproto::wire<T>::tag();
	router_table()[tag] = binding{new T(proto), &zproto::decode_void<T>, &zproto::destroy_void<T>, cb};
}

// dispatch by tag: decode incoming bytes into the prototype, invoke handler.
inline uint32_t call(int tag, const uint8_t *buf, size_t n) {
	auto it = router_table().find(tag);
	if (it == router_table().end()) return 0;
	auto &b = it->second;
	b.decode(b.ptr, buf, n);
	return b.cb(b.ptr);
}
