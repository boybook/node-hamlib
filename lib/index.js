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
  open() {
    return this._nativeInstance.open();
  }

  /**
   * Set VFO (Variable Frequency Oscillator)
   * @param {string} vfo - VFO identifier ('VFO-A' or 'VFO-B')
   * @throws {Error} Throws error when device doesn't support or operation fails
   */
  setVfo(vfo) {
    return this._nativeInstance.setVfo(vfo);
  }

  /**
   * Set frequency
   * @param {number} frequency - Frequency in hertz
   * @param {string} [vfo] - Optional VFO ('VFO-A' or 'VFO-B')
   */
  setFrequency(frequency, vfo) {
    if (vfo) {
      return this._nativeInstance.setFrequency(frequency, vfo);
    }
    return this._nativeInstance.setFrequency(frequency);
  }

  /**
   * Set radio mode
   * @param {string} mode - Radio mode ('USB', 'LSB', 'FM', 'PKTFM', etc.)
   * @param {string} [bandwidth] - Optional bandwidth ('narrow', 'wide')
   */
  setMode(mode, bandwidth) {
    if (bandwidth) {
      return this._nativeInstance.setMode(mode, bandwidth);
    }
    return this._nativeInstance.setMode(mode);
  }

  /**
   * Set PTT (Push-to-Talk) status
   * @param {boolean} state - true to enable PTT, false to disable
   */
  setPtt(state) {
    return this._nativeInstance.setPtt(state);
  }

  /**
   * Get current VFO
   * @returns {string} Current VFO identifier
   */
  getVfo() {
    return this._nativeInstance.getVfo();
  }

  /**
   * Get current frequency
   * @param {string} [vfo] - Optional VFO ('VFO-A' or 'VFO-B')
   * @returns {number} Current frequency in hertz
   */
  getFrequency(vfo) {
    if (vfo) {
      return this._nativeInstance.getFrequency(vfo);
    }
    return this._nativeInstance.getFrequency();
  }

  /**
   * Get current radio mode
   * @returns {Object} Object containing mode and bandwidth information
   */
  getMode() {
    return this._nativeInstance.getMode();
  }

  /**
   * Get current signal strength
   * @returns {number} Signal strength value
   */
  getStrength() {
    return this._nativeInstance.getStrength();
  }

  /**
   * Close connection to device
   * Connection can be re-established by calling open()
   */
  close() {
    return this._nativeInstance.close();
  }

  /**
   * Destroy connection to device
   * Object reference should be deleted after calling this
   */
  destroy() {
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
}

// Export for CommonJS
module.exports = { HamLib };
module.exports.HamLib = HamLib;
module.exports.default = { HamLib }; 