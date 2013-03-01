namespace Fastfile
{
    partial class FastfileForm
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(FastfileForm));
            this.ffProgress = new System.Windows.Forms.ProgressBar();
            this.ffCancelButton = new System.Windows.Forms.Button();
            this.ffLabel = new System.Windows.Forms.Label();
            this.ffWorker = new System.ComponentModel.BackgroundWorker();
            this.ffSelect = new System.Windows.Forms.OpenFileDialog();
            this.ffError = new System.Windows.Forms.TextBox();
            this.ffUriDisplay = new System.Windows.Forms.TextBox();
            this.SuspendLayout();
            // 
            // ffProgress
            // 
            this.ffProgress.Location = new System.Drawing.Point(12, 31);
            this.ffProgress.Name = "ffProgress";
            this.ffProgress.Size = new System.Drawing.Size(364, 23);
            this.ffProgress.TabIndex = 0;
            // 
            // ffCancelButton
            // 
            this.ffCancelButton.Location = new System.Drawing.Point(273, 64);
            this.ffCancelButton.Name = "ffCancelButton";
            this.ffCancelButton.Size = new System.Drawing.Size(103, 23);
            this.ffCancelButton.TabIndex = 1;
            this.ffCancelButton.Text = "Stop Transfer";
            this.ffCancelButton.UseVisualStyleBackColor = true;
            this.ffCancelButton.Click += new System.EventHandler(this.ffCancelButton_Click);
            // 
            // ffLabel
            // 
            this.ffLabel.AutoSize = true;
            this.ffLabel.Location = new System.Drawing.Point(17, 69);
            this.ffLabel.Name = "ffLabel";
            this.ffLabel.Size = new System.Drawing.Size(0, 13);
            this.ffLabel.TabIndex = 2;
            // 
            // ffWorker
            // 
            this.ffWorker.WorkerReportsProgress = true;
            this.ffWorker.WorkerSupportsCancellation = true;
            this.ffWorker.DoWork += new System.ComponentModel.DoWorkEventHandler(this.ffWorker_DoWork);
            this.ffWorker.RunWorkerCompleted += new System.ComponentModel.RunWorkerCompletedEventHandler(this.ffWorker_RunWorkerCompleted);
            this.ffWorker.ProgressChanged += new System.ComponentModel.ProgressChangedEventHandler(this.ffWorker_ProgressChanged);
            // 
            // ffError
            // 
            this.ffError.BackColor = System.Drawing.SystemColors.ControlLight;
            this.ffError.Location = new System.Drawing.Point(12, 24);
            this.ffError.Multiline = true;
            this.ffError.Name = "ffError";
            this.ffError.ReadOnly = true;
            this.ffError.Size = new System.Drawing.Size(364, 36);
            this.ffError.TabIndex = 3;
            this.ffError.Visible = false;
            // 
            // ffUriDisplay
            // 
            this.ffUriDisplay.BackColor = System.Drawing.SystemColors.ControlLight;
            this.ffUriDisplay.Enabled = false;
            this.ffUriDisplay.ForeColor = System.Drawing.SystemColors.WindowText;
            this.ffUriDisplay.Location = new System.Drawing.Point(-2, -2);
            this.ffUriDisplay.Name = "ffUriDisplay";
            this.ffUriDisplay.ReadOnly = true;
            this.ffUriDisplay.Size = new System.Drawing.Size(392, 20);
            this.ffUriDisplay.TabIndex = 4;
            // 
            // FastfileForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(388, 96);
            this.Controls.Add(this.ffUriDisplay);
            this.Controls.Add(this.ffError);
            this.Controls.Add(this.ffLabel);
            this.Controls.Add(this.ffCancelButton);
            this.Controls.Add(this.ffProgress);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.MaximizeBox = false;
            this.Name = "FastfileForm";
            this.SizeGripStyle = System.Windows.Forms.SizeGripStyle.Hide;
            this.Text = "Fastfile";
            this.WindowState = System.Windows.Forms.FormWindowState.Minimized;
            this.Load += new System.EventHandler(this.FastfileForm_Load);
            this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.FastfileForm_FormClosing);
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.ProgressBar ffProgress;
        private System.Windows.Forms.Button ffCancelButton;
        private System.Windows.Forms.Label ffLabel;
        private System.ComponentModel.BackgroundWorker ffWorker;
        private System.Windows.Forms.OpenFileDialog ffSelect;
        private System.Windows.Forms.TextBox ffError;
        private System.Windows.Forms.TextBox ffUriDisplay;
    }
}

