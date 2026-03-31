#include "hamlib.h"
#include "shim/hamlib_shim.h"
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <exception>
#include <limits>

// 安全宏 - 检查RIG指针有效性，防止空指针解引用和已销毁对象访问
#define CHECK_RIG_VALID() \
  do { \
    if (!hamlib_instance_ || !hamlib_instance_->my_rig) { \
      result_code_ = SHIM_RIG_EINVAL; \
      error_message_ = "RIG is not initialized or has been destroyed"; \
      return; \
    } \
  } while(0)

#define RETURN_NULL_IF_INVALID_VFO(vfo) \
  do { \
    if ((vfo) == kInvalidVfoParameter) { \
      return env.Null(); \
    } \
  } while(0)

#define RETURN_NULL_IF_RIG_HANDLE_INVALID() \
  do { \
    if (!my_rig) { \
      Napi::Error::New(env, "RIG is not initialized or has been destroyed").ThrowAsJavaScriptException(); \
      return env.Null(); \
    } \
  } while(0)

// Structure to hold rig information for the callback
struct RigListData {
  std::vector<Napi::Object> rigList;
  Napi::Env env;
};

using namespace Napi;

Napi::FunctionReference NodeHamLib::constructor;
Napi::ThreadSafeFunction tsfn;

constexpr int kInvalidVfoParameter = std::numeric_limits<int>::min();

static std::string publicVfoToken(int vfo) {
  const char* rawToken = shim_rig_strvfo(vfo);
  if (!rawToken || !*rawToken) {
    return "UNKNOWN";
  }
  return rawToken;
}

static int parseVfoString(Napi::Env env, const std::string& vfoToken) {
  int vfo = shim_rig_parse_vfo(vfoToken.c_str());
  if (vfo == SHIM_RIG_VFO_NONE) {
    Napi::TypeError::New(env, "Invalid Hamlib VFO token: " + vfoToken).ThrowAsJavaScriptException();
    return kInvalidVfoParameter;
  }
  return vfo;
}

// Base AsyncWorker implementation with Promise support
HamLibAsyncWorker::HamLibAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance)
    : AsyncWorker(env), hamlib_instance_(hamlib_instance), result_code_(0), error_message_(""), deferred_(Napi::Promise::Deferred::New(env)) {}

// Specific AsyncWorker classes for each operation
class OpenAsyncWorker : public HamLibAsyncWorker {
public:
    OpenAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance)
        : HamLibAsyncWorker(env, hamlib_instance) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_open(hamlib_instance_->my_rig);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        } else {
            shim_rig_set_freq_callback(hamlib_instance_->my_rig, NodeHamLib::freq_change_cb, hamlib_instance_);
            auto ppt_cb = +[](void* handle, int vfo, int ptt, void* arg) -> int {
                (void)handle;
                (void)vfo;
                (void)ptt;
                (void)arg;
                return 0;
            };
            shim_rig_set_ptt_callback(hamlib_instance_->my_rig, ppt_cb, NULL);
            shim_rig_set_trn(hamlib_instance_->my_rig, SHIM_RIG_TRN_POLL);
            hamlib_instance_->rig_is_open = true;
        }
    }

    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
};

class SetFrequencyAsyncWorker : public HamLibAsyncWorker {
public:
    SetFrequencyAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, double freq, int vfo)
        : HamLibAsyncWorker(env, hamlib_instance), freq_(freq), vfo_(vfo) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_set_freq(hamlib_instance_->my_rig, vfo_, freq_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    double freq_;
    int vfo_;
};

class GetFrequencyAsyncWorker : public HamLibAsyncWorker {
public:
    GetFrequencyAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), freq_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_get_freq(hamlib_instance_->my_rig, vfo_, &freq_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, freq_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
    double freq_;
};

class SetModeAsyncWorker : public HamLibAsyncWorker {
public:
    SetModeAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int mode, int width, int vfo = SHIM_RIG_VFO_CURR)
        : HamLibAsyncWorker(env, hamlib_instance), mode_(mode), width_(width), vfo_(vfo) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_set_mode(hamlib_instance_->my_rig, vfo_, mode_, width_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int mode_;
    int width_;
    int vfo_;
};

class GetModeAsyncWorker : public HamLibAsyncWorker {
public:
    GetModeAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance)
        : HamLibAsyncWorker(env, hamlib_instance), mode_(0), width_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_get_mode(hamlib_instance_->my_rig, SHIM_RIG_VFO_CURR, &mode_, &width_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            Napi::Object obj = Napi::Object::New(env);
            // Convert mode to string using rig_strrmode
            const char* mode_str = shim_rig_strrmode(mode_);
            obj.Set(Napi::String::New(env, "mode"), Napi::String::New(env, mode_str));
            obj.Set(Napi::String::New(env, "bandwidth"), width_);
            deferred_.Resolve(obj);
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int mode_;
    int width_;
};

class SetPttAsyncWorker : public HamLibAsyncWorker {
public:
    SetPttAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int ptt)
        : HamLibAsyncWorker(env, hamlib_instance), ptt_(ptt) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_set_ptt(hamlib_instance_->my_rig, SHIM_RIG_VFO_CURR, ptt_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int ptt_;
};

class GetStrengthAsyncWorker : public HamLibAsyncWorker {
public:
    GetStrengthAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), strength_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_get_strength(hamlib_instance_->my_rig, vfo_, &strength_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, strength_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
    int strength_;
};

class SetLevelAsyncWorker : public HamLibAsyncWorker {
public:
    SetLevelAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, uint64_t level_type, float value, int vfo)
        : HamLibAsyncWorker(env, hamlib_instance), level_type_(level_type), value_(value), vfo_(vfo) {}

    void Execute() override {
        CHECK_RIG_VALID();

        if (shim_rig_level_is_float(level_type_)) {
            result_code_ = shim_rig_set_level_f(hamlib_instance_->my_rig, vfo_, level_type_, value_);
        } else {
            result_code_ = shim_rig_set_level_i(hamlib_instance_->my_rig, vfo_, level_type_, static_cast<int>(value_));
        }
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }

    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }

    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }

private:
    uint64_t level_type_;
    float value_;
    int vfo_;
};

class GetLevelAsyncWorker : public HamLibAsyncWorker {
public:
    GetLevelAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, uint64_t level_type, int vfo = SHIM_RIG_VFO_CURR)
        : HamLibAsyncWorker(env, hamlib_instance), level_type_(level_type), vfo_(vfo), value_(0.0) {}

    void Execute() override {
        CHECK_RIG_VALID();

        // Use auto-detect to handle int levels (STRENGTH, RAWSTR) and
        // float levels (SWR, ALC, RFPOWER_METER, etc.) correctly
        result_code_ = shim_rig_get_level_auto(hamlib_instance_->my_rig, vfo_, level_type_, &value_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }

    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, value_));
        }
    }

    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }

private:
    uint64_t level_type_;
    int vfo_;
    double value_;
};

class SetFunctionAsyncWorker : public HamLibAsyncWorker {
public:
    SetFunctionAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, uint64_t func_type, int enable)
        : HamLibAsyncWorker(env, hamlib_instance), func_type_(func_type), enable_(enable) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_set_func(hamlib_instance_->my_rig, SHIM_RIG_VFO_CURR, func_type_, enable_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    uint64_t func_type_;
    int enable_;
};

class GetFunctionAsyncWorker : public HamLibAsyncWorker {
public:
    GetFunctionAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, uint64_t func_type)
        : HamLibAsyncWorker(env, hamlib_instance), func_type_(func_type), state_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_get_func(hamlib_instance_->my_rig, SHIM_RIG_VFO_CURR, func_type_, &state_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Boolean::New(env, state_ != 0));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    uint64_t func_type_;
    int state_;
};

class SetMemoryChannelAsyncWorker : public HamLibAsyncWorker {
public:
    SetMemoryChannelAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, const shim_channel_t& chan)
        : HamLibAsyncWorker(env, hamlib_instance), chan_(chan) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_set_channel(hamlib_instance_->my_rig, SHIM_RIG_VFO_MEM, &chan_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    shim_channel_t chan_;
};

class GetMemoryChannelAsyncWorker : public HamLibAsyncWorker {
public:
    GetMemoryChannelAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int channel_num, bool read_only)
        : HamLibAsyncWorker(env, hamlib_instance), channel_num_(channel_num), read_only_(read_only) {
        memset(&chan_, 0, sizeof(chan_));
    }
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        chan_.channel_num = channel_num_;
        chan_.vfo = SHIM_RIG_VFO_MEM;
        result_code_ = shim_rig_get_channel(hamlib_instance_->my_rig, SHIM_RIG_VFO_MEM, &chan_, read_only_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        Napi::Object obj = Napi::Object::New(env);
        
        obj.Set(Napi::String::New(env, "channelNumber"), chan_.channel_num);
        obj.Set(Napi::String::New(env, "frequency"), chan_.freq);
        
        if (chan_.mode != SHIM_RIG_MODE_NONE) {
            const char* mode_str = shim_rig_strrmode(chan_.mode);
            obj.Set(Napi::String::New(env, "mode"), Napi::String::New(env, mode_str));
        }
        
        obj.Set(Napi::String::New(env, "bandwidth"), chan_.width);
        
        if (chan_.channel_desc[0] != '\0') {
            obj.Set(Napi::String::New(env, "description"), Napi::String::New(env, chan_.channel_desc));
        }
        
        if (chan_.split == SHIM_RIG_SPLIT_ON) {
            obj.Set(Napi::String::New(env, "txFrequency"), chan_.tx_freq);
        }
        
        if (chan_.ctcss_tone != 0) {
            obj.Set(Napi::String::New(env, "ctcssTone"), chan_.ctcss_tone);
        }
        
        deferred_.Resolve(obj);
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int channel_num_;
    bool read_only_;
    shim_channel_t chan_;
};

class SelectMemoryChannelAsyncWorker : public HamLibAsyncWorker {
public:
    SelectMemoryChannelAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int channel_num)
        : HamLibAsyncWorker(env, hamlib_instance), channel_num_(channel_num) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_set_mem(hamlib_instance_->my_rig, SHIM_RIG_VFO_CURR, channel_num_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int channel_num_;
};

class SetRitAsyncWorker : public HamLibAsyncWorker {
public:
    SetRitAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int rit_offset)
        : HamLibAsyncWorker(env, hamlib_instance), rit_offset_(rit_offset) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_set_rit(hamlib_instance_->my_rig, SHIM_RIG_VFO_CURR, rit_offset_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int rit_offset_;
};

class GetRitAsyncWorker : public HamLibAsyncWorker {
public:
    GetRitAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance)
        : HamLibAsyncWorker(env, hamlib_instance), rit_offset_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_get_rit(hamlib_instance_->my_rig, SHIM_RIG_VFO_CURR, &rit_offset_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, rit_offset_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int rit_offset_;
};

class SetXitAsyncWorker : public HamLibAsyncWorker {
public:
    SetXitAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int xit_offset)
        : HamLibAsyncWorker(env, hamlib_instance), xit_offset_(xit_offset) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_set_xit(hamlib_instance_->my_rig, SHIM_RIG_VFO_CURR, xit_offset_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int xit_offset_;
};

class GetXitAsyncWorker : public HamLibAsyncWorker {
public:
    GetXitAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance)
        : HamLibAsyncWorker(env, hamlib_instance), xit_offset_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_get_xit(hamlib_instance_->my_rig, SHIM_RIG_VFO_CURR, &xit_offset_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, xit_offset_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int xit_offset_;
};

class ClearRitXitAsyncWorker : public HamLibAsyncWorker {
public:
    ClearRitXitAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance)
        : HamLibAsyncWorker(env, hamlib_instance) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        int ritCode = shim_rig_set_rit(hamlib_instance_->my_rig, SHIM_RIG_VFO_CURR, 0);
        int xitCode = shim_rig_set_xit(hamlib_instance_->my_rig, SHIM_RIG_VFO_CURR, 0);
        
        if (ritCode != SHIM_RIG_OK) {
            result_code_ = ritCode;
            error_message_ = shim_rigerror(ritCode);
        } else if (xitCode != SHIM_RIG_OK) {
            result_code_ = xitCode;
            error_message_ = shim_rigerror(xitCode);
        } else {
            result_code_ = SHIM_RIG_OK;
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
};

class SetSplitFreqAsyncWorker : public HamLibAsyncWorker {
public:
    SetSplitFreqAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, double tx_freq, int vfo = SHIM_RIG_VFO_CURR)
        : HamLibAsyncWorker(env, hamlib_instance), tx_freq_(tx_freq), vfo_(vfo) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_set_split_freq(hamlib_instance_->my_rig, vfo_, tx_freq_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    double tx_freq_;
    int vfo_;
};

class GetSplitFreqAsyncWorker : public HamLibAsyncWorker {
public:
    GetSplitFreqAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo = SHIM_RIG_VFO_CURR)
        : HamLibAsyncWorker(env, hamlib_instance), tx_freq_(0), vfo_(vfo) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_get_split_freq(hamlib_instance_->my_rig, vfo_, &tx_freq_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, tx_freq_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    double tx_freq_;
    int vfo_;
};

class SetSplitAsyncWorker : public HamLibAsyncWorker {
public:
    SetSplitAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int rx_vfo, int split, int tx_vfo)
        : HamLibAsyncWorker(env, hamlib_instance), rx_vfo_(rx_vfo), split_(split), tx_vfo_(tx_vfo) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_set_split_vfo(hamlib_instance_->my_rig, rx_vfo_, split_, tx_vfo_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int rx_vfo_;
    int split_;
    int tx_vfo_;
};

class GetSplitAsyncWorker : public HamLibAsyncWorker {
public:
    GetSplitAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo = SHIM_RIG_VFO_CURR)
        : HamLibAsyncWorker(env, hamlib_instance), split_(SHIM_RIG_SPLIT_OFF), tx_vfo_(SHIM_RIG_VFO_CURR), vfo_(vfo) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_get_split_vfo(hamlib_instance_->my_rig, vfo_, &split_, &tx_vfo_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            Napi::Object obj = Napi::Object::New(env);
            obj.Set(Napi::String::New(env, "enabled"), Napi::Boolean::New(env, split_ == SHIM_RIG_SPLIT_ON));
            obj.Set(Napi::String::New(env, "txVfo"), Napi::String::New(env, publicVfoToken(tx_vfo_)));
            
            deferred_.Resolve(obj);
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int split_;
    int tx_vfo_;
    int vfo_;
};

class SetVfoAsyncWorker : public HamLibAsyncWorker {
public:
    SetVfoAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_set_vfo(hamlib_instance_->my_rig, vfo_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
};

class GetVfoAsyncWorker : public HamLibAsyncWorker {
public:
    GetVfoAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_get_vfo(hamlib_instance_->my_rig, &vfo_);
        if (result_code_ != SHIM_RIG_OK) {
            // 提供更清晰的错误信息
            switch (result_code_) {
                case SHIM_RIG_ENAVAIL:
                    error_message_ = "VFO query not supported by this radio";
                    break;
                case SHIM_RIG_EIO:
                    error_message_ = "I/O error during VFO query";
                    break;
                case SHIM_RIG_ETIMEOUT:
                    error_message_ = "Timeout during VFO query";
                    break;
                case SHIM_RIG_EPROTO:
                    error_message_ = "Protocol error during VFO query";
                    break;
                default:
                    error_message_ = "VFO query failed with code " + std::to_string(result_code_);
                    break;
            }
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        
        // 如果API调用失败，reject Promise
        if (result_code_ != SHIM_RIG_OK) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
            return;
        }
        
        deferred_.Resolve(Napi::String::New(env, publicVfoToken(vfo_)));
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
};

class CloseAsyncWorker : public HamLibAsyncWorker {
public:
    CloseAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance)
        : HamLibAsyncWorker(env, hamlib_instance) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        // 检查rig是否已经关闭
        if (!hamlib_instance_->rig_is_open) {
            result_code_ = SHIM_RIG_OK;  // 已经关闭，返回成功
            return;
        }
        
        result_code_ = shim_rig_close(hamlib_instance_->my_rig);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        } else {
            hamlib_instance_->rig_is_open = false;
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
};

class DestroyAsyncWorker : public HamLibAsyncWorker {
public:
    DestroyAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance)
        : HamLibAsyncWorker(env, hamlib_instance) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        // rig_cleanup会自动调用rig_close，所以我们不需要重复调用
        result_code_ = shim_rig_cleanup(hamlib_instance_->my_rig);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        } else {
            hamlib_instance_->rig_is_open = false;
            hamlib_instance_->my_rig = nullptr;  // 重要：清空指针防止重复释放
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
};

// Start Scan AsyncWorker
class StartScanAsyncWorker : public HamLibAsyncWorker {
public:
    StartScanAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int intype, int channel)
        : HamLibAsyncWorker(env, hamlib_instance), intype_(intype), channel_(channel) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_scan(hamlib_instance_->my_rig, SHIM_RIG_VFO_CURR, intype_, channel_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int intype_;
    int channel_;
};

// Stop Scan AsyncWorker
class StopScanAsyncWorker : public HamLibAsyncWorker {
public:
    StopScanAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance)
        : HamLibAsyncWorker(env, hamlib_instance) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_scan(hamlib_instance_->my_rig, SHIM_RIG_VFO_CURR, SHIM_RIG_SCAN_STOP, 0);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
};

// VFO Operation AsyncWorker
class VfoOperationAsyncWorker : public HamLibAsyncWorker {
public:
    VfoOperationAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo_op)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_op_(vfo_op) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_vfo_op(hamlib_instance_->my_rig, SHIM_RIG_VFO_CURR, vfo_op_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_op_;
};

// Set Antenna AsyncWorker
class SetAntennaAsyncWorker : public HamLibAsyncWorker {
public:
    SetAntennaAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int antenna, int vfo, float option)
        : HamLibAsyncWorker(env, hamlib_instance), antenna_(antenna), vfo_(vfo), option_(option) {}

    void Execute() override {
        CHECK_RIG_VALID();

        result_code_ = shim_rig_set_ant(hamlib_instance_->my_rig, vfo_, antenna_, option_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }

    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }

    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }

private:
    int antenna_;
    int vfo_;
    float option_;
};

// Get Antenna AsyncWorker
class GetAntennaAsyncWorker : public HamLibAsyncWorker {
public:
    GetAntennaAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo, int antenna)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), antenna_(antenna), antenna_curr_(0), antenna_tx_(0), antenna_rx_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        option_ = 0.0f;

        result_code_ = shim_rig_get_ant(hamlib_instance_->my_rig, vfo_, antenna_, &option_, &antenna_curr_, &antenna_tx_, &antenna_rx_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }

    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            Napi::Object result = Napi::Object::New(env);

            result.Set("currentAntenna", Napi::Number::New(env, antenna_curr_));
            result.Set("txAntenna", Napi::Number::New(env, antenna_tx_));
            result.Set("rxAntenna", Napi::Number::New(env, antenna_rx_));
            result.Set("option", Napi::Number::New(env, option_));

            deferred_.Resolve(result);
        }
    }

    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }

private:
    int vfo_;
    int antenna_;
    int antenna_curr_;
    int antenna_tx_;
    int antenna_rx_;
    float option_;
};

// Power to mW Conversion AsyncWorker
class Power2mWAsyncWorker : public HamLibAsyncWorker {
public:
    Power2mWAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, float power, double freq, int mode)
        : HamLibAsyncWorker(env, hamlib_instance), power_(power), freq_(freq), mode_(mode), mwpower_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_power2mW(hamlib_instance_->my_rig, &mwpower_, power_, freq_, mode_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, mwpower_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    float power_;
    double freq_;
    int mode_;
    unsigned int mwpower_;
};

// mW to Power Conversion AsyncWorker
class MW2PowerAsyncWorker : public HamLibAsyncWorker {
public:
    MW2PowerAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, unsigned int mwpower, double freq, int mode)
        : HamLibAsyncWorker(env, hamlib_instance), mwpower_(mwpower), freq_(freq), mode_(mode), power_(0.0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_mW2power(hamlib_instance_->my_rig, &power_, mwpower_, freq_, mode_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, power_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    unsigned int mwpower_;
    double freq_;
    int mode_;
    float power_;
};

// Set Split Mode AsyncWorker
class SetSplitModeAsyncWorker : public HamLibAsyncWorker {
public:
    SetSplitModeAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int tx_mode, int tx_width, int vfo = SHIM_RIG_VFO_CURR)
        : HamLibAsyncWorker(env, hamlib_instance), tx_mode_(tx_mode), tx_width_(tx_width), vfo_(vfo) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_set_split_mode(hamlib_instance_->my_rig, vfo_, tx_mode_, tx_width_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int tx_mode_;
    int tx_width_;
    int vfo_;
};

// Get Split Mode AsyncWorker
class GetSplitModeAsyncWorker : public HamLibAsyncWorker {
public:
    GetSplitModeAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo = SHIM_RIG_VFO_CURR)
        : HamLibAsyncWorker(env, hamlib_instance), tx_mode_(0), tx_width_(0), vfo_(vfo) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_get_split_mode(hamlib_instance_->my_rig, vfo_, &tx_mode_, &tx_width_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            Napi::Object obj = Napi::Object::New(env);
            // Convert mode to string using rig_strrmode
            const char* mode_str = shim_rig_strrmode(tx_mode_);
            obj.Set(Napi::String::New(env, "mode"), Napi::String::New(env, mode_str));
            obj.Set(Napi::String::New(env, "width"), tx_width_);
            deferred_.Resolve(obj);
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int tx_mode_;
    int tx_width_;
    int vfo_;
};

// Set Serial Config AsyncWorker
class SetSerialConfigAsyncWorker : public HamLibAsyncWorker {
public:
    SetSerialConfigAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, const std::string& param_name, const std::string& param_value)
        : HamLibAsyncWorker(env, hamlib_instance), param_name_(param_name), param_value_(param_value) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        // Apply serial configuration parameter
        if (param_name_ == "data_bits") {
            int data_bits = std::stoi(param_value_);
            shim_rig_set_serial_data_bits(hamlib_instance_->my_rig, data_bits);
            result_code_ = SHIM_RIG_OK;
        } else if (param_name_ == "stop_bits") {
            int stop_bits = std::stoi(param_value_);
            shim_rig_set_serial_stop_bits(hamlib_instance_->my_rig, stop_bits);
            result_code_ = SHIM_RIG_OK;
        } else if (param_name_ == "serial_parity") {
            int parity;
            if (param_value_ == "None") {
                parity = SHIM_RIG_PARITY_NONE;
            } else if (param_value_ == "Even") {
                parity = SHIM_RIG_PARITY_EVEN;
            } else if (param_value_ == "Odd") {
                parity = SHIM_RIG_PARITY_ODD;
            } else {
                result_code_ = SHIM_RIG_EINVAL;
                error_message_ = "Invalid parity value";
                return;
            }
            shim_rig_set_serial_parity(hamlib_instance_->my_rig, parity);
            result_code_ = SHIM_RIG_OK;
        } else if (param_name_ == "serial_handshake") {
            int handshake;
            if (param_value_ == "None") {
                handshake = SHIM_RIG_HANDSHAKE_NONE;
            } else if (param_value_ == "Hardware") {
                handshake = SHIM_RIG_HANDSHAKE_HARDWARE;
            } else if (param_value_ == "Software") {
                handshake = SHIM_RIG_HANDSHAKE_XONXOFF;
            } else {
                result_code_ = SHIM_RIG_EINVAL;
                error_message_ = "Invalid handshake value";
                return;
            }
            shim_rig_set_serial_handshake(hamlib_instance_->my_rig, handshake);
            result_code_ = SHIM_RIG_OK;
        } else if (param_name_ == "rts_state") {
            int state;
            if (param_value_ == "ON") {
                state = SHIM_RIG_SIGNAL_ON;
            } else if (param_value_ == "OFF") {
                state = SHIM_RIG_SIGNAL_OFF;
            } else {
                result_code_ = SHIM_RIG_EINVAL;
                error_message_ = "Invalid RTS state value";
                return;
            }
            shim_rig_set_serial_rts_state(hamlib_instance_->my_rig, state);
            result_code_ = SHIM_RIG_OK;
        } else if (param_name_ == "dtr_state") {
            int state;
            if (param_value_ == "ON") {
                state = SHIM_RIG_SIGNAL_ON;
            } else if (param_value_ == "OFF") {
                state = SHIM_RIG_SIGNAL_OFF;
            } else {
                result_code_ = SHIM_RIG_EINVAL;
                error_message_ = "Invalid DTR state value";
                return;
            }
            shim_rig_set_serial_dtr_state(hamlib_instance_->my_rig, state);
            result_code_ = SHIM_RIG_OK;
        } else if (param_name_ == "rate") {
            int rate = std::stoi(param_value_);
            // Validate supported baud rates based on common values in Hamlib
            std::vector<int> valid_rates = {150, 300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200,
                                          230400, 460800, 500000, 576000, 921600, 1000000, 1152000, 1500000,
                                          2000000, 2500000, 3000000, 3500000, 4000000};
            if (std::find(valid_rates.begin(), valid_rates.end(), rate) != valid_rates.end()) {
                shim_rig_set_serial_rate(hamlib_instance_->my_rig, rate);
                result_code_ = SHIM_RIG_OK;
            } else {
                result_code_ = SHIM_RIG_EINVAL;
                error_message_ = "Invalid baud rate value";
            }
        } else if (param_name_ == "timeout") {
            int timeout_val = std::stoi(param_value_);
            if (timeout_val >= 0) {
                shim_rig_set_port_timeout(hamlib_instance_->my_rig, timeout_val);
                result_code_ = SHIM_RIG_OK;
            } else {
                result_code_ = SHIM_RIG_EINVAL;
                error_message_ = "Timeout must be non-negative";
            }
        } else if (param_name_ == "retry") {
            int retry_val = std::stoi(param_value_);
            if (retry_val >= 0) {
                shim_rig_set_port_retry(hamlib_instance_->my_rig, retry_val);
                result_code_ = SHIM_RIG_OK;
            } else {
                result_code_ = SHIM_RIG_EINVAL;
                error_message_ = "Retry count must be non-negative";
            }
        } else if (param_name_ == "write_delay") {
            int delay = std::stoi(param_value_);
            if (delay >= 0) {
                shim_rig_set_port_write_delay(hamlib_instance_->my_rig, delay);
                result_code_ = SHIM_RIG_OK;
            } else {
                result_code_ = SHIM_RIG_EINVAL;
                error_message_ = "Write delay must be non-negative";
            }
        } else if (param_name_ == "post_write_delay") {
            int delay = std::stoi(param_value_);
            if (delay >= 0) {
                shim_rig_set_port_post_write_delay(hamlib_instance_->my_rig, delay);
                result_code_ = SHIM_RIG_OK;
            } else {
                result_code_ = SHIM_RIG_EINVAL;
                error_message_ = "Post write delay must be non-negative";
            }
        } else if (param_name_ == "flushx") {
            if (param_value_ == "true" || param_value_ == "1") {
                shim_rig_set_port_flushx(hamlib_instance_->my_rig, 1);
                result_code_ = SHIM_RIG_OK;
            } else if (param_value_ == "false" || param_value_ == "0") {
                shim_rig_set_port_flushx(hamlib_instance_->my_rig, 0);
                result_code_ = SHIM_RIG_OK;
            } else {
                result_code_ = SHIM_RIG_EINVAL;
                error_message_ = "Flushx must be true/false or 1/0";
            }
        } else {
            result_code_ = SHIM_RIG_EINVAL;
            error_message_ = "Unknown serial configuration parameter";
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    std::string param_name_;
    std::string param_value_;
};

// Get Serial Config AsyncWorker
class GetSerialConfigAsyncWorker : public HamLibAsyncWorker {
public:
    GetSerialConfigAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, const std::string& param_name)
        : HamLibAsyncWorker(env, hamlib_instance), param_name_(param_name) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        // Get serial configuration parameter
        if (param_name_ == "data_bits") {
            param_value_ = std::to_string(shim_rig_get_serial_data_bits(hamlib_instance_->my_rig));
            result_code_ = SHIM_RIG_OK;
        } else if (param_name_ == "stop_bits") {
            param_value_ = std::to_string(shim_rig_get_serial_stop_bits(hamlib_instance_->my_rig));
            result_code_ = SHIM_RIG_OK;
        } else if (param_name_ == "serial_parity") {
            switch (shim_rig_get_serial_parity(hamlib_instance_->my_rig)) {
                case SHIM_RIG_PARITY_NONE:
                    param_value_ = "None";
                    break;
                case SHIM_RIG_PARITY_EVEN:
                    param_value_ = "Even";
                    break;
                case SHIM_RIG_PARITY_ODD:
                    param_value_ = "Odd";
                    break;
                default:
                    param_value_ = "Unknown";
                    break;
            }
            result_code_ = SHIM_RIG_OK;
        } else if (param_name_ == "serial_handshake") {
            switch (shim_rig_get_serial_handshake(hamlib_instance_->my_rig)) {
                case SHIM_RIG_HANDSHAKE_NONE:
                    param_value_ = "None";
                    break;
                case SHIM_RIG_HANDSHAKE_HARDWARE:
                    param_value_ = "Hardware";
                    break;
                case SHIM_RIG_HANDSHAKE_XONXOFF:
                    param_value_ = "Software";
                    break;
                default:
                    param_value_ = "Unknown";
                    break;
            }
            result_code_ = SHIM_RIG_OK;
        } else if (param_name_ == "rts_state") {
            switch (shim_rig_get_serial_rts_state(hamlib_instance_->my_rig)) {
                case SHIM_RIG_SIGNAL_ON:
                    param_value_ = "ON";
                    break;
                case SHIM_RIG_SIGNAL_OFF:
                    param_value_ = "OFF";
                    break;
                default:
                    param_value_ = "Unknown";
                    break;
            }
            result_code_ = SHIM_RIG_OK;
        } else if (param_name_ == "dtr_state") {
            switch (shim_rig_get_serial_dtr_state(hamlib_instance_->my_rig)) {
                case SHIM_RIG_SIGNAL_ON:
                    param_value_ = "ON";
                    break;
                case SHIM_RIG_SIGNAL_OFF:
                    param_value_ = "OFF";
                    break;
                default:
                    param_value_ = "Unknown";
                    break;
            }
            result_code_ = SHIM_RIG_OK;
        } else if (param_name_ == "rate") {
            param_value_ = std::to_string(shim_rig_get_serial_rate(hamlib_instance_->my_rig));
            result_code_ = SHIM_RIG_OK;
        } else if (param_name_ == "timeout") {
            param_value_ = std::to_string(shim_rig_get_port_timeout(hamlib_instance_->my_rig));
            result_code_ = SHIM_RIG_OK;
        } else if (param_name_ == "retry") {
            param_value_ = std::to_string(shim_rig_get_port_retry(hamlib_instance_->my_rig));
            result_code_ = SHIM_RIG_OK;
        } else if (param_name_ == "write_delay") {
            param_value_ = std::to_string(shim_rig_get_port_write_delay(hamlib_instance_->my_rig));
            result_code_ = SHIM_RIG_OK;
        } else if (param_name_ == "post_write_delay") {
            param_value_ = std::to_string(shim_rig_get_port_post_write_delay(hamlib_instance_->my_rig));
            result_code_ = SHIM_RIG_OK;
        } else if (param_name_ == "flushx") {
            param_value_ = shim_rig_get_port_flushx(hamlib_instance_->my_rig) ? "true" : "false";
            result_code_ = SHIM_RIG_OK;
        } else {
            result_code_ = SHIM_RIG_EINVAL;
            error_message_ = "Unknown serial configuration parameter";
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::String::New(env, param_value_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    std::string param_name_;
    std::string param_value_;
};

// Set PTT Type AsyncWorker
class SetPttTypeAsyncWorker : public HamLibAsyncWorker {
public:
    SetPttTypeAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, const std::string& intype)
        : HamLibAsyncWorker(env, hamlib_instance), intype_(intype) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        int intype;
        if (intype_ == "RIG") {
            intype = SHIM_RIG_PTT_RIG;
        } else if (intype_ == "DTR") {
            intype = SHIM_RIG_PTT_SERIAL_DTR;
        } else if (intype_ == "RTS") {
            intype = SHIM_RIG_PTT_SERIAL_RTS;
        } else if (intype_ == "PARALLEL") {
            intype = SHIM_RIG_PTT_PARALLEL;
        } else if (intype_ == "CM108") {
            intype = SHIM_RIG_PTT_CM108;
        } else if (intype_ == "GPIO") {
            intype = SHIM_RIG_PTT_GPIO;
        } else if (intype_ == "GPION") {
            intype = SHIM_RIG_PTT_GPION;
        } else if (intype_ == "NONE") {
            intype = SHIM_RIG_PTT_NONE;
        } else {
            result_code_ = SHIM_RIG_EINVAL;
            error_message_ = "Invalid PTT type";
            return;
        }
        
        shim_rig_set_ptt_type(hamlib_instance_->my_rig, intype);
        result_code_ = SHIM_RIG_OK;
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    std::string intype_;
};

// Get PTT Type AsyncWorker
class GetPttTypeAsyncWorker : public HamLibAsyncWorker {
public:
    GetPttTypeAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance)
        : HamLibAsyncWorker(env, hamlib_instance) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        int intype = shim_rig_get_ptt_type(hamlib_instance_->my_rig);
        
        switch (intype) {
            case SHIM_RIG_PTT_RIG:
                intype_str_ = "RIG";
                break;
            case SHIM_RIG_PTT_SERIAL_DTR:
                intype_str_ = "DTR";
                break;
            case SHIM_RIG_PTT_SERIAL_RTS:
                intype_str_ = "RTS";
                break;
            case SHIM_RIG_PTT_PARALLEL:
                intype_str_ = "PARALLEL";
                break;
            case SHIM_RIG_PTT_CM108:
                intype_str_ = "CM108";
                break;
            case SHIM_RIG_PTT_GPIO:
                intype_str_ = "GPIO";
                break;
            case SHIM_RIG_PTT_GPION:
                intype_str_ = "GPION";
                break;
            case SHIM_RIG_PTT_NONE:
                intype_str_ = "NONE";
                break;
            default:
                intype_str_ = "Unknown";
                break;
        }
        result_code_ = SHIM_RIG_OK;
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::String::New(env, intype_str_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    std::string intype_str_;
};

// Set DCD Type AsyncWorker
class SetDcdTypeAsyncWorker : public HamLibAsyncWorker {
public:
    SetDcdTypeAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, const std::string& intype)
        : HamLibAsyncWorker(env, hamlib_instance), intype_(intype) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        int intype;
        if (intype_ == "RIG") {
            intype = SHIM_RIG_DCD_RIG;
        } else if (intype_ == "DSR") {
            intype = SHIM_RIG_DCD_SERIAL_DSR;
        } else if (intype_ == "CTS") {
            intype = SHIM_RIG_DCD_SERIAL_CTS;
        } else if (intype_ == "CD") {
            intype = SHIM_RIG_DCD_SERIAL_CAR;
        } else if (intype_ == "PARALLEL") {
            intype = SHIM_RIG_DCD_PARALLEL;
        } else if (intype_ == "CM108") {
            intype = SHIM_RIG_DCD_CM108;
        } else if (intype_ == "GPIO") {
            intype = SHIM_RIG_DCD_GPIO;
        } else if (intype_ == "GPION") {
            intype = SHIM_RIG_DCD_GPION;
        } else if (intype_ == "NONE") {
            intype = SHIM_RIG_DCD_NONE;
        } else {
            result_code_ = SHIM_RIG_EINVAL;
            error_message_ = "Invalid DCD type";
            return;
        }
        
        shim_rig_set_dcd_type(hamlib_instance_->my_rig, intype);
        result_code_ = SHIM_RIG_OK;
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    std::string intype_;
};

// Get DCD Type AsyncWorker
class GetDcdTypeAsyncWorker : public HamLibAsyncWorker {
public:
    GetDcdTypeAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance)
        : HamLibAsyncWorker(env, hamlib_instance) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        int intype = shim_rig_get_dcd_type(hamlib_instance_->my_rig);
        
        switch (intype) {
            case SHIM_RIG_DCD_RIG:
                intype_str_ = "RIG";
                break;
            case SHIM_RIG_DCD_SERIAL_DSR:
                intype_str_ = "DSR";
                break;
            case SHIM_RIG_DCD_SERIAL_CTS:
                intype_str_ = "CTS";
                break;
            case SHIM_RIG_DCD_SERIAL_CAR:
                intype_str_ = "CD";
                break;
            case SHIM_RIG_DCD_PARALLEL:
                intype_str_ = "PARALLEL";
                break;
            case SHIM_RIG_DCD_CM108:
                intype_str_ = "CM108";
                break;
            case SHIM_RIG_DCD_GPIO:
                intype_str_ = "GPIO";
                break;
            case SHIM_RIG_DCD_GPION:
                intype_str_ = "GPION";
                break;
            case SHIM_RIG_DCD_NONE:
                intype_str_ = "NONE";
                break;
            default:
                intype_str_ = "Unknown";
                break;
        }
        result_code_ = SHIM_RIG_OK;
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::String::New(env, intype_str_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    std::string intype_str_;
};

// Power Control AsyncWorker classes
class SetPowerstatAsyncWorker : public HamLibAsyncWorker {
public:
    SetPowerstatAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int status)
        : HamLibAsyncWorker(env, hamlib_instance), status_(status) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_set_powerstat(hamlib_instance_->my_rig, status_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int status_;
};

class GetPowerstatAsyncWorker : public HamLibAsyncWorker {
public:
    GetPowerstatAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance)
        : HamLibAsyncWorker(env, hamlib_instance), status_(SHIM_RIG_POWER_UNKNOWN) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_get_powerstat(hamlib_instance_->my_rig, &status_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, static_cast<int>(status_)));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int status_;
};

// PTT Status Detection AsyncWorker
class GetPttAsyncWorker : public HamLibAsyncWorker {
public:
    GetPttAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), ptt_(SHIM_RIG_PTT_OFF) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_get_ptt(hamlib_instance_->my_rig, vfo_, &ptt_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Boolean::New(env, ptt_ == SHIM_RIG_PTT_ON));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
    int ptt_;
};

// Data Carrier Detect AsyncWorker
class GetDcdAsyncWorker : public HamLibAsyncWorker {
public:
    GetDcdAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), dcd_(SHIM_RIG_DCD_OFF) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_get_dcd(hamlib_instance_->my_rig, vfo_, &dcd_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Boolean::New(env, dcd_ == SHIM_RIG_DCD_ON));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
    int dcd_;
};

// Tuning Step Control AsyncWorker classes
class SetTuningStepAsyncWorker : public HamLibAsyncWorker {
public:
    SetTuningStepAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo, int ts)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), ts_(ts) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_set_ts(hamlib_instance_->my_rig, vfo_, ts_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
    int ts_;
};

class GetTuningStepAsyncWorker : public HamLibAsyncWorker {
public:
    GetTuningStepAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), ts_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_get_ts(hamlib_instance_->my_rig, vfo_, &ts_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, ts_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
    int ts_;
};

// Repeater Control AsyncWorker classes
class SetRepeaterShiftAsyncWorker : public HamLibAsyncWorker {
public:
    SetRepeaterShiftAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo, int shift)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), shift_(shift) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_set_rptr_shift(hamlib_instance_->my_rig, vfo_, shift_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
    int shift_;
};

class GetRepeaterShiftAsyncWorker : public HamLibAsyncWorker {
public:
    GetRepeaterShiftAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), shift_(SHIM_RIG_RPT_SHIFT_NONE) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_get_rptr_shift(hamlib_instance_->my_rig, vfo_, &shift_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        const char* shift_str = shim_rig_strptrshift(shift_);
        deferred_.Resolve(Napi::String::New(env, shift_str));
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
    int shift_;
};

class SetRepeaterOffsetAsyncWorker : public HamLibAsyncWorker {
public:
    SetRepeaterOffsetAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo, int offset)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), offset_(offset) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_set_rptr_offs(hamlib_instance_->my_rig, vfo_, offset_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
    int offset_;
};

class GetRepeaterOffsetAsyncWorker : public HamLibAsyncWorker {
public:
    GetRepeaterOffsetAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), offset_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_get_rptr_offs(hamlib_instance_->my_rig, vfo_, &offset_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, offset_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
    int offset_;
};

// Helper function to parse VFO parameter from JavaScript
int parseVfoParameter(const Napi::CallbackInfo& info, int index, int defaultVfo = SHIM_RIG_VFO_CURR) {
    if (info.Length() <= static_cast<size_t>(index) || info[index].IsUndefined() || info[index].IsNull()) {
        return defaultVfo;
    }
    if (!info[index].IsString()) {
        Napi::TypeError::New(info.Env(), "VFO must be specified as a Hamlib VFO token string")
          .ThrowAsJavaScriptException();
        return kInvalidVfoParameter;
    }
    std::string vfoStr = info[index].As<Napi::String>().Utf8Value();
    return parseVfoString(info.Env(), vfoStr);
}

NodeHamLib::NodeHamLib(const Napi::CallbackInfo & info): ObjectWrap(info) {
  Napi::Env env = info.Env();
  this->m_currentInfo = (Napi::CallbackInfo *)&info;
  if (info.Length() < 1) {
    Napi::TypeError::New(env, "Wrong number of arguments").ThrowAsJavaScriptException();
    return;
  }

  if (!info[0].IsNumber()) {
    Napi::TypeError::New(env, "Invalid Rig number").ThrowAsJavaScriptException();
    return;
  }

  // Check if port path is provided as second argument
  bool has_port = false;
  port_path[0] = '\0';

  if (info.Length() >= 2 && info[1].IsString()) {
    std::string portStr = info[1].As<Napi::String>().Utf8Value();
    if (!portStr.empty()) {
      strncpy(port_path, portStr.c_str(), SHIM_HAMLIB_FILPATHLEN - 1);
      port_path[SHIM_HAMLIB_FILPATHLEN - 1] = '\0';
      has_port = true;
    }
  }
  //unsigned int myrig_model;
  //   hamlib_port_t myport;
  // /* may be overriden by backend probe */
  // myport.type.rig = RIG_PORT_SERIAL;
  // myport.parm.serial.rate = 38400;
  // myport.parm.serial.data_bits = 8;
  // myport.parm.serial.stop_bits = 2;
  // myport.parm.serial.parity = SHIM_RIG_PARITY_NONE;
  // myport.parm.serial.handshake = SHIM_RIG_HANDSHAKE_HARDWARE;
  // strncpy(myport.pathname, "/dev/ttyUSB0", SHIM_HAMLIB_FILPATHLEN - 1);

  // shim_rig_load_all_backends();
  // myrig_model = rig_probe(&myport);
  // fprintf(stderr, "Got Rig Model %d \n", myrig_model);

  unsigned int myrig_model = info[0].As < Napi::Number > ().DoubleValue();
  original_model = myrig_model;

  // Check if port_path is a network address (contains colon)
  is_network_rig = isNetworkAddress(port_path);

  if (is_network_rig) {
    // Use NETRIGCTL model for network connections
    myrig_model = 2; // RIG_MODEL_NETRIGCTL
    // Network connection will be established on open()
  }

  my_rig = shim_rig_init(myrig_model);
  //int retcode = 0;
  if (!my_rig) {
    // Create detailed error message
    std::string errorMsg = "Unable to initialize rig (model: " +
                           std::to_string(myrig_model) +
                           "). Please check if the model number is valid.";
    Napi::TypeError::New(env, errorMsg).ThrowAsJavaScriptException();
  }
  
  // Only set port path and type when user explicitly provided a port.
  // Otherwise, let rig_init() defaults take effect (e.g., dummy rig uses RIG_PORT_NONE).
  if (has_port) {
    shim_rig_set_port_path(my_rig, port_path);

    if (is_network_rig) {
      shim_rig_set_port_type(my_rig, SHIM_RIG_PORT_NETWORK);
    } else {
      shim_rig_set_port_type(my_rig, SHIM_RIG_PORT_SERIAL);
    }
  }

  // this->freq_emit_cb = [info](double freq) {
  //     Napi::Env env = info.Env();
  //     Napi::Function emit = info.This().As<Napi::Object>().Get("emit").As<Napi::Function>();
  //       emit.Call(
  //       info.This(),
  //       {Napi::String::New(env, "frequency_change"), Napi::Number::New(env, freq)});
  //   } 

  rig_is_open = false;
}

// 析构函数 - 确保资源正确清理
NodeHamLib::~NodeHamLib() {
  StopSpectrumStreamInternal();

  // 如果rig指针存在，执行清理
  if (my_rig) {
    // 如果rig是打开状态，先关闭
    if (rig_is_open) {
      shim_rig_close(my_rig);
      rig_is_open = false;
    }
    // 清理rig资源
    shim_rig_cleanup(my_rig);
    my_rig = nullptr;
  }
}

int NodeHamLib::spectrum_line_cb(void* handle, const shim_spectrum_line_t* line, void* arg) {
  (void)handle;
  NodeHamLib* instance = static_cast<NodeHamLib*>(arg);
  if (!instance || !line) {
    return 0;
  }
  instance->EmitSpectrumLine(*line);
  return 0;
}

void NodeHamLib::EmitSpectrumLine(const shim_spectrum_line_t& line) {
  if (!spectrum_tsfn_) {
    return;
  }

  auto* line_copy = new shim_spectrum_line_t(line);
  napi_status status = spectrum_tsfn_.BlockingCall(
    line_copy,
    [](Napi::Env env, Napi::Function callback, shim_spectrum_line_t* data) {
      Napi::Object lineObject = Napi::Object::New(env);
      lineObject.Set("scopeId", Napi::Number::New(env, data->id));
      lineObject.Set("dataLevelMin", Napi::Number::New(env, data->data_level_min));
      lineObject.Set("dataLevelMax", Napi::Number::New(env, data->data_level_max));
      lineObject.Set("signalStrengthMin", Napi::Number::New(env, data->signal_strength_min));
      lineObject.Set("signalStrengthMax", Napi::Number::New(env, data->signal_strength_max));
      lineObject.Set("mode", Napi::Number::New(env, data->spectrum_mode));
      lineObject.Set("centerFreq", Napi::Number::New(env, data->center_freq));
      lineObject.Set("spanHz", Napi::Number::New(env, data->span_freq));
      lineObject.Set("lowEdgeFreq", Napi::Number::New(env, data->low_edge_freq));
      lineObject.Set("highEdgeFreq", Napi::Number::New(env, data->high_edge_freq));
      lineObject.Set("dataLength", Napi::Number::New(env, data->data_length));
      lineObject.Set("timestamp", Napi::Number::New(env, static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count())));
      lineObject.Set("data", Napi::Buffer<unsigned char>::Copy(env, data->data, static_cast<size_t>(data->data_length)));
      callback.Call({ lineObject });
      delete data;
    });

  if (status != napi_ok) {
    delete line_copy;
    return;
  }
}

void NodeHamLib::StopSpectrumStreamInternal() {
  std::lock_guard<std::mutex> lock(spectrum_mutex_);
  if (spectrum_stream_running_ && my_rig) {
    shim_rig_set_spectrum_callback(my_rig, nullptr, nullptr);
  }
  spectrum_stream_running_ = false;
  if (spectrum_tsfn_) {
    spectrum_tsfn_.Release();
    spectrum_tsfn_ = Napi::ThreadSafeFunction();
  }
}

int NodeHamLib::freq_change_cb(void *handle, int vfo, double freq, void* arg) {
      (void)handle;
      (void)vfo;
      (void)freq;
      auto instance = static_cast<NodeHamLib*>(arg);
      if (!instance) {
        return 0;
      }
      //Napi::Function emit = instance->m_currentInfo[0].Get("emit").As<Napi::Function>();
			// Napi::Function emit = instance->m_currentInfo[0]->This().As<Napi::Object>().Get("emit").As<Napi::Function>();
      //emit.Call(instance->m_currentInfo->This(), { Napi::String::New(env, "frequency_change"), Napi::Number::New(env, freq) });
        //this->freq_emit_cb(freq);
        //Napi::Function emit = this.As<Napi::Object>().Get("emit").As<Napi::Function>();
        //auto fn = global.Get("process").As<Napi::Object>().Get("emit").As<Napi::Function>();
        //fn.Call({Napi::Number::New(env, freq)});
        return 0;
}

Napi::Value NodeHamLib::Open(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  OpenAsyncWorker* worker = new OpenAsyncWorker(env, this);
  worker->Queue();
  
  return worker->GetPromise();
}

Napi::Value NodeHamLib::SetVFO(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();

  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (info.Length() < 1) {
    Napi::TypeError::New(env, "Must specify VFO")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (!info[0].IsString()) {
    Napi::TypeError::New(env, "Must specify a Hamlib VFO token as a string")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  
  int vfo = parseVfoParameter(info, 0, SHIM_RIG_VFO_CURR);
  RETURN_NULL_IF_INVALID_VFO(vfo);
  
  SetVfoAsyncWorker* worker = new SetVfoAsyncWorker(env, this, vfo);
  worker->Queue();
  
  return worker->GetPromise();
}

Napi::Value NodeHamLib::SetFrequency(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (info.Length() < 1) {
    Napi::TypeError::New(env, "Must specify frequency")
      .ThrowAsJavaScriptException();
    return env.Null();
  }

  if (!info[0].IsNumber()) {
    Napi::TypeError::New(env, "Frequency must be specified as a number")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  
  double freq = info[0].As<Napi::Number>().DoubleValue();
  
  // Basic frequency range validation
  if (freq < 1000 || freq > 10000000000) { // 1 kHz to 10 GHz reasonable range
    Napi::Error::New(env, "Frequency out of range (1 kHz - 10 GHz)").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  // Support optional VFO parameter
  int vfo = SHIM_RIG_VFO_CURR;
  
  if (info.Length() >= 2) {
    vfo = parseVfoParameter(info, 1, SHIM_RIG_VFO_CURR);
    RETURN_NULL_IF_INVALID_VFO(vfo);
  }
  
  SetFrequencyAsyncWorker* worker = new SetFrequencyAsyncWorker(env, this, freq, vfo);
  worker->Queue();
  
  return worker->GetPromise();
}

Napi::Value NodeHamLib::SetMode(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (info.Length() < 1) {
    Napi::TypeError::New(env, "Must specify mode")
      .ThrowAsJavaScriptException();
    return env.Null();
  }

  if (!info[0].IsString()) {
    Napi::TypeError::New(env, "Must specify mode as string")
      .ThrowAsJavaScriptException();
    return env.Null();
  }

  std::string modestr = info[0].As<Napi::String>().Utf8Value();
  int mode = shim_rig_parse_mode(modestr.c_str());
  
  if (mode == SHIM_RIG_MODE_NONE) {
    Napi::Error::New(env, "Invalid mode: " + modestr).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  int bandwidth = SHIM_RIG_PASSBAND_NORMAL;
  int vfo = SHIM_RIG_VFO_CURR;

  // Parse parameters: setMode(mode) or setMode(mode, bandwidth) or setMode(mode, bandwidth, vfo)
  if (info.Length() >= 2 && info[1].IsString()) {
    std::string bandstr = info[1].As<Napi::String>().Utf8Value();
    if (bandstr == "narrow") {
      bandwidth = shim_rig_passband_narrow(my_rig, mode);
    } else if (bandstr == "wide") {
      bandwidth = shim_rig_passband_wide(my_rig, mode);
    } else {
      // If second parameter is not "narrow" or "wide", might be VFO
      vfo = parseVfoParameter(info, 1, SHIM_RIG_VFO_CURR);
      RETURN_NULL_IF_INVALID_VFO(vfo);
    }
  }
  
  // Check for third parameter (VFO) if bandwidth was specified
  if (info.Length() >= 3) {
    vfo = parseVfoParameter(info, 2, SHIM_RIG_VFO_CURR);
    RETURN_NULL_IF_INVALID_VFO(vfo);
  }

  SetModeAsyncWorker* worker = new SetModeAsyncWorker(env, this, mode, bandwidth, vfo);
  worker->Queue();
  
  return worker->GetPromise();
}

Napi::Value NodeHamLib::SetPtt(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (info.Length() < 1) {
    Napi::TypeError::New(env, "Must specify PTT state")
      .ThrowAsJavaScriptException();
    return env.Null();
  }

  if (!info[0].IsBoolean()) {
    Napi::TypeError::New(env, "PTT state must be boolean")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  
  bool ptt_state = info[0].As<Napi::Boolean>();
  
  int ptt = ptt_state ? SHIM_RIG_PTT_ON : SHIM_RIG_PTT_OFF;
  SetPttAsyncWorker* worker = new SetPttAsyncWorker(env, this, ptt);
  worker->Queue();
  
  return worker->GetPromise();
}

Napi::Value NodeHamLib::GetVFO(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  
  GetVfoAsyncWorker* worker = new GetVfoAsyncWorker(env, this);
  worker->Queue();
  
  return worker->GetPromise();
}

Napi::Value NodeHamLib::GetFrequency(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  
  // Support optional VFO parameter
  int vfo = SHIM_RIG_VFO_CURR;
  
  if (info.Length() >= 1) {
    vfo = parseVfoParameter(info, 0, SHIM_RIG_VFO_CURR);
    RETURN_NULL_IF_INVALID_VFO(vfo);
  }
  
  GetFrequencyAsyncWorker* worker = new GetFrequencyAsyncWorker(env, this, vfo);
  worker->Queue();
  
  return worker->GetPromise();
}

Napi::Value NodeHamLib::GetMode(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  
  GetModeAsyncWorker* worker = new GetModeAsyncWorker(env, this);
  worker->Queue();
  
  return worker->GetPromise();
}

Napi::Value NodeHamLib::GetStrength(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  
  // Support optional VFO parameter: getStrength() or getStrength(vfo)
  int vfo = SHIM_RIG_VFO_CURR;
  
  if (info.Length() >= 1) {
    vfo = parseVfoParameter(info, 0, SHIM_RIG_VFO_CURR);
    RETURN_NULL_IF_INVALID_VFO(vfo);
  }
  
  GetStrengthAsyncWorker* worker = new GetStrengthAsyncWorker(env, this, vfo);
  worker->Queue();
  
  return worker->GetPromise();
}

Napi::Value NodeHamLib::Close(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!")
      .ThrowAsJavaScriptException();
    return env.Null();
  }

  StopSpectrumStreamInternal();
  
  CloseAsyncWorker* worker = new CloseAsyncWorker(env, this);
  worker->Queue();
  
  return worker->GetPromise();
}

Napi::Value NodeHamLib::Destroy(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();

  StopSpectrumStreamInternal();
  
  DestroyAsyncWorker* worker = new DestroyAsyncWorker(env, this);
  worker->Queue();
  
  return worker->GetPromise();
}

Napi::Value NodeHamLib::GetConnectionInfo(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  Napi::Object obj = Napi::Object::New(env);
  obj.Set(Napi::String::New(env, "connectionType"), 
          Napi::String::New(env, is_network_rig ? "network" : "serial"));
  obj.Set(Napi::String::New(env, "portPath"), 
          Napi::String::New(env, port_path));
  obj.Set(Napi::String::New(env, "isOpen"), 
          Napi::Boolean::New(env, rig_is_open));
  obj.Set(Napi::String::New(env, "originalModel"), 
          Napi::Number::New(env, original_model));
  obj.Set(Napi::String::New(env, "currentModel"), 
          Napi::Number::New(env, is_network_rig ? 2 : original_model));
  
  return obj;
}

// Memory Channel Management
Napi::Value NodeHamLib::SetMemoryChannel(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsObject()) {
    Napi::TypeError::New(env, "Expected (channelNumber: number, channelData: object)").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  int channel_num = info[0].As<Napi::Number>().Int32Value();
  Napi::Object chanObj = info[1].As<Napi::Object>();
  
  // Create channel structure
  shim_channel_t chan;
  memset(&chan, 0, sizeof(chan));
  
  chan.channel_num = channel_num;
  chan.vfo = SHIM_RIG_VFO_MEM;
  
  // Extract frequency
  if (chanObj.Has("frequency")) {
    chan.freq = chanObj.Get("frequency").As<Napi::Number>().DoubleValue();
  }
  
  // Extract mode
  if (chanObj.Has("mode")) {
    std::string modeStr = chanObj.Get("mode").As<Napi::String>().Utf8Value();
    chan.mode = shim_rig_parse_mode(modeStr.c_str());
  }
  
  // Extract bandwidth
  if (chanObj.Has("bandwidth")) {
    chan.width = chanObj.Get("bandwidth").As<Napi::Number>().Int32Value();
  } else {
    chan.width = SHIM_RIG_PASSBAND_NORMAL;
  }
  
  // Extract channel description
  if (chanObj.Has("description")) {
    std::string desc = chanObj.Get("description").As<Napi::String>().Utf8Value();
    strncpy(chan.channel_desc, desc.c_str(), sizeof(chan.channel_desc) - 1);
    chan.channel_desc[sizeof(chan.channel_desc) - 1] = '\0';
  }
  
  // Extract TX frequency for split operation
  if (chanObj.Has("txFrequency")) {
    chan.tx_freq = chanObj.Get("txFrequency").As<Napi::Number>().DoubleValue();
    chan.split = SHIM_RIG_SPLIT_ON;
  }
  
  // Extract CTCSS tone
  if (chanObj.Has("ctcssTone")) {
    chan.ctcss_tone = chanObj.Get("ctcssTone").As<Napi::Number>().Int32Value();
  }
  
  SetMemoryChannelAsyncWorker* worker = new SetMemoryChannelAsyncWorker(env, this, chan);
  worker->Queue();
  
  return worker->GetPromise();
}

Napi::Value NodeHamLib::GetMemoryChannel(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsBoolean()) {
    Napi::TypeError::New(env, "Expected (channelNumber: number, readOnly: boolean)").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  int channel_num = info[0].As<Napi::Number>().Int32Value();
  bool read_only = info[1].As<Napi::Boolean>().Value();
  
  GetMemoryChannelAsyncWorker* worker = new GetMemoryChannelAsyncWorker(env, this, channel_num, read_only);
  worker->Queue();
  
  return worker->GetPromise();
}

Napi::Value NodeHamLib::SelectMemoryChannel(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (info.Length() < 1 || !info[0].IsNumber()) {
    Napi::TypeError::New(env, "Expected (channelNumber: number)").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  int channel_num = info[0].As<Napi::Number>().Int32Value();
  
  SelectMemoryChannelAsyncWorker* worker = new SelectMemoryChannelAsyncWorker(env, this, channel_num);
  worker->Queue();
  
  return worker->GetPromise();
}

// RIT/XIT Control
Napi::Value NodeHamLib::SetRit(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (info.Length() < 1 || !info[0].IsNumber()) {
    Napi::TypeError::New(env, "Expected (ritOffset: number)").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  int rit_offset = info[0].As<Napi::Number>().Int32Value();
  
  SetRitAsyncWorker* worker = new SetRitAsyncWorker(env, this, rit_offset);
  worker->Queue();
  
  return worker->GetPromise();
}

Napi::Value NodeHamLib::GetRit(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  GetRitAsyncWorker* worker = new GetRitAsyncWorker(env, this);
  worker->Queue();
  
  return worker->GetPromise();
}

Napi::Value NodeHamLib::SetXit(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (info.Length() < 1 || !info[0].IsNumber()) {
    Napi::TypeError::New(env, "Expected (xitOffset: number)").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  int xit_offset = info[0].As<Napi::Number>().Int32Value();
  
  SetXitAsyncWorker* worker = new SetXitAsyncWorker(env, this, xit_offset);
  worker->Queue();
  
  return worker->GetPromise();
}

Napi::Value NodeHamLib::GetXit(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  GetXitAsyncWorker* worker = new GetXitAsyncWorker(env, this);
  worker->Queue();
  
  return worker->GetPromise();
}

Napi::Value NodeHamLib::ClearRitXit(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  ClearRitXitAsyncWorker* worker = new ClearRitXitAsyncWorker(env, this);
  worker->Queue();
  
  return worker->GetPromise();
}

// Scanning Operations
Napi::Value NodeHamLib::StartScan(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (info.Length() < 2 || !info[0].IsString() || !info[1].IsNumber()) {
    Napi::TypeError::New(env, "Expected (scanType: string, channel: number)").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  std::string scanTypeStr = info[0].As<Napi::String>().Utf8Value();
  int scanType;
  
  if (scanTypeStr == "VFO") {
    scanType = SHIM_RIG_SCAN_VFO;
  } else if (scanTypeStr == "MEM") {
    scanType = SHIM_RIG_SCAN_MEM;
  } else if (scanTypeStr == "PROG") {
    scanType = SHIM_RIG_SCAN_PROG;
  } else if (scanTypeStr == "DELTA") {
    scanType = SHIM_RIG_SCAN_DELTA;
  } else if (scanTypeStr == "PRIO") {
    scanType = SHIM_RIG_SCAN_PRIO;
  } else {
    Napi::TypeError::New(env, "Invalid scan type").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  int channel = info[1].As<Napi::Number>().Int32Value();
  
  StartScanAsyncWorker* asyncWorker = new StartScanAsyncWorker(env, this, scanType, channel);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::StopScan(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  StopScanAsyncWorker* asyncWorker = new StopScanAsyncWorker(env, this);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// Level Controls
Napi::Value NodeHamLib::SetLevel(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (info.Length() < 2 || !info[0].IsString() || !info[1].IsNumber()) {
    Napi::TypeError::New(env, "Expected (levelType: string, value: number, vfo?: string)").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  std::string levelTypeStr = info[0].As<Napi::String>().Utf8Value();
  double levelValue = info[1].As<Napi::Number>().DoubleValue();
  int vfo = SHIM_RIG_VFO_CURR;
  if (info.Length() >= 3) {
    vfo = parseVfoParameter(info, 2, SHIM_RIG_VFO_CURR);
    RETURN_NULL_IF_INVALID_VFO(vfo);
  }
  
  // Map level type strings to hamlib constants
  uint64_t levelType;
  if (levelTypeStr == "AF") {
    levelType = SHIM_RIG_LEVEL_AF;
  } else if (levelTypeStr == "RF") {
    levelType = SHIM_RIG_LEVEL_RF;
  } else if (levelTypeStr == "SQL") {
    levelType = SHIM_RIG_LEVEL_SQL;
  } else if (levelTypeStr == "RFPOWER") {
    levelType = SHIM_RIG_LEVEL_RFPOWER;
  } else if (levelTypeStr == "MICGAIN") {
    levelType = SHIM_RIG_LEVEL_MICGAIN;
  } else if (levelTypeStr == "IF") {
    levelType = SHIM_RIG_LEVEL_IF;
  } else if (levelTypeStr == "APF") {
    levelType = SHIM_RIG_LEVEL_APF;
  } else if (levelTypeStr == "NR") {
    levelType = SHIM_RIG_LEVEL_NR;
  } else if (levelTypeStr == "PBT_IN") {
    levelType = SHIM_RIG_LEVEL_PBT_IN;
  } else if (levelTypeStr == "PBT_OUT") {
    levelType = SHIM_RIG_LEVEL_PBT_OUT;
  } else if (levelTypeStr == "CWPITCH") {
    levelType = SHIM_RIG_LEVEL_CWPITCH;
  } else if (levelTypeStr == "KEYSPD") {
    levelType = SHIM_RIG_LEVEL_KEYSPD;
  } else if (levelTypeStr == "NOTCHF") {
    levelType = SHIM_RIG_LEVEL_NOTCHF;
  } else if (levelTypeStr == "COMP") {
    levelType = SHIM_RIG_LEVEL_COMP;
  } else if (levelTypeStr == "AGC") {
    levelType = SHIM_RIG_LEVEL_AGC;
  } else if (levelTypeStr == "BKINDL") {
    levelType = SHIM_RIG_LEVEL_BKINDL;
  } else if (levelTypeStr == "BALANCE") {
    levelType = SHIM_RIG_LEVEL_BALANCE;
  } else if (levelTypeStr == "VOXGAIN") {
    levelType = SHIM_RIG_LEVEL_VOXGAIN;
  } else if (levelTypeStr == "VOXDELAY") {
    levelType = SHIM_RIG_LEVEL_VOXDELAY;
  } else if (levelTypeStr == "ANTIVOX") {
    levelType = SHIM_RIG_LEVEL_ANTIVOX;
  } else if (levelTypeStr == "SPECTRUM_MODE") {
    levelType = SHIM_RIG_LEVEL_SPECTRUM_MODE;
  } else if (levelTypeStr == "SPECTRUM_SPAN") {
    levelType = SHIM_RIG_LEVEL_SPECTRUM_SPAN;
  } else if (levelTypeStr == "SPECTRUM_EDGE_LOW") {
    levelType = SHIM_RIG_LEVEL_SPECTRUM_EDGE_LOW;
  } else if (levelTypeStr == "SPECTRUM_EDGE_HIGH") {
    levelType = SHIM_RIG_LEVEL_SPECTRUM_EDGE_HIGH;
  } else if (levelTypeStr == "SPECTRUM_SPEED") {
    levelType = SHIM_RIG_LEVEL_SPECTRUM_SPEED;
  } else if (levelTypeStr == "SPECTRUM_REF") {
    levelType = SHIM_RIG_LEVEL_SPECTRUM_REF;
  } else if (levelTypeStr == "SPECTRUM_AVG") {
    levelType = SHIM_RIG_LEVEL_SPECTRUM_AVG;
  } else if (levelTypeStr == "SPECTRUM_ATT") {
    levelType = SHIM_RIG_LEVEL_SPECTRUM_ATT;
  } else {
    Napi::TypeError::New(env, "Invalid level type").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  const bool isSpectrumLevel =
    levelType == SHIM_RIG_LEVEL_SPECTRUM_MODE
    || levelType == SHIM_RIG_LEVEL_SPECTRUM_SPAN
    || levelType == SHIM_RIG_LEVEL_SPECTRUM_EDGE_LOW
    || levelType == SHIM_RIG_LEVEL_SPECTRUM_EDGE_HIGH
    || levelType == SHIM_RIG_LEVEL_SPECTRUM_SPEED
    || levelType == SHIM_RIG_LEVEL_SPECTRUM_REF
    || levelType == SHIM_RIG_LEVEL_SPECTRUM_AVG
    || levelType == SHIM_RIG_LEVEL_SPECTRUM_ATT;

  if (isSpectrumLevel) {
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
    int result = shim_rig_level_is_float(levelType)
      ? shim_rig_set_level_f(my_rig, vfo, levelType, static_cast<float>(levelValue))
      : shim_rig_set_level_i(my_rig, vfo, levelType, static_cast<int>(levelValue));

    if (result != SHIM_RIG_OK) {
      deferred.Reject(Napi::Error::New(env, shim_rigerror(result)).Value());
    } else {
      deferred.Resolve(Napi::Number::New(env, result));
    }
    return deferred.Promise();
  }

  float val = static_cast<float>(levelValue);
  SetLevelAsyncWorker* worker = new SetLevelAsyncWorker(env, this, levelType, val, vfo);
  worker->Queue();
  return worker->GetPromise();
}

Napi::Value NodeHamLib::GetLevel(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected (levelType: string)").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  std::string levelTypeStr = info[0].As<Napi::String>().Utf8Value();
  
  // Map level type strings to hamlib constants
  uint64_t levelType;
  if (levelTypeStr == "AF") {
    levelType = SHIM_RIG_LEVEL_AF;
  } else if (levelTypeStr == "RF") {
    levelType = SHIM_RIG_LEVEL_RF;
  } else if (levelTypeStr == "SQL") {
    levelType = SHIM_RIG_LEVEL_SQL;
  } else if (levelTypeStr == "RFPOWER") {
    levelType = SHIM_RIG_LEVEL_RFPOWER;
  } else if (levelTypeStr == "MICGAIN") {
    levelType = SHIM_RIG_LEVEL_MICGAIN;
  } else if (levelTypeStr == "SWR") {
    levelType = SHIM_RIG_LEVEL_SWR;
  } else if (levelTypeStr == "ALC") {
    levelType = SHIM_RIG_LEVEL_ALC;
  } else if (levelTypeStr == "STRENGTH") {
    levelType = SHIM_RIG_LEVEL_STRENGTH;
  } else if (levelTypeStr == "RAWSTR") {
    levelType = SHIM_RIG_LEVEL_RAWSTR;
  } else if (levelTypeStr == "RFPOWER_METER") {
    levelType = SHIM_RIG_LEVEL_RFPOWER_METER;
  } else if (levelTypeStr == "RFPOWER_METER_WATTS") {
    levelType = SHIM_RIG_LEVEL_RFPOWER_METER_WATTS;
  } else if (levelTypeStr == "COMP_METER") {
    levelType = SHIM_RIG_LEVEL_COMP_METER;
  } else if (levelTypeStr == "VD_METER") {
    levelType = SHIM_RIG_LEVEL_VD_METER;
  } else if (levelTypeStr == "ID_METER") {
    levelType = SHIM_RIG_LEVEL_ID_METER;
  } else if (levelTypeStr == "TEMP_METER") {
    levelType = SHIM_RIG_LEVEL_TEMP_METER;
  } else if (levelTypeStr == "NR") {
    levelType = SHIM_RIG_LEVEL_NR;
  } else if (levelTypeStr == "AF") {
    levelType = SHIM_RIG_LEVEL_AF;
  } else if (levelTypeStr == "RF") {
    levelType = SHIM_RIG_LEVEL_RF;
  } else if (levelTypeStr == "SQL") {
    levelType = SHIM_RIG_LEVEL_SQL;
  } else if (levelTypeStr == "RFPOWER") {
    levelType = SHIM_RIG_LEVEL_RFPOWER;
  } else if (levelTypeStr == "MICGAIN") {
    levelType = SHIM_RIG_LEVEL_MICGAIN;
  } else if (levelTypeStr == "VOXDELAY") {
    levelType = SHIM_RIG_LEVEL_VOXDELAY;
  } else if (levelTypeStr == "PBT_IN") {
    levelType = SHIM_RIG_LEVEL_PBT_IN;
  } else if (levelTypeStr == "PBT_OUT") {
    levelType = SHIM_RIG_LEVEL_PBT_OUT;
  } else if (levelTypeStr == "CWPITCH") {
    levelType = SHIM_RIG_LEVEL_CWPITCH;
  } else if (levelTypeStr == "COMP") {
    levelType = SHIM_RIG_LEVEL_COMP;
  } else if (levelTypeStr == "AGC") {
    levelType = SHIM_RIG_LEVEL_AGC;
  } else if (levelTypeStr == "VOXGAIN") {
    levelType = SHIM_RIG_LEVEL_VOXGAIN;
  } else if (levelTypeStr == "ANTIVOX") {
    levelType = SHIM_RIG_LEVEL_ANTIVOX;
  } else if (levelTypeStr == "SPECTRUM_MODE") {
    levelType = SHIM_RIG_LEVEL_SPECTRUM_MODE;
  } else if (levelTypeStr == "SPECTRUM_SPAN") {
    levelType = SHIM_RIG_LEVEL_SPECTRUM_SPAN;
  } else if (levelTypeStr == "SPECTRUM_EDGE_LOW") {
    levelType = SHIM_RIG_LEVEL_SPECTRUM_EDGE_LOW;
  } else if (levelTypeStr == "SPECTRUM_EDGE_HIGH") {
    levelType = SHIM_RIG_LEVEL_SPECTRUM_EDGE_HIGH;
  } else if (levelTypeStr == "SPECTRUM_SPEED") {
    levelType = SHIM_RIG_LEVEL_SPECTRUM_SPEED;
  } else if (levelTypeStr == "SPECTRUM_REF") {
    levelType = SHIM_RIG_LEVEL_SPECTRUM_REF;
  } else if (levelTypeStr == "SPECTRUM_AVG") {
    levelType = SHIM_RIG_LEVEL_SPECTRUM_AVG;
  } else if (levelTypeStr == "SPECTRUM_ATT") {
    levelType = SHIM_RIG_LEVEL_SPECTRUM_ATT;
  } else {
    Napi::TypeError::New(env, "Invalid level type").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  // Optional second parameter: Hamlib VFO token string ('VFOA', 'VFOB', 'currVFO', ...)
  int vfo = SHIM_RIG_VFO_CURR;
  if (info.Length() >= 2) {
    vfo = parseVfoParameter(info, 1, SHIM_RIG_VFO_CURR);
    RETURN_NULL_IF_INVALID_VFO(vfo);
  }

  GetLevelAsyncWorker* worker = new GetLevelAsyncWorker(env, this, levelType, vfo);
  worker->Queue();

  return worker->GetPromise();
}

Napi::Value NodeHamLib::GetSupportedLevels(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  RETURN_NULL_IF_RIG_HANDLE_INVALID();
  
  // Get capabilities from rig caps instead of state (doesn't require rig to be open)
  uint64_t levels = shim_rig_get_caps_has_get_level(my_rig) | shim_rig_get_caps_has_set_level(my_rig);
  Napi::Array levelArray = Napi::Array::New(env);
  uint32_t index = 0;
  
  // Check each level type
  if (levels & SHIM_RIG_LEVEL_AF) levelArray[index++] = Napi::String::New(env, "AF");
  if (levels & SHIM_RIG_LEVEL_RF) levelArray[index++] = Napi::String::New(env, "RF");
  if (levels & SHIM_RIG_LEVEL_SQL) levelArray[index++] = Napi::String::New(env, "SQL");
  if (levels & SHIM_RIG_LEVEL_RFPOWER) levelArray[index++] = Napi::String::New(env, "RFPOWER");
  if (levels & SHIM_RIG_LEVEL_MICGAIN) levelArray[index++] = Napi::String::New(env, "MICGAIN");
  if (levels & SHIM_RIG_LEVEL_IF) levelArray[index++] = Napi::String::New(env, "IF");
  if (levels & SHIM_RIG_LEVEL_APF) levelArray[index++] = Napi::String::New(env, "APF");
  if (levels & SHIM_RIG_LEVEL_NR) levelArray[index++] = Napi::String::New(env, "NR");
  if (levels & SHIM_RIG_LEVEL_PBT_IN) levelArray[index++] = Napi::String::New(env, "PBT_IN");
  if (levels & SHIM_RIG_LEVEL_PBT_OUT) levelArray[index++] = Napi::String::New(env, "PBT_OUT");
  if (levels & SHIM_RIG_LEVEL_CWPITCH) levelArray[index++] = Napi::String::New(env, "CWPITCH");
  if (levels & SHIM_RIG_LEVEL_KEYSPD) levelArray[index++] = Napi::String::New(env, "KEYSPD");
  if (levels & SHIM_RIG_LEVEL_NOTCHF) levelArray[index++] = Napi::String::New(env, "NOTCHF");
  if (levels & SHIM_RIG_LEVEL_COMP) levelArray[index++] = Napi::String::New(env, "COMP");
  if (levels & SHIM_RIG_LEVEL_AGC) levelArray[index++] = Napi::String::New(env, "AGC");
  if (levels & SHIM_RIG_LEVEL_BKINDL) levelArray[index++] = Napi::String::New(env, "BKINDL");
  if (levels & SHIM_RIG_LEVEL_BALANCE) levelArray[index++] = Napi::String::New(env, "BALANCE");
  if (levels & SHIM_RIG_LEVEL_VOXGAIN) levelArray[index++] = Napi::String::New(env, "VOXGAIN");
  if (levels & SHIM_RIG_LEVEL_VOXDELAY) levelArray[index++] = Napi::String::New(env, "VOXDELAY");
  if (levels & SHIM_RIG_LEVEL_ANTIVOX) levelArray[index++] = Napi::String::New(env, "ANTIVOX");
  if (levels & SHIM_RIG_LEVEL_STRENGTH) levelArray[index++] = Napi::String::New(env, "STRENGTH");
  if (levels & SHIM_RIG_LEVEL_RAWSTR) levelArray[index++] = Napi::String::New(env, "RAWSTR");
  if (levels & SHIM_RIG_LEVEL_SWR) levelArray[index++] = Napi::String::New(env, "SWR");
  if (levels & SHIM_RIG_LEVEL_ALC) levelArray[index++] = Napi::String::New(env, "ALC");
  if (levels & SHIM_RIG_LEVEL_RFPOWER_METER) levelArray[index++] = Napi::String::New(env, "RFPOWER_METER");
  if (levels & SHIM_RIG_LEVEL_RFPOWER_METER_WATTS) levelArray[index++] = Napi::String::New(env, "RFPOWER_METER_WATTS");
  if (levels & SHIM_RIG_LEVEL_COMP_METER) levelArray[index++] = Napi::String::New(env, "COMP_METER");
  if (levels & SHIM_RIG_LEVEL_VD_METER) levelArray[index++] = Napi::String::New(env, "VD_METER");
  if (levels & SHIM_RIG_LEVEL_ID_METER) levelArray[index++] = Napi::String::New(env, "ID_METER");
  if (levels & SHIM_RIG_LEVEL_SPECTRUM_MODE) levelArray[index++] = Napi::String::New(env, "SPECTRUM_MODE");
  if (levels & SHIM_RIG_LEVEL_SPECTRUM_SPAN) levelArray[index++] = Napi::String::New(env, "SPECTRUM_SPAN");
  if (levels & SHIM_RIG_LEVEL_SPECTRUM_EDGE_LOW) levelArray[index++] = Napi::String::New(env, "SPECTRUM_EDGE_LOW");
  if (levels & SHIM_RIG_LEVEL_SPECTRUM_EDGE_HIGH) levelArray[index++] = Napi::String::New(env, "SPECTRUM_EDGE_HIGH");
  if (levels & SHIM_RIG_LEVEL_SPECTRUM_SPEED) levelArray[index++] = Napi::String::New(env, "SPECTRUM_SPEED");
  if (levels & SHIM_RIG_LEVEL_SPECTRUM_REF) levelArray[index++] = Napi::String::New(env, "SPECTRUM_REF");
  if (levels & SHIM_RIG_LEVEL_SPECTRUM_AVG) levelArray[index++] = Napi::String::New(env, "SPECTRUM_AVG");
  if (levels & SHIM_RIG_LEVEL_SPECTRUM_ATT) levelArray[index++] = Napi::String::New(env, "SPECTRUM_ATT");
  if (levels & SHIM_RIG_LEVEL_TEMP_METER) levelArray[index++] = Napi::String::New(env, "TEMP_METER");
  
  return levelArray;
}

// Function Controls
Napi::Value NodeHamLib::SetFunction(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (info.Length() < 2 || !info[0].IsString() || !info[1].IsBoolean()) {
    Napi::TypeError::New(env, "Expected (functionType: string, enable: boolean)").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  std::string funcTypeStr = info[0].As<Napi::String>().Utf8Value();
  bool enable = info[1].As<Napi::Boolean>().Value();
  
  // Map function type strings to hamlib constants
  uint64_t funcType;
  if (funcTypeStr == "FAGC") {
    funcType = SHIM_RIG_FUNC_FAGC;
  } else if (funcTypeStr == "NB") {
    funcType = SHIM_RIG_FUNC_NB;
  } else if (funcTypeStr == "COMP") {
    funcType = SHIM_RIG_FUNC_COMP;
  } else if (funcTypeStr == "VOX") {
    funcType = SHIM_RIG_FUNC_VOX;
  } else if (funcTypeStr == "TONE") {
    funcType = SHIM_RIG_FUNC_TONE;
  } else if (funcTypeStr == "TSQL") {
    funcType = SHIM_RIG_FUNC_TSQL;
  } else if (funcTypeStr == "SBKIN") {
    funcType = SHIM_RIG_FUNC_SBKIN;
  } else if (funcTypeStr == "FBKIN") {
    funcType = SHIM_RIG_FUNC_FBKIN;
  } else if (funcTypeStr == "ANF") {
    funcType = SHIM_RIG_FUNC_ANF;
  } else if (funcTypeStr == "NR") {
    funcType = SHIM_RIG_FUNC_NR;
  } else if (funcTypeStr == "AIP") {
    funcType = SHIM_RIG_FUNC_AIP;
  } else if (funcTypeStr == "APF") {
    funcType = SHIM_RIG_FUNC_APF;
  } else if (funcTypeStr == "TUNER") {
    funcType = SHIM_RIG_FUNC_TUNER;
  } else if (funcTypeStr == "XIT") {
    funcType = SHIM_RIG_FUNC_XIT;
  } else if (funcTypeStr == "RIT") {
    funcType = SHIM_RIG_FUNC_RIT;
  } else if (funcTypeStr == "LOCK") {
    funcType = SHIM_RIG_FUNC_LOCK;
  } else if (funcTypeStr == "MUTE") {
    funcType = SHIM_RIG_FUNC_MUTE;
  } else if (funcTypeStr == "VSC") {
    funcType = SHIM_RIG_FUNC_VSC;
  } else if (funcTypeStr == "REV") {
    funcType = SHIM_RIG_FUNC_REV;
  } else if (funcTypeStr == "SQL") {
    funcType = SHIM_RIG_FUNC_SQL;
  } else if (funcTypeStr == "ABM") {
    funcType = SHIM_RIG_FUNC_ABM;
  } else if (funcTypeStr == "BC") {
    funcType = SHIM_RIG_FUNC_BC;
  } else if (funcTypeStr == "MBC") {
    funcType = SHIM_RIG_FUNC_MBC;
  } else if (funcTypeStr == "AFC") {
    funcType = SHIM_RIG_FUNC_AFC;
  } else if (funcTypeStr == "SATMODE") {
    funcType = SHIM_RIG_FUNC_SATMODE;
  } else if (funcTypeStr == "SCOPE") {
    funcType = SHIM_RIG_FUNC_SCOPE;
  } else if (funcTypeStr == "RESUME") {
    funcType = SHIM_RIG_FUNC_RESUME;
  } else if (funcTypeStr == "TBURST") {
    funcType = SHIM_RIG_FUNC_TBURST;
  } else if (funcTypeStr == "TRANSCEIVE") {
    funcType = SHIM_RIG_FUNC_TRANSCEIVE;
  } else if (funcTypeStr == "SPECTRUM") {
    funcType = SHIM_RIG_FUNC_SPECTRUM;
  } else if (funcTypeStr == "SPECTRUM_HOLD") {
    funcType = SHIM_RIG_FUNC_SPECTRUM_HOLD;
  } else {
    Napi::TypeError::New(env, "Invalid function type").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  SetFunctionAsyncWorker* worker = new SetFunctionAsyncWorker(env, this, funcType, enable ? 1 : 0);
  worker->Queue();
  
  return worker->GetPromise();
}

Napi::Value NodeHamLib::GetFunction(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected (functionType: string)").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  std::string funcTypeStr = info[0].As<Napi::String>().Utf8Value();
  
  // Map function type strings to hamlib constants (same as SetFunction)
  uint64_t funcType;
  if (funcTypeStr == "FAGC") {
    funcType = SHIM_RIG_FUNC_FAGC;
  } else if (funcTypeStr == "NB") {
    funcType = SHIM_RIG_FUNC_NB;
  } else if (funcTypeStr == "COMP") {
    funcType = SHIM_RIG_FUNC_COMP;
  } else if (funcTypeStr == "VOX") {
    funcType = SHIM_RIG_FUNC_VOX;
  } else if (funcTypeStr == "TONE") {
    funcType = SHIM_RIG_FUNC_TONE;
  } else if (funcTypeStr == "TSQL") {
    funcType = SHIM_RIG_FUNC_TSQL;
  } else if (funcTypeStr == "SBKIN") {
    funcType = SHIM_RIG_FUNC_SBKIN;
  } else if (funcTypeStr == "FBKIN") {
    funcType = SHIM_RIG_FUNC_FBKIN;
  } else if (funcTypeStr == "ANF") {
    funcType = SHIM_RIG_FUNC_ANF;
  } else if (funcTypeStr == "NR") {
    funcType = SHIM_RIG_FUNC_NR;
  } else if (funcTypeStr == "AIP") {
    funcType = SHIM_RIG_FUNC_AIP;
  } else if (funcTypeStr == "APF") {
    funcType = SHIM_RIG_FUNC_APF;
  } else if (funcTypeStr == "TUNER") {
    funcType = SHIM_RIG_FUNC_TUNER;
  } else if (funcTypeStr == "XIT") {
    funcType = SHIM_RIG_FUNC_XIT;
  } else if (funcTypeStr == "RIT") {
    funcType = SHIM_RIG_FUNC_RIT;
  } else if (funcTypeStr == "LOCK") {
    funcType = SHIM_RIG_FUNC_LOCK;
  } else if (funcTypeStr == "MUTE") {
    funcType = SHIM_RIG_FUNC_MUTE;
  } else if (funcTypeStr == "VSC") {
    funcType = SHIM_RIG_FUNC_VSC;
  } else if (funcTypeStr == "REV") {
    funcType = SHIM_RIG_FUNC_REV;
  } else if (funcTypeStr == "SQL") {
    funcType = SHIM_RIG_FUNC_SQL;
  } else if (funcTypeStr == "ABM") {
    funcType = SHIM_RIG_FUNC_ABM;
  } else if (funcTypeStr == "BC") {
    funcType = SHIM_RIG_FUNC_BC;
  } else if (funcTypeStr == "MBC") {
    funcType = SHIM_RIG_FUNC_MBC;
  } else if (funcTypeStr == "AFC") {
    funcType = SHIM_RIG_FUNC_AFC;
  } else if (funcTypeStr == "SATMODE") {
    funcType = SHIM_RIG_FUNC_SATMODE;
  } else if (funcTypeStr == "SCOPE") {
    funcType = SHIM_RIG_FUNC_SCOPE;
  } else if (funcTypeStr == "RESUME") {
    funcType = SHIM_RIG_FUNC_RESUME;
  } else if (funcTypeStr == "TBURST") {
    funcType = SHIM_RIG_FUNC_TBURST;
  } else if (funcTypeStr == "TRANSCEIVE") {
    funcType = SHIM_RIG_FUNC_TRANSCEIVE;
  } else if (funcTypeStr == "SPECTRUM") {
    funcType = SHIM_RIG_FUNC_SPECTRUM;
  } else if (funcTypeStr == "SPECTRUM_HOLD") {
    funcType = SHIM_RIG_FUNC_SPECTRUM_HOLD;
  } else {
    Napi::TypeError::New(env, "Invalid function type").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  GetFunctionAsyncWorker* worker = new GetFunctionAsyncWorker(env, this, funcType);
  worker->Queue();
  
  return worker->GetPromise();
}

Napi::Value NodeHamLib::GetSupportedFunctions(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  RETURN_NULL_IF_RIG_HANDLE_INVALID();
  
  // Get capabilities from rig caps instead of state (doesn't require rig to be open)
  uint64_t functions = shim_rig_get_caps_has_get_func(my_rig) | shim_rig_get_caps_has_set_func(my_rig);
  Napi::Array funcArray = Napi::Array::New(env);
  uint32_t index = 0;
  
  // Check each function type
  if (functions & SHIM_RIG_FUNC_FAGC) funcArray[index++] = Napi::String::New(env, "FAGC");
  if (functions & SHIM_RIG_FUNC_NB) funcArray[index++] = Napi::String::New(env, "NB");
  if (functions & SHIM_RIG_FUNC_COMP) funcArray[index++] = Napi::String::New(env, "COMP");
  if (functions & SHIM_RIG_FUNC_VOX) funcArray[index++] = Napi::String::New(env, "VOX");
  if (functions & SHIM_RIG_FUNC_TONE) funcArray[index++] = Napi::String::New(env, "TONE");
  if (functions & SHIM_RIG_FUNC_TSQL) funcArray[index++] = Napi::String::New(env, "TSQL");
  if (functions & SHIM_RIG_FUNC_SBKIN) funcArray[index++] = Napi::String::New(env, "SBKIN");
  if (functions & SHIM_RIG_FUNC_FBKIN) funcArray[index++] = Napi::String::New(env, "FBKIN");
  if (functions & SHIM_RIG_FUNC_ANF) funcArray[index++] = Napi::String::New(env, "ANF");
  if (functions & SHIM_RIG_FUNC_NR) funcArray[index++] = Napi::String::New(env, "NR");
  if (functions & SHIM_RIG_FUNC_AIP) funcArray[index++] = Napi::String::New(env, "AIP");
  if (functions & SHIM_RIG_FUNC_APF) funcArray[index++] = Napi::String::New(env, "APF");
  if (functions & SHIM_RIG_FUNC_TUNER) funcArray[index++] = Napi::String::New(env, "TUNER");
  if (functions & SHIM_RIG_FUNC_XIT) funcArray[index++] = Napi::String::New(env, "XIT");
  if (functions & SHIM_RIG_FUNC_RIT) funcArray[index++] = Napi::String::New(env, "RIT");
  if (functions & SHIM_RIG_FUNC_LOCK) funcArray[index++] = Napi::String::New(env, "LOCK");
  if (functions & SHIM_RIG_FUNC_MUTE) funcArray[index++] = Napi::String::New(env, "MUTE");
  if (functions & SHIM_RIG_FUNC_VSC) funcArray[index++] = Napi::String::New(env, "VSC");
  if (functions & SHIM_RIG_FUNC_REV) funcArray[index++] = Napi::String::New(env, "REV");
  if (functions & SHIM_RIG_FUNC_SQL) funcArray[index++] = Napi::String::New(env, "SQL");
  if (functions & SHIM_RIG_FUNC_ABM) funcArray[index++] = Napi::String::New(env, "ABM");
  if (functions & SHIM_RIG_FUNC_BC) funcArray[index++] = Napi::String::New(env, "BC");
  if (functions & SHIM_RIG_FUNC_MBC) funcArray[index++] = Napi::String::New(env, "MBC");
  if (functions & SHIM_RIG_FUNC_AFC) funcArray[index++] = Napi::String::New(env, "AFC");
  if (functions & SHIM_RIG_FUNC_SATMODE) funcArray[index++] = Napi::String::New(env, "SATMODE");
  if (functions & SHIM_RIG_FUNC_SCOPE) funcArray[index++] = Napi::String::New(env, "SCOPE");
  if (functions & SHIM_RIG_FUNC_RESUME) funcArray[index++] = Napi::String::New(env, "RESUME");
  if (functions & SHIM_RIG_FUNC_TBURST) funcArray[index++] = Napi::String::New(env, "TBURST");
  if (functions & SHIM_RIG_FUNC_TRANSCEIVE) funcArray[index++] = Napi::String::New(env, "TRANSCEIVE");
  if (functions & SHIM_RIG_FUNC_SPECTRUM) funcArray[index++] = Napi::String::New(env, "SPECTRUM");
  if (functions & SHIM_RIG_FUNC_SPECTRUM_HOLD) funcArray[index++] = Napi::String::New(env, "SPECTRUM_HOLD");
  
  return funcArray;
}

// Mode Query
Napi::Value NodeHamLib::GetSupportedModes(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  RETURN_NULL_IF_RIG_HANDLE_INVALID();

  // Get mode list from rig state (populated during rig_open from rx/tx range lists)
  uint64_t modes = shim_rig_get_mode_list(my_rig);
  Napi::Array modeArray = Napi::Array::New(env);
  uint32_t index = 0;

  // Iterate through all possible mode bits (similar to rig_sprintf_mode)
  for (unsigned int i = 0; i < SHIM_HAMLIB_MAX_MODES; i++) {
    uint64_t mode_bit = modes & (1ULL << i);

    if (mode_bit) {
      const char* mode_str = shim_rig_strrmode(mode_bit);

      // Skip empty or unknown modes
      if (mode_str && mode_str[0] != '\0') {
        modeArray[index++] = Napi::String::New(env, mode_str);
      }
    }
  }

  return modeArray;
}

// Split Operations
Napi::Value NodeHamLib::SetSplitFreq(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (info.Length() < 1 || !info[0].IsNumber()) {
    Napi::TypeError::New(env, "Expected TX frequency").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  double tx_freq = info[0].As<Napi::Number>().DoubleValue();
  
  // Basic frequency range validation
  if (tx_freq < 1000 || tx_freq > 10000000000) { // 1 kHz to 10 GHz reasonable range
    Napi::Error::New(env, "Frequency out of range (1 kHz - 10 GHz)").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  int vfo = SHIM_RIG_VFO_CURR;
  
  if (info.Length() >= 2 && info[1].IsString()) {
    // setSplitFreq(freq, vfo)
    vfo = parseVfoParameter(info, 1, SHIM_RIG_VFO_CURR);
    RETURN_NULL_IF_INVALID_VFO(vfo);
  }
  
  SetSplitFreqAsyncWorker* asyncWorker = new SetSplitFreqAsyncWorker(env, this, tx_freq, vfo);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::GetSplitFreq(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  int vfo = SHIM_RIG_VFO_CURR;
  
  if (info.Length() >= 1 && info[0].IsString()) {
    // getSplitFreq(vfo)
    vfo = parseVfoParameter(info, 0, SHIM_RIG_VFO_CURR);
    RETURN_NULL_IF_INVALID_VFO(vfo);
  }
  // Otherwise use default SHIM_RIG_VFO_CURR for getSplitFreq()
  
  GetSplitFreqAsyncWorker* asyncWorker = new GetSplitFreqAsyncWorker(env, this, vfo);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::SetSplitMode(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected TX mode string").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  std::string modeStr = info[0].As<Napi::String>().Utf8Value();
  int tx_mode = shim_rig_parse_mode(modeStr.c_str());
  
  if (tx_mode == SHIM_RIG_MODE_NONE) {
    Napi::Error::New(env, "Invalid mode: " + modeStr).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  int tx_width = SHIM_RIG_PASSBAND_NORMAL;
  int vfo = SHIM_RIG_VFO_CURR;
  
  // Parse parameters: setSplitMode(mode) | setSplitMode(mode, vfo) | setSplitMode(mode, width) | setSplitMode(mode, width, vfo)
  if (info.Length() == 2) {
    if (info[1].IsString()) {
      // setSplitMode(mode, vfo)
      vfo = parseVfoParameter(info, 1, SHIM_RIG_VFO_CURR);
      RETURN_NULL_IF_INVALID_VFO(vfo);
    } else if (info[1].IsNumber()) {
      // setSplitMode(mode, width)
      tx_width = info[1].As<Napi::Number>().Int32Value();
    }
  } else if (info.Length() == 3 && info[1].IsNumber() && info[2].IsString()) {
    // setSplitMode(mode, width, vfo)
    tx_width = info[1].As<Napi::Number>().Int32Value();
    vfo = parseVfoParameter(info, 2, SHIM_RIG_VFO_CURR);
    RETURN_NULL_IF_INVALID_VFO(vfo);
  }
  
  SetSplitModeAsyncWorker* asyncWorker = new SetSplitModeAsyncWorker(env, this, tx_mode, tx_width, vfo);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::GetSplitMode(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  int vfo = SHIM_RIG_VFO_CURR;
  
  if (info.Length() >= 1 && info[0].IsString()) {
    // getSplitMode(vfo)
    vfo = parseVfoParameter(info, 0, SHIM_RIG_VFO_CURR);
    RETURN_NULL_IF_INVALID_VFO(vfo);
  }
  // Otherwise use default SHIM_RIG_VFO_CURR for getSplitMode()
  
  GetSplitModeAsyncWorker* asyncWorker = new GetSplitModeAsyncWorker(env, this, vfo);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::SetSplit(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (info.Length() < 1 || !info[0].IsBoolean()) {
    Napi::TypeError::New(env, "Expected boolean split enable state").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  bool split_enabled = info[0].As<Napi::Boolean>().Value();
  int split = split_enabled ? SHIM_RIG_SPLIT_ON : SHIM_RIG_SPLIT_OFF;
  
  // Default values
  int rx_vfo = SHIM_RIG_VFO_CURR;
  int tx_vfo = SHIM_RIG_VFO_CURR;
  
  // ⚠️  CRITICAL HISTORICAL ISSUE WARNING ⚠️
  // This Split API had a severe parameter order bug that caused AI to repeatedly
  // modify but never correctly fix. See CLAUDE.md for full details.
  // 
  // Parse different overloads:
  // setSplit(enable)
  // setSplit(enable, rxVfo) - rxVfo here is RX VFO
  // setSplit(enable, rxVfo, txVfo) - RX VFO and TX VFO
  // 
  // ⚠️  VERIFIED PARAMETER ORDER (2024-08-10):
  // - info[1] = rxVfo (RX VFO) 
  // - info[2] = txVfo (TX VFO)
  // - Must match JavaScript: setSplit(enable, rxVfo, txVfo)
  // - Must match TypeScript: setSplit(enable: boolean, rxVfo?: VFO, txVfo?: VFO)
  
  if (info.Length() == 2 && info[1].IsString()) {
    // setSplit(enable, rxVfo) - treating vfo as RX VFO for current VFO operation
    rx_vfo = parseVfoParameter(info, 1, SHIM_RIG_VFO_CURR);
    RETURN_NULL_IF_INVALID_VFO(rx_vfo);
  } else if (info.Length() == 3 && info[1].IsString() && info[2].IsString()) {
    // setSplit(enable, rxVfo, txVfo)
    // ⚠️  CRITICAL: Parameter assignment was WRONG before 2024-08-10 fix!
    // ⚠️  Previous bug: info[1] -> txVfoStr, info[2] -> rxVfoStr (WRONG!)
    // ⚠️  Correct now: info[1] -> rxVfoStr, info[2] -> txVfoStr (RIGHT!)
    rx_vfo = parseVfoParameter(info, 1, SHIM_RIG_VFO_CURR);
    RETURN_NULL_IF_INVALID_VFO(rx_vfo);
    tx_vfo = parseVfoParameter(info, 2, SHIM_RIG_VFO_CURR);
    RETURN_NULL_IF_INVALID_VFO(tx_vfo);
  }
  
  SetSplitAsyncWorker* asyncWorker = new SetSplitAsyncWorker(env, this, rx_vfo, split, tx_vfo);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::GetSplit(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  int vfo = SHIM_RIG_VFO_CURR;
  
  if (info.Length() >= 1 && info[0].IsString()) {
    // getSplit(vfo)
    vfo = parseVfoParameter(info, 0, SHIM_RIG_VFO_CURR);
    RETURN_NULL_IF_INVALID_VFO(vfo);
  }
  // Otherwise use default SHIM_RIG_VFO_CURR for getSplit()
  
  GetSplitAsyncWorker* asyncWorker = new GetSplitAsyncWorker(env, this, vfo);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// VFO Operations
Napi::Value NodeHamLib::VfoOperation(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected VFO operation string").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  std::string vfoOpStr = info[0].As<Napi::String>().Utf8Value();
  
  int vfo_op;
  if (vfoOpStr == "CPY") {
    vfo_op = SHIM_RIG_OP_CPY;
  } else if (vfoOpStr == "XCHG") {
    vfo_op = SHIM_RIG_OP_XCHG;
  } else if (vfoOpStr == "FROM_VFO") {
    vfo_op = SHIM_RIG_OP_FROM_VFO;
  } else if (vfoOpStr == "TO_VFO") {
    vfo_op = SHIM_RIG_OP_TO_VFO;
  } else if (vfoOpStr == "MCL") {
    vfo_op = SHIM_RIG_OP_MCL;
  } else if (vfoOpStr == "UP") {
    vfo_op = SHIM_RIG_OP_UP;
  } else if (vfoOpStr == "DOWN") {
    vfo_op = SHIM_RIG_OP_DOWN;
  } else if (vfoOpStr == "BAND_UP") {
    vfo_op = SHIM_RIG_OP_BAND_UP;
  } else if (vfoOpStr == "BAND_DOWN") {
    vfo_op = SHIM_RIG_OP_BAND_DOWN;
  } else if (vfoOpStr == "LEFT") {
    vfo_op = SHIM_RIG_OP_LEFT;
  } else if (vfoOpStr == "RIGHT") {
    vfo_op = SHIM_RIG_OP_RIGHT;
  } else if (vfoOpStr == "TUNE") {
    vfo_op = SHIM_RIG_OP_TUNE;
  } else if (vfoOpStr == "TOGGLE") {
    vfo_op = SHIM_RIG_OP_TOGGLE;
  } else {
    Napi::TypeError::New(env, "Invalid VFO operation").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  VfoOperationAsyncWorker* asyncWorker = new VfoOperationAsyncWorker(env, this, vfo_op);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// Antenna Selection
Napi::Value NodeHamLib::SetAntenna(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (info.Length() < 1 || !info[0].IsNumber()) {
    Napi::TypeError::New(env, "Expected antenna number").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  int antenna = info[0].As<Napi::Number>().Int32Value();
  
  // Support optional VFO parameter: setAntenna(antenna) or setAntenna(antenna, vfo)
  int vfo = SHIM_RIG_VFO_CURR;
  if (info.Length() >= 2 && info[1].IsString()) {
    vfo = parseVfoParameter(info, 1, SHIM_RIG_VFO_CURR);
    RETURN_NULL_IF_INVALID_VFO(vfo);
  }
  
  // Default option value (can be extended later if needed)
  float option = 0.0f;

  SetAntennaAsyncWorker* asyncWorker = new SetAntennaAsyncWorker(env, this, antenna, vfo, option);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::GetAntenna(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  // Support optional VFO parameter: getAntenna() or getAntenna(vfo)
  int vfo = SHIM_RIG_VFO_CURR;
  if (info.Length() >= 1 && info[0].IsString()) {
    vfo = parseVfoParameter(info, 0, SHIM_RIG_VFO_CURR);
    RETURN_NULL_IF_INVALID_VFO(vfo);
  }
  
  // Default antenna query (SHIM_RIG_ANT_CURR gets all antenna info)
  int antenna = SHIM_RIG_ANT_CURR;
  
  GetAntennaAsyncWorker* asyncWorker = new GetAntennaAsyncWorker(env, this, vfo, antenna);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Function NodeHamLib::GetClass(Napi::Env env) {
  auto ret =  DefineClass(
    env,
    "HamLib", {
      NodeHamLib::InstanceMethod("open", & NodeHamLib::Open),
      NodeHamLib::InstanceMethod("setVfo", & NodeHamLib::SetVFO),
      NodeHamLib::InstanceMethod("setFrequency", & NodeHamLib::SetFrequency),
      NodeHamLib::InstanceMethod("setMode", & NodeHamLib::SetMode),
      NodeHamLib::InstanceMethod("setPtt", & NodeHamLib::SetPtt),
      NodeHamLib::InstanceMethod("getVfo", & NodeHamLib::GetVFO),
      NodeHamLib::InstanceMethod("getFrequency", & NodeHamLib::GetFrequency),
      NodeHamLib::InstanceMethod("getMode", & NodeHamLib::GetMode),
      NodeHamLib::InstanceMethod("getStrength", & NodeHamLib::GetStrength),
      
      // Memory Channel Management
      NodeHamLib::InstanceMethod("setMemoryChannel", & NodeHamLib::SetMemoryChannel),
      NodeHamLib::InstanceMethod("getMemoryChannel", & NodeHamLib::GetMemoryChannel),
      NodeHamLib::InstanceMethod("selectMemoryChannel", & NodeHamLib::SelectMemoryChannel),
      
      // RIT/XIT Control
      NodeHamLib::InstanceMethod("setRit", & NodeHamLib::SetRit),
      NodeHamLib::InstanceMethod("getRit", & NodeHamLib::GetRit),
      NodeHamLib::InstanceMethod("setXit", & NodeHamLib::SetXit),
      NodeHamLib::InstanceMethod("getXit", & NodeHamLib::GetXit),
      NodeHamLib::InstanceMethod("clearRitXit", & NodeHamLib::ClearRitXit),
      
      // Scanning Operations
      NodeHamLib::InstanceMethod("startScan", & NodeHamLib::StartScan),
      NodeHamLib::InstanceMethod("stopScan", & NodeHamLib::StopScan),
      
      // Level Controls
      NodeHamLib::InstanceMethod("setLevel", & NodeHamLib::SetLevel),
      NodeHamLib::InstanceMethod("getLevel", & NodeHamLib::GetLevel),
      NodeHamLib::InstanceMethod("getSupportedLevels", & NodeHamLib::GetSupportedLevels),
      
      // Function Controls
      NodeHamLib::InstanceMethod("setFunction", & NodeHamLib::SetFunction),
      NodeHamLib::InstanceMethod("getFunction", & NodeHamLib::GetFunction),
      NodeHamLib::InstanceMethod("getSupportedFunctions", & NodeHamLib::GetSupportedFunctions),

      // Mode Query
      NodeHamLib::InstanceMethod("getSupportedModes", & NodeHamLib::GetSupportedModes),

      // Split Operations
      NodeHamLib::InstanceMethod("setSplitFreq", & NodeHamLib::SetSplitFreq),
      NodeHamLib::InstanceMethod("getSplitFreq", & NodeHamLib::GetSplitFreq),
      NodeHamLib::InstanceMethod("setSplitMode", & NodeHamLib::SetSplitMode),
      NodeHamLib::InstanceMethod("getSplitMode", & NodeHamLib::GetSplitMode),
      NodeHamLib::InstanceMethod("setSplit", & NodeHamLib::SetSplit),
      NodeHamLib::InstanceMethod("getSplit", & NodeHamLib::GetSplit),
      
      // VFO Operations
      NodeHamLib::InstanceMethod("vfoOperation", & NodeHamLib::VfoOperation),
      
      // Antenna Selection
      NodeHamLib::InstanceMethod("setAntenna", & NodeHamLib::SetAntenna),
      NodeHamLib::InstanceMethod("getAntenna", & NodeHamLib::GetAntenna),
      
      // Serial Port Configuration
      NodeHamLib::InstanceMethod("setSerialConfig", & NodeHamLib::SetSerialConfig),
      NodeHamLib::InstanceMethod("getSerialConfig", & NodeHamLib::GetSerialConfig),
      NodeHamLib::InstanceMethod("setPttType", & NodeHamLib::SetPttType),
      NodeHamLib::InstanceMethod("getPttType", & NodeHamLib::GetPttType),
      NodeHamLib::InstanceMethod("setDcdType", & NodeHamLib::SetDcdType),
      NodeHamLib::InstanceMethod("getDcdType", & NodeHamLib::GetDcdType),
      NodeHamLib::InstanceMethod("getSupportedSerialConfigs", & NodeHamLib::GetSupportedSerialConfigs),
      
      // Power Control
      NodeHamLib::InstanceMethod("setPowerstat", & NodeHamLib::SetPowerstat),
      NodeHamLib::InstanceMethod("getPowerstat", & NodeHamLib::GetPowerstat),
      
      // PTT Status Detection 
      NodeHamLib::InstanceMethod("getPtt", & NodeHamLib::GetPtt),
      
      // Data Carrier Detect
      NodeHamLib::InstanceMethod("getDcd", & NodeHamLib::GetDcd),
      
      // Tuning Step Control
      NodeHamLib::InstanceMethod("setTuningStep", & NodeHamLib::SetTuningStep),
      NodeHamLib::InstanceMethod("getTuningStep", & NodeHamLib::GetTuningStep),
      
      // Repeater Control
      NodeHamLib::InstanceMethod("setRepeaterShift", & NodeHamLib::SetRepeaterShift),
      NodeHamLib::InstanceMethod("getRepeaterShift", & NodeHamLib::GetRepeaterShift),
      NodeHamLib::InstanceMethod("setRepeaterOffset", & NodeHamLib::SetRepeaterOffset),
      NodeHamLib::InstanceMethod("getRepeaterOffset", & NodeHamLib::GetRepeaterOffset),
      
      // CTCSS/DCS Tone Control
      NodeHamLib::InstanceMethod("setCtcssTone", & NodeHamLib::SetCtcssTone),
      NodeHamLib::InstanceMethod("getCtcssTone", & NodeHamLib::GetCtcssTone),
      NodeHamLib::InstanceMethod("setDcsCode", & NodeHamLib::SetDcsCode),
      NodeHamLib::InstanceMethod("getDcsCode", & NodeHamLib::GetDcsCode),
      NodeHamLib::InstanceMethod("setCtcssSql", & NodeHamLib::SetCtcssSql),
      NodeHamLib::InstanceMethod("getCtcssSql", & NodeHamLib::GetCtcssSql),
      NodeHamLib::InstanceMethod("setDcsSql", & NodeHamLib::SetDcsSql),
      NodeHamLib::InstanceMethod("getDcsSql", & NodeHamLib::GetDcsSql),
      
      // Parameter Control
      NodeHamLib::InstanceMethod("setParm", & NodeHamLib::SetParm),
      NodeHamLib::InstanceMethod("getParm", & NodeHamLib::GetParm),
      
      // DTMF Support
      NodeHamLib::InstanceMethod("sendDtmf", & NodeHamLib::SendDtmf),
      NodeHamLib::InstanceMethod("recvDtmf", & NodeHamLib::RecvDtmf),
      
      // Memory Channel Advanced Operations
      NodeHamLib::InstanceMethod("getMem", & NodeHamLib::GetMem),
      NodeHamLib::InstanceMethod("setBank", & NodeHamLib::SetBank),
      NodeHamLib::InstanceMethod("memCount", & NodeHamLib::MemCount),
      
      // Morse Code Support
      NodeHamLib::InstanceMethod("sendMorse", & NodeHamLib::SendMorse),
      NodeHamLib::InstanceMethod("stopMorse", & NodeHamLib::StopMorse),
      NodeHamLib::InstanceMethod("waitMorse", & NodeHamLib::WaitMorse),
      
      // Voice Memory Support
      NodeHamLib::InstanceMethod("sendVoiceMem", & NodeHamLib::SendVoiceMem),
      NodeHamLib::InstanceMethod("stopVoiceMem", & NodeHamLib::StopVoiceMem),
      
      // Complex Split Frequency/Mode Operations
      NodeHamLib::InstanceMethod("setSplitFreqMode", & NodeHamLib::SetSplitFreqMode),
      NodeHamLib::InstanceMethod("getSplitFreqMode", & NodeHamLib::GetSplitFreqMode),
      
      // Power Conversion Functions
      NodeHamLib::InstanceMethod("power2mW", & NodeHamLib::Power2mW),
      NodeHamLib::InstanceMethod("mW2power", & NodeHamLib::MW2Power),
      
      // Reset Function
      NodeHamLib::InstanceMethod("reset", & NodeHamLib::Reset),

      // Lock Mode (Hamlib >= 4.7.0)
      NodeHamLib::InstanceMethod("setLockMode", & NodeHamLib::SetLockMode),
      NodeHamLib::InstanceMethod("getLockMode", & NodeHamLib::GetLockMode),

      // Clock (Hamlib >= 4.7.0)
      NodeHamLib::InstanceMethod("setClock", & NodeHamLib::SetClock),
      NodeHamLib::InstanceMethod("getClock", & NodeHamLib::GetClock),

      // VFO Info (Hamlib >= 4.7.0)
      NodeHamLib::InstanceMethod("getVfoInfo", & NodeHamLib::GetVfoInfo),

      // Rig Info / Spectrum / Conf (async)
      NodeHamLib::InstanceMethod("getInfo", & NodeHamLib::GetInfo),
      NodeHamLib::InstanceMethod("sendRaw", & NodeHamLib::SendRaw),
      NodeHamLib::InstanceMethod("getSpectrumCapabilities", & NodeHamLib::GetSpectrumCapabilities),
      NodeHamLib::InstanceMethod("startSpectrumStream", & NodeHamLib::StartSpectrumStream),
      NodeHamLib::InstanceMethod("stopSpectrumStream", & NodeHamLib::StopSpectrumStream),
      NodeHamLib::InstanceMethod("setConf", & NodeHamLib::SetConf),
      NodeHamLib::InstanceMethod("getConf", & NodeHamLib::GetConf),

      // Passband / Resolution (sync)
      NodeHamLib::InstanceMethod("getPassbandNormal", & NodeHamLib::GetPassbandNormal),
      NodeHamLib::InstanceMethod("getPassbandNarrow", & NodeHamLib::GetPassbandNarrow),
      NodeHamLib::InstanceMethod("getPassbandWide", & NodeHamLib::GetPassbandWide),
      NodeHamLib::InstanceMethod("getResolution", & NodeHamLib::GetResolution),

      // Capability queries (sync)
      NodeHamLib::InstanceMethod("getSupportedParms", & NodeHamLib::GetSupportedParms),
      NodeHamLib::InstanceMethod("getSupportedVfoOps", & NodeHamLib::GetSupportedVfoOps),
      NodeHamLib::InstanceMethod("getSupportedScanTypes", & NodeHamLib::GetSupportedScanTypes),

      // Capability queries - batch 2 (sync)
      NodeHamLib::InstanceMethod("getPreampValues", & NodeHamLib::GetPreampValues),
      NodeHamLib::InstanceMethod("getAttenuatorValues", & NodeHamLib::GetAttenuatorValues),
      NodeHamLib::InstanceMethod("getMaxRit", & NodeHamLib::GetMaxRit),
      NodeHamLib::InstanceMethod("getMaxXit", & NodeHamLib::GetMaxXit),
      NodeHamLib::InstanceMethod("getMaxIfShift", & NodeHamLib::GetMaxIfShift),
      NodeHamLib::InstanceMethod("getAvailableCtcssTones", & NodeHamLib::GetAvailableCtcssTones),
      NodeHamLib::InstanceMethod("getAvailableDcsCodes", & NodeHamLib::GetAvailableDcsCodes),
      NodeHamLib::InstanceMethod("getFrequencyRanges", & NodeHamLib::GetFrequencyRanges),
      NodeHamLib::InstanceMethod("getTuningSteps", & NodeHamLib::GetTuningSteps),
      NodeHamLib::InstanceMethod("getFilterList", & NodeHamLib::GetFilterList),

      NodeHamLib::InstanceMethod("close", & NodeHamLib::Close),
      NodeHamLib::InstanceMethod("destroy", & NodeHamLib::Destroy),
      NodeHamLib::InstanceMethod("getConnectionInfo", & NodeHamLib::GetConnectionInfo),

      // Static methods
      NodeHamLib::StaticMethod("getSupportedRigs", & NodeHamLib::GetSupportedRigs),
      NodeHamLib::StaticMethod("getHamlibVersion", & NodeHamLib::GetHamlibVersion),
      NodeHamLib::StaticMethod("setDebugLevel", & NodeHamLib::SetDebugLevel),
      NodeHamLib::StaticMethod("getDebugLevel", & NodeHamLib::GetDebugLevel),
      NodeHamLib::StaticMethod("getCopyright", & NodeHamLib::GetCopyright),
      NodeHamLib::StaticMethod("getLicense", & NodeHamLib::GetLicense),
    });
      constructor = Napi::Persistent(ret);
      constructor.SuppressDestruct();
      return ret;
}

// Helper method to detect network address format (contains colon)
bool NodeHamLib::isNetworkAddress(const char* path) {
  if (path == nullptr) {
    return false;
  }
  
  // Check for IPv4 or hostname with port (e.g., "localhost:4532", "192.168.1.1:4532")
  const char* colon = strchr(path, ':');
  if (colon != nullptr) {
    // Make sure there's something after the colon (port number)
    if (strlen(colon + 1) > 0) {
      return true;
    }
  }
  
  return false;
}

// Static callback function for rig_list_foreach
int NodeHamLib::rig_list_callback(const shim_rig_info_t *info, void *data) {
  RigListData *rig_data = static_cast<RigListData*>(data);

  // Create rig info object
  Napi::Object rigInfo = Napi::Object::New(rig_data->env);
  rigInfo.Set(Napi::String::New(rig_data->env, "rigModel"),
              Napi::Number::New(rig_data->env, info->rig_model));
  rigInfo.Set(Napi::String::New(rig_data->env, "modelName"),
              Napi::String::New(rig_data->env, info->model_name ? info->model_name : ""));
  rigInfo.Set(Napi::String::New(rig_data->env, "mfgName"),
              Napi::String::New(rig_data->env, info->mfg_name ? info->mfg_name : ""));
  rigInfo.Set(Napi::String::New(rig_data->env, "version"),
              Napi::String::New(rig_data->env, info->version ? info->version : ""));
  rigInfo.Set(Napi::String::New(rig_data->env, "status"),
              Napi::String::New(rig_data->env, shim_rig_strstatus(info->status)));

  // Determine rig type string using shim helper
  const char* rigType = shim_rig_type_str(info->rig_type);

  rigInfo.Set(Napi::String::New(rig_data->env, "rigType"),
              Napi::String::New(rig_data->env, rigType));

  // Add to list
  rig_data->rigList.push_back(rigInfo);

  return 1; // Continue iteration (returning 0 would stop)
}

// Static method to get supported rig models
Napi::Value NodeHamLib::GetSupportedRigs(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  // Load all backends to ensure we get the complete list
  shim_rig_load_all_backends();
  
  // Prepare data structure for callback with proper initialization
  RigListData rigData{std::vector<Napi::Object>(), env};
  
  // Call hamlib function to iterate through all supported rigs
  int result = shim_rig_list_foreach(rig_list_callback, &rigData);
  
  if (result != SHIM_RIG_OK) {
    Napi::Error::New(env, "Failed to retrieve supported rig list").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  // Convert std::vector to Napi::Array
  Napi::Array rigArray = Napi::Array::New(env, rigData.rigList.size());
  for (size_t i = 0; i < rigData.rigList.size(); i++) {
    rigArray[i] = rigData.rigList[i];
  }
  
  return rigArray;
}

// Get Hamlib version information
Napi::Value NodeHamLib::GetHamlibVersion(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  return Napi::String::New(env, shim_rig_get_version());
}

// Set Hamlib debug level
// Debug levels: 0=NONE, 1=BUG, 2=ERR, 3=WARN, 4=VERBOSE, 5=TRACE
Napi::Value NodeHamLib::SetDebugLevel(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsNumber()) {
    Napi::TypeError::New(env, "Debug level (number) required").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  int level = info[0].As<Napi::Number>().Int32Value();

  // Validate debug level (0-5)
  if (level < 0 || level > 5) {
    Napi::RangeError::New(env, "Debug level must be between 0 (NONE) and 5 (TRACE)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  shim_rig_set_debug(level);

  return env.Undefined();
}

// Get current Hamlib debug level
Napi::Value NodeHamLib::GetDebugLevel(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // hamlib_get_debug() is not available in all versions, so we use a workaround
  // by calling rig_debug_level which is a global variable in hamlib
  // However, this is internal implementation, so we use rig_debug with RIG_DEBUG_NONE
  // to get the current level. For now, we'll just return a note that this is not
  // directly accessible. In practice, applications should track the level they set.

  // Since there's no public API to query debug level in Hamlib,
  // we'll document that users should track it themselves
  Napi::Error::New(env,
    "Getting debug level is not supported by Hamlib API. "
    "Please track the debug level you set using setDebugLevel().").ThrowAsJavaScriptException();
  return env.Undefined();
}

// Serial Port Configuration Methods

// Set serial configuration parameter (data_bits, stop_bits, parity, handshake, etc.)
Napi::Value NodeHamLib::SetSerialConfig(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() < 2 || !info[0].IsString() || !info[1].IsString()) {
    Napi::TypeError::New(env, "Expected parameter name and value as strings").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  std::string paramName = info[0].As<Napi::String>().Utf8Value();
  std::string paramValue = info[1].As<Napi::String>().Utf8Value();
  
  SetSerialConfigAsyncWorker* asyncWorker = new SetSerialConfigAsyncWorker(env, this, paramName, paramValue);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// Get serial configuration parameter
Napi::Value NodeHamLib::GetSerialConfig(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected parameter name as string").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  std::string paramName = info[0].As<Napi::String>().Utf8Value();
  
  GetSerialConfigAsyncWorker* asyncWorker = new GetSerialConfigAsyncWorker(env, this, paramName);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// Set PTT type
Napi::Value NodeHamLib::SetPttType(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected PTT type as string").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  std::string pttTypeStr = info[0].As<Napi::String>().Utf8Value();
  
  SetPttTypeAsyncWorker* asyncWorker = new SetPttTypeAsyncWorker(env, this, pttTypeStr);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// Get PTT type
Napi::Value NodeHamLib::GetPttType(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  GetPttTypeAsyncWorker* asyncWorker = new GetPttTypeAsyncWorker(env, this);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// Set DCD type
Napi::Value NodeHamLib::SetDcdType(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected DCD type as string").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  std::string dcdTypeStr = info[0].As<Napi::String>().Utf8Value();
  
  SetDcdTypeAsyncWorker* asyncWorker = new SetDcdTypeAsyncWorker(env, this, dcdTypeStr);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// Get DCD type
Napi::Value NodeHamLib::GetDcdType(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  GetDcdTypeAsyncWorker* asyncWorker = new GetDcdTypeAsyncWorker(env, this);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// Get supported serial configuration options
Napi::Value NodeHamLib::GetSupportedSerialConfigs(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  // Return object with supported configuration parameters and their possible values
  Napi::Object configs = Napi::Object::New(env);
  
  // Serial basic parameters
  Napi::Object serialParams = Napi::Object::New(env);
  
  // Data bits options
  Napi::Array dataBitsOptions = Napi::Array::New(env, 4);
  dataBitsOptions[0u] = Napi::String::New(env, "5");
  dataBitsOptions[1u] = Napi::String::New(env, "6");
  dataBitsOptions[2u] = Napi::String::New(env, "7");
  dataBitsOptions[3u] = Napi::String::New(env, "8");
  serialParams.Set("data_bits", dataBitsOptions);
  
  // Stop bits options
  Napi::Array stopBitsOptions = Napi::Array::New(env, 2);
  stopBitsOptions[0u] = Napi::String::New(env, "1");
  stopBitsOptions[1u] = Napi::String::New(env, "2");
  serialParams.Set("stop_bits", stopBitsOptions);
  
  // Parity options
  Napi::Array parityOptions = Napi::Array::New(env, 3);
  parityOptions[0u] = Napi::String::New(env, "None");
  parityOptions[1u] = Napi::String::New(env, "Even");
  parityOptions[2u] = Napi::String::New(env, "Odd");
  serialParams.Set("serial_parity", parityOptions);
  
  // Handshake options
  Napi::Array handshakeOptions = Napi::Array::New(env, 3);
  handshakeOptions[0u] = Napi::String::New(env, "None");
  handshakeOptions[1u] = Napi::String::New(env, "Hardware");
  handshakeOptions[2u] = Napi::String::New(env, "Software");
  serialParams.Set("serial_handshake", handshakeOptions);
  
  // Control line state options
  Napi::Array stateOptions = Napi::Array::New(env, 2);
  stateOptions[0u] = Napi::String::New(env, "ON");
  stateOptions[1u] = Napi::String::New(env, "OFF");
  serialParams.Set("rts_state", stateOptions);
  serialParams.Set("dtr_state", stateOptions);
  
  configs.Set("serial", serialParams);
  
  // PTT type options
  Napi::Array pttTypeOptions = Napi::Array::New(env, 8);
  pttTypeOptions[0u] = Napi::String::New(env, "RIG");
  pttTypeOptions[1u] = Napi::String::New(env, "DTR");
  pttTypeOptions[2u] = Napi::String::New(env, "RTS");
  pttTypeOptions[3u] = Napi::String::New(env, "PARALLEL");
  pttTypeOptions[4u] = Napi::String::New(env, "CM108");
  pttTypeOptions[5u] = Napi::String::New(env, "GPIO");
  pttTypeOptions[6u] = Napi::String::New(env, "GPION");
  pttTypeOptions[7u] = Napi::String::New(env, "NONE");
  configs.Set("intype", pttTypeOptions);
  
  // DCD type options
  Napi::Array dcdTypeOptions = Napi::Array::New(env, 9);
  dcdTypeOptions[0u] = Napi::String::New(env, "RIG");
  dcdTypeOptions[1u] = Napi::String::New(env, "DSR");
  dcdTypeOptions[2u] = Napi::String::New(env, "CTS");
  dcdTypeOptions[3u] = Napi::String::New(env, "CD");
  dcdTypeOptions[4u] = Napi::String::New(env, "PARALLEL");
  dcdTypeOptions[5u] = Napi::String::New(env, "CM108");
  dcdTypeOptions[6u] = Napi::String::New(env, "GPIO");
  dcdTypeOptions[7u] = Napi::String::New(env, "GPION");
  dcdTypeOptions[8u] = Napi::String::New(env, "NONE");
  configs.Set("intype", dcdTypeOptions);
  
  return configs;
}

// Power Control Methods
Napi::Value NodeHamLib::SetPowerstat(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() < 1 || !info[0].IsNumber()) {
    Napi::TypeError::New(env, "Expected power status as number").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  int powerStatus = info[0].As<Napi::Number>().Int32Value();
  int status = static_cast<int>(powerStatus);
  
  SetPowerstatAsyncWorker* asyncWorker = new SetPowerstatAsyncWorker(env, this, status);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::GetPowerstat(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  GetPowerstatAsyncWorker* asyncWorker = new GetPowerstatAsyncWorker(env, this);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// PTT Status Detection
Napi::Value NodeHamLib::GetPtt(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  int vfo = parseVfoParameter(info, 0, SHIM_RIG_VFO_CURR);
  RETURN_NULL_IF_INVALID_VFO(vfo);
  
  GetPttAsyncWorker* asyncWorker = new GetPttAsyncWorker(env, this, vfo);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// Data Carrier Detect
Napi::Value NodeHamLib::GetDcd(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  int vfo = parseVfoParameter(info, 0, SHIM_RIG_VFO_CURR);
  RETURN_NULL_IF_INVALID_VFO(vfo);
  
  GetDcdAsyncWorker* asyncWorker = new GetDcdAsyncWorker(env, this, vfo);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// Tuning Step Control
Napi::Value NodeHamLib::SetTuningStep(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() < 1 || !info[0].IsNumber()) {
    Napi::TypeError::New(env, "Expected tuning step as number").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  int ts = info[0].As<Napi::Number>().Int32Value();
  int vfo = parseVfoParameter(info, 1, SHIM_RIG_VFO_CURR);
  RETURN_NULL_IF_INVALID_VFO(vfo);
  
  SetTuningStepAsyncWorker* asyncWorker = new SetTuningStepAsyncWorker(env, this, vfo, ts);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::GetTuningStep(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  int vfo = parseVfoParameter(info, 0, SHIM_RIG_VFO_CURR);
  RETURN_NULL_IF_INVALID_VFO(vfo);
  
  GetTuningStepAsyncWorker* asyncWorker = new GetTuningStepAsyncWorker(env, this, vfo);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// Repeater Control
Napi::Value NodeHamLib::SetRepeaterShift(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected repeater shift as string").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  std::string shiftStr = info[0].As<Napi::String>().Utf8Value();
  int shift = SHIM_RIG_RPT_SHIFT_NONE;
  
  if (shiftStr == "NONE" || shiftStr == "none") {
    shift = SHIM_RIG_RPT_SHIFT_NONE;
  } else if (shiftStr == "MINUS" || shiftStr == "minus" || shiftStr == "-") {
    shift = SHIM_RIG_RPT_SHIFT_MINUS;
  } else if (shiftStr == "PLUS" || shiftStr == "plus" || shiftStr == "+") {
    shift = SHIM_RIG_RPT_SHIFT_PLUS;
  } else {
    Napi::TypeError::New(env, "Invalid repeater shift (must be 'NONE', 'MINUS', or 'PLUS')").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  int vfo = parseVfoParameter(info, 1, SHIM_RIG_VFO_CURR);
  RETURN_NULL_IF_INVALID_VFO(vfo);
  
  SetRepeaterShiftAsyncWorker* asyncWorker = new SetRepeaterShiftAsyncWorker(env, this, vfo, shift);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::GetRepeaterShift(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  int vfo = parseVfoParameter(info, 0, SHIM_RIG_VFO_CURR);
  RETURN_NULL_IF_INVALID_VFO(vfo);
  
  GetRepeaterShiftAsyncWorker* asyncWorker = new GetRepeaterShiftAsyncWorker(env, this, vfo);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::SetRepeaterOffset(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() < 1 || !info[0].IsNumber()) {
    Napi::TypeError::New(env, "Expected repeater offset as number").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  int offset = info[0].As<Napi::Number>().Int32Value();
  int vfo = parseVfoParameter(info, 1, SHIM_RIG_VFO_CURR);
  RETURN_NULL_IF_INVALID_VFO(vfo);
  
  SetRepeaterOffsetAsyncWorker* asyncWorker = new SetRepeaterOffsetAsyncWorker(env, this, vfo, offset);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::GetRepeaterOffset(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  int vfo = parseVfoParameter(info, 0, SHIM_RIG_VFO_CURR);
  RETURN_NULL_IF_INVALID_VFO(vfo);
  
  GetRepeaterOffsetAsyncWorker* asyncWorker = new GetRepeaterOffsetAsyncWorker(env, this, vfo);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// CTCSS/DCS Tone Control AsyncWorker classes
class SetCtcssToneAsyncWorker : public HamLibAsyncWorker {
public:
    SetCtcssToneAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo, unsigned int tone)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), tone_(tone) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_set_ctcss_tone(hamlib_instance_->my_rig, vfo_, tone_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
    unsigned int tone_;
};

class GetCtcssToneAsyncWorker : public HamLibAsyncWorker {
public:
    GetCtcssToneAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), tone_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_get_ctcss_tone(hamlib_instance_->my_rig, vfo_, &tone_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, tone_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
    unsigned int tone_;
};

class SetDcsCodeAsyncWorker : public HamLibAsyncWorker {
public:
    SetDcsCodeAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo, unsigned int code)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), code_(code) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_set_dcs_code(hamlib_instance_->my_rig, vfo_, code_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
    unsigned int code_;
};

class GetDcsCodeAsyncWorker : public HamLibAsyncWorker {
public:
    GetDcsCodeAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), code_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_get_dcs_code(hamlib_instance_->my_rig, vfo_, &code_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
    unsigned int code_;
};

class SetCtcssSqlAsyncWorker : public HamLibAsyncWorker {
public:
    SetCtcssSqlAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo, unsigned int tone)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), tone_(tone) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_set_ctcss_sql(hamlib_instance_->my_rig, vfo_, tone_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
    unsigned int tone_;
};

class GetCtcssSqlAsyncWorker : public HamLibAsyncWorker {
public:
    GetCtcssSqlAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), tone_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_get_ctcss_sql(hamlib_instance_->my_rig, vfo_, &tone_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, tone_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
    unsigned int tone_;
};

class SetDcsSqlAsyncWorker : public HamLibAsyncWorker {
public:
    SetDcsSqlAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo, unsigned int code)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), code_(code) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_set_dcs_sql(hamlib_instance_->my_rig, vfo_, code_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
    unsigned int code_;
};

class GetDcsSqlAsyncWorker : public HamLibAsyncWorker {
public:
    GetDcsSqlAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), code_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_get_dcs_sql(hamlib_instance_->my_rig, vfo_, &code_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
    unsigned int code_;
};

// Parameter Control AsyncWorker classes
class SetParmAsyncWorker : public HamLibAsyncWorker {
public:
    SetParmAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, uint64_t parm, float value)
        : HamLibAsyncWorker(env, hamlib_instance), parm_(parm), value_(value) {}

    void Execute() override {
        CHECK_RIG_VALID();

        result_code_ = shim_rig_set_parm_f(hamlib_instance_->my_rig, parm_, value_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }

    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }

    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }

private:
    uint64_t parm_;
    float value_;
};

class GetParmAsyncWorker : public HamLibAsyncWorker {
public:
    GetParmAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, uint64_t parm)
        : HamLibAsyncWorker(env, hamlib_instance), parm_(parm), value_(0.0f) {}

    void Execute() override {
        CHECK_RIG_VALID();

        result_code_ = shim_rig_get_parm_f(hamlib_instance_->my_rig, parm_, &value_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }

    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, value_));
        }
    }

    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }

private:
    uint64_t parm_;
    float value_;
};

// DTMF Support AsyncWorker classes
class SendDtmfAsyncWorker : public HamLibAsyncWorker {
public:
    SendDtmfAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo, const std::string& digits)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), digits_(digits) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_send_dtmf(hamlib_instance_->my_rig, vfo_, digits_.c_str());
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
    std::string digits_;
};

class RecvDtmfAsyncWorker : public HamLibAsyncWorker {
public:
    RecvDtmfAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo, int maxLength)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), max_length_(maxLength), length_(0) {
        digits_.resize(maxLength + 1, '\0');
    }
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        length_ = max_length_;
        // rig_recv_dtmf expects a mutable buffer (char*). Ensure non-const pointer.
        result_code_ = shim_rig_recv_dtmf(hamlib_instance_->my_rig, vfo_, &digits_[0], &length_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            Napi::Object obj = Napi::Object::New(env);
            obj.Set(Napi::String::New(env, "digits"), Napi::String::New(env, digits_.substr(0, length_)));
            obj.Set(Napi::String::New(env, "length"), Napi::Number::New(env, length_));
            deferred_.Resolve(obj);
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
    int max_length_;
    int length_;
    std::string digits_;
};

// Memory Channel Advanced Operations AsyncWorker classes
class GetMemAsyncWorker : public HamLibAsyncWorker {
public:
    GetMemAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), ch_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_get_mem(hamlib_instance_->my_rig, vfo_, &ch_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, ch_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
    int ch_;
};

class SetBankAsyncWorker : public HamLibAsyncWorker {
public:
    SetBankAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo, int bank)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), bank_(bank) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_set_bank(hamlib_instance_->my_rig, vfo_, bank_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
    int bank_;
};

class MemCountAsyncWorker : public HamLibAsyncWorker {
public:
    MemCountAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance)
        : HamLibAsyncWorker(env, hamlib_instance), count_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        count_ = shim_rig_mem_count(hamlib_instance_->my_rig);
        if (count_ < 0) {
            result_code_ = count_;
            error_message_ = shim_rigerror(result_code_);
        } else {
            result_code_ = SHIM_RIG_OK;
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, count_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int count_;
};

// Morse Code Support AsyncWorker classes
class SendMorseAsyncWorker : public HamLibAsyncWorker {
public:
    SendMorseAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo, const std::string& msg)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), msg_(msg) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_send_morse(hamlib_instance_->my_rig, vfo_, msg_.c_str());
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
    std::string msg_;
};

class StopMorseAsyncWorker : public HamLibAsyncWorker {
public:
    StopMorseAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_stop_morse(hamlib_instance_->my_rig, vfo_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
};

class WaitMorseAsyncWorker : public HamLibAsyncWorker {
public:
    WaitMorseAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_wait_morse(hamlib_instance_->my_rig, vfo_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
};

// Voice Memory Support AsyncWorker classes
class SendVoiceMemAsyncWorker : public HamLibAsyncWorker {
public:
    SendVoiceMemAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo, int ch)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), ch_(ch) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_send_voice_mem(hamlib_instance_->my_rig, vfo_, ch_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
    int ch_;
};

class StopVoiceMemAsyncWorker : public HamLibAsyncWorker {
public:
    StopVoiceMemAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_stop_voice_mem(hamlib_instance_->my_rig, vfo_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
};

// Complex Split Frequency/Mode Operations AsyncWorker classes
class SetSplitFreqModeAsyncWorker : public HamLibAsyncWorker {
public:
    SetSplitFreqModeAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo, 
                               double tx_freq, int tx_mode, int tx_width)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), tx_freq_(tx_freq), 
          tx_mode_(tx_mode), tx_width_(tx_width) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_set_split_freq_mode(hamlib_instance_->my_rig, vfo_, tx_freq_, tx_mode_, tx_width_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
    double tx_freq_;
    int tx_mode_;
    int tx_width_;
};

class GetSplitFreqModeAsyncWorker : public HamLibAsyncWorker {
public:
    GetSplitFreqModeAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), tx_freq_(0), tx_mode_(SHIM_RIG_MODE_NONE), tx_width_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_get_split_freq_mode(hamlib_instance_->my_rig, vfo_, &tx_freq_, &tx_mode_, &tx_width_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            Napi::Object obj = Napi::Object::New(env);
            obj.Set(Napi::String::New(env, "txFrequency"), Napi::Number::New(env, static_cast<double>(tx_freq_)));
            obj.Set(Napi::String::New(env, "txMode"), Napi::String::New(env, shim_rig_strrmode(tx_mode_)));
            obj.Set(Napi::String::New(env, "txWidth"), Napi::Number::New(env, static_cast<double>(tx_width_)));
            deferred_.Resolve(obj);
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int vfo_;
    double tx_freq_;
    int tx_mode_;
    int tx_width_;
};

// Reset Function AsyncWorker class
class ResetAsyncWorker : public HamLibAsyncWorker {
public:
    ResetAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int reset)
        : HamLibAsyncWorker(env, hamlib_instance), reset_(reset) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = shim_rig_reset(hamlib_instance_->my_rig, reset_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    int reset_;
};

// CTCSS/DCS Tone Control Methods Implementation
Napi::Value NodeHamLib::SetCtcssTone(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() < 1 || !info[0].IsNumber()) {
    Napi::TypeError::New(env, "Expected tone frequency as number").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  unsigned int tone = static_cast<unsigned int>(info[0].As<Napi::Number>().Uint32Value());
  int vfo = parseVfoParameter(info, 1, SHIM_RIG_VFO_CURR);
  RETURN_NULL_IF_INVALID_VFO(vfo);
  
  SetCtcssToneAsyncWorker* asyncWorker = new SetCtcssToneAsyncWorker(env, this, vfo, tone);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::GetCtcssTone(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  int vfo = parseVfoParameter(info, 0, SHIM_RIG_VFO_CURR);
  RETURN_NULL_IF_INVALID_VFO(vfo);
  
  GetCtcssToneAsyncWorker* asyncWorker = new GetCtcssToneAsyncWorker(env, this, vfo);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::SetDcsCode(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() < 1 || !info[0].IsNumber()) {
    Napi::TypeError::New(env, "Expected DCS code as number").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  unsigned int code = static_cast<unsigned int>(info[0].As<Napi::Number>().Uint32Value());
  int vfo = parseVfoParameter(info, 1, SHIM_RIG_VFO_CURR);
  RETURN_NULL_IF_INVALID_VFO(vfo);
  
  SetDcsCodeAsyncWorker* asyncWorker = new SetDcsCodeAsyncWorker(env, this, vfo, code);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::GetDcsCode(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  int vfo = parseVfoParameter(info, 0, SHIM_RIG_VFO_CURR);
  RETURN_NULL_IF_INVALID_VFO(vfo);
  
  GetDcsCodeAsyncWorker* asyncWorker = new GetDcsCodeAsyncWorker(env, this, vfo);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::SetCtcssSql(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() < 1 || !info[0].IsNumber()) {
    Napi::TypeError::New(env, "Expected CTCSS SQL tone frequency as number").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  unsigned int tone = static_cast<unsigned int>(info[0].As<Napi::Number>().Uint32Value());
  int vfo = parseVfoParameter(info, 1, SHIM_RIG_VFO_CURR);
  RETURN_NULL_IF_INVALID_VFO(vfo);
  
  SetCtcssSqlAsyncWorker* asyncWorker = new SetCtcssSqlAsyncWorker(env, this, vfo, tone);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::GetCtcssSql(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  int vfo = parseVfoParameter(info, 0, SHIM_RIG_VFO_CURR);
  RETURN_NULL_IF_INVALID_VFO(vfo);
  
  GetCtcssSqlAsyncWorker* asyncWorker = new GetCtcssSqlAsyncWorker(env, this, vfo);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::SetDcsSql(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() < 1 || !info[0].IsNumber()) {
    Napi::TypeError::New(env, "Expected DCS SQL code as number").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  unsigned int code = static_cast<unsigned int>(info[0].As<Napi::Number>().Uint32Value());
  int vfo = parseVfoParameter(info, 1, SHIM_RIG_VFO_CURR);
  RETURN_NULL_IF_INVALID_VFO(vfo);
  
  SetDcsSqlAsyncWorker* asyncWorker = new SetDcsSqlAsyncWorker(env, this, vfo, code);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::GetDcsSql(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  int vfo = parseVfoParameter(info, 0, SHIM_RIG_VFO_CURR);
  RETURN_NULL_IF_INVALID_VFO(vfo);
  
  GetDcsSqlAsyncWorker* asyncWorker = new GetDcsSqlAsyncWorker(env, this, vfo);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// Parameter Control Methods Implementation
Napi::Value NodeHamLib::SetParm(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() < 2 || !info[0].IsString() || !info[1].IsNumber()) {
    Napi::TypeError::New(env, "Expected parameter name as string and value as number").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  std::string parm_str = info[0].As<Napi::String>().Utf8Value();
  uint64_t parm = shim_rig_parse_parm(parm_str.c_str());
  
  if (parm == 0) {
    Napi::Error::New(env, "Invalid parameter name: " + parm_str).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  float value = info[1].As<Napi::Number>().FloatValue();

  SetParmAsyncWorker* asyncWorker = new SetParmAsyncWorker(env, this, parm, value);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::GetParm(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected parameter name as string").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  std::string parm_str = info[0].As<Napi::String>().Utf8Value();
  uint64_t parm = shim_rig_parse_parm(parm_str.c_str());
  
  if (parm == 0) {
    Napi::Error::New(env, "Invalid parameter name: " + parm_str).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  GetParmAsyncWorker* asyncWorker = new GetParmAsyncWorker(env, this, parm);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// DTMF Support Methods Implementation
Napi::Value NodeHamLib::SendDtmf(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected DTMF digits as string").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  std::string digits = info[0].As<Napi::String>().Utf8Value();
  int vfo = parseVfoParameter(info, 1, SHIM_RIG_VFO_CURR);
  RETURN_NULL_IF_INVALID_VFO(vfo);
  
  SendDtmfAsyncWorker* asyncWorker = new SendDtmfAsyncWorker(env, this, vfo, digits);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::RecvDtmf(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() < 1 || !info[0].IsNumber()) {
    Napi::TypeError::New(env, "Expected (maxLength: number, vfo?: string)").ThrowAsJavaScriptException();
    return env.Null();
  }
  int maxLength = info[0].As<Napi::Number>().Int32Value();
  
  int vfo = parseVfoParameter(info, 1, SHIM_RIG_VFO_CURR);
  RETURN_NULL_IF_INVALID_VFO(vfo);
  
  RecvDtmfAsyncWorker* asyncWorker = new RecvDtmfAsyncWorker(env, this, vfo, maxLength);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// Memory Channel Advanced Operations Methods Implementation
Napi::Value NodeHamLib::GetMem(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  int vfo = parseVfoParameter(info, 0, SHIM_RIG_VFO_CURR);
  RETURN_NULL_IF_INVALID_VFO(vfo);
  
  GetMemAsyncWorker* asyncWorker = new GetMemAsyncWorker(env, this, vfo);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::SetBank(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() < 1 || !info[0].IsNumber()) {
    Napi::TypeError::New(env, "Expected bank number").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  int bank = info[0].As<Napi::Number>().Int32Value();
  int vfo = parseVfoParameter(info, 1, SHIM_RIG_VFO_CURR);
  RETURN_NULL_IF_INVALID_VFO(vfo);
  
  SetBankAsyncWorker* asyncWorker = new SetBankAsyncWorker(env, this, vfo, bank);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::MemCount(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  MemCountAsyncWorker* asyncWorker = new MemCountAsyncWorker(env, this);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// Morse Code Support Methods Implementation
Napi::Value NodeHamLib::SendMorse(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected message as string").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  std::string msg = info[0].As<Napi::String>().Utf8Value();
  int vfo = parseVfoParameter(info, 1, SHIM_RIG_VFO_CURR);
  RETURN_NULL_IF_INVALID_VFO(vfo);
  
  SendMorseAsyncWorker* asyncWorker = new SendMorseAsyncWorker(env, this, vfo, msg);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::StopMorse(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  int vfo = parseVfoParameter(info, 0, SHIM_RIG_VFO_CURR);
  RETURN_NULL_IF_INVALID_VFO(vfo);
  
  StopMorseAsyncWorker* asyncWorker = new StopMorseAsyncWorker(env, this, vfo);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::WaitMorse(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  int vfo = parseVfoParameter(info, 0, SHIM_RIG_VFO_CURR);
  RETURN_NULL_IF_INVALID_VFO(vfo);
  
  WaitMorseAsyncWorker* asyncWorker = new WaitMorseAsyncWorker(env, this, vfo);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// Voice Memory Support Methods Implementation
Napi::Value NodeHamLib::SendVoiceMem(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() < 1 || !info[0].IsNumber()) {
    Napi::TypeError::New(env, "Expected channel number").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  int ch = info[0].As<Napi::Number>().Int32Value();
  int vfo = parseVfoParameter(info, 1, SHIM_RIG_VFO_CURR);
  RETURN_NULL_IF_INVALID_VFO(vfo);
  
  SendVoiceMemAsyncWorker* asyncWorker = new SendVoiceMemAsyncWorker(env, this, vfo, ch);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::StopVoiceMem(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  int vfo = parseVfoParameter(info, 0, SHIM_RIG_VFO_CURR);
  RETURN_NULL_IF_INVALID_VFO(vfo);
  
  StopVoiceMemAsyncWorker* asyncWorker = new StopVoiceMemAsyncWorker(env, this, vfo);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// Complex Split Frequency/Mode Operations Methods Implementation
Napi::Value NodeHamLib::SetSplitFreqMode(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsString() || !info[2].IsNumber()) {
    Napi::TypeError::New(env, "Expected TX frequency, mode, and width").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  double tx_freq = static_cast<double>(info[0].As<Napi::Number>().DoubleValue());
  
  // Basic frequency range validation
  if (tx_freq < 1000 || tx_freq > 10000000000) { // 1 kHz to 10 GHz reasonable range
    Napi::Error::New(env, "Frequency out of range (1 kHz - 10 GHz)").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  std::string mode_str = info[1].As<Napi::String>().Utf8Value();
  int tx_width = static_cast<int>(info[2].As<Napi::Number>().DoubleValue());
  int vfo = parseVfoParameter(info, 3, SHIM_RIG_VFO_CURR);
  RETURN_NULL_IF_INVALID_VFO(vfo);
  
  int tx_mode = shim_rig_parse_mode(mode_str.c_str());
  if (tx_mode == SHIM_RIG_MODE_NONE) {
    Napi::Error::New(env, "Invalid mode: " + mode_str).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  SetSplitFreqModeAsyncWorker* asyncWorker = new SetSplitFreqModeAsyncWorker(env, this, vfo, tx_freq, tx_mode, tx_width);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::GetSplitFreqMode(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  int vfo = parseVfoParameter(info, 0, SHIM_RIG_VFO_CURR);
  RETURN_NULL_IF_INVALID_VFO(vfo);
  
  GetSplitFreqModeAsyncWorker* asyncWorker = new GetSplitFreqModeAsyncWorker(env, this, vfo);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// Power2mW Method Implementation  
Napi::Value NodeHamLib::Power2mW(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsString()) {
    Napi::TypeError::New(env, "Expected power2mW(power, frequency, mode)").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  float power = info[0].As<Napi::Number>().FloatValue();
  double freq = info[1].As<Napi::Number>().DoubleValue();
  std::string mode_str = info[2].As<Napi::String>().Utf8Value();
  
  // Validate power (0.0 to 1.0)
  if (power < 0.0 || power > 1.0) {
    Napi::Error::New(env, "Power must be between 0.0 and 1.0").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  // Validate frequency range
  if (freq < 1000 || freq > 10000000000) { // 1 kHz to 10 GHz
    Napi::Error::New(env, "Frequency out of range (1 kHz - 10 GHz)").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  // Parse mode string
  int mode = shim_rig_parse_mode(mode_str.c_str());
  if (mode == SHIM_RIG_MODE_NONE) {
    Napi::Error::New(env, "Invalid mode: " + mode_str).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  Power2mWAsyncWorker* asyncWorker = new Power2mWAsyncWorker(env, this, power, freq, mode);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// MW2Power Method Implementation
Napi::Value NodeHamLib::MW2Power(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsString()) {
    Napi::TypeError::New(env, "Expected mW2power(milliwatts, frequency, mode)").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  unsigned int mwpower = info[0].As<Napi::Number>().Uint32Value();
  double freq = info[1].As<Napi::Number>().DoubleValue();
  std::string mode_str = info[2].As<Napi::String>().Utf8Value();
  
  // Validate milliwatts (reasonable range)
  if (mwpower > 10000000) { // 10kW max
    Napi::Error::New(env, "Milliwatts out of reasonable range (max 10,000,000 mW)").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  // Validate frequency range
  if (freq < 1000 || freq > 10000000000) { // 1 kHz to 10 GHz
    Napi::Error::New(env, "Frequency out of range (1 kHz - 10 GHz)").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  // Parse mode string
  int mode = shim_rig_parse_mode(mode_str.c_str());
  if (mode == SHIM_RIG_MODE_NONE) {
    Napi::Error::New(env, "Invalid mode: " + mode_str).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  MW2PowerAsyncWorker* asyncWorker = new MW2PowerAsyncWorker(env, this, mwpower, freq, mode);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// Reset Function Method Implementation
Napi::Value NodeHamLib::Reset(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected (resetType: string)").ThrowAsJavaScriptException();
    return env.Null();
  }

  int reset = SHIM_RIG_RESET_SOFT;
  std::string reset_str = info[0].As<Napi::String>().Utf8Value();
  if (reset_str == "NONE") {
    reset = SHIM_RIG_RESET_NONE;
  } else if (reset_str == "SOFT") {
    reset = SHIM_RIG_RESET_SOFT;
  } else if (reset_str == "MCALL") {
    reset = SHIM_RIG_RESET_MCALL;
  } else if (reset_str == "MASTER") {
    reset = SHIM_RIG_RESET_MASTER;
  } else if (reset_str == "VFO") {
    reset = SHIM_RIG_RESET_VFO;
  } else {
    Napi::Error::New(env, "Invalid reset type: " + reset_str +
                    " (valid: NONE, SOFT, VFO, MCALL, MASTER)").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  ResetAsyncWorker* asyncWorker = new ResetAsyncWorker(env, this, reset);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// ===== Lock Mode (Hamlib >= 4.7.0) =====

class SetLockModeAsyncWorker : public HamLibAsyncWorker {
public:
    SetLockModeAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int lock)
        : HamLibAsyncWorker(env, hamlib_instance), lock_(lock) {}

    void Execute() override {
        CHECK_RIG_VALID();
        result_code_ = shim_rig_set_lock_mode(hamlib_instance_->my_rig, lock_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }

    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }

    void OnError(const Napi::Error& e) override {
        deferred_.Reject(Napi::Error::New(Env(), error_message_).Value());
    }

private:
    int lock_;
};

class GetLockModeAsyncWorker : public HamLibAsyncWorker {
public:
    GetLockModeAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance)
        : HamLibAsyncWorker(env, hamlib_instance), lock_(0) {}

    void Execute() override {
        CHECK_RIG_VALID();
        result_code_ = shim_rig_get_lock_mode(hamlib_instance_->my_rig, &lock_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }

    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, lock_));
        }
    }

    void OnError(const Napi::Error& e) override {
        deferred_.Reject(Napi::Error::New(Env(), error_message_).Value());
    }

private:
    int lock_;
};

Napi::Value NodeHamLib::SetLockMode(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsNumber()) {
    Napi::TypeError::New(env, "Expected lock mode as number").ThrowAsJavaScriptException();
    return env.Null();
  }

  int lock = info[0].As<Napi::Number>().Int32Value();

  SetLockModeAsyncWorker* asyncWorker = new SetLockModeAsyncWorker(env, this, lock);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::GetLockMode(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  GetLockModeAsyncWorker* asyncWorker = new GetLockModeAsyncWorker(env, this);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// ===== Clock (Hamlib >= 4.7.0) =====

class SetClockAsyncWorker : public HamLibAsyncWorker {
public:
    SetClockAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance,
                        int year, int month, int day,
                        int hour, int min, int sec, double msec, int utc_offset)
        : HamLibAsyncWorker(env, hamlib_instance),
          year_(year), month_(month), day_(day),
          hour_(hour), min_(min), sec_(sec), msec_(msec), utc_offset_(utc_offset) {}

    void Execute() override {
        CHECK_RIG_VALID();
        result_code_ = shim_rig_set_clock(hamlib_instance_->my_rig,
                                           year_, month_, day_,
                                           hour_, min_, sec_, msec_, utc_offset_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }

    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }

    void OnError(const Napi::Error& e) override {
        deferred_.Reject(Napi::Error::New(Env(), error_message_).Value());
    }

private:
    int year_, month_, day_, hour_, min_, sec_;
    double msec_;
    int utc_offset_;
};

class GetClockAsyncWorker : public HamLibAsyncWorker {
public:
    GetClockAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance)
        : HamLibAsyncWorker(env, hamlib_instance),
          year_(0), month_(0), day_(0),
          hour_(0), min_(0), sec_(0), msec_(0.0), utc_offset_(0) {}

    void Execute() override {
        CHECK_RIG_VALID();
        result_code_ = shim_rig_get_clock(hamlib_instance_->my_rig,
                                           &year_, &month_, &day_,
                                           &hour_, &min_, &sec_, &msec_, &utc_offset_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }

    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            Napi::Object obj = Napi::Object::New(env);
            obj.Set("year", Napi::Number::New(env, year_));
            obj.Set("month", Napi::Number::New(env, month_));
            obj.Set("day", Napi::Number::New(env, day_));
            obj.Set("hour", Napi::Number::New(env, hour_));
            obj.Set("min", Napi::Number::New(env, min_));
            obj.Set("sec", Napi::Number::New(env, sec_));
            obj.Set("msec", Napi::Number::New(env, msec_));
            obj.Set("utcOffset", Napi::Number::New(env, utc_offset_));
            deferred_.Resolve(obj);
        }
    }

    void OnError(const Napi::Error& e) override {
        deferred_.Reject(Napi::Error::New(Env(), error_message_).Value());
    }

private:
    int year_, month_, day_, hour_, min_, sec_;
    double msec_;
    int utc_offset_;
};

Napi::Value NodeHamLib::SetClock(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "Expected clock object with year, month, day, hour, min, sec, msec, utcOffset").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Object obj = info[0].As<Napi::Object>();

  auto getInt = [&](const char* key, int defaultVal) -> int {
    if (obj.Has(key) && obj.Get(key).IsNumber()) {
      return obj.Get(key).As<Napi::Number>().Int32Value();
    }
    return defaultVal;
  };
  auto getDouble = [&](const char* key, double defaultVal) -> double {
    if (obj.Has(key) && obj.Get(key).IsNumber()) {
      return obj.Get(key).As<Napi::Number>().DoubleValue();
    }
    return defaultVal;
  };

  int year = getInt("year", 0);
  int month = getInt("month", 0);
  int day = getInt("day", 0);
  int hour = getInt("hour", 0);
  int min = getInt("min", 0);
  int sec = getInt("sec", 0);
  double msec = getDouble("msec", 0.0);
  int utcOffset = getInt("utcOffset", 0);

  SetClockAsyncWorker* asyncWorker = new SetClockAsyncWorker(env, this,
      year, month, day, hour, min, sec, msec, utcOffset);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::GetClock(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  GetClockAsyncWorker* asyncWorker = new GetClockAsyncWorker(env, this);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// ===== VFO Info (Hamlib >= 4.7.0) =====

class GetVfoInfoAsyncWorker : public HamLibAsyncWorker {
public:
    GetVfoInfoAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int vfo)
        : HamLibAsyncWorker(env, hamlib_instance),
          vfo_(vfo), freq_(0.0), mode_(0), width_(0), split_(0), satmode_(0) {}

    void Execute() override {
        CHECK_RIG_VALID();
        result_code_ = shim_rig_get_vfo_info(hamlib_instance_->my_rig, vfo_,
                                              &freq_, &mode_, &width_, &split_, &satmode_);
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }

    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            Napi::Object obj = Napi::Object::New(env);
            obj.Set("frequency", Napi::Number::New(env, freq_));
            obj.Set("mode", Napi::String::New(env, shim_rig_strrmode(static_cast<int>(mode_))));
            obj.Set("bandwidth", Napi::Number::New(env, static_cast<double>(width_)));
            obj.Set("split", Napi::Boolean::New(env, split_ != 0));
            obj.Set("satMode", Napi::Boolean::New(env, satmode_ != 0));
            deferred_.Resolve(obj);
        }
    }

    void OnError(const Napi::Error& e) override {
        deferred_.Reject(Napi::Error::New(Env(), error_message_).Value());
    }

private:
    int vfo_;
    double freq_;
    uint64_t mode_;
    long width_;
    int split_;
    int satmode_;
};

Napi::Value NodeHamLib::GetVfoInfo(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  int vfo = parseVfoParameter(info, 0, SHIM_RIG_VFO_CURR);
  RETURN_NULL_IF_INVALID_VFO(vfo);

  GetVfoInfoAsyncWorker* asyncWorker = new GetVfoInfoAsyncWorker(env, this, vfo);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// ===== GetInfo (async) =====

class GetInfoAsyncWorker : public HamLibAsyncWorker {
public:
    GetInfoAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance)
        : HamLibAsyncWorker(env, hamlib_instance) {}

    void Execute() override {
        CHECK_RIG_VALID();
        const char* info = shim_rig_get_info(hamlib_instance_->my_rig);
        info_str_ = info ? info : "";
    }

    void OnOK() override {
        Napi::Env env = Env();
        deferred_.Resolve(Napi::String::New(env, info_str_));
    }

    void OnError(const Napi::Error& e) override {
        deferred_.Reject(Napi::Error::New(Env(), error_message_).Value());
    }

private:
    std::string info_str_;
};

Napi::Value NodeHamLib::GetInfo(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }

  GetInfoAsyncWorker* asyncWorker = new GetInfoAsyncWorker(env, this);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// ===== SendRaw (async) =====

class SendRawAsyncWorker : public HamLibAsyncWorker {
public:
    SendRawAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance,
                       std::vector<unsigned char> send_data, int reply_max_len,
                       std::vector<unsigned char> terminator, bool has_terminator)
        : HamLibAsyncWorker(env, hamlib_instance),
          send_data_(std::move(send_data)), reply_max_len_(reply_max_len),
          terminator_(std::move(terminator)), has_terminator_(has_terminator),
          reply_len_(0) {
        reply_buf_.resize(reply_max_len > 0 ? reply_max_len : 1);
    }

    void Execute() override {
        CHECK_RIG_VALID();
        const unsigned char* term = has_terminator_ ? terminator_.data() : nullptr;
        result_code_ = shim_rig_send_raw(hamlib_instance_->my_rig,
            send_data_.data(), (int)send_data_.size(),
            reply_buf_.data(), reply_max_len_, term);
        if (result_code_ < 0) {
            error_message_ = shim_rigerror(result_code_);
        } else {
            reply_len_ = result_code_;
            result_code_ = SHIM_RIG_OK;
        }
    }

    void OnOK() override {
        Napi::Env env = Env();
        if (!error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Buffer<unsigned char>::Copy(env, reply_buf_.data(), reply_len_));
        }
    }

    void OnError(const Napi::Error& e) override {
        deferred_.Reject(Napi::Error::New(Env(), error_message_).Value());
    }

private:
    std::vector<unsigned char> send_data_;
    int reply_max_len_;
    std::vector<unsigned char> terminator_;
    bool has_terminator_;
    std::vector<unsigned char> reply_buf_;
    int reply_len_;
};

Napi::Value NodeHamLib::SendRaw(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() < 2 || !info[0].IsBuffer() || !info[1].IsNumber()) {
    Napi::TypeError::New(env, "Expected (data: Buffer, replyMaxLen: number, terminator?: Buffer)").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Buffer<unsigned char> dataBuf = info[0].As<Napi::Buffer<unsigned char>>();
  std::vector<unsigned char> send_data(dataBuf.Data(), dataBuf.Data() + dataBuf.Length());
  int replyMaxLen = info[1].As<Napi::Number>().Int32Value();

  std::vector<unsigned char> terminator;
  bool has_terminator = false;
  if (info.Length() >= 3 && info[2].IsBuffer()) {
    Napi::Buffer<unsigned char> termBuf = info[2].As<Napi::Buffer<unsigned char>>();
    terminator.assign(termBuf.Data(), termBuf.Data() + termBuf.Length());
    has_terminator = true;
  }

  SendRawAsyncWorker* asyncWorker = new SendRawAsyncWorker(env, this, std::move(send_data), replyMaxLen, std::move(terminator), has_terminator);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::GetSpectrumCapabilities(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  RETURN_NULL_IF_RIG_HANDLE_INVALID();
  Napi::Object result = Napi::Object::New(env);

  result.Set("asyncDataSupported", Napi::Boolean::New(env, shim_rig_is_async_data_supported(my_rig) != 0));

  Napi::Array scopes = Napi::Array::New(env);
  int scopeCount = shim_rig_get_caps_spectrum_scope_count(my_rig);
  for (int i = 0; i < scopeCount; ++i) {
    shim_spectrum_scope_t scope{};
    if (shim_rig_get_caps_spectrum_scope(my_rig, i, &scope) != SHIM_RIG_OK) {
      continue;
    }
    Napi::Object scopeObject = Napi::Object::New(env);
    scopeObject.Set("id", Napi::Number::New(env, scope.id));
    scopeObject.Set("name", Napi::String::New(env, scope.name));
    scopes.Set(i, scopeObject);
  }
  result.Set("scopes", scopes);

  Napi::Array modes = Napi::Array::New(env);
  int modeCount = shim_rig_get_caps_spectrum_mode_count(my_rig);
  for (int i = 0; i < modeCount; ++i) {
    int modeId = 0;
    if (shim_rig_get_caps_spectrum_mode(my_rig, i, &modeId) != SHIM_RIG_OK) {
      continue;
    }
    Napi::Object modeObject = Napi::Object::New(env);
    modeObject.Set("id", Napi::Number::New(env, modeId));
    modeObject.Set("name", Napi::String::New(env, shim_rig_str_spectrum_mode(modeId)));
    modes.Set(i, modeObject);
  }
  result.Set("modes", modes);

  Napi::Array spans = Napi::Array::New(env);
  int spanCount = shim_rig_get_caps_spectrum_span_count(my_rig);
  for (int i = 0; i < spanCount; ++i) {
    double spanHz = 0;
    if (shim_rig_get_caps_spectrum_span(my_rig, i, &spanHz) != SHIM_RIG_OK) {
      continue;
    }
    spans.Set(i, Napi::Number::New(env, spanHz));
  }
  result.Set("spans", spans);

  Napi::Array avgModes = Napi::Array::New(env);
  int avgModeCount = shim_rig_get_caps_spectrum_avg_mode_count(my_rig);
  for (int i = 0; i < avgModeCount; ++i) {
    shim_spectrum_avg_mode_t avgMode{};
    if (shim_rig_get_caps_spectrum_avg_mode(my_rig, i, &avgMode) != SHIM_RIG_OK) {
      continue;
    }
    Napi::Object avgModeObject = Napi::Object::New(env);
    avgModeObject.Set("id", Napi::Number::New(env, avgMode.id));
    avgModeObject.Set("name", Napi::String::New(env, avgMode.name));
    avgModes.Set(i, avgModeObject);
  }
  result.Set("avgModes", avgModes);

  return result;
}

Napi::Value NodeHamLib::StartSpectrumStream(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info.Length() < 1 || !info[0].IsFunction()) {
    Napi::TypeError::New(env, "Expected (callback: Function)").ThrowAsJavaScriptException();
    return env.Null();
  }

  std::lock_guard<std::mutex> lock(spectrum_mutex_);
  if (spectrum_stream_running_) {
    Napi::Error::New(env, "Spectrum stream is already running").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Function callback = info[0].As<Napi::Function>();
  spectrum_tsfn_ = Napi::ThreadSafeFunction::New(env, callback, "HamLibSpectrumStream", 0, 1);
  int ret = shim_rig_set_spectrum_callback(my_rig, &NodeHamLib::spectrum_line_cb, this);
  if (ret != SHIM_RIG_OK) {
    spectrum_tsfn_.Release();
    spectrum_tsfn_ = Napi::ThreadSafeFunction();
    Napi::Error::New(env, shim_rigerror(ret)).ThrowAsJavaScriptException();
    return env.Null();
  }
  spectrum_stream_running_ = true;
  return Napi::Boolean::New(env, true);
}

Napi::Value NodeHamLib::StopSpectrumStream(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  StopSpectrumStreamInternal();
  return Napi::Boolean::New(env, true);
}

// ===== SetConf (async) =====

class SetConfAsyncWorker : public HamLibAsyncWorker {
public:
    SetConfAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance,
                       std::string name, std::string value)
        : HamLibAsyncWorker(env, hamlib_instance),
          name_(std::move(name)), value_(std::move(value)) {}

    void Execute() override {
        CHECK_RIG_VALID();
        result_code_ = shim_rig_set_conf(hamlib_instance_->my_rig, name_.c_str(), value_.c_str());
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }

    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, result_code_));
        }
    }

    void OnError(const Napi::Error& e) override {
        deferred_.Reject(Napi::Error::New(Env(), error_message_).Value());
    }

private:
    std::string name_;
    std::string value_;
};

Napi::Value NodeHamLib::SetConf(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() < 2 || !info[0].IsString() || !info[1].IsString()) {
    Napi::TypeError::New(env, "Expected (name: string, value: string)").ThrowAsJavaScriptException();
    return env.Null();
  }

  std::string name = info[0].As<Napi::String>().Utf8Value();
  std::string value = info[1].As<Napi::String>().Utf8Value();

  SetConfAsyncWorker* asyncWorker = new SetConfAsyncWorker(env, this, std::move(name), std::move(value));
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// ===== GetConf (async) =====

class GetConfAsyncWorker : public HamLibAsyncWorker {
public:
    GetConfAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, std::string name)
        : HamLibAsyncWorker(env, hamlib_instance), name_(std::move(name)) {
        memset(buf_, 0, sizeof(buf_));
    }

    void Execute() override {
        CHECK_RIG_VALID();
        result_code_ = shim_rig_get_conf(hamlib_instance_->my_rig, name_.c_str(), buf_, sizeof(buf_));
        if (result_code_ != SHIM_RIG_OK) {
            error_message_ = shim_rigerror(result_code_);
        }
    }

    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != SHIM_RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::String::New(env, buf_));
        }
    }

    void OnError(const Napi::Error& e) override {
        deferred_.Reject(Napi::Error::New(Env(), error_message_).Value());
    }

private:
    std::string name_;
    char buf_[256];
};

Napi::Value NodeHamLib::GetConf(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected (name: string)").ThrowAsJavaScriptException();
    return env.Null();
  }

  std::string name = info[0].As<Napi::String>().Utf8Value();

  GetConfAsyncWorker* asyncWorker = new GetConfAsyncWorker(env, this, std::move(name));
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// ===== Passband methods (sync) =====

Napi::Value NodeHamLib::GetPassbandNormal(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  RETURN_NULL_IF_RIG_HANDLE_INVALID();

  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected (mode: string)").ThrowAsJavaScriptException();
    return env.Null();
  }

  std::string modeStr = info[0].As<Napi::String>().Utf8Value();
  int mode = shim_rig_parse_mode(modeStr.c_str());
  int pb = shim_rig_passband_normal(my_rig, mode);
  return Napi::Number::New(env, pb);
}

Napi::Value NodeHamLib::GetPassbandNarrow(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  RETURN_NULL_IF_RIG_HANDLE_INVALID();

  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected (mode: string)").ThrowAsJavaScriptException();
    return env.Null();
  }

  std::string modeStr = info[0].As<Napi::String>().Utf8Value();
  int mode = shim_rig_parse_mode(modeStr.c_str());
  int pb = shim_rig_passband_narrow(my_rig, mode);
  return Napi::Number::New(env, pb);
}

Napi::Value NodeHamLib::GetPassbandWide(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  RETURN_NULL_IF_RIG_HANDLE_INVALID();

  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected (mode: string)").ThrowAsJavaScriptException();
    return env.Null();
  }

  std::string modeStr = info[0].As<Napi::String>().Utf8Value();
  int mode = shim_rig_parse_mode(modeStr.c_str());
  int pb = shim_rig_passband_wide(my_rig, mode);
  return Napi::Number::New(env, pb);
}

// ===== Resolution (sync) =====

Napi::Value NodeHamLib::GetResolution(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  RETURN_NULL_IF_RIG_HANDLE_INVALID();

  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected (mode: string)").ThrowAsJavaScriptException();
    return env.Null();
  }

  std::string modeStr = info[0].As<Napi::String>().Utf8Value();
  int mode = shim_rig_parse_mode(modeStr.c_str());
  int res = shim_rig_get_resolution(my_rig, mode);
  return Napi::Number::New(env, res);
}

// ===== getSupportedParms (sync) =====

Napi::Value NodeHamLib::GetSupportedParms(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  RETURN_NULL_IF_RIG_HANDLE_INVALID();

  uint64_t parms = shim_rig_get_caps_has_get_parm(my_rig) | shim_rig_get_caps_has_set_parm(my_rig);
  Napi::Array parmArray = Napi::Array::New(env);
  uint32_t index = 0;

  if (parms & SHIM_RIG_PARM_ANN) parmArray[index++] = Napi::String::New(env, "ANN");
  if (parms & SHIM_RIG_PARM_APO) parmArray[index++] = Napi::String::New(env, "APO");
  if (parms & SHIM_RIG_PARM_BACKLIGHT) parmArray[index++] = Napi::String::New(env, "BACKLIGHT");
  if (parms & SHIM_RIG_PARM_BEEP) parmArray[index++] = Napi::String::New(env, "BEEP");
  if (parms & SHIM_RIG_PARM_TIME) parmArray[index++] = Napi::String::New(env, "TIME");
  if (parms & SHIM_RIG_PARM_BAT) parmArray[index++] = Napi::String::New(env, "BAT");
  if (parms & SHIM_RIG_PARM_KEYLIGHT) parmArray[index++] = Napi::String::New(env, "KEYLIGHT");
  if (parms & SHIM_RIG_PARM_SCREENSAVER) parmArray[index++] = Napi::String::New(env, "SCREENSAVER");

  return parmArray;
}

// ===== getSupportedVfoOps (sync) =====

Napi::Value NodeHamLib::GetSupportedVfoOps(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  RETURN_NULL_IF_RIG_HANDLE_INVALID();

  int ops = shim_rig_get_caps_vfo_ops(my_rig);
  Napi::Array opsArray = Napi::Array::New(env);
  uint32_t index = 0;

  if (ops & SHIM_RIG_OP_CPY) opsArray[index++] = Napi::String::New(env, "CPY");
  if (ops & SHIM_RIG_OP_XCHG) opsArray[index++] = Napi::String::New(env, "XCHG");
  if (ops & SHIM_RIG_OP_FROM_VFO) opsArray[index++] = Napi::String::New(env, "FROM_VFO");
  if (ops & SHIM_RIG_OP_TO_VFO) opsArray[index++] = Napi::String::New(env, "TO_VFO");
  if (ops & SHIM_RIG_OP_MCL) opsArray[index++] = Napi::String::New(env, "MCL");
  if (ops & SHIM_RIG_OP_UP) opsArray[index++] = Napi::String::New(env, "UP");
  if (ops & SHIM_RIG_OP_DOWN) opsArray[index++] = Napi::String::New(env, "DOWN");
  if (ops & SHIM_RIG_OP_BAND_UP) opsArray[index++] = Napi::String::New(env, "BAND_UP");
  if (ops & SHIM_RIG_OP_BAND_DOWN) opsArray[index++] = Napi::String::New(env, "BAND_DOWN");
  if (ops & SHIM_RIG_OP_LEFT) opsArray[index++] = Napi::String::New(env, "LEFT");
  if (ops & SHIM_RIG_OP_RIGHT) opsArray[index++] = Napi::String::New(env, "RIGHT");
  if (ops & SHIM_RIG_OP_TUNE) opsArray[index++] = Napi::String::New(env, "TUNE");
  if (ops & SHIM_RIG_OP_TOGGLE) opsArray[index++] = Napi::String::New(env, "TOGGLE");

  return opsArray;
}

// ===== getSupportedScanTypes (sync) =====

Napi::Value NodeHamLib::GetSupportedScanTypes(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  RETURN_NULL_IF_RIG_HANDLE_INVALID();

  int scan = shim_rig_get_caps_has_scan(my_rig);
  Napi::Array scanArray = Napi::Array::New(env);
  uint32_t index = 0;

  if (scan & SHIM_RIG_SCAN_MEM) scanArray[index++] = Napi::String::New(env, "MEM");
  if (scan & SHIM_RIG_SCAN_VFO) scanArray[index++] = Napi::String::New(env, "VFO");
  if (scan & SHIM_RIG_SCAN_PROG) scanArray[index++] = Napi::String::New(env, "PROG");
  if (scan & SHIM_RIG_SCAN_DELTA) scanArray[index++] = Napi::String::New(env, "DELTA");
  if (scan & SHIM_RIG_SCAN_PRIO) scanArray[index++] = Napi::String::New(env, "PRIO");

  return scanArray;
}

// ===== Capability Query Batch 2 (sync) =====

// Helper: convert mode bitmask to array of mode strings
static Napi::Array ModeBitmaskToArray(Napi::Env env, uint64_t modes) {
  Napi::Array arr = Napi::Array::New(env);
  uint32_t idx = 0;
  for (unsigned int i = 0; i < SHIM_HAMLIB_MAX_MODES; i++) {
    uint64_t bit = modes & (1ULL << i);
    if (bit) {
      const char* name = shim_rig_strrmode(bit);
      if (name && name[0] != '\0') {
        arr[idx++] = Napi::String::New(env, name);
      }
    }
  }
  return arr;
}

Napi::Value NodeHamLib::GetPreampValues(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  RETURN_NULL_IF_RIG_HANDLE_INVALID();
  int buf[SHIM_HAMLIB_MAX_MODES];
  int count = shim_rig_get_caps_preamp(my_rig, buf, SHIM_HAMLIB_MAX_MODES);
  Napi::Array arr = Napi::Array::New(env, count);
  for (int i = 0; i < count; i++) {
    arr[(uint32_t)i] = Napi::Number::New(env, buf[i]);
  }
  return arr;
}

Napi::Value NodeHamLib::GetAttenuatorValues(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  RETURN_NULL_IF_RIG_HANDLE_INVALID();
  int buf[SHIM_HAMLIB_MAX_MODES];
  int count = shim_rig_get_caps_attenuator(my_rig, buf, SHIM_HAMLIB_MAX_MODES);
  Napi::Array arr = Napi::Array::New(env, count);
  for (int i = 0; i < count; i++) {
    arr[(uint32_t)i] = Napi::Number::New(env, buf[i]);
  }
  return arr;
}

Napi::Value NodeHamLib::GetMaxRit(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  RETURN_NULL_IF_RIG_HANDLE_INVALID();
  return Napi::Number::New(info.Env(), (double)shim_rig_get_caps_max_rit(my_rig));
}

Napi::Value NodeHamLib::GetMaxXit(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  RETURN_NULL_IF_RIG_HANDLE_INVALID();
  return Napi::Number::New(info.Env(), (double)shim_rig_get_caps_max_xit(my_rig));
}

Napi::Value NodeHamLib::GetMaxIfShift(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  RETURN_NULL_IF_RIG_HANDLE_INVALID();
  return Napi::Number::New(info.Env(), (double)shim_rig_get_caps_max_ifshift(my_rig));
}

Napi::Value NodeHamLib::GetAvailableCtcssTones(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  RETURN_NULL_IF_RIG_HANDLE_INVALID();
  unsigned int buf[256];
  int count = shim_rig_get_caps_ctcss_list(my_rig, buf, 256);
  Napi::Array arr = Napi::Array::New(env, count);
  for (int i = 0; i < count; i++) {
    // CTCSS tones stored as tenths of Hz, convert to Hz
    arr[(uint32_t)i] = Napi::Number::New(env, buf[i] / 10.0);
  }
  return arr;
}

Napi::Value NodeHamLib::GetAvailableDcsCodes(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  RETURN_NULL_IF_RIG_HANDLE_INVALID();
  unsigned int buf[256];
  int count = shim_rig_get_caps_dcs_list(my_rig, buf, 256);
  Napi::Array arr = Napi::Array::New(env, count);
  for (int i = 0; i < count; i++) {
    arr[(uint32_t)i] = Napi::Number::New(env, buf[i]);
  }
  return arr;
}

Napi::Value NodeHamLib::GetFrequencyRanges(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  RETURN_NULL_IF_RIG_HANDLE_INVALID();

  shim_freq_range_t rx_buf[30];
  shim_freq_range_t tx_buf[30];
  int rx_count = shim_rig_get_caps_rx_range(my_rig, rx_buf, 30);
  int tx_count = shim_rig_get_caps_tx_range(my_rig, tx_buf, 30);

  auto buildArray = [&](shim_freq_range_t* buf, int count) -> Napi::Array {
    Napi::Array arr = Napi::Array::New(env, count);
    for (int i = 0; i < count; i++) {
      Napi::Object obj = Napi::Object::New(env);
      obj.Set("startFreq", Napi::Number::New(env, buf[i].start_freq));
      obj.Set("endFreq", Napi::Number::New(env, buf[i].end_freq));
      obj.Set("modes", ModeBitmaskToArray(env, buf[i].modes));
      obj.Set("lowPower", Napi::Number::New(env, buf[i].low_power));
      obj.Set("highPower", Napi::Number::New(env, buf[i].high_power));
      obj.Set("vfo", Napi::Number::New(env, buf[i].vfo));
      obj.Set("antenna", Napi::Number::New(env, buf[i].ant));
      arr[(uint32_t)i] = obj;
    }
    return arr;
  };

  Napi::Object result = Napi::Object::New(env);
  result.Set("rx", buildArray(rx_buf, rx_count));
  result.Set("tx", buildArray(tx_buf, tx_count));
  return result;
}

Napi::Value NodeHamLib::GetTuningSteps(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  RETURN_NULL_IF_RIG_HANDLE_INVALID();
  shim_mode_value_t buf[20];
  int count = shim_rig_get_caps_tuning_steps(my_rig, buf, 20);
  Napi::Array arr = Napi::Array::New(env, count);
  for (int i = 0; i < count; i++) {
    Napi::Object obj = Napi::Object::New(env);
    obj.Set("modes", ModeBitmaskToArray(env, buf[i].modes));
    obj.Set("stepHz", Napi::Number::New(env, buf[i].value));
    arr[(uint32_t)i] = obj;
  }
  return arr;
}

Napi::Value NodeHamLib::GetFilterList(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  RETURN_NULL_IF_RIG_HANDLE_INVALID();
  shim_mode_value_t buf[60];
  int count = shim_rig_get_caps_filters(my_rig, buf, 60);
  Napi::Array arr = Napi::Array::New(env, count);
  for (int i = 0; i < count; i++) {
    Napi::Object obj = Napi::Object::New(env);
    obj.Set("modes", ModeBitmaskToArray(env, buf[i].modes));
    obj.Set("width", Napi::Number::New(env, buf[i].value));
    arr[(uint32_t)i] = obj;
  }
  return arr;
}

// ===== Static: getCopyright / getLicense =====

Napi::Value NodeHamLib::GetCopyright(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  return Napi::String::New(env, shim_rig_copyright());
}

Napi::Value NodeHamLib::GetLicense(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  return Napi::String::New(env, shim_rig_license());
}
