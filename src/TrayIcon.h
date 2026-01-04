#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <string>

class TrayIcon
{
public:
    TrayIcon() = default;
    ~TrayIcon();

    TrayIcon(const TrayIcon &) = delete;
    TrayIcon &operator=(const TrayIcon &) = delete;

    bool Create(HWND hwnd, UINT callbackMessage, HICON icon, const std::wstring &tooltip);
    void Destroy();

    void SetIcon(HICON icon);
    void SetTooltip(const std::wstring &tooltip);

    void ShowContextMenu(HWND hwnd, const POINT &pt, HMENU menu);

private:
    NOTIFYICONDATAW nid_{};
    bool created_ = false;

    void Modify(DWORD flags);
};
