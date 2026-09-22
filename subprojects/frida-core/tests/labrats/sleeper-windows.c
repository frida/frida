#include <windows.h>

int WINAPI
wWinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
  MSG msg;
  UINT_PTR timer;
  BOOL result;

  (void) hInstance;
  (void) hPrevInstance;
  (void) pCmdLine;
  (void) nCmdShow;

  timer = SetTimer (NULL, 0, 60000, NULL);

  while ((result = GetMessage (&msg, NULL, 0, 0)) != 0)
  {
    if (result == -1)
    {
      return 1;
    }
    else
    {
      if (msg.message == WM_TIMER && msg.hwnd == NULL && msg.wParam == timer)
        break;
      TranslateMessage (&msg);
      DispatchMessage (&msg);
    }
  }

  return 0;
}