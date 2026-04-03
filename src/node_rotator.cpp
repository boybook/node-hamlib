#include "node_rotator.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#define CHECK_ROT_VALID() \
  do { \
    if (!rotator_instance_ || !rotator_instance_->my_rot) { \
      result_code_ = SHIM_RIG_EINVAL; \
      error_message_ = "Rotator is not initialized or has been destroyed"; \
      return; \
    } \
  } while (0)

#define RETURN_NULL_IF_ROT_HANDLE_INVALID() \
  do { \
    if (!my_rot) { \
      Napi::Error::New(env, "Rotator is not initialized or has been destroyed").ThrowAsJavaScriptException(); \
      return env.Null(); \
    } \
  } while (0)

namespace {

struct RotatorListData {
  std::vector<Napi::Object> rotators;
  Napi::Env env;
};

struct RotatorConfigFieldDescriptor {
  int token;
  std::string name;
  std::string label;
  std::string tooltip;
  std::string defaultValue;
  int type;
  double numericMin;
  double numericMax;
  double numericStep;
  std::vector<std::string> options;
};

struct RotatorConfigSchemaData {
  std::vector<RotatorConfigFieldDescriptor> fields;
};

const char* publicConfTypeName(int type) {
  switch (type) {
    case 0:
      return "string";
    case 1:
      return "combo";
    case 2:
      return "numeric";
    case 3:
      return "checkbutton";
    case 4:
      return "button";
    case 5:
      return "binary";
    case 6:
      return "int";
    default:
      return "unknown";
  }
}

bool hasPositiveValue(int value) {
  return value > 0;
}

Napi::Array buildStatusArray(Napi::Env env, int mask) {
  static const int kStatuses[] = {
    SHIM_ROT_STATUS_BUSY,
    SHIM_ROT_STATUS_MOVING,
    SHIM_ROT_STATUS_MOVING_AZ,
    SHIM_ROT_STATUS_MOVING_LEFT,
    SHIM_ROT_STATUS_MOVING_RIGHT,
    SHIM_ROT_STATUS_MOVING_EL,
    SHIM_ROT_STATUS_MOVING_UP,
    SHIM_ROT_STATUS_MOVING_DOWN,
    SHIM_ROT_STATUS_LIMIT_UP,
    SHIM_ROT_STATUS_LIMIT_DOWN,
    SHIM_ROT_STATUS_LIMIT_LEFT,
    SHIM_ROT_STATUS_LIMIT_RIGHT,
    SHIM_ROT_STATUS_OVERLAP_UP,
    SHIM_ROT_STATUS_OVERLAP_DOWN,
    SHIM_ROT_STATUS_OVERLAP_LEFT,
    SHIM_ROT_STATUS_OVERLAP_RIGHT,
  };

  Napi::Array result = Napi::Array::New(env);
  uint32_t index = 0;
  for (int status : kStatuses) {
    if ((mask & status) == status) {
      const char* name = shim_rot_strstatus(status);
      if (name && name[0] != '\0') {
        result[index++] = Napi::String::New(env, name);
      }
    }
  }
  return result;
}

Napi::Array buildSettingArrayFromMask(
    Napi::Env env,
    uint64_t mask,
    const char* (*name_fn)(uint64_t)) {
  Napi::Array result = Napi::Array::New(env);
  uint32_t index = 0;

  for (uint64_t bit = 1; bit != 0; bit <<= 1) {
    if ((mask & bit) == 0) {
      continue;
    }

    const char* name = name_fn(bit);
    if (name && name[0] != '\0') {
      result[index++] = Napi::String::New(env, name);
    }
  }

  return result;
}

int parseRotDirection(Napi::Env env, const Napi::Value& value) {
  if (value.IsNumber()) {
    return value.As<Napi::Number>().Int32Value();
  }
  if (!value.IsString()) {
    Napi::TypeError::New(env, "Expected direction as string or number").ThrowAsJavaScriptException();
    return std::numeric_limits<int>::min();
  }

  const std::string direction = value.As<Napi::String>().Utf8Value();
  if (direction == "UP") return SHIM_ROT_MOVE_UP;
  if (direction == "DOWN") return SHIM_ROT_MOVE_DOWN;
  if (direction == "LEFT" || direction == "CCW") return SHIM_ROT_MOVE_LEFT;
  if (direction == "RIGHT" || direction == "CW") return SHIM_ROT_MOVE_RIGHT;
  if (direction == "UP_LEFT" || direction == "UP_CCW") return SHIM_ROT_MOVE_UP_LEFT;
  if (direction == "UP_RIGHT" || direction == "UP_CW") return SHIM_ROT_MOVE_UP_RIGHT;
  if (direction == "DOWN_LEFT" || direction == "DOWN_CCW") return SHIM_ROT_MOVE_DOWN_LEFT;
  if (direction == "DOWN_RIGHT" || direction == "DOWN_CW") return SHIM_ROT_MOVE_DOWN_RIGHT;

  Napi::TypeError::New(env, "Invalid rotator direction").ThrowAsJavaScriptException();
  return std::numeric_limits<int>::min();
}

int parseRotReset(Napi::Env env, const Napi::Value& value) {
  if (value.IsNumber()) {
    return value.As<Napi::Number>().Int32Value();
  }
  if (!value.IsString()) {
    Napi::TypeError::New(env, "Expected reset type as string or number").ThrowAsJavaScriptException();
    return std::numeric_limits<int>::min();
  }

  const std::string reset = value.As<Napi::String>().Utf8Value();
  if (reset == "ALL") return SHIM_ROT_RESET_ALL;

  Napi::TypeError::New(env, "Invalid rotator reset type").ThrowAsJavaScriptException();
  return std::numeric_limits<int>::min();
}

uint64_t parseRotLevel(Napi::Env env, const Napi::Value& value) {
  if (!value.IsString()) {
    Napi::TypeError::New(env, "Expected level name as string").ThrowAsJavaScriptException();
    return 0;
  }
  const std::string name = value.As<Napi::String>().Utf8Value();
  const uint64_t level = shim_rot_parse_level(name.c_str());
  if (level == 0) {
    Napi::Error::New(env, "Invalid rotator level: " + name).ThrowAsJavaScriptException();
  }
  return level;
}

uint64_t parseRotFunc(Napi::Env env, const Napi::Value& value) {
  if (!value.IsString()) {
    Napi::TypeError::New(env, "Expected function name as string").ThrowAsJavaScriptException();
    return 0;
  }
  const std::string name = value.As<Napi::String>().Utf8Value();
  const uint64_t func = shim_rot_parse_func(name.c_str());
  if (func == 0) {
    Napi::Error::New(env, "Invalid rotator function: " + name).ThrowAsJavaScriptException();
  }
  return func;
}

uint64_t parseRotParm(Napi::Env env, const Napi::Value& value) {
  if (!value.IsString()) {
    Napi::TypeError::New(env, "Expected parameter name as string").ThrowAsJavaScriptException();
    return 0;
  }
  const std::string name = value.As<Napi::String>().Utf8Value();
  const uint64_t parm = shim_rot_parse_parm(name.c_str());
  if (parm == 0) {
    Napi::Error::New(env, "Invalid rotator parameter: " + name).ThrowAsJavaScriptException();
  }
  return parm;
}

}  // namespace

class RotatorAsyncWorker : public Napi::AsyncWorker {
 public:
  RotatorAsyncWorker(Napi::Env env, NodeRotator* rotator_instance)
      : Napi::AsyncWorker(env),
        rotator_instance_(rotator_instance),
        result_code_(0),
        deferred_(Napi::Promise::Deferred::New(env)) {}

  Napi::Promise GetPromise() { return deferred_.Promise(); }

 protected:
  NodeRotator* rotator_instance_;
  int result_code_;
  std::string error_message_;
  Napi::Promise::Deferred deferred_;
};

Napi::FunctionReference NodeRotator::constructor;

class RotatorOpenAsyncWorker : public RotatorAsyncWorker {
 public:
  RotatorOpenAsyncWorker(Napi::Env env, NodeRotator* rotator_instance)
      : RotatorAsyncWorker(env, rotator_instance) {}

  void Execute() override {
    CHECK_ROT_VALID();
    result_code_ = shim_rot_open(rotator_instance_->my_rot);
    if (result_code_ != SHIM_RIG_OK) {
      error_message_ = shim_rigerror(result_code_);
    } else {
      rotator_instance_->rot_is_open = true;
    }
  }

  void OnOK() override {
    if (!error_message_.empty()) {
      deferred_.Reject(Napi::Error::New(Env(), error_message_).Value());
      return;
    }
    deferred_.Resolve(Napi::Number::New(Env(), result_code_));
  }
};

class RotatorCloseAsyncWorker : public RotatorAsyncWorker {
 public:
  RotatorCloseAsyncWorker(Napi::Env env, NodeRotator* rotator_instance)
      : RotatorAsyncWorker(env, rotator_instance) {}

  void Execute() override {
    CHECK_ROT_VALID();
    result_code_ = shim_rot_close(rotator_instance_->my_rot);
    if (result_code_ != SHIM_RIG_OK) {
      error_message_ = shim_rigerror(result_code_);
    } else {
      rotator_instance_->rot_is_open = false;
    }
  }

  void OnOK() override {
    if (!error_message_.empty()) {
      deferred_.Reject(Napi::Error::New(Env(), error_message_).Value());
      return;
    }
    deferred_.Resolve(Napi::Number::New(Env(), result_code_));
  }
};

class RotatorDestroyAsyncWorker : public RotatorAsyncWorker {
 public:
  RotatorDestroyAsyncWorker(Napi::Env env, NodeRotator* rotator_instance)
      : RotatorAsyncWorker(env, rotator_instance) {}

  void Execute() override {
    CHECK_ROT_VALID();
    if (rotator_instance_->rot_is_open) {
      shim_rot_close(rotator_instance_->my_rot);
      rotator_instance_->rot_is_open = false;
    }
    result_code_ = shim_rot_cleanup(rotator_instance_->my_rot);
    if (result_code_ != SHIM_RIG_OK) {
      error_message_ = shim_rigerror(result_code_);
    } else {
      rotator_instance_->my_rot = nullptr;
    }
  }

  void OnOK() override {
    if (!error_message_.empty()) {
      deferred_.Reject(Napi::Error::New(Env(), error_message_).Value());
      return;
    }
    deferred_.Resolve(Napi::Number::New(Env(), result_code_));
  }
};

class RotatorSetPositionAsyncWorker : public RotatorAsyncWorker {
 public:
  RotatorSetPositionAsyncWorker(Napi::Env env, NodeRotator* rotator_instance, double azimuth, double elevation)
      : RotatorAsyncWorker(env, rotator_instance), azimuth_(azimuth), elevation_(elevation) {}

  void Execute() override {
    CHECK_ROT_VALID();
    result_code_ = shim_rot_set_position(rotator_instance_->my_rot, azimuth_, elevation_);
    if (result_code_ != SHIM_RIG_OK) {
      error_message_ = shim_rigerror(result_code_);
    }
  }

  void OnOK() override {
    if (!error_message_.empty()) {
      deferred_.Reject(Napi::Error::New(Env(), error_message_).Value());
      return;
    }
    deferred_.Resolve(Napi::Number::New(Env(), result_code_));
  }

 private:
  double azimuth_;
  double elevation_;
};

class RotatorGetPositionAsyncWorker : public RotatorAsyncWorker {
 public:
  RotatorGetPositionAsyncWorker(Napi::Env env, NodeRotator* rotator_instance)
      : RotatorAsyncWorker(env, rotator_instance), azimuth_(0), elevation_(0) {}

  void Execute() override {
    CHECK_ROT_VALID();
    result_code_ = shim_rot_get_position(rotator_instance_->my_rot, &azimuth_, &elevation_);
    if (result_code_ != SHIM_RIG_OK) {
      error_message_ = shim_rigerror(result_code_);
    }
  }

  void OnOK() override {
    if (!error_message_.empty()) {
      deferred_.Reject(Napi::Error::New(Env(), error_message_).Value());
      return;
    }
    Napi::Object result = Napi::Object::New(Env());
    result.Set("azimuth", Napi::Number::New(Env(), azimuth_));
    result.Set("elevation", Napi::Number::New(Env(), elevation_));
    deferred_.Resolve(result);
  }

 private:
  double azimuth_;
  double elevation_;
};

class RotatorMoveAsyncWorker : public RotatorAsyncWorker {
 public:
  RotatorMoveAsyncWorker(Napi::Env env, NodeRotator* rotator_instance, int direction, int speed)
      : RotatorAsyncWorker(env, rotator_instance), direction_(direction), speed_(speed) {}

  void Execute() override {
    CHECK_ROT_VALID();
    result_code_ = shim_rot_move(rotator_instance_->my_rot, direction_, speed_);
    if (result_code_ != SHIM_RIG_OK) {
      error_message_ = shim_rigerror(result_code_);
    }
  }

  void OnOK() override {
    if (!error_message_.empty()) {
      deferred_.Reject(Napi::Error::New(Env(), error_message_).Value());
      return;
    }
    deferred_.Resolve(Napi::Number::New(Env(), result_code_));
  }

 private:
  int direction_;
  int speed_;
};

class RotatorSimpleAsyncWorker : public RotatorAsyncWorker {
 public:
  typedef int (*OpFn)(hamlib_shim_handle_t);

  RotatorSimpleAsyncWorker(Napi::Env env, NodeRotator* rotator_instance, OpFn fn)
      : RotatorAsyncWorker(env, rotator_instance), fn_(fn) {}

  void Execute() override {
    CHECK_ROT_VALID();
    result_code_ = fn_(rotator_instance_->my_rot);
    if (result_code_ != SHIM_RIG_OK) {
      error_message_ = shim_rigerror(result_code_);
    }
  }

  void OnOK() override {
    if (!error_message_.empty()) {
      deferred_.Reject(Napi::Error::New(Env(), error_message_).Value());
      return;
    }
    deferred_.Resolve(Napi::Number::New(Env(), result_code_));
  }

 private:
  OpFn fn_;
};

class RotatorResetAsyncWorker : public RotatorAsyncWorker {
 public:
  RotatorResetAsyncWorker(Napi::Env env, NodeRotator* rotator_instance, int reset_type)
      : RotatorAsyncWorker(env, rotator_instance), reset_type_(reset_type) {}

  void Execute() override {
    CHECK_ROT_VALID();
    result_code_ = shim_rot_reset(rotator_instance_->my_rot, reset_type_);
    if (result_code_ != SHIM_RIG_OK) {
      error_message_ = shim_rigerror(result_code_);
    }
  }

  void OnOK() override {
    if (!error_message_.empty()) {
      deferred_.Reject(Napi::Error::New(Env(), error_message_).Value());
      return;
    }
    deferred_.Resolve(Napi::Number::New(Env(), result_code_));
  }

 private:
  int reset_type_;
};

class RotatorGetInfoAsyncWorker : public RotatorAsyncWorker {
 public:
  RotatorGetInfoAsyncWorker(Napi::Env env, NodeRotator* rotator_instance)
      : RotatorAsyncWorker(env, rotator_instance) {}

  void Execute() override {
    CHECK_ROT_VALID();
    info_ = shim_rot_get_info(rotator_instance_->my_rot);
  }

  void OnOK() override {
    deferred_.Resolve(Napi::String::New(Env(), info_));
  }

 private:
  std::string info_;
};

class RotatorGetStatusAsyncWorker : public RotatorAsyncWorker {
 public:
  RotatorGetStatusAsyncWorker(Napi::Env env, NodeRotator* rotator_instance)
      : RotatorAsyncWorker(env, rotator_instance), status_(0) {}

  void Execute() override {
    CHECK_ROT_VALID();
    result_code_ = shim_rot_get_status(rotator_instance_->my_rot, &status_);
    if (result_code_ != SHIM_RIG_OK) {
      error_message_ = shim_rigerror(result_code_);
    }
  }

  void OnOK() override {
    if (!error_message_.empty()) {
      deferred_.Reject(Napi::Error::New(Env(), error_message_).Value());
      return;
    }
    Napi::Object result = Napi::Object::New(Env());
    result.Set("mask", Napi::Number::New(Env(), status_));
    result.Set("flags", buildStatusArray(Env(), status_));
    deferred_.Resolve(result);
  }

 private:
  int status_;
};

class RotatorSetConfAsyncWorker : public RotatorAsyncWorker {
 public:
  RotatorSetConfAsyncWorker(Napi::Env env, NodeRotator* rotator_instance, std::string name, std::string value)
      : RotatorAsyncWorker(env, rotator_instance), name_(std::move(name)), value_(std::move(value)) {}

  void Execute() override {
    CHECK_ROT_VALID();
    result_code_ = shim_rot_set_conf(rotator_instance_->my_rot, name_.c_str(), value_.c_str());
    if (result_code_ != SHIM_RIG_OK) {
      error_message_ = shim_rigerror(result_code_);
    }
  }

  void OnOK() override {
    if (!error_message_.empty()) {
      deferred_.Reject(Napi::Error::New(Env(), error_message_).Value());
      return;
    }
    deferred_.Resolve(Napi::Number::New(Env(), result_code_));
  }

 private:
  std::string name_;
  std::string value_;
};

class RotatorGetConfAsyncWorker : public RotatorAsyncWorker {
 public:
  RotatorGetConfAsyncWorker(Napi::Env env, NodeRotator* rotator_instance, std::string name)
      : RotatorAsyncWorker(env, rotator_instance), name_(std::move(name)) {
    memset(buf_, 0, sizeof(buf_));
  }

  void Execute() override {
    CHECK_ROT_VALID();
    result_code_ = shim_rot_get_conf(rotator_instance_->my_rot, name_.c_str(), buf_, sizeof(buf_));
    if (result_code_ != SHIM_RIG_OK) {
      error_message_ = shim_rigerror(result_code_);
    }
  }

  void OnOK() override {
    if (!error_message_.empty()) {
      deferred_.Reject(Napi::Error::New(Env(), error_message_).Value());
      return;
    }
    deferred_.Resolve(Napi::String::New(Env(), buf_));
  }

 private:
  std::string name_;
  char buf_[256];
};

class RotatorSetLevelAsyncWorker : public RotatorAsyncWorker {
 public:
  RotatorSetLevelAsyncWorker(Napi::Env env, NodeRotator* rotator_instance, uint64_t level, double value)
      : RotatorAsyncWorker(env, rotator_instance), level_(level), value_(value) {}

  void Execute() override {
    CHECK_ROT_VALID();
    if (level_ == SHIM_ROT_LEVEL_SPEED) {
      result_code_ = shim_rot_set_level_i(rotator_instance_->my_rot, level_, static_cast<int>(value_));
    } else {
      result_code_ = shim_rot_set_level_f(rotator_instance_->my_rot, level_, static_cast<float>(value_));
    }
    if (result_code_ != SHIM_RIG_OK) {
      error_message_ = shim_rigerror(result_code_);
    }
  }

  void OnOK() override {
    if (!error_message_.empty()) {
      deferred_.Reject(Napi::Error::New(Env(), error_message_).Value());
      return;
    }
    deferred_.Resolve(Napi::Number::New(Env(), result_code_));
  }

 private:
  uint64_t level_;
  double value_;
};

class RotatorGetLevelAsyncWorker : public RotatorAsyncWorker {
 public:
  RotatorGetLevelAsyncWorker(Napi::Env env, NodeRotator* rotator_instance, uint64_t level)
      : RotatorAsyncWorker(env, rotator_instance), level_(level), value_(0) {}

  void Execute() override {
    CHECK_ROT_VALID();
    result_code_ = shim_rot_get_level_auto(rotator_instance_->my_rot, level_, &value_);
    if (result_code_ != SHIM_RIG_OK) {
      error_message_ = shim_rigerror(result_code_);
    }
  }

  void OnOK() override {
    if (!error_message_.empty()) {
      deferred_.Reject(Napi::Error::New(Env(), error_message_).Value());
      return;
    }
    deferred_.Resolve(Napi::Number::New(Env(), value_));
  }

 private:
  uint64_t level_;
  double value_;
};

class RotatorSetFunctionAsyncWorker : public RotatorAsyncWorker {
 public:
  RotatorSetFunctionAsyncWorker(Napi::Env env, NodeRotator* rotator_instance, uint64_t func, int enable)
      : RotatorAsyncWorker(env, rotator_instance), func_(func), enable_(enable) {}

  void Execute() override {
    CHECK_ROT_VALID();
    result_code_ = shim_rot_set_func(rotator_instance_->my_rot, func_, enable_);
    if (result_code_ != SHIM_RIG_OK) {
      error_message_ = shim_rigerror(result_code_);
    }
  }

  void OnOK() override {
    if (!error_message_.empty()) {
      deferred_.Reject(Napi::Error::New(Env(), error_message_).Value());
      return;
    }
    deferred_.Resolve(Napi::Number::New(Env(), result_code_));
  }

 private:
  uint64_t func_;
  int enable_;
};

class RotatorGetFunctionAsyncWorker : public RotatorAsyncWorker {
 public:
  RotatorGetFunctionAsyncWorker(Napi::Env env, NodeRotator* rotator_instance, uint64_t func)
      : RotatorAsyncWorker(env, rotator_instance), func_(func), state_(0) {}

  void Execute() override {
    CHECK_ROT_VALID();
    result_code_ = shim_rot_get_func(rotator_instance_->my_rot, func_, &state_);
    if (result_code_ != SHIM_RIG_OK) {
      error_message_ = shim_rigerror(result_code_);
    }
  }

  void OnOK() override {
    if (!error_message_.empty()) {
      deferred_.Reject(Napi::Error::New(Env(), error_message_).Value());
      return;
    }
    deferred_.Resolve(Napi::Boolean::New(Env(), state_ != 0));
  }

 private:
  uint64_t func_;
  int state_;
};

class RotatorSetParmAsyncWorker : public RotatorAsyncWorker {
 public:
  RotatorSetParmAsyncWorker(Napi::Env env, NodeRotator* rotator_instance, uint64_t parm, double value)
      : RotatorAsyncWorker(env, rotator_instance), parm_(parm), value_(value) {}

  void Execute() override {
    CHECK_ROT_VALID();
    result_code_ = shim_rot_set_parm_f(rotator_instance_->my_rot, parm_, static_cast<float>(value_));
    if (result_code_ != SHIM_RIG_OK) {
      error_message_ = shim_rigerror(result_code_);
    }
  }

  void OnOK() override {
    if (!error_message_.empty()) {
      deferred_.Reject(Napi::Error::New(Env(), error_message_).Value());
      return;
    }
    deferred_.Resolve(Napi::Number::New(Env(), result_code_));
  }

 private:
  uint64_t parm_;
  double value_;
};

class RotatorGetParmAsyncWorker : public RotatorAsyncWorker {
 public:
  RotatorGetParmAsyncWorker(Napi::Env env, NodeRotator* rotator_instance, uint64_t parm)
      : RotatorAsyncWorker(env, rotator_instance), parm_(parm), value_(0) {}

  void Execute() override {
    CHECK_ROT_VALID();
    float raw = 0;
    result_code_ = shim_rot_get_parm_f(rotator_instance_->my_rot, parm_, &raw);
    value_ = raw;
    if (result_code_ != SHIM_RIG_OK) {
      error_message_ = shim_rigerror(result_code_);
    }
  }

  void OnOK() override {
    if (!error_message_.empty()) {
      deferred_.Reject(Napi::Error::New(Env(), error_message_).Value());
      return;
    }
    deferred_.Resolve(Napi::Number::New(Env(), value_));
  }

 private:
  uint64_t parm_;
  double value_;
};

bool NodeRotator::isNetworkAddress(const char* path) {
  if (path == nullptr) {
    return false;
  }

  const char* colon = strchr(path, ':');
  return colon != nullptr && strlen(colon + 1) > 0;
}

NodeRotator::NodeRotator(const Napi::CallbackInfo& info) : ObjectWrap(info), my_rot(nullptr) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsNumber()) {
    Napi::TypeError::New(env, "Invalid rotator model").ThrowAsJavaScriptException();
    return;
  }

  bool has_port = false;
  port_path[0] = '\0';
  if (info.Length() >= 2 && info[1].IsString()) {
    const std::string port = info[1].As<Napi::String>().Utf8Value();
    if (!port.empty()) {
      strncpy(port_path, port.c_str(), SHIM_HAMLIB_FILPATHLEN - 1);
      port_path[SHIM_HAMLIB_FILPATHLEN - 1] = '\0';
      has_port = true;
    }
  }

  unsigned int model = info[0].As<Napi::Number>().Uint32Value();
  original_model = model;
  is_network_rot = isNetworkAddress(port_path);
  if (is_network_rot) {
    model = 2;  // ROT_MODEL_NETROTCTL
  }

  my_rot = shim_rot_init(model);
  if (!my_rot) {
    Napi::TypeError::New(env, "Unable to initialize rotator").ThrowAsJavaScriptException();
    return;
  }

  if (has_port) {
    shim_rot_set_port_path(my_rot, port_path);
    shim_rot_set_port_type(my_rot, is_network_rot ? SHIM_RIG_PORT_NETWORK : SHIM_RIG_PORT_SERIAL);
  }
}

NodeRotator::~NodeRotator() {
  if (my_rot) {
    if (rot_is_open) {
      shim_rot_close(my_rot);
      rot_is_open = false;
    }
    shim_rot_cleanup(my_rot);
    my_rot = nullptr;
  }
}

Napi::Value NodeRotator::Open(const Napi::CallbackInfo& info) {
  auto* worker = new RotatorOpenAsyncWorker(info.Env(), this);
  worker->Queue();
  return worker->GetPromise();
}

Napi::Value NodeRotator::Close(const Napi::CallbackInfo& info) {
  auto* worker = new RotatorCloseAsyncWorker(info.Env(), this);
  worker->Queue();
  return worker->GetPromise();
}

Napi::Value NodeRotator::Destroy(const Napi::CallbackInfo& info) {
  auto* worker = new RotatorDestroyAsyncWorker(info.Env(), this);
  worker->Queue();
  return worker->GetPromise();
}

Napi::Value NodeRotator::GetConnectionInfo(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::Object obj = Napi::Object::New(env);
  obj.Set("connectionType", Napi::String::New(env, is_network_rot ? "network" : "serial"));
  obj.Set("portPath", Napi::String::New(env, port_path));
  obj.Set("isOpen", Napi::Boolean::New(env, rot_is_open));
  obj.Set("originalModel", Napi::Number::New(env, original_model));
  obj.Set("currentModel", Napi::Number::New(env, is_network_rot ? 2 : original_model));
  return obj;
}

Napi::Value NodeRotator::SetPosition(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (!rot_is_open) {
    Napi::TypeError::New(env, "Rotator is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
    Napi::TypeError::New(env, "Expected (azimuth: number, elevation: number)").ThrowAsJavaScriptException();
    return env.Null();
  }

  auto* worker = new RotatorSetPositionAsyncWorker(
      env,
      this,
      info[0].As<Napi::Number>().DoubleValue(),
      info[1].As<Napi::Number>().DoubleValue());
  worker->Queue();
  return worker->GetPromise();
}

Napi::Value NodeRotator::GetPosition(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (!rot_is_open) {
    Napi::TypeError::New(env, "Rotator is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }

  auto* worker = new RotatorGetPositionAsyncWorker(env, this);
  worker->Queue();
  return worker->GetPromise();
}

Napi::Value NodeRotator::Move(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (!rot_is_open) {
    Napi::TypeError::New(env, "Rotator is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info.Length() < 2) {
    Napi::TypeError::New(env, "Expected (direction: string|number, speed: number)").ThrowAsJavaScriptException();
    return env.Null();
  }

  const int direction = parseRotDirection(env, info[0]);
  if (direction == std::numeric_limits<int>::min()) {
    return env.Null();
  }
  if (!info[1].IsNumber()) {
    Napi::TypeError::New(env, "Expected speed as number").ThrowAsJavaScriptException();
    return env.Null();
  }
  const int speed = info[1].As<Napi::Number>().Int32Value();

  auto* worker = new RotatorMoveAsyncWorker(env, this, direction, speed);
  worker->Queue();
  return worker->GetPromise();
}

Napi::Value NodeRotator::Stop(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (!rot_is_open) {
    Napi::TypeError::New(env, "Rotator is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  auto* worker = new RotatorSimpleAsyncWorker(env, this, shim_rot_stop);
  worker->Queue();
  return worker->GetPromise();
}

Napi::Value NodeRotator::Park(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (!rot_is_open) {
    Napi::TypeError::New(env, "Rotator is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  auto* worker = new RotatorSimpleAsyncWorker(env, this, shim_rot_park);
  worker->Queue();
  return worker->GetPromise();
}

Napi::Value NodeRotator::Reset(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (!rot_is_open) {
    Napi::TypeError::New(env, "Rotator is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info.Length() < 1) {
    Napi::TypeError::New(env, "Expected reset type").ThrowAsJavaScriptException();
    return env.Null();
  }
  const int reset = parseRotReset(env, info[0]);
  if (reset == std::numeric_limits<int>::min()) {
    return env.Null();
  }

  auto* worker = new RotatorResetAsyncWorker(env, this, reset);
  worker->Queue();
  return worker->GetPromise();
}

Napi::Value NodeRotator::GetInfo(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (!rot_is_open) {
    Napi::TypeError::New(env, "Rotator is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  auto* worker = new RotatorGetInfoAsyncWorker(env, this);
  worker->Queue();
  return worker->GetPromise();
}

Napi::Value NodeRotator::GetStatus(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (!rot_is_open) {
    Napi::TypeError::New(env, "Rotator is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  auto* worker = new RotatorGetStatusAsyncWorker(env, this);
  worker->Queue();
  return worker->GetPromise();
}

Napi::Value NodeRotator::SetConf(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  RETURN_NULL_IF_ROT_HANDLE_INVALID();
  if (info.Length() < 2 || !info[0].IsString() || !info[1].IsString()) {
    Napi::TypeError::New(env, "Expected (name: string, value: string)").ThrowAsJavaScriptException();
    return env.Null();
  }

  auto* worker = new RotatorSetConfAsyncWorker(
      env,
      this,
      info[0].As<Napi::String>().Utf8Value(),
      info[1].As<Napi::String>().Utf8Value());
  worker->Queue();
  return worker->GetPromise();
}

Napi::Value NodeRotator::GetConf(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  RETURN_NULL_IF_ROT_HANDLE_INVALID();
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected (name: string)").ThrowAsJavaScriptException();
    return env.Null();
  }

  auto* worker = new RotatorGetConfAsyncWorker(env, this, info[0].As<Napi::String>().Utf8Value());
  worker->Queue();
  return worker->GetPromise();
}

int NodeRotator::rot_config_callback(const shim_confparam_info_t* info, void* data) {
  auto* schema_data = static_cast<RotatorConfigSchemaData*>(data);
  if (!schema_data || !info) {
    return 0;
  }

  RotatorConfigFieldDescriptor field;
  field.token = info->token;
  field.name = info->name;
  field.label = info->label;
  field.tooltip = info->tooltip;
  field.defaultValue = info->dflt;
  field.type = info->type;
  field.numericMin = info->numeric_min;
  field.numericMax = info->numeric_max;
  field.numericStep = info->numeric_step;
  for (int i = 0; i < info->combo_count; ++i) {
    field.options.emplace_back(info->combo_options[i]);
  }

  schema_data->fields.push_back(field);
  return 1;
}

Napi::Value NodeRotator::GetConfigSchema(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  RETURN_NULL_IF_ROT_HANDLE_INVALID();

  RotatorConfigSchemaData schemaData{};
  const int result = shim_rot_cfgparams_foreach(my_rot, rot_config_callback, &schemaData);
  if (result != SHIM_RIG_OK) {
    Napi::Error::New(env, shim_rigerror(result)).ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Array schemaArray = Napi::Array::New(env, schemaData.fields.size());
  for (size_t i = 0; i < schemaData.fields.size(); ++i) {
    const RotatorConfigFieldDescriptor& descriptor = schemaData.fields[i];
    Napi::Object field = Napi::Object::New(env);
    field.Set("token", Napi::Number::New(env, descriptor.token));
    field.Set("name", Napi::String::New(env, descriptor.name));
    field.Set("label", Napi::String::New(env, descriptor.label));
    field.Set("tooltip", Napi::String::New(env, descriptor.tooltip));
    field.Set("defaultValue", Napi::String::New(env, descriptor.defaultValue));
    field.Set("type", Napi::String::New(env, publicConfTypeName(descriptor.type)));

    if (descriptor.type == 2 || descriptor.type == 6) {
      Napi::Object numeric = Napi::Object::New(env);
      numeric.Set("min", Napi::Number::New(env, descriptor.numericMin));
      numeric.Set("max", Napi::Number::New(env, descriptor.numericMax));
      numeric.Set("step", Napi::Number::New(env, descriptor.numericStep));
      field.Set("numeric", numeric);
    }

    if (!descriptor.options.empty()) {
      Napi::Array options = Napi::Array::New(env, descriptor.options.size());
      for (size_t optionIndex = 0; optionIndex < descriptor.options.size(); ++optionIndex) {
        options[static_cast<uint32_t>(optionIndex)] =
            Napi::String::New(env, descriptor.options[optionIndex]);
      }
      field.Set("options", options);
    }

    schemaArray[static_cast<uint32_t>(i)] = field;
  }

  return schemaArray;
}

Napi::Value NodeRotator::GetPortCaps(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  RETURN_NULL_IF_ROT_HANDLE_INVALID();

  shim_rig_port_caps_t caps{};
  const int result = shim_rot_get_port_caps(my_rot, &caps);
  if (result != SHIM_RIG_OK) {
    Napi::Error::New(env, shim_rigerror(result)).ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Object portCaps = Napi::Object::New(env);
  portCaps.Set("portType", Napi::String::New(env, caps.port_type));
  portCaps.Set("writeDelay", Napi::Number::New(env, caps.write_delay));
  portCaps.Set("postWriteDelay", Napi::Number::New(env, caps.post_write_delay));
  portCaps.Set("timeout", Napi::Number::New(env, caps.timeout));
  portCaps.Set("retry", Napi::Number::New(env, caps.retry));

  if (hasPositiveValue(caps.serial_rate_min)) {
    portCaps.Set("serialRateMin", Napi::Number::New(env, caps.serial_rate_min));
  }
  if (hasPositiveValue(caps.serial_rate_max)) {
    portCaps.Set("serialRateMax", Napi::Number::New(env, caps.serial_rate_max));
  }
  if (hasPositiveValue(caps.serial_data_bits)) {
    portCaps.Set("serialDataBits", Napi::Number::New(env, caps.serial_data_bits));
  }
  if (hasPositiveValue(caps.serial_stop_bits)) {
    portCaps.Set("serialStopBits", Napi::Number::New(env, caps.serial_stop_bits));
  }
  if (caps.serial_parity[0] != '\0' && std::string(caps.serial_parity) != "Unknown") {
    portCaps.Set("serialParity", Napi::String::New(env, caps.serial_parity));
  }
  if (caps.serial_handshake[0] != '\0' && std::string(caps.serial_handshake) != "Unknown") {
    portCaps.Set("serialHandshake", Napi::String::New(env, caps.serial_handshake));
  }

  return portCaps;
}

Napi::Value NodeRotator::GetRotatorCaps(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  RETURN_NULL_IF_ROT_HANDLE_INVALID();

  shim_rot_caps_t caps{};
  const int result = shim_rot_get_caps(my_rot, &caps);
  if (result != SHIM_RIG_OK) {
    Napi::Error::New(env, shim_rigerror(result)).ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Object rotCaps = Napi::Object::New(env);
  rotCaps.Set("rotType", Napi::String::New(env, shim_rot_type_str(caps.rot_type)));
  rotCaps.Set("rotTypeMask", Napi::Number::New(env, caps.rot_type));
  rotCaps.Set("minAz", Napi::Number::New(env, caps.min_az));
  rotCaps.Set("maxAz", Napi::Number::New(env, caps.max_az));
  rotCaps.Set("minEl", Napi::Number::New(env, caps.min_el));
  rotCaps.Set("maxEl", Napi::Number::New(env, caps.max_el));
  rotCaps.Set("supportedStatuses", buildStatusArray(env, caps.has_status));
  return rotCaps;
}

Napi::Value NodeRotator::SetLevel(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (!rot_is_open) {
    Napi::TypeError::New(env, "Rotator is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info.Length() < 2 || !info[1].IsNumber()) {
    Napi::TypeError::New(env, "Expected (level: string, value: number)").ThrowAsJavaScriptException();
    return env.Null();
  }
  const uint64_t level = parseRotLevel(env, info[0]);
  if (level == 0) return env.Null();

  auto* worker = new RotatorSetLevelAsyncWorker(env, this, level, info[1].As<Napi::Number>().DoubleValue());
  worker->Queue();
  return worker->GetPromise();
}

Napi::Value NodeRotator::GetLevel(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (!rot_is_open) {
    Napi::TypeError::New(env, "Rotator is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info.Length() < 1) {
    Napi::TypeError::New(env, "Expected level name").ThrowAsJavaScriptException();
    return env.Null();
  }
  const uint64_t level = parseRotLevel(env, info[0]);
  if (level == 0) return env.Null();

  auto* worker = new RotatorGetLevelAsyncWorker(env, this, level);
  worker->Queue();
  return worker->GetPromise();
}

Napi::Value NodeRotator::GetSupportedLevels(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  RETURN_NULL_IF_ROT_HANDLE_INVALID();
  const uint64_t levels = shim_rot_get_caps_has_get_level(my_rot) | shim_rot_get_caps_has_set_level(my_rot);
  return buildSettingArrayFromMask(env, levels, shim_rot_strlevel);
}

Napi::Value NodeRotator::SetFunction(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (!rot_is_open) {
    Napi::TypeError::New(env, "Rotator is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info.Length() < 2 || !info[1].IsBoolean()) {
    Napi::TypeError::New(env, "Expected (function: string, enable: boolean)").ThrowAsJavaScriptException();
    return env.Null();
  }
  const uint64_t func = parseRotFunc(env, info[0]);
  if (func == 0) return env.Null();

  auto* worker = new RotatorSetFunctionAsyncWorker(env, this, func, info[1].As<Napi::Boolean>().Value() ? 1 : 0);
  worker->Queue();
  return worker->GetPromise();
}

Napi::Value NodeRotator::GetFunction(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (!rot_is_open) {
    Napi::TypeError::New(env, "Rotator is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info.Length() < 1) {
    Napi::TypeError::New(env, "Expected function name").ThrowAsJavaScriptException();
    return env.Null();
  }
  const uint64_t func = parseRotFunc(env, info[0]);
  if (func == 0) return env.Null();

  auto* worker = new RotatorGetFunctionAsyncWorker(env, this, func);
  worker->Queue();
  return worker->GetPromise();
}

Napi::Value NodeRotator::GetSupportedFunctions(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  RETURN_NULL_IF_ROT_HANDLE_INVALID();
  const uint64_t funcs = shim_rot_get_caps_has_get_func(my_rot) | shim_rot_get_caps_has_set_func(my_rot);
  return buildSettingArrayFromMask(env, funcs, shim_rot_strfunc);
}

Napi::Value NodeRotator::SetParm(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (!rot_is_open) {
    Napi::TypeError::New(env, "Rotator is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info.Length() < 2 || !info[1].IsNumber()) {
    Napi::TypeError::New(env, "Expected (parameter: string, value: number)").ThrowAsJavaScriptException();
    return env.Null();
  }
  const uint64_t parm = parseRotParm(env, info[0]);
  if (parm == 0) return env.Null();

  auto* worker = new RotatorSetParmAsyncWorker(env, this, parm, info[1].As<Napi::Number>().DoubleValue());
  worker->Queue();
  return worker->GetPromise();
}

Napi::Value NodeRotator::GetParm(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (!rot_is_open) {
    Napi::TypeError::New(env, "Rotator is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info.Length() < 1) {
    Napi::TypeError::New(env, "Expected parameter name").ThrowAsJavaScriptException();
    return env.Null();
  }
  const uint64_t parm = parseRotParm(env, info[0]);
  if (parm == 0) return env.Null();

  auto* worker = new RotatorGetParmAsyncWorker(env, this, parm);
  worker->Queue();
  return worker->GetPromise();
}

Napi::Value NodeRotator::GetSupportedParms(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  RETURN_NULL_IF_ROT_HANDLE_INVALID();
  const uint64_t parms = shim_rot_get_caps_has_get_parm(my_rot) | shim_rot_get_caps_has_set_parm(my_rot);
  return buildSettingArrayFromMask(env, parms, shim_rot_strparm);
}

int NodeRotator::rot_list_callback(const shim_rot_info_t* info, void* data) {
  auto* rot_data = static_cast<RotatorListData*>(data);
  Napi::Object rotInfo = Napi::Object::New(rot_data->env);
  rotInfo.Set("rotModel", Napi::Number::New(rot_data->env, info->rot_model));
  rotInfo.Set("modelName", Napi::String::New(rot_data->env, info->model_name ? info->model_name : ""));
  rotInfo.Set("mfgName", Napi::String::New(rot_data->env, info->mfg_name ? info->mfg_name : ""));
  rotInfo.Set("version", Napi::String::New(rot_data->env, info->version ? info->version : ""));
  rotInfo.Set("status", Napi::String::New(rot_data->env, shim_rig_strstatus(info->status)));
  rotInfo.Set("rotType", Napi::String::New(rot_data->env, shim_rot_type_str(info->rot_type)));
  rotInfo.Set("rotTypeMask", Napi::Number::New(rot_data->env, info->rot_type));
  rot_data->rotators.push_back(rotInfo);
  return 1;
}

Napi::Value NodeRotator::GetSupportedRotators(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  shim_rot_load_all_backends();

  RotatorListData rotData{std::vector<Napi::Object>(), env};
  const int result = shim_rot_list_foreach(rot_list_callback, &rotData);
  if (result != SHIM_RIG_OK) {
    Napi::Error::New(env, "Failed to retrieve supported rotator list").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Array rotArray = Napi::Array::New(env, rotData.rotators.size());
  for (size_t i = 0; i < rotData.rotators.size(); ++i) {
    rotArray[static_cast<uint32_t>(i)] = rotData.rotators[i];
  }
  return rotArray;
}

Napi::Value NodeRotator::GetHamlibVersion(const Napi::CallbackInfo& info) {
  return Napi::String::New(info.Env(), shim_rig_get_version());
}

Napi::Value NodeRotator::SetDebugLevel(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() < 1 || !info[0].IsNumber()) {
    Napi::TypeError::New(env, "Debug level (number) required").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  const int level = info[0].As<Napi::Number>().Int32Value();
  if (level < 0 || level > 5) {
    Napi::RangeError::New(env, "Debug level must be between 0 (NONE) and 5 (TRACE)").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  shim_rig_set_debug(level);
  return env.Undefined();
}

Napi::Value NodeRotator::GetDebugLevel(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::Error::New(
      env,
      "Getting debug level is not supported by Hamlib API. "
      "Please track the debug level you set using setDebugLevel().")
      .ThrowAsJavaScriptException();
  return env.Undefined();
}

Napi::Value NodeRotator::GetCopyright(const Napi::CallbackInfo& info) {
  return Napi::String::New(info.Env(), shim_rig_copyright());
}

Napi::Value NodeRotator::GetLicense(const Napi::CallbackInfo& info) {
  return Napi::String::New(info.Env(), shim_rig_license());
}

Napi::Function NodeRotator::GetClass(Napi::Env env) {
  auto klass = DefineClass(
      env,
      "Rotator",
      {
          InstanceMethod("open", &NodeRotator::Open),
          InstanceMethod("close", &NodeRotator::Close),
          InstanceMethod("destroy", &NodeRotator::Destroy),
          InstanceMethod("getConnectionInfo", &NodeRotator::GetConnectionInfo),
          InstanceMethod("setPosition", &NodeRotator::SetPosition),
          InstanceMethod("getPosition", &NodeRotator::GetPosition),
          InstanceMethod("move", &NodeRotator::Move),
          InstanceMethod("stop", &NodeRotator::Stop),
          InstanceMethod("park", &NodeRotator::Park),
          InstanceMethod("reset", &NodeRotator::Reset),
          InstanceMethod("getInfo", &NodeRotator::GetInfo),
          InstanceMethod("getStatus", &NodeRotator::GetStatus),
          InstanceMethod("setConf", &NodeRotator::SetConf),
          InstanceMethod("getConf", &NodeRotator::GetConf),
          InstanceMethod("getConfigSchema", &NodeRotator::GetConfigSchema),
          InstanceMethod("getPortCaps", &NodeRotator::GetPortCaps),
          InstanceMethod("getRotatorCaps", &NodeRotator::GetRotatorCaps),
          InstanceMethod("setLevel", &NodeRotator::SetLevel),
          InstanceMethod("getLevel", &NodeRotator::GetLevel),
          InstanceMethod("getSupportedLevels", &NodeRotator::GetSupportedLevels),
          InstanceMethod("setFunction", &NodeRotator::SetFunction),
          InstanceMethod("getFunction", &NodeRotator::GetFunction),
          InstanceMethod("getSupportedFunctions", &NodeRotator::GetSupportedFunctions),
          InstanceMethod("setParm", &NodeRotator::SetParm),
          InstanceMethod("getParm", &NodeRotator::GetParm),
          InstanceMethod("getSupportedParms", &NodeRotator::GetSupportedParms),
          StaticMethod("getSupportedRotators", &NodeRotator::GetSupportedRotators),
          StaticMethod("getHamlibVersion", &NodeRotator::GetHamlibVersion),
          StaticMethod("setDebugLevel", &NodeRotator::SetDebugLevel),
          StaticMethod("getDebugLevel", &NodeRotator::GetDebugLevel),
          StaticMethod("getCopyright", &NodeRotator::GetCopyright),
          StaticMethod("getLicense", &NodeRotator::GetLicense),
      });
  constructor = Napi::Persistent(klass);
  constructor.SuppressDestruct();
  return klass;
}
