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
}

// Export for CommonJS
module.exports = { HamLib };
module.exports.HamLib = HamLib;
module.exports.default = { HamLib }; 