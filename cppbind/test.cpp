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

template<typename T> static void
assert_map(T &a, T &b)
{
	for (const auto &iter:a) {
		const auto &biter = b.find(iter.first);
		assert(biter != b.end());
		assert(iter.first == biter->first);
		//assert(iter->second == biter->second);
	};
}

template<typename T> void
assert_vector(T &a, T &b)
{
	assert(a.size() == b.size());
	for (size_t i = 0; i < a.size(); i++)
		assert(a[i] == b[i]);
}
static void
assert_struct(const hello::world::packet &a, const hello::world::packet &b)
{
	for (const auto &iter:b.phone) {
		const auto &aiter = a.phone.find(iter.second.home);
		assert(aiter != a.phone.end());
		assert(iter.second.home == aiter->second.home);
		assert(iter.second.work - aiter->second.work < 0.0001);
		assert(iter.second.home == aiter->second.home);
	};
	for (const auto &iter:b.phone2) {
		const auto &aiter = a.phone2.find(iter.second.work);
		assert(aiter != a.phone2.end());
		assert(iter.second.home == aiter->second.home);
		assert(iter.second.work - aiter->second.work < 0.0001);
		assert(iter.second.home == aiter->second.home);
	};

	assert(a.address == b.address);
	assert(a.luck == b.luck);
	assert_vector(a.address1, b.address1);
	assert(a.ii == b.ii);
	assert(a.ff == b.ff);
	assert(a.bb == b.bb);
}


static hello::world::packet pk;

static void
test_normal()
{
	std::string dat;
	std::string cook, tmp;
	const uint8_t *datbuf;
	const uint8_t *cookbuf;
	int datsize, cooksize;
	hello::world::packet pk2;
	hello::world::packet pk3;

	int sz = pk._serialize(dat);
	printf("tag:%x\n", pk._tag());
	printf("encode1 size:%d\n", sz);
	print_hex((uint8_t *)dat.c_str(), dat.size());
	sz = pk._pack((uint8_t *)dat.c_str(), dat.size(), cook);
	printf("pack1 size:%d\n", sz);
	print_hex((uint8_t *)cook.c_str(), cook.size());

	datsize = pk._serialize(&datbuf);
	printf("encode2 size:%d\n", datsize);
	print_hex(datbuf, datsize);
	cooksize = pk._pack(datbuf, datsize, &cookbuf);
	printf("pack2 size:%d\n", cooksize);
	print_hex(cookbuf, cooksize);
	tmp.assign((char *)cookbuf, cooksize);

	dat.clear();
	pk2._unpack((uint8_t *)cook.c_str(), cook.size(), dat);
	sz = pk2._parse(dat);
	printf("decode1 size:%d\n", sz);
	print_struct(pk2);
	assert_struct(pk2, pk);

	datsize = pk3._unpack((uint8_t *)tmp.c_str(), tmp.size(), &datbuf);
	sz = pk3._parse(datbuf, datsize);
	printf("decode2 size:%d\n", sz);
	print_struct(pk3);
	assert_struct(pk3, pk);
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
	test_normal();
	pthread_create(&pid1, NULL, test_thread, NULL);
	pthread_create(&pid2, NULL, test_thread, NULL);
	pthread_join(pid1, NULL);
	pthread_join(pid2, NULL);
	return 0;
}

