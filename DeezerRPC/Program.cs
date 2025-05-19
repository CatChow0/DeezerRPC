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

        private const string DISCORD_CLIENT_ID = "1234921022856237196";
        private static DiscordRpcClient _rpc;

        private static GlobalSystemMediaTransportControlsSessionManager sessionManager;
        private static GlobalSystemMediaTransportControlsSession session;

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

            sessionManager = await GlobalSystemMediaTransportControlsSessionManager.RequestAsync();
            sessionManager.SessionsChanged += SessionManager_SessionsChanged;

            // Initialisation de la session Deezer si présente
            await UpdateSession();

            // Garde l'application vivante (évite la sortie du thread)
            Application.Run();
        }

        private static async void SessionManager_SessionsChanged(GlobalSystemMediaTransportControlsSessionManager sender, object args)
        {
            await UpdateSession();
        }

        private static async Task UpdateSession()
        {
            // Désabonne les anciens handlers si besoin
            if (session != null)
            {
                session.PlaybackInfoChanged -= Session_PlaybackInfoChanged;
                session.MediaPropertiesChanged -= Session_MediaPropertiesChanged;
            }

            session = sessionManager.GetCurrentSession();
            if (session?.SourceAppUserModelId?.ToLower().Contains("deezer") == true)
            {
                session.PlaybackInfoChanged += Session_PlaybackInfoChanged;
                session.MediaPropertiesChanged += Session_MediaPropertiesChanged;
                // Mise à jour immédiate
                await UpdatePresenceFromSession();
            }
            else
            {
                _rpc.ClearPresence();
            }
        }

        private static async void Session_PlaybackInfoChanged(GlobalSystemMediaTransportControlsSession sender, PlaybackInfoChangedEventArgs args)
        {
            await UpdatePresenceFromSession();
        }

        private static async void Session_MediaPropertiesChanged(GlobalSystemMediaTransportControlsSession sender, MediaPropertiesChangedEventArgs args)
        {
            await UpdatePresenceFromSession();
        }

        private static async Task UpdatePresenceFromSession()
        {
            if (session == null) return;

            var playback = session.GetPlaybackInfo();
            var props = await session.TryGetMediaPropertiesAsync();
            var timeline = session.GetTimelineProperties();

            if (props == null) return;

            bool isPaused = playback.PlaybackStatus == GlobalSystemMediaTransportControlsSessionPlaybackStatus.Paused;
            bool isPlaying = playback.PlaybackStatus == GlobalSystemMediaTransportControlsSessionPlaybackStatus.Playing;

            if (isPlaying || isPaused)
            {
                var title = props.Title;
                var artist = props.Artist;
                var pos = timeline.Position.TotalSeconds;
                var dur = timeline.EndTime.TotalSeconds;

                var start = DateTimeOffset.UtcNow.AddSeconds(-pos).UtcDateTime;
                var endTime = start.AddSeconds(dur);

                var details = $"🎵 {title}";
                string state = isPaused ? "⏸️ En pause" : (string.IsNullOrEmpty(artist) ? "Écoute en cours" : $"par {artist}");

                var presence = new RichPresence()
                {
                    Details = details,
                    State = state,
                    Timestamps = isPaused ? null : new Timestamps
                    {
                        Start = start,
                        End = endTime
                    },
                    Assets = new Assets { LargeImageKey = "default", LargeImageText = isPaused ? "En pause" : "Lecture en cours" }
                };

                _rpc.SetPresence(presence);
            }
            else
            {
                _rpc.ClearPresence();
            }
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
