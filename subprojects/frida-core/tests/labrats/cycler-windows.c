#include <windows.h>

int WINAPI
wWinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
  DWORD end_time;

  (void) hInstance;
  (void) hPrevInstance;
  (void) pCmdLine;
  (void) nCmdShow;

  end_time = GetTickCount () + 60000;

  do
  {
    Sleep (1);
  }
  while (GetTickCount () < end_time);

  return 0;
}