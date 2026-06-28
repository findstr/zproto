#include "hello_world.hpp"
#include "hello_world.message.hpp"
#include "zproto.hpp"
#include <assert.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <pthread.h>
#include <unordered_set>

using namespace zproto;

static hello::world::packet pk;

static void
print_hex(const uint8_t *buf, size_t sz)
{
	for (size_t i = 0; i < sz; i++)
		printf("%x ", buf[i]);
	printf("\n");
}

static void
print_struct(const hello::world::packet &pk)
{
	for (auto &iter:pk.phone) {
		printf("packet::phone[%d]::home:%d\n", iter.first, iter.second.home);
		printf("packet::phone[%d]::work:%f\n", iter.first, iter.second.work);
	}
	for (auto &iter:pk.phone2) {
		printf("packet::phone2[%f]::home:%d\n", iter.first, iter.second.home);
		printf("packet::phone2[%f]::work:%f\n", iter.first, iter.second.work);
	}
	printf("packet::address:%s\n", pk.address.c_str());
	printf("packet::bb:%s\n", pk.bb ? "true" : "false");
	printf("packet::int8:%d\n", pk.int8);
	printf("packet::int16:%d\n", pk.int16);
	printf("packet::luck size:%lu\n", pk.luck.size());
	for (size_t i = 0; i < pk.luck.size(); i++)
		printf("%ld ", pk.luck[i]);
	printf("\n");
	for (const auto &iter:pk.address1)
		printf("packet::address1 %s\n", iter.c_str());
	printf("\n");
}

static void
assert_foo(const hello::world::packet::foo &a, const hello::world::packet::foo &b)
{
	assert(a.bar1 - b.bar1 < 0.0001f);
}

static void
assert_foo2(const hello::world::foo2 &a, const hello::world::foo2 &b)
{
	assert(a.bar2 == b.bar2);
}

static void
assert_phone(const struct hello::world::packet::phone &a, const struct hello::world::packet::phone &b)
{
	assert(a.home == b.home);
	assert(abs(a.work - b.work) < 0.0001f);
	assert(a.main == b.main);
	assert_foo(a.fooval, b.fooval);
	assert_foo2(a.fooval2, b.fooval2);
}

static void
assert_struct(const hello::world::packet &a, const hello::world::packet &b)
{
	for (const auto &iter:b.phone) {
		const auto &aiter = a.phone.find(iter.second.home);
		assert_phone(iter.second, aiter->second);
	}
	for (const auto &iter:b.phone2) {
		const auto &aiter = a.phone2.find(iter.second.work);
		assert(aiter != a.phone2.end());
		assert_phone(iter.second, aiter->second);
	};
	assert(a.address == b.address);
	assert(a.luck == b.luck);
	assert(a.address1 == b.address1);
	assert(a.ii == b.ii);
	assert(a.ff == b.ff);
	assert(a.ll == b.ll);
	assert(a.bb == b.bb);
	assert(a.int8 == b.int8);
	assert(a.int16 == b.int16);
}

static void
test_normal()
{
	Buffer dat, cook, tmp, up;
	hello::world::packet pk2;
	hello::world::packet clr;

	message<hello::world::packet>::encode(pk, dat);
	printf("tag:%x\n", message<hello::world::packet>::tag());
	printf("encode1 size:%zu\n", dat.size());
	print_hex((uint8_t *)dat.data(), dat.size());

	pack((const uint8_t *)dat.data(), (int)dat.size(), cook);
	printf("pack1 size:%zu\n", cook.size());
	print_hex((uint8_t *)cook.data(), cook.size());

	message<hello::world::packet>::encode(pk, tmp);
	printf("encode2 size:%zu\n", tmp.size());
	print_hex((uint8_t *)tmp.data(), tmp.size());

	// unpack -> decode (round 1)
	up.clear();
	unpack((const uint8_t *)cook.data(), (int)cook.size(), up);
	message<hello::world::packet>::decode(pk2, (const uint8_t *)up.data(), up.size());
	print_struct(pk2);
	assert_struct(pk2, pk);

	// unpack -> decode (round 2, fresh buffer)
	Buffer up2;
	unpack((const uint8_t *)cook.data(), (int)cook.size(), up2);
	hello::world::packet pk3;
	message<hello::world::packet>::decode(pk3, (const uint8_t *)up2.data(), up2.size());
	print_struct(pk3);
	assert_struct(pk3, pk);

	// reset == assign a default-constructed POD (no _reset() in the new API)
	pk3 = clr;
	assert_struct(pk3, clr);
}

static void *
test_thread(void *)
{
	int i;
	hello::world::packet pkk = pk;
	pkk.phone[1].home = 0x3389 + rand() % 10;
	pkk.phone[1].work = 999.98f + rand() % 10;
	Buffer pk_dat;
	message<hello::world::packet>::encode(pkk, pk_dat);
	for (i = 0; i < 100; i++) {
		Buffer out;
		message<hello::world::packet>::encode(pkk, out);
		if (out.size() != pk_dat.size() ||
			memcmp(out.data(), pk_dat.data(), out.size()) != 0)
			printf("[multithread] encode output differs\n");
	}
	return NULL;
}

int main()
{
	pthread_t pid1, pid2;
	pk.phone[1].home = -3389;
	pk.phone[1].work = 999.98;
	pk.phone[1].main = false;
	pk.phone[1].fooval.bar1 = 9.9f;
	pk.phone[1].fooval2.bar2 = 8.9f;
	pk.phone2[999.98].home = -3399;
	pk.phone2[999.98].work = 999.98;
	pk.phone2[999.98].main = true;
	pk.phone2[999.98].fooval.bar1 = 9.8f;
	pk.phone2[999.98].fooval2.bar2 = 8.8f;
	pk.address = "ShangHai";
	pk.luck.push_back(3);
	pk.luck.push_back(7);
	pk.luck.push_back(5);
	pk.address1.push_back("hello");
	pk.address1.push_back("world");
	pk.ii = 6;
	pk.ff = 7.0f;
	pk.ll = 8;
	pk.bb = true;
	pk.int8 = 127;
	pk.int16 = 256;
	test_normal();
	pthread_create(&pid1, NULL, test_thread, NULL);
	pthread_create(&pid2, NULL, test_thread, NULL);
	pthread_join(pid1, NULL);
	pthread_join(pid2, NULL);
	return 0;
}
