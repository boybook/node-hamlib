/**
 * Connection information interface
 */
interface ConnectionInfo {
  /** Connection type: serial or network */
  connectionType: 'serial' | 'network';
  /** Port path or network address */
  portPath: string;
  /** Connection status */
  connected: boolean;
  /** Original model number */
  originalModel: number;
  /** Actual model number used */
  actualModel: number;
}

/**
 * Radio mode information interface
 */
interface ModeInfo {
  /** Radio mode character */
  mode: string;
  /** Bandwidth width */
  width: number;
}

/**
 * VFO type
 */
type VFO = 'VFO-A' | 'VFO-B';

/**
 * Radio mode type
 */
type RadioMode = 'USB' | 'LSB' | 'FM' | 'PKTFM' | 'AM' | 'CW' | 'RTTY' | 'DIG' | string;

/**
 * HamLib class - for controlling amateur radio devices
 */
declare class HamLib {
  /**
   * Constructor
   * @param model Radio model number (execute rigctl -l to find your device model number)
   * @param port Optional port path
   *   - Serial connection: '/dev/ttyUSB0' (Linux default), '/dev/cu.usbserial-XXXX' (macOS), 'COM1' (Windows)
   *   - Network connection: 'localhost:4532' or '192.168.1.100:4532' (automatically switches to NETRIGCTL mode)
   * 
   * @example
   * // Use default serial port
   * const rig = new HamLib(1035);
   * 
   * // Use custom serial port
   * const rig = new HamLib(1035, '/dev/ttyUSB1');
   * 
   * // Network connection to rigctld
   * const rig = new HamLib(1035, 'localhost:4532');
   */
  constructor(model: number, port?: string);

  /**
   * Open connection to device
   * Must be called before other operations
   * @throws Throws error when connection fails
   */
  open(): void;

  /**
   * Set VFO (Variable Frequency Oscillator)
   * @param vfo VFO identifier, typically 'VFO-A' or 'VFO-B'
   * @throws Throws error when device doesn't support or operation fails
   */
  setVfo(vfo: VFO): void;

  /**
   * Set frequency
   * @param frequency Frequency value in hertz
   * @param vfo Optional VFO to set frequency on ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @example
   * rig.setFrequency(144390000); // Set to 144.39MHz on current VFO
   * rig.setFrequency(144390000, 'VFO-A'); // Set to 144.39MHz on VFO-A
   * rig.setFrequency(144390000, 'VFO-B'); // Set to 144.39MHz on VFO-B
   */
  setFrequency(frequency: number, vfo?: VFO): void;

  /**
   * Set radio mode
   * @param mode Radio mode (such as 'USB', 'LSB', 'FM', 'PKTFM')
   * @param bandwidth Optional bandwidth setting ('narrow', 'wide', or default)
   * @note Operates on the current VFO (RIG_VFO_CURR)
   * @example
   * rig.setMode('USB');
   * rig.setMode('FM', 'narrow');
   */
  setMode(mode: RadioMode, bandwidth?: 'narrow' | 'wide'): void;

  /**
   * Set PTT (Push-to-Talk) status
   * @param state true to enable PTT, false to disable PTT
   * @note Operates on the current VFO (RIG_VFO_CURR)
   */
  setPtt(state: boolean): void;

  /**
   * Get current VFO
   * @returns Current VFO identifier
   */
  getVfo(): string;

  /**
   * Get current frequency
   * @param vfo Optional VFO to get frequency from ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Current frequency value in hertz
   * @example
   * rig.getFrequency(); // Get frequency from current VFO
   * rig.getFrequency('VFO-A'); // Get frequency from VFO-A
   * rig.getFrequency('VFO-B'); // Get frequency from VFO-B
   */
  getFrequency(vfo?: VFO): number;

  /**
   * Get current radio mode
   * @returns Object containing mode and bandwidth information
   * @note Operates on the current VFO (RIG_VFO_CURR)
   */
  getMode(): ModeInfo;

  /**
   * Get current signal strength
   * @returns Signal strength value
   * @note Operates on the current VFO (RIG_VFO_CURR)
   */
  getStrength(): number;

  /**
   * Close connection to device
   * Does not destroy object, can re-establish connection by calling open()
   */
  close(): void;

  /**
   * Destroy connection to device
   * Should delete object reference after calling to enable garbage collection
   */
  destroy(): void;

  /**
   * Get connection information
   * @returns Object containing connection type, port path, connection status, and model numbers
   */
  getConnectionInfo(): ConnectionInfo;
}

/**
 * node-hamlib module export object
 */
declare const nodeHamlib: {
  HamLib: typeof HamLib;
};

// Export types for use elsewhere
export { ConnectionInfo, ModeInfo, VFO, RadioMode, HamLib };

// Support both CommonJS and ES module exports
export = nodeHamlib;
export default nodeHamlib; 