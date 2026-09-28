using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO.Ports;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;


namespace COM
{

    public partial class Form1 : Form
    {

        private int lastM1Speed = -1;
        private int lastM2Speed = -1;
        private int lastDir = -1; // 0: Stop, 1: CW, 2: CCW
        public Form1()
        {
            InitializeComponent();
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            try
            {
                string[] ports = SerialPort.GetPortNames();
                cboPort.Items.AddRange(ports);
                if (ports.Length > 0) cboPort.SelectedIndex = 0;

                btnClose.Enabled = false;
                serialPort1.DataReceived += SerialPort1_DataReceived;
            }
            catch (Exception ex)
            {
                MessageBox.Show("Error loading ports: " + ex.Message);
            }
        }

        private void btnOpen_Click(object sender, EventArgs e)
        {
            try
            {
                if (cboPort.SelectedItem == null) return;

                serialPort1.PortName = cboPort.Text;
                serialPort1.BaudRate = 9600;
                serialPort1.Open();

                btnOpen.Enabled = false;
                btnClose.Enabled = true;
            }
            catch (Exception ex)
            {
                MessageBox.Show("Could not open port: " + ex.Message);
            }
        }

        private void btnClose_Click(object sender, EventArgs e)
        {
            try
            {
                if (serialPort1.IsOpen) serialPort1.Close();
                btnOpen.Enabled = true;
                btnClose.Enabled = false;
            }
            catch (Exception ex)
            {
                MessageBox.Show("Error closing port: " + ex.Message);
            }
        }

        private void bntReceive_Click(object sender, EventArgs e)
        {
            if (serialPort1.IsOpen)
            {
                try
                {
                    int bytesToRead = serialPort1.BytesToRead;
                    if (bytesToRead > 0)
                    {
                        byte[] buffer = new byte[bytesToRead];
                        serialPort1.Read(buffer, 0, bytesToRead);
                        txtReceive.Text = BitConverter.ToString(buffer);
                    }
                }
                catch (Exception ex)
                {
                    MessageBox.Show("Error reading port: " + ex.Message);
                }
            }
        }

        private void SerialPort1_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            try
            {
                int bytesToRead = serialPort1.BytesToRead;
                byte[] buffer = new byte[bytesToRead];
                serialPort1.Read(buffer, 0, bytesToRead);

                string hexString = BitConverter.ToString(buffer) + Environment.NewLine;

                this.Invoke(new MethodInvoker(delegate {
                    txtReceive.AppendText(hexString);
                }));
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine("UART Rx Error: " + ex.Message);
            }
        }

        private void Form1_FormClosing(object sender, FormClosingEventArgs e)
        {
            if (serialPort1.IsOpen)
            {
                serialPort1.Close();
            }
        }

        private void btnSend_Click(object sender, EventArgs e)
        {
            if (!serialPort1.IsOpen)
            {
                MessageBox.Show("Please open the port first.");
                return;
            }

            int currentM1Speed = string.IsNullOrEmpty(txtMotor1.Text) ? 0 : Convert.ToInt32(txtMotor1.Text);
            int currentM2Speed = string.IsNullOrEmpty(txtMotor2.Text) ? 0 : Convert.ToInt32(txtMotor2.Text);

            int currentDir = 0;
            if (currentM1Speed > 0 || currentM2Speed > 0)
            {
                currentDir = rbRight.Checked ? 1 : 2;
            }

            currentM1Speed = Math.Max(0, Math.Min(255, currentM1Speed));
            currentM2Speed = Math.Max(0, Math.Min(255, currentM2Speed));

            bool m1Changed = (currentM1Speed != lastM1Speed) || (currentDir != lastDir);
            bool m2Changed = (currentM2Speed != lastM2Speed) || (currentDir != lastDir);

            if (m1Changed)
            {
                byte[] frameM1 = BuildMotorFrame(0, (byte)currentM1Speed, (byte)currentDir);
                serialPort1.Write(frameM1, 0, frameM1.Length);

                txtMesseage.AppendText("Sent M1: " + BitConverter.ToString(frameM1) + Environment.NewLine);
            }

            if (m2Changed)
            {
                byte[] frameM2 = BuildMotorFrame(1, (byte)currentM2Speed, (byte)currentDir);
                serialPort1.Write(frameM2, 0, frameM2.Length);

                txtMesseage.AppendText("Sent M2: " + BitConverter.ToString(frameM2) + Environment.NewLine);
            }

            if (!m1Changed && !m2Changed)
            {
                txtMesseage.AppendText("Nothing has changed." + Environment.NewLine);
            }

            lastM1Speed = currentM1Speed;
            lastM2Speed = currentM2Speed;
            lastDir = currentDir;
        }

        private byte[] BuildMotorFrame(byte motorId, byte speed, byte direction)
        {
            byte sof = 0xAA;
            byte eof = 0x55;
            byte addrTx = 0x01; // کامپیوتر
            byte addrRx = 0x02; // برد STM32
            byte cmd = 0x10;    // CMD_MOTOR_CONTROL
            byte len = 3;       // طول بخش داده شامل: motorId, speed, direction

            byte[] data = new byte[] { motorId, speed, direction };

            // ساخت آرایه کامل فریم طول کل: 5 بایت هدر و داده + 1 بایت چکسام + 1 بایت EOF = در مجموع 10 بایت
            byte[] frame = new byte[5 + data.Length + 2];

            frame[0] = sof;
            frame[1] = addrTx;
            frame[2] = addrRx;
            frame[3] = cmd;
            frame[4] = len;

            Array.Copy(data, 0, frame, 5, data.Length);

            byte crcValue = 0;
            for (int i = 1; i < 5 + data.Length; i++)
            {
                crcValue ^= frame[i];
            }

            frame[5 + data.Length] = crcValue;
            frame[5 + data.Length + 1] = eof;

            return frame;
        }

        private string CalculateChecksum(string text)
        {
            byte checksum = 0;
            foreach (char c in text)
            {
                checksum ^= (byte)c;
            }
            return checksum.ToString("X2"); // hex to digits
        }

        private void txtMotor1_TextChanged(object sender, EventArgs e)
        {
            if (!System.Text.RegularExpressions.Regex.IsMatch(txtMotor1.Text, "^[0-9]*$"))
            {
                MessageBox.Show("Insert the input in numbers only.");
                txtMotor1.Text = "";
            }
        }

        private void txtMotor2_TextChanged(object sender, EventArgs e)
        {
            if (!System.Text.RegularExpressions.Regex.IsMatch(txtMotor2.Text, "^[0-9]*$"))
            {
                MessageBox.Show("Insert the input in numbers only.");
                txtMotor2.Text = "";
            }
        }

        private void rbLeft_CheckedChanged(object sender, EventArgs e)
        {
            // نیازی به کد ندارد؛ چون وضعیت این رادیوباتن مستقیماً موقع زدن دکمه Send خوانده می‌شود.
        }

        private void rbRight_CheckedChanged(object sender, EventArgs e)
        {
            // نیازی به کد ندارد؛ وضعیت این هم موقع دکمه Send چک می‌شود.
        }

        private void grpDirection_Enter(object sender, EventArgs e)
        {
            // این رویداد مربوط به زمانی است که کاربر روی قاب یا کادر دور دکمه‌ها کلیک کند که هیچ کاربردی در منطق برنامه ندارد.
        }

        private void txtMesseage_TextChanged(object sender, EventArgs e)
        {
            // نیازی به کد ندارد؛ این باکس فقط برای نمایش فریم‌های ارسالی به کاربر است.
        }

        private void txtReceive_TextChanged(object sender, EventArgs e)
        {
            // نیازی به کد ندارد؛ چون وظیفه نوشتن روی این باکس بر عهده رویداد SerialPort1_DataReceived است.
        }

        private void cboPort_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (serialPort1.IsOpen)
            {
                try
                {
                    serialPort1.Close();
                    btnOpen.Enabled = true;
                    btnClose.Enabled = false;
                    txtMesseage.AppendText("Post has been changed. try again fo connection." + Environment.NewLine);
                }
                catch (Exception ex)
                {
                    MessageBox.Show("Error: " + ex.Message);
                }
            }
        }




        //private void btnSend_Click(object sender, EventArgs e)
        //{
        //    if (serialPort1.IsOpen)
        //    {
        //        string m1 = string.IsNullOrEmpty(txtMotor1.Text) ? "0" : txtMotor1.Text;
        //        string m2 = string.IsNullOrEmpty(txtMotor2.Text) ? "0" : txtMotor2.Text;
        //        string dir = rbLeft.Checked ? "L" : "R";

        //        string packet = $"{m1},{m2},{dir}\n";
        //        serialPort1.Write(packet);

        //        txtMesseage.AppendText("Sent: " + packet);
        //    }
        //}
    }
}
