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
    public partial class SymbolsWindow : DockContent
    {
        private ModuleNode m_curModule;

        public SymbolsWindow(ModuleNode mn)
        {
            InitializeComponent();

            m_curModule = mn;
            this.TabText = mn.ModuleName;

            MainForm.Instance.SystemExplorer.ModuleNodeSelectionChanged 
                += new SystemExplorerWindow.ModuleNodeSelectionChangedDelegate(
                    SystemExplorer_ModuleNodeSelectionChanged);
        }

        void SystemExplorer_ModuleNodeSelectionChanged(ModuleNode mn)
        {
            m_curModule = mn;

            if (this.Visible)
            {
                this.TabText = mn.ModuleName;
            }
        }
    }
}