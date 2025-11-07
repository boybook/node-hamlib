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
            "<!@(node -e \"if(process.env.HAMLIB_PREFIX) console.log(process.env.HAMLIB_PREFIX + '/include')\")",
            "/usr/include",
            "/usr/local/include"
          ],
          "libraries": [
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
            "/opt/homebrew/include"
          ],
          "libraries": [
            "<!@(node -e \"if(process.env.HAMLIB_PREFIX) console.log('-L' + process.env.HAMLIB_PREFIX + '/lib')\")",
            "-L/usr/local/lib",
            "-L/opt/homebrew/lib",
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
              "conditions": [
                # Check if we're in MinGW environment (HAMLIB_ROOT is set to /mingw64)
                ["\"<!(node -e \"console.log(process.env.HAMLIB_ROOT || '')\")\"==\"/mingw64\"", {
                  "include_dirs": [
                    "/mingw64/include"
                  ],
                  "library_dirs": [
                    "/mingw64/lib"
                  ],
                  "libraries": [
                    "-lhamlib",
                    "-lws2_32",
                    "-lwinmm"
                  ],
                  "cflags": [
                    "-I/mingw64/include"
                  ],
                  "ldflags": [
                    "-L/mingw64/lib",
                    "-static-libgcc",
                    "-static-libstdc++"
                  ]
                }, {
                  # Traditional Windows build with Visual C++
                  "include_dirs": [
                    # Try environment variable in different formats
                    "<!(node -e \"console.log((process.env.HAMLIB_ROOT || 'C:/hamlib') + '/include')\")",
                    # Fallback paths
                    "C:/hamlib/include",
                    "C:/Program Files/Hamlib/include", 
                    "C:/Program Files (x86)/Hamlib/include"
                  ],
                  "library_dirs": [
                    # Try environment variable paths
                    "<!(node -e \"console.log((process.env.HAMLIB_ROOT || 'C:/hamlib') + '/bin')\")",
                    "<!(node -e \"console.log((process.env.HAMLIB_ROOT || 'C:/hamlib') + '/lib')\")",
                    "<!(node -e \"console.log((process.env.HAMLIB_ROOT || 'C:/hamlib') + '/lib/gcc')\")",
                    "<!(node -e \"console.log((process.env.HAMLIB_ROOT || 'C:/hamlib') + '/lib/x64')\")",
                    "<!(node -e \"console.log((process.env.HAMLIB_ROOT || 'C:/hamlib') + '/lib/msvc')\")",
                    # Fallback paths
                    "C:/hamlib/bin",
                    "C:/hamlib/lib",
                    "C:/hamlib/lib/gcc",
                    "C:/hamlib/lib/x64",
                    "C:/hamlib/lib/Release",
                    "C:/Program Files/Hamlib/bin",
                    "C:/Program Files/Hamlib/lib",
                    "C:/Program Files/Hamlib/lib/gcc",
                    "C:/Program Files (x86)/Hamlib/bin",
                    "C:/Program Files (x86)/Hamlib/lib"
                  ],
                  "libraries": [
                    # Link against the import library present in hamlib-w64 zip
                    "libhamlib-4.lib",
                    # Common Win32 system libs used by hamlib
                    "Ws2_32.lib",
                    "Winmm.lib"
                  ],
                  "conditions": [
                    # Add pthread paths only if PTHREAD_ROOT is set
                    ["\"<!(node -e \"console.log(process.env.PTHREAD_ROOT || '')\")\"!=\"\"", {
                      "include_dirs": [
                        # Headers are placed directly under PTHREAD_ROOT to match this include path
                        "<!(node -e \"console.log((process.env.PTHREAD_ROOT || 'C:/pthread-win32'))\")"
                      ],
                      "library_dirs": [
                        "<!(node -e \"console.log((process.env.PTHREAD_ROOT || 'C:/pthread-win32') + '/lib/x64')\")",
                        "<!(node -e \"console.log((process.env.PTHREAD_ROOT || 'C:/pthread-win32') + '/lib')\")"
                      ],
                      "libraries": [
                        # Only link the MSVC import library we download from Sourceware
                        "pthreadVC2.lib"
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
            }]
          ]
        }]
      ]
    }
  ]
}
