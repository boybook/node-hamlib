const { loadNativeModule } = require('./binary-loader');

// Load the native module
const nativeModule = loadNativeModule();

/**
 * HamLib class for controlling amateur radio devices
 * 
 * This is a wrapper around the native hamlib module that provides
 * a consistent interface for controlling amateur radio devices.
 */
class HamLib {
  /**
   * Create a new HamLib instance
   * @param {number} model - Radio model number (run `rigctl -l` to find your device model)
   * @param {string} [port] - Optional port path or network address
   *   - Serial: '/dev/ttyUSB0' (Linux), '/dev/cu.usbserial-XXXX' (macOS), 'COM1' (Windows)
   *   - Network: 'localhost:4532' or '192.168.1.100:4532' (auto-switches to NETRIGCTL mode)
   */
  constructor(model, port) {
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
   * Open connection to the radio device
   * Must be called before other operations
   * @throws {Error} Throws error when connection fails
   */
  async open() {
    return new Promise((resolve, reject) => {
      this._nativeInstance.open((err, result) => {
        if (err) {
          reject(new Error(err.message || 'Failed to open connection'));
        } else {
          resolve(result);
        }
      });
    });
  }

  /**
   * Set VFO (Variable Frequency Oscillator)
   * @param {string} vfo - VFO identifier ('VFO-A' or 'VFO-B')
   * @throws {Error} Throws error when device doesn't support or operation fails
   */
  async setVfo(vfo) {
    return new Promise((resolve, reject) => {
      this._nativeInstance.setVfo(vfo, (err, result) => {
        if (err) {
          reject(new Error(err.message || 'Failed to set VFO'));
        } else {
          resolve(result);
        }
      });
    });
  }

  /**
   * Set frequency
   * @param {number} frequency - Frequency in hertz
   * @param {string} [vfo] - Optional VFO ('VFO-A' or 'VFO-B')
   */
  async setFrequency(frequency, vfo) {
    return new Promise((resolve, reject) => {
      if (vfo) {
        this._nativeInstance.setFrequency(frequency, vfo, (err, result) => {
          if (err) {
            reject(new Error(err.message || 'Failed to set frequency'));
          } else {
            resolve(result);
          }
        });
      } else {
        this._nativeInstance.setFrequency(frequency, (err, result) => {
          if (err) {
            reject(new Error(err.message || 'Failed to set frequency'));
          } else {
            resolve(result);
          }
        });
      }
    });
  }

  /**
   * Set radio mode
   * @param {string} mode - Radio mode ('USB', 'LSB', 'FM', 'PKTFM', etc.)
   * @param {string} [bandwidth] - Optional bandwidth ('narrow', 'wide')
   */
  async setMode(mode, bandwidth) {
    return new Promise((resolve, reject) => {
      if (bandwidth) {
        this._nativeInstance.setMode(mode, bandwidth, (err, result) => {
          if (err) {
            reject(new Error(err.message || 'Failed to set mode'));
          } else {
            resolve(result);
          }
        });
      } else {
        this._nativeInstance.setMode(mode, (err, result) => {
          if (err) {
            reject(new Error(err.message || 'Failed to set mode'));
          } else {
            resolve(result);
          }
        });
      }
    });
  }

  /**
   * Set PTT (Push-to-Talk) status
   * @param {boolean} state - true to enable PTT, false to disable
   */
  async setPtt(state) {
    return new Promise((resolve, reject) => {
      this._nativeInstance.setPtt(state, (err, result) => {
        if (err) {
          reject(new Error(err.message || 'Failed to set PTT'));
        } else {
          resolve(result);
        }
      });
    });
  }

  /**
   * Get current VFO
   * @returns {string} Current VFO identifier
   */
  async getVfo() {
    return new Promise((resolve, reject) => {
      this._nativeInstance.getVfo((err, result) => {
        if (err) {
          reject(new Error(err.message || 'Failed to get VFO'));
        } else {
          resolve(result);
        }
      });
    });
  }

  /**
   * Get current frequency
   * @param {string} [vfo] - Optional VFO ('VFO-A' or 'VFO-B')
   * @returns {number} Current frequency in hertz
   */
  async getFrequency(vfo) {
    return new Promise((resolve, reject) => {
      if (vfo) {
        this._nativeInstance.getFrequency(vfo, (err, result) => {
          if (err) {
            reject(new Error(err.message || 'Failed to get frequency'));
          } else {
            resolve(result);
          }
        });
      } else {
        this._nativeInstance.getFrequency((err, result) => {
          if (err) {
            reject(new Error(err.message || 'Failed to get frequency'));
          } else {
            resolve(result);
          }
        });
      }
    });
  }

  /**
   * Get current radio mode
   * @returns {Object} Object containing mode and bandwidth information
   */
  async getMode() {
    return new Promise((resolve, reject) => {
      this._nativeInstance.getMode((err, result) => {
        if (err) {
          reject(new Error(err.message || 'Failed to get mode'));
        } else {
          resolve(result);
        }
      });
    });
  }

  /**
   * Get current signal strength
   * @returns {number} Signal strength value
   */
  async getStrength() {
    return new Promise((resolve, reject) => {
      this._nativeInstance.getStrength((err, result) => {
        if (err) {
          reject(new Error(err.message || 'Failed to get signal strength'));
        } else {
          resolve(result);
        }
      });
    });
  }

  /**
   * Close connection to device
   * Connection can be re-established by calling open()
   */
  async close() {
    return new Promise((resolve, reject) => {
      this._nativeInstance.close((err, result) => {
        if (err) {
          reject(new Error(err.message || 'Failed to close connection'));
        } else {
          resolve(result);
        }
      });
    });
  }

  /**
   * Destroy connection to device
   * Object reference should be deleted after calling this
   */
  async destroy() {
    return new Promise((resolve, reject) => {
      this._nativeInstance.destroy((err, result) => {
        if (err) {
          reject(new Error(err.message || 'Failed to destroy connection'));
        } else {
          resolve(result);
        }
      });
    });
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
  setMemoryChannel(channelNumber, channelData) {
    return this._nativeInstance.setMemoryChannel(channelNumber, channelData);
  }

  /**
   * Get memory channel data
   * @param {number} channelNumber - Memory channel number
   * @param {boolean} [readOnly=true] - Whether to read in read-only mode
   * @returns {Object} Channel data
   */
  getMemoryChannel(channelNumber, readOnly = true) {
    return this._nativeInstance.getMemoryChannel(channelNumber, readOnly);
  }

  /**
   * Select memory channel for operation
   * @param {number} channelNumber - Memory channel number to select
   */
  selectMemoryChannel(channelNumber) {
    return this._nativeInstance.selectMemoryChannel(channelNumber);
  }

  // RIT/XIT Control

  /**
   * Set RIT (Receiver Incremental Tuning) offset
   * @param {number} offsetHz - RIT offset in Hz
   */
  setRit(offsetHz) {
    return this._nativeInstance.setRit(offsetHz);
  }

  /**
   * Get current RIT offset
   * @returns {number} RIT offset in Hz
   */
  getRit() {
    return this._nativeInstance.getRit();
  }

  /**
   * Set XIT (Transmitter Incremental Tuning) offset
   * @param {number} offsetHz - XIT offset in Hz
   */
  setXit(offsetHz) {
    return this._nativeInstance.setXit(offsetHz);
  }

  /**
   * Get current XIT offset
   * @returns {number} XIT offset in Hz
   */
  getXit() {
    return this._nativeInstance.getXit();
  }

  /**
   * Clear both RIT and XIT offsets
   * @returns {boolean} Success status
   */
  clearRitXit() {
    return this._nativeInstance.clearRitXit();
  }

  // Scanning Operations

  /**
   * Start scanning operation
   * @param {string} scanType - Scan type ('VFO', 'MEM', 'PROG', 'DELTA', 'PRIO')
   * @param {number} [channel=0] - Optional channel number for some scan types
   */
  startScan(scanType, channel = 0) {
    return this._nativeInstance.startScan(scanType, channel);
  }

  /**
   * Stop scanning operation
   */
  stopScan() {
    return this._nativeInstance.stopScan();
  }

  // Level Controls

  /**
   * Set radio level (gain, volume, etc.)
   * @param {string} levelType - Level type ('AF', 'RF', 'SQL', 'RFPOWER', etc.)
   * @param {number} value - Level value (0.0-1.0 typically)
   */
  setLevel(levelType, value) {
    return this._nativeInstance.setLevel(levelType, value);
  }

  /**
   * Get radio level
   * @param {string} levelType - Level type ('AF', 'RF', 'SQL', 'STRENGTH', etc.)
   * @returns {number} Level value
   */
  getLevel(levelType) {
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
  setFunction(functionType, enable) {
    return this._nativeInstance.setFunction(functionType, enable);
  }

  /**
   * Get radio function status
   * @param {string} functionType - Function type ('NB', 'COMP', 'VOX', 'TONE', etc.)
   * @returns {boolean} Function enabled status
   */
  getFunction(functionType) {
    return this._nativeInstance.getFunction(functionType);
  }

  /**
   * Get list of supported function types
   * @returns {Array<string>} Array of supported function types
   */
  getSupportedFunctions() {
    return this._nativeInstance.getSupportedFunctions();
  }

  // Split Operations

  /**
   * Set split mode TX frequency
   * @param {number} txFrequency - TX frequency in Hz
   */
  setSplitFreq(txFrequency) {
    return this._nativeInstance.setSplitFreq(txFrequency);
  }

  /**
   * Get split mode TX frequency
   * @returns {number} TX frequency in Hz
   */
  getSplitFreq() {
    return this._nativeInstance.getSplitFreq();
  }

  /**
   * Set split mode TX mode
   * @param {string} txMode - TX mode ('USB', 'LSB', 'FM', etc.)
   * @param {number} [txWidth] - Optional TX bandwidth
   */
  setSplitMode(txMode, txWidth) {
    if (txWidth !== undefined) {
      return this._nativeInstance.setSplitMode(txMode, txWidth);
    }
    return this._nativeInstance.setSplitMode(txMode);
  }

  /**
   * Get split mode TX mode
   * @returns {Object} TX mode and width
   */
  getSplitMode() {
    return this._nativeInstance.getSplitMode();
  }

  /**
   * Enable/disable split operation
   * @param {boolean} enable - true to enable split, false to disable
   * @param {string} [txVfo='VFO-B'] - TX VFO ('VFO-A' or 'VFO-B')
   */
  setSplit(enable, txVfo = 'VFO-B') {
    return this._nativeInstance.setSplit(enable, txVfo);
  }

  /**
   * Get split operation status
   * @returns {Object} Split status and TX VFO
   */
  getSplit() {
    return this._nativeInstance.getSplit();
  }

  // VFO Operations

  /**
   * Perform VFO operation
   * @param {string} operation - VFO operation ('CPY', 'XCHG', 'FROM_VFO', 'TO_VFO', 'MCL', 'UP', 'DOWN', 'BAND_UP', 'BAND_DOWN', 'LEFT', 'RIGHT', 'TUNE', 'TOGGLE')
   */
  vfoOperation(operation) {
    return this._nativeInstance.vfoOperation(operation);
  }

  // Antenna Selection

  /**
   * Set antenna
   * @param {number} antenna - Antenna number (1, 2, 3, etc.)
   */
  setAntenna(antenna) {
    return this._nativeInstance.setAntenna(antenna);
  }

  /**
   * Get current antenna
   * @returns {number} Current antenna number
   */
  getAntenna() {
    return this._nativeInstance.getAntenna();
  }

  // Serial Port Configuration

  /**
   * Set serial port configuration parameter
   * @param {string} paramName - Parameter name ('data_bits', 'stop_bits', 'serial_parity', 'serial_handshake', 'rts_state', 'dtr_state')
   * @param {string} paramValue - Parameter value
   * @example
   * // Set data bits to 8
   * hamlib.setSerialConfig('data_bits', '8');
   * 
   * // Set parity to even
   * hamlib.setSerialConfig('serial_parity', 'Even');
   * 
   * // Set handshake to hardware
   * hamlib.setSerialConfig('serial_handshake', 'Hardware');
   */
  setSerialConfig(paramName, paramValue) {
    return this._nativeInstance.setSerialConfig(paramName, paramValue);
  }

  /**
   * Get serial port configuration parameter
   * @param {string} paramName - Parameter name to retrieve
   * @returns {string} Parameter value
   * @example
   * // Get current data bits setting
   * const dataBits = hamlib.getSerialConfig('data_bits');
   * 
   * // Get current parity setting
   * const parity = hamlib.getSerialConfig('serial_parity');
   */
  getSerialConfig(paramName) {
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
  setPttType(pttType) {
    return this._nativeInstance.setPttType(pttType);
  }

  /**
   * Get current PTT type
   * @returns {string} Current PTT type
   */
  getPttType() {
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
  setDcdType(dcdType) {
    return this._nativeInstance.setDcdType(dcdType);
  }

  /**
   * Get current DCD type
   * @returns {string} Current DCD type
   */
  getDcdType() {
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
}

// Export for CommonJS
module.exports = { HamLib };
module.exports.HamLib = HamLib;
module.exports.default = { HamLib }; 