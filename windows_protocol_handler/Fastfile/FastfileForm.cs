using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using System.Windows.Forms;
using Microsoft.Win32;

namespace Fastfile
{
    public partial class FastfileForm : Form
    {
        private Uri ffUri = null;
        private Fastfile fastfile = null;

        public FastfileForm(string[] args) {
            if (args.Length > 0) {
                try { ffUri = new Uri(args[0]); }
                catch { }
            }
            InitializeComponent();
        }
        private void FastfileForm_Load(object sender, EventArgs e) {
            if(ffUri != null && ffUri.Host != null)
                ffSelect.Title = "Fastfile - Send file to " + ffUri.Host;
            if (ffUri != null && ffUri.Segments.Length > 0 && ffSelect.ShowDialog() == DialogResult.OK && File.Exists(ffSelect.FileName)) {
                Text = "Fastfile - " + Path.GetFileName(ffSelect.FileName);
                ffUriDisplay.Text = "  " + ffUri.OriginalString;
                WindowState = FormWindowState.Normal;

                /* create a fastfile send instance. */
                try {
                    fastfile = new Fastfile(Application.StartupPath, ffUri.Host, ffUri.Segments[ffUri.Segments.Length - 1].Replace("/", ""));

                    /* incorporate any options based on the uri. */
                    try {
                        if (ffUri.Scheme.Contains("6") || ffUri.Query.Contains("ipv6"))
                            fastfile.set("ipv", 6);
                        if (ffUri.Scheme.EndsWith("s") || ffUri.Query.Contains("ssl"))
                            fastfile.set("ssl", true);
                        if (ffUri.Port > 0)
                            fastfile.set("port", ffUri.Port);
                        if (ffUri.Segments.Length > 2)
                            fastfile.set("location", ffUri.Segments[1].Replace("/", ""));
                    }
                    catch { }

                    /* begin the transfer. */
                    fastfile.send(ffSelect.FileName);
                }
                catch (Exception ex) { MessageBox.Show("Fastfile fault: " + ex.Message); }

                /* monitor progress returned from fastfile. */
                try { ffWorker.RunWorkerAsync(); }
                catch (Exception ex) {
                    MessageBox.Show("Process status monitor failure: " + ex.Message);
                    Application.Exit();
                }
            }
            else
                Application.Exit();
        }
        private void FastfileForm_FormClosing(object sender, FormClosingEventArgs e) {
            try {
                if (fastfile != null)
                    fastfile.stop();
                if (ffWorker != null && ffWorker.IsBusy && !ffWorker.CancellationPending)
                    ffWorker.CancelAsync();
            }
            catch { }
        }

        private void ffCancelButton_Click(object sender, EventArgs e) {
            ffCancelButton.Enabled = false;
            fastfile.stop();
        }

        /* all the worker-bee stuff for monitoring the status of fastfile. */
        private void ffWorker_DoWork(object sender, DoWorkEventArgs e) {
            try {
                int lastStatus = 0;
                while (!ffWorker.CancellationPending && fastfile.complete == Fastfile.completeStatus.running) {
                    if (fastfile != null && lastStatus != fastfile.status)
                    {
                        lastStatus = fastfile.status;
                        ffWorker.ReportProgress(lastStatus);
                    }
                }
            }
            catch { }
        }
        private void ffWorker_ProgressChanged(object sender, ProgressChangedEventArgs e) {
            try {
                ffProgress.Value = e.ProgressPercentage >= 100 ? e.ProgressPercentage - 100 : e.ProgressPercentage;
                Text = "Fastfile - " + Path.GetFileName(ffSelect.FileName) + " [" + ffProgress.Value.ToString() + "%] (" + (e.ProgressPercentage >= 100 ? "2" : "1") + "/2)";
                if (e.ProgressPercentage >= 100)
                    ffLabel.Text = "Sending chunked file to destination...  [" + ffProgress.Value.ToString() + "%]";
                else if (e.ProgressPercentage >= 0)
                    ffLabel.Text = "Building file manifesto to send to server... [" + ffProgress.Value.ToString() + "%]";
            }
            catch { }
        }
        private void ffWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e) {
            ffCancelButton.Enabled = false;
            if (fastfile.complete == Fastfile.completeStatus.error) {
                ffLabel.Text = "Fastfile error, details above.";
                ffProgress.Visible = false;
                ffError.Text = fastfile.message;
                ffError.Visible = true;
            }
            else if (fastfile.complete == Fastfile.completeStatus.success)
                ffLabel.Text = fastfile.message;
        }
    }
}