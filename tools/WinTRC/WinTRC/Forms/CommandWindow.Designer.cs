namespace WinTrc
{
    partial class CommandWindow
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(CommandWindow));
            this.m_splitContainer = new System.Windows.Forms.SplitContainer();
            this.m_history = new System.Windows.Forms.TextBox();
            this.m_commandLine = new System.Windows.Forms.TextBox();
            this.m_splitContainer.Panel1.SuspendLayout();
            this.m_splitContainer.Panel2.SuspendLayout();
            this.m_splitContainer.SuspendLayout();
            this.SuspendLayout();
            // 
            // m_splitContainer
            // 
            resources.ApplyResources(this.m_splitContainer, "m_splitContainer");
            this.m_splitContainer.Name = "m_splitContainer";
            // 
            // m_splitContainer.Panel1
            // 
            this.m_splitContainer.Panel1.Controls.Add(this.m_history);
            // 
            // m_splitContainer.Panel2
            // 
            this.m_splitContainer.Panel2.Controls.Add(this.m_commandLine);
            // 
            // m_history
            // 
            this.m_history.BackColor = System.Drawing.SystemColors.Window;
            resources.ApplyResources(this.m_history, "m_history");
            this.m_history.Name = "m_history";
            this.m_history.ReadOnly = true;
            this.m_history.ShortcutsEnabled = false;
            // 
            // m_commandLine
            // 
            resources.ApplyResources(this.m_commandLine, "m_commandLine");
            this.m_commandLine.Name = "m_commandLine";
            this.m_commandLine.KeyDown += new System.Windows.Forms.KeyEventHandler(this.m_commandLine_KeyDown);
            // 
            // CommandWindow
            // 
            resources.ApplyResources(this, "$this");
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this.m_splitContainer);
            this.HideOnClose = true;
            this.Name = "CommandWindow";
            this.m_splitContainer.Panel1.ResumeLayout(false);
            this.m_splitContainer.Panel1.PerformLayout();
            this.m_splitContainer.Panel2.ResumeLayout(false);
            this.m_splitContainer.Panel2.PerformLayout();
            this.m_splitContainer.ResumeLayout(false);
            this.ResumeLayout(false);

        }





        #endregion

        private System.Windows.Forms.SplitContainer m_splitContainer;
        private System.Windows.Forms.TextBox m_commandLine;
        private System.Windows.Forms.TextBox m_history;
    }
}