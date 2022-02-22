#include <windows.h>
#include <iostream>
#include <winuser.h>
#include "keyinterceptor.hpp"


bool myInterceptor(WPARAM wparam, LPKBDLLHOOKSTRUCT keyEvent,
                   const char *const keystates, void *userData) {

  if (wparam == WM_KEYDOWN) {
        std::cout << "Key down: " << "VK_CODE is " << keyEvent->vkCode << std::endl;

        if (keyEvent->vkCode == VK_ESCAPE) {
          PostQuitMessage(0);
        }
  }

    return false;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {

    uchar lwin[]{VK_LWIN};
    uchar rwin[]{VK_RWIN};

    KeyboardInterceptor::AddInterceptor(lwin, sizeof(lwin));
    KeyboardInterceptor::AddInterceptor(rwin, sizeof(rwin));
    KeyboardInterceptor::AddCustomInterceptor(myInterceptor, NULL);

    KeyboardInterceptor::Hook();
    KeyboardInterceptor::Start();

    MessageBox(NULL,"Application start,will disable win button ,esc to exit!","Hello",MB_OK);

    MSG msg;
    while (GetMessage(&msg, NULL, NULL, NULL)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    MessageBox(NULL,"Application exiting!","Bye",MB_OK);

    KeyboardInterceptor::UnHook();

  return msg.wParam;
}