// main.cpp - program starts here.
#include "Cashier.h"
#include "Database.h"
#include <commctrl.h>
#include <string>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    // needed for the tab control
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_TAB_CLASSES;
    InitCommonControlsEx(&icc);

    // 1) connect to XAMPP MySQL. If it fails, show a message and stop.
    std::string error;
    if (!ConnectDatabase(error)) {
        std::string message = "Gagal terhubung ke MySQL.\n\nPastikan:\n"
                              "1. XAMPP sudah dibuka.\n"
                              "2. MySQL sudah di-Start.\n"
                              "3. Database cashier_db sudah di-import.\n"
                              "4. Host/port/user/password di Database.cpp benar.\n\n"
                              "Detail: " + error;
        MessageBoxA(NULL, message.c_str(), "Koneksi Gagal", MB_ICONERROR);
        return 1;
    }

    // 2) show the window
    if (!CreateMainWindow(hInstance, nCmdShow))
        return 1;

    // 3) message loop: keeps the window alive until it is closed
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CloseDatabase();
    return 0;
}
