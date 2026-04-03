#!/usr/bin/env node

/**
 * CI Functional Test - validates compiled binary works correctly
 * Uses Hamlib Dummy device (Model 1) directly, no rigctld needed.
 * Tests core API operations to ensure the native addon is functional.
 */

const { HamLib, Rotator } = require('../index.js');

let passed = 0;
let failed = 0;

function ok(desc) {
  passed++;
  console.log(`  PASS: ${desc}`);
}

function fail(desc, err) {
  failed++;
  console.log(`  FAIL: ${desc} - ${err}`);
}

async function test(desc, fn) {
  try {
    await fn();
    ok(desc);
  } catch (e) {
    fail(desc, e.message.split('\n')[0]);
  }
}

function assert(condition, msg) {
  if (!condition) throw new Error(msg || 'Assertion failed');
}

function assertThrows(fn, pattern) {
  let caught = null;
  try {
    fn();
  } catch (error) {
    caught = error;
  }

  if (!caught) {
    throw new Error('Expected function to throw');
  }

  if (pattern && !pattern.test(caught.message)) {
    throw new Error(`Unexpected error: ${caught.message}`);
  }
}

async function assertRejects(fn, pattern) {
  let caught = null;
  try {
    await fn();
  } catch (error) {
    caught = error;
  }

  if (!caught) {
    throw new Error('Expected promise to reject');
  }

  if (pattern && !pattern.test(caught.message)) {
    throw new Error(`Unexpected error: ${caught.message}`);
  }
}

async function run() {
  console.log('=== CI Functional Test ===\n');

  // --- Static methods ---
  console.log('[Static Methods]');

  await test('getHamlibVersion returns string', () => {
    const ver = HamLib.getHamlibVersion();
    assert(typeof ver === 'string' && ver.length > 0, `got: ${ver}`);
  });

  await test('getSupportedRigs returns non-empty array', () => {
    const rigs = HamLib.getSupportedRigs();
    assert(Array.isArray(rigs) && rigs.length > 100, `got ${rigs.length} rigs`);
    assert(rigs[0].rigModel !== undefined, 'rig entry missing rigModel');
    assert(rigs[0].modelName !== undefined, 'rig entry missing modelName');
  });

  // --- Instance lifecycle ---
  console.log('\n[Instance Lifecycle]');

  const rig = new HamLib(1);

  await test('constructor creates instance', () => {
    assert(rig !== null && rig !== undefined);
  });

  await test('getConnectionInfo before open', () => {
    const info = rig.getConnectionInfo();
    assert(info.isOpen === false, 'should not be open yet');
    assert(info.originalModel === 1, `model should be 1, got ${info.originalModel}`);
  });

  await test('getConfigSchema works before open', () => {
    const schema = rig.getConfigSchema();
    assert(Array.isArray(schema), 'schema should be an array');
    assert(schema.length > 0, 'schema should not be empty');
    assert(typeof schema[0].name === 'string', 'schema item missing name');
    assert(schema.some((field) => field.name === 'rig_pathname'), 'schema should include rig_pathname');
  });

  await test('getPortCaps works before open', () => {
    const caps = rig.getPortCaps();
    assert(caps && typeof caps === 'object', 'caps should be an object');
    assert(typeof caps.portType === 'string', 'caps.portType should be a string');
    assert(typeof caps.timeout === 'number', 'caps.timeout should be a number');
  });

  await test('FlexRadio direct backend schema exposes network endpoint fields', async () => {
    const flexRig = new HamLib(2036);
    try {
      const schema = flexRig.getConfigSchema();
      const caps = flexRig.getPortCaps();
      assert(schema.some((field) => field.name === 'rig_pathname'), 'Flex schema should include rig_pathname');
      assert(schema.some((field) => field.name === 'client'), 'Flex schema should include client');
      assert(caps.portType === 'network', `Flex port type should be network, got ${caps.portType}`);
    } finally {
      await flexRig.destroy();
    }
  });

  await test('IC-705 port caps expose serial defaults', async () => {
    const ic705 = new HamLib(3085);
    try {
      const caps = ic705.getPortCaps();
      assert(caps.portType === 'serial', `IC-705 port type should be serial, got ${caps.portType}`);
      assert(caps.serialRateMax === 19200, `IC-705 serialRateMax should be 19200, got ${caps.serialRateMax}`);
      assert(caps.serialDataBits === 8, `IC-705 serialDataBits should be 8, got ${caps.serialDataBits}`);
    } finally {
      await ic705.destroy();
    }
  });

  await test('setConf works before open', async () => {
    await rig.setConf('rig_pathname', '/dev/null');
  });

  await test('open() succeeds', async () => {
    const result = await rig.open();
    assert(result === 0, `expected 0, got ${result}`);
  });

  await test('getConnectionInfo after open', () => {
    const info = rig.getConnectionInfo();
    assert(info.isOpen === true, 'should be open');
  });

  // --- VFO operations ---
  console.log('\n[VFO Operations]');

  await test('setVfo VFOA', async () => {
    await rig.setVfo('VFOA');
  });

  await test('getVfo returns VFOA', async () => {
    const vfo = await rig.getVfo();
    assert(vfo === 'VFOA', `expected VFOA, got ${vfo}`);
  });

  await test('setVfo VFOB', async () => {
    await rig.setVfo('VFOB');
  });

  await test('getVfo returns VFOB', async () => {
    const vfo = await rig.getVfo();
    assert(vfo === 'VFOB', `expected VFOB, got ${vfo}`);
  });

  await test('setVfo accepts Hamlib token VFOA', async () => {
    await rig.setVfo('VFOA');
    const vfo = await rig.getVfo();
    assert(vfo === 'VFOA', `expected VFOA, got ${vfo}`);
  });

  // --- Frequency operations ---
  console.log('\n[Frequency Operations]');

  await test('setFrequency 144.39MHz on VFOA', async () => {
    await rig.setFrequency(144390000, 'VFOA');
  });

  await test('getFrequency returns 144.39MHz', async () => {
    const freq = await rig.getFrequency('VFOA');
    assert(freq === 144390000, `expected 144390000, got ${freq}`);
  });

  await test('setFrequency 432.1MHz on VFOB', async () => {
    await rig.setFrequency(432100000, 'VFOB');
  });

  await test('getFrequency VFOB returns 432.1MHz', async () => {
    const freq = await rig.getFrequency('VFOB');
    assert(freq === 432100000, `expected 432100000, got ${freq}`);
  });

  await test('getFrequency currVFO returns current VFO frequency', async () => {
    const freq = await rig.getFrequency('currVFO');
    assert(freq === 144390000, `expected 144390000, got ${freq}`);
  });

  await test('getFrequency rejects invalid VFO token', async () => {
    await assertRejects(() => rig.getFrequency('NOT-A-VFO'), /Invalid Hamlib VFO token/);
  });

  await test('getFrequency rejects empty-string VFO instead of silently falling back', async () => {
    await assertRejects(() => rig.getFrequency(''), /Invalid Hamlib VFO token/);
  });

  // --- PTT operations ---
  console.log('\n[PTT Operations]');

  await test('setPtt on', async () => {
    await rig.setPtt(true);
  });

  await test('getPtt returns true', async () => {
    const ptt = await rig.getPtt();
    assert(ptt === true, `expected true, got ${ptt}`);
  });

  await test('setPtt off', async () => {
    await rig.setPtt(false);
  });

  await test('getPtt returns false', async () => {
    const ptt = await rig.getPtt();
    assert(ptt === false, `expected false, got ${ptt}`);
  });

  // --- Serial config ---
  console.log('\n[Serial Configuration]');

  await test('setSerialConfig rate', async () => {
    await rig.setSerialConfig('rate', '115200');
  });

  await test('getSerialConfig rate', async () => {
    const rate = await rig.getSerialConfig('rate');
    assert(rate === '115200', `expected 115200, got ${rate}`);
  });

  await test('setSerialConfig data_bits', async () => {
    await rig.setSerialConfig('data_bits', '8');
  });

  await test('getSerialConfig data_bits', async () => {
    const bits = await rig.getSerialConfig('data_bits');
    assert(bits === '8', `expected 8, got ${bits}`);
  });

  await test('getSupportedSerialConfigs returns config object', () => {
    const configs = rig.getSupportedSerialConfigs();
    assert(configs.serial && configs.serial.data_bits, 'missing serial.data_bits');
  });

  // --- PTT/DCD type ---
  console.log('\n[PTT/DCD Type]');

  await test('setPttType RIG', async () => {
    await rig.setPttType('RIG');
  });

  await test('getPttType returns RIG', async () => {
    const type = await rig.getPttType();
    assert(type === 'RIG', `expected RIG, got ${type}`);
  });

  await test('setDcdType RIG', async () => {
    await rig.setDcdType('RIG');
  });

  await test('getDcdType returns RIG', async () => {
    const type = await rig.getDcdType();
    assert(type === 'RIG', `expected RIG, got ${type}`);
  });

  // --- Powerstat ---
  console.log('\n[Power Control]');

  await test('setPowerstat on', async () => {
    await rig.setPowerstat(1);
  });

  await test('getPowerstat returns 1', async () => {
    const ps = await rig.getPowerstat();
    assert(ps === 1, `expected 1, got ${ps}`);
  });

  // --- Lock Mode (Hamlib >= 4.7.0) ---
  console.log('\n[Lock Mode]');

  await test('setLockMode(0) succeeds or returns ENIMPL', async () => {
    try {
      await rig.setLockMode(0);
    } catch (e) {
      if (!e.message.toLowerCase().includes('not implemented') &&
          !e.message.toLowerCase().includes('enimpl')) throw e;
    }
  });

  await test('getLockMode returns number or ENIMPL', async () => {
    try {
      const lock = await rig.getLockMode();
      assert(typeof lock === 'number', `expected number, got ${typeof lock}`);
    } catch (e) {
      if (!e.message.toLowerCase().includes('not implemented') &&
          !e.message.toLowerCase().includes('enimpl')) throw e;
    }
  });

  // --- Clock (Hamlib >= 4.7.0) ---
  console.log('\n[Clock]');

  await test('setClock succeeds or returns ENIMPL', async () => {
    try {
      await rig.setClock({ year: 2026, month: 3, day: 7, hour: 12, min: 0, sec: 0, msec: 0, utcOffset: 0 });
    } catch (e) {
      if (!e.message.toLowerCase().includes('not implemented') &&
          !e.message.toLowerCase().includes('enimpl')) throw e;
    }
  });

  await test('getClock returns object or ENIMPL', async () => {
    try {
      const clock = await rig.getClock();
      assert(typeof clock === 'object', `expected object, got ${typeof clock}`);
      assert(clock.hasOwnProperty('year'), 'missing year field');
    } catch (e) {
      if (!e.message.toLowerCase().includes('not implemented') &&
          !e.message.toLowerCase().includes('enimpl')) throw e;
    }
  });

  // --- VFO Info (Hamlib >= 4.7.0) ---
  console.log('\n[VFO Info]');

  await test('getVfoInfo returns object or ENIMPL', async () => {
    try {
      const info = await rig.getVfoInfo('VFOA');
      assert(typeof info === 'object', `expected object, got ${typeof info}`);
      assert(info.hasOwnProperty('frequency'), 'missing frequency field');
      assert(info.hasOwnProperty('mode'), 'missing mode field');
      assert(info.hasOwnProperty('split'), 'missing split field');
    } catch (e) {
      if (!e.message.toLowerCase().includes('not implemented') &&
          !e.message.toLowerCase().includes('enimpl')) throw e;
    }
  });

  // --- New API: getInfo ---
  console.log('\n[Rig Info]');

  await test('getInfo returns string', async () => {
    const info = await rig.getInfo();
    assert(typeof info === 'string', `expected string, got ${typeof info}`);
  });

  // --- Antenna ---
  console.log('\n[Antenna]');

  await test('setAntenna accepts 1-based antenna numbers', async () => {
    await rig.setAntenna(3);
  });

  await test('getAntenna returns 1-based antenna numbers', async () => {
    const antenna = await rig.getAntenna();
    assert(antenna.currentAntenna === 3, `expected currentAntenna 3, got ${antenna.currentAntenna}`);
  });

  await test('setAntenna accepts option and VFO overload', async () => {
    await rig.setAntenna(2, 7, 'VFOA');
    const antenna = await rig.getAntenna('VFOA');
    assert(antenna.currentAntenna === 2, `expected currentAntenna 2, got ${antenna.currentAntenna}`);
    assert(antenna.option === 7, `expected option 7, got ${antenna.option}`);
  });

  // --- New API: sendRaw ---
  console.log('\n[Send Raw]');

  await test('sendRaw succeeds or returns ENIMPL', async () => {
    try {
      const data = Buffer.from([0xFE, 0xFE]);
      await rig.sendRaw(data, 64);
    } catch (e) {
      if (!e.message.toLowerCase().includes('not implemented') &&
          !e.message.toLowerCase().includes('enimpl') &&
          !e.message.toLowerCase().includes('protocol') &&
          !e.message.toLowerCase().includes('i/o')) throw e;
    }
  });

  // --- New API: setConf / getConf ---
  console.log('\n[Configuration]');

  await test('setConf/getConf or returns error for invalid token', async () => {
    try {
      await rig.setConf('rig_pathname', '/dev/null');
      const val = await rig.getConf('rig_pathname');
      assert(typeof val === 'string', `expected string, got ${typeof val}`);
    } catch (e) {
      // Invalid token or unsupported is acceptable
      if (!e.message.toLowerCase().includes('invalid') &&
          !e.message.toLowerCase().includes('not implemented') &&
          !e.message.toLowerCase().includes('enimpl')) throw e;
    }
  });

  // --- New API: Passband methods ---
  console.log('\n[Passband / Resolution]');

  await test('getPassbandNormal returns number for USB', () => {
    const pb = rig.getPassbandNormal('USB');
    assert(typeof pb === 'number', `expected number, got ${typeof pb}`);
  });

  await test('getPassbandNarrow returns number for USB', () => {
    const pb = rig.getPassbandNarrow('USB');
    assert(typeof pb === 'number', `expected number, got ${typeof pb}`);
  });

  await test('getPassbandWide returns number for USB', () => {
    const pb = rig.getPassbandWide('USB');
    assert(typeof pb === 'number', `expected number, got ${typeof pb}`);
  });

  await test('getResolution returns number for USB', () => {
    const res = rig.getResolution('USB');
    assert(typeof res === 'number', `expected number, got ${typeof res}`);
  });

  // --- New API: Capability queries ---
  console.log('\n[Capability Queries]');

  await test('getSupportedParms returns array', () => {
    const parms = rig.getSupportedParms();
    assert(Array.isArray(parms), `expected array, got ${typeof parms}`);
  });

  await test('getSupportedVfoOps returns array', () => {
    const ops = rig.getSupportedVfoOps();
    assert(Array.isArray(ops), `expected array, got ${typeof ops}`);
  });

  await test('getSupportedScanTypes returns array', () => {
    const types = rig.getSupportedScanTypes();
    assert(Array.isArray(types), `expected array, got ${typeof types}`);
  });

  // --- New API: Static methods ---
  console.log('\n[Static Copyright/License]');

  await test('getCopyright returns non-empty string', () => {
    const copyright = HamLib.getCopyright();
    assert(typeof copyright === 'string' && copyright.length > 0, `got: "${copyright}"`);
  });

  await test('getLicense returns non-empty string', () => {
    const license = HamLib.getLicense();
    assert(typeof license === 'string' && license.length > 0, `got: "${license}"`);
  });

  // --- Capability Query Batch 2 ---
  console.log('\n[Capability Query - Batch 2]');

  await test('getPreampValues returns number[]', () => {
    const vals = rig.getPreampValues();
    assert(Array.isArray(vals), `expected array, got ${typeof vals}`);
    vals.forEach((v, i) => assert(typeof v === 'number', `element ${i} not number`));
  });

  await test('getAttenuatorValues returns number[]', () => {
    const vals = rig.getAttenuatorValues();
    assert(Array.isArray(vals), `expected array, got ${typeof vals}`);
    vals.forEach((v, i) => assert(typeof v === 'number', `element ${i} not number`));
  });

  await test('getMaxRit returns number >= 0', () => {
    const val = rig.getMaxRit();
    assert(typeof val === 'number' && val >= 0, `expected number >= 0, got ${val}`);
  });

  await test('getMaxXit returns number >= 0', () => {
    const val = rig.getMaxXit();
    assert(typeof val === 'number' && val >= 0, `expected number >= 0, got ${val}`);
  });

  await test('getMaxIfShift returns number >= 0', () => {
    const val = rig.getMaxIfShift();
    assert(typeof val === 'number' && val >= 0, `expected number >= 0, got ${val}`);
  });

  await test('getAvailableCtcssTones returns number[]', () => {
    const tones = rig.getAvailableCtcssTones();
    assert(Array.isArray(tones), `expected array, got ${typeof tones}`);
    if (tones.length > 0) {
      assert(tones[0] > 0, `first tone should be > 0, got ${tones[0]}`);
    }
  });

  await test('getAvailableDcsCodes returns number[]', () => {
    const codes = rig.getAvailableDcsCodes();
    assert(Array.isArray(codes), `expected array, got ${typeof codes}`);
    if (codes.length > 0) {
      assert(codes[0] > 0, `first code should be > 0, got ${codes[0]}`);
    }
  });

  await test('getFrequencyRanges returns {rx, tx}', () => {
    const ranges = rig.getFrequencyRanges();
    assert(typeof ranges === 'object', `expected object, got ${typeof ranges}`);
    assert(Array.isArray(ranges.rx), `expected rx array, got ${typeof ranges.rx}`);
    assert(Array.isArray(ranges.tx), `expected tx array, got ${typeof ranges.tx}`);
    if (ranges.rx.length > 0) {
      const r = ranges.rx[0];
      assert(r.startFreq !== undefined, 'missing startFreq');
      assert(r.endFreq !== undefined, 'missing endFreq');
      assert(Array.isArray(r.modes), 'modes should be array');
    }
  });

  await test('getTuningSteps returns TuningStepInfo[]', () => {
    const steps = rig.getTuningSteps();
    assert(Array.isArray(steps), `expected array, got ${typeof steps}`);
    if (steps.length > 0) {
      const s = steps[0];
      assert(Array.isArray(s.modes), 'modes should be array');
      assert(typeof s.stepHz === 'number', `stepHz should be number, got ${typeof s.stepHz}`);
    }
  });

  await test('getFilterList returns FilterInfo[]', () => {
    const filters = rig.getFilterList();
    assert(Array.isArray(filters), `expected array, got ${typeof filters}`);
    if (filters.length > 0) {
      const f = filters[0];
      assert(Array.isArray(f.modes), 'modes should be array');
      assert(typeof f.width === 'number', `width should be number, got ${typeof f.width}`);
    }
  });

  await test('getMemoryChannel requires explicit readOnly flag', async () => {
    await assertRejects(() => rig.getMemoryChannel(1), /Expected \(channelNumber: number, readOnly: boolean\)/);
  });

  await test('startScan requires explicit channel argument', async () => {
    await assertRejects(() => rig.startScan('VFO'), /Expected \(scanType: string, channel: number\)/);
  });

  await test('recvDtmf requires explicit maxLength', async () => {
    await assertRejects(() => rig.recvDtmf(), /Expected \(maxLength: number, vfo\?: string\)/);
  });

  await test('reset requires explicit resetType', async () => {
    await assertRejects(() => rig.reset(), /Expected \(resetType: string\)/);
  });

  // --- Rotator ---
  console.log('\n[Rotator]');

  await test('Rotator.getSupportedRotators returns non-empty array', () => {
    const rotators = Rotator.getSupportedRotators();
    assert(Array.isArray(rotators) && rotators.length > 0, `got ${rotators.length} rotators`);
    assert(rotators[0].rotModel !== undefined, 'rotator entry missing rotModel');
  });

  const rotator = new Rotator(1);

  await test('rotator constructor creates instance', () => {
    assert(rotator !== null && rotator !== undefined);
  });

  await test('rotator getConfigSchema works before open', () => {
    const schema = rotator.getConfigSchema();
    assert(Array.isArray(schema), 'schema should be an array');
    assert(schema.length > 0, 'schema should not be empty');
    assert(schema.some((field) => field.name === 'rot_pathname'), 'schema should include rot_pathname');
  });

  await test('rotator getPortCaps works before open', () => {
    const caps = rotator.getPortCaps();
    assert(caps && typeof caps === 'object', 'caps should be an object');
    assert(typeof caps.portType === 'string', 'caps.portType should be a string');
  });

  await test('rotator setConf works before open', async () => {
    await rotator.setConf('rot_pathname', '/dev/null');
  });

  await test('rotator open() succeeds', async () => {
    const result = await rotator.open();
    assert(result === 0, `expected 0, got ${result}`);
  });

  await test('rotator getConnectionInfo after open', () => {
    const info = rotator.getConnectionInfo();
    assert(info.isOpen === true, 'should be open');
  });

  await test('rotator getRotatorCaps returns expected shape', () => {
    const caps = rotator.getRotatorCaps();
    assert(typeof caps.rotType === 'string', `expected rotType string, got ${typeof caps.rotType}`);
    assert(typeof caps.minAz === 'number', 'missing minAz');
    assert(Array.isArray(caps.supportedStatuses), 'supportedStatuses should be array');
  });

  await test('rotator getSupportedLevels includes SPEED', () => {
    const levels = rotator.getSupportedLevels();
    assert(Array.isArray(levels), `expected array, got ${typeof levels}`);
    assert(levels.includes('SPEED'), `expected SPEED in ${levels.join(', ')}`);
  });

  await test('rotator setPosition/getPosition round trip', async () => {
    await rotator.setPosition(120, 30);
    const position = await rotator.getPosition();
    assert(typeof position.azimuth === 'number', 'missing azimuth');
    assert(typeof position.elevation === 'number', 'missing elevation');
  });

  await test('rotator move/stop succeeds', async () => {
    await rotator.move('RIGHT', 2);
    await rotator.stop();
  });

  await test('rotator park succeeds', async () => {
    await rotator.park();
  });

  await test('rotator reset succeeds', async () => {
    await rotator.reset('ALL');
  });

  await test('rotator getInfo returns string', async () => {
    const info = await rotator.getInfo();
    assert(typeof info === 'string', `expected string, got ${typeof info}`);
  });

  await test('rotator getStatus returns object', async () => {
    const status = await rotator.getStatus();
    assert(typeof status === 'object', `expected object, got ${typeof status}`);
    assert(Array.isArray(status.flags), 'flags should be array');
  });

  await test('rotator setLevel/getLevel on SPEED', async () => {
    await rotator.setLevel('SPEED', 3);
    const speed = await rotator.getLevel('SPEED');
    assert(typeof speed === 'number', `expected number, got ${typeof speed}`);
  });

  await test('rotator getSupportedFunctions returns array', () => {
    const funcs = rotator.getSupportedFunctions();
    assert(Array.isArray(funcs), `expected array, got ${typeof funcs}`);
  });

  await test('rotator getSupportedParms returns array', () => {
    const parms = rotator.getSupportedParms();
    assert(Array.isArray(parms), `expected array, got ${typeof parms}`);
  });

  // --- Cleanup ---
  console.log('\n[Cleanup]');

  await test('close() succeeds', async () => {
    await rig.close();
  });

  await test('destroy() succeeds', async () => {
    await rig.destroy();
  });

  await test('sync capability query after destroy throws instead of crashing', () => {
    assertThrows(() => rig.getSupportedModes(), /destroyed|initialized/);
  });

  await test('rotator close() succeeds', async () => {
    await rotator.close();
  });

  await test('rotator destroy() succeeds', async () => {
    await rotator.destroy();
  });

  await test('rotator sync capability query after destroy throws instead of crashing', () => {
    assertThrows(() => rotator.getRotatorCaps(), /destroyed|initialized/);
  });

  // --- Summary ---
  const total = passed + failed;
  console.log(`\n=== Results: ${passed}/${total} passed, ${failed} failed ===`);

  if (failed > 0) {
    process.exit(1);
  }
}

run().catch(err => {
  console.error('Fatal error:', err);
  process.exit(1);
});
