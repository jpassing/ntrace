namespace WinTrc
{
    partial class SymbolsWindow
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(SymbolsWindow));
            this.m_symbols = new System.Windows.Forms.ListBox();
            this.m_toolbar = new System.Windows.Forms.ToolStrip();
            this.m_searchBox = new System.Windows.Forms.ToolStripTextBox();
            this.m_toolbar.SuspendLayout();
            this.SuspendLayout();
            // 
            // m_symbols
            // 
            this.m_symbols.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom)
                        | System.Windows.Forms.AnchorStyles.Left)
                        | System.Windows.Forms.AnchorStyles.Right)));
            this.m_symbols.FormattingEnabled = true;
            this.m_symbols.Location = new System.Drawing.Point(0, 27);
            this.m_symbols.Name = "m_symbols";
            this.m_symbols.Size = new System.Drawing.Size(292, 238);
            this.m_symbols.TabIndex = 0;
            // 
            // m_toolbar
            // 
            this.m_toolbar.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.m_searchBox});
            this.m_toolbar.Location = new System.Drawing.Point(0, 0);
            this.m_toolbar.Name = "m_toolbar";
            this.m_toolbar.Size = new System.Drawing.Size(292, 25);
            this.m_toolbar.TabIndex = 1;
            // 
            // m_searchBox
            // 
            this.m_searchBox.Name = "m_searchBox";
            this.m_searchBox.Size = new System.Drawing.Size(200, 25);
            // 
            // SymbolsWindow
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(292, 273);
            this.Controls.Add(this.m_toolbar);
            this.Controls.Add(this.m_symbols);
            this.HideOnClose = true;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "SymbolsWindow";
            this.TabText = "SymbolsWindow";
            this.Text = "Symbols";
            this.m_toolbar.ResumeLayout(false);
            this.m_toolbar.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.ListBox m_symbols;
        private System.Windows.Forms.ToolStrip m_toolbar;
        private System.Windows.Forms.ToolStripTextBox m_searchBox;
    }
}