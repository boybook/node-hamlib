#include "hamlib.h"
#include <string>



using namespace Napi;

Napi::FunctionReference NodeHamLib::constructor;
Napi::ThreadSafeFunction tsfn;

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
  
  int retcode = rig_open(my_rig);
  if (retcode != RIG_OK) {
    printf("rig_open: error = %s\n", rigerror(retcode));
    // Napi::TypeError::New(env, "Unable to open rig")
    // .ThrowAsJavaScriptException();
  }



  rig_set_freq_callback(my_rig, NodeHamLib::freq_change_cb, this);

  auto ppt_cb =+[](RIG *rig, vfo_t vfo, ptt_t ptt, rig_ptr_t arg) {
    printf("PPT pushed!");
    return 0;
  };
  retcode = rig_set_ptt_callback (my_rig, ppt_cb, NULL);
  rig_set_trn(my_rig, RIG_TRN_POLL);
  if (retcode != RIG_OK ) {
	  printf("rig_set_trn: error = %s \n", rigerror(retcode));
	}

  printf ("callback: %s", rigerror(retcode));

  rig_is_open = true;
  return Napi::Number::New(env, retcode);
}

Napi::Value NodeHamLib::SetVFO(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  int retcode;

  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info.Length() < 1) {
    Napi::TypeError::New(env, "Must Specify VFO-A or VFO-B")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  if (!info[0].IsString()) {
    Napi::TypeError::New(env, "Must Specify VFO-A or VFO-B as a string")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  auto name = info[0].As < Napi::String > ().Utf8Value().c_str();
  if (strcmp(name, "VFO-A") == 0) {
    retcode = rig_set_vfo(my_rig, RIG_VFO_A);
  } else if (strcmp(name, "VFO-B") == 0) {
    retcode = rig_set_vfo(my_rig, RIG_VFO_B);
  } else {
    retcode = 1;
  }
  return Napi::Number::New(env, retcode);
}

Napi::Value NodeHamLib::SetFrequency(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  int retcode;
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
    Napi::TypeError::New(env, "Frequency must be specified as an integer")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  auto freq = info[0].As < Napi::Number > ().Int32Value();
  
  // Support optional VFO parameter
  vfo_t vfo = RIG_VFO_CURR;
  if (info.Length() >= 2 && info[1].IsString()) {
    auto vfostr = info[1].As < Napi::String > ().Utf8Value().c_str();
    if (strcmp(vfostr, "VFO-A") == 0) {
      vfo = RIG_VFO_A;
    } else if (strcmp(vfostr, "VFO-B") == 0) {
      vfo = RIG_VFO_B;
    }
  }
  
  retcode = rig_set_freq(my_rig, vfo, freq);
  return Napi::Number::New(env, retcode);
}

Napi::Value NodeHamLib::SetMode(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  int retcode;
  pbwidth_t bandwidth;
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info.Length() < 1) {
    Napi::TypeError::New(env, "Must Specify Mode")
      .ThrowAsJavaScriptException();
    return env.Null();
  }

  if (!info[0].IsString()) {
    Napi::TypeError::New(env, "Must Specify Mode as string")
      .ThrowAsJavaScriptException();
    return env.Null();
  }

  auto modestr = info[0].As < Napi::String > ().Utf8Value().c_str();
  auto mode = rig_parse_mode(modestr);

  if (info.Length() > 1) {
    if (!info[1].IsString()) {
      Napi::TypeError::New(env, "Must Specify Mode as string")
        .ThrowAsJavaScriptException();
      return env.Null();
    }
    auto bandstr = info[1].As < Napi::String > ().Utf8Value().c_str();
    if (strcmp(bandstr, "narrow") == 0) {
      bandwidth = rig_passband_narrow(my_rig, mode);
    } else if (strcmp(bandstr, "wide") == 0) {
      bandwidth = rig_passband_wide(my_rig, mode);
    } else {
      bandwidth = RIG_PASSBAND_NORMAL;
    }
  } else {
    bandwidth = RIG_PASSBAND_NORMAL;
  }

  retcode = rig_set_mode(my_rig, RIG_VFO_CURR, mode, bandwidth);
  if (retcode != RIG_OK) {

    Napi::TypeError::New(env, rigerror(retcode))
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  return Napi::Number::New(env, retcode);
}

Napi::Value NodeHamLib::SetPtt(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  int retcode;
  bool ptt_state;
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info.Length() < 1) {
    Napi::TypeError::New(env, "Specify true or false for ppt state")
      .ThrowAsJavaScriptException();
    return env.Null();
  }

  if (!info[0].IsBoolean()) {
    Napi::TypeError::New(env, "PTT state is not boolean")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  ptt_state = info[0].As < Napi::Boolean > ();
  if (ptt_state) {
    retcode = rig_set_ptt(my_rig, RIG_VFO_CURR, RIG_PTT_ON);
  } else {
    retcode = rig_set_ptt(my_rig, RIG_VFO_CURR, RIG_PTT_OFF);
  }
  if (retcode != RIG_OK) {

    Napi::TypeError::New(env, rigerror(retcode))
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  return Napi::Number::New(env, retcode);
}

Napi::Value NodeHamLib::GetVFO(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  int retcode;
  vfo_t vfo;
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  retcode = rig_get_vfo(my_rig, & vfo);
  if (retcode == RIG_OK) {
    return Napi::Number::New(env, vfo);
  } else {
    //dont throw an exception here, not every radio reports vfo
    Napi::Error::New(env, rigerror(retcode));
    return env.Null();
  }

}

Napi::Value NodeHamLib::GetFrequency(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  int retcode;
  freq_t freq;
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  
  // Support optional VFO parameter
  vfo_t vfo = RIG_VFO_CURR;
  if (info.Length() >= 1 && info[0].IsString()) {
    auto vfostr = info[0].As < Napi::String > ().Utf8Value().c_str();
    if (strcmp(vfostr, "VFO-A") == 0) {
      vfo = RIG_VFO_A;
    } else if (strcmp(vfostr, "VFO-B") == 0) {
      vfo = RIG_VFO_B;
    }
  }
  
  retcode = rig_get_freq(my_rig, vfo, & freq);
  if (retcode == RIG_OK) {
    return Napi::Number::New(env, freq);
  } else {
    Napi::Error::New(env, rigerror(retcode));
    return env.Null();
  }
}

Napi::Value NodeHamLib::GetMode(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  int retcode;
  rmode_t rmode;
  pbwidth_t width;
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  retcode = rig_get_mode(my_rig, RIG_VFO_CURR, & rmode, & width);
  if (retcode == RIG_OK) {
    Napi::Object obj = Napi::Object::New(env);
    obj.Set(Napi::String::New(env, "mode"), (char)rmode);
    obj.Set(Napi::String::New(env, "width"), width);
    return obj;
  } else {
    Napi::Error::New(env, rigerror(retcode));
    return env.Null();
  }
}

Napi::Value NodeHamLib::GetStrength(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  int retcode;
  int strength;
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  retcode = rig_get_strength(my_rig, RIG_VFO_CURR, & strength);
  if (retcode == RIG_OK) {
    return Napi::Number::New(env, strength);
  } else {
    Napi::Error::New(env, rigerror(retcode));
    return env.Null();
  }
}

Napi::Value NodeHamLib::Close(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  int retcode = rig_close(my_rig);
  if (retcode != RIG_OK) {
    Napi::TypeError::New(env, "Unable to open rig")
      .ThrowAsJavaScriptException();
  }
  rig_is_open = false;
  return Napi::Number::New(env, retcode);
}

Napi::Value NodeHamLib::Destroy(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  if (rig_is_open) {
    rig_close(my_rig);
  }
  int retcode = rig_cleanup(my_rig);
  if (retcode != RIG_OK) {

    Napi::TypeError::New(env, rigerror(retcode))
      .ThrowAsJavaScriptException();
  }
  rig_is_open = false;
  return Napi::Number::New(env, retcode);
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
      
      
      

      NodeHamLib::InstanceMethod("close", & NodeHamLib::Close),
      NodeHamLib::InstanceMethod("destroy", & NodeHamLib::Destroy),
      NodeHamLib::InstanceMethod("getConnectionInfo", & NodeHamLib::GetConnectionInfo),
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

