#include <assert.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <pthread.h>
#include <unordered_set>
#include "hello_world.hpp"

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
	std::string dat;
	std::string cook, tmp;
	hello::world::packet pk2;
	hello::world::packet pk3;
	hello::world::packet clr;
	int sz = pk._encode(dat);
	printf("tag:%x\n", pk._tag());
	printf("encode1 size:%d\n", sz);
	print_hex((uint8_t *)dat.c_str(), dat.size());
	sz = pk._pack((uint8_t *)dat.c_str(), dat.size(), cook);
	printf("pack1 size:%d\n", sz);
	print_hex((uint8_t *)cook.c_str(), cook.size());
	//
	sz = pk._encode(tmp);
	printf("encode2 size:%d\n", sz);
	print_hex((uint8_t *)tmp.c_str(), tmp.size());
	sz = pk._pack((uint8_t *)tmp.c_str(), tmp.size(), cook);
	printf("pack2 size:%d\n", sz);
	print_hex((uint8_t *)cook.c_str(), cook.size());
	//
	dat.clear();
	pk2._unpack((uint8_t *)cook.c_str(), cook.size(), dat);
	sz = pk2._decode((const uint8_t *)dat.data(), dat.size());
	printf("decode1 size:%d\n", sz);
	print_struct(pk2);
	assert_struct(pk2, pk);
	//
	pk3._unpack((uint8_t *)cook.c_str(), cook.size(), tmp);
	sz = pk3._decode((const uint8_t *)tmp.data(), tmp.size());
	printf("decode2 size:%d\n", sz);
	print_struct(pk3);
	assert_struct(pk3, pk);
	pk3._reset();
	assert_struct(pk3, clr);
}

static void *
test_thread(void *)
{
	int i;
	hello::world::packet pkk = pk;
	pkk.phone[1].home = 0x3389 + rand() % 10;
	pkk.phone[1].work = 999.98 + rand() % 10;
	std::string pk_dat;
	pkk._encode(pk_dat);
	for (i = 0; i < 100; i++) {
		std::string out;
		pkk._encode(out);
		if (out != pk_dat)
			printf("[multithread] _encode memory corrupt\n");
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
