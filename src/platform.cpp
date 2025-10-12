#include "platform.hpp"

#include <cstring>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <vector>

#include <nlohmann/json.hpp>

#include <ida.hpp>
#include <kernwin.hpp>

#include "pattern.hpp"

// Platform-specific headers
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#elif defined(__linux__)
#include <dlfcn.h>
#include <elf.h>
#include <link.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

/**
 * @brief Locates the configuration file path
 *
 * Searches for "ida-patcher.json" in the same directory as the plugin binary.
 * This allows the configuration file to be deployed alongside the plugin.
 *
 * @param module_handle Handle to the plugin module
 * @return Path to the configuration file (may not exist yet)
 * @throws std::runtime_error if unable to get module path
 */
std::filesystem::path get_config_path(module_handle_t module_handle) {
#ifdef _WIN32
  wchar_t module_path[MAX_PATH];
  // Get the full path to this DLL
  if (!GetModuleFileNameW(module_handle, module_path, MAX_PATH)) {
    throw std::runtime_error("Failed to get module file name");
  }
  return std::filesystem::path(module_path).parent_path() / "ida-patcher.json";
#else
  (void)module_handle;  // Unused on Unix
  Dl_info dl_info;
  // Get info about the loaded shared library
  if (dladdr(reinterpret_cast<void*>(&get_config_path), &dl_info) == 0) {
    throw std::runtime_error("Failed to get module file name");
  }
  return std::filesystem::path(dl_info.dli_fname).parent_path() / "ida-patcher.json";
#endif
}

/**
 * @brief Cross-platform module handle retrieval
 *
 * Gets a handle to a loaded module by name.
 *
 * @param module_name Name of the module to find
 * @return Module handle, or nullptr if not found
 */
module_handle_t get_module_handle(const std::string& module_name) {
#ifdef _WIN32
  return GetModuleHandleA(module_name.c_str());
#elif defined(__APPLE__)
  // On macOS, search through loaded images by name
  const uint32_t image_count = _dyld_image_count();
  for (uint32_t i = 0; i < image_count; i++) {
    const char* image_name = _dyld_get_image_name(i);
    if (image_name != nullptr) {
      // Check if the module name matches either the full path or just the filename
      const std::string full_path(image_name);
      if (full_path.find(module_name) != std::string::npos ||
          std::filesystem::path(full_path).filename() == module_name) {
        const auto* header = _dyld_get_image_header(i);
        return const_cast<void*>(static_cast<const void*>(header));
      }
    }
  }
  return nullptr;
#elif defined(__linux__)
  // On Linux, try to open with RTLD_NOLOAD first (doesn't increment refcount)
  void* handle = dlopen(module_name.c_str(), RTLD_NOLOAD | RTLD_NOW);
  if (handle != nullptr) {
    return handle;
  }

  // If not found by direct name, search through loaded modules
  // This callback searches for a module by name
  struct callback_data {
    std::string target_name;
    void* result;
  };

  callback_data data{module_name, nullptr};

  dl_iterate_phdr(
    [](struct dl_phdr_info* info, size_t size, void* data_ptr) -> int {
      auto* cb_data = static_cast<callback_data*>(data_ptr);
      if (info->dlpi_name != nullptr && info->dlpi_name[0] != '\0') {
        std::string full_path(info->dlpi_name);
        // Check if the module name matches either the full path or just the filename
        if (full_path.find(cb_data->target_name) != std::string::npos ||
            std::filesystem::path(full_path).filename() == cb_data->target_name) {
          // Open the module with RTLD_NOLOAD to get a handle
          cb_data->result = dlopen(info->dlpi_name, RTLD_NOLOAD | RTLD_NOW);
          return 1;  // Stop iteration
        }
      }
      return 0;  // Continue iteration
    },
    &data
  );

  return data.result;
#else
  return dlopen(module_name.c_str(), RTLD_NOLOAD | RTLD_NOW);
#endif
}

/**
 * @brief Cross-platform module memory information retrieval
 *
 * Gets the base address and size of a loaded module's memory region.
 *
 * @param module_handle Handle to the module
 * @param base_address Output: pointer to module's base address
 * @param size Output: size of module's memory region
 * @return true if successful, false otherwise
 */
bool get_module_info(module_handle_t module_handle, void** base_address, size_t* size) {
#ifdef _WIN32
  MODULEINFO module_info;
  if (!GetModuleInformation(
        GetCurrentProcess(), module_handle, &module_info, sizeof(module_info)
      )) {
    return false;
  }
  *base_address = module_info.lpBaseOfDll;
  *size = module_info.SizeOfImage;
  return true;
#elif defined(__APPLE__)
  // On macOS, module_handle is the mach_header pointer
  const mach_header_64* header = reinterpret_cast<const mach_header_64*>(module_handle);
  *base_address = module_handle;

  // Calculate total size by iterating through load commands
  size_t total_size = 0;
  const uint8_t* cmd_ptr = reinterpret_cast<const uint8_t*>(header) + sizeof(mach_header_64);

  for (uint32_t i = 0; i < header->ncmds; i++) {
    const auto* cmd = reinterpret_cast<const load_command*>(cmd_ptr);

    if (cmd->cmd == LC_SEGMENT_64) {
      const auto* seg = reinterpret_cast<const segment_command_64*>(cmd);
      // Calculate the end of this segment
      size_t seg_end = seg->vmaddr + seg->vmsize;
      if (seg_end > total_size) {
        total_size = seg_end;
      }
    }

    cmd_ptr += cmd->cmdsize;
  }

  *size = total_size;
  return true;
#elif defined(__linux__)
  auto* map = static_cast<link_map*>(module_handle);
  if (map == nullptr) {
    return false;
  }

  // Get base address from link_map
  *base_address = reinterpret_cast<void*>(map->l_addr);

  // Parse ELF header to calculate actual module size
  const auto* ehdr = reinterpret_cast<const ElfW(Ehdr)*>(map->l_addr);
  const auto* phdr = reinterpret_cast<const ElfW(Phdr)*>(map->l_addr + ehdr->e_phoff);

  size_t max_addr = 0;
  for (ElfW(Half) i = 0; i < ehdr->e_phnum; i++) {
    if (phdr[i].p_type == PT_LOAD) {
      const size_t seg_end = phdr[i].p_vaddr + phdr[i].p_memsz;
      if (seg_end > max_addr) {
        max_addr = seg_end;
      }
    }
  }

  *size = max_addr;
  return true;
#endif
}

/**
 * @brief Cross-platform memory write with protection handling
 *
 * Writes data to process memory, temporarily changing protection if needed.
 *
 * @param address Target address to write to
 * @param data Data to write
 * @param size Number of bytes to write
 * @return true if successful, false otherwise
 */
bool write_process_memory(void* address, const void* data, size_t size) {
#ifdef _WIN32
  return WriteProcessMemory(GetCurrentProcess(), address, data, size, nullptr) != 0;
#elif defined(__APPLE__)
  // Change memory protection to allow writing
  kern_return_t kr = mach_vm_protect(
    mach_task_self(),
    reinterpret_cast<mach_vm_address_t>(address),
    size,
    FALSE,
    VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY
  );
  if (kr != KERN_SUCCESS) {
    return false;
  }

  // Write the data
  memcpy(address, data, size);

  // Restore original protection
  mach_vm_protect(
    mach_task_self(),
    reinterpret_cast<mach_vm_address_t>(address),
    size,
    FALSE,
    VM_PROT_READ | VM_PROT_EXECUTE
  );

  return true;
#elif defined(__linux__)
  // Get page size and align address
  const long page_size = sysconf(_SC_PAGESIZE);
  const auto addr_int = reinterpret_cast<uintptr_t>(address);
  auto* page_start = reinterpret_cast<void*>(addr_int & ~static_cast<uintptr_t>(page_size - 1));
  const auto page_start_int = reinterpret_cast<uintptr_t>(page_start);
  const size_t protection_size = size + (addr_int - page_start_int);

  // Change memory protection
  if (mprotect(page_start, protection_size, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
    return false;
  }

  // Write the data
  memcpy(address, data, size);

  return true;
#endif
}

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
static std::vector<patch_t> parse_config(const std::filesystem::path& path) {
  // Verify config file exists
  if (!std::filesystem::exists(path)) {
    throw std::runtime_error("Config file not found: " + path.string());
  }

  // Open config file
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open config file: " + path.string());
  }

  // Parse JSON from file
  nlohmann::json patch_list;
  try {
    file >> patch_list;
  } catch (const nlohmann::json::exception& e) {
    throw std::runtime_error(std::string("JSON parse error: ") + e.what());
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
static void apply_patches(const std::vector<patch_t>& patches) {
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
      module_handle_t module_handle = get_module_handle(module_name);
      if (module_handle == nullptr) {
        msg("Module not found: %s (patch: %s)\n", module_name.c_str(), patch.name.c_str());
        continue;
      }

      // Get module's memory range (base address and size)
      void* base_address = nullptr;
      size_t module_size = 0;
      if (!get_module_info(module_handle, &base_address, &module_size)) {
        msg(
          "Failed to get module information: %s (patch: %s)\n",
          module_name.c_str(),
          patch.name.c_str()
        );
        continue;
      }

      // Set up pointers to module's memory region
      auto* data = reinterpret_cast<std::uint8_t*>(base_address);
      auto data_size = module_size;

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
        apply_pattern_patch(buffer.data(), buffer.size(), patch.replace);

        // Write patched bytes back to process memory
        if (!write_process_memory(data + location, buffer.data(), buffer_size)) {
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
 * @brief Performs patching initialization
 *
 * Called when the plugin is loaded to apply patches early in process lifecycle:
 * 1. Uses a static mutex to prevent multiple instances
 * 2. Locates configuration file (ida-patcher.json)
 * 3. Parses patch definitions from configuration
 * 4. Applies all enabled patches to target modules
 *
 * The static mutex and once_flag prevent race conditions if multiple threads
 * try to initialize simultaneously.
 *
 * Error handling:
 * - Logs errors via IDA's msg() function
 * - Continues execution even if patching fails (logs errors but doesn't crash)
 *
 * @param module_handle Handle to the plugin module
 */
void initialize_patcher(module_handle_t module_handle) {
  static std::once_flag init_flag;

  std::call_once(init_flag, [module_handle]() {
    try {
      // Locate the JSON configuration file
      std::filesystem::path config_path = get_config_path(module_handle);

      // Parse patch definitions from JSON
      std::vector<patch_t> patches = parse_config(config_path);

      // Apply all enabled patches to their target modules
      apply_patches(patches);
    } catch (const std::exception& e) {
      // Log error but don't crash IDA
      msg("ida-patcher error: %s\n", e.what());
    }
  });
}

#ifdef _WIN32
/**
 * @brief Windows DLL entry point
 *
 * Performs patching during DLL_PROCESS_ATTACH.
 *
 * @param module Handle to the DLL instance
 * @param reason Reason code for DLL entry
 * @param Reserved
 * @return TRUE to continue DLL loading
 */
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    initialize_patcher(module);
  }
  return TRUE;
}
#else
/**
 * @brief Unix shared library constructor
 *
 * Automatically called when the shared library is loaded.
 * Performs patching initialization.
 */
__attribute__((constructor)) static void plugin_init() {
  // For Unix systems, we use dladdr to get our own handle in get_config_path
  initialize_patcher(nullptr);
}
#endif
