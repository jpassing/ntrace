using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using WeifenLuo.WinFormsUI.Docking;

namespace WinTrc
{
    public partial class SystemExplorerWindow : DockContent
    {
        public const int ProcessImageIndex = 0;
        public const int ClosedFolderImageIndex = 1;
        public const int OpenFolderImageIndex = 2;
        public const int ErrorImageIndex = 3;
        public const int ModuleImageIndex = 4;
        public const int ThreadImageIndex = 5;

        public delegate void ModuleNodeSelectionChangedDelegate(ModuleNode mn);
        public delegate void ModuleNodeDoubleClickedDelegate(ModuleNode mn);
        public delegate void PropertyRequestedDelegate(object o);

        public event ModuleNodeSelectionChangedDelegate ModuleNodeSelectionChanged;
        public event ModuleNodeDoubleClickedDelegate ModuleNodeDoubleClicked;
        public event PropertyRequestedDelegate PropertyRequested;

        private TreeNode m_processesNode = 
            new TreeNode("Processes",ClosedFolderImageIndex, OpenFolderImageIndex);

        public SystemExplorerWindow()
        {
            InitializeComponent();

            m_tree.Nodes.Add(m_processesNode);
        }



        private void ReloadProcesses()
        {
            if (m_tree.Nodes.Count != 0)
            {
                m_processesNode.Nodes.Clear();
            }

            //
            // Load processes.
            //
            using (Native.ProcessEnumeration pe = new Native.ProcessEnumeration())
            {
                Native.ProcessInfo info = new Native.ProcessInfo();

                while (pe.NextItem(ref info))
                {
                    m_processesNode.Nodes.Add(new ProcessNode(new Process(info)));
                }
            }
        }

        /*--------------------------------------------------------------
         * 
         * Events.
         * 
         */

        private void m_reload_Click(object sender, EventArgs e)
        {
            ReloadProcesses();
        }

        private void SystemExplorer_Load(object sender, EventArgs e)
        {
            ReloadProcesses();
            m_processesNode.Expand();
        }

        void m_tree_BeforeExpand(object sender, System.Windows.Forms.TreeViewCancelEventArgs e)
        {
            ModulesNode mn = e.Node as ModulesNode;
            ThreadsNode tn = e.Node as ThreadsNode;
            if (mn != null)
            {
                mn.LoadChildren();
            }
            else if (tn != null)
            {
                tn.LoadChildren();
            }
        }

        void m_tree_KeyDown(object sender, System.Windows.Forms.KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Return)
            {
                ModuleNode mn = m_tree.SelectedNode as ModuleNode;
                if (mn != null)
                {
                    OnModuleNodeDoubleClicked(mn);
                }
            }
            else if (e.KeyCode == Keys.F4)
            {
                ExplorerNode xn = m_tree.SelectedNode as ExplorerNode;
                if (xn != null && xn.WrappedObject != null)
                {
                    OnPropertyRequested(xn.WrappedObject);
                }
            }
        }

        void m_tree_AfterSelect(object sender, System.Windows.Forms.TreeViewEventArgs e)
        {
            ModuleNode mn = e.Node as ModuleNode;
            if (mn != null)
            {
                OnModuleNodeSelectionChanged(mn);
            }
        }

        void m_tree_NodeMouseDoubleClick(object sender, System.Windows.Forms.TreeNodeMouseClickEventArgs e)
        {
            ModuleNode mn = e.Node as ModuleNode;
            if (mn != null)
            {
                OnModuleNodeDoubleClicked(mn);
            }
        }

        
        protected void OnPropertyRequested(object o)
        {
            if (PropertyRequested != null)
            {
                PropertyRequested(o);
            }
        }

        protected void OnModuleNodeDoubleClicked(ModuleNode mn)
        {
            if (ModuleNodeDoubleClicked != null)
            {
                ModuleNodeDoubleClicked(mn);
            }
        }

        protected void OnModuleNodeSelectionChanged(ModuleNode mn)
        {
            if (ModuleNodeSelectionChanged != null)
            {
                ModuleNodeSelectionChanged(mn);
            }
        }
    }

    /*------------------------------------------------------------------
     * 
     * Tree Node Classes.
     * 
     */

    public class ExplorerNode : TreeNode
    {
        public ExplorerNode() : base()
        {
        }

        public ExplorerNode(string text, int icon, int selIcon)
            : base(text, icon, selIcon)
        {
        }

        //
        // Object that is to be shown in property window.
        //
        public virtual object WrappedObject 
        { 
            get
            {
                return null;
            }
        }
    }

    class DummyNode : ExplorerNode
    {
    }

    class ErrorNode : ExplorerNode
    {
        public ErrorNode(string msg)
            : base("(" + msg + ")",
                   SystemExplorerWindow.ErrorImageIndex,
                   SystemExplorerWindow.ErrorImageIndex)
        {
        }
    }

    class ModulesNode : ExplorerNode
    {
        private uint m_processId;
        private bool m_childrenLoaded = false;

        public ModulesNode(uint processId) : 
            base("Modules",
                 SystemExplorerWindow.ClosedFolderImageIndex,
                 SystemExplorerWindow.OpenFolderImageIndex)
        {
            m_processId = processId;
            Nodes.Add(new DummyNode());
        }

        public void LoadChildren()
        {
            if (m_childrenLoaded)
            {
                return;
            }

            //
            // Load modules.
            //
            Nodes.Clear();
            try
            {
                using (Native.ModuleEnumeration pe =
                    new Native.ModuleEnumeration(m_processId))
                {
                    Native.ModuleInfo info = new Native.ModuleInfo();

                    while (pe.NextItem(ref info))
                    {
                        Nodes.Add(new ModuleNode(new Module(info)));
                    }
                }
            }
            catch (Exception x)
            {
                Nodes.Add(new ErrorNode(x.Message));
            }
            m_childrenLoaded = true;
        }
    }

    class ThreadsNode : ExplorerNode
    {
        private uint m_processId;
        private bool m_childrenLoaded = false;

        public ThreadsNode(uint processId)
            :
            base("Threads",
                 SystemExplorerWindow.ClosedFolderImageIndex,
                 SystemExplorerWindow.OpenFolderImageIndex)
        {
            m_processId = processId;
            Nodes.Add(new DummyNode());
        }

        public void LoadChildren()
        {
            if (m_childrenLoaded)
            {
                return;
            }

            //
            // Load threads.
            //
            Nodes.Clear();
            try
            {
                using (Native.ThreadEnumeration pe =
                    new Native.ThreadEnumeration(m_processId))
                {
                    Native.ThreadInfo info = new Native.ThreadInfo();

                    while (pe.NextItem(ref info))
                    {
                        Nodes.Add(new ThreadNode(new Thread(info)));
                    }
                }
            }
            catch (Exception x)
            {
                Nodes.Add(new ErrorNode(x.Message));
            }
            m_childrenLoaded = true;
        }
    }

    class ProcessNode : ExplorerNode
    {
        private Process m_info;

        public ProcessNode(Process info)
            : base(info.ExeName + " (" + info.ProcessId + ")",
                   SystemExplorerWindow.ProcessImageIndex,
                   SystemExplorerWindow.ProcessImageIndex)
        {
            m_info = info;

            Nodes.Add(new ModulesNode(info.ProcessId));
            Nodes.Add(new ThreadsNode(info.ProcessId));
        }

        public override object WrappedObject
        {
            get
            {
                return m_info;
            }
        }

        public uint Id
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

    public class ModuleNode : ExplorerNode
    {
        private Module m_info;

        public ModuleNode(Module info)
            : base(info.ModuleName,
                   SystemExplorerWindow.ModuleImageIndex,
                   SystemExplorerWindow.ModuleImageIndex)
        {
            m_info = info;
        }

        public override object WrappedObject
        {
            get
            {
                return m_info;
            }
        }

        public string ModuleName
        {
            get
            {
                return m_info.ModuleName;
            }
        }
    }

    class ThreadNode : ExplorerNode
    {
        private Thread m_info;

        public ThreadNode(Thread info)
            : base(info.ThreadId.ToString(),
                   SystemExplorerWindow.ThreadImageIndex,
                   SystemExplorerWindow.ThreadImageIndex)
        {
            m_info = info;
        }

        public override object WrappedObject
        {
            get
            {
                return m_info;
            }
        }
    }
}