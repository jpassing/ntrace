using System;
using System.Diagnostics;
using System.Runtime.InteropServices;

/// <summary>
/// Native code wrappers
/// </summary>
namespace WinTrc
{
    public static class Native
    {
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
            public uint BaseAddress;
            public uint ModuleSize;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
            public string ModuleName;
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        public struct ThreadInfo
        {
            public uint Size;
            public uint ThreadId;
        }

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

                Info.Size = (uint) Marshal.SizeOf(typeof(ProcessInfo));
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