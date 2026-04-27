#!/usr/bin/env node

const { HamLib } = require('../index.js');

function assert(condition, message) {
  if (!condition) throw new Error(message || 'Assertion failed');
}

async function assertRejects(fn, message) {
  let rejected = false;
  try {
    await fn();
  } catch (error) {
    rejected = true;
  }
  assert(rejected, message);
}

async function main() {
  const rig = new HamLib(1);
  await rig.open();

  try {
    assert(rig.memory && typeof rig.memory.layout === 'function', 'memory facade missing');

    const layout = await rig.memory.layout();
    assert(layout.count > 0, 'layout count should be positive');
    assert(Array.isArray(layout.ranges) && layout.ranges.length > 0, 'layout ranges missing');
    assert(layout.ranges[0].capabilities && typeof layout.ranges[0].capabilities.frequency === 'boolean', 'range capabilities missing');

    const count = await rig.memory.count();
    const legacyCount = await rig.memCount();
    assert(count === legacyCount, `memory.count mismatch: ${count} !== ${legacyCount}`);

    const caps = await rig.memory.capabilities(layout.ranges[0].start);
    assert(caps && typeof caps.mode === 'boolean', 'capabilities object missing mode flag');

    const channel = await rig.memory.get(layout.ranges[0].start, { readOnly: true });
    assert(channel.channelNumber === layout.ranges[0].start, 'channel number mismatch');
    assert(typeof channel.empty === 'boolean', 'channel empty flag missing');
    assert(channel.flags && typeof channel.flags.skip === 'boolean', 'channel flags missing');
    assert(channel.levels && typeof channel.levels === 'object', 'channel levels missing');

    const legacyChannel = await rig.getMemoryChannel(layout.ranges[0].start, true);
    assert(legacyChannel.channelNumber === channel.channelNumber, 'legacy getMemoryChannel mismatch');

    const list = await rig.memory.list({ includeEmpty: true, continueOnError: true });
    assert(list.layout.count === layout.count, 'list layout count mismatch');
    assert(Array.isArray(list.channels) && list.channels.length === layout.count, 'list should include all dummy memory slots with includeEmpty');
    assert(Array.isArray(list.errors), 'list errors missing');

    const outsideChannel = Math.max(...layout.ranges.map((range) => range.end)) + 1;
    await assertRejects(() => rig.memory.get(outsideChannel, { readOnly: true }), 'memory.get should reject channels outside layout');
    await assertRejects(() => rig.memory.setMany([{}], { continueOnError: true }), 'setMany should require channelNumber');
    await assertRejects(() => rig.memory.replaceAll([{ channelNumber: layout.ranges[0].start }]), 'replaceAll should require complete layout');

    const writeResult = await rig.memory.setMany([
      {
        channelNumber: layout.ranges[0].start,
        frequency: 144390000,
        mode: 'FM',
        bandwidth: 0,
        txFrequency: 144990000,
        txMode: 'FM',
        txBandwidth: 0,
        split: true,
        repeaterShift: 'PLUS',
        repeaterOffset: 600000,
        ctcssTone: 885,
        ctcssSql: 885,
        dcsCode: 23,
        dcsSql: 23,
        flags: { skip: true, data: false, pskip: false },
        description: 'Memory API test',
        tag: 'MEMAPI',
        functions: ['TONE'],
        levels: { AF: 0.5 },
      },
    ], { continueOnError: true });
    assert(typeof writeResult.written === 'number', 'write result missing written');
    assert(Array.isArray(writeResult.errors), 'write result missing errors');

    console.log('Memory API tests passed');
  } finally {
    await rig.close();
  }
}

main().catch((error) => {
  console.error(error);
  process.exit(1);
});
