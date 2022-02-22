#ifndef KEYBOARD_INTERCEPTOR_H_
#define KEYBOARD_INTERCEPTOR_H_

#include <Windows.h>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>


enum class AccessibilityShortcuts : int {
  StickyKeys = 0x01,
  ToggleKeys = 0x02,
  FilterKeys = 0x04,
  AllKeys = StickyKeys | ToggleKeys | FilterKeys
};

inline AccessibilityShortcuts operator|(AccessibilityShortcuts a,
                                        AccessibilityShortcuts b) {
  return static_cast<AccessibilityShortcuts>(static_cast<int>(a) |
                                             static_cast<int>(b));
}

inline int operator&(AccessibilityShortcuts a, AccessibilityShortcuts b) {
  return static_cast<int>(a) & static_cast<int>(b);
}

typedef unsigned char uchar;
typedef bool (*KeyInterceptor)(WPARAM wparam, LPKBDLLHOOKSTRUCT keyEvent,
                               const char *const keystates, void *userData);

struct KeyInterceptorData;

/***
 * those keys can't be intercepted
 * - ctrl + alt + delete
 * - win + g
 * + win + l
 */

class KeyboardInterceptor {

private :

  enum class KeyboardInterceptorState {
    Unhook,
    Hooked,
    Started,
    Stoped,
  };


  // store key states
  static char keystates_[255];
  static std::vector<std::unique_ptr<uchar[]>> interceptors_;
  static std::vector<KeyInterceptorData> customInterceptors_;
  static std::atomic<KeyboardInterceptorState> state_;


  // hook handle
  static HHOOK hook_;

  // mutex
  static std::mutex mtx_;

  static void storeKeyState(WPARAM wparam, LPKBDLLHOOKSTRUCT kbdhookEvent);
  static bool isKeyDown(DWORD vkCode);
  static void dumpKeyInfo(WPARAM wparam, LPKBDLLHOOKSTRUCT kbdhookEvent);
  static bool shouldIntercept(WPARAM wparam, LPKBDLLHOOKSTRUCT kbdEvent);

  static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wparam,
                                               LPARAM lparam);
  // accessibility shortcuts
  static STICKYKEYS startupStickyKeys_;
  static TOGGLEKEYS startupToggleKeys_;
  static FILTERKEYS startupFilterKeys_;

public:
  /**
   * hook system keyboard events
   */
  static void Hook();
  static void AddInterceptor(uchar vkcode[], size_t lenOfVkCode);
  static void AddCustomInterceptor(KeyInterceptor interceptor, void *userData);

  static void ClearInterceptor();
  static void ClearCustomInterceptor();

  static void UnHook();

  static void DisableAccessibilityShortcuts(AccessibilityShortcuts keys);
  static void RestoreAccessibilityShortcuts(AccessibilityShortcuts keys);

  /**
   * start working
   */
  static void Start();
  /**
   * stop working
   */
  static void Stop();
 };


#endif // KEYBOARD_INTERCEPTOR_H_