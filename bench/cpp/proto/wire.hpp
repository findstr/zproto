#pragma once
#include <string>
#include <cstdint>

namespace zproto {

// pre-sized byte buffer: encode resizes once then writes by index (no push_back,
// no growth); decode reads raw. This is the fast path validated in profiling.
struct Buffer {
	std::string s;
	void resize(size_t n) { s.resize(n); }
	void clear() { s.clear(); }
	char *data() { return &s[0]; }
	const char *cdata() const { return s.data(); }
	size_t size() const { return s.size(); }
};

// traits primary — specialize per message type to enable tag/name/byte_size/encode/decode.
// Like std::hash<T>: non-intrusive, opt-in, the data type knows nothing of it.
template<class T> struct wire;

}  // namespace zproto
