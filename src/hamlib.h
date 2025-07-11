#pragma once

#include <napi.h>
#include <hamlib/rig.h>
#include <memory>
//forward declarations
//typedef struct s_rig RIG;
//typedef unsigned int 	vfo_t;
//typedef double freq_t;

// Forward declaration
class NodeHamLib;

// Base AsyncWorker class for hamlib operations
class HamLibAsyncWorker : public Napi::AsyncWorker {
public:
    HamLibAsyncWorker(Napi::Function& callback, NodeHamLib* hamlib_instance);
    virtual ~HamLibAsyncWorker() = default;

protected:
    NodeHamLib* hamlib_instance_;
    int result_code_;
    std::string error_message_;
};


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
  
  // Memory Channel Management
  Napi::Value SetMemoryChannel(const Napi::CallbackInfo&);
  Napi::Value GetMemoryChannel(const Napi::CallbackInfo&);
  Napi::Value SelectMemoryChannel(const Napi::CallbackInfo&);
  
  // RIT/XIT Control
  Napi::Value SetRit(const Napi::CallbackInfo&);
  Napi::Value GetRit(const Napi::CallbackInfo&);
  Napi::Value SetXit(const Napi::CallbackInfo&);
  Napi::Value GetXit(const Napi::CallbackInfo&);
  Napi::Value ClearRitXit(const Napi::CallbackInfo&);
  
  // Scanning Operations
  Napi::Value StartScan(const Napi::CallbackInfo&);
  Napi::Value StopScan(const Napi::CallbackInfo&);
  
  // Level Controls
  Napi::Value SetLevel(const Napi::CallbackInfo&);
  Napi::Value GetLevel(const Napi::CallbackInfo&);
  Napi::Value GetSupportedLevels(const Napi::CallbackInfo&);
  
  // Function Controls
  Napi::Value SetFunction(const Napi::CallbackInfo&);
  Napi::Value GetFunction(const Napi::CallbackInfo&);
  Napi::Value GetSupportedFunctions(const Napi::CallbackInfo&);
  
  // Split Operations
  Napi::Value SetSplitFreq(const Napi::CallbackInfo&);
  Napi::Value GetSplitFreq(const Napi::CallbackInfo&);
  Napi::Value SetSplitMode(const Napi::CallbackInfo&);
  Napi::Value GetSplitMode(const Napi::CallbackInfo&);
  Napi::Value SetSplit(const Napi::CallbackInfo&);
  Napi::Value GetSplit(const Napi::CallbackInfo&);
  
  // VFO Operations
  Napi::Value VfoOperation(const Napi::CallbackInfo&);
  
  // Antenna Selection
  Napi::Value SetAntenna(const Napi::CallbackInfo&);
  Napi::Value GetAntenna(const Napi::CallbackInfo&);
  
  // Serial Port Configuration
  Napi::Value SetSerialConfig(const Napi::CallbackInfo&);
  Napi::Value GetSerialConfig(const Napi::CallbackInfo&);
  Napi::Value SetPttType(const Napi::CallbackInfo&);
  Napi::Value GetPttType(const Napi::CallbackInfo&);
  Napi::Value SetDcdType(const Napi::CallbackInfo&);
  Napi::Value GetDcdType(const Napi::CallbackInfo&);
  Napi::Value GetSupportedSerialConfigs(const Napi::CallbackInfo&);
  
  // Static method to get supported rig models
  static Napi::Value GetSupportedRigs(const Napi::CallbackInfo&);
  
  static Napi::Function GetClass(Napi::Env);

  static int freq_change_cb(RIG*, vfo_t, freq_t, void*);

 public:
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