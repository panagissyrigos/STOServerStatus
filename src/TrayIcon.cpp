#include "TrayIcon.h"

TrayIcon::~TrayIcon()
{
    Destroy();
}

bool TrayIcon::Create(HWND hwnd, UINT callbackMessage, HICON icon, const std::wstring &tooltip)
{
    Destroy();

    nid_ = {};
    nid_.cbSize = sizeof(nid_);
    nid_.hWnd = hwnd;
    nid_.uID = 1;
    nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid_.uCallbackMessage = callbackMessage;
    nid_.hIcon = icon;

    wcsncpy_s(nid_.szTip, tooltip.c_str(), _TRUNCATE);

    if (!Shell_NotifyIconW(NIM_ADD, &nid_))
    {
        created_ = false;
        return false;
    }

    created_ = true;
    return true;
}

void TrayIcon::Destroy()
{
    if (!created_)
        return;
    Shell_NotifyIconW(NIM_DELETE, &nid_);
    created_ = false;
}

void TrayIcon::Modify(DWORD flags)
{
    if (!created_)
        return;
    nid_.uFlags = flags;
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

void TrayIcon::SetIcon(HICON icon)
{
    nid_.hIcon = icon;
    Modify(NIF_ICON);
}

void TrayIcon::SetTooltip(const std::wstring &tooltip)
{
    wcsncpy_s(nid_.szTip, tooltip.c_str(), _TRUNCATE);
    Modify(NIF_TIP);
}

void TrayIcon::ShowContextMenu(HWND hwnd, const POINT &pt, HMENU menu)
{
    // Required so the menu closes correctly when you click elsewhere.
    SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, nullptr);
    PostMessageW(hwnd, WM_NULL, 0, 0);
}
