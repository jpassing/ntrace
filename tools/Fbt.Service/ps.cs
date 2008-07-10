/// <summary>
/// Wrapper classes for Modules/Threads/Processes.
/// </summary>
namespace Fbt.Service
{
    public struct Process
    {
        private Native.ProcessInfo m_info;

        public Process(Native.ProcessInfo info)
        {
            m_info = info;
        }

        public uint ProcessId
        {
            get
            {
                return m_info.ProcessId;
            }
        }

        public string ExeName
        {
            get
            {
                return m_info.ExeName;
            }
        }
    }

    public struct Module
    {
        private Native.ModuleInfo m_info;

        public Module(Native.ModuleInfo info)
        {
            m_info = info;
        }

        public uint LoadAddress
        {
            get
            {
                return m_info.LoadAddress;
            }
        }

        public uint ModuleSize
        {
            get
            {
                return m_info.ModuleSize;
            }
        }

        public string ModuleName
        {
            get
            {
                return m_info.ModuleName;
            }
        }

        public string ModulePath
        {
            get
            {
                return m_info.ModulePath;
            }
        }
    }

    public struct Thread
    {
        private Native.ThreadInfo m_info;

        public Thread(Native.ThreadInfo info)
        {
            m_info = info;
        }

        public uint ThreadId
        {
            get
            {
                return m_info.ThreadId;
            }
        }
    }
}