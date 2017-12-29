using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Diagnostics;

namespace zprotobuf
{
	public abstract class wire {
		private static int oid = 0;
		private static Dictionary<IntPtr, wire> uds = new Dictionary<IntPtr, wire>();
		private static int encode_cb(ref dll.args arg) {
			wire obj = uds[arg.ud];
			return obj._encode_field(ref arg);
		}

		private static int decode_cb(ref dll.args arg) {
			wire obj = uds[arg.ud];
			return obj._decode_field(ref arg);
		}

		private IntPtr udbegin() {
			IntPtr id = (IntPtr)(++oid);
			uds[id] = this;
			return id;
		}
		private void udend(IntPtr id) {
			uds[id] = null;
		}

		protected int write(ref dll.args arg, byte[] src) {
			if (src.Length > arg.buffsz)
				return dll.OOM;
			Marshal.Copy(src, 0, arg.buff, src.Length);
			return src.Length;
		}

		protected int write(ref dll.args arg, bool val) {
			byte[] src = BitConverter.GetBytes(val);
 			return write(ref arg, src);
		}

		protected int write(ref dll.args arg, int val) {
			byte[] src = BitConverter.GetBytes(val);
			return write(ref arg, src);
		}

		protected int write(ref dll.args arg, long val) {
			byte[] src = BitConverter.GetBytes(val);
			return write(ref arg, src);
		}

		protected int write(ref dll.args arg, float val) {
			byte[] src = BitConverter.GetBytes(val);
			return write(ref arg, src);
		}

		private byte[] read(ref dll.args arg) {
			byte[] ret = new byte[arg.buffsz];
			Marshal.Copy(arg.buff, ret, 0, ret.Length);
			return ret;
		}
		protected int read(ref dll.args arg, out bool val) {
			val = false;
			if (arg.buffsz < 1)
				return dll.ERROR;
			val = BitConverter.ToBoolean(read(ref arg), 0);
			return arg.buffsz;
		}
		protected int read(ref dll.args arg, out int val) {
			val = 0;
			if (arg.buffsz < sizeof(int))
				return dll.ERROR;
			val = BitConverter.ToInt32(read(ref arg), 0);
			return arg.buffsz;
		}
		protected int read(ref dll.args arg, out long val) {
			val = 0;
			if (arg.buffsz < sizeof(long))
				return dll.ERROR;
			val = BitConverter.ToInt64(read(ref arg), 0);
			return arg.buffsz;
		}
		protected int read(ref dll.args arg, out float val) {
			val = 0.0f;
			if (arg.buffsz < sizeof(float))
				return dll.ERROR;
			val = BitConverter.ToSingle(read(ref arg), 0);
			return arg.buffsz;
		}
		protected int read(ref dll.args arg, out byte[] val) {
			val = read(ref arg);
			return arg.buffsz;
		}

		public int _encode(IntPtr buff, int len, IntPtr st) {
			IntPtr id = udbegin();
			int err = dll.encode(st, buff, len, encode_cb, id);
			udend(id);
			return err;
		}

		public int _decode(IntPtr buff, int len, IntPtr st) {
			IntPtr id = udbegin();
			int err = dll.decode(st, buff, len, decode_cb, id);
			udend(id);
			return err;
		}

		public virtual int _serialize(out byte[] dat) {
			dat = null;
			Debug.Assert("NotImplement" == null);
			return dll.ERROR;
		}
		public virtual int _parse(byte[] dat, int size) {
			Debug.Assert("NotImplement" == null);
			return dll.ERROR;
		}
		public virtual int _pack(byte[] src, int size, out byte[] dst) {
			dst = null;
			Debug.Assert("NotImplement" == null);
			return dll.ERROR;
		}
		public virtual int _unpack(byte[] src, int size, out byte[] dst) {
			dst = null;
			Debug.Assert("NotImplement" == null);
			return dll.ERROR;
		}
		public virtual int _tag() {
			Debug.Assert("NotImplement" == null);
			return 0;
		}
		//abstract function
		public abstract string _name();
		protected abstract int _encode_field(ref dll.args arg);
		protected abstract int _decode_field(ref dll.args arg);
	}

	public class wiretree {
		private string protodef = "";
		private IntPtr Z = IntPtr.Zero;
		private int buffsize = 64;
		private Dictionary<string, IntPtr> cache = new Dictionary<string,IntPtr>();
		public wiretree(string def) {
			protodef = def;
			Z = dll.parse(protodef);
		}
		~wiretree() {

		}
		private IntPtr query(string name) {
			if (cache.ContainsKey(name))
				return cache[name];
			IntPtr st = dll.query(Z, name);
			cache[name] = st;
			return st;
		}

		public int tag(string name) {
			IntPtr st = query(name);
			return dll.tag(st);
		}

		private IntPtr expand(IntPtr buf) {
			buffsize *= 2;
			return Marshal.ReAllocHGlobal(buf, (IntPtr)buffsize);
		}

		public int encode(wire obj, out byte[] data) {
			data = null;
			IntPtr st = query(obj._name());
			IntPtr buff = Marshal.AllocHGlobal(buffsize);
			for (;;) {
				int sz = obj._encode(buff, buffsize, st);
				if (sz == dll.ERROR) {
					Marshal.FreeHGlobal(buff);
					return sz;
				}
				if (sz == dll.OOM) {
					buff = expand(buff);
					continue;
				}
				data = new byte[sz];
				Marshal.Copy(buff, data, 0, sz);
				Marshal.FreeHGlobal(buff);
				return sz;
			}
		}
		public int decode(wire obj, byte[] data, int size) {
			int ret;
			IntPtr st = query(obj._name());
			while (buffsize < size)
				buffsize *= 2;
			IntPtr buff = Marshal.AllocHGlobal(buffsize);
			Marshal.Copy(data, 0, buff, size);
			ret = obj._decode(buff, size, st);
			Marshal.FreeHGlobal(buff);
			return ret;

		}
		public int pack(byte[] src, int size, out byte[] dst) {
			IntPtr srcptr, dstptr;
			while (buffsize < size)
				buffsize *= 2;
			srcptr = Marshal.AllocHGlobal(buffsize);
			dstptr = Marshal.AllocHGlobal(buffsize);
			Marshal.Copy(src, 0, srcptr, size);
			dst = null;
			for (;;) {
				int ret = dll.pack(srcptr, size, dstptr, buffsize);
				if (ret == dll.ERROR) {
					Marshal.FreeHGlobal(srcptr);
					Marshal.FreeHGlobal(dstptr);
					return ret;
				}
				if (ret == dll.OOM) {
					dstptr = expand(dstptr);
					continue;
				}
				dst = new byte[ret];
				Marshal.Copy(dstptr, dst, 0, ret);
				Marshal.FreeHGlobal(srcptr);
				Marshal.FreeHGlobal(dstptr);
				return ret;
			}
		}
		public int unpack(byte[] src, int size, out byte[] dst) {
			IntPtr srcptr, dstptr;
			while (buffsize < size * 2)	//assume compress ratio is 0.5
				buffsize *= 2;
			srcptr = Marshal.AllocHGlobal(buffsize);
			dstptr = Marshal.AllocHGlobal(buffsize);
			Marshal.Copy(src, 0, srcptr, size);
			dst = null;
			for (;;) {
				int ret = dll.unpack(srcptr, size, dstptr, buffsize);
				if (ret == dll.ERROR) {
					Marshal.FreeHGlobal(srcptr);
					Marshal.FreeHGlobal(dstptr);
					return ret;
				}
				if (ret == dll.OOM) {
					dstptr = expand(dstptr);
					continue;
				}
				dst = new byte[ret];
				Marshal.Copy(dstptr, dst, 0, ret);
				Marshal.FreeHGlobal(srcptr);
				Marshal.FreeHGlobal(dstptr);
				return ret;
			}
		}
	}

	public abstract class iwirep:wire {
		public override int _serialize(out byte[] dat) {
			return _wiretree().encode(this, out dat);
		}
		public override int _parse(byte[] dat, int size) {
			return _wiretree().decode(this, dat, size);
		}
		public override int _pack(byte[] src, int size, out byte[] dst) {
			return _wiretree().pack(src, size, out dst);
		}
		public override int _unpack(byte[] src, int size, out byte[] dst) {
			return _wiretree().unpack(src, size, out dst);
		}
		public override int _tag() {
			return _wiretree().tag(_name());
		}
		protected abstract wiretree _wiretree();
	}
}
