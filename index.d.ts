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
 * Supported rig information interface
 */
interface SupportedRigInfo {
  /** Rig model number */
  rigModel: number;
  /** Model name */
  modelName: string;
  /** Manufacturer name */
  mfgName: string;
  /** Driver version */
  version: string;
  /** Driver status (Alpha, Beta, Stable, etc.) */
  status: string;
  /** Rig type (Transceiver, Handheld, Mobile, etc.) */
  rigType: string;
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
 * Memory channel data interface
 */
interface MemoryChannelData {
  /** Channel frequency in Hz */
  frequency?: number;
  /** Radio mode */
  mode?: RadioMode;
  /** Bandwidth */
  bandwidth?: number;
  /** Channel description */
  description?: string;
  /** TX frequency for split operation */
  txFrequency?: number;
  /** CTCSS tone frequency */
  ctcssTone?: number;
}

/**
 * Memory channel info interface
 */
interface MemoryChannelInfo {
  /** Channel number */
  channelNumber: number;
  /** Channel frequency */
  frequency: number;
  /** Radio mode */
  mode: string;
  /** Bandwidth */
  bandwidth: number;
  /** Channel description */
  description: string;
  /** Split operation enabled */
  split: boolean;
  /** TX frequency (if split enabled) */
  txFrequency?: number;
  /** CTCSS tone frequency */
  ctcssTone?: number;
}

/**
 * Split mode info interface
 */
interface SplitModeInfo {
  /** TX mode */
  mode: string;
  /** TX bandwidth */
  width: number;
}

/**
 * Split status info interface
 */
interface SplitStatusInfo {
  /** Split enabled status */
  enabled: boolean;
  /** TX VFO */
  txVfo: string;
}

/**
 * Level type
 */
type LevelType = 'AF' | 'RF' | 'SQL' | 'RFPOWER' | 'MICGAIN' | 'IF' | 'APF' | 'NR' | 
                 'PBT_IN' | 'PBT_OUT' | 'CWPITCH' | 'KEYSPD' | 'NOTCHF' | 'COMP' | 
                 'AGC' | 'BKINDL' | 'BALANCE' | 'VOXGAIN' | 'VOXDELAY' | 'ANTIVOX' |
                 'STRENGTH' | 'RAWSTR' | 'SWR' | 'ALC' | 'RFPOWER_METER' | 
                 'COMP_METER' | 'VD_METER' | 'ID_METER' | 'TEMP_METER' | string;

/**
 * Function type
 */
type FunctionType = 'FAGC' | 'NB' | 'COMP' | 'VOX' | 'TONE' | 'TSQL' | 'SBKIN' | 
                    'FBKIN' | 'ANF' | 'NR' | 'AIP' | 'APF' | 'TUNER' | 'XIT' | 
                    'RIT' | 'LOCK' | 'MUTE' | 'VSC' | 'REV' | 'SQL' | 'ABM' | 
                    'BC' | 'MBC' | 'AFC' | 'SATMODE' | 'SCOPE' | 'RESUME' | 
                    'TBURST' | string;

/**
 * Scan type
 */
type ScanType = 'VFO' | 'MEM' | 'PROG' | 'DELTA' | 'PRIO';

/**
 * VFO operation type
 */
type VfoOperationType = 'CPY' | 'XCHG' | 'FROM_VFO' | 'TO_VFO' | 'MCL' | 'UP' | 
                        'DOWN' | 'BAND_UP' | 'BAND_DOWN' | 'LEFT' | 'RIGHT' | 
                        'TUNE' | 'TOGGLE';

/**
 * Serial configuration parameter names
 */
type SerialConfigParam = 'data_bits' | 'stop_bits' | 'serial_parity' | 'serial_handshake' | 
                        'rts_state' | 'dtr_state';

/**
 * PTT (Push-to-Talk) types
 */
type PttType = 'RIG' | 'DTR' | 'RTS' | 'PARALLEL' | 'CM108' | 'GPIO' | 'GPION' | 'NONE';

/**
 * DCD (Data Carrier Detect) types
 */
type DcdType = 'RIG' | 'DSR' | 'CTS' | 'CD' | 'PARALLEL' | 'CM108' | 'GPIO' | 'GPION' | 'NONE';

/**
 * Serial configuration options interface
 */
interface SerialConfigOptions {
  /** Serial port basic parameters */
  serial: {
    /** Data bits options */
    data_bits: string[];
    /** Stop bits options */
    stop_bits: string[];
    /** Parity options */
    serial_parity: string[];
    /** Handshake options */
    serial_handshake: string[];
    /** RTS state options */
    rts_state: string[];
    /** DTR state options */
    dtr_state: string[];
  };
  /** PTT type options */
  ptt_type: string[];
  /** DCD type options */
  dcd_type: string[];
}

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
   * Get list of all supported radio models
   * @returns Array of supported radio models with details
   * @static
   * @example
   * const supportedRigs = HamLib.getSupportedRigs();
   * console.log(`Found ${supportedRigs.length} supported rigs`);
   * supportedRigs.forEach(rig => {
   *   console.log(`${rig.rigModel}: ${rig.mfgName} ${rig.modelName} (${rig.status})`);
   * });
   */
  static getSupportedRigs(): SupportedRigInfo[];

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

  // Memory Channel Management

  /**
   * Set memory channel data
   * @param channelNumber Memory channel number
   * @param channelData Channel data (frequency, mode, description, etc.)
   */
  setMemoryChannel(channelNumber: number, channelData: MemoryChannelData): void;

  /**
   * Get memory channel data
   * @param channelNumber Memory channel number
   * @param readOnly Whether to read in read-only mode (default: true)
   * @returns Channel data
   */
  getMemoryChannel(channelNumber: number, readOnly?: boolean): MemoryChannelInfo;

  /**
   * Select memory channel for operation
   * @param channelNumber Memory channel number to select
   */
  selectMemoryChannel(channelNumber: number): void;

  // RIT/XIT Control

  /**
   * Set RIT (Receiver Incremental Tuning) offset
   * @param offsetHz RIT offset in Hz
   */
  setRit(offsetHz: number): void;

  /**
   * Get current RIT offset
   * @returns RIT offset in Hz
   */
  getRit(): number;

  /**
   * Set XIT (Transmitter Incremental Tuning) offset
   * @param offsetHz XIT offset in Hz
   */
  setXit(offsetHz: number): void;

  /**
   * Get current XIT offset
   * @returns XIT offset in Hz
   */
  getXit(): number;

  /**
   * Clear both RIT and XIT offsets
   * @returns Success status
   */
  clearRitXit(): boolean;

  // Scanning Operations

  /**
   * Start scanning operation
   * @param scanType Scan type ('VFO', 'MEM', 'PROG', 'DELTA', 'PRIO')
   * @param channel Optional channel number for some scan types
   */
  startScan(scanType: ScanType, channel?: number): void;

  /**
   * Stop scanning operation
   */
  stopScan(): void;

  // Level Controls

  /**
   * Set radio level (gain, volume, etc.)
   * @param levelType Level type ('AF', 'RF', 'SQL', 'RFPOWER', etc.)
   * @param value Level value (0.0-1.0 typically)
   */
  setLevel(levelType: LevelType, value: number): void;

  /**
   * Get radio level
   * @param levelType Level type ('AF', 'RF', 'SQL', 'STRENGTH', etc.)
   * @returns Level value
   */
  getLevel(levelType: LevelType): number;

  /**
   * Get list of supported level types
   * @returns Array of supported level types
   */
  getSupportedLevels(): string[];

  // Function Controls

  /**
   * Set radio function on/off
   * @param functionType Function type ('NB', 'COMP', 'VOX', 'TONE', etc.)
   * @param enable true to enable, false to disable
   */
  setFunction(functionType: FunctionType, enable: boolean): void;

  /**
   * Get radio function status
   * @param functionType Function type ('NB', 'COMP', 'VOX', 'TONE', etc.)
   * @returns Function enabled status
   */
  getFunction(functionType: FunctionType): boolean;

  /**
   * Get list of supported function types
   * @returns Array of supported function types
   */
  getSupportedFunctions(): string[];

  // Split Operations

  /**
   * Set split mode TX frequency
   * @param txFrequency TX frequency in Hz
   */
  setSplitFreq(txFrequency: number): void;

  /**
   * Get split mode TX frequency
   * @returns TX frequency in Hz
   */
  getSplitFreq(): number;

  /**
   * Set split mode TX mode
   * @param txMode TX mode ('USB', 'LSB', 'FM', etc.)
   * @param txWidth Optional TX bandwidth
   */
  setSplitMode(txMode: RadioMode, txWidth?: number): void;

  /**
   * Get split mode TX mode
   * @returns TX mode and width
   */
  getSplitMode(): SplitModeInfo;

  /**
   * Enable/disable split operation
   * @param enable true to enable split, false to disable
   * @param txVfo TX VFO ('VFO-A' or 'VFO-B')
   */
  setSplit(enable: boolean, txVfo?: VFO): void;

  /**
   * Get split operation status
   * @returns Split status and TX VFO
   */
  getSplit(): SplitStatusInfo;

  // VFO Operations

  /**
   * Perform VFO operation
   * @param operation VFO operation ('CPY', 'XCHG', 'FROM_VFO', 'TO_VFO', etc.)
   */
  vfoOperation(operation: VfoOperationType): void;

  // Antenna Selection

  /**
   * Set antenna
   * @param antenna Antenna number (1, 2, 3, etc.)
   */
  setAntenna(antenna: number): void;

  /**
   * Get current antenna
   * @returns Current antenna number
   */
  getAntenna(): number;

  // Serial Port Configuration

  /**
   * Set serial port configuration parameter
   * @param paramName Parameter name ('data_bits', 'stop_bits', 'serial_parity', 'serial_handshake', 'rts_state', 'dtr_state')
   * @param paramValue Parameter value
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
  setSerialConfig(paramName: SerialConfigParam, paramValue: string): void;

  /**
   * Get serial port configuration parameter
   * @param paramName Parameter name to retrieve
   * @returns Parameter value
   * @example
   * // Get current data bits setting
   * const dataBits = hamlib.getSerialConfig('data_bits');
   * 
   * // Get current parity setting
   * const parity = hamlib.getSerialConfig('serial_parity');
   */
  getSerialConfig(paramName: SerialConfigParam): string;

  /**
   * Set PTT (Push-to-Talk) type
   * @param pttType PTT type ('RIG', 'DTR', 'RTS', 'PARALLEL', 'CM108', 'GPIO', 'GPION', 'NONE')
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
  setPttType(pttType: PttType): void;

  /**
   * Get current PTT type
   * @returns Current PTT type
   */
  getPttType(): string;

  /**
   * Set DCD (Data Carrier Detect) type
   * @param dcdType DCD type ('RIG', 'DSR', 'CTS', 'CD', 'PARALLEL', 'CM108', 'GPIO', 'GPION', 'NONE')
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
  setDcdType(dcdType: DcdType): void;

  /**
   * Get current DCD type
   * @returns Current DCD type
   */
  getDcdType(): string;

  /**
   * Get supported serial configuration options
   * @returns Object containing all supported configuration parameters and their possible values
   * @example
   * const configs = hamlib.getSupportedSerialConfigs();
   * console.log('Supported data bits:', configs.serial.data_bits);
   * console.log('Supported PTT types:', configs.ptt_type);
   */
  getSupportedSerialConfigs(): SerialConfigOptions;
}

/**
 * node-hamlib module export object
 */
declare const nodeHamlib: {
  HamLib: typeof HamLib;
};

// Export types for use elsewhere
export { ConnectionInfo, ModeInfo, SupportedRigInfo, VFO, RadioMode, MemoryChannelData, 
         MemoryChannelInfo, SplitModeInfo, SplitStatusInfo, LevelType, FunctionType, 
         ScanType, VfoOperationType, SerialConfigParam, PttType, DcdType, SerialConfigOptions, HamLib };

// Support both CommonJS and ES module exports
export = nodeHamlib;
export default nodeHamlib; 