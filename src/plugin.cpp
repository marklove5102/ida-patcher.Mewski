#include <cstring>

#include <nlohmann/json.hpp>

#include <ida.hpp>
#include <idp.hpp>
#include <loader.hpp>

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
