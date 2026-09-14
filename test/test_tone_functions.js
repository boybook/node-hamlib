#!/usr/bin/env node

const assert = require('assert').strict;
const { HamLib } = require('../index.js');

async function main() {
  const rig = new HamLib(1);
  try {
    await rig.open();
    const functions = await rig.getSupportedFunctions();
    for (const name of ['TONE', 'CSQL']) {
      assert(functions.includes(name), `Dummy must advertise ${name}`);
      const setValue = name === 'TONE'
        ? (value) => rig.setCtcssTone(value)
        : (value) => rig.setDcsCode(value);
      const getValue = name === 'TONE'
        ? () => rig.getCtcssTone()
        : () => rig.getDcsCode();
      const value = name === 'TONE' ? 885 : 23;
      await setValue(value);
      for (const enabled of [true, false, true]) {
        await rig.setFunction(name, enabled);
        assert.equal(await rig.getFunction(name), enabled, `${name} switch round trip`);
        assert.equal(await getValue(), value, `${name} switch must preserve the configured value`);
      }
    }
    await assert.rejects(() => rig.getFunction('INVALID_FUNCTION'), /Invalid function type/);
    await assert.rejects(() => rig.setFunction('INVALID_FUNCTION', true), /Invalid function type/);
    console.log('Tone function tests passed');
  } finally {
    await rig.close();
    rig.destroy();
  }
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
