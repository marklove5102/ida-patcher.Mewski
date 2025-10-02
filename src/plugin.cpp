#include <format>
#include <fstream>

#include <windows.h>

#include <ida.hpp>
#include <idp.hpp>
#include <loader.hpp>

#include <nlohmann/json.hpp>

struct idapatch_plugmod_t : public plugmod_t {
  bool idaapi run(size_t) override;
};

bool idaapi idapatch_plugmod_t::run(size_t) {
  return true;
}

static plugmod_t* idaapi init() {
  return new idapatch_plugmod_t;
}

plugin_t PLUGIN = {
  IDP_INTERFACE_VERSION,
  PLUGIN_MULTI,
  init,
  nullptr,
  nullptr,
  nullptr,
  nullptr,
  "ida-patcher",
  nullptr
};

struct patch_t {
  std::string name;
  bool enabled;
  std::string module;
  std::string search;
  std::string replace;
};

std::vector<patch_t> parse_config(const std::string& path) {
  std::ifstream file(path);
  nlohmann::json patch_list;
  file >> patch_list;

  std::vector<patch_t> patches;
  for (const auto& patch : patch_list) {
    patches.push_back({
      patch["name"].get<std::string>(),
      patch["enabled"].get<bool>(),
      patch["module"].get<std::string>(),
      patch["search"].get<std::string>(),
      patch["replace"].get<std::string>()
    });
  }
  
  return patches;
}

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    std::string mutex_name = std::format("ida-patcher-{}", GetCurrentProcessId());

    HANDLE mutex_handle = CreateMutexA(nullptr, FALSE, mutex_name.c_str());
    if (mutex_handle == nullptr) {
      return FALSE;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
      CloseHandle(mutex_handle);
      return FALSE;
    }

    MessageBoxA(
      nullptr, "ida-patcher plugin loaded successfully.", "ida-patcher", MB_OK | MB_ICONINFORMATION
    );
  }

  return TRUE;
}
