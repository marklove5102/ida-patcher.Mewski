#include <windows.h>

static BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    DWORD process_id = GetCurrentProcessId();
  }

  return TRUE;
}
