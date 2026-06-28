#pragma once
#include <cstdint>

// clean POD data struct — no zproto dependency. Generated (or hand-written).
// This is the user-facing type: plain data, no vtable, no virtuals.
struct frame {
	uint64_t eid;
	float x;
	float y;
	float z;
	float yaw;
	uint32_t seq;
};
