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
        private Native.CommandProcessor m_cmdProc = new Native.CommandProcessor();

        public CommandWindow()
        {
            InitializeComponent();
        }

        /*--------------------------------------------------------------
         * 
         * Privates.
         * 
         */
        private void Output(string text)
        {
            m_history.AppendText(text);
            m_history.ScrollToCaret();
        }

        /*--------------------------------------------------------------
         * 
         * History.
         * 
         */
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

        /*--------------------------------------------------------------
         * 
         * Events.
         * 
         */
        private void m_commandLine_KeyDown(object sender, System.Windows.Forms.KeyEventArgs e)
        {
            string cmd = m_commandLine.Text.Trim();
            if (e.KeyCode == Keys.Return && cmd.Length>0)
            {
                PushInputHistory(cmd);

                ProcessCommand(cmd);
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

        /*--------------------------------------------------------------
         * 
         * Publics.
         * 
         */
        public void SetFocusOnCommandLine()
        {
            m_commandLine.Focus();
        }

        delegate void SimpleDelegate();

        public void ProcessCommand(string cmd)
        {
            m_commandLine.Enabled = false;

            //
            // ProcessCommand may block, so call it asynchronously
            // to keep UI responsive.
            //
            SimpleDelegate d = delegate
            {
                m_cmdProc.ProcessCommand(
                    cmd, 
                    delegate (string text){
                        this.Invoke((SimpleDelegate) delegate
                        {
                            Output(text);
                        });
                    });
                this.Invoke((SimpleDelegate)delegate
                {
                    m_commandLine.Enabled = true;
                    Output("\n");
                    m_commandLine.Focus();
                });
            };
            
            d.BeginInvoke(null, null);
        }
    }

}