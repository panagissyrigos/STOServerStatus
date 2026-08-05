#include "App.h"
#include "resource.h"
#include "crypto.h"
#include <shellapi.h>
#include <windows.h>
#include <taskschd.h>
#include <comdef.h>
#include <atlbase.h>

#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comsuppw.lib")

using namespace std;

namespace
{
    constexpr wchar_t DEFAULT_STATUS_URL[] = L"/+rC0jh59WFJ8r5AlmPbdrhMnXotpeYf+q+onwpf/8Xh/ASWAY1YWEknvTywI9LEb4C+Rk5WlxC7PyP3LhbBhrjKE4YDZdOpqD3gvrW0Wa7PeGDA5x1mceqpzuIKG1W9ZQetGkwOxEgbeNN1o+F/943W7zVjNivK4i/c/VLL9NA=";
    constexpr wchar_t DEFAULT_STATUS_PAGE[] = L"https://www.arcgames.com/en/games/star-trek-online/news";
    static constexpr wchar_t TASK_NAME[] = L"STO Status Tray";
}

App::App(HINSTANCE hInstance)
    : hInstance_(hInstance),
      checker_(Crypto::DecryptString(L"STOSEKRIT",DEFAULT_STATUS_URL)) {}

int App::Run()
{
    if (!InitWindow())
        return 10;

    if (!InitResources())
        return 20;

    // Initial tray icon.
    if (!tray_.Create(hwnd_, WM_TRAYICON, iconApp_, L"STO Status: starting..."))
        return 30;

	// Initialize COM. for the Schedule task functionality.
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
    {
        MessageBoxW(nullptr, L"CoInitializeEx failed", L"COM Error", MB_ICONERROR);
        return 40;
    }

    hr = CoInitializeSecurity(NULL,-1,NULL,NULL,RPC_C_AUTHN_LEVEL_PKT_PRIVACY,RPC_C_IMP_LEVEL_IMPERSONATE,NULL,0,NULL);
    if (FAILED(hr))
    {
        MessageBoxW(nullptr, L"CoInitializeSecurity failed", L"COM Error", MB_ICONERROR);
        return 50;
    }

    // Start polling.
    SetTimer(hwnd_, TIMER_ID, POLL_INTERVAL_MS, nullptr);

    // First check immediately.
    CheckAndUpdate(false);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    Cleanup();
    return static_cast<int>(msg.wParam);
}

bool App::InitWindow()
{
    const wchar_t CLASS_NAME[] = L"STOStatusTrayHiddenWindow";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &App::WndProc;
    wc.hInstance = hInstance_;
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClassExW(&wc))
        return false;

    hwnd_ = CreateWindowExW(
        0,
        CLASS_NAME,
        L"STO Status Tray",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 300, 200,
        nullptr,
        nullptr,
        hInstance_,
        this
    );

    if (!hwnd_) {
        DWORD err = GetLastError();
        wchar_t buf[256];
        wsprintfW(buf, L"CreateWindowExW failed. GetLastError = %lu", err);
        MessageBoxW(nullptr, buf, L"FATAL", MB_ICONERROR);
        return false;
    }


    return hwnd_ != nullptr;
}

bool App::InitResources()
{
    iconApp_ = reinterpret_cast<HICON>(LoadImageW(hInstance_, MAKEINTRESOURCEW(IDI_APP), ICON_BIG, 0, 0, LR_DEFAULTSIZE));
    iconOnline_ = reinterpret_cast<HICON>(LoadImageW(hInstance_, MAKEINTRESOURCEW(IDI_ONLINE), ICON_BIG, 0, 0, LR_DEFAULTSIZE));
    iconMaintenance_ = reinterpret_cast<HICON>(LoadImageW(hInstance_, MAKEINTRESOURCEW(IDI_MAINTENANCE), ICON_BIG, 0, 0, LR_DEFAULTSIZE));
    iconError_ = reinterpret_cast<HICON>(LoadImageW(hInstance_, MAKEINTRESOURCEW(IDI_ICONERROR), ICON_BIG, 0, 0, LR_DEFAULTSIZE));

    if (!iconApp_ || !iconOnline_ || !iconMaintenance_ || !iconError_)
        return false;

    menu_ = CreatePopupMenu();
    if (!menu_)
        return false;

    std::wstring checkNowText =
        std::wstring(L"Check now (") +
        Crypto::DecryptString(L"STOSEKRIT", DEFAULT_STATUS_URL) +
        L")";


    AppendMenuW(menu_, MF_STRING, IDM_OPEN_STATUS_PAGE, L"Open status page");
    AppendMenuW(menu_, MF_STRING, IDM_CHECK_NOW, checkNowText.c_str());
    AppendMenuW(menu_, MF_STRING, IDM_STARTUP_TASK, L"Run at logon");
    AppendMenuW(menu_, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu_, MF_STRING, IDM_EXIT, L"Exit");

    return true;
}

void App::Cleanup()
{
    tray_.Destroy();

    if (menu_)
        DestroyMenu(menu_);
    menu_ = nullptr;
}

void App::CheckAndUpdate(bool forceBalloon)
{
    StatusResult r = checker_.CheckOnce();
    UpdateTrayForResult(r, forceBalloon);
}

HICON App::IconForState(ServerState s) const
{
    switch (s)
    {
    case ServerState::Online:
        return iconOnline_;
    case ServerState::Maintenance:
        return iconMaintenance_;
    case ServerState::Unknown:
    default:
        return iconError_;
    }
}

std::wstring App::TooltipFor(const StatusResult &r) const
{
    switch (r.state)
    {
    case ServerState::Online:
        return L"STO Servers: Online";
    case ServerState::Maintenance:
        return L"STO Servers: Maintenance / Offline";
    case ServerState::Unknown:
    default:
        return L"STO Status: Error / Unknown";
    }
}

void App::UpdateTrayForResult(const StatusResult &r, bool /*forceBalloon*/)
{
    if (r.state != lastState_)
    {
        tray_.SetIcon(IconForState(r.state));
        lastState_ = r.state;
    }
    tray_.SetTooltip(TooltipFor(r));
}

void App::OpenStatusPage() const
{
    ShellExecuteW(nullptr, L"open", DEFAULT_STATUS_PAGE, nullptr, nullptr, SW_SHOWNORMAL);
}

void ShowErrorMessage(_In_opt_ LPCSTR lpOutputString) {
	MessageBoxA(nullptr, lpOutputString, "STO Status Tray - Error", MB_OK | MB_ICONERROR);
}

bool App::InstallStartupTask()
{
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    // get the username
    wchar_t userName[256];
    DWORD size = _countof(userName);
    GetUserNameW(userName, &size);

    ITaskService* pService = NULL;
    HRESULT hr = CoCreateInstance(CLSID_TaskScheduler,
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_ITaskService,
        (void**)&pService);

    if (FAILED(hr))
    {
        ShowErrorMessage("Failed to create an instance of ITaskService: "+ hr);
        CoUninitialize();
        return false;
    }

    //  Connect to the task service.
    hr = pService->Connect(_variant_t(), _variant_t(),_variant_t(), _variant_t());
    if (FAILED(hr))
    {
        ShowErrorMessage("ITaskService::Connect failed: " +  hr);
        pService->Release();
        CoUninitialize();
        return false;
    }

    ITaskFolder* pRootFolder = NULL;
    hr = pService->GetFolder(_bstr_t(L"\\"), &pRootFolder);
    if (FAILED(hr))
    {
        ShowErrorMessage("Cannot get Root Folder pointer: " + hr);
        pService->Release();
        CoUninitialize();
        return false;
    }
        
    ITaskDefinition* pTask = NULL;
    hr = pService->NewTask(0, &pTask);
    
    pService->Release();  // COM clean up.  Pointer is no longer used.
    if (FAILED(hr))
    {
        ShowErrorMessage("Failed to create a task definition: " + hr);
        pRootFolder->Release();
        CoUninitialize();
        return false;
    }
    
    IRegistrationInfo* pRegInfo = NULL;
    hr = pTask->get_RegistrationInfo(&pRegInfo);
    if (FAILED(hr))
    {
        ShowErrorMessage("Cannot get identification pointer: " + hr);
        pRootFolder->Release();
        pTask->Release();
        CoUninitialize();
        return false;
    }
    
    hr = pRegInfo->put_Author(userName);
    pRegInfo->Release();
    if (FAILED(hr))
    {
        ShowErrorMessage("Cannot put identification info: " + hr);
        pRootFolder->Release();
        pTask->Release();
        CoUninitialize();
        return false;
    }
    
    ITaskSettings* pSettings = NULL;
    hr = pTask->get_Settings(&pSettings);
    if (FAILED(hr))
    {
        ShowErrorMessage("Cannot get settings pointer: " + hr);
        pRootFolder->Release();
        pTask->Release();
        CoUninitialize();
        return false;
    }
    
    hr = pSettings->put_StartWhenAvailable(VARIANT_TRUE);
    pSettings->Release();
    if (FAILED(hr))
    {
        ShowErrorMessage("Cannot put setting info: " + hr);
        pRootFolder->Release();
        pTask->Release();
        return false;
    }
    
    ITriggerCollection* pTriggerCollection = NULL;
    hr = pTask->get_Triggers(&pTriggerCollection);
    if (FAILED(hr))
    {
        ShowErrorMessage("Cannot get trigger collection: " + hr);
        pRootFolder->Release();
        pTask->Release();
        return false;
    }
    
    //  Add the logon trigger to the task.
    ITrigger* pTrigger = NULL;
    hr = pTriggerCollection->Create(TASK_TRIGGER_LOGON, &pTrigger);
    pTriggerCollection->Release();
    if (FAILED(hr))
    {
        ShowErrorMessage("Cannot create the trigger: " + hr);
        pRootFolder->Release();
        pTask->Release();
        return false;
    }
    
    ILogonTrigger* pLogonTrigger = NULL;
    hr = pTrigger->QueryInterface(
        IID_ILogonTrigger, (void**)&pLogonTrigger);
    pTrigger->Release();
    if (FAILED(hr))
    {
        ShowErrorMessage("QueryInterface call failed for ILogonTrigger: " + hr);
        pRootFolder->Release();
        pTask->Release();
        return false;
    }
    
    hr = pLogonTrigger->put_Id(_bstr_t(L"Trigger1"));
    if (FAILED(hr))
        ShowErrorMessage("Cannot put the trigger ID: " +  hr);
        
    //  Define the user.  The task will execute when the user logs on.
    hr = pLogonTrigger->put_UserId(_bstr_t(userName));
    pLogonTrigger->Release();
    if (FAILED(hr))
    {
        ShowErrorMessage("Cannot add user ID to logon trigger: " + hr);
        pRootFolder->Release();
        pTask->Release();
        return false;
    }
    
    IActionCollection* pActionCollection = NULL;    
    hr = pTask->get_Actions(&pActionCollection);
    if (FAILED(hr))
    {
        ShowErrorMessage("Cannot get Task collection pointer: " + hr);
        pRootFolder->Release();
        pTask->Release();
        return false;
    }
    
    //  Create the action, specifying that it is an executable action.
    IAction* pAction = NULL;
    hr = pActionCollection->Create(TASK_ACTION_EXEC, &pAction);
    pActionCollection->Release();
    if (FAILED(hr))
    {
        ShowErrorMessage("Cannot create the action: " + hr);
        pRootFolder->Release();
        pTask->Release();
        return false;
    }
    
    IExecAction* pExecAction = NULL;
    //  QI for the executable task pointer.
    hr = pAction->QueryInterface(IID_IExecAction, (void**)&pExecAction);
    pAction->Release();
    if (FAILED(hr))
    {
        ShowErrorMessage("QueryInterface call failed for IExecAction: " + hr);
        pRootFolder->Release();
        pTask->Release();
        return false;
    }
    
    //  Set the path of the executable to notepad.exe.
    hr = pExecAction->put_Path(_bstr_t(exePath));
    pExecAction->Release();
    if (FAILED(hr))
    {
        ShowErrorMessage("Cannot set path of executable: " + hr);
        pRootFolder->Release();
        pTask->Release();
        return false;
    }
    
    //  ------------------------------------------------------
    //  Save the task in the root folder.
    IRegisteredTask* pRegisteredTask = NULL;
    
    hr = pRootFolder->RegisterTaskDefinition(
        _bstr_t(TASK_NAME),
        pTask,
        TASK_CREATE_OR_UPDATE,
        _variant_t(),
        _variant_t(),
        TASK_LOGON_INTERACTIVE_TOKEN,
        _variant_t(L""),
        &pRegisteredTask);
    if (FAILED(hr))
    {
        pRootFolder->Release();
        pTask->Release();
        return false;
    }
        
    // Clean up
    pRootFolder->Release();
    pTask->Release();
    pRegisteredTask->Release();
    return true;
}



bool App::StartupTaskExists() const
{
    CComPtr<ITaskService> service;
    if (FAILED(service.CoCreateInstance(CLSID_TaskScheduler)))
        return false;

    if (FAILED(service->Connect(_variant_t(), _variant_t(),
        _variant_t(), _variant_t())))
        return false;

    CComPtr<ITaskFolder> root;
    if (FAILED(service->GetFolder(_bstr_t(L"\\"), &root)))
        return false;

    CComPtr<IRegisteredTask> task;
    HRESULT hr = root->GetTask(_bstr_t(TASK_NAME), &task);

    return SUCCEEDED(hr);
}



LRESULT CALLBACK App::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    App *app = nullptr;

    if (msg == WM_NCCREATE)
    {
        auto *cs = reinterpret_cast<CREATESTRUCTW *>(lParam);
        app = reinterpret_cast<App *>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        return true;
    }
    else
    {
        app = reinterpret_cast<App *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (app)
        return app->HandleMessage(msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT App::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_TIMER:
        if (wParam == TIMER_ID)
        {
            CheckAndUpdate(false);
            return 0;
        }
        break;

    case WM_COMMAND:
    {
        const UINT id = LOWORD(wParam);
        switch (id)
        {
        case IDM_OPEN_STATUS_PAGE:
            OpenStatusPage();
            return 0;
        case IDM_STARTUP_TASK:
        {
            if (!StartupTaskExists())
                InstallStartupTask();
            return 0;
        }

        case IDM_CHECK_NOW:
            CheckAndUpdate(false);
            return 0;
        case IDM_EXIT:
            CoUninitialize();
            PostQuitMessage(0);
            return 0;
        }
        break;
    }

    case WM_TRAYICON:
    {
        // lParam holds the mouse message.
        if (lParam == WM_RBUTTONUP)
        {
            bool exists = StartupTaskExists();

            CheckMenuItem(
                menu_,
                IDM_STARTUP_TASK,
                MF_BYCOMMAND | (exists ? MF_CHECKED : MF_UNCHECKED)
            );

            POINT pt{};
            GetCursorPos(&pt);
            tray_.ShowContextMenu(hwnd_, pt, menu_);
            return 0;
        }
        if (lParam == WM_LBUTTONDBLCLK)
        {
            OpenStatusPage();
            return 0;
        }
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}
