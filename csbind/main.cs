using System;
using System.Collections.Generic;
using System.Text;
using hello.world;

namespace test
{
    class Program
    {
        static void Main(string[] args)
        {
		ASCIIEncoding enc = new ASCIIEncoding();
		byte[] data = null;
		byte[] pack = null;
		byte[] unpack = null;
		packet obj = new packet();
		packet res = new packet();
		obj.phoneval = new packet.phone[1];
		obj.phoneval[0] = new packet.phone();
		obj.address = enc.GetBytes("hello");
		obj.luck = new long[1];
		obj.luck[0] = 1122334455;
		obj.address1 = new byte[1][];
		obj.address1[0] = enc.GetBytes("world");
		obj._serialize(out data);
		Console.WriteLine("encode: {0}", BitConverter.ToString(data));
		obj._pack(data, data.Length, out pack);
		Console.WriteLine("pack: {0}", BitConverter.ToString(pack));
		obj._unpack(pack, pack.Length, out unpack);
		Console.WriteLine("unpack: {0}", BitConverter.ToString(unpack));
		res._parse(unpack, unpack.Length);
		Console.WriteLine("world: {0} {1} {2} {3} {4}",
			UTF8Encoding.UTF8.GetString(res.address),
			res.phoneval.Length,
			res.luck[0],
			UTF8Encoding.UTF8.GetString(res.address1[0]),
			res.address1.Length);
        }
    }
}
