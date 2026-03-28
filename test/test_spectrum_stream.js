#!/usr/bin/env node

const { HamLib } = require('../index.js');

function parseArgs(argv) {
  const options = {
    model: null,
    port: null,
    rate: null,
    durationMs: 15000,
    settleMs: 5000,
    mode: null,
    spanHz: null,
    speed: null,
    referenceLevel: null,
    averageMode: null,
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
      case 'duration-ms':
        options.durationMs = Number(value);
        break;
      case 'settle-ms':
        options.settleMs = Number(value);
        break;
      case 'mode':
        options.mode = Number(value);
        break;
      case 'span-hz':
        options.spanHz = Number(value);
        break;
      case 'speed':
        options.speed = Number(value);
        break;
      case 'reference-level':
        options.referenceLevel = Number(value);
        break;
      case 'average-mode':
        options.averageMode = Number(value);
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
  node test/test_spectrum_stream.js --model=<rigModel> --port=<serialPath> [options]

Options:
  --rate=<baudRate>              Optional serial rate, e.g. 115200
  --duration-ms=<ms>            Test duration after stream starts, default 15000
  --settle-ms=<ms>              Extra wait before declaring timeout, default 5000
  --mode=<id>                   Optional SPECTRUM_MODE level value
  --span-hz=<hz>                Optional SPECTRUM_SPAN level value
  --speed=<id>                  Optional SPECTRUM_SPEED level value
  --reference-level=<value>     Optional SPECTRUM_REF level value
  --average-mode=<id>           Optional SPECTRUM_AVG level value
`);
}

function log(message, data) {
  const prefix = `[${new Date().toISOString()}]`;
  if (data === undefined) {
    console.log(`${prefix} ${message}`);
    return;
  }
  console.log(`${prefix} ${message}`, data);
}

function summarizeLine(line) {
  return {
    scopeId: line.scopeId,
    mode: line.mode,
    centerFreq: line.centerFreq,
    spanHz: line.spanHz,
    lowEdgeFreq: line.lowEdgeFreq,
    highEdgeFreq: line.highEdgeFreq,
    dataLength: line.dataLength,
    dataLevelMin: line.dataLevelMin,
    dataLevelMax: line.dataLevelMax,
    signalStrengthMin: line.signalStrengthMin,
    signalStrengthMax: line.signalStrengthMax,
  };
}

async function main() {
  const options = parseArgs(process.argv.slice(2));
  const rig = new HamLib(options.model, options.port);
  const startedAt = Date.now();
  const metrics = {
    lines: 0,
    bytes: 0,
    firstLineAtMs: null,
    lastLine: null,
  };

  const config = {};
  if (Number.isFinite(options.mode)) config.mode = options.mode;
  if (Number.isFinite(options.spanHz)) config.spanHz = options.spanHz;
  if (Number.isFinite(options.speed)) config.speed = options.speed;
  if (Number.isFinite(options.referenceLevel)) config.referenceLevel = options.referenceLevel;
  if (Number.isFinite(options.averageMode)) config.averageMode = options.averageMode;

  const onSpectrumLine = (line) => {
    metrics.lines += 1;
    metrics.bytes += Number(line.dataLength || 0);
    metrics.lastLine = summarizeLine(line);
    if (metrics.firstLineAtMs === null) {
      metrics.firstLineAtMs = Date.now() - startedAt;
      log('first spectrumLine received', {
        firstLineAtMs: metrics.firstLineAtMs,
        summary: metrics.lastLine,
      });
    } else if (metrics.lines <= 5) {
      log('spectrumLine received', {
        count: metrics.lines,
        summary: metrics.lastLine,
      });
    }
  };

  rig.on('spectrumLine', onSpectrumLine);
  rig.on('spectrumStateChanged', (state) => log('spectrumStateChanged', state));
  rig.on('spectrumError', (error) => log('spectrumError', error));

  let exitCode = 0;

  try {
    log('opening rig', {
      model: options.model,
      port: options.port,
      rate: options.rate ?? '(default)',
    });
    if (options.rate) {
      await rig.setSerialConfig('rate', options.rate);
      log('serial rate configured', { rate: options.rate });
    }

    await rig.open();
    log('rig opened', rig.getConnectionInfo());

    const summary = await rig.getSpectrumSupportSummary();
    log('spectrum support summary', summary);

    if (!summary.supported) {
      throw new Error('Spectrum is not supported by current rig/backend summary');
    }

    log('starting managed spectrum', Object.keys(config).length > 0 ? config : { config: '(default)' });
    await rig.startManagedSpectrum(config);

    await new Promise((resolve) => setTimeout(resolve, options.durationMs));
    if (metrics.lines === 0 && options.settleMs > 0) {
      log('no spectrumLine yet, waiting settle window', { settleMs: options.settleMs });
      await new Promise((resolve) => setTimeout(resolve, options.settleMs));
    }

    if (metrics.lines === 0) {
      exitCode = 2;
      log('no spectrumLine received during test window', { durationMs: options.durationMs });
    } else {
      log('spectrum test completed with data', {
        durationMs: options.durationMs,
        lines: metrics.lines,
        bytes: metrics.bytes,
        firstLineAtMs: metrics.firstLineAtMs,
        lastLine: metrics.lastLine,
      });
    }
  } catch (error) {
    exitCode = 1;
    log('test failed', {
      message: error.message,
      stack: error.stack,
    });
  } finally {
    try {
      await rig.stopManagedSpectrum();
      log('managed spectrum stopped');
    } catch (error) {
      log('stopManagedSpectrum failed during cleanup', { message: error.message });
    }

    try {
      await rig.close();
      log('rig closed');
    } catch (error) {
      log('close failed during cleanup', { message: error.message });
    }
  }

  process.exit(exitCode);
}

main();
