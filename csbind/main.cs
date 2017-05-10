using System;
using System.Collections.Generic;
using System.Text;
using test_zproto;

namespace test
{
    class Program
    {
        static void Main(string[] args)
        {
		byte[] data = null;
		packet obj = new packet();
		packet res = new packet();
		obj.phoneval = new packet.phone[1];
		obj.phoneval[0] = new packet.phone();
		obj.address = "hello";
		obj.luck = new int[1];
		obj.luck[0] = 3;
		obj.address1 = new string[1];
		obj.address1[0] = "world";
		obj._serialize(out data);
		Console.WriteLine("hello: {0}", BitConverter.ToString(data));
		res._parse(data);
		Console.WriteLine("world: {0} {1} {2} {3} {4}", res.address, res.phoneval.Length, res.luck[0], res.address, res.address1[0], res.address1.Length);
        }
    }
}
