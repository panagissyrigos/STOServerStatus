#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

#include "TrayIcon.h"
#include "StatusChecker.h"

class App
{
public:
    explicit App(HINSTANCE hInstance);
    int Run();

private:
    static constexpr UINT WM_TRAYICON = WM_APP + 1;
    static constexpr UINT TIMER_ID = 1;
	static constexpr UINT POLL_INTERVAL_MS = 120000;  // how often to poll the status (2 minutes)

    HINSTANCE hInstance_ = nullptr;
    HWND hwnd_ = nullptr;

    TrayIcon tray_;
    StatusChecker checker_;

    HICON iconApp_ = nullptr;
    HICON iconOnline_ = nullptr;
    HICON iconMaintenance_ = nullptr;
    HICON iconError_ = nullptr;

    HMENU menu_ = nullptr;

    ServerState lastState_ = ServerState::Unknown;

    bool InitWindow();
    bool InitResources();
    void Cleanup();

    void CheckAndUpdate(bool forceBalloon);

    void UpdateTrayForResult(const StatusResult &r, bool forceBalloon);
    HICON IconForState(ServerState s) const;

    void OpenStatusPage() const;

    bool StartupTaskExists() const;
    bool InstallStartupTask();

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    std::wstring TooltipFor(const StatusResult &r) const;
};
