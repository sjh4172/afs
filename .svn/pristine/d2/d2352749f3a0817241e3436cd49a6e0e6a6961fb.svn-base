using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Net;
using System.Net.Sockets;
using System.Text;

namespace Gui
{
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();
        }

        public static void SendOnce(string host, int port, string message)
        {
            using (UdpClient udp = new UdpClient())
            {
                byte[] data = Encoding.UTF8.GetBytes(message);
                udp.Send(data, data.Length, host, port);
            }
        }

        private void Form1_Load(object sender, EventArgs e)
        {

        }

        // INIT
        private void button4_Click(object sender, EventArgs e)
        {
            SendOnce("127.0.0.1", 50000, "INIT");
        }

        // START
        private void button1_Click(object sender, EventArgs e)
        {
            SendOnce("127.0.0.1", 50000, "START");
        }

        // STOP
        private void button2_Click(object sender, EventArgs e)
        {
            SendOnce("127.0.0.1", 50000, "STOP");
        }

    }
}
