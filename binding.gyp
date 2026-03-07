{
  "targets": [
    {
      "target_name": "hamlib",
      "sources": [
        "src/hamlib.cpp",
        "src/decoder.cpp",
        "src/addon.cpp"
      ],
      "include_dirs": [
        "include",
        "src/shim",
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "defines": [
        "NAPI_DISABLE_CPP_EXCEPTIONS"
      ],
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "conditions": [
        # Linux configuration
        ["OS==\"linux\"", {
          "include_dirs": [
            "<!@(node -e \"if(process.env.HAMLIB_PREFIX) console.log(process.env.HAMLIB_PREFIX + '/include')\")",
            "/usr/include",
            "/usr/local/include"
          ],
          "libraries": [
            "<(module_root_dir)/shim-build/libhamlib_shim.a",
            "<!@(node -e \"if(process.env.HAMLIB_PREFIX) console.log('-L' + process.env.HAMLIB_PREFIX + '/lib')\")",
            "-L/usr/lib",
            "-L/usr/local/lib",
            "-lhamlib"
          ],
          "ldflags": [
            "-Wl,-rpath,\\$ORIGIN"
          ]
        }],
        # macOS configuration
        ["OS==\"mac\"", {
          "include_dirs": [
            "<!@(node -e \"if(process.env.HAMLIB_PREFIX) console.log(process.env.HAMLIB_PREFIX + '/include')\")",
            "/usr/local/include",
            "/usr/local/opt/hamlib/include",
            "/usr/local/opt/libusb/include",
            "/opt/homebrew/include",
            "/opt/homebrew/opt/hamlib/include",
            "/opt/homebrew/opt/libusb/include"
          ],
          "libraries": [
            "<(module_root_dir)/shim-build/libhamlib_shim.a",
            "<!@(node -e \"if(process.env.HAMLIB_PREFIX) console.log('-L' + process.env.HAMLIB_PREFIX + '/lib')\")",
            "-L/usr/local/lib",
            "-L/usr/local/opt/hamlib/lib",
            "-L/usr/local/opt/libusb/lib",
            "-L/opt/homebrew/lib",
            "-L/opt/homebrew/opt/hamlib/lib",
            "-L/opt/homebrew/opt/libusb/lib",
            "-lhamlib"
          ],
          "xcode_settings": {
            "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
            "CLANG_CXX_LIBRARY": "libc++",
            "MACOSX_DEPLOYMENT_TARGET": "10.15"
          },
          "ldflags": [
            "-Wl,-rpath,@loader_path"
          ]
        }],
        # Windows configuration
        ["OS==\"win\"", {
          "defines": [
            "WIN32_LEAN_AND_MEAN",
            "_WIN32_WINNT=0x0600"
          ],
          "conditions": [
            ["target_arch==\"x64\"", {
              "library_dirs": [
                "<(module_root_dir)/shim-build"
              ],
              "libraries": [
                "hamlib_shim.lib",
                "Ws2_32.lib"
              ],
              "msvs_settings": {
                "VCCLCompilerTool": {
                  "ExceptionHandling": 1,
                  "AdditionalOptions": ["/std:c++14"]
                }
              }
            }]
          ]
        }]
      ]
    }
  ]
}
