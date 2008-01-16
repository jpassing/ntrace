namespace WinTrc
{
    partial class SystemExplorerWindow
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
            this.components = new System.ComponentModel.Container();
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(SystemExplorerWindow));
            this.m_toolbar = new System.Windows.Forms.ToolStrip();
            this.m_reload = new System.Windows.Forms.ToolStripButton();
            this.m_tree = new System.Windows.Forms.TreeView();
            this.m_imglist = new System.Windows.Forms.ImageList(this.components);
            this.m_toolbar.SuspendLayout();
            this.SuspendLayout();
            // 
            // m_toolbar
            // 
            this.m_toolbar.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.m_reload});
            this.m_toolbar.Location = new System.Drawing.Point(0, 0);
            this.m_toolbar.Name = "m_toolbar";
            this.m_toolbar.Size = new System.Drawing.Size(292, 25);
            this.m_toolbar.TabIndex = 0;
            this.m_toolbar.Text = "m_toolbar";
            // 
            // m_reload
            // 
            this.m_reload.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.m_reload.Image = ((System.Drawing.Image)(resources.GetObject("m_reload.Image")));
            this.m_reload.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.m_reload.Name = "m_reload";
            this.m_reload.Size = new System.Drawing.Size(23, 22);
            this.m_reload.Text = "Reload";
            this.m_reload.Click += new System.EventHandler(this.m_reload_Click);
            // 
            // m_tree
            // 
            this.m_tree.Dock = System.Windows.Forms.DockStyle.Fill;
            this.m_tree.ImageIndex = 0;
            this.m_tree.ImageList = this.m_imglist;
            this.m_tree.Location = new System.Drawing.Point(0, 25);
            this.m_tree.Name = "m_tree";
            this.m_tree.SelectedImageIndex = 0;
            this.m_tree.Size = new System.Drawing.Size(292, 248);
            this.m_tree.StateImageList = this.m_imglist;
            this.m_tree.TabIndex = 1;
            this.m_tree.BeforeExpand += new System.Windows.Forms.TreeViewCancelEventHandler(this.m_tree_BeforeExpand);
            this.m_tree.AfterSelect += new System.Windows.Forms.TreeViewEventHandler(m_tree_AfterSelect);
            this.m_tree.NodeMouseDoubleClick += new System.Windows.Forms.TreeNodeMouseClickEventHandler(m_tree_NodeMouseDoubleClick);
            this.m_tree.KeyDown += new System.Windows.Forms.KeyEventHandler(m_tree_KeyDown);
            // 
            // m_imglist
            // 
            this.m_imglist.ImageStream = ((System.Windows.Forms.ImageListStreamer)(resources.GetObject("m_imglist.ImageStream")));
            this.m_imglist.TransparentColor = System.Drawing.Color.Transparent;
            this.m_imglist.Images.SetKeyName(0, "otheroptions.ico");
            this.m_imglist.Images.SetKeyName(1, "CLSDFOLD.ICO");
            this.m_imglist.Images.SetKeyName(2, "OPENFOLD.ICO");
            this.m_imglist.Images.SetKeyName(3, "delete_16x.ico");
            this.m_imglist.Images.SetKeyName(4, "idr_dll.ico");
            this.m_imglist.Images.SetKeyName(5, "services.ico");
            // 
            // SystemExplorer
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(292, 273);
            this.Controls.Add(this.m_tree);
            this.Controls.Add(this.m_toolbar);
            this.HideOnClose = true;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "SystemExplorer";
            this.TabText = "SystemExplorer";
            this.Text = "System Explorer";
            this.Load += new System.EventHandler(this.SystemExplorer_Load);
            this.m_toolbar.ResumeLayout(false);
            this.m_toolbar.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        

        
        #endregion

        private System.Windows.Forms.ToolStrip m_toolbar;
        private System.Windows.Forms.TreeView m_tree;
        private System.Windows.Forms.ToolStripButton m_reload;
        private System.Windows.Forms.ImageList m_imglist;
    }
}