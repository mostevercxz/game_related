using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Threading;//allow multiple client connection
using System.Net.Sockets;

namespace TCPServer
{

    public partial class Form1 : Form
    {
        private TcpListener m_listener;
        private bool m_continueListen = false;//false

        private const int m_servicePort = 12000;
        private static List<ClientHandler> m_handlers = new List<ClientHandler>();        
        private static string m_contentForTextBox = "";//the string to be shown at the textBox panel

        private Thread m_checkRecycleAndUpdateTextThread;
        private Thread m_updateTextBox;
        private Thread m_acceptClientThread;

        public Form1()
        {
            InitializeComponent();
            m_continueListen = true;
            m_checkRecycleAndUpdateTextThread = new Thread(new ThreadStart(CheckRecycleAndUpdateTextBoxString));
            m_checkRecycleAndUpdateTextThread.Start();

            m_updateTextBox = new Thread(new ThreadStart(UpdateTextBox));
            m_updateTextBox.Start();

            m_acceptClientThread = new Thread(new ThreadStart(StartAcceptClient));
            m_acceptClientThread.Start();
        }

        private void StartAcceptClient()
        {
            System.Net.IPAddress ip = System.Net.IPAddress.Parse("127.0.0.1");
            m_listener = new TcpListener(ip, m_servicePort);
            try
            {
                m_listener.Start();
                while(m_continueListen)
                {
                    TcpClient client = m_listener.AcceptTcpClient();
                    if(client != null)
                    {
                        lock(m_handlers)
                        {                            
                            ClientHandler handler = new ClientHandler(client, "the " + m_handlers.Count.ToString() + " client");
                            handler.Start();
                            m_handlers.Add(handler);
                        }
                    }
                    else
                    {
                        m_continueListen = false;
                        break;
                    }
                    Thread.Sleep(200);
                }

                for(int i = 0; i < m_handlers.Count; ++i)
                {
                    ClientHandler handler = m_handlers[i];
                    handler.Stop();
                }
            }
            catch (Exception e)
            {
                System.Windows.Forms.MessageBox.Show(e.ToString());
            }
        }

        private void UpdateTextBox()
        {
            while(true)
            {
                lock(m_contentForTextBox)
                {
                    this.SetText(m_contentForTextBox);
                }
                Thread.Sleep(50);
            }
        }

        private static void CheckRecycleAndUpdateTextBoxString()
        {
            while (true)
            {
                lock (m_handlers)
                {
                    m_contentForTextBox = "";
                    for (int i = 0; i < m_handlers.Count; ++i)
                    {
                        ClientHandler handler = m_handlers[i];
                        m_contentForTextBox += handler.Content;
                        if(!handler.Alive)
                        {
                            m_handlers.Remove(handler);
                            m_contentForTextBox += handler.Name +  " left";
                        }
                    }
                }
                Thread.Sleep(200);
            }
        }

        delegate void SetTextCallBack(string text);

        private void SetText(string text)
        {
            if(this.textBox1.InvokeRequired)
            {
                SetTextCallBack callback = SetText;
                this.Invoke(callback, new object[] {text} );
            }
            else
            {
                this.textBox1.Text = text;
            }
        }
    }

    public class ClientHandler
    {
        private TcpClient m_client;
        private Thread m_clientThread;
        private bool m_continueProcess;
        private string m_clientName;
        private string m_recvContent;

        public ClientHandler(TcpClient client, string name)
        {
            m_client = client;
            m_clientName = name;
        }

        public string Name
        {
            get
            {
                return m_clientName;
            }
        }

        public string Content
        {
            get
            {
                return m_recvContent;
            }
        }

        public void Start()
        {
            m_clientThread = new Thread(new ThreadStart(Run));
            m_continueProcess = true;
            m_clientThread.Start();
        }

        private void Run()
        {
            //1.keep receiving message from the client, and send the messagae back            
            if (m_client != null)
            {
                NetworkStream stream = m_client.GetStream();
                if (stream.CanRead && stream.CanWrite)
                {
                    string content = "";
                    byte[] contentBytes = null;
                    while (m_continueProcess)
                    {
                        contentBytes = new byte[m_client.ReceiveBufferSize];
                        stream.Read(contentBytes, 0, contentBytes.Length);
                        content = Encoding.ASCII.GetString(contentBytes);
                        m_recvContent = "receive from client " + m_clientName + content;

                        stream.Write(contentBytes, 0, contentBytes.Length);
                    }
                }
            }
        }

        public void Stop()
        {
            //do not continue process
            m_continueProcess = false;
            if (this.Alive)
            {
                m_clientThread.Join();
            }
        }

        public bool Alive
        {
            get
            {
                return (m_clientThread != null && m_clientThread.IsAlive);
            }
        }
    };
}
