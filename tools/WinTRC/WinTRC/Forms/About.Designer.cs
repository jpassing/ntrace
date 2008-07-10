namespace WinTrc
{
    partial class About
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(About));
            this.m_aboutText = new System.Windows.Forms.Label();
            this.m_close = new System.Windows.Forms.Button();
            this.SuspendLayout();
            // 
            // m_aboutText
            // 
            resources.ApplyResources(this.m_aboutText, "m_aboutText");
            this.m_aboutText.Name = "m_aboutText";
            // 
            // m_close
            // 
            this.m_close.DialogResult = System.Windows.Forms.DialogResult.OK;
            resources.ApplyResources(this.m_close, "m_close");
            this.m_close.Name = "m_close";
            this.m_close.UseVisualStyleBackColor = true;
            // 
            // About
            // 
            resources.ApplyResources(this, "$this");
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this.m_close);
            this.Controls.Add(this.m_aboutText);
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "About";
            this.ShowInTaskbar = false;
            this.SizeGripStyle = System.Windows.Forms.SizeGripStyle.Hide;
            ((System.ComponentModel.ISupportInitialize)(this.m_pic)).EndInit();
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label m_aboutText;
        private System.Windows.Forms.Button m_close;
        private System.Windows.Forms.PictureBox m_pic;
    }
}