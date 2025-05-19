using DiscordRPC;
using DiscordRPC.Logging;
using System.Runtime.InteropServices;
using Windows.Media.Control;
using System.Windows.Forms;
using Microsoft.Win32;

namespace DeezerPresence
{
    static class Program
    {
        private const bool DEBUG = false; // Mettre à true pour activer le mode debug

        // Votre client ID Discord
        private const string DISCORD_CLIENT_ID = "1234921022856237196";
        private static DiscordRpcClient _rpc;

        [STAThread]
        static void Main()
        {
            if (DEBUG)
            {
                AllocConsole();
                Console.WriteLine("Mode debug activé.");
                RunPresenceLogic();
            }
            else
            {
                Application.EnableVisualStyles();
                Application.SetCompatibleTextRenderingDefault(false);
                Application.Run(new SystrayForm());
            }
        }

        [DllImport("kernel32.dll")]
        private static extern bool AllocConsole();

        public static async void RunPresenceLogic()
        {
            _rpc = new DiscordRpcClient(DISCORD_CLIENT_ID);
            _rpc.Logger = new ConsoleLogger() { Level = LogLevel.Warning };
            _rpc.Initialize();

            ulong lastPosition = 0;
            string lastTitle = null;

            while (true)
            {
                try
                {
                    var (title, artist, pos, dur) = await GetMediaInfo();
                    if (title != null && (title != lastTitle || Math.Abs((long)pos - (long)lastPosition) > 5))
                    {
                        await UpdatePresence(title, artist, pos, dur);
                        lastTitle = title;
                        lastPosition = (ulong)pos;
                    }
                }
                catch (Exception ex)
                {
                    Console.Error.WriteLine($"Erreur boucle principale : {ex}");
                }

                await Task.Delay(1000);
            }
        }

        private static async Task<(string title, string artist, double pos, double dur)> GetMediaInfo()
        {
            var sessions = await GlobalSystemMediaTransportControlsSessionManager.RequestAsync();
            var session = sessions.GetCurrentSession();
            if (session?.SourceAppUserModelId?.ToLower().Contains("deezer") == true)
            {
                var playback = session.GetPlaybackInfo();
                if (playback.PlaybackStatus == GlobalSystemMediaTransportControlsSessionPlaybackStatus.Playing)
                {
                    var props = await session.TryGetMediaPropertiesAsync();
                    var timeline = session.GetTimelineProperties();
                    return (props.Title, props.Artist,
                        timeline.Position.TotalSeconds,
                        timeline.EndTime.TotalSeconds);
                }
            }
            return (null, null, 0, 0);
        }

        private static async Task UpdatePresence(string title, string artist, double pos, double dur)
        {
            var start = DateTimeOffset.UtcNow.AddSeconds(-pos).UtcDateTime;
            var endTime = start.AddSeconds(dur);

            var details = $"🎵 {title}";
            var state = string.IsNullOrEmpty(artist) ? "Écoute en cours" : $"par {artist}";

            var presence = new RichPresence()
            {
                Details = details,
                State = state,
                Timestamps = new Timestamps
                {
                    Start = start,
                    End = endTime
                },
                Assets = new Assets { LargeImageKey = "default", LargeImageText = "Lecture en cours" }
            };

            _rpc.SetPresence(presence);
        }
    }

    public class SystrayForm : Form
    {
        private NotifyIcon notifyIcon;
        private ToolStripMenuItem startupItem;
        private const string RUN_REGISTRY_KEY = @"SOFTWARE\Microsoft\Windows\CurrentVersion\Run";
        private const string APP_NAME = "DeezerPresence";

        public SystrayForm()
        {
            notifyIcon = new NotifyIcon
            {
                Icon = System.Drawing.Icon.ExtractAssociatedIcon(Application.ExecutablePath),
                Visible = true,
                Text = "DeezerPresence"
            };

            var contextMenu = new ContextMenuStrip();

            // Option "Lancer au démarrage"
            startupItem = new ToolStripMenuItem("Lancer au démarrage")
            {
                Checked = IsStartupEnabled()
            };
            startupItem.Click += (s, e) =>
            {
                if (startupItem.Checked)
                {
                    DisableStartup();
                    startupItem.Checked = false;
                }
                else
                {
                    EnableStartup();
                    startupItem.Checked = true;
                }
            };
            contextMenu.Items.Add(startupItem);

            var exitItem = new ToolStripMenuItem("Quitter", null, (s, e) => Application.Exit());
            contextMenu.Items.Add(exitItem);

            notifyIcon.ContextMenuStrip = contextMenu;

            this.WindowState = FormWindowState.Minimized;
            this.ShowInTaskbar = false;
            this.Load += (s, e) => this.Hide();

            // Lancer la logique métier en tâche de fond
            Task.Run(() => Program.RunPresenceLogic());
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            notifyIcon.Visible = false;
            base.OnFormClosing(e);
        }

        private bool IsStartupEnabled()
        {
            using var key = Registry.CurrentUser.OpenSubKey(RUN_REGISTRY_KEY, false);
            return key?.GetValue(APP_NAME) is string value && value == Application.ExecutablePath;
        }

        private void EnableStartup()
        {
            using var key = Registry.CurrentUser.OpenSubKey(RUN_REGISTRY_KEY, true);
            key?.SetValue(APP_NAME, Application.ExecutablePath);
        }

        private void DisableStartup()
        {
            using var key = Registry.CurrentUser.OpenSubKey(RUN_REGISTRY_KEY, true);
            key?.DeleteValue(APP_NAME, false);
        }
    }

    // Classes pour la désérialisation JSON Deezer
    public class DeezerSearch
    {
        public DeezerTrack[] data { get; set; }
    }
    public class DeezerTrack
    {
        public string link { get; set; }
    }
}
