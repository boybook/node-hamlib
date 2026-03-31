#!/usr/bin/env node

const { HamLib } = require('../index.js');
const { SpectrumController } = require('../lib/spectrum.js');

function parseArgs(argv) {
  const options = {
    model: null,
    port: null,
    rate: null,
    durationMs: 15000,
    settleMs: 5000,
    firstLineTimeoutMs: 30000,
    pumpIntervalMs: 200,
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
      case 'first-line-timeout-ms':
        options.firstLineTimeoutMs = Number(value);
        break;
      case 'pump-interval-ms':
        options.pumpIntervalMs = Number(value);
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
  --first-line-timeout-ms=<ms>  Maximum wait for the first spectrum line, default 30000
  --pump-interval-ms=<ms>       Managed helper pump interval, 0 disables, default 200
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

function waitForResult(promise, timeoutMs, fallbackValue) {
  return Promise.race([
    promise,
    new Promise((resolve) => setTimeout(() => resolve(fallbackValue), timeoutMs)),
  ]);
}

async function main() {
  const options = parseArgs(process.argv.slice(2));
  const rig = new HamLib(options.model, options.port);
  const spectrum = new SpectrumController(rig);
  const startedAt = Date.now();
  const metrics = {
    lines: 0,
    bytes: 0,
    openAtMs: null,
    managedStartAtMs: null,
    firstLineAtMs: null,
    lastLine: null,
  };
  let resolveFirstLine;
  const firstLinePromise = new Promise((resolve) => {
    resolveFirstLine = resolve;
  });

  const config = {};
  if (Number.isFinite(options.mode)) config.mode = options.mode;
  if (Number.isFinite(options.spanHz)) config.spanHz = options.spanHz;
  if (Number.isFinite(options.speed)) config.speed = options.speed;
  if (Number.isFinite(options.referenceLevel)) config.referenceLevel = options.referenceLevel;
  if (Number.isFinite(options.averageMode)) config.averageMode = options.averageMode;
  if (Number.isFinite(options.pumpIntervalMs)) config.pumpIntervalMs = options.pumpIntervalMs;

  const onSpectrumLine = (line) => {
    metrics.lines += 1;
    metrics.bytes += Number(line.dataLength || 0);
    metrics.lastLine = summarizeLine(line);
    if (metrics.firstLineAtMs === null) {
      metrics.firstLineAtMs = Date.now() - startedAt;
      resolveFirstLine(true);
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

  spectrum.on('spectrumLine', onSpectrumLine);
  spectrum.on('spectrumStateChanged', (state) => log('spectrumStateChanged', state));
  spectrum.on('spectrumError', (error) => log('spectrumError', error));

  let exitCode = 0;

  try {
    log('opening rig', {
      model: options.model,
      port: options.port,
      rate: options.rate ?? '(default)',
      pumpIntervalMs: options.pumpIntervalMs,
    });
    if (options.rate) {
      await rig.setSerialConfig('rate', options.rate);
      log('serial rate configured', { rate: options.rate });
    }

    await rig.open();
    metrics.openAtMs = Date.now() - startedAt;
    log('rig opened', rig.getConnectionInfo());

    const summary = await spectrum.getSpectrumSupportSummary();
    log('spectrum support summary', summary);

    if (!summary.supported) {
      throw new Error('Spectrum is not supported by current rig/backend summary');
    }

    log('starting managed spectrum', Object.keys(config).length > 0 ? config : { config: '(default)' });
    await spectrum.startManagedSpectrum(config);
    metrics.managedStartAtMs = Date.now() - startedAt;

    let firstLineReceived = await waitForResult(firstLinePromise, options.firstLineTimeoutMs, false);

    if (!firstLineReceived && metrics.lines === 0) {
      const graceMs = 2000;
      log('first-line timeout elapsed, waiting grace window', { graceMs });
      firstLineReceived = await waitForResult(firstLinePromise, graceMs, false);
    }

    if (!firstLineReceived) {
      exitCode = 2;
      log('no spectrumLine received before first-line timeout', {
        firstLineTimeoutMs: options.firstLineTimeoutMs,
        durationMs: options.durationMs,
        settleMs: options.settleMs,
      });
    } else {
      const observationEndsAt = startedAt + options.durationMs;
      const remainingObservationMs = observationEndsAt - Date.now();
      if (remainingObservationMs > 0) {
        await new Promise((resolve) => setTimeout(resolve, remainingObservationMs));
      }
      log('spectrum test completed with data', {
        durationMs: options.durationMs,
        lines: metrics.lines,
        bytes: metrics.bytes,
        openAtMs: metrics.openAtMs,
        managedStartAtMs: metrics.managedStartAtMs,
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
      await spectrum.stopManagedSpectrum();
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
