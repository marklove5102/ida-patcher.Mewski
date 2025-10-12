#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

// Platform-specific type definitions
#ifdef _WIN32
#include <windows.h>
using module_handle_t = HMODULE;
#else
using module_handle_t = void*;
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
std::filesystem::path get_config_path(module_handle_t module_handle);

/**
 * @brief Cross-platform module handle retrieval
 *
 * Gets a handle to a loaded module by name.
 *
 * @param module_name Name of the module to find
 * @return Module handle, or nullptr if not found
 */
module_handle_t get_module_handle(const std::string& module_name);

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
bool get_module_info(module_handle_t module_handle, void** base_address, size_t* size);

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
bool write_process_memory(void* address, const void* data, size_t size);

/**
 * @brief Performs patching initialization
 *
 * Called when the plugin is loaded to apply patches early in process lifecycle.
 * This function is called by platform-specific entry points.
 *
 * @param module_handle Handle to the plugin module
 */
void initialize_patcher(module_handle_t module_handle);
