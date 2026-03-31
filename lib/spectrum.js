const { EventEmitter } = require('events');

const DEFAULT_SPECTRUM_EDGE_SLOTS = [1, 2, 3, 4];

function normalizeSpectrumModeName(name) {
  const normalized = String(name || '').trim().toLowerCase();
  if (normalized === 'center') return 'center';
  if (normalized === 'fixed') return 'fixed';
  if (normalized === 'center scroll' || normalized === 'center-scroll' || normalized === 'scroll-center') return 'scroll-center';
  if (normalized === 'fixed scroll' || normalized === 'fixed-scroll' || normalized === 'scroll-fixed') return 'scroll-fixed';
  return null;
}

class SpectrumController extends EventEmitter {
  constructor(rig) {
    super();

    if (!rig || typeof rig !== 'object') {
      throw new TypeError('Expected a HamLib instance');
    }

    const requiredMethods = [
      'getSpectrumCapabilities',
      'getSupportedFunctions',
      'getSupportedLevels',
      'setLevel',
      'getLevel',
      'setFunction',
      'setConf',
      'getConf',
      'startSpectrumStream',
      'stopSpectrumStream',
    ];

    for (const method of requiredMethods) {
      if (typeof rig[method] !== 'function') {
        throw new TypeError(`Expected a HamLib instance with method ${method}()`);
      }
    }

    this._rig = rig;
    this._managedSpectrumRunning = false;
    this._lastSpectrumLine = null;
    this._pumpTimer = null;
    this._pumpInFlight = false;
  }

  _normalizePumpIntervalMs(value) {
    if (value === false || value === 0) {
      return 0;
    }
    if (value === undefined || value === null) {
      return 200;
    }

    const parsed = Number(value);
    if (!Number.isFinite(parsed) || parsed < 0) {
      throw new Error(`Invalid pumpIntervalMs: ${value}`);
    }
    return parsed;
  }

  _startPump(intervalMs) {
    this._stopPump();

    if (!intervalMs) {
      return;
    }

    // Some backends only flush async spectrum data while normal CAT traffic continues.
    this._pumpTimer = setInterval(() => {
      if (!this._managedSpectrumRunning || this._pumpInFlight) {
        return;
      }

      this._pumpInFlight = true;
      this._rig.getFrequency()
        .catch(() => {})
        .finally(() => {
          this._pumpInFlight = false;
        });
    }, intervalMs);
  }

  _stopPump() {
    if (this._pumpTimer) {
      clearInterval(this._pumpTimer);
      this._pumpTimer = null;
    }
    this._pumpInFlight = false;
  }

  _recordSpectrumLine(line) {
    if (!line || typeof line !== 'object') {
      return line;
    }
    this._lastSpectrumLine = line;
    return line;
  }

  _getLastSpectrumDisplayStateFromLine() {
    const line = this._lastSpectrumLine;
    if (!line || typeof line !== 'object') {
      return null;
    }

    return {
      modeId: Number.isFinite(line.mode) ? line.mode : null,
      spanHz: Number.isFinite(line.spanHz) ? line.spanHz : null,
      edgeLowHz: Number.isFinite(line.lowEdgeFreq) ? line.lowEdgeFreq : null,
      edgeHighHz: Number.isFinite(line.highEdgeFreq) ? line.highEdgeFreq : null,
    };
  }

  async getSpectrumSupportSummary() {
    const capabilities = await this._rig.getSpectrumCapabilities();
    const supportedFunctions = this._rig.getSupportedFunctions();
    const supportedLevels = this._rig.getSupportedLevels();
    const hasSpectrumFunction = supportedFunctions.includes('SPECTRUM');
    const hasSpectrumHoldFunction = supportedFunctions.includes('SPECTRUM_HOLD');
    const hasTransceiveFunction = supportedFunctions.includes('TRANSCEIVE');
    const asyncDataSupported = capabilities.asyncDataSupported ?? hasSpectrumFunction;
    const configurableLevels = ['SPECTRUM_MODE', 'SPECTRUM_SPAN', 'SPECTRUM_EDGE_LOW', 'SPECTRUM_EDGE_HIGH', 'SPECTRUM_SPEED', 'SPECTRUM_REF', 'SPECTRUM_AVG']
      .filter((name) => supportedLevels.includes(name));
    let supportsEdgeSlotSelection = false;

    try {
      const edgeSlot = await this._rig.getConf('SPECTRUM_EDGE');
      supportsEdgeSlotSelection = Number.isFinite(Number.parseInt(String(edgeSlot), 10));
    } catch (_) {
      supportsEdgeSlotSelection = false;
    }

    return {
      supported: Boolean(hasSpectrumFunction),
      asyncDataSupported: Boolean(asyncDataSupported),
      hasSpectrumFunction,
      hasSpectrumHoldFunction,
      hasTransceiveFunction,
      configurableLevels,
      supportsFixedEdges: configurableLevels.includes('SPECTRUM_EDGE_LOW') && configurableLevels.includes('SPECTRUM_EDGE_HIGH'),
      supportsEdgeSlotSelection,
      supportedEdgeSlots: supportsEdgeSlotSelection ? [...DEFAULT_SPECTRUM_EDGE_SLOTS] : [],
      scopes: capabilities.scopes ?? [],
      modes: capabilities.modes ?? [],
      spans: capabilities.spans ?? [],
      avgModes: capabilities.avgModes ?? [],
    };
  }

  async configureSpectrum(config = {}) {
    const summary = await this.getSpectrumSupportSummary();
    const applyLevel = async (name, value) => {
      if (value === undefined || !summary.configurableLevels.includes(name)) return;
      await this._rig.setLevel(name, value);
    };
    const resolveModeId = async (mode) => {
      if (mode === undefined || mode === null) return undefined;
      if (Number.isFinite(mode)) return mode;
      const requested = normalizeSpectrumModeName(mode);
      if (!requested) {
        throw new Error(`Unsupported spectrum mode: ${mode}`);
      }
      const matched = (summary.modes ?? []).find((entry) => normalizeSpectrumModeName(entry?.name) === requested);
      if (!matched) {
        throw new Error(`Spectrum mode not supported by this backend: ${mode}`);
      }
      return matched.id;
    };

    if (summary.hasSpectrumHoldFunction && config.hold !== undefined) {
      await this._rig.setFunction('SPECTRUM_HOLD', Boolean(config.hold));
    }

    if (config.edgeSlot !== undefined && summary.supportsEdgeSlotSelection) {
      await this.setSpectrumEdgeSlot(config.edgeSlot);
    }

    await applyLevel('SPECTRUM_MODE', await resolveModeId(config.mode));
    await applyLevel('SPECTRUM_SPAN', config.spanHz);
    if (config.edgeLowHz !== undefined || config.edgeHighHz !== undefined) {
      await this.setSpectrumFixedEdges({
        lowHz: config.edgeLowHz,
        highHz: config.edgeHighHz,
      });
    }
    await applyLevel('SPECTRUM_SPEED', config.speed);
    await applyLevel('SPECTRUM_REF', config.referenceLevel);
    await applyLevel('SPECTRUM_AVG', config.averageMode);

    return summary;
  }

  async getSpectrumEdgeSlot() {
    const raw = await this._rig.getConf('SPECTRUM_EDGE');
    const parsed = Number.parseInt(String(raw), 10);
    if (!Number.isFinite(parsed)) {
      throw new Error('Spectrum edge slot is not available');
    }
    return parsed;
  }

  async setSpectrumEdgeSlot(slot) {
    const parsed = Number.parseInt(String(slot), 10);
    if (!Number.isFinite(parsed) || parsed < 1) {
      throw new Error(`Invalid spectrum edge slot: ${slot}`);
    }
    await this._rig.setConf('SPECTRUM_EDGE', String(parsed));
    return parsed;
  }

  async getSpectrumSupportedEdgeSlots() {
    const summary = await this.getSpectrumSupportSummary();
    return summary.supportedEdgeSlots ?? [];
  }

  async getSpectrumFixedEdges() {
    const lineState = this._getLastSpectrumDisplayStateFromLine();
    if (lineState?.edgeLowHz !== null && lineState?.edgeHighHz !== null) {
      return { lowHz: lineState.edgeLowHz, highHz: lineState.edgeHighHz };
    }

    const [lowHz, highHz] = await Promise.all([
      this._rig.getLevel('SPECTRUM_EDGE_LOW'),
      this._rig.getLevel('SPECTRUM_EDGE_HIGH'),
    ]);
    return { lowHz, highHz };
  }

  async setSpectrumFixedEdges({ lowHz, highHz }) {
    if (!Number.isFinite(lowHz) || !Number.isFinite(highHz) || lowHz >= highHz) {
      throw new Error('Spectrum fixed edge range must satisfy lowHz < highHz');
    }
    await this._rig.setLevel('SPECTRUM_EDGE_LOW', lowHz);
    await this._rig.setLevel('SPECTRUM_EDGE_HIGH', highHz);
    return { lowHz, highHz };
  }

  async getSpectrumDisplayState() {
    const summary = await this.getSpectrumSupportSummary();
    const lineState = this._getLastSpectrumDisplayStateFromLine();
    const [queriedModeId, queriedSpanHz, fixedEdges, edgeSlot] = await Promise.all([
      summary.configurableLevels.includes('SPECTRUM_MODE') ? this._rig.getLevel('SPECTRUM_MODE') : Promise.resolve(null),
      summary.configurableLevels.includes('SPECTRUM_SPAN') ? this._rig.getLevel('SPECTRUM_SPAN') : Promise.resolve(null),
      summary.supportsFixedEdges ? this.getSpectrumFixedEdges().catch(() => null) : Promise.resolve(null),
      summary.supportsEdgeSlotSelection ? this.getSpectrumEdgeSlot().catch(() => null) : Promise.resolve(null),
    ]);

    const modeId = queriedModeId ?? lineState?.modeId ?? null;
    const spanHz = queriedSpanHz ?? lineState?.spanHz ?? null;
    const modeInfo = (summary.modes ?? []).find((entry) => entry.id === modeId) ?? null;
    const mode = normalizeSpectrumModeName(modeInfo?.name);
    const edgeLowHz = fixedEdges?.lowHz ?? lineState?.edgeLowHz ?? null;
    const edgeHighHz = fixedEdges?.highHz ?? lineState?.edgeHighHz ?? null;
    const derivedSpanHz = (edgeLowHz !== null && edgeHighHz !== null) ? (edgeHighHz - edgeLowHz) : null;

    return {
      mode,
      modeId,
      modeName: modeInfo?.name ?? null,
      spanHz: spanHz ?? derivedSpanHz,
      edgeSlot,
      edgeLowHz,
      edgeHighHz,
      supportedModes: summary.modes ?? [],
      supportedSpans: summary.spans ?? [],
      supportedEdgeSlots: summary.supportedEdgeSlots ?? [],
      supportsFixedEdges: Boolean(summary.supportsFixedEdges),
      supportsEdgeSlotSelection: Boolean(summary.supportsEdgeSlotSelection),
    };
  }

  async configureSpectrumDisplay(config = {}) {
    const normalizedConfig = { ...config };
    if ((normalizedConfig.mode === 'fixed' || normalizedConfig.mode === 'scroll-fixed')
      && normalizedConfig.edgeLowHz !== undefined
      && normalizedConfig.edgeHighHz !== undefined
      && normalizedConfig.edgeLowHz >= normalizedConfig.edgeHighHz) {
      throw new Error('Spectrum fixed edge range must satisfy edgeLowHz < edgeHighHz');
    }

    await this.configureSpectrum(normalizedConfig);
    return this.getSpectrumDisplayState();
  }

  async startManagedSpectrum(config = {}) {
    const summary = await this.getSpectrumSupportSummary();
    if (!summary.supported) {
      throw new Error('Official Hamlib spectrum streaming is not supported by this rig/backend');
    }

    if (this._managedSpectrumRunning) {
      return true;
    }

    const pumpIntervalMs = this._normalizePumpIntervalMs(config.pumpIntervalMs);

    await this._rig.startSpectrumStream((line) => {
      const recorded = this._recordSpectrumLine(line);
      this.emit('spectrumLine', recorded);
    });

    try {
      try {
        await this._rig.setConf('async', '1');
      } catch (_) {
        try {
          await this._rig.setConf('async', 'True');
        } catch (_) {
          // Not every backend accepts the config write even if async is supported.
        }
      }

      await this.configureSpectrum({ hold: false, ...config });
      await this._rig.setFunction('SPECTRUM', true);
      if (summary.hasTransceiveFunction) {
        await this._rig.setFunction('TRANSCEIVE', true);
      }
    } catch (error) {
      this.emit('spectrumError', error);
      try {
        await this.stopManagedSpectrum();
      } catch (_) {
        // Ignore cleanup failures and surface the original startup error.
      }
      throw error;
    }

    this._managedSpectrumRunning = true;
    this._startPump(pumpIntervalMs);
    this.emit('spectrumStateChanged', { active: true });
    return true;
  }

  async stopManagedSpectrum() {
    if (!this._managedSpectrumRunning) {
      return true;
    }

    try {
      const supportedFunctions = this._rig.getSupportedFunctions();
      if (supportedFunctions.includes('TRANSCEIVE')) {
        await this._rig.setFunction('TRANSCEIVE', false);
      }
      if (supportedFunctions.includes('SPECTRUM_HOLD')) {
        await this._rig.setFunction('SPECTRUM_HOLD', false);
      }
      if (supportedFunctions.includes('SPECTRUM')) {
        await this._rig.setFunction('SPECTRUM', false);
      }
    } finally {
      this._stopPump();
      await this._rig.stopSpectrumStream();
    }

    this._managedSpectrumRunning = false;
    this.emit('spectrumStateChanged', { active: false });
    return true;
  }
}

module.exports = { SpectrumController };
module.exports.SpectrumController = SpectrumController;
module.exports.default = { SpectrumController };
