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
            "/opt/homebrew/include"
          ],
          "libraries": [
            "-L/usr/local/lib",
            "-L/opt/homebrew/lib",
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
                    "<!(node -e \"console.log((process.env.HAMLIB_ROOT || 'C:/hamlib') + '/lib/x64')\")",
                    "<!(node -e \"console.log((process.env.HAMLIB_ROOT || 'C:/hamlib') + '/lib/msvc')\")",
                    # Fallback paths
                    "C:/hamlib/bin",
                    "C:/hamlib/lib",
                    "C:/hamlib/lib/x64",
                    "C:/hamlib/lib/Release",
                    "C:/Program Files/Hamlib/bin",
                    "C:/Program Files/Hamlib/lib",
                    "C:/Program Files (x86)/Hamlib/bin",
                    "C:/Program Files (x86)/Hamlib/lib"
                  ],
                  "libraries": [
                    # Try to link against the DLL import library
                    "libhamlib-4.dll.a",
                    "hamlib-4.dll.a", 
                    "libhamlib.dll.a",
                    "hamlib.dll.a",
                    # Fallback to static libraries if available
                    "libhamlib-4.lib",
                    "hamlib-4.lib",
                    "libhamlib.lib",
                    "hamlib.lib"
                  ],
                  "conditions": [
                    # Add pthread paths only if PTHREAD_ROOT is set
                    ["\"<!(node -e \"console.log(process.env.PTHREAD_ROOT || '')\")\"!=\"\"", {
                      "include_dirs": [
                        "<!(node -e \"console.log((process.env.PTHREAD_ROOT || 'C:/pthread-win32'))\")"
                      ],
                      "library_dirs": [
                        "<!(node -e \"console.log((process.env.PTHREAD_ROOT || 'C:/pthread-win32') + '/lib')\")",
                        "<!(node -e \"console.log((process.env.PTHREAD_ROOT || 'C:/pthread-win32') + '/lib/x64')\")"
                      ],
                      "libraries": [
                        "pthreadVC2.lib",
                        "pthreadVCE2.lib",
                        "pthreadVSE2.lib",
                        "pthread.lib"
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