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
    public partial class MainForm : Form
    {
        private static MainForm s_singleton = null;

        private OutputWindow m_outputWindow = new OutputWindow();
        private CommandWindow m_cmdWindow = new CommandWindow();
        private SystemExplorerWindow m_sysExplorer = new SystemExplorerWindow();
        //private SymbolsWindow m_sym = null;
        private PropertiesWindow m_prop = null;
     

        public MainForm()
        {
            InitializeComponent();

            m_sysExplorer.ModuleNodeDoubleClicked += new SystemExplorerWindow.ModuleNodeDoubleClickedDelegate(m_sysExplorer_ModuleNodeDoubleClicked);
            m_sysExplorer.PropertyRequested += new SystemExplorerWindow.PropertyRequestedDelegate(m_sysExplorer_PropertyRequested);
        }

        public static MainForm Instance
        {
            get
            {
                if (s_singleton == null)
                {
                    s_singleton = new MainForm();
                }

                return s_singleton;
            }
        }

        public OutputWindow Output
        {
            get
            {
                return m_outputWindow;
            }
        }

        public CommandWindow Command
        {
            get
            {
                return m_cmdWindow;
            }
        }

        public SystemExplorerWindow SystemExplorer
        {
            get
            {
                return m_sysExplorer;
            }
        }

        //public SymbolsWindow Symbols
        //{
        //    get
        //    {
        //        return m_sym;
        //    }
        //}

        public PropertiesWindow Properties
        {
            get
            {
                if (m_prop == null)
                {
                    m_prop = new PropertiesWindow();
                }
                return m_prop;
            }
        }

        /*--------------------------------------------------------------
         * 
         * Events.
         * 
         */

        void m_sysExplorer_PropertyRequested(object o)
        {
            Properties.SelectedObject = o;

            if (!Properties.Visible)
            {
                Properties.Show(this.m_dockPanel, DockState.DockRight);
            }
        }

        void m_sysExplorer_ModuleNodeDoubleClicked(ModuleNode mn)
        {
            //if (m_sym == null)
            //{
            //    m_sym = new SymbolsWindow(mn);
            //}
                
            //if (!Symbols.Visible)
            //{
            //    Symbols.Show(this.m_dockPanel, DockState.DockRight);
            //}
        }

        private void MainForm_Load(object sender, EventArgs e)
        {
            m_cmdWindow.Show(this.m_dockPanel);
            m_outputWindow.Show(this.m_dockPanel, DockState.DockTop);
            m_sysExplorer.Show(this.m_dockPanel, DockState.DockLeft);
        }

        private void m_menuHelpAbout_Click(object sender, EventArgs e)
        {
            new About().ShowDialog();
        }

        private void m_toolbarOpenCmdWindow_Click(object sender, EventArgs e)
        {
            if (m_cmdWindow.Visible)
            {
                m_cmdWindow.Hide();
            }
            else
            {
                m_cmdWindow.Show(this.m_dockPanel, DockState.DockBottom);
            }
        }

        private void m_toolbarOpenSysExplorer_Click(object sender, EventArgs e)
        {
            if (m_sysExplorer.Visible)
            {
                m_sysExplorer.Hide();
            }
            else
            {
                m_sysExplorer.Show(this.m_dockPanel, DockState.DockLeft);
            }
        }

        private void m_menuOpenSysExplorer_Click(object sender, EventArgs e)
        {
            m_toolbarOpenSysExplorer_Click(sender, e);
        }

        private void m_menuOpenCmdWIndow_Click(object sender, EventArgs e)
        {
            m_toolbarOpenCmdWindow_Click(sender, e);
        }

        private void m_menuEnterCmd_Click(object sender, EventArgs e)
        {
            if (!m_cmdWindow.Visible)
            {
                m_cmdWindow.Show(this.m_dockPanel, DockState.DockBottom);
            }

            m_cmdWindow.SetFocusOnCommandLine();
        }


        private void m_toolbarOpenOutput_Click(object sender, EventArgs e)
        {
            if (m_outputWindow.Visible)
            {
                m_outputWindow.Hide();
            }
            else
            {
                m_outputWindow.Show(this.m_dockPanel, DockState.DockTop);
            }
        }

        private void m_menuOpenOutput_Click(object sender, EventArgs e)
        {
            m_toolbarOpenOutput_Click(sender, e);
        }

        /*--------------------------------------------------------------
         * 
         * Publics.
         * 
         */
        public String Status
        {
            get
            {
                return m_status.Text;
            }
            set
            {
                m_status.Text = value;
            }
        }



    }
}