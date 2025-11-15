#include <napi.h>
#include "hamlib.h"
#include "decoder.h"
#include <hamlib/rig.h>

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  // Set Hamlib debug level to NONE by default to prevent unwanted output
  // Users can change this using HamLib.setDebugLevel() if needed
  rig_set_debug(RIG_DEBUG_NONE);

  Napi::String name = Napi::String::New(env, "HamLib");
  exports.Set(name, NodeHamLib::GetClass(env));


  // Napi::String decoder_name = Napi::String::New(env, "Decoder");
  // exports.Set(decoder_name, Decoder::GetClass(env));

  return exports;
}

NODE_API_MODULE(radio, Init)