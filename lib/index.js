const path = require('path');
const { EventEmitter } = require('events');
const nodeGypBuild = require('node-gyp-build');
// Ensure loader resolves from package root (contains prebuilds/ and build/)
const nativeModule = nodeGypBuild(path.join(__dirname, '..'));

/**
 * HamLib class for controlling amateur radio devices
 * 
 * This is a wrapper around the native hamlib module that provides
 * a consistent interface for controlling amateur radio devices.
 */
class HamLib extends EventEmitter {
  /**
   * Create a new HamLib instance
   * @param {number} model - Radio model number (run `rigctl -l` to find your device model)
   * @param {string} [port] - Optional port path or network address
   *   - Serial: '/dev/ttyUSB0' (Linux), '/dev/cu.usbserial-XXXX' (macOS), 'COM1' (Windows)
   *   - Network: 'localhost:4532' or '192.168.1.100:4532' (auto-switches to NETRIGCTL mode)
   */
  constructor(model, port) {
    super();
    this._nativeInstance = new nativeModule.HamLib(model, port);
  }

  /**
   * Get list of all supported radio models
   * @returns {Array} Array of supported radio models with details
   * @static
   */
  static getSupportedRigs() {
    return nativeModule.HamLib.getSupportedRigs();
  }

  /**
   * Get Hamlib library version information
   * @returns {string} Hamlib version string (e.g., "Hamlib 4.5.5 2024-01-15 12:00:00 64-bit")
   * @static
   */
  static getHamlibVersion() {
    return nativeModule.HamLib.getHamlibVersion();
  }

  /**
   * Set Hamlib debug level (affects all instances globally)
   * @param {number} level - Debug level:
   *   - 0 = NONE (no debug output)
   *   - 1 = BUG (serious bug messages)
   *   - 2 = ERR (error messages)
   *   - 3 = WARN (warning messages)
   *   - 4 = VERBOSE (verbose messages)
   *   - 5 = TRACE (trace messages - very detailed)
   * @static
   * @example
   * // Disable all debug output
   * HamLib.setDebugLevel(0);
   *
   * // Enable verbose debugging
   * HamLib.setDebugLevel(4);
   *
   * // Enable full trace debugging
   * HamLib.setDebugLevel(5);
   */
  static setDebugLevel(level) {
    return nativeModule.HamLib.setDebugLevel(level);
  }

  /**
   * Get current Hamlib debug level
   * Note: This method is not supported by Hamlib API and will throw an error.
   * Applications should track the debug level they set using setDebugLevel().
   * @static
   * @throws {Error} Always throws error as Hamlib doesn't provide API to query debug level
   */
  static getDebugLevel() {
    return nativeModule.HamLib.getDebugLevel();
  }

  /**
   * Open connection to the radio device
   * Must be called before other operations
   * @throws {Error} Throws error when connection fails
   */
  async open() {
    return this._nativeInstance.open();
  }

  /**
   * Set VFO (Variable Frequency Oscillator)
   * @param {string} vfo - VFO identifier ('VFOA' or 'VFOB')
   * @throws {Error} Throws error when device doesn't support or operation fails
   */
  async setVfo(vfo) {
    return this._nativeInstance.setVfo(vfo);
  }

  /**
   * Set frequency
   * @param {number} frequency - Frequency in hertz
   * @param {string} [vfo] - Optional VFO ('VFOA' or 'VFOB')
   */
  async setFrequency(frequency, vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.setFrequency(frequency, vfo);
    } else {
      return this._nativeInstance.setFrequency(frequency);
    }
  }

  /**
   * Set radio mode
   * @param {string} mode - Radio mode ('USB', 'LSB', 'FM', 'PKTFM', etc.)
   * @param {string} [bandwidth] - Optional bandwidth ('narrow', 'wide')
   * @param {string} [vfo] - Optional VFO ('VFOA' or 'VFOB')
   */
  async setMode(mode, bandwidth, vfo) {
    if (vfo !== undefined) {
      if (bandwidth !== undefined) {
        return this._nativeInstance.setMode(mode, bandwidth, vfo);
      } else {
        return this._nativeInstance.setMode(mode, vfo);
      }
    } else if (bandwidth !== undefined) {
      return this._nativeInstance.setMode(mode, bandwidth);
    } else {
      return this._nativeInstance.setMode(mode);
    }
  }

  /**
   * Set PTT (Push-to-Talk) status
   * @param {boolean} state - true to enable PTT, false to disable
   */
  async setPtt(state) {
    return this._nativeInstance.setPtt(state);
  }

  /**
   * Get current VFO
   * @returns {string} Current Hamlib VFO token
   */
  async getVfo() {
    return this._nativeInstance.getVfo();
  }

  /**
   * Get current frequency
   * @param {string} [vfo] - Optional VFO ('VFOA' or 'VFOB')
   * @returns {number} Current frequency in hertz
   */
  async getFrequency(vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.getFrequency(vfo);
    } else {
      return this._nativeInstance.getFrequency();
    }
  }

  /**
   * Get current radio mode
   * @returns {Object} Object containing mode and bandwidth information
   */
  async getMode() {
    return this._nativeInstance.getMode();
  }

  /**
   * Get current signal strength
   * @param {string} [vfo] - Optional VFO ('VFOA' or 'VFOB')
   * @returns {number} Signal strength value
   */
  async getStrength(vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.getStrength(vfo);
    } else {
      return this._nativeInstance.getStrength();
    }
  }

  /**
   * Close connection to device
   * Connection can be re-established by calling open()
   */
  async close() {
    return this._nativeInstance.close();
  }

  /**
   * Destroy connection to device
   * Object reference should be deleted after calling this
   */
  async destroy() {
    return this._nativeInstance.destroy();
  }

  /**
   * Get connection information
   * @returns {Object} Connection info including type, port, status, and model numbers
   */
  getConnectionInfo() {
    return this._nativeInstance.getConnectionInfo();
  }

  // Memory Channel Management
  
  /**
   * Set memory channel data
   * @param {number} channelNumber - Memory channel number
   * @param {Object} channelData - Channel data (frequency, mode, description, etc.)
   */
  async setMemoryChannel(channelNumber, channelData) {
    return this._nativeInstance.setMemoryChannel(channelNumber, channelData);
  }

  /**
   * Get memory channel data
   * @param {number} channelNumber - Memory channel number
   * @param {boolean} readOnly - Whether to read in read-only mode
   * @returns {Object} Channel data
   */
  async getMemoryChannel(channelNumber, readOnly) {
    return this._nativeInstance.getMemoryChannel(channelNumber, readOnly);
  }

  /**
   * Select memory channel for operation
   * @param {number} channelNumber - Memory channel number to select
   */
  async selectMemoryChannel(channelNumber) {
    return this._nativeInstance.selectMemoryChannel(channelNumber);
  }

  // RIT/XIT Control

  /**
   * Set RIT (Receiver Incremental Tuning) offset
   * @param {number} offsetHz - RIT offset in Hz
   */
  async setRit(offsetHz) {
    return this._nativeInstance.setRit(offsetHz);
  }

  /**
   * Get current RIT offset
   * @returns {number} RIT offset in Hz
   */
  async getRit() {
    return this._nativeInstance.getRit();
  }

  /**
   * Set XIT (Transmitter Incremental Tuning) offset
   * @param {number} offsetHz - XIT offset in Hz
   */
  async setXit(offsetHz) {
    return this._nativeInstance.setXit(offsetHz);
  }

  /**
   * Get current XIT offset
   * @returns {number} XIT offset in Hz
   */
  async getXit() {
    return this._nativeInstance.getXit();
  }

  /**
   * Clear both RIT and XIT offsets
   * @returns {boolean} Success status
   */
  async clearRitXit() {
    return this._nativeInstance.clearRitXit();
  }

  // Scanning Operations

  /**
   * Start scanning operation
   * @param {string} scanType - Scan type ('VFO', 'MEM', 'PROG', 'DELTA', 'PRIO')
   * @param {number} [channel=0] - Optional channel number for some scan types
   */
  async startScan(scanType, channel) {
    return this._nativeInstance.startScan(scanType, channel);
  }

  /**
   * Stop scanning operation
   */
  async stopScan() {
    return this._nativeInstance.stopScan();
  }

  // Level Controls

  /**
   * Set radio level (gain, volume, etc.)
   * @param {string} levelType - Level type ('AF', 'RF', 'SQL', 'RFPOWER', etc.)
   * @param {number} value - Level value (0.0-1.0 typically)
   */
  async setLevel(levelType, value, vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.setLevel(levelType, value, vfo);
    }
    return this._nativeInstance.setLevel(levelType, value);
  }

  /**
   * Get radio level
   * @param {string} levelType - Level type ('AF', 'RF', 'SQL', 'STRENGTH', etc.)
   * @returns {number} Level value
   */
  async getLevel(levelType, vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.getLevel(levelType, vfo);
    }
    return this._nativeInstance.getLevel(levelType);
  }

  /**
   * Get list of supported level types
   * @returns {Array<string>} Array of supported level types
   */
  getSupportedLevels() {
    return this._nativeInstance.getSupportedLevels();
  }

  // Function Controls

  /**
   * Set radio function on/off
   * @param {string} functionType - Function type ('NB', 'COMP', 'VOX', 'TONE', etc.)
   * @param {boolean} enable - true to enable, false to disable
   */
  async setFunction(functionType, enable) {
    return this._nativeInstance.setFunction(functionType, enable);
  }

  /**
   * Get radio function status
   * @param {string} functionType - Function type ('NB', 'COMP', 'VOX', 'TONE', etc.)
   * @returns {boolean} Function enabled status
   */
  async getFunction(functionType) {
    return this._nativeInstance.getFunction(functionType);
  }

  /**
   * Get list of supported function types
   * @returns {Array<string>} Array of supported function types
   */
  getSupportedFunctions() {
    return this._nativeInstance.getSupportedFunctions();
  }

  /**
   * Get list of supported radio modes
   * @returns {Array<string>} Array of supported mode strings
   * @example
   * const modes = rig.getSupportedModes();
   * console.log('Supported modes:', modes); // ['USB', 'LSB', 'CW', 'FM', 'AM', ...]
   */
  getSupportedModes() {
    return this._nativeInstance.getSupportedModes();
  }

  // Split Operations

  /**
   * Set split mode TX frequency
   * @param {number} txFrequency - TX frequency in Hz
   * @param {string} [vfo] - Optional VFO ('VFOA' or 'VFOB')
   */
  async setSplitFreq(txFrequency, vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.setSplitFreq(txFrequency, vfo);
    } else {
      return this._nativeInstance.setSplitFreq(txFrequency);
    }
  }

  /**
   * Get split mode TX frequency
   * @param {string} [vfo] - Optional VFO ('VFOA' or 'VFOB')
   * @returns {number} TX frequency in Hz
   */
  async getSplitFreq(vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.getSplitFreq(vfo);
    } else {
      return this._nativeInstance.getSplitFreq();
    }
  }

  /**
   * Set split mode TX mode
   * @param {string} txMode - TX mode ('USB', 'LSB', 'FM', etc.)
   * @param {number} [txWidth] - Optional TX bandwidth
   * @param {string} [vfo] - Optional VFO ('VFOA' or 'VFOB')
   */
  async setSplitMode(txMode, txWidth, vfo) {
    if (vfo !== undefined) {
      if (txWidth !== undefined) {
        return this._nativeInstance.setSplitMode(txMode, txWidth, vfo);
      } else {
        return this._nativeInstance.setSplitMode(txMode, vfo);
      }
    } else if (txWidth !== undefined) {
      return this._nativeInstance.setSplitMode(txMode, txWidth);
    } else {
      return this._nativeInstance.setSplitMode(txMode);
    }
  }

  /**
   * Get split mode TX mode
   * @param {string} [vfo] - Optional VFO ('VFOA' or 'VFOB')
   * @returns {Object} TX mode and width
   */
  async getSplitMode(vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.getSplitMode(vfo);
    } else {
      return this._nativeInstance.getSplitMode();
    }
  }

  /**
   * Enable/disable split operation
   * @param {boolean} enable - true to enable split, false to disable
   * @param {string} [rxVfo] - RX VFO ('VFOA' or 'VFOB'). Default: current VFO
   * @param {string} [txVfo] - TX VFO ('VFOA' or 'VFOB'). Default: current VFO
   */
  async setSplit(enable, rxVfo, txVfo) {
    if (txVfo !== undefined) {
      // 3-parameter version: setSplit(enable, rxVfo, txVfo) - matches Hamlib API order
      return this._nativeInstance.setSplit(enable, rxVfo, txVfo);
    } else if (rxVfo !== undefined) {
      // 2-parameter version: setSplit(enable, rxVfo)
      return this._nativeInstance.setSplit(enable, rxVfo);
    } else {
      // 1-parameter version: setSplit(enable)
      return this._nativeInstance.setSplit(enable);
    }
  }

  /**
   * Get split operation status
   * @param {string} [vfo] - Optional VFO ('VFOA' or 'VFOB')
   * @returns {Object} Split status and TX VFO
   */
  async getSplit(vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.getSplit(vfo);
    } else {
      return this._nativeInstance.getSplit();
    }
  }

  // VFO Operations

  /**
   * Perform VFO operation
   * @param {string} operation - VFO operation ('CPY', 'XCHG', 'FROM_VFO', 'TO_VFO', 'MCL', 'UP', 'DOWN', 'BAND_UP', 'BAND_DOWN', 'LEFT', 'RIGHT', 'TUNE', 'TOGGLE')
   */
  async vfoOperation(operation) {
    return this._nativeInstance.vfoOperation(operation);
  }

  // Antenna Selection

  // Note: setAntenna and getAntenna are defined later in the file with full VFO support

  // Serial Port Configuration

  /**
   * Set serial port configuration parameter
   * 
   * **IMPORTANT: Must be called BEFORE rig.open()**
   * Serial configuration cannot be changed after connection is established.
   * 
   * @param {string} paramName - Parameter name: 
   *   - Serial settings: 'data_bits', 'stop_bits', 'serial_parity', 'serial_handshake'
   *   - Control signals: 'rts_state', 'dtr_state'
   *   - Communication: 'rate', 'timeout', 'retry'
   *   - Timing: 'write_delay', 'post_write_delay'
   *   - Advanced: 'flushx'
   * @param {string} paramValue - Parameter value
   * @example
   * // Configure serial settings BEFORE opening connection
   * const rig = new HamLib(1035, '/dev/ttyUSB0');
   * 
   * // Basic serial settings
   * await rig.setSerialConfig('data_bits', '8');
   * await rig.setSerialConfig('rate', '115200');
   * await rig.setSerialConfig('serial_parity', 'None');
   * 
   * // Now open with configured settings
   * await rig.open();
   */
  async setSerialConfig(paramName, paramValue) {
    return this._nativeInstance.setSerialConfig(paramName, paramValue);
  }

  /**
   * Get serial port configuration parameter
   * @param {string} paramName - Parameter name to retrieve (same as setSerialConfig)
   * @returns {string} Parameter value
   * @example
   * // Get basic serial settings
   * const dataBits = hamlib.getSerialConfig('data_bits');
   * const parity = hamlib.getSerialConfig('serial_parity');
   * const handshake = hamlib.getSerialConfig('serial_handshake');
   * 
   * // Get communication settings
   * const rate = hamlib.getSerialConfig('rate');
   * const timeout = hamlib.getSerialConfig('timeout');
   * 
   * // Get control signals state
   * const rtsState = hamlib.getSerialConfig('rts_state');
   * const dtrState = hamlib.getSerialConfig('dtr_state');
   * 
   * // Get timing settings
   * const writeDelay = hamlib.getSerialConfig('write_delay');
   * const flushx = hamlib.getSerialConfig('flushx');
   */
  async getSerialConfig(paramName) {
    return this._nativeInstance.getSerialConfig(paramName);
  }

  /**
   * Set PTT (Push-to-Talk) type
   * @param {string} pttType - PTT type ('RIG', 'DTR', 'RTS', 'PARALLEL', 'CM108', 'GPIO', 'GPION', 'NONE')
   * @example
   * // Use DTR line for PTT
   * hamlib.setPttType('DTR');
   * 
   * // Use RTS line for PTT
   * hamlib.setPttType('RTS');
   * 
   * // Use CAT command for PTT
   * hamlib.setPttType('RIG');
   */
  async setPttType(pttType) {
    return this._nativeInstance.setPttType(pttType);
  }

  /**
   * Get current PTT type
   * @returns {string} Current PTT type
   */
  async getPttType() {
    return this._nativeInstance.getPttType();
  }

  /**
   * Set DCD (Data Carrier Detect) type
   * @param {string} dcdType - DCD type ('RIG', 'DSR', 'CTS', 'CD', 'PARALLEL', 'CM108', 'GPIO', 'GPION', 'NONE')
   * @example
   * // Use DSR line for DCD
   * hamlib.setDcdType('DSR');
   * 
   * // Use CTS line for DCD
   * hamlib.setDcdType('CTS');
   * 
   * // Use CAT command for DCD
   * hamlib.setDcdType('RIG');
   */
  async setDcdType(dcdType) {
    return this._nativeInstance.setDcdType(dcdType);
  }

  /**
   * Get current DCD type
   * @returns {string} Current DCD type
   */
  async getDcdType() {
    return this._nativeInstance.getDcdType();
  }

  /**
   * Get supported serial configuration options
   * @returns {Object} Object containing all supported configuration parameters and their possible values
   * @example
   * const configs = hamlib.getSupportedSerialConfigs();
   * console.log('Supported data bits:', configs.serial.data_bits);
   * console.log('Supported PTT types:', configs.ptt_type);
   */
  getSupportedSerialConfigs() {
    return this._nativeInstance.getSupportedSerialConfigs();
  }

  // Power Control

  /**
   * Set radio power status
   * @param {number} status - Power status (0=OFF, 1=ON, 2=STANDBY, 4=OPERATE, 8=UNKNOWN)
   * @returns {Promise<number>} Success status
   * @example
   * await rig.setPowerstat(1); // Power on
   * await rig.setPowerstat(0); // Power off
   * await rig.setPowerstat(2); // Standby mode
   */
  async setPowerstat(status) {
    return this._nativeInstance.setPowerstat(status);
  }

  /**
   * Get current radio power status
   * @returns {Promise<number>} Power status (0=OFF, 1=ON, 2=STANDBY, 4=OPERATE, 8=UNKNOWN)
   * @example
   * const powerStatus = await rig.getPowerstat();
   * if (powerStatus === 1) {
   *   console.log('Radio is powered on');
   * }
   */
  async getPowerstat() {
    return this._nativeInstance.getPowerstat();
  }

  // PTT Status Detection

  /**
   * Get current PTT status
   * @param {string} [vfo] - Optional VFO to get PTT status from ('VFOA' or 'VFOB'). If not specified, uses current VFO
   * @returns {Promise<boolean>} PTT status (true if transmitting, false if receiving)
   * @example
   * const isTransmitting = await rig.getPtt();
   * if (isTransmitting) {
   *   console.log('Radio is transmitting');
   * }
   */
  async getPtt(vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.getPtt(vfo);
    } else {
      return this._nativeInstance.getPtt();
    }
  }

  // Data Carrier Detect

  /**
   * Get current DCD (Data Carrier Detect) status
   * @param {string} [vfo] - Optional VFO to get DCD status from ('VFOA' or 'VFOB'). If not specified, uses current VFO
   * @returns {Promise<boolean>} DCD status (true if carrier detected, false if no carrier)
   * @example
   * const carrierDetected = await rig.getDcd();
   * if (carrierDetected) {
   *   console.log('Carrier detected');
   * }
   */
  async getDcd(vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.getDcd(vfo);
    } else {
      return this._nativeInstance.getDcd();
    }
  }

  // Tuning Step Control

  /**
   * Set tuning step
   * @param {number} step - Tuning step in Hz
   * @param {string} [vfo] - Optional VFO to set tuning step on ('VFOA' or 'VFOB'). If not specified, uses current VFO
   * @returns {Promise<number>} Success status
   * @example
   * await rig.setTuningStep(25000); // 25 kHz steps
   * await rig.setTuningStep(12500); // 12.5 kHz steps
   * await rig.setTuningStep(100);   // 100 Hz steps for HF
   */
  async setTuningStep(step, vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.setTuningStep(step, vfo);
    } else {
      return this._nativeInstance.setTuningStep(step);
    }
  }

  /**
   * Get current tuning step
   * @param {string} [vfo] - Optional VFO to get tuning step from ('VFOA' or 'VFOB'). If not specified, uses current VFO
   * @returns {Promise<number>} Current tuning step in Hz
   * @example
   * const step = await rig.getTuningStep();
   * console.log(`Current tuning step: ${step} Hz`);
   */
  async getTuningStep(vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.getTuningStep(vfo);
    } else {
      return this._nativeInstance.getTuningStep();
    }
  }

  // Repeater Control

  /**
   * Set repeater shift direction
   * @param {string} shift - Repeater shift direction ('NONE', 'MINUS', 'PLUS', '-', '+')
   * @param {string} [vfo] - Optional VFO to set repeater shift on ('VFOA' or 'VFOB'). If not specified, uses current VFO
   * @returns {Promise<number>} Success status
   * @example
   * await rig.setRepeaterShift('PLUS');  // Positive shift (+)
   * await rig.setRepeaterShift('MINUS'); // Negative shift (-)
   * await rig.setRepeaterShift('NONE');  // No shift (simplex)
   */
  async setRepeaterShift(shift, vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.setRepeaterShift(shift, vfo);
    } else {
      return this._nativeInstance.setRepeaterShift(shift);
    }
  }

  /**
   * Get current repeater shift direction
   * @param {string} [vfo] - Optional VFO to get repeater shift from ('VFOA' or 'VFOB'). If not specified, uses current VFO
   * @returns {Promise<string>} Current repeater shift direction
   * @example
   * const shift = await rig.getRepeaterShift();
   * console.log(`Repeater shift: ${shift}`); // 'None', 'Minus', or 'Plus'
   */
  async getRepeaterShift(vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.getRepeaterShift(vfo);
    } else {
      return this._nativeInstance.getRepeaterShift();
    }
  }

  /**
   * Set repeater offset frequency
   * @param {number} offset - Repeater offset in Hz
   * @param {string} [vfo] - Optional VFO to set repeater offset on ('VFOA' or 'VFOB'). If not specified, uses current VFO
   * @returns {Promise<number>} Success status
   * @example
   * await rig.setRepeaterOffset(600000);  // 600 kHz offset (2m band)
   * await rig.setRepeaterOffset(5000000); // 5 MHz offset (70cm band)
   */
  async setRepeaterOffset(offset, vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.setRepeaterOffset(offset, vfo);
    } else {
      return this._nativeInstance.setRepeaterOffset(offset);
    }
  }

  /**
   * Get current repeater offset frequency
   * @param {string} [vfo] - Optional VFO to get repeater offset from ('VFOA' or 'VFOB'). If not specified, uses current VFO
   * @returns {Promise<number>} Current repeater offset in Hz
   * @example
   * const offset = await rig.getRepeaterOffset();
   * console.log(`Repeater offset: ${offset} Hz`);
   */
  async getRepeaterOffset(vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.getRepeaterOffset(vfo);
    } else {
      return this._nativeInstance.getRepeaterOffset();
    }
  }

  // CTCSS/DCS Tone Control

  /**
   * Set CTCSS tone frequency
   * @param {number} tone - CTCSS tone frequency in tenths of Hz (e.g., 1000 for 100.0 Hz)
   * @param {string} [vfo] - Optional VFO ('VFOA' or 'VFOB')
   */
  async setCtcssTone(tone, vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.setCtcssTone(tone, vfo);
    } else {
      return this._nativeInstance.setCtcssTone(tone);
    }
  }

  /**
   * Get current CTCSS tone frequency
   * @param {string} [vfo] - Optional VFO ('VFOA' or 'VFOB')
   * @returns {number} Current CTCSS tone frequency in tenths of Hz
   */
  async getCtcssTone(vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.getCtcssTone(vfo);
    } else {
      return this._nativeInstance.getCtcssTone();
    }
  }

  /**
   * Set DCS code
   * @param {number} code - DCS code (e.g., 23, 174, 754)
   * @param {string} [vfo] - Optional VFO ('VFOA' or 'VFOB')
   */
  async setDcsCode(code, vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.setDcsCode(code, vfo);
    } else {
      return this._nativeInstance.setDcsCode(code);
    }
  }

  /**
   * Get current DCS code
   * @param {string} [vfo] - Optional VFO ('VFOA' or 'VFOB')
   * @returns {number} Current DCS code
   */
  async getDcsCode(vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.getDcsCode(vfo);
    } else {
      return this._nativeInstance.getDcsCode();
    }
  }

  /**
   * Set CTCSS SQL (squelch) tone frequency
   * @param {number} tone - CTCSS SQL tone frequency in tenths of Hz (e.g., 1000 for 100.0 Hz)
   * @param {string} [vfo] - Optional VFO ('VFOA' or 'VFOB')
   */
  async setCtcssSql(tone, vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.setCtcssSql(tone, vfo);
    } else {
      return this._nativeInstance.setCtcssSql(tone);
    }
  }

  /**
   * Get current CTCSS SQL tone frequency
   * @param {string} [vfo] - Optional VFO ('VFOA' or 'VFOB')
   * @returns {number} Current CTCSS SQL tone frequency in tenths of Hz
   */
  async getCtcssSql(vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.getCtcssSql(vfo);
    } else {
      return this._nativeInstance.getCtcssSql();
    }
  }

  /**
   * Set DCS SQL (squelch) code
   * @param {number} code - DCS SQL code (e.g., 23, 174, 754)
   * @param {string} [vfo] - Optional VFO ('VFOA' or 'VFOB')
   */
  async setDcsSql(code, vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.setDcsSql(code, vfo);
    } else {
      return this._nativeInstance.setDcsSql(code);
    }
  }

  /**
   * Get current DCS SQL code
   * @param {string} [vfo] - Optional VFO ('VFOA' or 'VFOB')
   * @returns {number} Current DCS SQL code
   */
  async getDcsSql(vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.getDcsSql(vfo);
    } else {
      return this._nativeInstance.getDcsSql();
    }
  }

  // Parameter Control

  /**
   * Set radio parameter
   * @param {string} paramName - Parameter name (e.g., 'ANN', 'APO', 'BACKLIGHT', 'BEEP', 'TIME', 'BAT')
   * @param {number} value - Parameter value (numeric)
   */
  async setParm(paramName, value) {
    return this._nativeInstance.setParm(paramName, value);
  }

  /**
   * Get radio parameter value
   * @param {string} paramName - Parameter name (e.g., 'ANN', 'APO', 'BACKLIGHT', 'BEEP', 'TIME', 'BAT')
   * @returns {number} Parameter value
   */
  async getParm(paramName) {
    return this._nativeInstance.getParm(paramName);
  }

  // DTMF Support

  /**
   * Send DTMF digits
   * @param {string} digits - DTMF digits to send (0-9, A-D, *, #)
   * @param {string} [vfo] - Optional VFO ('VFOA' or 'VFOB')
   */
  async sendDtmf(digits, vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.sendDtmf(digits, vfo);
    } else {
      return this._nativeInstance.sendDtmf(digits);
    }
  }

  /**
   * Receive DTMF digits
   * @param {number} maxLength - Maximum number of digits to receive
   * @param {string} [vfo] - Optional VFO ('VFOA' or 'VFOB')
   * @returns {Object} Object containing digits and length
   */
  async recvDtmf(maxLength, vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.recvDtmf(maxLength, vfo);
    } else {
      return this._nativeInstance.recvDtmf(maxLength);
    }
  }

  // Memory Channel Advanced Operations

  /**
   * Get current memory channel number
   * @param {string} [vfo] - Optional VFO ('VFOA' or 'VFOB')
   * @returns {number} Current memory channel number
   */
  async getMem(vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.getMem(vfo);
    } else {
      return this._nativeInstance.getMem();
    }
  }

  /**
   * Set memory bank
   * @param {number} bank - Bank number to select
   * @param {string} [vfo] - Optional VFO ('VFOA' or 'VFOB')
   */
  async setBank(bank, vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.setBank(bank, vfo);
    } else {
      return this._nativeInstance.setBank(bank);
    }
  }

  /**
   * Get total number of memory channels available
   * @returns {number} Total number of memory channels
   */
  async memCount() {
    return this._nativeInstance.memCount();
  }

  // Morse Code Support

  /**
   * Send Morse code message
   * @param {string} message - Morse code message to send (text will be converted to Morse)
   * @param {string} [vfo] - Optional VFO ('VFOA' or 'VFOB')
   */
  async sendMorse(message, vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.sendMorse(message, vfo);
    } else {
      return this._nativeInstance.sendMorse(message);
    }
  }

  /**
   * Stop current Morse code transmission
   * @param {string} [vfo] - Optional VFO ('VFOA' or 'VFOB')
   */
  async stopMorse(vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.stopMorse(vfo);
    } else {
      return this._nativeInstance.stopMorse();
    }
  }

  /**
   * Wait for Morse code transmission to complete
   * @param {string} [vfo] - Optional VFO ('VFOA' or 'VFOB')
   */
  async waitMorse(vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.waitMorse(vfo);
    } else {
      return this._nativeInstance.waitMorse();
    }
  }

  // Voice Memory Support

  /**
   * Play voice memory channel
   * @param {number} channel - Voice memory channel number to play
   * @param {string} [vfo] - Optional VFO ('VFOA' or 'VFOB')
   */
  async sendVoiceMem(channel, vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.sendVoiceMem(channel, vfo);
    } else {
      return this._nativeInstance.sendVoiceMem(channel);
    }
  }

  /**
   * Stop voice memory playback
   * @param {string} [vfo] - Optional VFO ('VFOA' or 'VFOB')
   */
  async stopVoiceMem(vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.stopVoiceMem(vfo);
    } else {
      return this._nativeInstance.stopVoiceMem();
    }
  }

  // Complex Split Frequency/Mode Operations

  /**
   * Set split frequency and mode simultaneously
   * @param {number} txFrequency - TX frequency in Hz
   * @param {string} txMode - TX mode ('USB', 'LSB', 'FM', etc.)
   * @param {number} txWidth - TX bandwidth in Hz
   * @param {string} [vfo] - Optional VFO ('VFOA' or 'VFOB')
   */
  async setSplitFreqMode(txFrequency, txMode, txWidth, vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.setSplitFreqMode(txFrequency, txMode, txWidth, vfo);
    } else {
      return this._nativeInstance.setSplitFreqMode(txFrequency, txMode, txWidth);
    }
  }

  /**
   * Get split frequency and mode information
   * @param {string} [vfo] - Optional VFO ('VFOA' or 'VFOB')
   * @returns {Object} Object containing TX frequency, mode, and width
   */
  async getSplitFreqMode(vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.getSplitFreqMode(vfo);
    } else {
      return this._nativeInstance.getSplitFreqMode();
    }
  }

  // Antenna Control

  /**
   * Set antenna selection
   * @param {number} antenna - Antenna number to select (1-based)
   * @param {string} [vfo] - Optional VFO ('VFOA' or 'VFOB')
   * @returns {Promise<number>} Success status
   * @example
   * await rig.setAntenna(1); // Select antenna 1
   * await rig.setAntenna(2); // Select antenna 2
   */
  async setAntenna(antenna, vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.setAntenna(antenna, vfo);
    } else {
      return this._nativeInstance.setAntenna(antenna);
    }
  }

  /**
   * Get comprehensive antenna information
   * @param {string} [vfo] - Optional VFO ('VFOA' or 'VFOB')
   * @returns {Promise<Object>} Antenna information object containing:
   *   - currentAntenna: Currently selected antenna
   *   - txAntenna: TX antenna selection
   *   - rxAntenna: RX antenna selection  
   *   - option: Additional antenna option/parameter
   * @example
   * const antennaInfo = await rig.getAntenna();
   * console.log(`Current: ${antennaInfo.currentAntenna}, TX: ${antennaInfo.txAntenna}, RX: ${antennaInfo.rxAntenna}`);
   */
  async getAntenna(vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.getAntenna(vfo);
    } else {
      return this._nativeInstance.getAntenna();
    }
  }

  // Power Conversion Functions

  /**
   * Convert power level to milliwatts
   * @param {number} power - Power level (0.0-1.0)
   * @param {number} frequency - Frequency in Hz
   * @param {string} mode - Radio mode ('USB', 'LSB', 'FM', etc.)
   * @returns {Promise<number>} Power in milliwatts
   * @example
   * const milliwatts = await rig.power2mW(0.5, 14205000, 'USB');
   * console.log(`50% power = ${milliwatts} mW`);
   */
  async power2mW(power, frequency, mode) {
    return this._nativeInstance.power2mW(power, frequency, mode);
  }

  /**
   * Convert milliwatts to power level
   * @param {number} milliwatts - Power in milliwatts
   * @param {number} frequency - Frequency in Hz
   * @param {string} mode - Radio mode ('USB', 'LSB', 'FM', etc.)
   * @returns {Promise<number>} Power level (0.0-1.0)
   * @example
   * const powerLevel = await rig.mW2power(5000, 14205000, 'USB');
   * console.log(`5000 mW = ${(powerLevel * 100).toFixed(1)}% power`);
   */
  async mW2power(milliwatts, frequency, mode) {
    return this._nativeInstance.mW2power(milliwatts, frequency, mode);
  }

  // Reset Function

  /**
   * Reset radio to default state
   * @param {string} resetType - Reset type ('NONE', 'SOFT', 'VFO', 'MCALL', 'MASTER')
   */
  async reset(resetType) {
    return this._nativeInstance.reset(resetType);
  }

  // Rig Info (async)

  /**
   * Get rig identification info (model, firmware version, etc.)
   * @returns {Promise<string>} Rig info string
   */
  async getInfo() {
    return this._nativeInstance.getInfo();
  }

  /**
   * Send raw command bytes and receive reply
   * @param {Buffer} data - Raw command data to send
   * @param {number} replyMaxLen - Maximum reply length
   * @param {Buffer} [terminator] - Optional terminator bytes
   * @returns {Promise<Buffer>} Reply data
   */
  async sendRaw(data, replyMaxLen, terminator) {
    if (terminator !== undefined) {
      return this._nativeInstance.sendRaw(data, replyMaxLen, terminator);
    }
    return this._nativeInstance.sendRaw(data, replyMaxLen);
  }

  /**
   * Get official Hamlib spectrum capability metadata from the backend.
   * @returns {Promise<Object>} Spectrum capability object.
   */
  async getSpectrumCapabilities() {
    return this._nativeInstance.getSpectrumCapabilities();
  }

  /**
   * Start the official Hamlib spectrum callback stream.
   * @param {(line: Object) => void} [callback]
   * @returns {Promise<boolean>}
   */
  async startSpectrumStream(callback) {
    const listener = typeof callback === 'function'
      ? callback
      : (line) => this.emit('spectrumLine', line);
    return this._nativeInstance.startSpectrumStream(listener);
  }

  /**
   * Stop the official Hamlib spectrum callback stream.
   * @returns {Promise<boolean>}
   */
  async stopSpectrumStream() {
    return this._nativeInstance.stopSpectrumStream();
  }

  /**
   * Set a backend configuration parameter
   * @param {string} name - Configuration token name
   * @param {string} value - Configuration value
   * @returns {Promise<number>} Success status
   */
  async setConf(name, value) {
    return this._nativeInstance.setConf(name, value);
  }

  /**
   * Get a backend configuration parameter
   * @param {string} name - Configuration token name
   * @returns {Promise<string>} Configuration value
   */
  async getConf(name) {
    return this._nativeInstance.getConf(name);
  }

  // Passband / Resolution (sync)

  /**
   * Get normal passband width for a given mode
   * @param {string} mode - Radio mode ('USB', 'LSB', 'FM', etc.)
   * @returns {number} Normal passband width in Hz
   */
  getPassbandNormal(mode) {
    return this._nativeInstance.getPassbandNormal(mode);
  }

  /**
   * Get narrow passband width for a given mode
   * @param {string} mode - Radio mode ('USB', 'LSB', 'FM', etc.)
   * @returns {number} Narrow passband width in Hz
   */
  getPassbandNarrow(mode) {
    return this._nativeInstance.getPassbandNarrow(mode);
  }

  /**
   * Get wide passband width for a given mode
   * @param {string} mode - Radio mode ('USB', 'LSB', 'FM', etc.)
   * @returns {number} Wide passband width in Hz
   */
  getPassbandWide(mode) {
    return this._nativeInstance.getPassbandWide(mode);
  }

  /**
   * Get frequency resolution (step) for a given mode
   * @param {string} mode - Radio mode ('USB', 'LSB', 'FM', etc.)
   * @returns {number} Resolution in Hz
   */
  getResolution(mode) {
    return this._nativeInstance.getResolution(mode);
  }

  /**
   * Get list of supported parameter types
   * @returns {Array<string>} Array of supported parameter types
   */
  getSupportedParms() {
    return this._nativeInstance.getSupportedParms();
  }

  /**
   * Get list of supported VFO operations
   * @returns {Array<string>} Array of supported VFO operation types
   */
  getSupportedVfoOps() {
    return this._nativeInstance.getSupportedVfoOps();
  }

  /**
   * Get list of supported scan types
   * @returns {Array<string>} Array of supported scan types
   */
  getSupportedScanTypes() {
    return this._nativeInstance.getSupportedScanTypes();
  }

  // Capability Query - Batch 2 (sync)

  /**
   * Get supported preamplifier values
   * @returns {number[]} Array of preamp dB values (e.g., [10, 20])
   */
  getPreampValues() {
    return this._nativeInstance.getPreampValues();
  }

  /**
   * Get supported attenuator values
   * @returns {number[]} Array of attenuator dB values (e.g., [10, 20, 30])
   */
  getAttenuatorValues() {
    return this._nativeInstance.getAttenuatorValues();
  }

  /**
   * Get maximum RIT offset supported
   * @returns {number} Maximum RIT offset in Hz
   */
  getMaxRit() {
    return this._nativeInstance.getMaxRit();
  }

  /**
   * Get maximum XIT offset supported
   * @returns {number} Maximum XIT offset in Hz
   */
  getMaxXit() {
    return this._nativeInstance.getMaxXit();
  }

  /**
   * Get maximum IF shift supported
   * @returns {number} Maximum IF shift in Hz
   */
  getMaxIfShift() {
    return this._nativeInstance.getMaxIfShift();
  }

  /**
   * Get list of available CTCSS tone frequencies
   * @returns {number[]} Array of CTCSS tones in Hz (e.g., [67.0, 71.9, ...])
   */
  getAvailableCtcssTones() {
    return this._nativeInstance.getAvailableCtcssTones();
  }

  /**
   * Get list of available DCS codes
   * @returns {number[]} Array of DCS codes (integer values)
   */
  getAvailableDcsCodes() {
    return this._nativeInstance.getAvailableDcsCodes();
  }

  /**
   * Get supported frequency ranges for RX and TX
   * @returns {{ rx: FrequencyRange[], tx: FrequencyRange[] }} RX and TX frequency ranges
   */
  getFrequencyRanges() {
    return this._nativeInstance.getFrequencyRanges();
  }

  /**
   * Get supported tuning steps per mode
   * @returns {TuningStepInfo[]} Array of tuning step info objects
   */
  getTuningSteps() {
    return this._nativeInstance.getTuningSteps();
  }

  /**
   * Get supported filter widths per mode
   * @returns {FilterInfo[]} Array of filter info objects
   */
  getFilterList() {
    return this._nativeInstance.getFilterList();
  }

  /**
   * Get Hamlib copyright information
   * @returns {string} Copyright string
   * @static
   */
  static getCopyright() {
    return nativeModule.HamLib.getCopyright();
  }

  /**
   * Get Hamlib license information
   * @returns {string} License string
   * @static
   */
  static getLicense() {
    return nativeModule.HamLib.getLicense();
  }

  // Lock Mode (Hamlib >= 4.7.0)

  async setLockMode(lock) {
    return this._nativeInstance.setLockMode(lock);
  }

  async getLockMode() {
    return this._nativeInstance.getLockMode();
  }

  // Clock (Hamlib >= 4.7.0)

  async setClock({ year, month, day, hour, min, sec, msec, utcOffset }) {
    return this._nativeInstance.setClock({ year, month, day, hour, min, sec, msec, utcOffset });
  }

  async getClock() {
    return this._nativeInstance.getClock();
  }

  // VFO Info (Hamlib >= 4.7.0)

  async getVfoInfo(vfo) {
    if (vfo !== undefined) {
      return this._nativeInstance.getVfoInfo(vfo);
    }
    return this._nativeInstance.getVfoInfo();
  }
}

// Export for CommonJS
module.exports = { HamLib };
module.exports.HamLib = HamLib;
module.exports.default = { HamLib }; 
