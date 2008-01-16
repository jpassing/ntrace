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
    public partial class CommandWindow : DockContent
    {
        private IList<string> m_cmdHistory = new List<string>(50);
        private int m_cmdHistoryCursor = -1;

        public CommandWindow()
        {
            InitializeComponent();
        }

        private void PushInputHistory(string cmd)
        {
            m_cmdHistory.Add(cmd);
            m_cmdHistoryCursor = m_cmdHistory.Count - 1;
        }

        private string PopPrevInputHistory()
        {
            if (m_cmdHistoryCursor == -1)
            {
                return "";
            }
            else
            {
                return m_cmdHistory[m_cmdHistoryCursor--];
            }
        }

        private string PopNextInputHistory()
        {
            if (m_cmdHistoryCursor == m_cmdHistory.Count - 1)
            {
                return "";
            }
            else
            {
                return m_cmdHistory[++m_cmdHistoryCursor];
            }
        }


        private void AppendHistory(string text)
        {
            m_history.AppendText(text + "\n");
            m_history.ScrollToCaret();
        }

        private void m_commandLine_KeyDown(object sender, System.Windows.Forms.KeyEventArgs e)
        {
            string cmd = m_commandLine.Text.Trim();
            if (e.KeyCode == Keys.Return && cmd.Length>0)
            {
                //
                // Append to history.
                //
                PushInputHistory(cmd);

                AppendHistory(cmd);
                m_commandLine.Clear();
            }
            else if (e.KeyCode == Keys.Up)
            {
                m_commandLine.Text = PopPrevInputHistory();
            }
            else if (e.KeyCode == Keys.Down)
            {
                m_commandLine.Text = PopNextInputHistory();
            }
        }


        public void SetFocusOnCommandLine()
        {
            m_commandLine.Focus();
        }
    }

}