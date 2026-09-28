namespace COM
{
    partial class Form1
    {
        private System.ComponentModel.IContainer components = null;

        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        private void InitializeComponent()
        {
            this.components = new System.ComponentModel.Container();
            this.label1 = new System.Windows.Forms.Label();
            this.cboPort = new System.Windows.Forms.ComboBox();
            this.btnOpen = new System.Windows.Forms.Button();
            this.btnClose = new System.Windows.Forms.Button();
            this.txtMesseage = new System.Windows.Forms.TextBox();
            this.btnSend = new System.Windows.Forms.Button();
            this.bntReceive = new System.Windows.Forms.Button();
            this.txtReceive = new System.Windows.Forms.TextBox();
            this.label2 = new System.Windows.Forms.Label();
            this.label3 = new System.Windows.Forms.Label();
            this.serialPort1 = new System.IO.Ports.SerialPort(this.components);
            this.label4 = new System.Windows.Forms.Label();
            this.label5 = new System.Windows.Forms.Label();
            this.txtMotor1 = new System.Windows.Forms.TextBox();
            this.txtMotor2 = new System.Windows.Forms.TextBox();
            this.rbLeft = new System.Windows.Forms.RadioButton();
            this.rbRight = new System.Windows.Forms.RadioButton();
            this.grpDirection = new System.Windows.Forms.GroupBox();
            this.grpDirection.SuspendLayout();
            this.SuspendLayout();
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(133, 61);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(34, 16);
            this.label1.TabIndex = 14;
            this.label1.Text = "Port:";
            // 
            // cboPort
            // 
            this.cboPort.FormattingEnabled = true;
            this.cboPort.Location = new System.Drawing.Point(175, 58);
            this.cboPort.Name = "cboPort";
            this.cboPort.Size = new System.Drawing.Size(160, 24);
            this.cboPort.TabIndex = 13;
            this.cboPort.SelectedIndexChanged += new System.EventHandler(this.cboPort_SelectedIndexChanged);
            // 
            // btnOpen
            // 
            this.btnOpen.Location = new System.Drawing.Point(343, 56);
            this.btnOpen.Name = "btnOpen";
            this.btnOpen.Size = new System.Drawing.Size(81, 25);
            this.btnOpen.TabIndex = 12;
            this.btnOpen.Text = "Open";
            this.btnOpen.UseVisualStyleBackColor = true;
            this.btnOpen.Click += new System.EventHandler(this.btnOpen_Click);
            // 
            // btnClose
            // 
            this.btnClose.Location = new System.Drawing.Point(432, 56);
            this.btnClose.Name = "btnClose";
            this.btnClose.Size = new System.Drawing.Size(81, 25);
            this.btnClose.TabIndex = 11;
            this.btnClose.Text = "Close";
            this.btnClose.UseVisualStyleBackColor = true;
            this.btnClose.Click += new System.EventHandler(this.btnClose_Click);
            // 
            // txtMesseage
            // 
            this.txtMesseage.Location = new System.Drawing.Point(175, 110);
            this.txtMesseage.Multiline = true;
            this.txtMesseage.Name = "txtMesseage";
            this.txtMesseage.Size = new System.Drawing.Size(338, 132);
            this.txtMesseage.TabIndex = 10;
            this.txtMesseage.TextChanged += new System.EventHandler(this.txtMesseage_TextChanged);
            // 
            // btnSend
            // 
            this.btnSend.Location = new System.Drawing.Point(413, 250);
            this.btnSend.Name = "btnSend";
            this.btnSend.Size = new System.Drawing.Size(100, 28);
            this.btnSend.TabIndex = 9;
            this.btnSend.Text = "Send";
            this.btnSend.UseVisualStyleBackColor = true;
            this.btnSend.Click += new System.EventHandler(this.btnSend_Click);
            // 
            // bntReceive
            // 
            this.bntReceive.Location = new System.Drawing.Point(413, 448);
            this.bntReceive.Name = "bntReceive";
            this.bntReceive.Size = new System.Drawing.Size(100, 28);
            this.bntReceive.TabIndex = 7;
            this.bntReceive.Text = "Receive";
            this.bntReceive.UseVisualStyleBackColor = true;
            this.bntReceive.Click += new System.EventHandler(this.bntReceive_Click);
            // 
            // txtReceive
            // 
            this.txtReceive.Location = new System.Drawing.Point(175, 303);
            this.txtReceive.Multiline = true;
            this.txtReceive.Name = "txtReceive";
            this.txtReceive.Size = new System.Drawing.Size(338, 137);
            this.txtReceive.TabIndex = 8;
            this.txtReceive.TextChanged += new System.EventHandler(this.txtReceive_TextChanged);
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(125, 113);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(42, 16);
            this.label2.TabIndex = 6;
            this.label2.Text = "Send:";
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(106, 303);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(61, 16);
            this.label3.TabIndex = 5;
            this.label3.Text = "Receive:";
            // 
            // label4
            // 
            this.label4.AutoSize = true;
            this.label4.Location = new System.Drawing.Point(597, 133);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(96, 16);
            this.label4.TabIndex = 4;
            this.label4.Text = "Motor1_Speed";
            // 
            // label5
            // 
            this.label5.AutoSize = true;
            this.label5.Location = new System.Drawing.Point(597, 183);
            this.label5.Name = "label5";
            this.label5.Size = new System.Drawing.Size(96, 16);
            this.label5.TabIndex = 3;
            this.label5.Text = "Motor2_Speed";
            // 
            // txtMotor1
            // 
            this.txtMotor1.Location = new System.Drawing.Point(699, 130);
            this.txtMotor1.Name = "txtMotor1";
            this.txtMotor1.Size = new System.Drawing.Size(100, 22);
            this.txtMotor1.TabIndex = 2;
            this.txtMotor1.TextChanged += new System.EventHandler(this.txtMotor1_TextChanged);
            // 
            // txtMotor2
            // 
            this.txtMotor2.Location = new System.Drawing.Point(699, 180);
            this.txtMotor2.Name = "txtMotor2";
            this.txtMotor2.Size = new System.Drawing.Size(100, 22);
            this.txtMotor2.TabIndex = 1;
            this.txtMotor2.TextChanged += new System.EventHandler(this.txtMotor2_TextChanged);
            // 
            // rbLeft
            // 
            this.rbLeft.AutoSize = true;
            this.rbLeft.Location = new System.Drawing.Point(17, 28);
            this.rbLeft.Name = "rbLeft";
            this.rbLeft.Size = new System.Drawing.Size(49, 20);
            this.rbLeft.TabIndex = 0;
            this.rbLeft.Text = "Left";
            this.rbLeft.UseVisualStyleBackColor = true;
            this.rbLeft.CheckedChanged += new System.EventHandler(this.rbLeft_CheckedChanged);
            // 
            // rbRight
            // 
            this.rbRight.AutoSize = true;
            this.rbRight.Checked = true;
            this.rbRight.Location = new System.Drawing.Point(17, 74);
            this.rbRight.Name = "rbRight";
            this.rbRight.Size = new System.Drawing.Size(59, 20);
            this.rbRight.TabIndex = 1;
            this.rbRight.TabStop = true;
            this.rbRight.Text = "Right";
            this.rbRight.UseVisualStyleBackColor = true;
            this.rbRight.CheckedChanged += new System.EventHandler(this.rbRight_CheckedChanged);
            // 
            // grpDirection
            // 
            this.grpDirection.Controls.Add(this.rbLeft);
            this.grpDirection.Controls.Add(this.rbRight);
            this.grpDirection.Location = new System.Drawing.Point(805, 105);
            this.grpDirection.Name = "grpDirection";
            this.grpDirection.Size = new System.Drawing.Size(151, 118);
            this.grpDirection.TabIndex = 0;
            this.grpDirection.TabStop = false;
            this.grpDirection.Text = "Direction";
            this.grpDirection.Enter += new System.EventHandler(this.grpDirection_Enter);
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(8F, 16F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(1131, 554);
            this.Controls.Add(this.grpDirection);
            this.Controls.Add(this.txtMotor2);
            this.Controls.Add(this.txtMotor1);
            this.Controls.Add(this.label5);
            this.Controls.Add(this.label4);
            this.Controls.Add(this.label3);
            this.Controls.Add(this.label2);
            this.Controls.Add(this.bntReceive);
            this.Controls.Add(this.txtReceive);
            this.Controls.Add(this.btnSend);
            this.Controls.Add(this.txtMesseage);
            this.Controls.Add(this.btnClose);
            this.Controls.Add(this.btnOpen);
            this.Controls.Add(this.cboPort);
            this.Controls.Add(this.label1);
            this.Name = "Form1";
            this.Text = "Serial Control System";
            this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.Form1_FormClosing);
            this.Load += new System.EventHandler(this.Form1_Load);
            this.grpDirection.ResumeLayout(false);
            this.grpDirection.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.ComboBox cboPort;
        private System.Windows.Forms.Button btnOpen;
        private System.Windows.Forms.Button btnClose;
        private System.Windows.Forms.TextBox txtMesseage;
        private System.Windows.Forms.Button btnSend;
        private System.Windows.Forms.Button bntReceive;
        private System.Windows.Forms.TextBox txtReceive;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Label label3;
        private System.IO.Ports.SerialPort serialPort1;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.Label label5;
        private System.Windows.Forms.TextBox txtMotor1;
        private System.Windows.Forms.TextBox txtMotor2;
        private System.Windows.Forms.RadioButton rbLeft;
        private System.Windows.Forms.RadioButton rbRight;
        private System.Windows.Forms.GroupBox grpDirection;
    }
}
