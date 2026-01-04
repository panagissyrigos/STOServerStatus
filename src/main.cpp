#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "App.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    App app(hInstance);
    return app.Run();
}
