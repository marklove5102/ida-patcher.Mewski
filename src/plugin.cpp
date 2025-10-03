#include <windows.h>
#include <psapi.h>

#include <format>
#include <fstream>

#include <nlohmann/json.hpp>

#include <ida.hpp>
#include <idp.hpp>
#include <loader.hpp>

#include "pattern.hpp"

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
  std::vector<pattern_byte_t> search;
  std::vector<pattern_byte_t> replace;
};

std::filesystem::path get_config_path(HMODULE module_handle) {
  wchar_t module_path[MAX_PATH];
  if (!GetModuleFileNameW(module_handle, module_path, MAX_PATH)) {
    throw std::runtime_error("Failed to get module file name");
  }

  // Look for config file in same directory as the DLL
  std::filesystem::path config_path =
    std::filesystem::path(module_path).parent_path() / "ida-patcher.json";
  return config_path;
}

std::vector<patch_t> parse_config(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) {
    throw std::runtime_error(std::format("Config file not found: {}", path.string()));
  }

  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error(std::format("Failed to open config file: {}", path.string()));
  }

  nlohmann::json patch_list;
  try {
    file >> patch_list;
  } catch (const nlohmann::json::exception& e) {
    throw std::runtime_error(std::format("JSON parse error: {}", e.what()));
  }

  std::vector<patch_t> patches;
  for (const auto& patch : patch_list) {
    try {
      std::vector<pattern_byte_t> search_pattern =
        parse_pattern(patch["search"].get<std::string>());
      std::vector<pattern_byte_t> replace_pattern =
        parse_pattern(patch["replace"].get<std::string>());

      patches.push_back(
        {patch["name"].get<std::string>(),
         patch["enabled"].get<bool>(),
         patch["module"].get<std::string>(),
         search_pattern,
         replace_pattern}
      );
    } catch (const std::exception& e) {
      msg(
        "Warning: Failed to parse patch '%s': %s\n",
        patch.value("name", "unknown").c_str(),
        e.what()
      );
    }
  }

  return patches;
}

void apply_patches(const std::vector<patch_t>& patches) {
  for (const auto& patch : patches) {
    if (!patch.enabled) {
      continue;
    }

    HMODULE module_handle = GetModuleHandleA(patch.module.c_str());
    if (module_handle == nullptr) {
      msg("Module not found: %s\n", patch.module.c_str());
      continue;
    }

    // Get module memory range for pattern searching
    MODULEINFO module_info;
    if (!GetModuleInformation(
          GetCurrentProcess(), module_handle, &module_info, sizeof(module_info)
        )) {
      msg("Failed to get module information: %s\n", patch.module.c_str());
      continue;
    }

    auto* data = reinterpret_cast<std::uint8_t*>(module_info.lpBaseOfDll);
    auto data_size = static_cast<std::size_t>(module_info.SizeOfImage);

    if (patch.search.size() != patch.replace.size()) {
      msg("Search and replace patterns must be of the same length: %s\n", patch.name.c_str());
      continue;
    }

    std::vector<std::size_t> matches = find_pattern(data, data_size, patch.search);

    if (matches.empty()) {
      msg("Pattern not found: %s\n", patch.name.c_str());
      continue;
    }

    for (std::size_t location : matches) {
      std::size_t buffer_size = patch.replace.size();
      std::vector<std::uint8_t> buffer(buffer_size);
      memcpy(buffer.data(), data + location, buffer_size);
      // Apply replacement pattern (handles wildcards by preserving original bytes)
      apply_pattern_patch(buffer.data(), buffer_size, patch.replace);

      if (!WriteProcessMemory(
            GetCurrentProcess(), data + location, buffer.data(), buffer_size, nullptr
          )) {
        msg("Failed to write memory for patch: %s at 0x%p\n", patch.name.c_str(), data + location);
      }

      msg("Applied patch: %s at 0x%p\n", patch.name.c_str(), data + location);
    }
  }
}

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    try {
      // Prevent multiple instances from running simultaneously
      std::string mutex_name = std::format("ida-patcher-{}", GetCurrentProcessId());

      HANDLE mutex_handle = CreateMutexA(nullptr, FALSE, mutex_name.c_str());
      if (mutex_handle == nullptr || GetLastError() == ERROR_ALREADY_EXISTS) {
        return FALSE;
      }

      std::filesystem::path config_path = get_config_path(module);
      std::vector<patch_t> patches = parse_config(config_path);

      apply_patches(patches);
    } catch (const std::exception& e) {
      msg("ida-patcher error: %s\n", e.what());
    }
  }

  return TRUE;
}
