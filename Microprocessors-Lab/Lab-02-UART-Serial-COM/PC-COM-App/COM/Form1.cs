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
using System.IO.Ports;

namespace COM
{
    public partial class Form1 : Form
    {
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
                // اتصال رویداد دریافت داده
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

        private void btnSend_Click(object sender, EventArgs e)
        {
            if (serialPort1.IsOpen)
            {
                // ساخت پکت ارسالی موتورها
                string m1 = string.IsNullOrEmpty(txtMotor1.Text) ? "0" : txtMotor1.Text;
                string m2 = string.IsNullOrEmpty(txtMotor2.Text) ? "0" : txtMotor2.Text;
                string dir = rbLeft.Checked ? "L" : "R";

                string packet = $"{m1},{m2},{dir}\n";
                serialPort1.Write(packet);

                txtMesseage.AppendText("Sent: " + packet);
            }
        }

        private void bntReceive_Click(object sender, EventArgs e)
        {
            if (serialPort1.IsOpen)
            {
                txtReceive.Text = serialPort1.ReadExisting();
            }
        }

        private void SerialPort1_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            string incomingData = serialPort1.ReadExisting();
            // استفاده از Invoke برای جلوگیری از کراش در Threadها
            this.Invoke(new MethodInvoker(delegate {
                txtReceive.AppendText(incomingData);
            }));
        }

        private void Form1_FormClosing(object sender, FormClosingEventArgs e)
        {
            if (serialPort1.IsOpen)
            {
                serialPort1.Close();
            }
        }
    }
}
