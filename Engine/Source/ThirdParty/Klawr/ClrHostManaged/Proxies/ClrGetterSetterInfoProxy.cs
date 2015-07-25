using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Klawr.ClrHost.Managed.SafeHandles;
using System.Runtime.InteropServices;


namespace Klawr.ClrHost.Managed.Proxies
{
    [ComVisible(true)]
    [Guid("0C14EE29-4E37-43A5-9666-C19B3C29D7F9")]
    [StructLayout(LayoutKind.Sequential)]
    public struct ClrGetterSetterInfoProxy
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void FloatSetter(long instanceID, string propertyName, float value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void IntSetter(long instanceID, string propertyName, float value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void BoolSetter(long instanceID, string propertyName, float value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void StrSetter(long instanceID, string propertyName, float value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void ObjSetter(long instanceID, string propertyName, IntPtr value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate float FloatGetter(long instanceID, string propertyName);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate int IntGetter(long instanceID, string propertyName);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate bool BoolGetter(long instanceID, string propertyName);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate string StrGetter(long instanceID, string propertyName);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate IntPtr ObjGetter(long instanceID, string propertyName);

        [MarshalAs(UnmanagedType.FunctionPtr)]
        public FloatSetter SetFloat;

        [MarshalAs(UnmanagedType.FunctionPtr)]
        public IntSetter SetInt;

        [MarshalAs(UnmanagedType.FunctionPtr)]
        public BoolSetter SetBool;

        [MarshalAs(UnmanagedType.FunctionPtr)]
        public StrSetter SetStr;

        [MarshalAs(UnmanagedType.FunctionPtr)]
        public ObjSetter SetObj;

        [MarshalAs(UnmanagedType.FunctionPtr)]
        public FloatGetter GetFloat;

        [MarshalAs(UnmanagedType.FunctionPtr)]
        public IntGetter GetInt;

        [MarshalAs(UnmanagedType.FunctionPtr)]
        public BoolGetter GetBool;

        [MarshalAs(UnmanagedType.FunctionPtr)]
        public StrGetter GetStr;

        [MarshalAs(UnmanagedType.FunctionPtr)]
        public ObjGetter GetObj;

    }
}
