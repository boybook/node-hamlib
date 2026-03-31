#!/usr/bin/env node

const path = require('path');
const { spawnSync } = require('child_process');
const nodeGypBuild = require('node-gyp-build');
const { HamLib } = require('../index.js');

const nativeModule = nodeGypBuild(path.join(__dirname, '..'));

function parseArgs(argv) {
  const options = {
    model: null,
    port: null,
    rate: null,
    rigctl: 'rigctl',
    debugLevel: null,
    timeoutMs: 15000,
  };

  for (const arg of argv) {
    if (!arg.startsWith('--')) continue;

    const [key, rawValue = ''] = arg.slice(2).split('=');
    const value = rawValue.trim();

    switch (key) {
      case 'model':
        options.model = Number(value);
        break;
      case 'port':
        options.port = value;
        break;
      case 'rate':
        options.rate = value;
        break;
      case 'rigctl':
        options.rigctl = value || options.rigctl;
        break;
      case 'debug':
        options.debugLevel = Number(value);
        break;
      case 'timeout-ms':
        options.timeoutMs = Number(value);
        break;
      case 'help':
        printHelp();
        process.exit(0);
        break;
      default:
        throw new Error(`Unknown argument: --${key}`);
    }
  }

  if (!Number.isFinite(options.model) || !options.port) {
    printHelp();
    throw new Error('Both --model and --port are required');
  }

  return options;
}

function printHelp() {
  console.log(`Usage:
  node test/test_curr_vfo_probe.js --model=<rigModel> --port=<serialPath> [options]

Options:
  --rate=<baudRate>             Optional serial rate
  --rigctl=<path>               rigctl executable path, default: rigctl
  --debug=<0-5>                 Optional Hamlib debug level for addon tests
  --timeout-ms=<ms>             Timeout for each rigctl call, default: 15000
`);
}

function log(title, payload) {
  const prefix = `[${new Date().toISOString()}]`;
  if (payload === undefined) {
    console.log(`${prefix} ${title}`);
    return;
  }
  console.log(`${prefix} ${title}`, payload);
}

function summarizeError(error) {
  if (!error) return null;
  return {
    name: error.name,
    message: error.message,
    stack: error.stack,
  };
}

function rigctlCommandArgs(options, extraArgs) {
  const args = ['-m', String(options.model), '-r', options.port];
  if (options.rate) {
    args.push('-s', String(options.rate));
  }
  args.push(...extraArgs);
  return args;
}

function runRigctl(options, extraArgs, input) {
  const args = rigctlCommandArgs(options, extraArgs);
  const result = spawnSync(options.rigctl, args, {
    encoding: 'utf8',
    input,
    timeout: options.timeoutMs,
  });

  return {
    executable: options.rigctl,
    args,
    exitCode: result.status,
    signal: result.signal,
    stdout: result.stdout,
    stderr: result.stderr,
    error: summarizeError(result.error),
  };
}

async function withRig(label, rig, options, tests) {
  const result = {
    label,
    version: null,
    connectionInfo: null,
    open: null,
    tests: [],
    close: null,
  };

  if (typeof rig.constructor.getHamlibVersion === 'function') {
    result.version = rig.constructor.getHamlibVersion();
  }

  if (options.debugLevel !== null && typeof rig.constructor.setDebugLevel === 'function') {
    rig.constructor.setDebugLevel(options.debugLevel);
  }

  if (options.rate && typeof rig.setSerialConfig === 'function') {
    await rig.setSerialConfig('rate', String(options.rate));
  }

  try {
    result.connectionInfo = typeof rig.getConnectionInfo === 'function' ? rig.getConnectionInfo() : null;
  } catch (error) {
    result.connectionInfo = { error: summarizeError(error) };
  }

  try {
    result.open = await rig.open();
  } catch (error) {
    result.open = { error: summarizeError(error) };
    return result;
  }

  for (const test of tests) {
    try {
      const value = await test.run(rig);
      result.tests.push({
        name: test.name,
        ok: true,
        value,
      });
    } catch (error) {
      result.tests.push({
        name: test.name,
        ok: false,
        error: summarizeError(error),
      });
    }
  }

  try {
    result.close = await rig.close();
  } catch (error) {
    result.close = { error: summarizeError(error) };
  }

  return result;
}

function createNativeRig(options) {
  return new nativeModule.HamLib(options.model, options.port);
}

function createWrapperRig(options) {
  return new HamLib(options.model, options.port);
}

function printSection(title, value) {
  console.log(`\n=== ${title} ===`);
  if (typeof value === 'string') {
    console.log(value);
    return;
  }
  console.log(JSON.stringify(value, null, 2));
}

async function main() {
  const options = parseArgs(process.argv.slice(2));

  const addonTests = [
    { name: 'getVfo()', run: (rig) => rig.getVfo() },
    { name: 'getFrequency()', run: (rig) => rig.getFrequency() },
    { name: 'getFrequency("VFOA")', run: (rig) => rig.getFrequency('VFOA') },
    { name: 'getFrequency("VFOB")', run: (rig) => rig.getFrequency('VFOB') },
    { name: 'getFrequency("currVFO")', run: (rig) => rig.getFrequency('currVFO') },
    { name: 'getFrequency("NOT-A-VFO")', run: (rig) => rig.getFrequency('NOT-A-VFO') },
    { name: 'getMode()', run: (rig) => rig.getMode() },
  ];

  const summary = {
    requested: options,
    versions: {
      wrapper: HamLib.getHamlibVersion(),
      native: nativeModule.HamLib.getHamlibVersion(),
    },
    rigctl: {
      version: runRigctl(options, ['-V']),
      defaultRead: runRigctl(options, ['-'], 'v\nf\nm\n'),
      explicitCurrVfo: runRigctl(options, ['-o', '-'], 'f currVFO\nm currVFO\nv\n'),
      explicitConcreteVfo: runRigctl(options, ['-o', '-'], 'f VFOA\nf VFOB\nv\n'),
    },
    nativeAddon: await withRig('native-addon', createNativeRig(options), options, addonTests),
    jsWrapper: await withRig('js-wrapper', createWrapperRig(options), options, addonTests),
  };

  printSection('Summary', summary);
}

main().catch((error) => {
  console.error(JSON.stringify({
    fatal: summarizeError(error),
  }, null, 2));
  process.exit(1);
});
