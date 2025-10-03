#include <format>
#include <fstream>

#include <windows.h>
#include <psapi.h>

#include <nlohmann/json.hpp>

#include <ida.hpp>
#include <idp.hpp>
#include <loader.hpp>

#include "pattern.hpp"

/**
 * @brief IDA Pro plugin module implementation
 *
 * Minimal plugin module that satisfies IDA Pro's plugin interface.
 * The actual patching work is done in DllMain during process attach.
 */
struct idapatch_plugmod_t : public plugmod_t {
  bool idaapi run(size_t) override;
};

/**
 * @brief Plugin run method (not used)
 *
 * Required by IDA Pro plugin interface but not actively used.
 * All patching happens during DLL_PROCESS_ATTACH in DllMain.
 *
 * @return true (always successful)
 */
bool idaapi idapatch_plugmod_t::run(size_t) {
  return true;
}

/**
 * @brief Initializes the IDA Pro plugin
 *
 * Called by IDA Pro to create the plugin module instance.
 *
 * @return Pointer to newly allocated plugin module
 */
static plugmod_t* idaapi init() {
  return new idapatch_plugmod_t;
}

/**
 * @brief IDA Pro plugin descriptor
 *
 * Defines plugin metadata and entry points for IDA Pro.
 * Uses PLUGIN_MULTI flag to allow multiple instances.
 */
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

/**
 * @brief Represents a single patch configuration from JSON
 *
 * Contains all information needed to search for and apply a memory patch:
 * - Identification (name)
 * - Control flag (enabled)
 * - Target modules to patch
 * - Search pattern to locate patch sites
 * - Replacement pattern to apply
 */
struct patch_t {
  std::string name;                     ///< Human-readable patch name
  bool enabled;                         ///< Whether this patch should be applied
  std::vector<std::string> modules;     ///< List of module names to patch (e.g., "ida64.exe")
  std::vector<pattern_byte_t> search;   ///< Pattern to search for in module memory
  std::vector<pattern_byte_t> replace;  ///< Pattern to apply when search pattern is found
};

/**
 * @brief Locates the configuration file path
 *
 * Searches for "ida-patcher.json" in the same directory as the plugin DLL.
 * This allows the configuration file to be deployed alongside the plugin.
 *
 * @param module_handle Handle to the plugin DLL module
 * @return Path to the configuration file (may not exist yet)
 * @throws std::runtime_error if GetModuleFileNameW fails
 */
std::filesystem::path get_config_path(HMODULE module_handle) {
  wchar_t module_path[MAX_PATH];

  // Get the full path to this DLL
  if (!GetModuleFileNameW(module_handle, module_path, MAX_PATH)) {
    throw std::runtime_error("Failed to get module file name");
  }

  // Look for config file in same directory as the DLL
  std::filesystem::path config_path =
    std::filesystem::path(module_path).parent_path() / "ida-patcher.json";
  return config_path;
}

/**
 * @brief Parses the JSON configuration file into patch structures
 *
 * Reads and validates the JSON configuration file, converting each patch entry
 * into a patch_t structure. Expected JSON format:
 * [
 *   {
 *     "name": "patch_name",
 *     "enabled": true,
 *     "modules": ["module1.dll", "module2.exe"],
 *     "search": "48 8B ?? C3",
 *     "replace": "90 90 90 C3"
 *   }
 * ]
 *
 * Validates:
 * - File exists and is readable
 * - JSON is well-formed
 * - Required fields are present in each patch
 *
 * Logs warnings for individual patches that fail to parse but continues
 * processing remaining patches.
 *
 * @param path Path to the ida-patcher.json configuration file
 * @return Vector of successfully parsed patches
 * @throws std::runtime_error if file doesn't exist, can't be opened, or JSON is invalid
 */
std::vector<patch_t> parse_config(const std::filesystem::path& path) {
  // Verify config file exists
  if (!std::filesystem::exists(path)) {
    throw std::runtime_error(std::format("Config file not found: {}", path.string()));
  }

  // Open config file
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error(std::format("Failed to open config file: {}", path.string()));
  }

  // Parse JSON from file
  nlohmann::json patch_list;
  try {
    file >> patch_list;
  } catch (const nlohmann::json::exception& e) {
    throw std::runtime_error(std::format("JSON parse error: {}", e.what()));
  }

  // Convert JSON array to patch structures
  std::vector<patch_t> patches;
  for (const auto& patch : patch_list) {
    try {
      // Parse hex pattern strings into structured format
      std::vector<pattern_byte_t> search_pattern =
        parse_pattern(patch["search"].get<std::string>());
      std::vector<pattern_byte_t> replace_pattern =
        parse_pattern(patch["replace"].get<std::string>());

      // Build patch structure from JSON fields
      patches.push_back(
        {patch["name"].get<std::string>(),
         patch["enabled"].get<bool>(),
         patch["modules"].get<std::vector<std::string>>(),
         search_pattern,
         replace_pattern}
      );
    } catch (const std::exception& e) {
      // Log warning but continue processing other patches
      msg(
        "Warning: Failed to parse patch '%s': %s\n",
        patch.value("name", "unknown").c_str(),
        e.what()
      );
    }
  }

  return patches;
}

/**
 * @brief Applies all enabled patches to their target modules
 *
 * Processing flow for each patch:
 * 1. Check if patch is enabled
 * 2. Validate search and replace patterns are same length
 * 3. For each target module:
 *    a. Verify module is loaded in process
 *    b. Get module memory information (base address and size)
 *    c. Search for pattern in module memory
 *    d. For each match:
 *       - Copy original bytes to buffer
 *       - Apply replacement pattern (respecting wildcards)
 *       - Write patched bytes back to process memory
 *
 * Logs status messages for:
 * - Disabled patches (skipped)
 * - Pattern length mismatches
 * - Module not found errors
 * - Pattern not found in module
 * - Memory write failures
 * - Successful patch applications
 *
 * @param patches Vector of patches to apply
 */
void apply_patches(const std::vector<patch_t>& patches) {
  for (const auto& patch : patches) {
    // Skip disabled patches
    if (!patch.enabled) {
      continue;
    }

    // Validate that search and replace patterns are the same length
    if (patch.search.size() != patch.replace.size()) {
      msg("Search and replace patterns must be of the same length: %s\n", patch.name.c_str());
      continue;
    }

    // Apply this patch to each target module
    for (const auto& module_name : patch.modules) {
      // Check if module is loaded in the process
      HMODULE module_handle = GetModuleHandleA(module_name.c_str());
      if (module_handle == nullptr) {
        msg("Module not found: %s (patch: %s)\n", module_name.c_str(), patch.name.c_str());
        continue;
      }

      // Get module's memory range (base address and size)
      MODULEINFO module_info;
      if (!GetModuleInformation(
            GetCurrentProcess(), module_handle, &module_info, sizeof(module_info)
          )) {
        msg(
          "Failed to get module information: %s (patch: %s)\n",
          module_name.c_str(),
          patch.name.c_str()
        );
        continue;
      }

      // Set up pointers to module's memory region
      auto* data = reinterpret_cast<std::uint8_t*>(module_info.lpBaseOfDll);
      auto data_size = static_cast<std::size_t>(module_info.SizeOfImage);

      // Search for all occurrences of the pattern in this module
      std::vector<std::size_t> matches = find_pattern(data, data_size, patch.search);

      if (matches.empty()) {
        msg("Pattern not found in %s (patch: %s)\n", module_name.c_str(), patch.name.c_str());
        continue;
      }

      // Apply the patch at each match location
      for (std::size_t location : matches) {
        std::size_t buffer_size = patch.replace.size();
        std::vector<std::uint8_t> buffer(buffer_size);

        // Copy original bytes to buffer
        memcpy(buffer.data(), data + location, buffer_size);

        // Apply replacement pattern (respects wildcards to preserve some original bytes)
        apply_pattern_patch(buffer.data(), buffer_size, patch.replace);

        // Write patched bytes back to process memory
        if (!WriteProcessMemory(
              GetCurrentProcess(), data + location, buffer.data(), buffer_size, nullptr
            )) {
          msg(
            "Failed to write memory for patch: %s at 0x%p in %s\n",
            patch.name.c_str(),
            data + location,
            module_name.c_str()
          );
        } else {
          msg(
            "Applied patch: %s at 0x%p in %s\n",
            patch.name.c_str(),
            data + location,
            module_name.c_str()
          );
        }
      }
    }
  }
}

/**
 * @brief DLL entry point - performs patching during process attach
 *
 * Handles DLL_PROCESS_ATTACH event to apply patches early in process lifecycle:
 * 1. Creates a process-specific mutex to prevent multiple instances
 * 2. Locates configuration file (ida-patcher.json)
 * 3. Parses patch definitions from configuration
 * 4. Applies all enabled patches to target modules
 *
 * The mutex prevents race conditions if multiple threads try to load the plugin
 * simultaneously. Uses process ID in mutex name for uniqueness.
 *
 * Error handling:
 * - Logs errors via IDA's msg() function
 * - Returns FALSE if mutex already exists (prevents duplicate patching)
 * - Continues execution even if patching fails (logs errors but doesn't crash)
 *
 * @param module Handle to the DLL instance
 * @param reason Reason code for DLL entry (DLL_PROCESS_ATTACH, etc.)
 * @param Reserved
 * @return TRUE to continue DLL loading, FALSE to abort
 */
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
  // Only run patching when DLL is first loaded into the process
  if (reason == DLL_PROCESS_ATTACH) {
    try {
      // Create a process-specific mutex to prevent duplicate patching
      // If multiple threads try to load the plugin simultaneously, only the first succeeds
      std::string mutex_name = std::format("ida-patcher-{}", GetCurrentProcessId());

      HANDLE mutex_handle = CreateMutexA(nullptr, FALSE, mutex_name.c_str());
      if (mutex_handle == nullptr || GetLastError() == ERROR_ALREADY_EXISTS) {
        // Mutex already exists - another instance is running or already ran
        return FALSE;
      }

      // Locate the JSON configuration file
      std::filesystem::path config_path = get_config_path(module);

      // Parse patch definitions from JSON
      std::vector<patch_t> patches = parse_config(config_path);

      // Apply all enabled patches to their target modules
      apply_patches(patches);
    } catch (const std::exception& e) {
      // Log error but don't crash IDA
      msg("Error: %s\n", e.what());
    }
  }

  return TRUE;
}
