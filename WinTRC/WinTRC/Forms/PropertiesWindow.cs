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
    public partial class PropertiesWindow : DockContent
    {
        public PropertiesWindow()
        {
            InitializeComponent();
        }

        public Object SelectedObject 
        {
            get
            {
                return m_pg.SelectedObject;
            }
            set
            {
                m_pg.SelectedObject = value;
            }
        }

    }
}