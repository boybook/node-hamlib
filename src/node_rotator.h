#pragma once

#include <napi.h>
#include "shim/hamlib_shim.h"
#include <string>

class NodeRotator : public Napi::ObjectWrap<NodeRotator> {
 public:
  NodeRotator(const Napi::CallbackInfo&);
  ~NodeRotator();

  Napi::Value Open(const Napi::CallbackInfo&);
  Napi::Value Close(const Napi::CallbackInfo&);
  Napi::Value Destroy(const Napi::CallbackInfo&);
  Napi::Value GetConnectionInfo(const Napi::CallbackInfo&);

  Napi::Value SetPosition(const Napi::CallbackInfo&);
  Napi::Value GetPosition(const Napi::CallbackInfo&);
  Napi::Value Move(const Napi::CallbackInfo&);
  Napi::Value Stop(const Napi::CallbackInfo&);
  Napi::Value Park(const Napi::CallbackInfo&);
  Napi::Value Reset(const Napi::CallbackInfo&);
  Napi::Value GetInfo(const Napi::CallbackInfo&);
  Napi::Value GetStatus(const Napi::CallbackInfo&);

  Napi::Value SetConf(const Napi::CallbackInfo&);
  Napi::Value GetConf(const Napi::CallbackInfo&);
  Napi::Value GetConfigSchema(const Napi::CallbackInfo&);
  Napi::Value GetPortCaps(const Napi::CallbackInfo&);
  Napi::Value GetRotatorCaps(const Napi::CallbackInfo&);

  Napi::Value SetLevel(const Napi::CallbackInfo&);
  Napi::Value GetLevel(const Napi::CallbackInfo&);
  Napi::Value GetSupportedLevels(const Napi::CallbackInfo&);
  Napi::Value SetFunction(const Napi::CallbackInfo&);
  Napi::Value GetFunction(const Napi::CallbackInfo&);
  Napi::Value GetSupportedFunctions(const Napi::CallbackInfo&);
  Napi::Value SetParm(const Napi::CallbackInfo&);
  Napi::Value GetParm(const Napi::CallbackInfo&);
  Napi::Value GetSupportedParms(const Napi::CallbackInfo&);

  static Napi::Value GetSupportedRotators(const Napi::CallbackInfo&);
  static Napi::Value GetHamlibVersion(const Napi::CallbackInfo&);
  static Napi::Value SetDebugLevel(const Napi::CallbackInfo&);
  static Napi::Value GetDebugLevel(const Napi::CallbackInfo&);
  static Napi::Value GetCopyright(const Napi::CallbackInfo&);
  static Napi::Value GetLicense(const Napi::CallbackInfo&);

  static Napi::Function GetClass(Napi::Env);

  hamlib_shim_handle_t my_rot;
  bool rot_is_open = false;
  bool is_network_rot = false;
  unsigned int original_model = 0;
  char port_path[SHIM_HAMLIB_FILPATHLEN];
  static Napi::FunctionReference constructor;

 private:
  bool isNetworkAddress(const char* path);
  static int rot_list_callback(const shim_rot_info_t* info, void* data);
  static int rot_config_callback(const shim_confparam_info_t* info, void* data);
};
