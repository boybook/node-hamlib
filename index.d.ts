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
 * Antenna information interface
 */
interface AntennaInfo {
  /** Currently selected antenna */
  currentAntenna: number;
  /** TX antenna selection */
  txAntenna: number;
  /** RX antenna selection */
  rxAntenna: number;
  /** Additional antenna option/parameter */
  option: number;
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
  open(): Promise<number>;

  /**
   * Set VFO (Variable Frequency Oscillator)
   * @param vfo VFO identifier, typically 'VFO-A' or 'VFO-B'
   * @throws Throws error when device doesn't support or operation fails
   */
  setVfo(vfo: VFO): Promise<number>;

  /**
   * Set frequency
   * @param frequency Frequency value in hertz
   * @param vfo Optional VFO to set frequency on ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @example
   * await rig.setFrequency(144390000); // Set to 144.39MHz on current VFO
   * await rig.setFrequency(144390000, 'VFO-A'); // Set to 144.39MHz on VFO-A
   * await rig.setFrequency(144390000, 'VFO-B'); // Set to 144.39MHz on VFO-B
   */
  setFrequency(frequency: number, vfo?: VFO): Promise<number>;

  /**
   * Set radio mode
   * @param mode Radio mode (such as 'USB', 'LSB', 'FM', 'PKTFM')
   * @param bandwidth Optional bandwidth setting ('narrow', 'wide', or default)
   * @param vfo Optional VFO to set mode on ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @example
   * await rig.setMode('USB');
   * await rig.setMode('FM', 'narrow');
   * await rig.setMode('USB', 'wide', 'VFO-A');
   */
  setMode(mode: RadioMode, bandwidth?: 'narrow' | 'wide', vfo?: VFO): Promise<number>;

  /**
   * Set PTT (Push-to-Talk) status
   * @param state true to enable PTT, false to disable PTT
   * @note Operates on the current VFO (RIG_VFO_CURR)
   */
  setPtt(state: boolean): Promise<number>;

  /**
   * Get current VFO
   * @returns Current VFO identifier
   */
  getVfo(): Promise<string>;

  /**
   * Get current frequency
   * @param vfo Optional VFO to get frequency from ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Current frequency value in hertz
   * @example
   * await rig.getFrequency(); // Get frequency from current VFO
   * await rig.getFrequency('VFO-A'); // Get frequency from VFO-A
   * await rig.getFrequency('VFO-B'); // Get frequency from VFO-B
   */
  getFrequency(vfo?: VFO): Promise<number>;

  /**
   * Get current radio mode
   * @returns Object containing mode and bandwidth information
   * @note Operates on the current VFO (RIG_VFO_CURR)
   */
  getMode(): Promise<ModeInfo>;

  /**
   * Get current signal strength
   * @param vfo Optional VFO to get signal strength from ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Signal strength value
   * @example
   * const strength = await rig.getStrength(); // Get strength from current VFO
   * const strengthA = await rig.getStrength('VFO-A'); // Get strength from VFO-A
   */
  getStrength(vfo?: VFO): Promise<number>;

  /**
   * Close connection to device
   * Does not destroy object, can re-establish connection by calling open()
   */
  close(): Promise<number>;

  /**
   * Destroy connection to device
   * Should delete object reference after calling to enable garbage collection
   */
  destroy(): Promise<number>;

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
  setMemoryChannel(channelNumber: number, channelData: MemoryChannelData): Promise<number>;

  /**
   * Get memory channel data
   * @param channelNumber Memory channel number
   * @param readOnly Whether to read in read-only mode (default: true)
   * @returns Channel data
   */
  getMemoryChannel(channelNumber: number, readOnly?: boolean): Promise<MemoryChannelInfo>;

  /**
   * Select memory channel for operation
   * @param channelNumber Memory channel number to select
   */
  selectMemoryChannel(channelNumber: number): Promise<number>;

  // RIT/XIT Control

  /**
   * Set RIT (Receiver Incremental Tuning) offset
   * @param offsetHz RIT offset in Hz
   */
  setRit(offsetHz: number): Promise<number>;

  /**
   * Get current RIT offset
   * @returns RIT offset in Hz
   */
  getRit(): Promise<number>;

  /**
   * Set XIT (Transmitter Incremental Tuning) offset
   * @param offsetHz XIT offset in Hz
   */
  setXit(offsetHz: number): Promise<number>;

  /**
   * Get current XIT offset
   * @returns XIT offset in Hz
   */
  getXit(): Promise<number>;

  /**
   * Clear both RIT and XIT offsets
   * @returns Success status
   */
  clearRitXit(): Promise<number>;

  // Scanning Operations

  /**
   * Start scanning operation
   * @param scanType Scan type ('VFO', 'MEM', 'PROG', 'DELTA', 'PRIO')
   * @param callback Callback function
   */
  startScan(scanType: ScanType): Promise<number>;

  /**
   * Start scanning operation with channel
   * @param scanType Scan type ('VFO', 'MEM', 'PROG', 'DELTA', 'PRIO')
   * @param channel Channel number for some scan types
   * @param callback Callback function
   */
  startScan(scanType: ScanType, channel: number): Promise<number>;

  /**
   * Stop scanning operation
   */
  stopScan(): Promise<number>;

  // Level Controls

  /**
   * Set radio level (gain, volume, etc.)
   * @param levelType Level type ('AF', 'RF', 'SQL', 'RFPOWER', etc.)
   * @param value Level value (0.0-1.0 typically)
   */
  setLevel(levelType: LevelType, value: number): Promise<number>;

  /**
   * Get radio level
   * @param levelType Level type ('AF', 'RF', 'SQL', 'STRENGTH', etc.)
   * @returns Level value
   */
  getLevel(levelType: LevelType): Promise<number>;

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
  setFunction(functionType: FunctionType, enable: boolean): Promise<number>;

  /**
   * Get radio function status
   * @param functionType Function type ('NB', 'COMP', 'VOX', 'TONE', etc.)
   * @returns Function enabled status
   */
  getFunction(functionType: FunctionType): Promise<boolean>;

  /**
   * Get list of supported function types
   * @returns Array of supported function types
   */
  getSupportedFunctions(): string[];

  // Split Operations

  /**
   * Set split mode TX frequency
   * @param txFrequency TX frequency in Hz
   * @param vfo Optional VFO to set TX frequency on ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   */
  setSplitFreq(txFrequency: number, vfo?: VFO): Promise<number>;

  /**
   * Get split mode TX frequency
   * @param vfo Optional VFO to get TX frequency from ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns TX frequency in Hz
   */
  getSplitFreq(vfo?: VFO): Promise<number>;

  /**
   * Set split mode TX mode
   * @param txMode TX mode ('USB', 'LSB', 'FM', etc.)
   * @param vfo Optional VFO to set TX mode on ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   */
  setSplitMode(txMode: RadioMode, vfo?: VFO): Promise<number>;

  /**
   * Set split mode TX mode with width
   * @param txMode TX mode ('USB', 'LSB', 'FM', etc.)
   * @param txWidth TX bandwidth
   * @param vfo Optional VFO to set TX mode on ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   */
  setSplitMode(txMode: RadioMode, txWidth: number, vfo?: VFO): Promise<number>;

  /**
   * Get split mode TX mode
   * @param vfo Optional VFO to get TX mode from ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns TX mode and width
   */
  getSplitMode(vfo?: VFO): Promise<SplitModeInfo>;

  /**
   * Enable/disable split operation
   * @param enable true to enable split, false to disable
   * @param rxVfo Optional RX VFO ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @param txVfo Optional TX VFO ('VFO-A' or 'VFO-B'). If not specified, uses VFO-B
   * @example
   * await rig.setSplit(true);                    // Enable split with defaults
   * await rig.setSplit(true, 'VFO-A');          // Enable split, RX on VFO-A, TX on VFO-B
   * await rig.setSplit(true, 'VFO-A', 'VFO-B'); // Enable split, explicit RX and TX VFOs
   */
  setSplit(enable: boolean, rxVfo?: VFO, txVfo?: VFO): Promise<number>;

  /**
   * Get split operation status
   * @param vfo Optional VFO to get status from ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Split status and TX VFO
   */
  getSplit(vfo?: VFO): Promise<SplitStatusInfo>;

  // VFO Operations

  /**
   * Perform VFO operation
   * @param operation VFO operation ('CPY', 'XCHG', 'FROM_VFO', 'TO_VFO', etc.)
   */
  vfoOperation(operation: VfoOperationType): Promise<number>;

  // Antenna Selection

  // Note: setAntenna and getAntenna are defined later with full VFO support

  // Serial Port Configuration

  /**
   * Set serial port configuration parameter
   * @param paramName Parameter name ('data_bits', 'stop_bits', 'serial_parity', 'serial_handshake', 'rts_state', 'dtr_state')
   * @param paramValue Parameter value
   * @example
   * // Set data bits to 8
   * await hamlib.setSerialConfig('data_bits', '8');
   * 
   * // Set parity to even
   * await hamlib.setSerialConfig('serial_parity', 'Even');
   * 
   * // Set handshake to hardware
   * await hamlib.setSerialConfig('serial_handshake', 'Hardware');
   */
  setSerialConfig(paramName: SerialConfigParam, paramValue: string): Promise<number>;

  /**
   * Get serial port configuration parameter
   * @param paramName Parameter name to retrieve
   * @returns Parameter value
   * @example
   * // Get current data bits setting
   * const dataBits = await hamlib.getSerialConfig('data_bits');
   * 
   * // Get current parity setting
   * const parity = await hamlib.getSerialConfig('serial_parity');
   */
  getSerialConfig(paramName: SerialConfigParam): Promise<string>;

  /**
   * Set PTT (Push-to-Talk) type
   * @param pttType PTT type ('RIG', 'DTR', 'RTS', 'PARALLEL', 'CM108', 'GPIO', 'GPION', 'NONE')
   * @example
   * // Use DTR line for PTT
   * await hamlib.setPttType('DTR');
   * 
   * // Use RTS line for PTT
   * await hamlib.setPttType('RTS');
   * 
   * // Use CAT command for PTT
   * await hamlib.setPttType('RIG');
   */
  setPttType(pttType: PttType): Promise<number>;

  /**
   * Get current PTT type
   * @returns Current PTT type
   */
  getPttType(): Promise<string>;

  /**
   * Set DCD (Data Carrier Detect) type
   * @param dcdType DCD type ('RIG', 'DSR', 'CTS', 'CD', 'PARALLEL', 'CM108', 'GPIO', 'GPION', 'NONE')
   * @example
   * // Use DSR line for DCD
   * await hamlib.setDcdType('DSR');
   * 
   * // Use CTS line for DCD
   * await hamlib.setDcdType('CTS');
   * 
   * // Use CAT command for DCD
   * await hamlib.setDcdType('RIG');
   */
  setDcdType(dcdType: DcdType): Promise<number>;

  /**
   * Get current DCD type
   * @returns Current DCD type
   */
  getDcdType(): Promise<string>;

  /**
   * Get supported serial configuration options
   * @returns Object containing all supported configuration parameters and their possible values
   * @example
   * const configs = hamlib.getSupportedSerialConfigs();
   * console.log('Supported data bits:', configs.serial.data_bits);
   * console.log('Supported PTT types:', configs.ptt_type);
   */
  getSupportedSerialConfigs(): SerialConfigOptions;

  // Power Control

  /**
   * Set radio power status
   * @param status Power status (0=OFF, 1=ON, 2=STANDBY, 4=OPERATE, 8=UNKNOWN)
   * @returns Success status
   * @example
   * await rig.setPowerstat(1); // Power on
   * await rig.setPowerstat(0); // Power off
   * await rig.setPowerstat(2); // Standby mode
   */
  setPowerstat(status: number): Promise<number>;

  /**
   * Get current radio power status
   * @returns Power status (0=OFF, 1=ON, 2=STANDBY, 4=OPERATE, 8=UNKNOWN)
   * @example
   * const powerStatus = await rig.getPowerstat();
   * if (powerStatus === 1) {
   *   console.log('Radio is powered on');
   * }
   */
  getPowerstat(): Promise<number>;

  // PTT Status Detection

  /**
   * Get current PTT status
   * @param vfo Optional VFO to get PTT status from ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns PTT status (true if transmitting, false if receiving)
   * @example
   * const isTransmitting = await rig.getPtt();
   * if (isTransmitting) {
   *   console.log('Radio is transmitting');
   * }
   */
  getPtt(vfo?: VFO): Promise<boolean>;

  // Data Carrier Detect

  /**
   * Get current DCD (Data Carrier Detect) status
   * @param vfo Optional VFO to get DCD status from ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns DCD status (true if carrier detected, false if no carrier)
   * @example
   * const carrierDetected = await rig.getDcd();
   * if (carrierDetected) {
   *   console.log('Carrier detected');
   * }
   */
  getDcd(vfo?: VFO): Promise<boolean>;

  // Tuning Step Control

  /**
   * Set tuning step
   * @param step Tuning step in Hz
   * @param vfo Optional VFO to set tuning step on ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Success status
   * @example
   * await rig.setTuningStep(25000); // 25 kHz steps
   * await rig.setTuningStep(12500); // 12.5 kHz steps
   * await rig.setTuningStep(100);   // 100 Hz steps for HF
   */
  setTuningStep(step: number, vfo?: VFO): Promise<number>;

  /**
   * Get current tuning step
   * @param vfo Optional VFO to get tuning step from ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Current tuning step in Hz
   * @example
   * const step = await rig.getTuningStep();
   * console.log(`Current tuning step: ${step} Hz`);
   */
  getTuningStep(vfo?: VFO): Promise<number>;

  // Repeater Control

  /**
   * Set repeater shift direction
   * @param shift Repeater shift direction ('NONE', 'MINUS', 'PLUS', '-', '+')
   * @param vfo Optional VFO to set repeater shift on ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Success status
   * @example
   * await rig.setRepeaterShift('PLUS');  // Positive shift (+)
   * await rig.setRepeaterShift('MINUS'); // Negative shift (-)
   * await rig.setRepeaterShift('NONE');  // No shift (simplex)
   */
  setRepeaterShift(shift: 'NONE' | 'MINUS' | 'PLUS' | '-' | '+' | 'none' | 'minus' | 'plus', vfo?: VFO): Promise<number>;

  /**
   * Get current repeater shift direction
   * @param vfo Optional VFO to get repeater shift from ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Current repeater shift direction
   * @example
   * const shift = await rig.getRepeaterShift();
   * console.log(`Repeater shift: ${shift}`); // 'None', 'Minus', or 'Plus'
   */
  getRepeaterShift(vfo?: VFO): Promise<string>;

  /**
   * Set repeater offset frequency
   * @param offset Repeater offset in Hz
   * @param vfo Optional VFO to set repeater offset on ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Success status
   * @example
   * await rig.setRepeaterOffset(600000);  // 600 kHz offset (2m band)
   * await rig.setRepeaterOffset(5000000); // 5 MHz offset (70cm band)
   */
  setRepeaterOffset(offset: number, vfo?: VFO): Promise<number>;

  /**
   * Get current repeater offset frequency
   * @param vfo Optional VFO to get repeater offset from ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Current repeater offset in Hz
   * @example
   * const offset = await rig.getRepeaterOffset();
   * console.log(`Repeater offset: ${offset} Hz`);
   */
  getRepeaterOffset(vfo?: VFO): Promise<number>;

  // CTCSS/DCS Tone Control
  
  /**
   * Set CTCSS tone frequency
   * @param tone CTCSS tone frequency in tenths of Hz (e.g., 1000 for 100.0 Hz)
   * @param vfo Optional VFO to set tone on ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Success status
   * @example
   * await rig.setCtcssTone(1000); // Set 100.0 Hz CTCSS tone
   * await rig.setCtcssTone(1318); // Set 131.8 Hz CTCSS tone
   */
  setCtcssTone(tone: number, vfo?: VFO): Promise<number>;

  /**
   * Get current CTCSS tone frequency
   * @param vfo Optional VFO to get tone from ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Current CTCSS tone frequency in tenths of Hz
   * @example
   * const tone = await rig.getCtcssTone();
   * console.log(`CTCSS tone: ${tone / 10} Hz`);
   */
  getCtcssTone(vfo?: VFO): Promise<number>;

  /**
   * Set DCS code
   * @param code DCS code (e.g., 23, 174, 754)
   * @param vfo Optional VFO to set code on ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Success status
   * @example
   * await rig.setDcsCode(23);  // Set DCS code 023
   * await rig.setDcsCode(174); // Set DCS code 174
   */
  setDcsCode(code: number, vfo?: VFO): Promise<number>;

  /**
   * Get current DCS code
   * @param vfo Optional VFO to get code from ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Current DCS code
   * @example
   * const code = await rig.getDcsCode();
   * console.log(`DCS code: ${code.toString().padStart(3, '0')}`);
   */
  getDcsCode(vfo?: VFO): Promise<number>;

  /**
   * Set CTCSS SQL (squelch) tone frequency
   * @param tone CTCSS SQL tone frequency in tenths of Hz (e.g., 1000 for 100.0 Hz)
   * @param vfo Optional VFO to set tone on ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Success status
   * @example
   * await rig.setCtcssSql(1000); // Set 100.0 Hz CTCSS SQL tone
   */
  setCtcssSql(tone: number, vfo?: VFO): Promise<number>;

  /**
   * Get current CTCSS SQL tone frequency
   * @param vfo Optional VFO to get tone from ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Current CTCSS SQL tone frequency in tenths of Hz
   * @example
   * const tone = await rig.getCtcssSql();
   * console.log(`CTCSS SQL tone: ${tone / 10} Hz`);
   */
  getCtcssSql(vfo?: VFO): Promise<number>;

  /**
   * Set DCS SQL (squelch) code
   * @param code DCS SQL code (e.g., 23, 174, 754)
   * @param vfo Optional VFO to set code on ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Success status
   * @example
   * await rig.setDcsSql(23); // Set DCS SQL code 023
   */
  setDcsSql(code: number, vfo?: VFO): Promise<number>;

  /**
   * Get current DCS SQL code
   * @param vfo Optional VFO to get code from ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Current DCS SQL code
   * @example
   * const code = await rig.getDcsSql();
   * console.log(`DCS SQL code: ${code.toString().padStart(3, '0')}`);
   */
  getDcsSql(vfo?: VFO): Promise<number>;

  // Parameter Control

  /**
   * Set radio parameter
   * @param paramName Parameter name (e.g., 'ANN', 'APO', 'BACKLIGHT', 'BEEP', 'TIME', 'BAT')
   * @param value Parameter value (numeric)
   * @returns Success status
   * @example
   * await rig.setParm('BACKLIGHT', 0.5); // Set backlight to 50%
   * await rig.setParm('BEEP', 1);        // Enable beep
   */
  setParm(paramName: string, value: number): Promise<number>;

  /**
   * Get radio parameter value
   * @param paramName Parameter name (e.g., 'ANN', 'APO', 'BACKLIGHT', 'BEEP', 'TIME', 'BAT')
   * @returns Parameter value
   * @example
   * const backlight = await rig.getParm('BACKLIGHT');
   * const beep = await rig.getParm('BEEP');
   */
  getParm(paramName: string): Promise<number>;

  // DTMF Support

  /**
   * Send DTMF digits
   * @param digits DTMF digits to send (0-9, A-D, *, #)
   * @param vfo Optional VFO to send from ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Success status
   * @example
   * await rig.sendDtmf('1234'); // Send DTMF sequence 1234
   * await rig.sendDtmf('*70#'); // Send *70# sequence
   */
  sendDtmf(digits: string, vfo?: VFO): Promise<number>;

  /**
   * Receive DTMF digits
   * @param maxLength Maximum number of digits to receive (default: 32)
   * @param vfo Optional VFO to receive from ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Received DTMF digits and count
   * @example
   * const result = await rig.recvDtmf(10);
   * console.log(`Received DTMF: ${result.digits} (${result.length} digits)`);
   */
  recvDtmf(maxLength?: number, vfo?: VFO): Promise<{digits: string, length: number}>;

  // Memory Channel Advanced Operations

  /**
   * Get current memory channel number
   * @param vfo Optional VFO to get memory channel from ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Current memory channel number
   * @example
   * const currentChannel = await rig.getMem();
   * console.log(`Current memory channel: ${currentChannel}`);
   */
  getMem(vfo?: VFO): Promise<number>;

  /**
   * Set memory bank
   * @param bank Bank number to select
   * @param vfo Optional VFO to set bank on ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Success status
   * @example
   * await rig.setBank(1); // Select memory bank 1
   * await rig.setBank(2); // Select memory bank 2
   */
  setBank(bank: number, vfo?: VFO): Promise<number>;

  /**
   * Get total number of memory channels available
   * @returns Total number of memory channels
   * @example
   * const totalChannels = await rig.memCount();
   * console.log(`Total memory channels: ${totalChannels}`);
   */
  memCount(): Promise<number>;

  // Morse Code Support

  /**
   * Send Morse code message
   * @param message Morse code message to send (text will be converted to Morse)
   * @param vfo Optional VFO to send from ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Success status
   * @example
   * await rig.sendMorse('CQ CQ DE VK3ABC K'); // Send CQ call
   * await rig.sendMorse('TEST MSG');          // Send test message
   */
  sendMorse(message: string, vfo?: VFO): Promise<number>;

  /**
   * Stop current Morse code transmission
   * @param vfo Optional VFO to stop ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Success status
   * @example
   * await rig.stopMorse(); // Stop ongoing Morse transmission
   */
  stopMorse(vfo?: VFO): Promise<number>;

  /**
   * Wait for Morse code transmission to complete
   * @param vfo Optional VFO to wait on ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Success status when transmission completes
   * @example
   * await rig.sendMorse('TEST');
   * await rig.waitMorse(); // Wait for transmission to complete
   */
  waitMorse(vfo?: VFO): Promise<number>;

  // Voice Memory Support

  /**
   * Play voice memory channel
   * @param channel Voice memory channel number to play
   * @param vfo Optional VFO to play from ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Success status
   * @example
   * await rig.sendVoiceMem(1); // Play voice memory channel 1
   * await rig.sendVoiceMem(3); // Play voice memory channel 3
   */
  sendVoiceMem(channel: number, vfo?: VFO): Promise<number>;

  /**
   * Stop voice memory playback
   * @param vfo Optional VFO to stop ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Success status
   * @example
   * await rig.stopVoiceMem(); // Stop ongoing voice memory playback
   */
  stopVoiceMem(vfo?: VFO): Promise<number>;

  // Complex Split Frequency/Mode Operations

  /**
   * Set split frequency and mode simultaneously
   * @param txFrequency TX frequency in Hz
   * @param txMode TX mode ('USB', 'LSB', 'FM', etc.)
   * @param txWidth TX bandwidth in Hz
   * @param vfo Optional VFO for split operation ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Success status
   * @example
   * await rig.setSplitFreqMode(14205000, 'USB', 2400); // Set split: TX on 14.205 MHz USB 2400Hz width
   * await rig.setSplitFreqMode(145520000, 'FM', 15000); // Set split: TX on 145.52 MHz FM 15kHz width
   */
  setSplitFreqMode(txFrequency: number, txMode: RadioMode, txWidth: number, vfo?: VFO): Promise<number>;

  /**
   * Get split frequency and mode information
   * @param vfo Optional VFO to get split info from ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Object containing TX frequency, mode, and width
   * @example
   * const splitInfo = await rig.getSplitFreqMode();
   * console.log(`Split TX: ${splitInfo.txFrequency} Hz, Mode: ${splitInfo.txMode}, Width: ${splitInfo.txWidth} Hz`);
   */
  getSplitFreqMode(vfo?: VFO): Promise<{txFrequency: number, txMode: string, txWidth: number}>;

  // Antenna Control

  /**
   * Set antenna selection
   * @param antenna Antenna number to select (1-based)
   * @param vfo Optional VFO to set antenna on ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Success status
   * @example
   * await rig.setAntenna(1); // Select antenna 1
   * await rig.setAntenna(2); // Select antenna 2
   */
  setAntenna(antenna: number, vfo?: VFO): Promise<number>;

  /**
   * Get comprehensive antenna information
   * @param vfo Optional VFO to get antenna info from ('VFO-A' or 'VFO-B'). If not specified, uses current VFO
   * @returns Antenna information object containing current, TX, RX antenna settings and option
   * @example
   * const antennaInfo = await rig.getAntenna();
   * console.log(`Current: ${antennaInfo.currentAntenna}`);
   * console.log(`TX: ${antennaInfo.txAntenna}, RX: ${antennaInfo.rxAntenna}`);
   * console.log(`Option: ${antennaInfo.option}`);
   */
  getAntenna(vfo?: VFO): Promise<AntennaInfo>;

  // Power Conversion Functions

  /**
   * Convert power level to milliwatts
   * @param power Power level (0.0-1.0, where 1.0 = 100% power)
   * @param frequency Frequency in Hz
   * @param mode Radio mode ('USB', 'LSB', 'FM', 'AM', etc.)
   * @returns Power in milliwatts
   * @example
   * const milliwatts = await rig.power2mW(0.5, 14205000, 'USB');
   * console.log(`50% power = ${milliwatts} mW`);
   */
  power2mW(power: number, frequency: number, mode: RadioMode): Promise<number>;

  /**
   * Convert milliwatts to power level
   * @param milliwatts Power in milliwatts
   * @param frequency Frequency in Hz
   * @param mode Radio mode ('USB', 'LSB', 'FM', 'AM', etc.)
   * @returns Power level (0.0-1.0, where 1.0 = 100% power)
   * @example
   * const powerLevel = await rig.mW2power(5000, 14205000, 'USB');
   * console.log(`5000 mW = ${(powerLevel * 100).toFixed(1)}% power`);
   */
  mW2power(milliwatts: number, frequency: number, mode: RadioMode): Promise<number>;

  // Reset Function

  /**
   * Reset radio to default state
   * @param resetType Reset type ('NONE', 'SOFT', 'VFO', 'MCALL', 'MASTER'). Default: 'SOFT'
   *   - NONE: No reset
   *   - SOFT: Soft reset (preserve some settings)
   *   - VFO: VFO reset
   *   - MCALL: Memory clear
   *   - MASTER: Master reset (complete factory reset)
   * @returns Success status
   * @example
   * await rig.reset('SOFT');   // Soft reset
   * await rig.reset('MASTER'); // Factory reset (CAUTION: loses all settings!)
   * await rig.reset();         // Default soft reset
   */
  reset(resetType?: 'NONE' | 'SOFT' | 'VFO' | 'MCALL' | 'MASTER'): Promise<number>;
}

/**
 * node-hamlib module export object
 */
declare const nodeHamlib: {
  HamLib: typeof HamLib;
};

// Export types for use elsewhere
export { ConnectionInfo, ModeInfo, SupportedRigInfo, AntennaInfo, VFO, RadioMode, MemoryChannelData, 
         MemoryChannelInfo, SplitModeInfo, SplitStatusInfo, LevelType, FunctionType, 
         ScanType, VfoOperationType, SerialConfigParam, PttType, DcdType, SerialConfigOptions, HamLib };

// Support both CommonJS and ES module exports
// @ts-ignore
export = nodeHamlib;
export default nodeHamlib; 