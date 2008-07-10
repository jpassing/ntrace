using System.Windows.Forms;

namespace WinTrc
{
    partial class MainForm
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(MainForm));
            this.m_dockPanel = new WeifenLuo.WinFormsUI.Docking.DockPanel();
            this.m_mainMenu = new System.Windows.Forms.MenuStrip();
            this.m_menuAction = new System.Windows.Forms.ToolStripMenuItem();
            this.m_menuEnterCmd = new System.Windows.Forms.ToolStripMenuItem();
            this.viewToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.m_menuHelp = new System.Windows.Forms.ToolStripMenuItem();
            this.m_menuHelpAbout = new System.Windows.Forms.ToolStripMenuItem();
            this.m_toolbar = new System.Windows.Forms.ToolStrip();
            this.m_statusBar = new System.Windows.Forms.StatusStrip();
            this.m_status = new System.Windows.Forms.ToolStripStatusLabel();
            this.m_toolbarOpenCmdWindow = new System.Windows.Forms.ToolStripButton();
            this.m_toolbarOpenSysExplorer = new System.Windows.Forms.ToolStripButton();
            this.m_menuOpenCmdWIndow = new System.Windows.Forms.ToolStripMenuItem();
            this.m_menuOpenSysExplorer = new System.Windows.Forms.ToolStripMenuItem();
            this.m_menuOpenOutput = new System.Windows.Forms.ToolStripMenuItem();
            this.m_toolbarOpenOutput = new System.Windows.Forms.ToolStripButton();
            this.m_mainMenu.SuspendLayout();
            this.m_toolbar.SuspendLayout();
            this.m_statusBar.SuspendLayout();
            this.SuspendLayout();
            // 
            // m_dockPanel
            // 
            this.m_dockPanel.ActiveAutoHideContent = null;
            resources.ApplyResources(this.m_dockPanel, "m_dockPanel");
            this.m_dockPanel.DockBottomPortion = 0.4;
            this.m_dockPanel.DockLeftPortion = 0.3;
            this.m_dockPanel.DocumentStyle = WeifenLuo.WinFormsUI.Docking.DocumentStyle.DockingWindow;
            this.m_dockPanel.Name = "m_dockPanel";
            // 
            // m_mainMenu
            // 
            this.m_mainMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.m_menuAction,
            this.viewToolStripMenuItem,
            this.m_menuHelp});
            resources.ApplyResources(this.m_mainMenu, "m_mainMenu");
            this.m_mainMenu.Name = "m_mainMenu";
            // 
            // m_menuAction
            // 
            this.m_menuAction.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.m_menuEnterCmd});
            this.m_menuAction.Name = "m_menuAction";
            resources.ApplyResources(this.m_menuAction, "m_menuAction");
            // 
            // m_menuEnterCmd
            // 
            this.m_menuEnterCmd.Name = "m_menuEnterCmd";
            resources.ApplyResources(this.m_menuEnterCmd, "m_menuEnterCmd");
            this.m_menuEnterCmd.Click += new System.EventHandler(this.m_menuEnterCmd_Click);
            // 
            // viewToolStripMenuItem
            // 
            this.viewToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.m_menuOpenCmdWIndow,
            this.m_menuOpenSysExplorer,
            this.m_menuOpenOutput});
            this.viewToolStripMenuItem.Name = "viewToolStripMenuItem";
            resources.ApplyResources(this.viewToolStripMenuItem, "viewToolStripMenuItem");
            // 
            // m_menuHelp
            // 
            this.m_menuHelp.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.m_menuHelpAbout});
            this.m_menuHelp.Name = "m_menuHelp";
            resources.ApplyResources(this.m_menuHelp, "m_menuHelp");
            // 
            // m_menuHelpAbout
            // 
            this.m_menuHelpAbout.Name = "m_menuHelpAbout";
            resources.ApplyResources(this.m_menuHelpAbout, "m_menuHelpAbout");
            this.m_menuHelpAbout.Click += new System.EventHandler(this.m_menuHelpAbout_Click);
            // 
            // m_toolbar
            // 
            this.m_toolbar.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.m_toolbarOpenCmdWindow,
            this.m_toolbarOpenSysExplorer,
            this.m_toolbarOpenOutput});
            resources.ApplyResources(this.m_toolbar, "m_toolbar");
            this.m_toolbar.Name = "m_toolbar";
            // 
            // m_statusBar
            // 
            resources.ApplyResources(this.m_statusBar, "m_statusBar");
            this.m_statusBar.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.m_status});
            this.m_statusBar.Name = "m_statusBar";
            this.m_statusBar.RenderMode = System.Windows.Forms.ToolStripRenderMode.Professional;
            // 
            // m_status
            // 
            this.m_status.Name = "m_status";
            resources.ApplyResources(this.m_status, "m_status");
            // 
            // m_toolbarOpenCmdWindow
            // 
            this.m_toolbarOpenCmdWindow.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            resources.ApplyResources(this.m_toolbarOpenCmdWindow, "m_toolbarOpenCmdWindow");
            this.m_toolbarOpenCmdWindow.Name = "m_toolbarOpenCmdWindow";
            this.m_toolbarOpenCmdWindow.Click += new System.EventHandler(this.m_toolbarOpenCmdWindow_Click);
            // 
            // m_toolbarOpenSysExplorer
            // 
            this.m_toolbarOpenSysExplorer.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            resources.ApplyResources(this.m_toolbarOpenSysExplorer, "m_toolbarOpenSysExplorer");
            this.m_toolbarOpenSysExplorer.Name = "m_toolbarOpenSysExplorer";
            this.m_toolbarOpenSysExplorer.Click += new System.EventHandler(this.m_toolbarOpenSysExplorer_Click);
            // 
            // m_menuOpenCmdWIndow
            // 
            resources.ApplyResources(this.m_menuOpenCmdWIndow, "m_menuOpenCmdWIndow");
            this.m_menuOpenCmdWIndow.Name = "m_menuOpenCmdWIndow";
            this.m_menuOpenCmdWIndow.Click += new System.EventHandler(this.m_menuOpenCmdWIndow_Click);
            // 
            // m_menuOpenSysExplorer
            // 
            this.m_menuOpenSysExplorer.Image = global::WinTrc.Properties.Resources.ICS_client;
            this.m_menuOpenSysExplorer.Name = "m_menuOpenSysExplorer";
            resources.ApplyResources(this.m_menuOpenSysExplorer, "m_menuOpenSysExplorer");
            this.m_menuOpenSysExplorer.Click += new System.EventHandler(this.m_menuOpenSysExplorer_Click);
            // 
            // m_menuOpenOutput
            // 
            this.m_menuOpenOutput.Image = global::WinTrc.Properties.Resources.document;
            this.m_menuOpenOutput.Name = "m_menuOpenOutput";
            resources.ApplyResources(this.m_menuOpenOutput, "m_menuOpenOutput");
            this.m_menuOpenOutput.Click += new System.EventHandler(this.m_menuOpenOutput_Click);
            // 
            // m_toolbarOpenOutput
            // 
            this.m_toolbarOpenOutput.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            resources.ApplyResources(this.m_toolbarOpenOutput, "m_toolbarOpenOutput");
            this.m_toolbarOpenOutput.Name = "m_toolbarOpenOutput";
            this.m_toolbarOpenOutput.Click += new System.EventHandler(this.m_toolbarOpenOutput_Click);
            // 
            // MainForm
            // 
            resources.ApplyResources(this, "$this");
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this.m_statusBar);
            this.Controls.Add(this.m_toolbar);
            this.Controls.Add(this.m_dockPanel);
            this.Controls.Add(this.m_mainMenu);
            this.MainMenuStrip = this.m_mainMenu;
            this.Name = "MainForm";
            this.WindowState = System.Windows.Forms.FormWindowState.Maximized;
            this.Load += new System.EventHandler(this.MainForm_Load);
            this.m_mainMenu.ResumeLayout(false);
            this.m_mainMenu.PerformLayout();
            this.m_toolbar.ResumeLayout(false);
            this.m_toolbar.PerformLayout();
            this.m_statusBar.ResumeLayout(false);
            this.m_statusBar.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private WeifenLuo.WinFormsUI.Docking.DockPanel m_dockPanel;
        private System.Windows.Forms.MenuStrip m_mainMenu;
        private System.Windows.Forms.ToolStripMenuItem m_menuHelp;
        private System.Windows.Forms.ToolStripMenuItem m_menuHelpAbout;
        private System.Windows.Forms.ToolStrip m_toolbar;
        private System.Windows.Forms.ToolStripButton m_toolbarOpenCmdWindow;
        private System.Windows.Forms.ToolStripButton m_toolbarOpenSysExplorer;
        private System.Windows.Forms.ToolStripMenuItem viewToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem m_menuOpenSysExplorer;
        private System.Windows.Forms.ToolStripMenuItem m_menuOpenCmdWIndow;
        private ToolStripMenuItem m_menuAction;
        private ToolStripMenuItem m_menuEnterCmd;
        private StatusStrip m_statusBar;
        private ToolStripStatusLabel m_status;
        private ToolStripMenuItem m_menuOpenOutput;
        private ToolStripButton m_toolbarOpenOutput;
    }
}

