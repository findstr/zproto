#include <string>
#include "bench.pb.h"
using namespace benchpb;
int main() {
	Frame m;
	m.set_eid(0x0123456789abcdefULL);
	m.set_x(1.5f);
	m.set_y(-2.25f);
	m.set_z(3.0f);
	m.set_yaw(-0.5f);
	m.set_seq(42);
	long sink = 0;
	for (long i = 0; i < 1000000; i++) {   // 1M frame marshals
		std::string o;
		m.SerializeToString(&o);
		sink += (long)o.size();
	}
	return (int)(sink & 1);
}
