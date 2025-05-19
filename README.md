# Deezer Discord Rich Presence

![Systray Icon](https://via.placeholder.com/16)

**Deezer Discord Rich Presence** is a standalone Windows application that displays your current Deezer playback status as a Discord Rich Presence. The app runs in the background and sits in the Windows notification area (system tray).

---

## 🎵 Features

* Shows the title and artist of the currently playing track on Deezer.
* Automatically updates your Discord Rich Presence.
* Standalone executable (`.exe`), no external dependencies required.
* Icon in the Windows system tray for quick access.

---

## ⚙️ Requirements

* Windows 10 or later (64-bit recommended)
* No .NET runtime installation needed (self-contained app)
* Discord running and logged in
* Deezer Desktop or the native Deezer app open

---

## 🚀 Installation

1. Download the latest **DeezerPresence.exe** from the [Releases](#) section of the project.
2. Place `DeezerPresence.exe` in a folder of your choice.
3. (Optional) Create a desktop shortcut or add it to your Startup folder if you want the app to launch on Windows start.

---

## ▶️ Usage

1. Double-click `DeezerPresence.exe`.
2. The app will start, and an icon will appear in the system tray: ![Systray](https://via.placeholder.com/16).
3. Open Deezer Desktop and play a track.
4. Your Discord profile will automatically update with:

   * The track title
   * The artist
5. To quit, right-click the tray icon and select **Exit**.

---

## 🔧 Configuration

The `Discord Client ID` is compiled into the executable. To change it:

1. Clone the C# project.
2. Open `Program.cs` and modify the `DISCORD_CLIENT_ID` constant.
3. Rebuild with the command:

   ```bash
   dotnet publish -c Release -r win-x64 --self-contained true /p:PublishSingleFile=true
   ```

---

## 🐞 Troubleshooting

* **No status appears**

  * Ensure Discord is running and not in "Offline" mode.
  * Make sure Deezer Desktop is open and a track is playing.

* **App crashes on startup**

  * Confirm you downloaded the **win-x64** version for 64-bit Windows.
  * Verify you did not enable "Trim unused code" if you rebuilt the app.

* **No tray icon**

  * Click the arrow icon to show hidden icons in the notification area.

---

## 🤝 Contributing

Contributions are welcome! To propose improvements or fix bugs:

1. Fork the repository.
2. Create a branch named `feature/your-feature` or `fix/your-bug`.
3. Commit your changes (`git commit -m "Add your feature"`).
4. Push to your branch (`git push origin feature/your-feature`).
5. Open a Pull Request.

---

## 📄 License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

---

*Built with ❤️ for Deezer and Discord Rich Presence enthusiasts.*
