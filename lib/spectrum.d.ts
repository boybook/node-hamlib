import { EventEmitter } from 'events';
import type {
  HamLib,
  SpectrumCapabilities,
  SpectrumConfig,
  SpectrumDisplayState,
  SpectrumLine,
  SpectrumSupportSummary,
} from '../index';

interface ManagedSpectrumConfig extends SpectrumConfig {
  /**
   * Interval for lightweight CAT pump reads while managed spectrum is active.
   * Set to 0 or false to disable the helper-side pump.
   * Default: 200ms
   */
  pumpIntervalMs?: number | false;
}

declare class SpectrumController extends EventEmitter {
  constructor(rig: HamLib);

  getSpectrumSupportSummary(): Promise<SpectrumSupportSummary>;
  configureSpectrum(config?: SpectrumConfig): Promise<SpectrumSupportSummary>;
  getSpectrumEdgeSlot(): Promise<number>;
  setSpectrumEdgeSlot(slot: number): Promise<number>;
  getSpectrumSupportedEdgeSlots(): Promise<number[]>;
  getSpectrumFixedEdges(): Promise<{lowHz: number, highHz: number}>;
  setSpectrumFixedEdges(range: {lowHz: number, highHz: number}): Promise<{lowHz: number, highHz: number}>;
  getSpectrumDisplayState(): Promise<SpectrumDisplayState>;
  configureSpectrumDisplay(config?: SpectrumConfig): Promise<SpectrumDisplayState>;
  startManagedSpectrum(config?: ManagedSpectrumConfig): Promise<boolean>;
  stopManagedSpectrum(): Promise<boolean>;

  on(event: 'spectrumLine', listener: (line: SpectrumLine) => void): this;
  once(event: 'spectrumLine', listener: (line: SpectrumLine) => void): this;
  off(event: 'spectrumLine', listener: (line: SpectrumLine) => void): this;

  on(event: 'spectrumStateChanged', listener: (state: { active: boolean }) => void): this;
  once(event: 'spectrumStateChanged', listener: (state: { active: boolean }) => void): this;
  off(event: 'spectrumStateChanged', listener: (state: { active: boolean }) => void): this;

  on(event: 'spectrumError', listener: (error: Error) => void): this;
  once(event: 'spectrumError', listener: (error: Error) => void): this;
  off(event: 'spectrumError', listener: (error: Error) => void): this;
}

declare const spectrumModule: {
  SpectrumController: typeof SpectrumController;
};

export { SpectrumController };
export type {
  HamLib,
  ManagedSpectrumConfig,
  SpectrumCapabilities,
  SpectrumConfig,
  SpectrumDisplayState,
  SpectrumLine,
  SpectrumSupportSummary,
};

// @ts-ignore
export = spectrumModule;
export default spectrumModule;
