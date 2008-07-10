using System;
using System.Collections.Generic;
using System.Text;
using System.Runtime.InteropServices;
using System.ComponentModel;

namespace Fbt.Trace.Reader
{
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    internal struct JPTRCR_CLIENT
    {
        public UInt32 ProcessId;
        public UInt32 ThreadId;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    internal struct JPTRCR_MODULE
    {
        public UInt64 LoadAddress;
        public UInt32 Size;

        [MarshalAs(UnmanagedType.LPWStr)]
        public string Name;

        [MarshalAs(UnmanagedType.LPWStr)]
        public string FilePath;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    internal struct SYMBOL_INFO
    {
        public UInt32 SizeOfStruct;  
        public UInt32 TypeIndex;  
        public UInt64 Reserved1;  
        public UInt64 Reserved2;  
        public UInt32 Index;  
        public UInt32 Size;  
        public UInt64 ModBase; 
        public UInt32 Flags;  
        public UInt64 Value;  
        public UInt64 Address;  
        public UInt32 Register;  
        public UInt32 Scope;  
        public UInt32 Tag;  
        public UInt32 NameLen;  
        public UInt32 MaxNameLen;  

        [MarshalAs(
            UnmanagedType.ByValTStr, 
            /* HACK */ SizeConst=64)]
        public string Name;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    internal struct JPTRCR_CALL_HANDLE
    {
        public IntPtr Chunk;
        public UInt32 Index;
    }

    public enum JPTRCR_CALL_ENTRY_TYPE
    {
        JptrcrNormalEntry,
        JptrcrSyntheticEntry
    }

    public enum JPTRCR_CALL_EXIT_TYPE
    {
        JptrcrNormalExit,
        JptrcrSyntheticExit,
        JptrcrException
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    internal struct JPTRCR_CALL
    {
        public UInt32 EntryType;
        public UInt32 ExitType;
        public JPTRCR_CALL_HANDLE CallHandle;
        public UInt64 Procedure;
        public IntPtr Symbol;
        public IntPtr Module;
        public UInt64 EntryTimestamp;
        public UInt64 ExitTimestamp;
        public UInt32 CallerIp;
        public UInt32 ChildCalls;

        // 
        // Union omitted.
        //
    }

    /*--------------------------------------------------------------
     *
     * Native bridge.
     * 
     */
    public static class Native
    {
        public class TraceFileHandle : SafeHandle
        {
            public TraceFileHandle(IntPtr handle)
                : base(handle, true)
            {
            }

            public override bool IsInvalid
            {
                get
                {
                    return handle == IntPtr.Zero;
                }
            }

            protected override bool ReleaseHandle()
            {
                return Native.JptrcrCloseFile( handle ) > 0;
            }

        }
    
        internal delegate void EnumClientsRoutine(
            ref JPTRCR_CLIENT Client,
            IntPtr Context);

        internal delegate void EnumCallsRoutine(
            ref JPTRCR_CALL Call,
            IntPtr Context);

        [DllImport("jptrcr.dll", 
            CallingConvention=CallingConvention.StdCall, 
            CharSet=CharSet.Unicode)]
        internal extern static int JptrcrOpenFile(
            string FilePath,
            out IntPtr FileHandle
            );

        [DllImport("jptrcr.dll",
            CallingConvention = CallingConvention.StdCall,
            CharSet = CharSet.Unicode)]
        internal extern static int JptrcrCloseFile(
            IntPtr FileHandle
            );

        [DllImport("jptrcr.dll",
            CallingConvention = CallingConvention.StdCall,
            CharSet = CharSet.Unicode)]
        internal extern static int JptrcrEnumClients(
            Native.TraceFileHandle FileHandle,
            IntPtr Callback,
            IntPtr Context );

        [DllImport("jptrcr.dll",
            CallingConvention = CallingConvention.StdCall,
            CharSet = CharSet.Unicode)]
        internal extern static int JptrcrEnumCalls(
            Native.TraceFileHandle FileHandle,
            ref JPTRCR_CLIENT Client,
            IntPtr Callback,
            IntPtr Context);

        [DllImport("jptrcr.dll",
            CallingConvention = CallingConvention.StdCall,
            CharSet = CharSet.Unicode)]
        internal extern static int JptrcrEnumChildCalls(
            Native.TraceFileHandle FileHandle,
            ref JPTRCR_CALL_HANDLE CallerHandle,
            IntPtr Callback,
            IntPtr Context);
    }

    /*--------------------------------------------------------------
     *
     * Wraper
     * 
     */
    public class TraceCall
    {
        private Native.TraceFileHandle Handle;
        private string Function;
        private JPTRCR_MODULE Module;
        private JPTRCR_CALL Call;
        private JPTRCR_CALL_ENTRY_TYPE EntryType;

        internal TraceCall(
            Native.TraceFileHandle Handle,
            JPTRCR_CALL Call, 
            JPTRCR_MODULE Module,
            SYMBOL_INFO SymInfo)
        {
            this.Handle = Handle;
            this.Function = SymInfo.Name;
            this.Call = Call;
            this.Module = Module;
            this.EntryType = ( JPTRCR_CALL_ENTRY_TYPE ) Call.EntryType;
        }

        public String FunctionName
        {
            get
            {
                return this.Function;
            }
        }

        public String ModuleName
        {
            get
            {
                return this.Module.Name;
            }
        }
        
        public String ModulePath
        {
            get
            {
                return this.Module.FilePath;
            }
        }

        public bool IsSynthetic
        {
            get
            {
                return this.EntryType == JPTRCR_CALL_ENTRY_TYPE.JptrcrSyntheticEntry;
            }
        }

        public ICollection<TraceCall> Calls
        {
            get
            {
                ICollection<TraceCall> Accu = new LinkedList<TraceCall>();

                Native.EnumCallsRoutine Delegate =
                    delegate(ref JPTRCR_CALL Call, IntPtr Context)
                    {
                        SYMBOL_INFO Sym = (SYMBOL_INFO)Marshal.PtrToStructure(
                            Call.Symbol,
                            typeof(SYMBOL_INFO));

                        JPTRCR_MODULE Module = (JPTRCR_MODULE)Marshal.PtrToStructure(
                            Call.Module,
                            typeof(JPTRCR_MODULE));
                        Accu.Add(new TraceCall(this.Handle, Call, Module, Sym));
                    };

                IntPtr Callback = Marshal.GetFunctionPointerForDelegate(
                    Delegate);

                int Hr = Native.JptrcrEnumChildCalls(
                    this.Handle,
                    ref this.Call.CallHandle,
                    Callback,
                    IntPtr.Zero);
                if (Hr < 0)
                {
                    throw new Win32Exception(Hr);
                }

                return Accu;
            }
        }

        public override String ToString()
        {
            return this.Module.Name + "!" + this.FunctionName;
        }
    }

    public class TraceClient
    {
        private Native.TraceFileHandle Handle;
        private JPTRCR_CLIENT Client;

        internal TraceClient(
            Native.TraceFileHandle Handle,
            JPTRCR_CLIENT Client
            )
        {
            this.Handle = Handle;
            this.Client = Client;
        }

        public UInt32 ThreadId
        {
            get
            {
                return this.Client.ThreadId;
            }
        }

        public UInt32 ProcessId
        {
            get
            {
                return this.Client.ProcessId;
            }
        }

        public ICollection<TraceCall> Calls
        {
            get
            {
                ICollection<TraceCall> Accu = new LinkedList<TraceCall>();

                Native.EnumCallsRoutine Delegate =
                    delegate(ref JPTRCR_CALL Call, IntPtr Context)
                    {
                        SYMBOL_INFO Sym = (SYMBOL_INFO) Marshal.PtrToStructure(
                            Call.Symbol,
                            typeof(SYMBOL_INFO));

                        JPTRCR_MODULE Module = (JPTRCR_MODULE) Marshal.PtrToStructure(
                            Call.Module,
                            typeof(JPTRCR_MODULE));
                        Accu.Add(new TraceCall(this.Handle, Call, Module, Sym));
                    };

                IntPtr Callback = Marshal.GetFunctionPointerForDelegate(
                    Delegate);

                int Hr = Native.JptrcrEnumCalls(
                    this.Handle,
                    ref this.Client,
                    Callback,
                    IntPtr.Zero);
                if (Hr < 0)
                {
                    throw new Win32Exception(Hr);
                }

                return Accu;
            }
        }
    }

    public class TraceFile : IDisposable
    {
        private Native.TraceFileHandle Handle;

        public TraceFile(string Path)
        {
            IntPtr RawHandle;
            int Hr = Native.JptrcrOpenFile(
                Path,
                out RawHandle);
            if (Hr < 0)
            {
                throw new Win32Exception(Hr);
            }

            this.Handle = new Native.TraceFileHandle(RawHandle);
        }

        public void Dispose()
        {
            this.Handle.Dispose();
        }

        public ICollection<TraceClient> Clients
        {
            get
            {
                ICollection<TraceClient> Accu = new LinkedList<TraceClient>();

                Native.EnumClientsRoutine Delegate = 
                    delegate (ref JPTRCR_CLIENT Client, IntPtr Context)
                    {
                        Accu.Add(new TraceClient(this.Handle, Client));
                    };

                IntPtr Callback = Marshal.GetFunctionPointerForDelegate(
                    Delegate);
                
                int Hr = Native.JptrcrEnumClients(
                    this.Handle,
                    Callback,
                    IntPtr.Zero);
                if (Hr < 0)
                {
                    throw new Win32Exception(Hr);
                }

                return Accu;
            }
        }

    }
}
