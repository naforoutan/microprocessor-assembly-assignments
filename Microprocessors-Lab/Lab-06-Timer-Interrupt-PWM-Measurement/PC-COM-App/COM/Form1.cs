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
using System.IO;

namespace COM
{

    public partial class Form1 : Form
    {
        private const byte ADDR_PC = 0x01;
        private const byte ADDR_STM32 = 0x02;
        private const byte ADDR_ADC_BOARD = 0x03;
        private const byte ADDR_MOTOR_BOARD = 0x04;

        private int lastM1Speed = -1;
        private int lastM2Speed = -1;
        private int lastDir = -1; // 0: Stop, 1: CW, 2: CCW


        private const byte ADDR_BROADCAST = 0xFF;
        private const byte CMD_FREQ_REPORT = 0x50;

        private readonly List<byte> _rxBuffer = new List<byte>(4096);

        private readonly StringBuilder _textRxBuffer = new StringBuilder();
        private readonly string _timeLogPath = "time_log.txt";
        private static int IndexOfByte(List<byte> buffer, byte value)
        {
            for (int i = 0; i < buffer.Count; i++)
                if (buffer[i] == value) return i;
            return -1;
        }

        private static string AddrName(byte a)
        {
            switch (a)
            {
                case 0x01: return "PC(0x01)";
                case 0x02: return "STM32(0x02)";
                case 0x03: return "ADC(0x03)";
                case 0x04: return "MOTOR(0x04)";
                default: return "UNK(0x" + a.ToString("X2") + ")";
            }
        }


        public Form1()
        {
            InitializeComponent();
            SetupAddressComboBox(); // مقداردهی اولیه لیست آدرس‌ها
        }

        private void SerialPort1_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            try
            {
                if (!serialPort1.IsOpen) return;

                int bytesToRead = serialPort1.BytesToRead;
                if (bytesToRead <= 0) return;

                byte[] buffer = new byte[bytesToRead];
                serialPort1.Read(buffer, 0, bytesToRead);

                // روی UI thread
                this.BeginInvoke(new Action(() =>
                {
                    ProcessTextMessages(buffer);

                    _rxBuffer.AddRange(buffer);

                    ExtractAndParseFrames();
                }));
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine("UART Rx Error: " + ex.Message);
            }
        }

        private void ExtractAndParseFrames()
        {
            while (true)
            {
                int sof = IndexOfByte(_rxBuffer, 0xAA);
                if (sof < 0)
                {
                    //_rxBuffer.Clear();
                    return;
                }

                if (sof > 0)
                    _rxBuffer.RemoveRange(0, sof);

                // حداقل: AA TX RX CMD LEN CRC 55 => 7
                if (_rxBuffer.Count < 7)
                    return;

                byte len = _rxBuffer[4];
                int frameLen = 1 + 4 + len + 1 + 1; // SOF + header + data + crc + eof

                if (_rxBuffer.Count < frameLen)
                    return;

                byte[] frame = _rxBuffer.GetRange(0, frameLen).ToArray();
                _rxBuffer.RemoveRange(0, frameLen);

                ParseFrame(frame);
            }
        }



        // مقداردهی ComboBox برای انتخاب مقصد
        private void SetupAddressComboBox()
        {
            var addresses = new Dictionary<string, byte>
            {
                { "STM32 (0x02)", ADDR_STM32 },
                { "ADC Board (0x03)", ADDR_ADC_BOARD },
                { "Motor Board (0x04)", ADDR_MOTOR_BOARD }
            };
            cmbDestination.DataSource = new BindingSource(addresses, null);
            cmbDestination.DisplayMember = "Key";
            cmbDestination.ValueMember = "Value";
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            // تعریف لیست مقصدها
            var destinations = new List<KeyValuePair<string, byte>>
                {
                    new KeyValuePair<string, byte>("STM32 (0x02)", 0x02),
                    new KeyValuePair<string, byte>("ADC Board (0x03)", 0x03),
                    new KeyValuePair<string, byte>("Motor Board (0x04)", 0x04),
                    new KeyValuePair<string, byte>("PC (0x01) - Loopback Test", 0x01) // برای تست بلاک کردن
                };

            cmbDestination.DataSource = destinations;
            cmbDestination.DisplayMember = "Key";
            cmbDestination.ValueMember = "Value";

            // انتخاب پیش‌فرض روی STM32
            cmbDestination.SelectedIndex = 0;
        }


        private void btnOpen_Click(object sender, EventArgs e)
        {
            try
            {
                if (cboPort.SelectedItem == null) return;

                serialPort1.PortName = cboPort.Text;
                serialPort1.BaudRate = 115200;
                serialPort1.Open();

                btnOpen.Enabled = false;
                btnClose.Enabled = true;
                txtMesseage.AppendText("Port Opened Successfully.\r\n");
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
                txtMesseage.AppendText("Port Closed.\r\n");
            }
            catch (Exception ex)
            {
                MessageBox.Show("Error closing port: " + ex.Message);
            }
        }

        private void ParseFrame(byte[] frame)
        {
            if (frame == null || frame.Length < 7)
                return;

            if (frame[0] != 0xAA)
            {
                txtReceive.AppendText("Invalid SOF\r\n");
                return;
            }

            if (frame[frame.Length - 1] != 0x55)
            {
                txtReceive.AppendText("Invalid EOF\r\n");
                return;
            }

            byte tx = frame[1];
            byte rx = frame[2];
            byte cmd = frame[3];
            byte len = frame[4];

            int expectedLen = 1 + 4 + len + 1 + 1;
            if (frame.Length != expectedLen)
            {
                txtReceive.AppendText("Invalid length. LEN=" + len +
                                      ", frame=" + frame.Length +
                                      ", expected=" + expectedLen + "\r\n");
                return;
            }

            byte crc = 0;
            for (int i = 1; i < 5 + len; i++)
            {
                crc ^= frame[i];
            }

            byte crcIn = frame[5 + len];
            if (crc != crcIn)
            {
                txtReceive.AppendText("CRC Error TX=" + AddrName(tx) +
                                      " RX=" + AddrName(rx) +
                                      " CMD=0x" + cmd.ToString("X2") +
                                      " (calc=" + crc.ToString("X2") +
                                      ", in=" + crcIn.ToString("X2") + ")\r\n");
                return;
            }

            txtReceive.AppendText("Recv <- TX=" + AddrName(tx) +
                                  " RX=" + AddrName(rx) +
                                  " CMD=0x" + cmd.ToString("X2") +
                                  " LEN=" + len + "\r\n");

            if (rx != ADDR_PC && rx != ADDR_BROADCAST)
            {
                txtReceive.AppendText("Ignored (not addressed to PC or Broadcast)\r\n");
                return;
            }
            ProcessCommand(cmd, frame);
        }
        

        private void ProcessCommand(byte cmd, byte[] frame)
        {
            switch (cmd)
            {
                case 0x40: // فرمان ADC
                    ProcessAdcValue(frame);
                    break;

                case 0x10: // پاسخ کنترل موتور
                    txtReceive.AppendText("Motor Control ACK received.\r\n");
                    break;

                case CMD_FREQ_REPORT:
                    ProcessFrequencyValue(frame);
                    break;

                default:
                    txtReceive.AppendText($"Unknown Command: 0x{cmd:X2}\r\n");
                    break;
            }
        }

        private void ProcessFrequencyValue(byte[] frame)
        {
            byte len = frame[4];

            if (len < 4)
            {
                txtReceive.AppendText("Invalid frequency frame length.\r\n");
                return;
            }

            uint frequency =
                ((uint)frame[5] << 24) |
                ((uint)frame[6] << 16) |
                ((uint)frame[7] << 8) |
                frame[8];

            txtFrequency.Text = frequency.ToString();
        }

        private void ProcessAdcValue(byte[] frame)
        {
            byte len = frame[4];
            if (len < 2) return; // باید حداقل 2 بایت دیتا داشته باشه

            // دیتا از frame[5]
            ushort adc = (ushort)((frame[5] << 8) | frame[6]);
            txtAnalogVoltage.Text = adc.ToString();
        }



        private void btnSend_Click(object sender, EventArgs e)
        {
            if (!serialPort1.IsOpen) return;

            byte addrTx = 0x01; // آدرس خودِ PC همیشه 1 است
            byte addrRx = (byte)cmbDestination.SelectedValue; // مقدار بایت مقصد را می‌گیرد

            // چک کردن Loopback: اگر فرستنده و گیرنده یکی باشند
            if (addrTx == addrRx)
            {
                txtMesseage.AppendText("[Warning]: TX and RX are identical (0x01). Transmission Blocked.\r\n");
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
                byte[] frameM1 = BuildMotorFrame(0, (byte)currentM1Speed, (byte)currentDir, addrRx);
                serialPort1.Write(frameM1, 0, frameM1.Length);
                txtMesseage.AppendText("Sent M1: " + BitConverter.ToString(frameM1) + Environment.NewLine);
            }

            if (m2Changed)
            {
                byte[] frameM2 = BuildMotorFrame(1, (byte)currentM2Speed, (byte)currentDir, addrRx);
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

        private byte[] BuildMotorFrame(byte motorId, byte speed, byte direction, byte rxAddr)
        {
            byte sof = 0xAA;
            byte eof = 0x55;
            byte addrTx = ADDR_PC;
            byte cmd = 0x10;    // CMD_MOTOR_CONTROL
            byte len = 3;       // motorId, speed, direction

            byte[] data = new byte[] { motorId, speed, direction };
            byte[] frame = new byte[5 + data.Length + 2];

            frame[0] = sof;
            frame[1] = addrTx;
            frame[2] = rxAddr;
            frame[3] = cmd;
            frame[4] = len;

            Array.Copy(data, 0, frame, 5, data.Length);

            // محاسبه CRC
            byte crcValue = 0;
            for (int i = 1; i < 5 + data.Length; i++)
            {
                crcValue ^= frame[i];
            }

            frame[5 + data.Length] = crcValue;
            frame[5 + data.Length + 1] = eof;

            return frame;
        }

        private void ProcessTextMessages(byte[] buffer)
        {
            string receivedText = Encoding.ASCII.GetString(buffer);
            _textRxBuffer.Append(receivedText);

            while (true)
            {
                string allText = _textRxBuffer.ToString();
                int newLineIndex = allText.IndexOf('\n');

                if (newLineIndex < 0)
                    break;

                string line = allText.Substring(0, newLineIndex).Trim();
                _textRxBuffer.Remove(0, newLineIndex + 1);

                if (line.StartsWith("Timer Tick:"))
                {
                    txtTimerTick.AppendText(line + Environment.NewLine);
                    File.AppendAllText(_timeLogPath, line + Environment.NewLine);
                }
            }

            if (_textRxBuffer.Length > 4096)
            {
                _textRxBuffer.Clear();
            }
        }

        private void Form1_FormClosing(object sender, FormClosingEventArgs e)
        {
            if (serialPort1.IsOpen) serialPort1.Close();
        }

        // متدهای کمکی برای کنترل ورودی
        private void txtMotor1_TextChanged(object sender, EventArgs e)
        {
            if (!System.Text.RegularExpressions.Regex.IsMatch(txtMotor1.Text, "^[0-9]*$"))
            {
                txtMotor1.Text = "";
            }
        }

        private void txtMotor2_TextChanged(object sender, EventArgs e)
        {
            if (!System.Text.RegularExpressions.Regex.IsMatch(txtMotor2.Text, "^[0-9]*$"))
            {
                txtMotor2.Text = "";
            }
        }

        private void cboPort_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (serialPort1.IsOpen)
            {
                serialPort1.Close();
                btnOpen.Enabled = true;
                btnClose.Enabled = false;
                txtMesseage.AppendText("Port changed. Re-open to connect.\r\n");
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
                        this.Invoke(new MethodInvoker(delegate
                        {
                            ProcessTextMessages(buffer);

                            _rxBuffer.AddRange(buffer);
                            ExtractAndParseFrames();
                        }));
                    }
                }
                catch (Exception ex)
                {
                    MessageBox.Show("Error reading port: " + ex.Message);
                }
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

        private void cmbDestination_SelectedIndexChanged(object sender, EventArgs e)
        {
            // این متد می‌تواند خالی باشد یا پیامی نمایش دهد
        }

        private void label7_Click(object sender, EventArgs e)
        {
            // متد موقت برای رفع ارور دیزاینر
        }

        private void comboBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            // متد موقت برای رفع ارور دیزاینر
        }

        private void txtAnalogVoltage_TextChanged(object sender, EventArgs e)
        {

        }

        private void label6_Click(object sender, EventArgs e)
        {

        }

        private void label7_Click_1(object sender, EventArgs e)
        {

        }

        private void textBox1_TextChanged(object sender, EventArgs e)
        {

        }

        private void textBox1_TextChanged_1(object sender, EventArgs e)
        {

        }

        private void label8_Click(object sender, EventArgs e)
        {

        }
    }


}
