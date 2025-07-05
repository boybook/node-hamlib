# node-hamlib

A comprehensive Node.js wrapper for [Hamlib](https://hamlib.github.io/) - control amateur radio transceivers from JavaScript/TypeScript.

## 🚀 Features

- **Built-in Methods**: Complete radio control API including memory channels, RIT/XIT, scanning, levels, functions, and more
- **303+ Supported Radios**: Works with Yaesu, Icom, Kenwood, Elecraft, FlexRadio, and many more
- **Multi-platform**: Pre-built binaries for Windows x64, Linux x64/ARM64, macOS ARM64
- **Connection Types**: Serial ports, network (rigctld), and direct control
- **TypeScript Support**: Full type definitions and IntelliSense
- **Modern JavaScript**: CommonJS and ES Modules support

## 📦 Quick Start

```bash
npm install node-hamlib
```

```javascript
const { HamLib } = require('node-hamlib');

// Find your radio model
const rigs = HamLib.getSupportedRigs();
console.log('Supported radios:', rigs.length);

// Connect to radio
const rig = new HamLib(1035, '/dev/ttyUSB0'); // FT-991A
rig.open();

// Basic control
rig.setFrequency(144390000); // 144.39 MHz
rig.setMode('FM');
rig.setPtt(true);

// Get status
console.log('Frequency:', rig.getFrequency());
console.log('Mode:', rig.getMode());
console.log('Signal:', rig.getStrength());

rig.close();
```

## 📡 Complete API Reference

### 🔍 Radio Discovery

#### `HamLib.getSupportedRigs()`
Get all supported radio models
```javascript
const rigs = HamLib.getSupportedRigs();
console.log(`Found ${rigs.length} supported radios`);

// Find your radio
const yaesu = rigs.filter(r => r.mfgName === 'Yaesu');
const ft991a = rigs.find(r => r.modelName === 'FT-991A');
console.log('FT-991A Model ID:', ft991a.rigModel);
```

### 🔌 Connection Management

#### `new HamLib(model, port)`
Create radio instance
```javascript
// Serial connection
const rig = new HamLib(1035, '/dev/ttyUSB0');        // Linux
const rig = new HamLib(1035, '/dev/cu.usbserial-1420'); // macOS  
const rig = new HamLib(1035, 'COM3');                // Windows

// Network connection
const rig = new HamLib(1035, 'localhost:4532');      // rigctld
```

#### `open()` / `close()` / `destroy()`
Connection control
```javascript
rig.open();              // Open connection
rig.close();             // Close (can reopen)
rig.destroy();           // Destroy permanently
```

#### `getConnectionInfo()`
Get connection details
```javascript
const info = rig.getConnectionInfo();
console.log('Connection type:', info.connectionType);
console.log('Port:', info.port);
console.log('Status:', info.status);
```

### 📻 Basic Radio Control

#### Frequency Control
```javascript
// Set frequency (Hz)
rig.setFrequency(144390000);
rig.setFrequency(144390000, 'VFO-A');

// Get frequency
const freq = rig.getFrequency();
const freqA = rig.getFrequency('VFO-A');
```

#### Mode Control
```javascript
// Set mode
rig.setMode('FM');
rig.setMode('USB', 'wide');

// Get mode
const mode = rig.getMode();
console.log('Mode:', mode.mode);
console.log('Bandwidth:', mode.bandwidth);
```

#### VFO Control
```javascript
// Set VFO
rig.setVfo('VFO-A');
rig.setVfo('VFO-B');

// Get current VFO
const vfo = rig.getVfo();
```

#### PTT Control
```javascript
rig.setPtt(true);   // Transmit
rig.setPtt(false);  // Receive
```

#### Signal Monitoring
```javascript
const strength = rig.getStrength();
console.log('Signal strength:', strength);
```

### 💾 Memory Channel Management

#### `setMemoryChannel(channel, data)`
Store memory channel
```javascript
rig.setMemoryChannel(1, {
  frequency: 144390000,
  mode: 'FM',
  description: 'Local Repeater'
});
```

#### `getMemoryChannel(channel, readOnly)`
Retrieve memory channel
```javascript
const channel = rig.getMemoryChannel(1);
console.log('Channel 1:', channel);
```

#### `selectMemoryChannel(channel)`
Select memory channel
```javascript
rig.selectMemoryChannel(1);
```

### 🎛️ RIT/XIT Control

#### RIT (Receiver Incremental Tuning)
```javascript
rig.setRit(100);        // +100 Hz offset
const rit = rig.getRit();
```

#### XIT (Transmitter Incremental Tuning)
```javascript
rig.setXit(-50);        // -50 Hz offset
const xit = rig.getXit();
```

#### Clear RIT/XIT
```javascript
rig.clearRitXit();      // Clear both offsets
```

### 🔍 Scanning Operations

#### `startScan(type, channel)`
Start scanning
```javascript
rig.startScan('VFO');     // VFO scan
rig.startScan('MEM');     // Memory scan
rig.startScan('PROG');    // Program scan
```

#### `stopScan()`
Stop scanning
```javascript
rig.stopScan();
```

### 🎚️ Level Controls

#### `getSupportedLevels()`
Get available level controls
```javascript
const levels = rig.getSupportedLevels();
console.log('Available levels:', levels);
// ['AF', 'RF', 'SQL', 'RFPOWER', 'MICGAIN', ...]
```

#### `setLevel(type, value)` / `getLevel(type)`
Control audio and RF levels
```javascript
rig.setLevel('AF', 0.7);          // Audio gain 70%
rig.setLevel('RF', 0.5);          // RF gain 50%
rig.setLevel('SQL', 0.3);         // Squelch 30%
rig.setLevel('RFPOWER', 0.5);     // TX power 50%
rig.setLevel('MICGAIN', 0.8);     // Mic gain 80%

// Get levels
const audioGain = rig.getLevel('AF');
const rfGain = rig.getLevel('RF');
```

### 🔧 Function Controls

#### `getSupportedFunctions()`
Get available function controls
```javascript
const functions = rig.getSupportedFunctions();
console.log('Available functions:', functions);
// ['NB', 'COMP', 'VOX', 'TONE', 'TSQL', 'TUNER', ...]
```

#### `setFunction(type, enable)` / `getFunction(type)`
Control radio functions
```javascript
rig.setFunction('NB', true);      // Enable noise blanker
rig.setFunction('COMP', true);    // Enable compressor
rig.setFunction('VOX', true);     // Enable VOX
rig.setFunction('TUNER', true);   // Enable auto tuner

// Get function status
const nbEnabled = rig.getFunction('NB');
const compEnabled = rig.getFunction('COMP');
```

### 📡 Split Operations

#### Split Frequency
```javascript
rig.setSplitFreq(144340000);      // TX frequency
const txFreq = rig.getSplitFreq();
```

#### Split Mode
```javascript
rig.setSplitMode('FM', 'wide');   // TX mode
const txMode = rig.getSplitMode();
```

#### Split Control
```javascript
rig.setSplit(true, 'VFO-B');      // Enable split
const splitStatus = rig.getSplit();
```

### 📻 VFO Operations

#### `vfoOperation(operation)`
VFO operations
```javascript
rig.vfoOperation('CPY');      // Copy VFO A to B
rig.vfoOperation('XCHG');     // Exchange VFO A and B
rig.vfoOperation('UP');       // Frequency up
rig.vfoOperation('DOWN');     // Frequency down
rig.vfoOperation('TOGGLE');   // Toggle VFO A/B
```

### 📶 Antenna Selection

#### `setAntenna(antenna)` / `getAntenna()`
Antenna control
```javascript
rig.setAntenna(1);            // Select antenna 1
rig.setAntenna(2);            // Select antenna 2
const antenna = rig.getAntenna();
```

## 🎯 Complete Example

```javascript
const { HamLib } = require('node-hamlib');

async function radioControl() {
  // Find radios
  const rigs = HamLib.getSupportedRigs();
  const ft991a = rigs.find(r => r.modelName === 'FT-991A');
  
  if (!ft991a) {
    console.log('FT-991A not found');
    return;
  }
  
  // Connect
  const rig = new HamLib(ft991a.rigModel, '/dev/ttyUSB0');
  
  try {
    rig.open();
    console.log('Connected to', rig.getConnectionInfo().port);
    
    // Set up radio
    rig.setFrequency(144390000);
    rig.setMode('FM');
    rig.setLevel('RFPOWER', 0.5);
    rig.setFunction('NB', true);
    
    // Store memory channel
    rig.setMemoryChannel(1, {
      frequency: 144390000,
      mode: 'FM',
      description: 'Local Repeater'
    });
    
    // Monitor signal
    setInterval(() => {
      const freq = rig.getFrequency();
      const mode = rig.getMode();
      const strength = rig.getStrength();
      
      console.log(`${freq/1000000} MHz ${mode.mode} S:${strength}`);
    }, 1000);
    
  } catch (error) {
    console.error('Error:', error.message);
  } finally {
    rig.close();
  }
}

radioControl();
```

## 📋 Supported Radios

This library supports **303+ radio models** from major manufacturers:

| Manufacturer | Models | Popular Radios |
|--------------|--------|----------------|
| **Yaesu** | 46 | FT-991A, FT-891, FT-857D, FT-817ND |
| **Icom** | 83 | IC-7300, IC-9700, IC-705, IC-7610 |
| **Kenwood** | 35 | TS-2000, TS-590SG, TS-890S, TH-D74 |
| **Elecraft** | 7 | K3, K4, KX3, KX2 |
| **FlexRadio** | 10 | 6300, 6400, 6500, 6600, 6700 |
| **Others** | 122 | AOR, JRC, Ten-Tec, Winradio, etc. |

### Finding Your Radio
```javascript
const rigs = HamLib.getSupportedRigs();

// Search by manufacturer
const yaesu = rigs.filter(r => r.mfgName === 'Yaesu');

// Search by model name
const ft991a = rigs.find(r => r.modelName.includes('FT-991A'));

// List all radios
rigs.forEach(rig => {
  console.log(`${rig.rigModel}: ${rig.mfgName} ${rig.modelName}`);
});
```

## 🛠️ Installation & Setup

### System Requirements
- Node.js 12.0.0 or higher
- Supported platforms: Windows x64, Linux x64/ARM64, macOS ARM64

### Install Dependencies (if building from source)
```bash
# Ubuntu/Debian
sudo apt-get install libhamlib-dev

# macOS
brew install hamlib

# Windows
vcpkg install hamlib:x64-windows
```

### Building from Source
```bash
npm install
npm run build
```

### Testing
```bash
npm test
```

## 🔧 Connection Types

### Serial Connection
```javascript
// Linux
const rig = new HamLib(1035, '/dev/ttyUSB0');

// macOS
const rig = new HamLib(1035, '/dev/cu.usbserial-1420');

// Windows
const rig = new HamLib(1035, 'COM3');
```

### Network Connection (rigctld)
```javascript
const rig = new HamLib(1035, 'localhost:4532');
```

Start rigctld:
```bash
rigctld -m 1035 -r /dev/ttyUSB0 -t 4532
```

## 🐛 Troubleshooting

### Permission Issues (Linux)
```bash
sudo usermod -a -G dialout $USER
# Log out and back in
```

### Finding Serial Ports
```bash
# Linux
ls /dev/tty*

# macOS
ls /dev/cu.*

# Windows
# Use Device Manager
```

### Testing Connection
```bash
# Test with rigctl
rigctl -m 1035 -r /dev/ttyUSB0 f
```

## 📄 License

LGPL - see [COPYING](COPYING) file for details.

## 🔗 Links

- [Hamlib Project](https://hamlib.github.io/)
- [Supported Radios](https://github.com/Hamlib/Hamlib/wiki/Supported-Radios)
- [rigctl Documentation](https://hamlib.github.io/manpages/rigctl.html)