#!/usr/bin/env node

/**
 * CI Functional Test - validates compiled binary works correctly
 * Uses Hamlib Dummy device (Model 1) directly, no rigctld needed.
 * Tests core API operations to ensure the native addon is functional.
 */

const { HamLib } = require('../index.js');

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

  await test('setVfo VFO-A', async () => {
    await rig.setVfo('VFO-A');
  });

  await test('getVfo returns VFO-A', async () => {
    const vfo = await rig.getVfo();
    assert(vfo === 'VFO-A', `expected VFO-A, got ${vfo}`);
  });

  await test('setVfo VFO-B', async () => {
    await rig.setVfo('VFO-B');
  });

  await test('getVfo returns VFO-B', async () => {
    const vfo = await rig.getVfo();
    assert(vfo === 'VFO-B', `expected VFO-B, got ${vfo}`);
  });

  // --- Frequency operations ---
  console.log('\n[Frequency Operations]');

  await test('setFrequency 144.39MHz on VFO-A', async () => {
    await rig.setFrequency(144390000, 'VFO-A');
  });

  await test('getFrequency returns 144.39MHz', async () => {
    const freq = await rig.getFrequency('VFO-A');
    assert(freq === 144390000, `expected 144390000, got ${freq}`);
  });

  await test('setFrequency 432.1MHz on VFO-B', async () => {
    await rig.setFrequency(432100000, 'VFO-B');
  });

  await test('getFrequency VFO-B returns 432.1MHz', async () => {
    const freq = await rig.getFrequency('VFO-B');
    assert(freq === 432100000, `expected 432100000, got ${freq}`);
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

  // --- Cleanup ---
  console.log('\n[Cleanup]');

  await test('close() succeeds', async () => {
    await rig.close();
  });

  await test('destroy() succeeds', async () => {
    await rig.destroy();
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
