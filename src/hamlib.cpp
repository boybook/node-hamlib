#include "hamlib.h"
#include "hamlib_compat.h"
#include <string>
#include <vector>
#include <memory>
#include <algorithm>

// Cross-platform compatibility for hamlib token types
// Linux versions use token_t, some others use hamlib_token_t
#ifndef HAMLIB_TOKEN_T
#ifdef __linux__
#define HAMLIB_TOKEN_T token_t
#else
#define HAMLIB_TOKEN_T hamlib_token_t
#endif
#endif

// 安全宏 - 检查RIG指针有效性，防止空指针解引用和已销毁对象访问
#define CHECK_RIG_VALID() \
  do { \
    if (!hamlib_instance_ || !hamlib_instance_->my_rig) { \
      result_code_ = -RIG_EINVAL; \
      error_message_ = "RIG is not initialized or has been destroyed"; \
      return; \
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
        
        result_code_ = rig_open(hamlib_instance_->my_rig);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        } else {
            rig_set_freq_callback(hamlib_instance_->my_rig, NodeHamLib::freq_change_cb, hamlib_instance_);
            auto ppt_cb = +[](RIG *rig, vfo_t vfo, ptt_t ptt, rig_ptr_t arg) {
                printf("PPT pushed!");
                return 0;
            };
            int cb_result = rig_set_ptt_callback(hamlib_instance_->my_rig, ppt_cb, NULL);
            rig_set_trn(hamlib_instance_->my_rig, RIG_TRN_POLL);
            hamlib_instance_->rig_is_open = true;
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    SetFrequencyAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, freq_t freq, vfo_t vfo)
        : HamLibAsyncWorker(env, hamlib_instance), freq_(freq), vfo_(vfo) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_set_freq(hamlib_instance_->my_rig, vfo_, freq_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    freq_t freq_;
    vfo_t vfo_;
};

class GetFrequencyAsyncWorker : public HamLibAsyncWorker {
public:
    GetFrequencyAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), freq_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_get_freq(hamlib_instance_->my_rig, vfo_, &freq_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    vfo_t vfo_;
    freq_t freq_;
};

class SetModeAsyncWorker : public HamLibAsyncWorker {
public:
    SetModeAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, rmode_t mode, pbwidth_t width, vfo_t vfo = RIG_VFO_CURR)
        : HamLibAsyncWorker(env, hamlib_instance), mode_(mode), width_(width), vfo_(vfo) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_set_mode(hamlib_instance_->my_rig, vfo_, mode_, width_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    rmode_t mode_;
    pbwidth_t width_;
    vfo_t vfo_;
};

class GetModeAsyncWorker : public HamLibAsyncWorker {
public:
    GetModeAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance)
        : HamLibAsyncWorker(env, hamlib_instance), mode_(0), width_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_get_mode(hamlib_instance_->my_rig, RIG_VFO_CURR, &mode_, &width_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            Napi::Object obj = Napi::Object::New(env);
            // Convert mode to string using rig_strrmode
            const char* mode_str = rig_strrmode(mode_);
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
    rmode_t mode_;
    pbwidth_t width_;
};

class SetPttAsyncWorker : public HamLibAsyncWorker {
public:
    SetPttAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, ptt_t ptt)
        : HamLibAsyncWorker(env, hamlib_instance), ptt_(ptt) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_set_ptt(hamlib_instance_->my_rig, RIG_VFO_CURR, ptt_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    ptt_t ptt_;
};

class GetStrengthAsyncWorker : public HamLibAsyncWorker {
public:
    GetStrengthAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), strength_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_get_strength(hamlib_instance_->my_rig, vfo_, &strength_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    vfo_t vfo_;
    int strength_;
};

class SetLevelAsyncWorker : public HamLibAsyncWorker {
public:
    SetLevelAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, setting_t level_type, value_t value)
        : HamLibAsyncWorker(env, hamlib_instance), level_type_(level_type), value_(value) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_set_level(hamlib_instance_->my_rig, RIG_VFO_CURR, level_type_, value_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    setting_t level_type_;
    value_t value_;
};

class GetLevelAsyncWorker : public HamLibAsyncWorker {
public:
    GetLevelAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, setting_t level_type)
        : HamLibAsyncWorker(env, hamlib_instance), level_type_(level_type) {
        value_.f = 0.0;
    }
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_get_level(hamlib_instance_->my_rig, RIG_VFO_CURR, level_type_, &value_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, value_.f));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    setting_t level_type_;
    value_t value_;
};

class SetFunctionAsyncWorker : public HamLibAsyncWorker {
public:
    SetFunctionAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, setting_t func_type, int enable)
        : HamLibAsyncWorker(env, hamlib_instance), func_type_(func_type), enable_(enable) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_set_func(hamlib_instance_->my_rig, RIG_VFO_CURR, func_type_, enable_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    setting_t func_type_;
    int enable_;
};

class GetFunctionAsyncWorker : public HamLibAsyncWorker {
public:
    GetFunctionAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, setting_t func_type)
        : HamLibAsyncWorker(env, hamlib_instance), func_type_(func_type), state_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_get_func(hamlib_instance_->my_rig, RIG_VFO_CURR, func_type_, &state_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    setting_t func_type_;
    int state_;
};

class SetMemoryChannelAsyncWorker : public HamLibAsyncWorker {
public:
    SetMemoryChannelAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, const channel_t& chan)
        : HamLibAsyncWorker(env, hamlib_instance), chan_(chan) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_set_channel(hamlib_instance_->my_rig, RIG_VFO_MEM, &chan_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    channel_t chan_;
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
        chan_.vfo = RIG_VFO_MEM;
        result_code_ = rig_get_channel(hamlib_instance_->my_rig, RIG_VFO_MEM, &chan_, read_only_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        Napi::Object obj = Napi::Object::New(env);
        
        obj.Set(Napi::String::New(env, "channelNumber"), chan_.channel_num);
        obj.Set(Napi::String::New(env, "frequency"), chan_.freq);
        
        if (chan_.mode != RIG_MODE_NONE) {
            const char* mode_str = rig_strrmode(chan_.mode);
            obj.Set(Napi::String::New(env, "mode"), Napi::String::New(env, mode_str));
        }
        
        obj.Set(Napi::String::New(env, "bandwidth"), chan_.width);
        
        if (chan_.channel_desc[0] != '\0') {
            obj.Set(Napi::String::New(env, "description"), Napi::String::New(env, chan_.channel_desc));
        }
        
        if (chan_.split == RIG_SPLIT_ON) {
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
    channel_t chan_;
};

class SelectMemoryChannelAsyncWorker : public HamLibAsyncWorker {
public:
    SelectMemoryChannelAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, int channel_num)
        : HamLibAsyncWorker(env, hamlib_instance), channel_num_(channel_num) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_set_mem(hamlib_instance_->my_rig, RIG_VFO_CURR, channel_num_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    SetRitAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, shortfreq_t rit_offset)
        : HamLibAsyncWorker(env, hamlib_instance), rit_offset_(rit_offset) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_set_rit(hamlib_instance_->my_rig, RIG_VFO_CURR, rit_offset_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    shortfreq_t rit_offset_;
};

class GetRitAsyncWorker : public HamLibAsyncWorker {
public:
    GetRitAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance)
        : HamLibAsyncWorker(env, hamlib_instance), rit_offset_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_get_rit(hamlib_instance_->my_rig, RIG_VFO_CURR, &rit_offset_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    shortfreq_t rit_offset_;
};

class SetXitAsyncWorker : public HamLibAsyncWorker {
public:
    SetXitAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, shortfreq_t xit_offset)
        : HamLibAsyncWorker(env, hamlib_instance), xit_offset_(xit_offset) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_set_xit(hamlib_instance_->my_rig, RIG_VFO_CURR, xit_offset_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    shortfreq_t xit_offset_;
};

class GetXitAsyncWorker : public HamLibAsyncWorker {
public:
    GetXitAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance)
        : HamLibAsyncWorker(env, hamlib_instance), xit_offset_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_get_xit(hamlib_instance_->my_rig, RIG_VFO_CURR, &xit_offset_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    shortfreq_t xit_offset_;
};

class ClearRitXitAsyncWorker : public HamLibAsyncWorker {
public:
    ClearRitXitAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance)
        : HamLibAsyncWorker(env, hamlib_instance) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        int ritCode = rig_set_rit(hamlib_instance_->my_rig, RIG_VFO_CURR, 0);
        int xitCode = rig_set_xit(hamlib_instance_->my_rig, RIG_VFO_CURR, 0);
        
        if (ritCode != RIG_OK) {
            result_code_ = ritCode;
            error_message_ = rigerror(ritCode);
        } else if (xitCode != RIG_OK) {
            result_code_ = xitCode;
            error_message_ = rigerror(xitCode);
        } else {
            result_code_ = RIG_OK;
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    SetSplitFreqAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, freq_t tx_freq, vfo_t vfo = RIG_VFO_CURR)
        : HamLibAsyncWorker(env, hamlib_instance), tx_freq_(tx_freq), vfo_(vfo) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_set_split_freq(hamlib_instance_->my_rig, vfo_, tx_freq_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    freq_t tx_freq_;
    vfo_t vfo_;
};

class GetSplitFreqAsyncWorker : public HamLibAsyncWorker {
public:
    GetSplitFreqAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo = RIG_VFO_CURR)
        : HamLibAsyncWorker(env, hamlib_instance), tx_freq_(0), vfo_(vfo) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_get_split_freq(hamlib_instance_->my_rig, vfo_, &tx_freq_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    freq_t tx_freq_;
    vfo_t vfo_;
};

class SetSplitAsyncWorker : public HamLibAsyncWorker {
public:
    SetSplitAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t rx_vfo, split_t split, vfo_t tx_vfo)
        : HamLibAsyncWorker(env, hamlib_instance), rx_vfo_(rx_vfo), split_(split), tx_vfo_(tx_vfo) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_set_split_vfo(hamlib_instance_->my_rig, rx_vfo_, split_, tx_vfo_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    vfo_t rx_vfo_;
    split_t split_;
    vfo_t tx_vfo_;
};

class GetSplitAsyncWorker : public HamLibAsyncWorker {
public:
    GetSplitAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo = RIG_VFO_CURR)
        : HamLibAsyncWorker(env, hamlib_instance), split_(RIG_SPLIT_OFF), tx_vfo_(RIG_VFO_B), vfo_(vfo) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_get_split_vfo(hamlib_instance_->my_rig, vfo_, &split_, &tx_vfo_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            Napi::Object obj = Napi::Object::New(env);
            obj.Set(Napi::String::New(env, "enabled"), Napi::Boolean::New(env, split_ == RIG_SPLIT_ON));
            
            const char* vfo_str = "VFO-B";
            if (tx_vfo_ == RIG_VFO_A) {
                vfo_str = "VFO-A";
            }
            obj.Set(Napi::String::New(env, "txVfo"), Napi::String::New(env, vfo_str));
            
            deferred_.Resolve(obj);
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    split_t split_;
    vfo_t tx_vfo_;
    vfo_t vfo_;
};

class SetVfoAsyncWorker : public HamLibAsyncWorker {
public:
    SetVfoAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_set_vfo(hamlib_instance_->my_rig, vfo_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    vfo_t vfo_;
};

class GetVfoAsyncWorker : public HamLibAsyncWorker {
public:
    GetVfoAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_get_vfo(hamlib_instance_->my_rig, &vfo_);
        if (result_code_ != RIG_OK) {
            // 提供更清晰的错误信息
            switch (result_code_) {
                case RIG_ENAVAIL:
                case -11:  // Feature not available
                    error_message_ = "VFO query not supported by this radio";
                    break;
                case RIG_EIO:
                    error_message_ = "I/O error during VFO query";
                    break;
                case RIG_ETIMEOUT:
                    error_message_ = "Timeout during VFO query";
                    break;
                case RIG_EPROTO:
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
        if (result_code_ != RIG_OK) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
            return;
        }
        
        const char* vfo_str = "VFO-CURR";  // 默认值
        if (vfo_ == RIG_VFO_A) {
            vfo_str = "VFO-A";
        } else if (vfo_ == RIG_VFO_B) {
            vfo_str = "VFO-B";
        } else if (vfo_ == RIG_VFO_CURR) {
            vfo_str = "VFO-CURR";
        } else if (vfo_ == RIG_VFO_MEM) {
            vfo_str = "VFO-MEM";
        }
        deferred_.Resolve(Napi::String::New(env, vfo_str));
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    vfo_t vfo_;
};

class CloseAsyncWorker : public HamLibAsyncWorker {
public:
    CloseAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance)
        : HamLibAsyncWorker(env, hamlib_instance) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        // 检查rig是否已经关闭
        if (!hamlib_instance_->rig_is_open) {
            result_code_ = RIG_OK;  // 已经关闭，返回成功
            return;
        }
        
        result_code_ = rig_close(hamlib_instance_->my_rig);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        } else {
            hamlib_instance_->rig_is_open = false;
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
        result_code_ = rig_cleanup(hamlib_instance_->my_rig);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        } else {
            hamlib_instance_->rig_is_open = false;
            hamlib_instance_->my_rig = nullptr;  // 重要：清空指针防止重复释放
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    StartScanAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, scan_t scan_type, int channel)
        : HamLibAsyncWorker(env, hamlib_instance), scan_type_(scan_type), channel_(channel) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_scan(hamlib_instance_->my_rig, RIG_VFO_CURR, scan_type_, channel_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    scan_t scan_type_;
    int channel_;
};

// Stop Scan AsyncWorker
class StopScanAsyncWorker : public HamLibAsyncWorker {
public:
    StopScanAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance)
        : HamLibAsyncWorker(env, hamlib_instance) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_scan(hamlib_instance_->my_rig, RIG_VFO_CURR, RIG_SCAN_STOP, 0);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    VfoOperationAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_op_t vfo_op)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_op_(vfo_op) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_vfo_op(hamlib_instance_->my_rig, RIG_VFO_CURR, vfo_op_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    vfo_op_t vfo_op_;
};

// Set Antenna AsyncWorker
class SetAntennaAsyncWorker : public HamLibAsyncWorker {
public:
    SetAntennaAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, ant_t antenna, vfo_t vfo, value_t option)
        : HamLibAsyncWorker(env, hamlib_instance), antenna_(antenna), vfo_(vfo), option_(option) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_set_ant(hamlib_instance_->my_rig, vfo_, antenna_, option_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    ant_t antenna_;
    vfo_t vfo_;
    value_t option_;
};

// Get Antenna AsyncWorker
class GetAntennaAsyncWorker : public HamLibAsyncWorker {
public:
    GetAntennaAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo, ant_t antenna)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), antenna_(antenna), antenna_curr_(0), antenna_tx_(0), antenna_rx_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        option_ = {0};
        
        result_code_ = rig_get_ant(hamlib_instance_->my_rig, vfo_, antenna_, &option_, &antenna_curr_, &antenna_tx_, &antenna_rx_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            Napi::Object result = Napi::Object::New(env);
            
            result.Set("currentAntenna", Napi::Number::New(env, antenna_curr_));
            result.Set("txAntenna", Napi::Number::New(env, antenna_tx_));
            result.Set("rxAntenna", Napi::Number::New(env, antenna_rx_));
            result.Set("option", Napi::Number::New(env, option_.f));
            
            deferred_.Resolve(result);
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    vfo_t vfo_;
    ant_t antenna_;
    ant_t antenna_curr_;
    ant_t antenna_tx_;
    ant_t antenna_rx_;
    value_t option_;
};

// Power to mW Conversion AsyncWorker
class Power2mWAsyncWorker : public HamLibAsyncWorker {
public:
    Power2mWAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, float power, freq_t freq, rmode_t mode)
        : HamLibAsyncWorker(env, hamlib_instance), power_(power), freq_(freq), mode_(mode), mwpower_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_power2mW(hamlib_instance_->my_rig, &mwpower_, power_, freq_, mode_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    freq_t freq_;
    rmode_t mode_;
    unsigned int mwpower_;
};

// mW to Power Conversion AsyncWorker
class MW2PowerAsyncWorker : public HamLibAsyncWorker {
public:
    MW2PowerAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, unsigned int mwpower, freq_t freq, rmode_t mode)
        : HamLibAsyncWorker(env, hamlib_instance), mwpower_(mwpower), freq_(freq), mode_(mode), power_(0.0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_mW2power(hamlib_instance_->my_rig, &power_, mwpower_, freq_, mode_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    freq_t freq_;
    rmode_t mode_;
    float power_;
};

// Set Split Mode AsyncWorker
class SetSplitModeAsyncWorker : public HamLibAsyncWorker {
public:
    SetSplitModeAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, rmode_t tx_mode, pbwidth_t tx_width, vfo_t vfo = RIG_VFO_CURR)
        : HamLibAsyncWorker(env, hamlib_instance), tx_mode_(tx_mode), tx_width_(tx_width), vfo_(vfo) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_set_split_mode(hamlib_instance_->my_rig, vfo_, tx_mode_, tx_width_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    rmode_t tx_mode_;
    pbwidth_t tx_width_;
    vfo_t vfo_;
};

// Get Split Mode AsyncWorker
class GetSplitModeAsyncWorker : public HamLibAsyncWorker {
public:
    GetSplitModeAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo = RIG_VFO_CURR)
        : HamLibAsyncWorker(env, hamlib_instance), tx_mode_(0), tx_width_(0), vfo_(vfo) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_get_split_mode(hamlib_instance_->my_rig, vfo_, &tx_mode_, &tx_width_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            Napi::Object obj = Napi::Object::New(env);
            // Convert mode to string using rig_strrmode
            const char* mode_str = rig_strrmode(tx_mode_);
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
    rmode_t tx_mode_;
    pbwidth_t tx_width_;
    vfo_t vfo_;
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
            hamlib_instance_->my_rig->state.rigport.parm.serial.data_bits = data_bits;
            result_code_ = RIG_OK;
        } else if (param_name_ == "stop_bits") {
            int stop_bits = std::stoi(param_value_);
            hamlib_instance_->my_rig->state.rigport.parm.serial.stop_bits = stop_bits;
            result_code_ = RIG_OK;
        } else if (param_name_ == "serial_parity") {
            enum serial_parity_e parity;
            if (param_value_ == "None") {
                parity = RIG_PARITY_NONE;
            } else if (param_value_ == "Even") {
                parity = RIG_PARITY_EVEN;
            } else if (param_value_ == "Odd") {
                parity = RIG_PARITY_ODD;
            } else {
                result_code_ = -RIG_EINVAL;
                error_message_ = "Invalid parity value";
                return;
            }
            hamlib_instance_->my_rig->state.rigport.parm.serial.parity = parity;
            result_code_ = RIG_OK;
        } else if (param_name_ == "serial_handshake") {
            enum serial_handshake_e handshake;
            if (param_value_ == "None") {
                handshake = RIG_HANDSHAKE_NONE;
            } else if (param_value_ == "Hardware") {
                handshake = RIG_HANDSHAKE_HARDWARE;
            } else if (param_value_ == "Software") {
                handshake = RIG_HANDSHAKE_XONXOFF;
            } else {
                result_code_ = -RIG_EINVAL;
                error_message_ = "Invalid handshake value";
                return;
            }
            hamlib_instance_->my_rig->state.rigport.parm.serial.handshake = handshake;
            result_code_ = RIG_OK;
        } else if (param_name_ == "rts_state") {
            enum serial_control_state_e state;
            if (param_value_ == "ON") {
                state = RIG_SIGNAL_ON;
            } else if (param_value_ == "OFF") {
                state = RIG_SIGNAL_OFF;
            } else {
                result_code_ = -RIG_EINVAL;
                error_message_ = "Invalid RTS state value";
                return;
            }
            hamlib_instance_->my_rig->state.rigport.parm.serial.rts_state = state;
            result_code_ = RIG_OK;
        } else if (param_name_ == "dtr_state") {
            enum serial_control_state_e state;
            if (param_value_ == "ON") {
                state = RIG_SIGNAL_ON;
            } else if (param_value_ == "OFF") {
                state = RIG_SIGNAL_OFF;
            } else {
                result_code_ = -RIG_EINVAL;
                error_message_ = "Invalid DTR state value";
                return;
            }
            hamlib_instance_->my_rig->state.rigport.parm.serial.dtr_state = state;
            result_code_ = RIG_OK;
        } else if (param_name_ == "rate") {
            int rate = std::stoi(param_value_);
            // Validate supported baud rates based on common values in Hamlib
            std::vector<int> valid_rates = {150, 300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 
                                          230400, 460800, 500000, 576000, 921600, 1000000, 1152000, 1500000, 
                                          2000000, 2500000, 3000000, 3500000, 4000000};
            if (std::find(valid_rates.begin(), valid_rates.end(), rate) != valid_rates.end()) {
                hamlib_instance_->my_rig->state.rigport.parm.serial.rate = rate;
                result_code_ = RIG_OK;
            } else {
                result_code_ = -RIG_EINVAL;
                error_message_ = "Invalid baud rate value";
            }
        } else if (param_name_ == "timeout") {
            int timeout_val = std::stoi(param_value_);
            if (timeout_val >= 0) {
                hamlib_instance_->my_rig->state.rigport.timeout = timeout_val;
                result_code_ = RIG_OK;
            } else {
                result_code_ = -RIG_EINVAL;
                error_message_ = "Timeout must be non-negative";
            }
        } else if (param_name_ == "retry") {
            int retry_val = std::stoi(param_value_);
            if (retry_val >= 0) {
                hamlib_instance_->my_rig->state.rigport.retry = (short)retry_val;
                result_code_ = RIG_OK;
            } else {
                result_code_ = -RIG_EINVAL;
                error_message_ = "Retry count must be non-negative";
            }
        } else if (param_name_ == "write_delay") {
            int delay = std::stoi(param_value_);
            if (delay >= 0) {
                hamlib_instance_->my_rig->state.rigport.write_delay = delay;
                result_code_ = RIG_OK;
            } else {
                result_code_ = -RIG_EINVAL;
                error_message_ = "Write delay must be non-negative";
            }
        } else if (param_name_ == "post_write_delay") {
            int delay = std::stoi(param_value_);
            if (delay >= 0) {
                hamlib_instance_->my_rig->state.rigport.post_write_delay = delay;
                result_code_ = RIG_OK;
            } else {
                result_code_ = -RIG_EINVAL;
                error_message_ = "Post write delay must be non-negative";
            }
        } else if (param_name_ == "flushx") {
            if (param_value_ == "true" || param_value_ == "1") {
                hamlib_instance_->my_rig->state.rigport.flushx = 1;
                result_code_ = RIG_OK;
            } else if (param_value_ == "false" || param_value_ == "0") {
                hamlib_instance_->my_rig->state.rigport.flushx = 0;
                result_code_ = RIG_OK;
            } else {
                result_code_ = -RIG_EINVAL;
                error_message_ = "Flushx must be true/false or 1/0";
            }
        } else {
            result_code_ = -RIG_EINVAL;
            error_message_ = "Unknown serial configuration parameter";
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
            param_value_ = std::to_string(hamlib_instance_->my_rig->state.rigport.parm.serial.data_bits);
            result_code_ = RIG_OK;
        } else if (param_name_ == "stop_bits") {
            param_value_ = std::to_string(hamlib_instance_->my_rig->state.rigport.parm.serial.stop_bits);
            result_code_ = RIG_OK;
        } else if (param_name_ == "serial_parity") {
            switch (hamlib_instance_->my_rig->state.rigport.parm.serial.parity) {
                case RIG_PARITY_NONE:
                    param_value_ = "None";
                    break;
                case RIG_PARITY_EVEN:
                    param_value_ = "Even";
                    break;
                case RIG_PARITY_ODD:
                    param_value_ = "Odd";
                    break;
                default:
                    param_value_ = "Unknown";
                    break;
            }
            result_code_ = RIG_OK;
        } else if (param_name_ == "serial_handshake") {
            switch (hamlib_instance_->my_rig->state.rigport.parm.serial.handshake) {
                case RIG_HANDSHAKE_NONE:
                    param_value_ = "None";
                    break;
                case RIG_HANDSHAKE_HARDWARE:
                    param_value_ = "Hardware";
                    break;
                case RIG_HANDSHAKE_XONXOFF:
                    param_value_ = "Software";
                    break;
                default:
                    param_value_ = "Unknown";
                    break;
            }
            result_code_ = RIG_OK;
        } else if (param_name_ == "rts_state") {
            switch (hamlib_instance_->my_rig->state.rigport.parm.serial.rts_state) {
                case RIG_SIGNAL_ON:
                    param_value_ = "ON";
                    break;
                case RIG_SIGNAL_OFF:
                    param_value_ = "OFF";
                    break;
                default:
                    param_value_ = "Unknown";
                    break;
            }
            result_code_ = RIG_OK;
        } else if (param_name_ == "dtr_state") {
            switch (hamlib_instance_->my_rig->state.rigport.parm.serial.dtr_state) {
                case RIG_SIGNAL_ON:
                    param_value_ = "ON";
                    break;
                case RIG_SIGNAL_OFF:
                    param_value_ = "OFF";
                    break;
                default:
                    param_value_ = "Unknown";
                    break;
            }
            result_code_ = RIG_OK;
        } else if (param_name_ == "rate") {
            param_value_ = std::to_string(hamlib_instance_->my_rig->state.rigport.parm.serial.rate);
            result_code_ = RIG_OK;
        } else if (param_name_ == "timeout") {
            param_value_ = std::to_string(hamlib_instance_->my_rig->state.rigport.timeout);
            result_code_ = RIG_OK;
        } else if (param_name_ == "retry") {
            param_value_ = std::to_string(hamlib_instance_->my_rig->state.rigport.retry);
            result_code_ = RIG_OK;
        } else if (param_name_ == "write_delay") {
            param_value_ = std::to_string(hamlib_instance_->my_rig->state.rigport.write_delay);
            result_code_ = RIG_OK;
        } else if (param_name_ == "post_write_delay") {
            param_value_ = std::to_string(hamlib_instance_->my_rig->state.rigport.post_write_delay);
            result_code_ = RIG_OK;
        } else if (param_name_ == "flushx") {
            param_value_ = hamlib_instance_->my_rig->state.rigport.flushx ? "true" : "false";
            result_code_ = RIG_OK;
        } else {
            result_code_ = -RIG_EINVAL;
            error_message_ = "Unknown serial configuration parameter";
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    SetPttTypeAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, const std::string& ptt_type)
        : HamLibAsyncWorker(env, hamlib_instance), ptt_type_(ptt_type) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        ptt_type_t ptt_type;
        if (ptt_type_ == "RIG") {
            ptt_type = RIG_PTT_RIG;
        } else if (ptt_type_ == "DTR") {
            ptt_type = RIG_PTT_SERIAL_DTR;
        } else if (ptt_type_ == "RTS") {
            ptt_type = RIG_PTT_SERIAL_RTS;
        } else if (ptt_type_ == "PARALLEL") {
            ptt_type = RIG_PTT_PARALLEL;
        } else if (ptt_type_ == "CM108") {
            ptt_type = RIG_PTT_CM108;
        } else if (ptt_type_ == "GPIO") {
            ptt_type = RIG_PTT_GPIO;
        } else if (ptt_type_ == "GPION") {
            ptt_type = RIG_PTT_GPION;
        } else if (ptt_type_ == "NONE") {
            ptt_type = RIG_PTT_NONE;
        } else {
            result_code_ = -RIG_EINVAL;
            error_message_ = "Invalid PTT type";
            return;
        }
        
        hamlib_instance_->my_rig->state.pttport.type.ptt = ptt_type;
        result_code_ = RIG_OK;
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    std::string ptt_type_;
};

// Get PTT Type AsyncWorker
class GetPttTypeAsyncWorker : public HamLibAsyncWorker {
public:
    GetPttTypeAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance)
        : HamLibAsyncWorker(env, hamlib_instance) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        ptt_type_t ptt_type = hamlib_instance_->my_rig->state.pttport.type.ptt;
        
        switch (ptt_type) {
            case RIG_PTT_RIG:
                ptt_type_str_ = "RIG";
                break;
            case RIG_PTT_SERIAL_DTR:
                ptt_type_str_ = "DTR";
                break;
            case RIG_PTT_SERIAL_RTS:
                ptt_type_str_ = "RTS";
                break;
            case RIG_PTT_PARALLEL:
                ptt_type_str_ = "PARALLEL";
                break;
            case RIG_PTT_CM108:
                ptt_type_str_ = "CM108";
                break;
            case RIG_PTT_GPIO:
                ptt_type_str_ = "GPIO";
                break;
            case RIG_PTT_GPION:
                ptt_type_str_ = "GPION";
                break;
            case RIG_PTT_NONE:
                ptt_type_str_ = "NONE";
                break;
            default:
                ptt_type_str_ = "Unknown";
                break;
        }
        result_code_ = RIG_OK;
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::String::New(env, ptt_type_str_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    std::string ptt_type_str_;
};

// Set DCD Type AsyncWorker
class SetDcdTypeAsyncWorker : public HamLibAsyncWorker {
public:
    SetDcdTypeAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, const std::string& dcd_type)
        : HamLibAsyncWorker(env, hamlib_instance), dcd_type_(dcd_type) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        dcd_type_t dcd_type;
        if (dcd_type_ == "RIG") {
            dcd_type = RIG_DCD_RIG;
        } else if (dcd_type_ == "DSR") {
            dcd_type = RIG_DCD_SERIAL_DSR;
        } else if (dcd_type_ == "CTS") {
            dcd_type = RIG_DCD_SERIAL_CTS;
        } else if (dcd_type_ == "CD") {
            dcd_type = RIG_DCD_SERIAL_CAR;
        } else if (dcd_type_ == "PARALLEL") {
            dcd_type = RIG_DCD_PARALLEL;
        } else if (dcd_type_ == "CM108") {
            dcd_type = RIG_DCD_CM108;
        } else if (dcd_type_ == "GPIO") {
            dcd_type = RIG_DCD_GPIO;
        } else if (dcd_type_ == "GPION") {
            dcd_type = RIG_DCD_GPION;
        } else if (dcd_type_ == "NONE") {
            dcd_type = RIG_DCD_NONE;
        } else {
            result_code_ = -RIG_EINVAL;
            error_message_ = "Invalid DCD type";
            return;
        }
        
        hamlib_instance_->my_rig->state.dcdport.type.dcd = dcd_type;
        result_code_ = RIG_OK;
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    std::string dcd_type_;
};

// Get DCD Type AsyncWorker
class GetDcdTypeAsyncWorker : public HamLibAsyncWorker {
public:
    GetDcdTypeAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance)
        : HamLibAsyncWorker(env, hamlib_instance) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        dcd_type_t dcd_type = hamlib_instance_->my_rig->state.dcdport.type.dcd;
        
        switch (dcd_type) {
            case RIG_DCD_RIG:
                dcd_type_str_ = "RIG";
                break;
            case RIG_DCD_SERIAL_DSR:
                dcd_type_str_ = "DSR";
                break;
            case RIG_DCD_SERIAL_CTS:
                dcd_type_str_ = "CTS";
                break;
            case RIG_DCD_SERIAL_CAR:
                dcd_type_str_ = "CD";
                break;
            case RIG_DCD_PARALLEL:
                dcd_type_str_ = "PARALLEL";
                break;
            case RIG_DCD_CM108:
                dcd_type_str_ = "CM108";
                break;
            case RIG_DCD_GPIO:
                dcd_type_str_ = "GPIO";
                break;
            case RIG_DCD_GPION:
                dcd_type_str_ = "GPION";
                break;
            case RIG_DCD_NONE:
                dcd_type_str_ = "NONE";
                break;
            default:
                dcd_type_str_ = "Unknown";
                break;
        }
        result_code_ = RIG_OK;
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::String::New(env, dcd_type_str_));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    std::string dcd_type_str_;
};

// Power Control AsyncWorker classes
class SetPowerstatAsyncWorker : public HamLibAsyncWorker {
public:
    SetPowerstatAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, powerstat_t status)
        : HamLibAsyncWorker(env, hamlib_instance), status_(status) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_set_powerstat(hamlib_instance_->my_rig, status_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    powerstat_t status_;
};

class GetPowerstatAsyncWorker : public HamLibAsyncWorker {
public:
    GetPowerstatAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance)
        : HamLibAsyncWorker(env, hamlib_instance), status_(RIG_POWER_UNKNOWN) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_get_powerstat(hamlib_instance_->my_rig, &status_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    powerstat_t status_;
};

// PTT Status Detection AsyncWorker
class GetPttAsyncWorker : public HamLibAsyncWorker {
public:
    GetPttAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), ptt_(RIG_PTT_OFF) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_get_ptt(hamlib_instance_->my_rig, vfo_, &ptt_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Boolean::New(env, ptt_ == RIG_PTT_ON));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    vfo_t vfo_;
    ptt_t ptt_;
};

// Data Carrier Detect AsyncWorker
class GetDcdAsyncWorker : public HamLibAsyncWorker {
public:
    GetDcdAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), dcd_(RIG_DCD_OFF) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_get_dcd(hamlib_instance_->my_rig, vfo_, &dcd_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Boolean::New(env, dcd_ == RIG_DCD_ON));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    vfo_t vfo_;
    dcd_t dcd_;
};

// Tuning Step Control AsyncWorker classes
class SetTuningStepAsyncWorker : public HamLibAsyncWorker {
public:
    SetTuningStepAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo, shortfreq_t ts)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), ts_(ts) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_set_ts(hamlib_instance_->my_rig, vfo_, ts_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    vfo_t vfo_;
    shortfreq_t ts_;
};

class GetTuningStepAsyncWorker : public HamLibAsyncWorker {
public:
    GetTuningStepAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), ts_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_get_ts(hamlib_instance_->my_rig, vfo_, &ts_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    vfo_t vfo_;
    shortfreq_t ts_;
};

// Repeater Control AsyncWorker classes
class SetRepeaterShiftAsyncWorker : public HamLibAsyncWorker {
public:
    SetRepeaterShiftAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo, rptr_shift_t shift)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), shift_(shift) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_set_rptr_shift(hamlib_instance_->my_rig, vfo_, shift_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    vfo_t vfo_;
    rptr_shift_t shift_;
};

class GetRepeaterShiftAsyncWorker : public HamLibAsyncWorker {
public:
    GetRepeaterShiftAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), shift_(RIG_RPT_SHIFT_NONE) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_get_rptr_shift(hamlib_instance_->my_rig, vfo_, &shift_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        const char* shift_str = rig_strptrshift(shift_);
        deferred_.Resolve(Napi::String::New(env, shift_str));
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    vfo_t vfo_;
    rptr_shift_t shift_;
};

class SetRepeaterOffsetAsyncWorker : public HamLibAsyncWorker {
public:
    SetRepeaterOffsetAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo, shortfreq_t offset)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), offset_(offset) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_set_rptr_offs(hamlib_instance_->my_rig, vfo_, offset_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    vfo_t vfo_;
    shortfreq_t offset_;
};

class GetRepeaterOffsetAsyncWorker : public HamLibAsyncWorker {
public:
    GetRepeaterOffsetAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), offset_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_get_rptr_offs(hamlib_instance_->my_rig, vfo_, &offset_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    vfo_t vfo_;
    shortfreq_t offset_;
};

// Helper function to parse VFO parameter from JavaScript
vfo_t parseVfoParameter(const Napi::CallbackInfo& info, int index, vfo_t defaultVfo = RIG_VFO_CURR) {
    if (info.Length() > index && info[index].IsString()) {
        std::string vfoStr = info[index].As<Napi::String>().Utf8Value();
        if (vfoStr == "VFO-A") {
            return RIG_VFO_A;
        } else if (vfoStr == "VFO-B") {
            return RIG_VFO_B;
        }
    }
    return defaultVfo;
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

  // Set default port path if not provided
  strncpy(port_path, "/dev/ttyUSB0", HAMLIB_FILPATHLEN - 1);
  port_path[HAMLIB_FILPATHLEN - 1] = '\0';

  // Check if port path is provided as second argument
  if (info.Length() >= 2) {
    if (info[1].IsString()) {
      std::string portStr = info[1].As<Napi::String>().Utf8Value();
      strncpy(port_path, portStr.c_str(), HAMLIB_FILPATHLEN - 1);
      port_path[HAMLIB_FILPATHLEN - 1] = '\0';
    } else {
      // If second argument exists but is not a string, treat it as debug level (backward compatibility)
      rig_set_debug_level(RIG_DEBUG_NONE);
    }
  } else {
    rig_set_debug_level(RIG_DEBUG_NONE);
  }
  //rig_model_t myrig_model;
  //   hamlib_port_t myport;
  // /* may be overriden by backend probe */
  // myport.type.rig = RIG_PORT_SERIAL;
  // myport.parm.serial.rate = 38400;
  // myport.parm.serial.data_bits = 8;
  // myport.parm.serial.stop_bits = 2;
  // myport.parm.serial.parity = RIG_PARITY_NONE;
  // myport.parm.serial.handshake = RIG_HANDSHAKE_HARDWARE;
  // strncpy(myport.pathname, "/dev/ttyUSB0", HAMLIB_FILPATHLEN - 1);

  // rig_load_all_backends();
  // myrig_model = rig_probe(&myport);
  // fprintf(stderr, "Got Rig Model %d \n", myrig_model);

  rig_model_t myrig_model = info[0].As < Napi::Number > ().DoubleValue();
  original_model = myrig_model;

  // Check if port_path is a network address (contains colon)
  is_network_rig = isNetworkAddress(port_path);
  
  if (is_network_rig) {
    // Use NETRIGCTL model for network connections
    myrig_model = 2; // RIG_MODEL_NETRIGCTL
    printf("Using network connection to %s\n", port_path);
  }

  my_rig = rig_init(myrig_model);
  //int retcode = 0;
  if (!my_rig) {
    fprintf(stderr, "Unknown rig num: %d\n", myrig_model);
    fprintf(stderr, "Please check riglist.h\n");
    Napi::TypeError::New(env, "Unable to Init Rig").ThrowAsJavaScriptException();
  }
  
  // Set port path and type based on connection type
  strncpy(my_rig -> state.rigport.pathname, port_path, HAMLIB_FILPATHLEN - 1);
  
  if (is_network_rig) {
    my_rig -> state.rigport.type.rig = RIG_PORT_NETWORK;
  } else {
    my_rig -> state.rigport.type.rig = RIG_PORT_SERIAL;
  }

  // this->freq_emit_cb = [info](freq_t freq) {
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
  // 如果rig指针存在，执行清理
  if (my_rig) {
    // 如果rig是打开状态，先关闭
    if (rig_is_open) {
      rig_close(my_rig);
      rig_is_open = false;
    }
    // 清理rig资源
    rig_cleanup(my_rig);
    my_rig = nullptr;
  }
}

int NodeHamLib::freq_change_cb(RIG *rig, vfo_t vfo, freq_t freq, void* arg) {
      auto instance = static_cast<NodeHamLib*>(arg);
      printf("Rig changed freq to %0.7f Hz\n", freq);
      Napi::Env env = instance->m_currentInfo->Env();
      //Napi::Function emit = instance->m_currentInfo[0].Get("emit").As<Napi::Function>();
			// Napi::Function emit = instance->m_currentInfo[0]->This().As<Napi::Object>().Get("emit").As<Napi::Function>();
      //emit.Call(instance->m_currentInfo->This(), { Napi::String::New(env, "frequency_change"), Napi::Number::New(env, freq) });
        //this->freq_emit_cb(freq);
        //Napi::Function emit = this.As<Napi::Object>().Get("emit").As<Napi::Function>();
        //auto fn = global.Get("process").As<Napi::Object>().Get("emit").As<Napi::Function>();
        //fn.Call({Napi::Number::New(env, freq)});
        return 0;
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
    Napi::TypeError::New(env, "Must specify VFO-A or VFO-B as a string")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  
  std::string name = info[0].As<Napi::String>().Utf8Value();
  vfo_t vfo;
  
  if (name == "VFO-A") {
    vfo = RIG_VFO_A;
  } else if (name == "VFO-B") {
    vfo = RIG_VFO_B;
  } else {
    Napi::TypeError::New(env, "Invalid VFO name")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  
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
  
  freq_t freq = info[0].As<Napi::Number>().DoubleValue();
  
  // Basic frequency range validation
  if (freq < 1000 || freq > 10000000000) { // 1 kHz to 10 GHz reasonable range
    Napi::Error::New(env, "Frequency out of range (1 kHz - 10 GHz)").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  // Support optional VFO parameter
  vfo_t vfo = RIG_VFO_CURR;
  
  if (info.Length() >= 2 && info[1].IsString()) {
    std::string vfostr = info[1].As<Napi::String>().Utf8Value();
    if (vfostr == "VFO-A") {
      vfo = RIG_VFO_A;
    } else if (vfostr == "VFO-B") {
      vfo = RIG_VFO_B;
    }
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
  rmode_t mode = rig_parse_mode(modestr.c_str());
  
  if (mode == RIG_MODE_NONE) {
    Napi::Error::New(env, "Invalid mode: " + modestr).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  pbwidth_t bandwidth = RIG_PASSBAND_NORMAL;
  vfo_t vfo = RIG_VFO_CURR;

  // Parse parameters: setMode(mode) or setMode(mode, bandwidth) or setMode(mode, bandwidth, vfo)
  if (info.Length() >= 2 && info[1].IsString()) {
    std::string bandstr = info[1].As<Napi::String>().Utf8Value();
    if (bandstr == "narrow") {
      bandwidth = rig_passband_narrow(my_rig, mode);
    } else if (bandstr == "wide") {
      bandwidth = rig_passband_wide(my_rig, mode);
    } else {
      // If second parameter is not "narrow" or "wide", might be VFO
      vfo = parseVfoParameter(info, 1, RIG_VFO_CURR);
    }
  }
  
  // Check for third parameter (VFO) if bandwidth was specified
  if (info.Length() >= 3) {
    vfo = parseVfoParameter(info, 2, RIG_VFO_CURR);
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
  
  ptt_t ptt = ptt_state ? RIG_PTT_ON : RIG_PTT_OFF;
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
  vfo_t vfo = RIG_VFO_CURR;
  
  if (info.Length() >= 1 && info[0].IsString()) {
    std::string vfostr = info[0].As<Napi::String>().Utf8Value();
    if (vfostr == "VFO-A") {
      vfo = RIG_VFO_A;
    } else if (vfostr == "VFO-B") {
      vfo = RIG_VFO_B;
    }
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
  vfo_t vfo = RIG_VFO_CURR;
  
  if (info.Length() >= 1 && info[0].IsString()) {
    vfo = parseVfoParameter(info, 0, RIG_VFO_CURR);
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
  
  CloseAsyncWorker* worker = new CloseAsyncWorker(env, this);
  worker->Queue();
  
  return worker->GetPromise();
}

Napi::Value NodeHamLib::Destroy(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
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
  channel_t chan;
  memset(&chan, 0, sizeof(chan));
  
  chan.channel_num = channel_num;
  chan.vfo = RIG_VFO_MEM;
  
  // Extract frequency
  if (chanObj.Has("frequency")) {
    chan.freq = chanObj.Get("frequency").As<Napi::Number>().DoubleValue();
  }
  
  // Extract mode
  if (chanObj.Has("mode")) {
    std::string modeStr = chanObj.Get("mode").As<Napi::String>().Utf8Value();
    chan.mode = rig_parse_mode(modeStr.c_str());
  }
  
  // Extract bandwidth
  if (chanObj.Has("bandwidth")) {
    chan.width = chanObj.Get("bandwidth").As<Napi::Number>().Int32Value();
  } else {
    chan.width = RIG_PASSBAND_NORMAL;
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
    chan.split = RIG_SPLIT_ON;
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
  
  if (info.Length() < 1 || !info[0].IsNumber()) {
    Napi::TypeError::New(env, "Expected (channelNumber: number) or (channelNumber: number, readOnly: boolean)").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  int channel_num = info[0].As<Napi::Number>().Int32Value();
  bool read_only = true;
  
  if (info.Length() >= 2 && info[1].IsBoolean()) {
    // (channelNumber, readOnly)
    read_only = info[1].As<Napi::Boolean>().Value();
  }
  
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
  
  shortfreq_t rit_offset = info[0].As<Napi::Number>().Int32Value();
  
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
  
  shortfreq_t xit_offset = info[0].As<Napi::Number>().Int32Value();
  
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
  
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected scan type (VFO, MEM, PROG, etc.)").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  std::string scanTypeStr = info[0].As<Napi::String>().Utf8Value();
  scan_t scanType;
  
  if (scanTypeStr == "VFO") {
    scanType = RIG_SCAN_VFO;
  } else if (scanTypeStr == "MEM") {
    scanType = RIG_SCAN_MEM;
  } else if (scanTypeStr == "PROG") {
    scanType = RIG_SCAN_PROG;
  } else if (scanTypeStr == "DELTA") {
    scanType = RIG_SCAN_DELTA;
  } else if (scanTypeStr == "PRIO") {
    scanType = RIG_SCAN_PRIO;
  } else {
    Napi::TypeError::New(env, "Invalid scan type").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  int channel = 0;
  if (info.Length() > 1 && info[1].IsNumber()) {
    channel = info[1].As<Napi::Number>().Int32Value();
  }
  
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
    Napi::TypeError::New(env, "Expected (levelType: string, value: number)").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  std::string levelTypeStr = info[0].As<Napi::String>().Utf8Value();
  double levelValue = info[1].As<Napi::Number>().DoubleValue();
  
  // Map level type strings to hamlib constants
  setting_t levelType;
  if (levelTypeStr == "AF") {
    levelType = RIG_LEVEL_AF;
  } else if (levelTypeStr == "RF") {
    levelType = RIG_LEVEL_RF;
  } else if (levelTypeStr == "SQL") {
    levelType = RIG_LEVEL_SQL;
  } else if (levelTypeStr == "RFPOWER") {
    levelType = RIG_LEVEL_RFPOWER;
  } else if (levelTypeStr == "MICGAIN") {
    levelType = RIG_LEVEL_MICGAIN;
  } else if (levelTypeStr == "IF") {
    levelType = RIG_LEVEL_IF;
  } else if (levelTypeStr == "APF") {
    levelType = RIG_LEVEL_APF;
  } else if (levelTypeStr == "NR") {
    levelType = RIG_LEVEL_NR;
  } else if (levelTypeStr == "PBT_IN") {
    levelType = RIG_LEVEL_PBT_IN;
  } else if (levelTypeStr == "PBT_OUT") {
    levelType = RIG_LEVEL_PBT_OUT;
  } else if (levelTypeStr == "CWPITCH") {
    levelType = RIG_LEVEL_CWPITCH;
  } else if (levelTypeStr == "KEYSPD") {
    levelType = RIG_LEVEL_KEYSPD;
  } else if (levelTypeStr == "NOTCHF") {
    levelType = RIG_LEVEL_NOTCHF;
  } else if (levelTypeStr == "COMP") {
    levelType = RIG_LEVEL_COMP;
  } else if (levelTypeStr == "AGC") {
    levelType = RIG_LEVEL_AGC;
  } else if (levelTypeStr == "BKINDL") {
    levelType = RIG_LEVEL_BKINDL;
  } else if (levelTypeStr == "BALANCE") {
    levelType = RIG_LEVEL_BALANCE;
  } else if (levelTypeStr == "VOXGAIN") {
    levelType = RIG_LEVEL_VOXGAIN;
  } else if (levelTypeStr == "VOXDELAY") {
    levelType = RIG_LEVEL_VOXDELAY;
  } else if (levelTypeStr == "ANTIVOX") {
    levelType = RIG_LEVEL_ANTIVOX;
  } else {
    Napi::TypeError::New(env, "Invalid level type").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  // Create value union
  value_t val;
  val.f = levelValue;
  
  SetLevelAsyncWorker* worker = new SetLevelAsyncWorker(env, this, levelType, val);
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
  setting_t levelType;
  if (levelTypeStr == "AF") {
    levelType = RIG_LEVEL_AF;
  } else if (levelTypeStr == "RF") {
    levelType = RIG_LEVEL_RF;
  } else if (levelTypeStr == "SQL") {
    levelType = RIG_LEVEL_SQL;
  } else if (levelTypeStr == "RFPOWER") {
    levelType = RIG_LEVEL_RFPOWER;
  } else if (levelTypeStr == "MICGAIN") {
    levelType = RIG_LEVEL_MICGAIN;
  } else if (levelTypeStr == "SWR") {
    levelType = RIG_LEVEL_SWR;
  } else if (levelTypeStr == "ALC") {
    levelType = RIG_LEVEL_ALC;
  } else if (levelTypeStr == "STRENGTH") {
    levelType = RIG_LEVEL_STRENGTH;
  } else if (levelTypeStr == "RAWSTR") {
    levelType = RIG_LEVEL_RAWSTR;
  } else if (levelTypeStr == "RFPOWER_METER") {
    levelType = RIG_LEVEL_RFPOWER_METER;
  } else if (levelTypeStr == "COMP_METER") {
    levelType = RIG_LEVEL_COMP_METER;
  } else if (levelTypeStr == "VD_METER") {
    levelType = RIG_LEVEL_VD_METER;
  } else if (levelTypeStr == "ID_METER") {
    levelType = RIG_LEVEL_ID_METER;
  } else if (levelTypeStr == "TEMP_METER") {
    levelType = RIG_LEVEL_TEMP_METER;
  } else {
    Napi::TypeError::New(env, "Invalid level type").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  GetLevelAsyncWorker* worker = new GetLevelAsyncWorker(env, this, levelType);
  worker->Queue();
  
  return worker->GetPromise();
}

Napi::Value NodeHamLib::GetSupportedLevels(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  // Get capabilities from rig caps instead of state (doesn't require rig to be open)
  setting_t levels = my_rig->caps->has_get_level | my_rig->caps->has_set_level;
  Napi::Array levelArray = Napi::Array::New(env);
  uint32_t index = 0;
  
  // Check each level type
  if (levels & RIG_LEVEL_AF) levelArray[index++] = Napi::String::New(env, "AF");
  if (levels & RIG_LEVEL_RF) levelArray[index++] = Napi::String::New(env, "RF");
  if (levels & RIG_LEVEL_SQL) levelArray[index++] = Napi::String::New(env, "SQL");
  if (levels & RIG_LEVEL_RFPOWER) levelArray[index++] = Napi::String::New(env, "RFPOWER");
  if (levels & RIG_LEVEL_MICGAIN) levelArray[index++] = Napi::String::New(env, "MICGAIN");
  if (levels & RIG_LEVEL_IF) levelArray[index++] = Napi::String::New(env, "IF");
  if (levels & RIG_LEVEL_APF) levelArray[index++] = Napi::String::New(env, "APF");
  if (levels & RIG_LEVEL_NR) levelArray[index++] = Napi::String::New(env, "NR");
  if (levels & RIG_LEVEL_PBT_IN) levelArray[index++] = Napi::String::New(env, "PBT_IN");
  if (levels & RIG_LEVEL_PBT_OUT) levelArray[index++] = Napi::String::New(env, "PBT_OUT");
  if (levels & RIG_LEVEL_CWPITCH) levelArray[index++] = Napi::String::New(env, "CWPITCH");
  if (levels & RIG_LEVEL_KEYSPD) levelArray[index++] = Napi::String::New(env, "KEYSPD");
  if (levels & RIG_LEVEL_NOTCHF) levelArray[index++] = Napi::String::New(env, "NOTCHF");
  if (levels & RIG_LEVEL_COMP) levelArray[index++] = Napi::String::New(env, "COMP");
  if (levels & RIG_LEVEL_AGC) levelArray[index++] = Napi::String::New(env, "AGC");
  if (levels & RIG_LEVEL_BKINDL) levelArray[index++] = Napi::String::New(env, "BKINDL");
  if (levels & RIG_LEVEL_BALANCE) levelArray[index++] = Napi::String::New(env, "BALANCE");
  if (levels & RIG_LEVEL_VOXGAIN) levelArray[index++] = Napi::String::New(env, "VOXGAIN");
  if (levels & RIG_LEVEL_VOXDELAY) levelArray[index++] = Napi::String::New(env, "VOXDELAY");
  if (levels & RIG_LEVEL_ANTIVOX) levelArray[index++] = Napi::String::New(env, "ANTIVOX");
  if (levels & RIG_LEVEL_STRENGTH) levelArray[index++] = Napi::String::New(env, "STRENGTH");
  if (levels & RIG_LEVEL_RAWSTR) levelArray[index++] = Napi::String::New(env, "RAWSTR");
  if (levels & RIG_LEVEL_SWR) levelArray[index++] = Napi::String::New(env, "SWR");
  if (levels & RIG_LEVEL_ALC) levelArray[index++] = Napi::String::New(env, "ALC");
  if (levels & RIG_LEVEL_RFPOWER_METER) levelArray[index++] = Napi::String::New(env, "RFPOWER_METER");
  if (levels & RIG_LEVEL_COMP_METER) levelArray[index++] = Napi::String::New(env, "COMP_METER");
  if (levels & RIG_LEVEL_VD_METER) levelArray[index++] = Napi::String::New(env, "VD_METER");
  if (levels & RIG_LEVEL_ID_METER) levelArray[index++] = Napi::String::New(env, "ID_METER");
  if (levels & RIG_LEVEL_TEMP_METER) levelArray[index++] = Napi::String::New(env, "TEMP_METER");
  
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
  setting_t funcType;
  if (funcTypeStr == "FAGC") {
    funcType = RIG_FUNC_FAGC;
  } else if (funcTypeStr == "NB") {
    funcType = RIG_FUNC_NB;
  } else if (funcTypeStr == "COMP") {
    funcType = RIG_FUNC_COMP;
  } else if (funcTypeStr == "VOX") {
    funcType = RIG_FUNC_VOX;
  } else if (funcTypeStr == "TONE") {
    funcType = RIG_FUNC_TONE;
  } else if (funcTypeStr == "TSQL") {
    funcType = RIG_FUNC_TSQL;
  } else if (funcTypeStr == "SBKIN") {
    funcType = RIG_FUNC_SBKIN;
  } else if (funcTypeStr == "FBKIN") {
    funcType = RIG_FUNC_FBKIN;
  } else if (funcTypeStr == "ANF") {
    funcType = RIG_FUNC_ANF;
  } else if (funcTypeStr == "NR") {
    funcType = RIG_FUNC_NR;
  } else if (funcTypeStr == "AIP") {
    funcType = RIG_FUNC_AIP;
  } else if (funcTypeStr == "APF") {
    funcType = RIG_FUNC_APF;
  } else if (funcTypeStr == "TUNER") {
    funcType = RIG_FUNC_TUNER;
  } else if (funcTypeStr == "XIT") {
    funcType = RIG_FUNC_XIT;
  } else if (funcTypeStr == "RIT") {
    funcType = RIG_FUNC_RIT;
  } else if (funcTypeStr == "LOCK") {
    funcType = RIG_FUNC_LOCK;
  } else if (funcTypeStr == "MUTE") {
    funcType = RIG_FUNC_MUTE;
  } else if (funcTypeStr == "VSC") {
    funcType = RIG_FUNC_VSC;
  } else if (funcTypeStr == "REV") {
    funcType = RIG_FUNC_REV;
  } else if (funcTypeStr == "SQL") {
    funcType = RIG_FUNC_SQL;
  } else if (funcTypeStr == "ABM") {
    funcType = RIG_FUNC_ABM;
  } else if (funcTypeStr == "BC") {
    funcType = RIG_FUNC_BC;
  } else if (funcTypeStr == "MBC") {
    funcType = RIG_FUNC_MBC;
  } else if (funcTypeStr == "AFC") {
    funcType = RIG_FUNC_AFC;
  } else if (funcTypeStr == "SATMODE") {
    funcType = RIG_FUNC_SATMODE;
  } else if (funcTypeStr == "SCOPE") {
    funcType = RIG_FUNC_SCOPE;
  } else if (funcTypeStr == "RESUME") {
    funcType = RIG_FUNC_RESUME;
  } else if (funcTypeStr == "TBURST") {
    funcType = RIG_FUNC_TBURST;
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
  setting_t funcType;
  if (funcTypeStr == "FAGC") {
    funcType = RIG_FUNC_FAGC;
  } else if (funcTypeStr == "NB") {
    funcType = RIG_FUNC_NB;
  } else if (funcTypeStr == "COMP") {
    funcType = RIG_FUNC_COMP;
  } else if (funcTypeStr == "VOX") {
    funcType = RIG_FUNC_VOX;
  } else if (funcTypeStr == "TONE") {
    funcType = RIG_FUNC_TONE;
  } else if (funcTypeStr == "TSQL") {
    funcType = RIG_FUNC_TSQL;
  } else if (funcTypeStr == "SBKIN") {
    funcType = RIG_FUNC_SBKIN;
  } else if (funcTypeStr == "FBKIN") {
    funcType = RIG_FUNC_FBKIN;
  } else if (funcTypeStr == "ANF") {
    funcType = RIG_FUNC_ANF;
  } else if (funcTypeStr == "NR") {
    funcType = RIG_FUNC_NR;
  } else if (funcTypeStr == "AIP") {
    funcType = RIG_FUNC_AIP;
  } else if (funcTypeStr == "APF") {
    funcType = RIG_FUNC_APF;
  } else if (funcTypeStr == "TUNER") {
    funcType = RIG_FUNC_TUNER;
  } else if (funcTypeStr == "XIT") {
    funcType = RIG_FUNC_XIT;
  } else if (funcTypeStr == "RIT") {
    funcType = RIG_FUNC_RIT;
  } else if (funcTypeStr == "LOCK") {
    funcType = RIG_FUNC_LOCK;
  } else if (funcTypeStr == "MUTE") {
    funcType = RIG_FUNC_MUTE;
  } else if (funcTypeStr == "VSC") {
    funcType = RIG_FUNC_VSC;
  } else if (funcTypeStr == "REV") {
    funcType = RIG_FUNC_REV;
  } else if (funcTypeStr == "SQL") {
    funcType = RIG_FUNC_SQL;
  } else if (funcTypeStr == "ABM") {
    funcType = RIG_FUNC_ABM;
  } else if (funcTypeStr == "BC") {
    funcType = RIG_FUNC_BC;
  } else if (funcTypeStr == "MBC") {
    funcType = RIG_FUNC_MBC;
  } else if (funcTypeStr == "AFC") {
    funcType = RIG_FUNC_AFC;
  } else if (funcTypeStr == "SATMODE") {
    funcType = RIG_FUNC_SATMODE;
  } else if (funcTypeStr == "SCOPE") {
    funcType = RIG_FUNC_SCOPE;
  } else if (funcTypeStr == "RESUME") {
    funcType = RIG_FUNC_RESUME;
  } else if (funcTypeStr == "TBURST") {
    funcType = RIG_FUNC_TBURST;
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
  
  // Get capabilities from rig caps instead of state (doesn't require rig to be open)
  setting_t functions = my_rig->caps->has_get_func | my_rig->caps->has_set_func;
  Napi::Array funcArray = Napi::Array::New(env);
  uint32_t index = 0;
  
  // Check each function type
  if (functions & RIG_FUNC_FAGC) funcArray[index++] = Napi::String::New(env, "FAGC");
  if (functions & RIG_FUNC_NB) funcArray[index++] = Napi::String::New(env, "NB");
  if (functions & RIG_FUNC_COMP) funcArray[index++] = Napi::String::New(env, "COMP");
  if (functions & RIG_FUNC_VOX) funcArray[index++] = Napi::String::New(env, "VOX");
  if (functions & RIG_FUNC_TONE) funcArray[index++] = Napi::String::New(env, "TONE");
  if (functions & RIG_FUNC_TSQL) funcArray[index++] = Napi::String::New(env, "TSQL");
  if (functions & RIG_FUNC_SBKIN) funcArray[index++] = Napi::String::New(env, "SBKIN");
  if (functions & RIG_FUNC_FBKIN) funcArray[index++] = Napi::String::New(env, "FBKIN");
  if (functions & RIG_FUNC_ANF) funcArray[index++] = Napi::String::New(env, "ANF");
  if (functions & RIG_FUNC_NR) funcArray[index++] = Napi::String::New(env, "NR");
  if (functions & RIG_FUNC_AIP) funcArray[index++] = Napi::String::New(env, "AIP");
  if (functions & RIG_FUNC_APF) funcArray[index++] = Napi::String::New(env, "APF");
  if (functions & RIG_FUNC_TUNER) funcArray[index++] = Napi::String::New(env, "TUNER");
  if (functions & RIG_FUNC_XIT) funcArray[index++] = Napi::String::New(env, "XIT");
  if (functions & RIG_FUNC_RIT) funcArray[index++] = Napi::String::New(env, "RIT");
  if (functions & RIG_FUNC_LOCK) funcArray[index++] = Napi::String::New(env, "LOCK");
  if (functions & RIG_FUNC_MUTE) funcArray[index++] = Napi::String::New(env, "MUTE");
  if (functions & RIG_FUNC_VSC) funcArray[index++] = Napi::String::New(env, "VSC");
  if (functions & RIG_FUNC_REV) funcArray[index++] = Napi::String::New(env, "REV");
  if (functions & RIG_FUNC_SQL) funcArray[index++] = Napi::String::New(env, "SQL");
  if (functions & RIG_FUNC_ABM) funcArray[index++] = Napi::String::New(env, "ABM");
  if (functions & RIG_FUNC_BC) funcArray[index++] = Napi::String::New(env, "BC");
  if (functions & RIG_FUNC_MBC) funcArray[index++] = Napi::String::New(env, "MBC");
  if (functions & RIG_FUNC_AFC) funcArray[index++] = Napi::String::New(env, "AFC");
  if (functions & RIG_FUNC_SATMODE) funcArray[index++] = Napi::String::New(env, "SATMODE");
  if (functions & RIG_FUNC_SCOPE) funcArray[index++] = Napi::String::New(env, "SCOPE");
  if (functions & RIG_FUNC_RESUME) funcArray[index++] = Napi::String::New(env, "RESUME");
  if (functions & RIG_FUNC_TBURST) funcArray[index++] = Napi::String::New(env, "TBURST");
  
  return funcArray;
}

// Mode Query
Napi::Value NodeHamLib::GetSupportedModes(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();

  // Get mode list from rig state (populated during rig_open from rx/tx range lists)
  rmode_t modes = my_rig->state.mode_list;
  Napi::Array modeArray = Napi::Array::New(env);
  uint32_t index = 0;

  // Iterate through all possible mode bits (similar to rig_sprintf_mode)
  for (unsigned int i = 0; i < HAMLIB_MAX_MODES; i++) {
    rmode_t mode_bit = modes & (1ULL << i);

    if (mode_bit) {
      const char* mode_str = rig_strrmode(mode_bit);

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
  
  freq_t tx_freq = info[0].As<Napi::Number>().DoubleValue();
  
  // Basic frequency range validation
  if (tx_freq < 1000 || tx_freq > 10000000000) { // 1 kHz to 10 GHz reasonable range
    Napi::Error::New(env, "Frequency out of range (1 kHz - 10 GHz)").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  vfo_t vfo = RIG_VFO_CURR;
  
  if (info.Length() >= 2 && info[1].IsString()) {
    // setSplitFreq(freq, vfo)
    vfo = parseVfoParameter(info, 1, RIG_VFO_CURR);
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
  
  vfo_t vfo = RIG_VFO_CURR;
  
  if (info.Length() >= 1 && info[0].IsString()) {
    // getSplitFreq(vfo)
    vfo = parseVfoParameter(info, 0, RIG_VFO_CURR);
  }
  // Otherwise use default RIG_VFO_CURR for getSplitFreq()
  
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
  rmode_t tx_mode = rig_parse_mode(modeStr.c_str());
  
  if (tx_mode == RIG_MODE_NONE) {
    Napi::Error::New(env, "Invalid mode: " + modeStr).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  pbwidth_t tx_width = RIG_PASSBAND_NORMAL;
  vfo_t vfo = RIG_VFO_CURR;
  
  // Parse parameters: setSplitMode(mode) | setSplitMode(mode, vfo) | setSplitMode(mode, width) | setSplitMode(mode, width, vfo)
  if (info.Length() == 2) {
    if (info[1].IsString()) {
      // setSplitMode(mode, vfo)
      vfo = parseVfoParameter(info, 1, RIG_VFO_CURR);
    } else if (info[1].IsNumber()) {
      // setSplitMode(mode, width)
      tx_width = info[1].As<Napi::Number>().Int32Value();
    }
  } else if (info.Length() == 3 && info[1].IsNumber() && info[2].IsString()) {
    // setSplitMode(mode, width, vfo)
    tx_width = info[1].As<Napi::Number>().Int32Value();
    vfo = parseVfoParameter(info, 2, RIG_VFO_CURR);
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
  
  vfo_t vfo = RIG_VFO_CURR;
  
  if (info.Length() >= 1 && info[0].IsString()) {
    // getSplitMode(vfo)
    vfo = parseVfoParameter(info, 0, RIG_VFO_CURR);
  }
  // Otherwise use default RIG_VFO_CURR for getSplitMode()
  
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
  split_t split = split_enabled ? RIG_SPLIT_ON : RIG_SPLIT_OFF;
  
  // Default values
  vfo_t rx_vfo = RIG_VFO_CURR;
  vfo_t tx_vfo = RIG_VFO_B;  // Default TX VFO
  
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
    std::string vfoStr = info[1].As<Napi::String>().Utf8Value();
    if (vfoStr == "VFO-A") {
      rx_vfo = RIG_VFO_A;
    } else if (vfoStr == "VFO-B") {
      rx_vfo = RIG_VFO_B;
    }
  } else if (info.Length() == 3 && info[1].IsString() && info[2].IsString()) {
    // setSplit(enable, rxVfo, txVfo)
    // ⚠️  CRITICAL: Parameter assignment was WRONG before 2024-08-10 fix!
    // ⚠️  Previous bug: info[1] -> txVfoStr, info[2] -> rxVfoStr (WRONG!)
    // ⚠️  Correct now: info[1] -> rxVfoStr, info[2] -> txVfoStr (RIGHT!)
    std::string rxVfoStr = info[1].As<Napi::String>().Utf8Value();  // ✅ CORRECT: info[1] is rxVfo
    std::string txVfoStr = info[2].As<Napi::String>().Utf8Value();  // ✅ CORRECT: info[2] is txVfo
    
    if (rxVfoStr == "VFO-A") {
      rx_vfo = RIG_VFO_A;
    } else if (rxVfoStr == "VFO-B") {
      rx_vfo = RIG_VFO_B;
    }
    
    if (txVfoStr == "VFO-A") {
      tx_vfo = RIG_VFO_A;
    } else if (txVfoStr == "VFO-B") {
      tx_vfo = RIG_VFO_B;
    }
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
  
  vfo_t vfo = RIG_VFO_CURR;
  
  if (info.Length() >= 1 && info[0].IsString()) {
    // getSplit(vfo)
    vfo = parseVfoParameter(info, 0, RIG_VFO_CURR);
  }
  // Otherwise use default RIG_VFO_CURR for getSplit()
  
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
  
  vfo_op_t vfo_op;
  if (vfoOpStr == "CPY") {
    vfo_op = RIG_OP_CPY;
  } else if (vfoOpStr == "XCHG") {
    vfo_op = RIG_OP_XCHG;
  } else if (vfoOpStr == "FROM_VFO") {
    vfo_op = RIG_OP_FROM_VFO;
  } else if (vfoOpStr == "TO_VFO") {
    vfo_op = RIG_OP_TO_VFO;
  } else if (vfoOpStr == "MCL") {
    vfo_op = RIG_OP_MCL;
  } else if (vfoOpStr == "UP") {
    vfo_op = RIG_OP_UP;
  } else if (vfoOpStr == "DOWN") {
    vfo_op = RIG_OP_DOWN;
  } else if (vfoOpStr == "BAND_UP") {
    vfo_op = RIG_OP_BAND_UP;
  } else if (vfoOpStr == "BAND_DOWN") {
    vfo_op = RIG_OP_BAND_DOWN;
  } else if (vfoOpStr == "LEFT") {
    vfo_op = RIG_OP_LEFT;
  } else if (vfoOpStr == "RIGHT") {
    vfo_op = RIG_OP_RIGHT;
  } else if (vfoOpStr == "TUNE") {
    vfo_op = RIG_OP_TUNE;
  } else if (vfoOpStr == "TOGGLE") {
    vfo_op = RIG_OP_TOGGLE;
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
  
  ant_t antenna = info[0].As<Napi::Number>().Int32Value();
  
  // Support optional VFO parameter: setAntenna(antenna) or setAntenna(antenna, vfo)
  vfo_t vfo = RIG_VFO_CURR;
  if (info.Length() >= 2 && info[1].IsString()) {
    vfo = parseVfoParameter(info, 1, RIG_VFO_CURR);
  }
  
  // Default option value (can be extended later if needed)
  value_t option = {0};
  
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
  vfo_t vfo = RIG_VFO_CURR;
  if (info.Length() >= 1 && info[0].IsString()) {
    vfo = parseVfoParameter(info, 0, RIG_VFO_CURR);
  }
  
  // Default antenna query (RIG_ANT_CURR gets all antenna info)
  ant_t antenna = RIG_ANT_CURR;
  
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

      NodeHamLib::InstanceMethod("close", & NodeHamLib::Close),
      NodeHamLib::InstanceMethod("destroy", & NodeHamLib::Destroy),
      NodeHamLib::InstanceMethod("getConnectionInfo", & NodeHamLib::GetConnectionInfo),

      // Static methods
      NodeHamLib::StaticMethod("getSupportedRigs", & NodeHamLib::GetSupportedRigs),
      NodeHamLib::StaticMethod("getHamlibVersion", & NodeHamLib::GetHamlibVersion),
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
int NodeHamLib::rig_list_callback(const struct rig_caps *caps, void *data) {
  RigListData *rig_data = static_cast<RigListData*>(data);
  
  // Create rig info object
  Napi::Object rigInfo = Napi::Object::New(rig_data->env);
  rigInfo.Set(Napi::String::New(rig_data->env, "rigModel"), 
              Napi::Number::New(rig_data->env, caps->rig_model));
  rigInfo.Set(Napi::String::New(rig_data->env, "modelName"), 
              Napi::String::New(rig_data->env, caps->model_name ? caps->model_name : ""));
  rigInfo.Set(Napi::String::New(rig_data->env, "mfgName"), 
              Napi::String::New(rig_data->env, caps->mfg_name ? caps->mfg_name : ""));
  rigInfo.Set(Napi::String::New(rig_data->env, "version"), 
              Napi::String::New(rig_data->env, caps->version ? caps->version : ""));
  rigInfo.Set(Napi::String::New(rig_data->env, "status"), 
              Napi::String::New(rig_data->env, rig_strstatus(caps->status)));
  
  // Determine rig type string
  const char* rigType = "Unknown";
  switch (caps->rig_type & RIG_TYPE_MASK) {
    case RIG_TYPE_TRANSCEIVER:
      rigType = "Transceiver";
      break;
    case RIG_TYPE_HANDHELD:
      rigType = "Handheld";
      break;
    case RIG_TYPE_MOBILE:
      rigType = "Mobile";
      break;
    case RIG_TYPE_RECEIVER:
      rigType = "Receiver";
      break;
    case RIG_TYPE_PCRECEIVER:
      rigType = "PC Receiver";
      break;
    case RIG_TYPE_SCANNER:
      rigType = "Scanner";
      break;
    case RIG_TYPE_TRUNKSCANNER:
      rigType = "Trunk Scanner";
      break;
    case RIG_TYPE_COMPUTER:
      rigType = "Computer";
      break;
    case RIG_TYPE_OTHER:
      rigType = "Other";
      break;
  }
  
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
  rig_load_all_backends();
  
  // Prepare data structure for callback with proper initialization
  RigListData rigData{std::vector<Napi::Object>(), env};
  
  // Call hamlib function to iterate through all supported rigs
  int result = rig_list_foreach(rig_list_callback, &rigData);
  
  if (result != RIG_OK) {
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

  // Reference to external hamlib_version2 variable
  extern const char* hamlib_version2;

  return Napi::String::New(env, hamlib_version2);
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
  configs.Set("ptt_type", pttTypeOptions);
  
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
  configs.Set("dcd_type", dcdTypeOptions);
  
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
  powerstat_t status = static_cast<powerstat_t>(powerStatus);
  
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
  
  vfo_t vfo = parseVfoParameter(info, 0, RIG_VFO_CURR);
  
  GetPttAsyncWorker* asyncWorker = new GetPttAsyncWorker(env, this, vfo);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// Data Carrier Detect
Napi::Value NodeHamLib::GetDcd(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  vfo_t vfo = parseVfoParameter(info, 0, RIG_VFO_CURR);
  
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
  
  shortfreq_t ts = info[0].As<Napi::Number>().Int32Value();
  vfo_t vfo = parseVfoParameter(info, 1, RIG_VFO_CURR);
  
  SetTuningStepAsyncWorker* asyncWorker = new SetTuningStepAsyncWorker(env, this, vfo, ts);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::GetTuningStep(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  vfo_t vfo = parseVfoParameter(info, 0, RIG_VFO_CURR);
  
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
  rptr_shift_t shift = RIG_RPT_SHIFT_NONE;
  
  if (shiftStr == "NONE" || shiftStr == "none") {
    shift = RIG_RPT_SHIFT_NONE;
  } else if (shiftStr == "MINUS" || shiftStr == "minus" || shiftStr == "-") {
    shift = RIG_RPT_SHIFT_MINUS;
  } else if (shiftStr == "PLUS" || shiftStr == "plus" || shiftStr == "+") {
    shift = RIG_RPT_SHIFT_PLUS;
  } else {
    Napi::TypeError::New(env, "Invalid repeater shift (must be 'NONE', 'MINUS', or 'PLUS')").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  vfo_t vfo = parseVfoParameter(info, 1, RIG_VFO_CURR);
  
  SetRepeaterShiftAsyncWorker* asyncWorker = new SetRepeaterShiftAsyncWorker(env, this, vfo, shift);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::GetRepeaterShift(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  vfo_t vfo = parseVfoParameter(info, 0, RIG_VFO_CURR);
  
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
  
  shortfreq_t offset = info[0].As<Napi::Number>().Int32Value();
  vfo_t vfo = parseVfoParameter(info, 1, RIG_VFO_CURR);
  
  SetRepeaterOffsetAsyncWorker* asyncWorker = new SetRepeaterOffsetAsyncWorker(env, this, vfo, offset);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::GetRepeaterOffset(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  vfo_t vfo = parseVfoParameter(info, 0, RIG_VFO_CURR);
  
  GetRepeaterOffsetAsyncWorker* asyncWorker = new GetRepeaterOffsetAsyncWorker(env, this, vfo);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// CTCSS/DCS Tone Control AsyncWorker classes
class SetCtcssToneAsyncWorker : public HamLibAsyncWorker {
public:
    SetCtcssToneAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo, tone_t tone)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), tone_(tone) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_set_ctcss_tone(hamlib_instance_->my_rig, vfo_, tone_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    vfo_t vfo_;
    tone_t tone_;
};

class GetCtcssToneAsyncWorker : public HamLibAsyncWorker {
public:
    GetCtcssToneAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), tone_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_get_ctcss_tone(hamlib_instance_->my_rig, vfo_, &tone_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    vfo_t vfo_;
    tone_t tone_;
};

class SetDcsCodeAsyncWorker : public HamLibAsyncWorker {
public:
    SetDcsCodeAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo, tone_t code)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), code_(code) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_set_dcs_code(hamlib_instance_->my_rig, vfo_, code_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    vfo_t vfo_;
    tone_t code_;
};

class GetDcsCodeAsyncWorker : public HamLibAsyncWorker {
public:
    GetDcsCodeAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), code_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_get_dcs_code(hamlib_instance_->my_rig, vfo_, &code_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    vfo_t vfo_;
    tone_t code_;
};

class SetCtcssSqlAsyncWorker : public HamLibAsyncWorker {
public:
    SetCtcssSqlAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo, tone_t tone)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), tone_(tone) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_set_ctcss_sql(hamlib_instance_->my_rig, vfo_, tone_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    vfo_t vfo_;
    tone_t tone_;
};

class GetCtcssSqlAsyncWorker : public HamLibAsyncWorker {
public:
    GetCtcssSqlAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), tone_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_get_ctcss_sql(hamlib_instance_->my_rig, vfo_, &tone_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    vfo_t vfo_;
    tone_t tone_;
};

class SetDcsSqlAsyncWorker : public HamLibAsyncWorker {
public:
    SetDcsSqlAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo, tone_t code)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), code_(code) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_set_dcs_sql(hamlib_instance_->my_rig, vfo_, code_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    vfo_t vfo_;
    tone_t code_;
};

class GetDcsSqlAsyncWorker : public HamLibAsyncWorker {
public:
    GetDcsSqlAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), code_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_get_dcs_sql(hamlib_instance_->my_rig, vfo_, &code_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    vfo_t vfo_;
    tone_t code_;
};

// Parameter Control AsyncWorker classes
class SetParmAsyncWorker : public HamLibAsyncWorker {
public:
    SetParmAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, setting_t parm, value_t value)
        : HamLibAsyncWorker(env, hamlib_instance), parm_(parm), value_(value) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_set_parm(hamlib_instance_->my_rig, parm_, value_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    setting_t parm_;
    value_t value_;
};

class GetParmAsyncWorker : public HamLibAsyncWorker {
public:
    GetParmAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, setting_t parm)
        : HamLibAsyncWorker(env, hamlib_instance), parm_(parm) {
        value_.f = 0.0;
    }
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_get_parm(hamlib_instance_->my_rig, parm_, &value_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            deferred_.Resolve(Napi::Number::New(env, value_.f));
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    setting_t parm_;
    value_t value_;
};

// DTMF Support AsyncWorker classes
class SendDtmfAsyncWorker : public HamLibAsyncWorker {
public:
    SendDtmfAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo, const std::string& digits)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), digits_(digits) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_send_dtmf(hamlib_instance_->my_rig, vfo_, digits_.c_str());
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    vfo_t vfo_;
    std::string digits_;
};

class RecvDtmfAsyncWorker : public HamLibAsyncWorker {
public:
    RecvDtmfAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo, int maxLength)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), max_length_(maxLength), length_(0) {
        digits_.resize(maxLength + 1, '\0');
    }
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        length_ = max_length_;
        // rig_recv_dtmf expects a mutable buffer (char*). Ensure non-const pointer.
        result_code_ = rig_recv_dtmf(hamlib_instance_->my_rig, vfo_, &digits_[0], &length_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    vfo_t vfo_;
    int max_length_;
    int length_;
    std::string digits_;
};

// Memory Channel Advanced Operations AsyncWorker classes
class GetMemAsyncWorker : public HamLibAsyncWorker {
public:
    GetMemAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), ch_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_get_mem(hamlib_instance_->my_rig, vfo_, &ch_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    vfo_t vfo_;
    int ch_;
};

class SetBankAsyncWorker : public HamLibAsyncWorker {
public:
    SetBankAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo, int bank)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), bank_(bank) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_set_bank(hamlib_instance_->my_rig, vfo_, bank_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    vfo_t vfo_;
    int bank_;
};

class MemCountAsyncWorker : public HamLibAsyncWorker {
public:
    MemCountAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance)
        : HamLibAsyncWorker(env, hamlib_instance), count_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        count_ = rig_mem_count(hamlib_instance_->my_rig);
        if (count_ < 0) {
            result_code_ = count_;
            error_message_ = rigerror(result_code_);
        } else {
            result_code_ = RIG_OK;
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    SendMorseAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo, const std::string& msg)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), msg_(msg) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_send_morse(hamlib_instance_->my_rig, vfo_, msg_.c_str());
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    vfo_t vfo_;
    std::string msg_;
};

class StopMorseAsyncWorker : public HamLibAsyncWorker {
public:
    StopMorseAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_stop_morse(hamlib_instance_->my_rig, vfo_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    vfo_t vfo_;
};

class WaitMorseAsyncWorker : public HamLibAsyncWorker {
public:
    WaitMorseAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_wait_morse(hamlib_instance_->my_rig, vfo_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    vfo_t vfo_;
};

// Voice Memory Support AsyncWorker classes
class SendVoiceMemAsyncWorker : public HamLibAsyncWorker {
public:
    SendVoiceMemAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo, int ch)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), ch_(ch) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_send_voice_mem(hamlib_instance_->my_rig, vfo_, ch_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    vfo_t vfo_;
    int ch_;
};

class StopVoiceMemAsyncWorker : public HamLibAsyncWorker {
public:
    StopVoiceMemAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        #if HAVE_RIG_STOP_VOICE_MEM
        result_code_ = rig_stop_voice_mem(hamlib_instance_->my_rig, vfo_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
        #else
        // rig_stop_voice_mem function is not available in this hamlib version
        // Return not implemented for compatibility with older hamlib versions
        result_code_ = -RIG_ENIMPL;
        error_message_ = "rig_stop_voice_mem not available in this hamlib version";
        #endif
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    vfo_t vfo_;
};

// Complex Split Frequency/Mode Operations AsyncWorker classes
class SetSplitFreqModeAsyncWorker : public HamLibAsyncWorker {
public:
    SetSplitFreqModeAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo, 
                               freq_t tx_freq, rmode_t tx_mode, pbwidth_t tx_width)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), tx_freq_(tx_freq), 
          tx_mode_(tx_mode), tx_width_(tx_width) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        #if HAVE_RIG_SPLIT_FREQ_MODE
        result_code_ = rig_set_split_freq_mode(hamlib_instance_->my_rig, vfo_, tx_freq_, tx_mode_, tx_width_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
        #else
        // rig_set_split_freq_mode function is not available in this hamlib version
        // Fall back to using separate calls
        result_code_ = -RIG_ENIMPL;
        error_message_ = "rig_set_split_freq_mode not available - use setSplitFreq and setSplitMode separately";
        #endif
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    vfo_t vfo_;
    freq_t tx_freq_;
    rmode_t tx_mode_;
    pbwidth_t tx_width_;
};

class GetSplitFreqModeAsyncWorker : public HamLibAsyncWorker {
public:
    GetSplitFreqModeAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, vfo_t vfo)
        : HamLibAsyncWorker(env, hamlib_instance), vfo_(vfo), tx_freq_(0), tx_mode_(RIG_MODE_NONE), tx_width_(0) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        #if HAVE_RIG_SPLIT_FREQ_MODE
        result_code_ = rig_get_split_freq_mode(hamlib_instance_->my_rig, vfo_, &tx_freq_, &tx_mode_, &tx_width_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
        #else
        // rig_get_split_freq_mode function is not available in this hamlib version
        // Fall back to using separate calls
        result_code_ = -RIG_ENIMPL;
        error_message_ = "rig_get_split_freq_mode not available - use getSplitFreq and getSplitMode separately";
        #endif
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
            deferred_.Reject(Napi::Error::New(env, error_message_).Value());
        } else {
            Napi::Object obj = Napi::Object::New(env);
            obj.Set(Napi::String::New(env, "txFrequency"), Napi::Number::New(env, static_cast<double>(tx_freq_)));
            obj.Set(Napi::String::New(env, "txMode"), Napi::String::New(env, rig_strrmode(tx_mode_)));
            obj.Set(Napi::String::New(env, "txWidth"), Napi::Number::New(env, static_cast<double>(tx_width_)));
            deferred_.Resolve(obj);
        }
    }
    
    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        deferred_.Reject(Napi::Error::New(env, error_message_).Value());
    }
    
private:
    vfo_t vfo_;
    freq_t tx_freq_;
    rmode_t tx_mode_;
    pbwidth_t tx_width_;
};

// Reset Function AsyncWorker class
class ResetAsyncWorker : public HamLibAsyncWorker {
public:
    ResetAsyncWorker(Napi::Env env, NodeHamLib* hamlib_instance, reset_t reset)
        : HamLibAsyncWorker(env, hamlib_instance), reset_(reset) {}
    
    void Execute() override {
        CHECK_RIG_VALID();
        
        result_code_ = rig_reset(hamlib_instance_->my_rig, reset_);
        if (result_code_ != RIG_OK) {
            error_message_ = rigerror(result_code_);
        }
    }
    
    void OnOK() override {
        Napi::Env env = Env();
        if (result_code_ != RIG_OK && !error_message_.empty()) {
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
    reset_t reset_;
};

// CTCSS/DCS Tone Control Methods Implementation
Napi::Value NodeHamLib::SetCtcssTone(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() < 1 || !info[0].IsNumber()) {
    Napi::TypeError::New(env, "Expected tone frequency as number").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  tone_t tone = static_cast<tone_t>(info[0].As<Napi::Number>().Uint32Value());
  vfo_t vfo = parseVfoParameter(info, 1, RIG_VFO_CURR);
  
  SetCtcssToneAsyncWorker* asyncWorker = new SetCtcssToneAsyncWorker(env, this, vfo, tone);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::GetCtcssTone(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  vfo_t vfo = parseVfoParameter(info, 0, RIG_VFO_CURR);
  
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
  
  tone_t code = static_cast<tone_t>(info[0].As<Napi::Number>().Uint32Value());
  vfo_t vfo = parseVfoParameter(info, 1, RIG_VFO_CURR);
  
  SetDcsCodeAsyncWorker* asyncWorker = new SetDcsCodeAsyncWorker(env, this, vfo, code);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::GetDcsCode(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  vfo_t vfo = parseVfoParameter(info, 0, RIG_VFO_CURR);
  
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
  
  tone_t tone = static_cast<tone_t>(info[0].As<Napi::Number>().Uint32Value());
  vfo_t vfo = parseVfoParameter(info, 1, RIG_VFO_CURR);
  
  SetCtcssSqlAsyncWorker* asyncWorker = new SetCtcssSqlAsyncWorker(env, this, vfo, tone);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::GetCtcssSql(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  vfo_t vfo = parseVfoParameter(info, 0, RIG_VFO_CURR);
  
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
  
  tone_t code = static_cast<tone_t>(info[0].As<Napi::Number>().Uint32Value());
  vfo_t vfo = parseVfoParameter(info, 1, RIG_VFO_CURR);
  
  SetDcsSqlAsyncWorker* asyncWorker = new SetDcsSqlAsyncWorker(env, this, vfo, code);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::GetDcsSql(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  vfo_t vfo = parseVfoParameter(info, 0, RIG_VFO_CURR);
  
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
  setting_t parm = rig_parse_parm(parm_str.c_str());
  
  if (parm == 0) {
    Napi::Error::New(env, "Invalid parameter name: " + parm_str).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  value_t value;
  value.f = info[1].As<Napi::Number>().FloatValue();
  
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
  setting_t parm = rig_parse_parm(parm_str.c_str());
  
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
  vfo_t vfo = parseVfoParameter(info, 1, RIG_VFO_CURR);
  
  SendDtmfAsyncWorker* asyncWorker = new SendDtmfAsyncWorker(env, this, vfo, digits);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::RecvDtmf(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  int maxLength = 32; // Default max length
  if (info.Length() > 0 && info[0].IsNumber()) {
    maxLength = info[0].As<Napi::Number>().Int32Value();
  }
  
  vfo_t vfo = parseVfoParameter(info, 1, RIG_VFO_CURR);
  
  RecvDtmfAsyncWorker* asyncWorker = new RecvDtmfAsyncWorker(env, this, vfo, maxLength);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

// Memory Channel Advanced Operations Methods Implementation
Napi::Value NodeHamLib::GetMem(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  vfo_t vfo = parseVfoParameter(info, 0, RIG_VFO_CURR);
  
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
  vfo_t vfo = parseVfoParameter(info, 1, RIG_VFO_CURR);
  
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
  vfo_t vfo = parseVfoParameter(info, 1, RIG_VFO_CURR);
  
  SendMorseAsyncWorker* asyncWorker = new SendMorseAsyncWorker(env, this, vfo, msg);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::StopMorse(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  vfo_t vfo = parseVfoParameter(info, 0, RIG_VFO_CURR);
  
  StopMorseAsyncWorker* asyncWorker = new StopMorseAsyncWorker(env, this, vfo);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::WaitMorse(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  vfo_t vfo = parseVfoParameter(info, 0, RIG_VFO_CURR);
  
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
  vfo_t vfo = parseVfoParameter(info, 1, RIG_VFO_CURR);
  
  SendVoiceMemAsyncWorker* asyncWorker = new SendVoiceMemAsyncWorker(env, this, vfo, ch);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::StopVoiceMem(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  vfo_t vfo = parseVfoParameter(info, 0, RIG_VFO_CURR);
  
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
  
  freq_t tx_freq = static_cast<freq_t>(info[0].As<Napi::Number>().DoubleValue());
  
  // Basic frequency range validation
  if (tx_freq < 1000 || tx_freq > 10000000000) { // 1 kHz to 10 GHz reasonable range
    Napi::Error::New(env, "Frequency out of range (1 kHz - 10 GHz)").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  std::string mode_str = info[1].As<Napi::String>().Utf8Value();
  pbwidth_t tx_width = static_cast<pbwidth_t>(info[2].As<Napi::Number>().DoubleValue());
  vfo_t vfo = parseVfoParameter(info, 3, RIG_VFO_CURR);
  
  rmode_t tx_mode = rig_parse_mode(mode_str.c_str());
  if (tx_mode == RIG_MODE_NONE) {
    Napi::Error::New(env, "Invalid mode: " + mode_str).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  SetSplitFreqModeAsyncWorker* asyncWorker = new SetSplitFreqModeAsyncWorker(env, this, vfo, tx_freq, tx_mode, tx_width);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}

Napi::Value NodeHamLib::GetSplitFreqMode(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  vfo_t vfo = parseVfoParameter(info, 0, RIG_VFO_CURR);
  
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
  freq_t freq = info[1].As<Napi::Number>().DoubleValue();
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
  rmode_t mode = rig_parse_mode(mode_str.c_str());
  if (mode == RIG_MODE_NONE) {
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
  freq_t freq = info[1].As<Napi::Number>().DoubleValue();
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
  rmode_t mode = rig_parse_mode(mode_str.c_str());
  if (mode == RIG_MODE_NONE) {
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
  
  reset_t reset = RIG_RESET_SOFT; // Default to soft reset
  
  if (info.Length() > 0 && info[0].IsString()) {
    std::string reset_str = info[0].As<Napi::String>().Utf8Value();
    if (reset_str == "NONE") {
      reset = RIG_RESET_NONE;
    } else if (reset_str == "SOFT") {
      reset = RIG_RESET_SOFT;
    } else if (reset_str == "MCALL") {
      reset = RIG_RESET_MCALL;
    } else if (reset_str == "MASTER") {
      reset = RIG_RESET_MASTER;
    } else if (reset_str == "VFO") {
      reset = RIG_RESET_VFO;
    } else {
      Napi::Error::New(env, "Invalid reset type: " + reset_str + 
                      " (valid: NONE, SOFT, VFO, MCALL, MASTER)").ThrowAsJavaScriptException();
      return env.Null();
    }
  }
  
  ResetAsyncWorker* asyncWorker = new ResetAsyncWorker(env, this, reset);
  asyncWorker->Queue();
  return asyncWorker->GetPromise();
}
