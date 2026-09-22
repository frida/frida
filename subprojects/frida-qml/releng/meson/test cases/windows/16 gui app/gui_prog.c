#include <windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    // avoid unused argument error while matching template
    ((void)hInstance);
    ((void)hPrevInstance);
    ((void)lpCmdLine);
    ((void)nCmdShow);
    return 0;
}
