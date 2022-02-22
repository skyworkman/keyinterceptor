#include "keyinterceptor.hpp"
#include <WinUser.h>
#include <Windows.h>
#include <assert.h>
#include <iostream>
#include <memory>
#include <sstream>


struct KeyInterceptorData {
  KeyInterceptor interceptor;
  void *userData;
};

void KeyboardInterceptor::storeKeyState(WPARAM wparam, LPKBDLLHOOKSTRUCT kbdhookEvent) {

  if (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN) {
    keystates_[kbdhookEvent->vkCode] = 1;
  }
  if (wparam == WM_KEYUP || wparam == WM_SYSKEYUP) {
    keystates_[kbdhookEvent->vkCode] = 0;
  }
}

bool KeyboardInterceptor::isKeyDown(DWORD vkCode) {

  if (vkCode == VK_CONTROL) {
    return keystates_[VK_LCONTROL] || keystates_[VK_RCONTROL];
  }

  if (vkCode == VK_MENU) {
    return keystates_[VK_LMENU] || keystates_[VK_RMENU];
  }

  if (vkCode == VK_SHIFT) {
    return keystates_[VK_LSHIFT] || keystates_[VK_RSHIFT];
  }

  return keystates_[vkCode];
}

void KeyboardInterceptor::dumpKeyInfo(WPARAM wparam,
                                      LPKBDLLHOOKSTRUCT kbdhookEvent) {

  std::wstringstream ss;

  switch (wparam) {
  case WM_KEYDOWN: {
    ss << "WM_KEYDOWN";
  } break;
  case WM_KEYUP: {
    ss << "WM_KEYUP";
  } break;
  case WM_SYSKEYDOWN: {
    ss << "WM_SYSKEYDOWN";
  } break;
  case WM_SYSKEYUP: {
    ss << "WM_SYSKEYUP";
  } break;
  };
  ss << " | scanCode: " << kbdhookEvent->scanCode
     << " | extraInfo: " << kbdhookEvent->dwExtraInfo << " | flags "
     << kbdhookEvent->flags << " | vkCode: " << kbdhookEvent->vkCode
     << " | time: " << kbdhookEvent->time << std::endl;

  OutputDebugStringW(ss.str().c_str());
}

bool KeyboardInterceptor::shouldIntercept(WPARAM wparam,
                                          LPKBDLLHOOKSTRUCT kbdEvent) {

  for (auto iter = customInterceptors_.begin();
       iter != customInterceptors_.end(); iter++) {
    try {
      KeyInterceptorData *data = &(*iter);
      if (data->interceptor != nullptr) {

        bool intercept =
            (data->interceptor)(wparam, kbdEvent, keystates_, data->userData);
        if (intercept)
          return true;
      }
    } catch (std::exception &ex) {
      OutputDebugStringA((std::string(ex.what()) + "\n").c_str());
    }
  }

  for (auto iter = interceptors_.begin(); iter != interceptors_.end(); iter++) {
    uchar *codes = iter->get();
    bool isDown = false;
    int index = 0;
    for (uchar *code = codes; *code != 0; code++, index++) {
      isDown = isKeyDown(*code);
      if (!isDown)
        break;
    }
    if (isDown)
      return true;
  }

  return false;
}

LRESULT CALLBACK KeyboardInterceptor::LowLevelKeyboardProc(int nCode,
                                                           WPARAM wparam,
                                                           LPARAM lparam) {

  if (nCode < 0 || state_ != KeyboardInterceptorState::Started)
    return CallNextHookEx(hook_, nCode, wparam, lparam);

  if (nCode == HC_ACTION) {
    LPKBDLLHOOKSTRUCT keyEvent = reinterpret_cast<LPKBDLLHOOKSTRUCT>(lparam);
    assert(keyEvent);

    storeKeyState(wparam, keyEvent);
    // dumpKeyInfo(wparam, keyEvent);

    if (shouldIntercept(wparam, keyEvent)) {
      return 1;
    }
  }

  return CallNextHookEx(hook_, nCode, wparam, lparam);
}

void KeyboardInterceptor::Hook() {
  if (state_ == KeyboardInterceptorState::Unhook) {
    hook_ = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc,
                             GetModuleHandle(NULL), 0);
    state_ = KeyboardInterceptorState::Hooked;
  }
}

void KeyboardInterceptor::AddInterceptor(uchar vkcode[], size_t lenOfVkCode) {

  mtx_.lock();

  // VKCODE 1-254
  std::unique_ptr<uchar[]> codes = std::make_unique<uchar[]>(lenOfVkCode + 1);

  codes.get()[lenOfVkCode] = 0;
  for (int i = 0; i < lenOfVkCode; i++) {
    codes.get()[i] = vkcode[i];
  }

  interceptors_.push_back(std::move(codes));

  mtx_.unlock();
}

void KeyboardInterceptor::AddCustomInterceptor(KeyInterceptor interceptor,
                                               void *userData) {
  mtx_.lock();
  customInterceptors_.push_back(
      std::move(KeyInterceptorData{interceptor, userData}));
  mtx_.unlock();
}
void KeyboardInterceptor::ClearInterceptor() {
  mtx_.lock();
  interceptors_.clear();
  mtx_.unlock();
}
void KeyboardInterceptor::ClearCustomInterceptor() {
  mtx_.lock();
  // customInterceptors_.clear();
  mtx_.unlock();
}
void KeyboardInterceptor::UnHook() {

  if (state_ == KeyboardInterceptorState::Unhook)
    return;

  UnhookWindowsHookEx(hook_);
  hook_ = NULL;
  ClearInterceptor();
  ClearCustomInterceptor();
  state_ = KeyboardInterceptorState::Unhook;
}

void KeyboardInterceptor::DisableAccessibilityShortcuts(
    AccessibilityShortcuts keys) {

  if (keys & AccessibilityShortcuts::StickyKeys) {

    SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS),
                         &startupStickyKeys_, 0);

    STICKYKEYS skOff = startupStickyKeys_;
    if ((skOff.dwFlags & SKF_STICKYKEYSON) == 0) {
      // Disable the hotkey and the confirmation
      skOff.dwFlags &= ~SKF_HOTKEYACTIVE;
      skOff.dwFlags &= ~SKF_CONFIRMHOTKEY;

      SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &skOff, 0);
    }
  }

  if (keys & AccessibilityShortcuts::ToggleKeys) {

    SystemParametersInfo(SPI_GETTOGGLEKEYS, sizeof(STICKYKEYS),
                         &startupToggleKeys_, 0);

    TOGGLEKEYS tkOff = startupToggleKeys_;
    if ((tkOff.dwFlags & TKF_TOGGLEKEYSON) == 0) {
      // Disable the hotkey and the confirmation
      tkOff.dwFlags &= ~TKF_HOTKEYACTIVE;
      tkOff.dwFlags &= ~TKF_CONFIRMHOTKEY;

      SystemParametersInfo(SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), &tkOff, 0);
    }
  }

  if (keys & AccessibilityShortcuts::FilterKeys) {

    SystemParametersInfo(SPI_GETFILTERKEYS, sizeof(STICKYKEYS),
                         &startupFilterKeys_, 0);
    FILTERKEYS fkOff = startupFilterKeys_;
    if ((fkOff.dwFlags & FKF_FILTERKEYSON) == 0) {
      // Disable the hotkey and the confirmation
      fkOff.dwFlags &= ~FKF_HOTKEYACTIVE;
      fkOff.dwFlags &= ~FKF_CONFIRMHOTKEY;

      SystemParametersInfo(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &fkOff, 0);
    }
  }
}

void KeyboardInterceptor::RestoreAccessibilityShortcuts(
    AccessibilityShortcuts keys) {

  if (keys & AccessibilityShortcuts::StickyKeys) {
    SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS),
                         &startupStickyKeys_, 0);
  }

  if (keys & AccessibilityShortcuts::ToggleKeys) {
    SystemParametersInfo(SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS),
                         &startupToggleKeys_, 0);
  }

  if (keys & AccessibilityShortcuts::FilterKeys) {
    SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(FILTERKEYS),
                         &startupFilterKeys_, 0);
  }
}

void KeyboardInterceptor::Start() {
  if (state_ != KeyboardInterceptorState::Unhook) {
    state_ = KeyboardInterceptorState::Started;
  }
}

void KeyboardInterceptor::Stop() {
  if (state_ != KeyboardInterceptorState::Unhook) {
    state_ = KeyboardInterceptorState::Stoped;
  }
}

char KeyboardInterceptor::keystates_[255]{0};
std::vector<std::unique_ptr<uchar[]>> KeyboardInterceptor::interceptors_;
std::vector<KeyInterceptorData> KeyboardInterceptor::customInterceptors_;

std::atomic<KeyboardInterceptor::KeyboardInterceptorState>
    KeyboardInterceptor::state_{KeyboardInterceptorState::Unhook};
HHOOK KeyboardInterceptor::hook_;
std::mutex KeyboardInterceptor::mtx_;

STICKYKEYS KeyboardInterceptor::startupStickyKeys_ = {sizeof(STICKYKEYS), 0};
TOGGLEKEYS KeyboardInterceptor::startupToggleKeys_ = {sizeof(TOGGLEKEYS), 0};
FILTERKEYS KeyboardInterceptor::startupFilterKeys_ = {sizeof(FILTERKEYS), 0};
