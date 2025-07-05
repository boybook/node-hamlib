# node-hamlib

A Node.js wrapper for [Hamlib](https://hamlib.github.io/) - control amateur radio transceivers from JavaScript/TypeScript.

## 🚀 Features

- **Pre-built binaries**: No compilation needed for most platforms
- **Multi-platform support**: Windows x64, Linux x64/ARM64, macOS ARM64
- **CommonJS & ES Modules**: Works with both `require()` and `import`
- **TypeScript support**: Full type definitions included
- **Serial & Network**: Direct serial connection or network via rigctld
- **Comprehensive API**: All essential radio control functions

## 📦 Installation

```bash
npm install node-hamlib
```

The package will automatically:
1. Try to use pre-built binaries for your platform
2. Fall back to compiling from source if needed
3. Provide helpful error messages for missing dependencies

## 🔧 System Requirements

### For Pre-built Binaries
- Node.js 12.0.0 or higher
- Supported platforms: Windows x64, Linux x64/ARM64, macOS ARM64

### For Source Compilation
- All of the above, plus:
- **Linux**: `libhamlib-dev` package
- **macOS**: `hamlib` via Homebrew
- **Windows**: hamlib via vcpkg or manual installation
- **Build tools**: `node-gyp` and platform-specific compilers

### Install System Dependencies

```bash
# Ubuntu/Debian
sudo apt-get install libhamlib-dev libhamlib-utils

# macOS
brew install hamlib

# Windows (using vcpkg)
vcpkg install hamlib:x64-windows
```

## 📋 Usage

### CommonJS (Node.js)
```javascript
const { HamLib } = require('node-hamlib');

// Create instance with radio model and port
const rig = new HamLib(1035, '/dev/ttyUSB0');

// Open connection
rig.open();

// Control your radio
rig.setFrequency(144390000); // 144.39 MHz
rig.setMode('FM');
rig.setPtt(true);

// Get radio status
console.log('Current frequency:', rig.getFrequency());
console.log('Current mode:', rig.getMode());
console.log('Signal strength:', rig.getStrength());

// Close connection
rig.close();
```

### ES Modules (ESM)
```javascript
import { HamLib } from 'node-hamlib';

const rig = new HamLib(1035, '/dev/ttyUSB0');
// ... same API as above
```

### TypeScript
```typescript
import { HamLib, ConnectionInfo, ModeInfo } from 'node-hamlib';

const rig = new HamLib(1035, '/dev/ttyUSB0');
rig.open();

const info: ConnectionInfo = rig.getConnectionInfo();
const mode: ModeInfo = rig.getMode();
```

## 🔌 Connection Types

### Serial Connection
```javascript
// Linux/Raspberry Pi
const rig = new HamLib(1035, '/dev/ttyUSB0');

// macOS
const rig = new HamLib(1035, '/dev/cu.usbserial-1420');

// Windows
const rig = new HamLib(1035, 'COM3');
```

### Network Connection (rigctld)
```javascript
// Connect to rigctld daemon
const rig = new HamLib(1035, 'localhost:4532');
const rig = new HamLib(1035, '192.168.1.100:4532');
```

Start rigctld daemon:
```bash
# Basic usage
rigctld -m 1035 -r /dev/ttyUSB0

# With custom port
rigctld -m 1035 -r /dev/ttyUSB0 -t 5001

# With remote access
rigctld -m 1035 -r /dev/ttyUSB0 -T 0.0.0.0
```

## 🎛️ API Reference

### Constructor
- `new HamLib(model, port?)`: Create a new instance
  - `model`: Radio model number (find yours with `rigctl -l`)
  - `port`: Optional serial port or network address

### Connection Methods
- `open()`: Open connection to radio
- `close()`: Close connection (can reopen)
- `destroy()`: Destroy connection permanently

### Control Methods
- `setVfo(vfo)`: Set VFO ('VFO-A' or 'VFO-B')
- `setFrequency(freq, vfo?)`: Set frequency in Hz
- `setMode(mode, bandwidth?)`: Set mode ('USB', 'LSB', 'FM', etc.)
- `setPtt(state)`: Set PTT on/off

### Query Methods
- `getVfo()`: Get current VFO
- `getFrequency(vfo?)`: Get frequency in Hz
- `getMode()`: Get current mode and bandwidth
- `getStrength()`: Get signal strength
- `getConnectionInfo()`: Get connection details

## 🏗️ Development

### Building from Source
```bash
# Install dependencies
npm install

# Build native module
npm run build

# Run tests
npm test
```

### Testing the Module
```bash
# Test module loading
node test/test_loader.js

# Test basic functionality
node test/mod.js

# Test network connection
node test/test_network.js
```

## 📂 Project Structure

```
node-hamlib/
├── lib/                    # JavaScript wrapper code
│   ├── index.js           # Main CommonJS entry
│   ├── index.mjs          # ES Module entry
│   └── binary-loader.js   # Binary loading logic
├── src/                   # C++ source code
├── prebuilds/             # Pre-built binaries
├── scripts/               # Build and install scripts
├── test/                  # Test files
├── index.js               # Package entry point
├── index.d.ts             # TypeScript definitions
└── binding.gyp            # Build configuration
```

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## 📋 Supported Platforms

| Platform | Architecture | Status |
|----------|-------------|---------|
| Linux    | x64         | ✅ Pre-built (native) |
| Linux    | ARM64       | ✅ Pre-built (native) |
| macOS    | ARM64       | ✅ Pre-built (native) |
| Windows  | x64         | ✅ Pre-built (native) |

## 🐛 Troubleshooting

### Binary Loading Issues
If you see "Failed to load hamlib native module":

1. **Check platform support**: Ensure your platform is supported
2. **Install system dependencies**: Follow the installation guide above
3. **Try building from source**: `npm run build`
4. **Check permissions**: Ensure you have access to the serial port

### Serial Port Issues
```bash
# Linux: Add user to dialout group
sudo usermod -a -G dialout $USER

# Check available ports
ls /dev/tty*

# Test with rigctl
rigctl -m 1035 -r /dev/ttyUSB0 f
```

### Network Connection Issues
```bash
# Test rigctld connection
telnet localhost 4532

# In telnet session:
f  # get frequency
q  # quit
```

## 📄 License

This project is licensed under the LGPL license - see the [COPYING](COPYING) file for details.

## 🔗 Related Projects

- [Hamlib](https://hamlib.github.io/) - The underlying radio control library
- [rigctl](https://github.com/Hamlib/Hamlib) - Command-line radio control utility