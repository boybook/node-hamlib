# node-hamlib

Control amateur radio transceivers from Node.js using the [Hamlib](https://hamlib.github.io/) library.

## Features

- **300+ Supported Radios** - Yaesu, Icom, Kenwood, Elecraft, FlexRadio, and more
- **Full Async/Promise API** - Non-blocking operations with async/await support  
- **Comprehensive Serial Control** - 13 parameters for complete serial port configuration
- **Multiple Connections** - Serial ports, network (rigctld), direct control
- **TypeScript Ready** - Complete type definitions included
- **Cross-platform** - Windows, Linux, macOS

## Installation

### Option 1: NPM Installation (Recommended)
```bash
npm install node-hamlib
```

The package will automatically use precompiled binaries if available for your platform, otherwise it will build from source.

### Option 2: Manual Prebuilds Installation

For faster installation or offline environments, you can manually install precompiled binaries:

1. **Download Prebuilds**: Go to [Releases](../../releases) and download `node-hamlib-prebuilds.zip`
2. **Extract**: Unzip to your project's `node_modules/node-hamlib/prebuilds/` directory
3. **Install**: Run `npm install node-hamlib --ignore-scripts`

**Supported Prebuilt Platforms:**
- ✅ Linux x64
- ✅ Linux ARM64
- ✅ macOS ARM64 (Apple Silicon)
- ✅ Windows x64

## Quick Start

```javascript
const { HamLib } = require('node-hamlib');

async function main() {
  // Create rig instance (model 1035 = FT-991A)
  const rig = new HamLib(1035, '/dev/ttyUSB0');
  
  await rig.open();
  
  // Basic operations
  await rig.setFrequency(144390000);  // 144.39 MHz
  await rig.setMode('FM');
  
  const freq = await rig.getFrequency();
  const mode = await rig.getMode();
  console.log(`${freq/1000000} MHz ${mode.mode}`);
  
  await rig.close();
}

main().catch(console.error);
```

## API Reference

### Connection

```javascript
// Find your radio model
const rigs = HamLib.getSupportedRigs();
const ft991a = rigs.find(r => r.modelName === 'FT-991A');

// Create connection
const rig = new HamLib(1035, '/dev/ttyUSB0');  // Serial
const rig = new HamLib(1035, 'localhost:4532'); // Network (rigctld)

await rig.open();
await rig.close();
```

### Basic Control

```javascript
// Frequency
await rig.setFrequency(14074000);           // 14.074 MHz
const freq = await rig.getFrequency();

// Mode
await rig.setMode('USB');
const mode = await rig.getMode();

// VFO
await rig.setVfo('VFO-A');
const vfo = await rig.getVfo();

// PTT
await rig.setPtt(true);                     // Transmit
const isTransmitting = await rig.getPtt();

// Signal
const strength = await rig.getStrength();
```

### Memory Channels

```javascript
// Store channel
await rig.setMemoryChannel(1, {
  frequency: 144390000,
  mode: 'FM',
  description: 'Local Repeater'
});

// Recall channel
const channel = await rig.getMemoryChannel(1);
await rig.selectMemoryChannel(1);
```

### Advanced Features

```javascript
// RIT/XIT offsets
await rig.setRit(100);               // +100 Hz RIT
await rig.setXit(-50);               // -50 Hz XIT
await rig.clearRitXit();             // Clear both

// Scanning
await rig.startScan('VFO');          // Start VFO scan
await rig.stopScan();                // Stop scan

// Levels (0.0-1.0)
await rig.setLevel('AF', 0.7);       // Audio 70%
await rig.setLevel('RFPOWER', 0.5);  // TX power 50%
const audioLevel = await rig.getLevel('AF');

// Functions
await rig.setFunction('NB', true);   // Noise blanker on
const voxEnabled = await rig.getFunction('VOX');

// Split operation
await rig.setSplit(true);            // Enable split
await rig.setSplitFreq(144340000);   // TX frequency

// VFO operations
await rig.vfoOperation('CPY');       // Copy VFO A to B
await rig.vfoOperation('TOGGLE');    // Toggle VFO A/B
```

### Power and Status

```javascript
// Power control
await rig.setPowerstat(1);           // Power on (0=off, 1=on, 2=standby)
const powerStatus = await rig.getPowerstat();

// Carrier detection
const carrierDetected = await rig.getDcd();  // Signal present?

// Tuning steps
await rig.setTuningStep(12500);      // 12.5 kHz steps
const step = await rig.getTuningStep();
```

### Repeater Operation

```javascript
// Set repeater shift
await rig.setRepeaterShift('PLUS');  // '+', '-', or 'NONE'
const shift = await rig.getRepeaterShift();

// Set offset frequency
await rig.setRepeaterOffset(600000); // 600 kHz for 2m
const offset = await rig.getRepeaterOffset();
```

### Serial Configuration

Node-hamlib provides **comprehensive serial port configuration** with **13 parameters** covering all aspects of serial communication from basic data format to advanced timing control and device-specific features.

**Important**: Serial configuration must be done **before** calling `rig.open()`.

```javascript
// Create rig instance
const rig = new HamLib(1035, '/dev/ttyUSB0');

// Configure serial parameters BEFORE opening connection
await rig.setSerialConfig('rate', '115200');           // Baud rate: 150 to 4000000 bps
await rig.setSerialConfig('data_bits', '8');           // Data bits: 5, 6, 7, 8
await rig.setSerialConfig('serial_parity', 'None');    // Parity: None, Even, Odd, Mark, Space
await rig.setSerialConfig('timeout', '1000');          // Timeout in milliseconds
await rig.setSerialConfig('write_delay', '10');        // Inter-byte delay (ms)

// PTT/DCD configuration (also before opening)
await rig.setPttType('DTR');                           // PTT: RIG, DTR, RTS, NONE, etc.
await rig.setDcdType('RIG');                           // DCD: RIG, DSR, CTS, NONE, etc.

// Now open the connection with configured settings
await rig.open();

// Read current settings (can be done anytime)
const rate = await rig.getSerialConfig('rate');
const parity = await rig.getSerialConfig('serial_parity');
```

#### Complete Serial Configuration Reference

| Category | Parameter | Description | Supported Values |
|----------|-----------|-------------|------------------|
| **Basic Serial** | `data_bits` | Number of data bits | `5`, `6`, `7`, `8` |
| | `stop_bits` | Number of stop bits | `1`, `2` |
| | `serial_parity` | Parity checking | `None`, `Even`, `Odd`, `Mark`, `Space` |
| | `serial_handshake` | Flow control | `None`, `Hardware`, `Software` |
| **Control Signals** | `rts_state` | RTS line state | `ON`, `OFF`, `UNSET` |
| | `dtr_state` | DTR line state | `ON`, `OFF`, `UNSET` |
| **Communication** | `rate` | Baud rate (bps) | `150` to `4000000` |
| | `timeout` | I/O timeout (ms) | Any non-negative integer |
| | `retry` | Max retry count | Any non-negative integer |
| **Timing** | `write_delay` | Inter-byte delay (ms) | Any non-negative integer |
| | `post_write_delay` | Inter-command delay (ms) | Any non-negative integer |
| **Advanced** | `flushx` | MicroHam flush mode | `true`, `false` |

## Complete Example

```javascript
const { HamLib } = require('node-hamlib');

async function repeaterOperation() {
  const rig = new HamLib(1035, '/dev/ttyUSB0');
  
  try {
    await rig.open();
    
    // Set up for 2m repeater
    await rig.setFrequency(145500000);      // 145.500 MHz
    await rig.setMode('FM');
    await rig.setRepeaterShift('MINUS');    // Negative offset
    await rig.setRepeaterOffset(600000);    // 600 kHz offset
    await rig.setTuningStep(12500);         // 12.5 kHz steps
    await rig.setLevel('RFPOWER', 0.5);     // 50% power
    
    // Save to memory
    await rig.setMemoryChannel(1, {
      frequency: 145500000,
      mode: 'FM',
      description: 'W1AW Repeater'
    });
    
    console.log('Setup complete for repeater operation');
    
  } catch (error) {
    console.error('Error:', error.message);
  } finally {
    await rig.close();
  }
}

repeaterOperation();
```

## Supported Radios

Over **300 radio models** supported, including:

| Manufacturer | Popular Models |
|--------------|----------------|
| **Yaesu** | FT-991A, FT-891, FT-857D, FT-817ND |
| **Icom** | IC-7300, IC-9700, IC-705, IC-7610 |
| **Kenwood** | TS-2000, TS-590SG, TS-890S |
| **Elecraft** | K3, K4, KX3, KX2 |
| **FlexRadio** | 6300, 6400, 6500, 6600, 6700 |

Find your radio model:
```javascript
const rigs = HamLib.getSupportedRigs();
console.log(rigs.find(r => r.modelName.includes('FT-991')));
```

## Connection Setup

### Serial Connection
```bash
# Linux/macOS
const rig = new HamLib(1035, '/dev/ttyUSB0');

# Windows
const rig = new HamLib(1035, 'COM3');
```

### Network Connection
```bash
# Start rigctld daemon
rigctld -m 1035 -r /dev/ttyUSB0 -t 4532

# Connect from Node.js
const rig = new HamLib(1035, 'localhost:4532');
```

## Troubleshooting

### Linux Permissions
```bash
sudo usermod -a -G dialout $USER
# Log out and log back in
```

### Find Serial Ports
```bash
# Linux
ls /dev/tty*

# macOS  
ls /dev/cu.*

# Test connection
rigctl -m 1035 -r /dev/ttyUSB0 f
```

## License

LGPL - see [COPYING](COPYING) file for details.

## Links

- [Hamlib Project](https://hamlib.github.io/)
- [Supported Radios](https://github.com/Hamlib/Hamlib/wiki/Supported-Radios)
- [rigctl Documentation](https://hamlib.github.io/manpages/rigctl.html)