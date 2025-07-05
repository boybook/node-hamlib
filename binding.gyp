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
            "/usr/include",
            "/usr/local/include"
          ],
          "libraries": [
            "-L/usr/lib",
            "-L/usr/local/lib",
            "-lhamlib"
          ]
        }],
        # macOS configuration
        ["OS==\"mac\"", {
          "include_dirs": [
            "/usr/local/include",
            "/opt/homebrew/include",
            "/opt/homebrew/Cellar/hamlib/4.6.2/include"
          ],
          "libraries": [
            "-L/usr/local/lib",
            "-L/opt/homebrew/lib",
            "-L/opt/homebrew/Cellar/hamlib/4.6.2/lib",
            "-lhamlib"
          ],
          "xcode_settings": {
            "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
            "CLANG_CXX_LIBRARY": "libc++",
            "MACOSX_DEPLOYMENT_TARGET": "10.15"
          }
        }],
        # Windows configuration
        ["OS==\"win\"", {
          "conditions": [
            ["target_arch==\"x64\"", {
              "include_dirs": [
                "$(VCPKG_ROOT)/installed/x64-windows/include",
                "C:/vcpkg/installed/x64-windows/include"
              ],
              "libraries": [
                "$(VCPKG_ROOT)/installed/x64-windows/lib/hamlib.lib",
                "C:/vcpkg/installed/x64-windows/lib/hamlib.lib"
              ]
            }]
          ],
          "msvs_settings": {
            "VCCLCompilerTool": {
              "ExceptionHandling": 1,
              "AdditionalOptions": ["/std:c++14"]
            }
          }
        }]
      ]
    }
  ]
}