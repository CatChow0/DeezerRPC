// DeezerRPC.cpp
#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>
#include <commctrl.h>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "comctl32.lib")

const bool DEBUG = false;

#define DISCORDPP_IMPLEMENTATION
#include "discordpp.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <string>
#include <csignal>
#include <fstream>
#include <winrt/base.h>
#include <wrl.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <json.hpp>

#define WM_APP_TRAYICON (WM_APP + 1)
#define ID_TRAYICON 1
#define ID_EXIT     2
#define ID_AUTOSTART   3
#define IDI_ICON1 101
#define IDI_ICON2 102

using json = nlohmann::json;

using namespace winrt::Windows::Media::Control;
using namespace winrt::Windows::Foundation;

const uint64_t APPLICATION_ID = 1234921022856237196;
std::atomic<bool> running = true;
bool showConsole = DEBUG;
HWND g_hwnd = NULL;
NOTIFYICONDATA nid = { 0 };

void signalHandler(int signum) {
    running.store(false);
}

void DebugLog(const std::string& message) {
    if (DEBUG) {
        std::cout << message << std::endl;
    }
}

template<typename T>
void DebugLog(const std::string& prefix, const T& value) {
    if (DEBUG) {
        std::cout << prefix << value << std::endl;
    }
}

void DebugError(const std::string& message) {
    if (DEBUG) {
        std::cerr << message << std::endl;
    }
}

std::string UrlEncode(const std::string& str) {
    std::string encoded;
    for (unsigned char c : str) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += c;
        } else if (c == ' ') {
            encoded += '+';
        } else {
            char hex[3];
            sprintf_s(hex, "%02X", c);
            encoded += '%' + std::string(hex);
        }
    }
    return encoded;
}

std::string HttpGet(const std::wstring& host, const std::wstring& path) {
    std::string response;

    HINTERNET hSession = WinHttpOpen(L"DeezerRPC/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return response;

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return response;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                           NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return response;
    }

    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(hRequest, NULL)) {

        unsigned long bytesAvailable = 0;

        do {
            bytesAvailable = 0;
            WinHttpQueryDataAvailable(hRequest, &bytesAvailable);

            if (bytesAvailable > 0) {
                std::vector<char> buffer(bytesAvailable + 1);
                unsigned long bytesRead = 0;

                if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
                    buffer[bytesRead] = '\0';
                    response += buffer.data();
                }
            }
        } while (bytesAvailable > 0);
        }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return response;
}

std::string GetDeezerAlbumArtUrl(const std::string& title, const std::string& artist, const std::string& album = "") {
    try {
        std::string query = UrlEncode(title + " " + artist);
        if (!album.empty()) {
            query += " " + UrlEncode(album);
        }

        std::wstring host = L"api.deezer.com";
        std::wstring path = L"/search?q=" + std::wstring(query.begin(), query.end());

        std::string response = HttpGet(host, path);
        if (response.empty()) return "";

        auto j = json::parse(response);

        if (j.contains("data") && !j["data"].empty()) {
            auto firstMatch = j["data"][0];
            if (firstMatch.contains("album") && firstMatch["album"].contains("id")) {
                int albumId = firstMatch["album"]["id"];
                DebugLog("Trouvé album ID: ", albumId);

                return "https://api.deezer.com/album/" + std::to_string(albumId) + "/image?size=big";
            }
        }
    }
    catch (const std::exception& e) {
        DebugError("Erreur lors de la recuperation de la pochette: " + std::string(e.what()));
    }

    return "";
}

int GetDeezerArtistId(const std::string& artist) {
    try {
        std::string query = UrlEncode(artist);
        std::wstring host = L"api.deezer.com";
        std::wstring path = L"/search/artist?q=" + std::wstring(query.begin(), query.end());

        std::string response = HttpGet(host, path);
        if (response.empty()) return 0;

        auto j = json::parse(response);
        if (j.contains("data") && !j["data"].empty()) {
            auto firstMatch = j["data"][0];
            if (firstMatch.contains("id")) {
                int artistId = firstMatch["id"];
                DebugLog("Trouve artist ID: ", artistId);
                return artistId;
            }
        }
    }
    catch (const std::exception& e) {
        DebugError("Erreur lors de l image de l artiste : " + std::string(e.what()));
    }
    return 0;
}

std::string GetDeezerArtistImageUrl(const std::string& artist) {
    int artistId = GetDeezerArtistId(artist);
    if (artistId > 0) {
        return "https://api.deezer.com/artist/" + std::to_string(artistId) + "/image";
    }
    return "";
}

GlobalSystemMediaTransportControlsSession GetDeezerSession(GlobalSystemMediaTransportControlsSessionManager const& manager) {
    auto sessions = manager.GetSessions();
    for (uint32_t i = 0; i < sessions.Size(); ++i) {
        auto session = sessions.GetAt(i);
        auto id = session.SourceAppUserModelId();
        if (!id.empty() && std::wstring(id).find(L"deezer") != std::wstring::npos) {
            return session;
        }
    }
    return nullptr;
}

void DisableAutoStart() {
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValue(hKey, L"DeezerRPC");
        RegCloseKey(hKey);
    }
}

void EnableAutoStart() {
    WCHAR path[MAX_PATH];
    GetModuleFileName(NULL, path, MAX_PATH);

    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegSetValueEx(hKey, L"DeezerRPC", 0, REG_SZ, (BYTE*)path, (wcslen(path) + 1) * sizeof(WCHAR));
        RegCloseKey(hKey);
    }
}

void InitTrayIcon(HWND hwnd) {
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = ID_TRAYICON;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_APP_TRAYICON;

    // Charger l'icône personnalisée
    nid.hIcon = (HICON)LoadImage(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDI_ICON2),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), // 16x16 généralement
        GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR
    );

    
    // Utiliser l'icône par défaut si échec du chargement
    if (nid.hIcon == NULL) {
        nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }
    
    wcscpy_s(nid.szTip, L"Deezer Rich Presence");

    Shell_NotifyIcon(NIM_ADD, &nid);
}

bool IsAutoStartEnabled() {
    HKEY hKey;
    bool enabled = false;

    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        WCHAR path[MAX_PATH] = {0};
        DWORD pathSize = sizeof(path);

        if (RegQueryValueEx(hKey, L"DeezerRPC", NULL, NULL, (LPBYTE)path, &pathSize) == ERROR_SUCCESS) {
            enabled = true;
        }

        RegCloseKey(hKey);
    }

    return enabled;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_APP_TRAYICON:
        if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);

            bool autoStartEnabled = IsAutoStartEnabled();

            HMENU hMenu = CreatePopupMenu();
            AppendMenu(hMenu, MF_STRING, ID_AUTOSTART,
                       autoStartEnabled ? L"Disabled start on Launch" : L"Enabled start on Launch");
            AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenu(hMenu, MF_STRING, ID_EXIT, L"Exit");

            SetForegroundWindow(hwnd);
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_AUTOSTART:
            if (IsAutoStartEnabled()) {
                DisableAutoStart();
                DebugLog("Demarrage automatique desactive");
            } else {
                EnableAutoStart();
                DebugLog("Demarrage automatique active");
            }
            return 0;

        case ID_EXIT:
            running = false;
            PostQuitMessage(0);
            return 0;
        }
        break;

    case WM_DESTROY:
        Shell_NotifyIcon(NIM_DELETE, &nid);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

HWND CreateMessageWindow() {
    WNDCLASSEX wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.lpfnWndProc = WindowProc;
    wcex.hInstance = GetModuleHandle(NULL);
    wcex.lpszClassName = L"DeezerRPCClass";

    RegisterClassEx(&wcex);

    HWND hwnd = CreateWindowEx(
        0, L"DeezerRPCClass", L"DeezerRPC",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        400, 300, NULL, NULL, GetModuleHandle(NULL), NULL);

    return hwnd;
}

// Remplace main() par WinMain() pour une application Windows native
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Creer une console uniquement si DEBUG est active
    if (DEBUG) {
        AllocConsole();
        FILE* pConsole;
        freopen_s(&pConsole, "CONOUT$", "w", stdout);
        freopen_s(&pConsole, "CONOUT$", "w", stderr);
    }

    // Initialiser les contrôles communs (necessaire pour les menus)
    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icex);

    // Creer la fenêtre invisible et l icône du systray
    g_hwnd = CreateMessageWindow();
    InitTrayIcon(g_hwnd);

    winrt::init_apartment();
    std::signal(SIGINT, signalHandler);

    DebugLog("🚀 Initialisation Discord SDK...");
    auto client = std::make_shared<discordpp::Client>();
    client->SetApplicationId(APPLICATION_ID);

    client->AddLogCallback([](auto message, auto severity) {
        if (DEBUG) {
            std::cout << "[" << EnumToString(severity) << "] " << message << std::endl;
        }
    }, discordpp::LoggingSeverity::Info);

    client->SetStatusChangedCallback([client](discordpp::Client::Status status, discordpp::Client::Error error, int32_t errorDetail) {
        DebugLog("🔄 Status changed: " + discordpp::Client::StatusToString(status));
        if (status == discordpp::Client::Status::Ready) {
            DebugLog("✅ Client prêt !");
        }
    });

    // Initialisation du contrôleur media Windows
    GlobalSystemMediaTransportControlsSessionManager manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();

    MSG msg;
    while (running) {
        // Traiter les messages Windows sans bloquer
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            if (msg.message == WM_QUIT) {
                running = false;
                break;
            }
        }

        auto session = GetDeezerSession(manager);
        if (session) {
            auto playback = session.GetPlaybackInfo();
            auto props = session.TryGetMediaPropertiesAsync().get();
            auto timeline = session.GetTimelineProperties();

            bool isPaused = playback.PlaybackStatus() == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Paused;
            bool isPlaying = playback.PlaybackStatus() == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;

            if (isPlaying || isPaused) {
                std::string title_utf8 = winrt::to_string(props.Title());
                std::string artist_utf8 = winrt::to_string(props.Artist());
                std::string album_utf8 = winrt::to_string(props.AlbumTitle());
                double pos = timeline.Position().count() / 1e7;
                double dur = (timeline.EndTime() - timeline.StartTime()).count() / 1e7;

                auto start = std::chrono::system_clock::now() - std::chrono::seconds((int)pos);
                auto end = start + std::chrono::seconds((int)dur);

                // Recuperer l URL de la pochette Deezer
                std::string albumArtUrl = GetDeezerAlbumArtUrl(title_utf8, artist_utf8, album_utf8);
                std::string artistImageUrl = GetDeezerArtistImageUrl(artist_utf8);

                discordpp::Activity activity;
                activity.SetType(discordpp::ActivityTypes::Playing);
                activity.SetState(isPaused ? "⏸️ En pause" : ("par " + artist_utf8));
                activity.SetDetails("🎵 " + title_utf8);

                // Ajouter les assets d image
                discordpp::ActivityAssets assets;
                if (!albumArtUrl.empty()) {
                    assets.SetLargeImage(albumArtUrl.c_str());
                    assets.SetLargeText(title_utf8 + " - " + artist_utf8);
                } else {
                    // Image par defaut si la pochette n est pas trouvee
                    assets.SetLargeImage("deezer_logo");
                    assets.SetLargeText("Deezer Music");
                }

                // Ajouter une petite icône pour l etat de lecture
                if (!artistImageUrl.empty()) {
                    assets.SetSmallImage(artistImageUrl.c_str());
                    assets.SetSmallText(artist_utf8);
                } else {
                    // Fallback aux icônes statiques
                    assets.SetSmallImage(isPaused ? "paused_icon" : "playing_icon");
                    assets.SetSmallText(isPaused ? "En pause" : "En lecture");
                }

                activity.SetAssets(assets);

                if (!isPaused) {
                    discordpp::ActivityTimestamps timestamps;
                    timestamps.SetStart(std::chrono::duration_cast<std::chrono::seconds>(start.time_since_epoch()).count());
                    timestamps.SetEnd(std::chrono::duration_cast<std::chrono::seconds>(end.time_since_epoch()).count());
                    activity.SetTimestamps(timestamps);
                }

                client->UpdateRichPresence(activity, [](discordpp::ClientResult result) {
                    if (result.Successful()) {
                        DebugLog("🎮 Rich Presence mise a jour !");
                    }
                });
            } else {
                client->ClearRichPresence();
            }
        } else {
            client->ClearRichPresence();
        }

        discordpp::RunCallbacks();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // Nettoyer l icône du systray avant de quitter
    Shell_NotifyIcon(NIM_DELETE, &nid);

    return 0;
}