using System;
using System.Diagnostics;
using System.Runtime.InteropServices;

/// <summary>
/// Native code wrappers
/// </summary>
namespace Fbt.Service
{
    /*------------------------------------------------------------------
     *
     * Native Wrappers.
     * 
     */
    public static class Native
    {
        public const Int32 JPFSV_KERNEL = -1;

        /*------------------------------------------------------------------
         *
         * Structs.
         * 
         */
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        public struct ProcessInfo
        {
            public uint Size;
            public uint ProcessId;

            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
            public string ExeName;
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        public struct ModuleInfo
        {
            public uint Size;
            public uint LoadAddress;
            public uint ModuleSize;

            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
            public string ModuleName;

            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
            public string ModulePath;
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        public struct ThreadInfo
        {
            public uint Size;
            public uint ThreadId;
        }

        /*------------------------------------------------------------------
         *
         * DllImports.
         * 
         */
        [DllImport("jpfsv.dll")]
        private extern static int JpfsvEnumProcesses(
            uint Reserved,
            out IntPtr EnumHandle
            );

        [DllImport("jpfsv.dll")]
        private extern static int JpfsvEnumModules(
            uint Reserved,
            uint ProcessId,
            out IntPtr EnumHandle
            );

        [DllImport("jpfsv.dll")]
        private extern static int JpfsvEnumThreads(
            uint Reserved,
            uint ProcessId,
            out IntPtr EnumHandle
            );

        [DllImport("jpfsv.dll", EntryPoint = "JpfsvGetNextItem")]
        private extern static int JpfsvGetNextProcess(
            IntPtr EnumHandle,
            ref ProcessInfo Info
            );

        [DllImport("jpfsv.dll", EntryPoint = "JpfsvGetNextItem")]
        private extern static int JpfsvGetNextModule(
            IntPtr EnumHandle,
            ref ModuleInfo Info
            );

        [DllImport("jpfsv.dll", EntryPoint = "JpfsvGetNextItem")]
        private extern static int JpfsvGetNextThread(
            IntPtr EnumHandle,
            ref ThreadInfo Info
            );

        [DllImport("jpfsv.dll")]
        private extern static int JpfsvCloseEnum(
            IntPtr EnumHandle
            );

        [DllImport("jpfsv.dll")]
        private extern static int JpfsvCreateCommandProcessor(
            IntPtr OutputRoutine,
            Int32 InitialProcessId,
            out IntPtr Processor
            );

        [DllImport("jpfsv.dll")]
        private extern static int JpfsvCloseCommandProcessor(
            IntPtr Processor
            );

        public delegate void OutputDelegate(
            [MarshalAs(UnmanagedType.LPWStr)] string text);

        [DllImport("jpfsv.dll", CharSet = CharSet.Unicode)]
        private extern static int JpfsvProcessCommand(
            IntPtr Processor,
            string Command
            );

        /*------------------------------------------------------------------
         *
         * Resource wrappers.
         * 
         */
        public class CommandProcessor : IDisposable
        {
            private IntPtr m_proc;
            private OutputDelegate outputDelegate;

            public CommandProcessor(
                OutputDelegate outputDelegate,
                int initialProcessId)
            {
                this.outputDelegate = outputDelegate;

                int hr = JpfsvCreateCommandProcessor(
                    Marshal.GetFunctionPointerForDelegate(this.outputDelegate),
                    initialProcessId,
                    out m_proc);
                if (hr < 0)
                {
                    Marshal.ThrowExceptionForHR(hr);
                }
            }

            public void ProcessCommand(string cmd)
            {
                int hr = JpfsvProcessCommand(
                    m_proc,
                    cmd);
                if (hr < 0)
                {
                    Marshal.ThrowExceptionForHR(hr);
                }
            }

            public void Dispose()
            {
                int hr = JpfsvCloseCommandProcessor(m_proc);
                Debug.Assert(hr == 0);
            }
        }

        public class ProcessEnumeration : IDisposable
        {
            private IntPtr m_EnumHandle;

            public ProcessEnumeration()
            {
                int hr = JpfsvEnumProcesses(0, out m_EnumHandle);
                if (hr < 0)
                {
                    Marshal.ThrowExceptionForHR(hr);
                }
            }

            public bool NextItem(ref ProcessInfo Info)
            {
                Debug.Assert(m_EnumHandle != null);

                Info.Size = (uint)Marshal.SizeOf(typeof(ProcessInfo));
                int hr = JpfsvGetNextProcess(m_EnumHandle, ref Info);
                if (hr < 0)
                {
                    Marshal.ThrowExceptionForHR(hr);
                    throw new InvalidOperationException("Cannot get here");
                }
                else if (hr == 1)
                {
                    return false;
                }
                else
                {
                    return true;
                }
            }

            public void Dispose()
            {
                Debug.Assert(m_EnumHandle != null);
                int hr = JpfsvCloseEnum(m_EnumHandle);
                Debug.Assert(hr == 0, "JpfsvCloseEnum");
            }
        }

        public class ModuleEnumeration : IDisposable
        {
            private IntPtr m_EnumHandle;

            public ModuleEnumeration(uint ProcessId)
            {
                int hr = JpfsvEnumModules(0, ProcessId, out m_EnumHandle);
                if (hr < 0)
                {
                    Marshal.ThrowExceptionForHR(hr);
                }
            }

            public bool NextItem(ref ModuleInfo Info)
            {
                Debug.Assert(m_EnumHandle != null);

                Info.Size = (uint)Marshal.SizeOf(typeof(ModuleInfo));
                int hr = JpfsvGetNextModule(m_EnumHandle, ref Info);
                if (hr < 0)
                {
                    Marshal.ThrowExceptionForHR(hr);
                    throw new InvalidOperationException("Cannot get here");
                }
                else if (hr == 1)
                {
                    return false;
                }
                else
                {
                    return true;
                }
            }

            public void Dispose()
            {
                Debug.Assert(m_EnumHandle != null);
                int hr = JpfsvCloseEnum(m_EnumHandle);
                Debug.Assert(hr == 0, "JpfsvCloseEnum");
            }
        }

        public class ThreadEnumeration : IDisposable
        {
            private IntPtr m_EnumHandle;

            public ThreadEnumeration(uint ProcessId)
            {
                int hr = JpfsvEnumThreads(0, ProcessId, out m_EnumHandle);
                if (hr < 0)
                {
                    Marshal.ThrowExceptionForHR(hr);
                }
            }

            public bool NextItem(ref ThreadInfo Info)
            {
                Debug.Assert(m_EnumHandle != null);

                Info.Size = (uint)Marshal.SizeOf(typeof(ThreadInfo));
                int hr = JpfsvGetNextThread(m_EnumHandle, ref Info);
                if (hr < 0)
                {
                    Marshal.ThrowExceptionForHR(hr);
                    throw new InvalidOperationException("Cannot get here");
                }
                else if (hr == 1)
                {
                    return false;
                }
                else
                {
                    return true;
                }
            }

            public void Dispose()
            {
                Debug.Assert(m_EnumHandle != null);
                int hr = JpfsvCloseEnum(m_EnumHandle);
                Debug.Assert(hr == 0, "JpfsvCloseEnum");
            }
        }
    }
}