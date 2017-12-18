#include <assert.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <unordered_set>
#include "hello_world.hpp"

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
	printf("packet::luck size:%lu\n", pk.luck.size());
	for (size_t i = 0; i < pk.luck.size(); i++)
		printf("%ld ", pk.luck[i]);
	printf("\n");
	for (const auto &iter:pk.address1)
		printf("packet::address1 %s\n", iter.c_str());
	printf("\n");
}

static hello::world::packet pk;

static void
test_normal()
{
	std::string dat;
	const uint8_t *datbuf;
	int datsize;
	hello::world::packet pk2;
	hello::world::packet pk3;

	int sz = pk._serialize(dat);
	printf("tag:%x\n", pk._tag());
	printf("encode1 size:%d\n", sz);
	print_hex((uint8_t *)dat.c_str(), dat.size());
	datsize = pk._serialize(&datbuf);
	printf("encode2 size:%d\n", sz);
	print_hex(datbuf, datsize);
	sz = pk2._parse(dat);
	printf("decode1 size:%d\n", sz);
	print_struct(pk2);
	sz = pk3._parse(datbuf, datsize);
	printf("decode2 size:%d\n", sz);
	print_struct(pk3);
	pk3._reset();
	assert(pk3.phone.size() == 0);
	assert(pk3.address.size() == 0);
	assert(pk3.luck.size() == 0);
	assert(pk3.address1.size() == 0);
	assert(pk3.ii == 0);
	assert(pk3.ff == 0.0f);
	assert(pk3.ll == 0);
	assert(pk3.bb == false);
}

static void *
test_thread(void *)
{
	int i;
	hello::world::packet pkk = pk;
	pkk.phone[1].home = 0x3389 + rand() % 10;
	pkk.phone[1].work = 999.98 + rand() % 10;
	std::string pk_dat;
	pkk._serializesafe(pk_dat);
	for (i = 0; i < 100; i++) {
		std::string out;
		pkk._serialize(out);
		if (out != pk_dat)
			printf("[multithread] _serialize memory corrupt\n");
		pkk._serializesafe(out);
		if (out != pk_dat)
			printf("[multithread] _serializesafe memory corrupt\n");
	}
	return NULL;
}


int main()
{
	pthread_t pid1, pid2;
	pk.phone[1].home = -3389;
	pk.phone[1].work = 999.98;
	pk.phone[1].main = false;
	pk.phone2[999.98].home = -3399;
	pk.phone2[999.98].work = 999.98;
	pk.phone2[999.98].main = true;

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
	test_normal();
	pthread_create(&pid1, NULL, test_thread, NULL);
	pthread_create(&pid2, NULL, test_thread, NULL);
	pthread_join(pid1, NULL);
	pthread_join(pid2, NULL);
	return 0;
}

