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

        public event ModuleNodeSelectionChangedDelegate ModuleNodeSelectionChanged;
        public event ModuleNodeDoubleClickedDelegate ModuleNodeDoubleClicked;
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
                    m_processesNode.Nodes.Add(new ProcessNode(info));
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

        
        protected void OnModuleNodeSelectionChanged(ModuleNode mn)
        {
            if (ModuleNodeSelectionChanged != null)
            {
                ModuleNodeSelectionChanged(mn);
            }
        }

        protected void OnModuleNodeDoubleClicked(ModuleNode mn)
        {
            if (ModuleNodeDoubleClicked != null)
            {
                ModuleNodeDoubleClicked(mn);
            }
        }
    }

    class DummyNode : TreeNode
    {
    }

    class ErrorNode : TreeNode
    {
        public ErrorNode(string msg)
            : base("(" + msg + ")",
                   SystemExplorerWindow.ErrorImageIndex,
                   SystemExplorerWindow.ErrorImageIndex)
        {
        }
    }

    class ModulesNode : TreeNode
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
                        Nodes.Add(new ModuleNode(info));
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

    class ThreadsNode : TreeNode
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
                        Nodes.Add(new ThreadNode(info));
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

    class ProcessNode : TreeNode
    {
        private Native.ProcessInfo m_info;

        public ProcessNode(Native.ProcessInfo info)
            : base(info.ExeName + " (" + info.ProcessId + ")",
                   SystemExplorerWindow.ProcessImageIndex,
                   SystemExplorerWindow.ProcessImageIndex)
        {
            m_info = info;

            Nodes.Add(new ModulesNode(info.ProcessId));
            Nodes.Add(new ThreadsNode(info.ProcessId));
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

    public class ModuleNode : TreeNode
    {
        private Native.ModuleInfo m_info;

        public ModuleNode(Native.ModuleInfo info)
            : base(info.ModuleName,
                   SystemExplorerWindow.ModuleImageIndex,
                   SystemExplorerWindow.ModuleImageIndex)
        {
            m_info = info;
        }

        public string ModuleName
        {
            get
            {
                return m_info.ModuleName;
            }
        }
    }

    class ThreadNode : TreeNode
    {
        private Native.ThreadInfo m_info;

        public ThreadNode(Native.ThreadInfo info)
            : base(info.ThreadId.ToString(),
                   SystemExplorerWindow.ThreadImageIndex,
                   SystemExplorerWindow.ThreadImageIndex)
        {
            m_info = info;
        }
    }
}