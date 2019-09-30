﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.InteropServices;

using BizHawk.Emulation.Common;

namespace BizHawk.Emulation.Cores.Consoles.Nintendo.NDS
{
	unsafe partial class DeSmuME : IVideoProvider
	{
		private const int NATIVE_WIDTH = 256;
		/// <summary>
		/// for a single screen
		/// </summary>
		private const int NATIVE_HEIGHT = 192;

		public int VirtualWidth => NATIVE_WIDTH;
		public int VirtualHeight => NATIVE_HEIGHT * 2;

		public int BufferWidth => NATIVE_WIDTH;
		public int BufferHeight => NATIVE_HEIGHT * 2;

		public int VsyncNumerator => 60;

		public int VsyncDenominator => 1;

		public int BackgroundColor => 0;


		[DllImport(dllPath)]
		private static extern void VideoBuffer32bit(int* dstBuffer);

		public int[] GetVideoBuffer()
		{
			int[] ret = new int[NATIVE_WIDTH * NATIVE_HEIGHT * 2];
			fixed (int* v = ret)
			{
				VideoBuffer32bit(v);
			}
			return ret;
		}
	}
}