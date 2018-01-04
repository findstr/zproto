using System;
using System.Runtime.InteropServices;
using System.Collections;
using System.Collections.Generic;

namespace zprotobuf
{
	public class zdll
	{
		private const string DLLNAME = "zproto";
		public const int OOM = -1;
		public const int NOFIELD = -2;
		public const int ERROR = -3;
		public struct args
		{
			public int tag;
			public int type;
			public int idx;
			public int len;
			public System.IntPtr ud;
			public int maptag;
			public System.IntPtr name;
			public System.IntPtr mapname;
			public System.IntPtr buff;
			public int buffsz;
			public System.IntPtr sttype;
			public args(int tag) {
				this.tag = tag;
				type = 0;
				idx = 0;
				len = 0;
				ud = System.IntPtr.Zero;
				maptag = 0;
				name = System.IntPtr.Zero;
				mapname = System.IntPtr.Zero;
				buff = System.IntPtr.Zero;
				buffsz = 0;
				sttype = System.IntPtr.Zero;
			}
		};

		public delegate int zproto_cb_t(ref args arg);

		[DllImport(DLLNAME, EntryPoint = "cszload", CallingConvention = CallingConvention.Cdecl)]
		public extern static System.IntPtr load(string name);

		[DllImport(DLLNAME, EntryPoint = "cszparse", CallingConvention = CallingConvention.Cdecl)]
		public extern static System.IntPtr parse(string content);

		[DllImport(DLLNAME, EntryPoint = "cszfree", CallingConvention = CallingConvention.Cdecl)]
		public extern static void free(System.IntPtr z);

		[DllImport(DLLNAME, EntryPoint = "cszquery", CallingConvention = CallingConvention.Cdecl)]
		public extern static System.IntPtr query(System.IntPtr z, string name);

		[DllImport(DLLNAME, EntryPoint = "csztag", CallingConvention = CallingConvention.Cdecl)]
		public extern static int tag(System.IntPtr st);

		[DllImport(DLLNAME, EntryPoint = "cszquerytag", CallingConvention = CallingConvention.Cdecl)]
		public extern static System.IntPtr querytag(System.IntPtr z, int tag);

		[DllImport(DLLNAME, EntryPoint = "cszencode", CallingConvention = CallingConvention.Cdecl)]
		public extern static int encode(System.IntPtr st, System.IntPtr ptr, int len, zproto_cb_t cb, IntPtr obj);

		[DllImport(DLLNAME, EntryPoint = "cszdecode", CallingConvention = CallingConvention.Cdecl)]
		public extern static int decode(System.IntPtr st, System.IntPtr ptr, int len, zproto_cb_t cb, IntPtr obj);

		[DllImport(DLLNAME, EntryPoint = "cszpack", CallingConvention = CallingConvention.Cdecl)]
		public extern static int pack(System.IntPtr src, int srcsz, System.IntPtr dst, int dstsz);

		[DllImport(DLLNAME, EntryPoint = "cszunpack", CallingConvention = CallingConvention.Cdecl)]
		public extern static int unpack(System.IntPtr src, int srcsz, System.IntPtr dst, int dstsz);

	}
}
