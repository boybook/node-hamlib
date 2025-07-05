#pragma once

#include <napi.h>
#include <hamlib/rig.h>
//forward declarations
//typedef struct s_rig RIG;
//typedef unsigned int 	vfo_t;
//typedef double freq_t;


class NodeHamLib : public Napi::ObjectWrap<NodeHamLib> {
 public:
  NodeHamLib(const Napi::CallbackInfo&);
  Napi::Value Open(const Napi::CallbackInfo&);
  Napi::Value SetVFO(const Napi::CallbackInfo&);
  Napi::Value SetFrequency(const Napi::CallbackInfo&);
  Napi::Value SetMode(const Napi::CallbackInfo&);
  Napi::Value SetPtt(const Napi::CallbackInfo&);
  Napi::Value GetFrequency(const Napi::CallbackInfo&);
  Napi::Value GetVFO(const Napi::CallbackInfo&);
  Napi::Value GetMode(const Napi::CallbackInfo&);
  Napi::Value GetStrength(const Napi::CallbackInfo&);

  Napi::Value Close(const Napi::CallbackInfo&);
  Napi::Value Destroy(const Napi::CallbackInfo&);
  Napi::Value GetConnectionInfo(const Napi::CallbackInfo&);
  
  // Static method to get supported rig models
  static Napi::Value GetSupportedRigs(const Napi::CallbackInfo&);
  
  static Napi::Function GetClass(Napi::Env);

  static int freq_change_cb(RIG*, vfo_t, freq_t, void*);

 private:
  RIG *my_rig;
  bool rig_is_open = false;
  bool is_network_rig = false;     // Flag to indicate if using network connection
  rig_model_t original_model = 0;  // Store original model when using network
  int count = 0;
  void* freq_emit_cb;
  char port_path[HAMLIB_FILPATHLEN];  // Store the port path
  static Napi::FunctionReference constructor;
  Napi::CallbackInfo * m_currentInfo;
  
  // Helper method to detect network address format
  bool isNetworkAddress(const char* path);
  
  // Static callback helper for rig_list_foreach
  static int rig_list_callback(const struct rig_caps *caps, void *data);
};