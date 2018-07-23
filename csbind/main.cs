using System;
using System.Diagnostics;
using System.Reflection;
using System.Collections.Generic;
using System.Text;
using hello.world;

namespace test {

class Program {
	static ASCIIEncoding enc = new ASCIIEncoding();
	static void assert_foo(hello.world.packet.foo a, hello.world.packet.foo b)
	{
		Console.WriteLine("pakcet.foo bar1:{0} int8:{1}, int16:{2}", a.bar1, a.int8, a.int16);
		Debug.Assert(a.bar1 == b.bar1);
		Debug.Assert(a.int8 == b.int8);
		Debug.Assert(a.int16 == b.int16);
	}
	static void assert_phone(hello.world.packet.phone a, hello.world.packet.phone b)
	{
		Console.WriteLine("packet.phone home:{0} work:{1} main:{2}", a.home, a.work, b.main);
		assert_foo(a.fooval, b.fooval);
		Debug.Assert(a.home == b.home);
		Debug.Assert(a.work == b.work);
		Debug.Assert(a.main == b.main);
	}
	static void assert_packet(hello.world.packet a, hello.world.packet b)
	{
		Console.WriteLine("packet.address:{0}", enc.GetString(a.address));
		for (int i = 0; i < a.luck.Length; i++)
			Console.WriteLine("packet.luck[{1}]:{0}", a.luck[i], i);
		for (int i = 0; i < a.address1.Length; i++)
			Console.WriteLine("packet.address1[{1}]:{0}", a.address1[i], i);
		Console.WriteLine("packet.foval.bar2:{0}", a.fooval.bar2);
		assert_phone(a.phoneval[0], b.phoneval[0]);
		Debug.Assert(a.address == b.address);
		Debug.Assert(a.luck == b.luck);
		Debug.Assert(a.address1 == b.address1);
		Debug.Assert(a.fooval.bar2 == b.fooval.bar2);
	}

	static void Main(string[] args)
	{
		byte[] data = null;
		byte[] pack = null;
		byte[] unpack = null;
		packet obj = new packet();
		packet res = new packet();
		obj.phoneval = new packet.phone[1];
		obj.phoneval[0] = new packet.phone();
		obj.phoneval[0].home = 3389;
		obj.phoneval[0].work = 3389.1f;
		obj.phoneval[0].main = true;
		obj.phoneval[0].fooval = new packet.foo();
		obj.phoneval[0].fooval.bar1 = 3.99f;
		obj.phoneval[0].fooval.int8 = 127;
		obj.phoneval[0].fooval.int16 = 256;
		obj.address = enc.GetBytes("hello");
		obj.luck = new long[1];
		obj.luck[0] = 1122334455;
		obj.address1 = new string[1];
		obj.address1[0] = "world";
		obj.fooval = new foo2();
		obj.fooval.bar2 = 3.3f;
		obj._serialize(out data);
		Console.WriteLine("encode: {0}", BitConverter.ToString(data));
		obj._pack(data, data.Length, out pack);
		Console.WriteLine("pack: {0}", BitConverter.ToString(pack));
		obj._unpack(pack, pack.Length, out unpack);
		Console.WriteLine("unpack: {0}", BitConverter.ToString(unpack));
		res._parse(unpack, unpack.Length);
		assert_packet(res, obj);
	}
}

}
