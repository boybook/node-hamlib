#include "hamlib.h"
#include <string>
#include <vector>

// Structure to hold rig information for the callback
struct RigListData {
  std::vector<Napi::Object> rigList;
  Napi::Env env;
};

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
  
  int retcode = rig_set_channel(my_rig, RIG_VFO_MEM, &chan);
  if (retcode != RIG_OK) {
    Napi::Error::New(env, rigerror(retcode)).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  return Napi::Number::New(env, retcode);
}

Napi::Value NodeHamLib::GetMemoryChannel(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (info.Length() < 1 || !info[0].IsNumber()) {
    Napi::TypeError::New(env, "Expected channel number").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  int channel_num = info[0].As<Napi::Number>().Int32Value();
  bool read_only = true;
  
  if (info.Length() >= 2 && info[1].IsBoolean()) {
    read_only = info[1].As<Napi::Boolean>().Value();
  }
  
  // Create channel structure
  channel_t chan;
  memset(&chan, 0, sizeof(chan));
  chan.channel_num = channel_num;
  chan.vfo = RIG_VFO_MEM;
  
  int retcode = rig_get_channel(my_rig, RIG_VFO_MEM, &chan, read_only);
  if (retcode != RIG_OK) {
    Napi::Error::New(env, rigerror(retcode)).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  // Create result object
  Napi::Object result = Napi::Object::New(env);
  result.Set("channelNumber", Napi::Number::New(env, chan.channel_num));
  result.Set("frequency", Napi::Number::New(env, chan.freq));
  result.Set("mode", Napi::String::New(env, rig_strrmode(chan.mode)));
  result.Set("bandwidth", Napi::Number::New(env, chan.width));
  result.Set("description", Napi::String::New(env, chan.channel_desc));
  
  if (chan.split == RIG_SPLIT_ON) {
    result.Set("txFrequency", Napi::Number::New(env, chan.tx_freq));
    result.Set("split", Napi::Boolean::New(env, true));
  } else {
    result.Set("split", Napi::Boolean::New(env, false));
  }
  
  if (chan.ctcss_tone > 0) {
    result.Set("ctcssTone", Napi::Number::New(env, chan.ctcss_tone));
  }
  
  return result;
}

Napi::Value NodeHamLib::SelectMemoryChannel(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (info.Length() < 1 || !info[0].IsNumber()) {
    Napi::TypeError::New(env, "Expected channel number").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  int channel_num = info[0].As<Napi::Number>().Int32Value();
  
  int retcode = rig_set_mem(my_rig, RIG_VFO_CURR, channel_num);
  if (retcode != RIG_OK) {
    Napi::Error::New(env, rigerror(retcode)).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  return Napi::Number::New(env, retcode);
}

// RIT/XIT Control
Napi::Value NodeHamLib::SetRit(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (info.Length() < 1 || !info[0].IsNumber()) {
    Napi::TypeError::New(env, "Expected RIT offset in Hz").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  shortfreq_t rit_offset = info[0].As<Napi::Number>().Int32Value();
  
  int retcode = rig_set_rit(my_rig, RIG_VFO_CURR, rit_offset);
  if (retcode != RIG_OK) {
    Napi::Error::New(env, rigerror(retcode)).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  return Napi::Number::New(env, retcode);
}

Napi::Value NodeHamLib::GetRit(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  shortfreq_t rit_offset;
  int retcode = rig_get_rit(my_rig, RIG_VFO_CURR, &rit_offset);
  if (retcode != RIG_OK) {
    Napi::Error::New(env, rigerror(retcode)).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  return Napi::Number::New(env, rit_offset);
}

Napi::Value NodeHamLib::SetXit(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (info.Length() < 1 || !info[0].IsNumber()) {
    Napi::TypeError::New(env, "Expected XIT offset in Hz").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  shortfreq_t xit_offset = info[0].As<Napi::Number>().Int32Value();
  
  int retcode = rig_set_xit(my_rig, RIG_VFO_CURR, xit_offset);
  if (retcode != RIG_OK) {
    Napi::Error::New(env, rigerror(retcode)).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  return Napi::Number::New(env, retcode);
}

Napi::Value NodeHamLib::GetXit(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  shortfreq_t xit_offset;
  int retcode = rig_get_xit(my_rig, RIG_VFO_CURR, &xit_offset);
  if (retcode != RIG_OK) {
    Napi::Error::New(env, rigerror(retcode)).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  return Napi::Number::New(env, xit_offset);
}

Napi::Value NodeHamLib::ClearRitXit(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  // Clear both RIT and XIT by setting them to 0
  int ritCode = rig_set_rit(my_rig, RIG_VFO_CURR, 0);
  int xitCode = rig_set_xit(my_rig, RIG_VFO_CURR, 0);
  
  if (ritCode != RIG_OK && ritCode != -RIG_ENAVAIL) {
    Napi::Error::New(env, rigerror(ritCode)).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (xitCode != RIG_OK && xitCode != -RIG_ENAVAIL) {
    Napi::Error::New(env, rigerror(xitCode)).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  return Napi::Boolean::New(env, true);
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
  if (info.Length() >= 2 && info[1].IsNumber()) {
    channel = info[1].As<Napi::Number>().Int32Value();
  }
  
  int retcode = rig_scan(my_rig, RIG_VFO_CURR, scanType, channel);
  if (retcode != RIG_OK) {
    Napi::Error::New(env, rigerror(retcode)).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  return Napi::Number::New(env, retcode);
}

Napi::Value NodeHamLib::StopScan(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  int retcode = rig_scan(my_rig, RIG_VFO_CURR, RIG_SCAN_STOP, 0);
  if (retcode != RIG_OK) {
    Napi::Error::New(env, rigerror(retcode)).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  return Napi::Number::New(env, retcode);
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
  
  int retcode = rig_set_level(my_rig, RIG_VFO_CURR, levelType, val);
  if (retcode != RIG_OK) {
    Napi::Error::New(env, rigerror(retcode)).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  return Napi::Number::New(env, retcode);
}

Napi::Value NodeHamLib::GetLevel(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected level type string").ThrowAsJavaScriptException();
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
  
  value_t val;
  int retcode = rig_get_level(my_rig, RIG_VFO_CURR, levelType, &val);
  if (retcode != RIG_OK) {
    Napi::Error::New(env, rigerror(retcode)).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  // Return the appropriate value based on the level type
  if (levelType == RIG_LEVEL_STRENGTH || levelType == RIG_LEVEL_RAWSTR) {
    return Napi::Number::New(env, val.i);
  } else {
    return Napi::Number::New(env, val.f);
  }
}

Napi::Value NodeHamLib::GetSupportedLevels(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  setting_t levels = my_rig->state.has_get_level | my_rig->state.has_set_level;
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
  
  int retcode = rig_set_func(my_rig, RIG_VFO_CURR, funcType, enable ? 1 : 0);
  if (retcode != RIG_OK) {
    Napi::Error::New(env, rigerror(retcode)).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  return Napi::Number::New(env, retcode);
}

Napi::Value NodeHamLib::GetFunction(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected function type string").ThrowAsJavaScriptException();
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
  
  int state;
  int retcode = rig_get_func(my_rig, RIG_VFO_CURR, funcType, &state);
  if (retcode != RIG_OK) {
    Napi::Error::New(env, rigerror(retcode)).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  return Napi::Boolean::New(env, state != 0);
}

Napi::Value NodeHamLib::GetSupportedFunctions(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  setting_t functions = my_rig->state.has_get_func | my_rig->state.has_set_func;
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
  
  int retcode = rig_set_split_freq(my_rig, RIG_VFO_CURR, tx_freq);
  if (retcode != RIG_OK) {
    Napi::Error::New(env, rigerror(retcode)).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  return Napi::Number::New(env, retcode);
}

Napi::Value NodeHamLib::GetSplitFreq(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  freq_t tx_freq;
  int retcode = rig_get_split_freq(my_rig, RIG_VFO_CURR, &tx_freq);
  if (retcode != RIG_OK) {
    Napi::Error::New(env, rigerror(retcode)).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  return Napi::Number::New(env, tx_freq);
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
  
  pbwidth_t tx_width = RIG_PASSBAND_NORMAL;
  if (info.Length() >= 2 && info[1].IsNumber()) {
    tx_width = info[1].As<Napi::Number>().Int32Value();
  }
  
  int retcode = rig_set_split_mode(my_rig, RIG_VFO_CURR, tx_mode, tx_width);
  if (retcode != RIG_OK) {
    Napi::Error::New(env, rigerror(retcode)).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  return Napi::Number::New(env, retcode);
}

Napi::Value NodeHamLib::GetSplitMode(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  rmode_t tx_mode;
  pbwidth_t tx_width;
  int retcode = rig_get_split_mode(my_rig, RIG_VFO_CURR, &tx_mode, &tx_width);
  if (retcode != RIG_OK) {
    Napi::Error::New(env, rigerror(retcode)).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  Napi::Object result = Napi::Object::New(env);
  result.Set("mode", Napi::String::New(env, rig_strrmode(tx_mode)));
  result.Set("width", Napi::Number::New(env, tx_width));
  return result;
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
  
  vfo_t tx_vfo = RIG_VFO_B;
  if (info.Length() >= 2 && info[1].IsString()) {
    std::string vfoStr = info[1].As<Napi::String>().Utf8Value();
    if (vfoStr == "VFO-A") {
      tx_vfo = RIG_VFO_A;
    } else if (vfoStr == "VFO-B") {
      tx_vfo = RIG_VFO_B;
    }
  }
  
  split_t split = split_enabled ? RIG_SPLIT_ON : RIG_SPLIT_OFF;
  int retcode = rig_set_split_vfo(my_rig, RIG_VFO_CURR, split, tx_vfo);
  if (retcode != RIG_OK) {
    Napi::Error::New(env, rigerror(retcode)).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  return Napi::Number::New(env, retcode);
}

Napi::Value NodeHamLib::GetSplit(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  split_t split;
  vfo_t tx_vfo;
  int retcode = rig_get_split_vfo(my_rig, RIG_VFO_CURR, &split, &tx_vfo);
  if (retcode != RIG_OK) {
    Napi::Error::New(env, rigerror(retcode)).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  Napi::Object result = Napi::Object::New(env);
  result.Set("enabled", Napi::Boolean::New(env, split == RIG_SPLIT_ON));
  
  std::string vfoStr = "VFO-CURR";
  if (tx_vfo == RIG_VFO_A) {
    vfoStr = "VFO-A";
  } else if (tx_vfo == RIG_VFO_B) {
    vfoStr = "VFO-B";
  }
  result.Set("txVfo", Napi::String::New(env, vfoStr));
  
  return result;
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
  
  int retcode = rig_vfo_op(my_rig, RIG_VFO_CURR, vfo_op);
  if (retcode != RIG_OK) {
    Napi::Error::New(env, rigerror(retcode)).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  return Napi::Number::New(env, retcode);
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
  value_t option = {0};  // Additional option parameter
  
  int retcode = rig_set_ant(my_rig, RIG_VFO_CURR, antenna, option);
  if (retcode != RIG_OK) {
    Napi::Error::New(env, rigerror(retcode)).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  return Napi::Number::New(env, retcode);
}

Napi::Value NodeHamLib::GetAntenna(const Napi::CallbackInfo & info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  ant_t antenna = RIG_ANT_CURR;
  value_t option;
  ant_t antenna_curr;
  ant_t antenna_tx;
  ant_t antenna_rx;
  
  int retcode = rig_get_ant(my_rig, RIG_VFO_CURR, antenna, &option, &antenna_curr, &antenna_tx, &antenna_rx);
  if (retcode != RIG_OK) {
    Napi::Error::New(env, rigerror(retcode)).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  return Napi::Number::New(env, antenna_curr);
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

      NodeHamLib::InstanceMethod("close", & NodeHamLib::Close),
      NodeHamLib::InstanceMethod("destroy", & NodeHamLib::Destroy),
      NodeHamLib::InstanceMethod("getConnectionInfo", & NodeHamLib::GetConnectionInfo),
      
      // Static method to get supported rig models
      NodeHamLib::StaticMethod("getSupportedRigs", & NodeHamLib::GetSupportedRigs),
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

// Serial Port Configuration Methods

// Set serial configuration parameter (data_bits, stop_bits, parity, handshake, etc.)
Napi::Value NodeHamLib::SetSerialConfig(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (info.Length() < 2 || !info[0].IsString() || !info[1].IsString()) {
    Napi::TypeError::New(env, "Expected parameter name and value as strings").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  std::string paramName = info[0].As<Napi::String>().Utf8Value();
  std::string paramValue = info[1].As<Napi::String>().Utf8Value();
  
  // Get configuration token from parameter name
  hamlib_token_t token = rig_token_lookup(my_rig, paramName.c_str());
  if (token == RIG_CONF_END) {
    Napi::Error::New(env, "Unknown configuration parameter").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  // Use rig_set_conf to set configuration parameter
  int retcode = rig_set_conf(my_rig, token, paramValue.c_str());
  if (retcode != RIG_OK) {
    Napi::Error::New(env, rigerror(retcode)).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  return Napi::Number::New(env, retcode);
}

// Get serial configuration parameter
Napi::Value NodeHamLib::GetSerialConfig(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (!rig_is_open) {
    Napi::TypeError::New(env, "Rig is not open!").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected parameter name as string").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  std::string paramName = info[0].As<Napi::String>().Utf8Value();
  char value[256];
  
  // Get configuration token from parameter name
  hamlib_token_t token = rig_token_lookup(my_rig, paramName.c_str());
  if (token == RIG_CONF_END) {
    Napi::Error::New(env, "Unknown configuration parameter").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  // Use rig_get_conf to get configuration parameter
  int retcode = rig_get_conf(my_rig, token, value);
  if (retcode != RIG_OK) {
    Napi::Error::New(env, rigerror(retcode)).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  return Napi::String::New(env, value);
}

// Set PTT type
Napi::Value NodeHamLib::SetPttType(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected PTT type as string").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  std::string pttTypeStr = info[0].As<Napi::String>().Utf8Value();
  
  // Get configuration token for ptt_type
  hamlib_token_t token = rig_token_lookup(my_rig, "ptt_type");
  if (token == RIG_CONF_END) {
    Napi::Error::New(env, "PTT type configuration not supported").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  // Set PTT type through configuration
  int retcode = rig_set_conf(my_rig, token, pttTypeStr.c_str());
  if (retcode != RIG_OK) {
    Napi::Error::New(env, rigerror(retcode)).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  return Napi::Number::New(env, retcode);
}

// Get PTT type
Napi::Value NodeHamLib::GetPttType(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  char value[256];
  
  // Get configuration token for ptt_type
  hamlib_token_t token = rig_token_lookup(my_rig, "ptt_type");
  if (token == RIG_CONF_END) {
    Napi::Error::New(env, "PTT type configuration not supported").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  // Get PTT type through configuration
  int retcode = rig_get_conf(my_rig, token, value);
  if (retcode != RIG_OK) {
    Napi::Error::New(env, rigerror(retcode)).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  return Napi::String::New(env, value);
}

// Set DCD type
Napi::Value NodeHamLib::SetDcdType(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected DCD type as string").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  std::string dcdTypeStr = info[0].As<Napi::String>().Utf8Value();
  
  // Get configuration token for dcd_type
  hamlib_token_t token = rig_token_lookup(my_rig, "dcd_type");
  if (token == RIG_CONF_END) {
    Napi::Error::New(env, "DCD type configuration not supported").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  // Set DCD type through configuration
  int retcode = rig_set_conf(my_rig, token, dcdTypeStr.c_str());
  if (retcode != RIG_OK) {
    Napi::Error::New(env, rigerror(retcode)).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  return Napi::Number::New(env, retcode);
}

// Get DCD type
Napi::Value NodeHamLib::GetDcdType(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  char value[256];
  
  // Get configuration token for dcd_type
  hamlib_token_t token = rig_token_lookup(my_rig, "dcd_type");
  if (token == RIG_CONF_END) {
    Napi::Error::New(env, "DCD type configuration not supported").ThrowAsJavaScriptException();
    return env.Null();
  }
  
  // Get DCD type through configuration
  int retcode = rig_get_conf(my_rig, token, value);
  if (retcode != RIG_OK) {
    Napi::Error::New(env, rigerror(retcode)).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  return Napi::String::New(env, value);
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

