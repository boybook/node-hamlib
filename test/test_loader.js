/**
 * 基础模块加载和功能测试
 * Tests module loading, instantiation, method existence, and static methods
 */

const { spawnSync } = require('child_process');
const { HamLib, Rotator, PASSBAND } = require('../index.js');
const { SpectrumController } = require('../spectrum.js');

console.log('🧪 测试node-hamlib模块加载和基础功能...\n');

// 输出Hamlib版本信息
try {
  const hamlibVersion = HamLib.getHamlibVersion();
  console.log('📌 Hamlib版本信息:');
  console.log(`   ${hamlibVersion}\n`);
} catch (e) {
  console.log('⚠️  无法获取Hamlib版本信息:', e.message, '\n');
}

let testsPassed = 0;
let testsFailed = 0;

function test(description, testFn) {
  try {
    const result = testFn();
    if (result !== false) {
      console.log(`✅ ${description}`);
      testsPassed++;
    } else {
      console.log(`❌ ${description}`);
      testsFailed++;
    }
  } catch (error) {
    console.log(`❌ ${description} - ${error.message}`);
    testsFailed++;
  }
}

function readGlobalLockEnabledFromChild(value) {
  const env = { ...process.env };
  if (value === undefined) {
    delete env.NODE_HAMLIB_GLOBAL_LOCK;
  } else {
    env.NODE_HAMLIB_GLOBAL_LOCK = value;
  }

  const result = spawnSync(
    process.execPath,
    ['-e', "const { HamLib } = require('./index.js'); process.stdout.write(String(HamLib.isGlobalLockEnabled()));"],
    { cwd: require('path').join(__dirname, '..'), env, encoding: 'utf8' }
  );

  if (result.status !== 0) {
    throw new Error(result.stderr || `child exited with ${result.status}`);
  }

  return result.stdout.trim();
}

function runLoaderChild(script, extraArgs = []) {
  const result = spawnSync(
    process.execPath,
    [...extraArgs, '-e', script],
    { cwd: require('path').join(__dirname, '..'), env: process.env, encoding: 'utf8' }
  );

  if (result.status !== 0) {
    throw new Error(result.stderr || result.stdout || `child exited with ${result.status}`);
  }

  return true;
}

try {
  // 1. 模块加载测试
  console.log('📦 模块加载测试:');
  test('HamLib类成功加载', () => HamLib && typeof HamLib === 'function');
  test('HamLib构造函数可用', () => typeof HamLib === 'function');
  test('Rotator类成功加载', () => Rotator && typeof Rotator === 'function');
  test('PASSBAND常量成功加载', () => PASSBAND && typeof PASSBAND === 'object');
  
  // 2. 静态方法测试
  console.log('\n📊 静态方法测试:');
  test('getHamlibVersion静态方法存在', () => typeof HamLib.getHamlibVersion === 'function');
  test('getSupportedRigs静态方法存在', () => typeof HamLib.getSupportedRigs === 'function');
  test('Rotator.getSupportedRotators静态方法存在', () => typeof Rotator.getSupportedRotators === 'function');
  test('PASSBAND.NORMAL 常量正确', () => PASSBAND.NORMAL === 0);
  test('PASSBAND.NOCHANGE 常量正确', () => PASSBAND.NOCHANGE === -1);

  try {
    const supportedRigs = HamLib.getSupportedRigs();
    test('getSupportedRigs返回数组', () => Array.isArray(supportedRigs));
    test('getSupportedRigs返回非空数据', () => supportedRigs.length > 0);
    test('支持的电台数据结构正确', () => {
      if (supportedRigs.length === 0) return false;
      const first = supportedRigs[0];
      return first.rigModel && first.modelName && first.mfgName;
    });
    console.log(`   📈 找到 ${supportedRigs.length} 个支持的电台型号`);

    // 打印所有支持的设备型号
    console.log('\n📻 所有支持的设备型号:');
    console.log('   ────────────────────────────────────────────────────────────────');
    console.log('   型号ID  | 制造商              | 型号名称                    | 状态');
    console.log('   ────────────────────────────────────────────────────────────────');
    supportedRigs.forEach((rig, index) => {
      const model = String(rig.rigModel).padEnd(7);
      const mfg = (rig.mfgName || '').substring(0, 20).padEnd(20);
      const name = (rig.modelName || '').substring(0, 28).padEnd(28);
      const status = rig.status || '';
      console.log(`   ${model} | ${mfg} | ${name} | ${status}`);

      // 每50行输出一个分隔符，便于阅读
      if ((index + 1) % 50 === 0 && index + 1 < supportedRigs.length) {
        console.log('   ────────────────────────────────────────────────────────────────');
      }
    });
    console.log('   ────────────────────────────────────────────────────────────────');
    console.log(`   共计: ${supportedRigs.length} 个型号\n`);

  } catch (e) {
    console.log(`❌ getSupportedRigs调用失败: ${e.message}`);
    testsFailed++;
  }

  try {
    const supportedRotators = Rotator.getSupportedRotators();
    test('getSupportedRotators返回数组', () => Array.isArray(supportedRotators));
    test('getSupportedRotators返回非空数据', () => supportedRotators.length > 0);
    test('支持的旋转器数据结构正确', () => {
      if (supportedRotators.length === 0) return false;
      const first = supportedRotators[0];
      return first.rotModel && first.modelName && first.mfgName;
    });
    console.log(`   🛰️ 找到 ${supportedRotators.length} 个支持的旋转器型号`);
  } catch (e) {
    console.log(`❌ getSupportedRotators调用失败: ${e.message}`);
    testsFailed++;
  }
  
  // 3. 实例化测试
  console.log('\n🔧 实例化测试:');
  const testRig = new HamLib(1035, '/dev/null');
  const testRotator = new Rotator(1);
  test('HamLib实例创建成功', () => testRig && typeof testRig === 'object');
  test('Rotator实例创建成功', () => testRotator && typeof testRotator === 'object');
  const spectrumController = new SpectrumController(testRig);
  test('SpectrumController实例创建成功', () => spectrumController && typeof spectrumController === 'object');
  test('spectrum 子路径 CJS 代理可用', () => typeof SpectrumController === 'function');
  
  // 4. 基础方法存在性测试
  console.log('\n🔍 基础方法存在性测试:');
  const coreMethods = [
    'open', 'close', 'destroy', 'getConnectionInfo',
    'setVfo', 'getVfo', 'setFrequency', 'getFrequency', 
    'setMode', 'getMode', 'setPtt', 'getStrength'
  ];
  
  coreMethods.forEach(method => {
    test(`方法 ${method} 存在`, () => typeof testRig[method] === 'function');
  });
  
  // 5. 新增方法存在性测试 (之前缺失的方法)
  console.log('\n🆕 新增方法存在性测试:');
  const newMethods = [
    'getPtt', 'getDcd', 'setPowerstat', 'getPowerstat',
    'setTuningStep', 'getTuningStep', 
    'setRepeaterShift', 'getRepeaterShift',
    'setRepeaterOffset', 'getRepeaterOffset'
  ];
  
  newMethods.forEach(method => {
    test(`新增方法 ${method} 存在`, () => typeof testRig[method] === 'function');
  });
  
  // Hamlib 4.7.0 新增方法存在性测试
  console.log('\n🆕 Hamlib 4.7.0 新增方法存在性测试:');
  const hamlib47Methods = [
    'setLockMode', 'getLockMode',
    'setClock', 'getClock',
    'getVfoInfo'
  ];

  hamlib47Methods.forEach(method => {
    test(`Hamlib 4.7.0 方法 ${method} 存在`, () => typeof testRig[method] === 'function');
  });

  // 新增 API 方法存在性测试
  console.log('\n🆕 补齐 API 方法存在性测试:');
  const newApiMethods = [
    'getInfo', 'sendRaw', 'getSpectrumCapabilities',
    'startSpectrumStream', 'stopSpectrumStream', 'setConf', 'getConf', 'getConfigSchema', 'getPortCaps',
    'getPassbandNormal', 'getPassbandNarrow', 'getPassbandWide',
    'getResolution',
    'getSupportedParms', 'getSupportedVfoOps', 'getSupportedScanTypes'
  ];

  newApiMethods.forEach(method => {
    test(`新增方法 ${method} 存在`, () => typeof testRig[method] === 'function');
  });

  console.log('\n🆕 SpectrumController 方法存在性测试:');
  const spectrumMethods = [
    'getSpectrumSupportSummary', 'configureSpectrum',
    'getSpectrumDisplayState', 'configureSpectrumDisplay',
    'getSpectrumEdgeSlot', 'setSpectrumEdgeSlot',
    'getSpectrumSupportedEdgeSlots',
    'getSpectrumFixedEdges', 'setSpectrumFixedEdges',
    'startManagedSpectrum', 'stopManagedSpectrum'
  ];

  spectrumMethods.forEach(method => {
    test(`SpectrumController 方法 ${method} 存在`, () => typeof spectrumController[method] === 'function');
  });

  console.log('\n🧹 主入口纯桥接边界测试:');
  const removedSpectrumMethods = [
    'getSpectrumSupportSummary', 'configureSpectrum',
    'getSpectrumDisplayState', 'configureSpectrumDisplay',
    'getSpectrumEdgeSlot', 'setSpectrumEdgeSlot',
    'getSpectrumSupportedEdgeSlots',
    'getSpectrumFixedEdges', 'setSpectrumFixedEdges',
    'startManagedSpectrum', 'stopManagedSpectrum'
  ];

  removedSpectrumMethods.forEach(method => {
    test(`HamLib 主入口不再暴露 ${method}`, () => typeof testRig[method] === 'undefined');
  });

  // 新增静态方法测试
  console.log('\n🆕 新增静态方法测试:');
  const newStaticMethods = ['getCopyright', 'getLicense', 'getConfigSchemaForModel', 'getPortCapsForModel'];
  newStaticMethods.forEach(method => {
    test(`静态方法 ${method} 存在`, () => typeof HamLib[method] === 'function');
  });

  console.log('\n🔒 HamLib 全局串行锁开关测试:');
  test('setGlobalLockEnabled 静态方法存在', () => typeof HamLib.setGlobalLockEnabled === 'function');
  test('isGlobalLockEnabled 静态方法存在', () => typeof HamLib.isGlobalLockEnabled === 'function');
  test('全局串行锁默认开启', () => HamLib.isGlobalLockEnabled() === true);
  test('全局串行锁可运行时关闭和重新开启', () => {
    HamLib.setGlobalLockEnabled(false);
    const disabled = HamLib.isGlobalLockEnabled() === false;
    HamLib.setGlobalLockEnabled(true);
    return disabled && HamLib.isGlobalLockEnabled() === true;
  });
  test('无环境变量时子进程默认开启全局串行锁', () => readGlobalLockEnabledFromChild(undefined) === 'true');
  ['0', 'false', 'off', 'no'].forEach(value => {
    test(`NODE_HAMLIB_GLOBAL_LOCK=${value} 时默认关闭`, () => readGlobalLockEnabledFromChild(value) === 'false');
  });
  test('静态型号配置 schema 同步返回数组', () => {
    const schema = HamLib.getConfigSchemaForModel(1);
    return Array.isArray(schema) && schema.length > 0 && typeof schema[0].name === 'string';
  });
  test('静态型号端口能力同步返回对象', () => {
    const caps = HamLib.getPortCapsForModel(1);
    return caps && typeof caps === 'object' && typeof caps.portType === 'string';
  });

  // Capability Query Batch 2 方法存在性测试
  console.log('\n🆕 能力查询方法存在性测试 (第二批):');
  const capQueryMethods = [
    'getPreampValues', 'getAttenuatorValues',
    'getAgcLevels',
    'getMaxRit', 'getMaxXit', 'getMaxIfShift',
    'getAvailableCtcssTones', 'getAvailableDcsCodes',
    'getFrequencyRanges', 'getTuningSteps', 'getFilterList',
    'getLevelGranularity', 'getRfPowerStepTable'
  ];

  capQueryMethods.forEach(method => {
    test(`能力查询方法 ${method} 存在`, () => typeof testRig[method] === 'function');
  });
  [
    'getSpectrumCapabilities',
    'stopSpectrumStream',
    'getConfigSchema',
    'getPortCaps',
    'getPassbandNormal',
    'getSupportedLevels',
    'getSupportedFunctions',
    'getSupportedModes',
    'getSupportedParms',
    'getFrequencyRanges',
    'getTuningSteps',
    'getFilterList',
  ].forEach(method => {
    test(`${method} 返回 Promise API`, () => {
      const result = method.startsWith('getPassband') ? testRig[method]('USB') : testRig[method]();
      result.catch(() => {});
      return result && typeof result.then === 'function';
    });
  });
  test('销毁后的异步能力查询以 Promise reject 返回统一 operation 字段', () => runLoaderChild(`
    (async () => {
      const { HamLib } = require('./index.js');
      const rig = new HamLib(1);
      await rig.destroy();
      const result = rig.getSpectrumCapabilities();
      if (!result || typeof result.then !== 'function') throw new Error('expected Promise');
      try {
        await result;
        throw new Error('expected rejection');
      } catch (error) {
        if (error && error.message === 'expected rejection') throw error;
        if (!error || error.operation !== 'GetSpectrumCapabilities') {
          throw new Error('missing operation on rejection: ' + JSON.stringify({
            message: error && error.message,
            operation: error && error.operation,
            code: error && error.code,
          }));
        }
      }
    })().catch((error) => {
      console.error(error && error.stack || error);
      process.exit(1);
    });
  `));
  test('未打开的核心 CAT 调用以 Promise reject 返回统一错误字段', () => runLoaderChild(`
    (async () => {
      const { HamLib } = require('./index.js');
      const rig = new HamLib(1);
      const result = rig.getFrequency();
      if (!result || typeof result.then !== 'function') throw new Error('expected Promise');
      try {
        await result;
        throw new Error('expected rejection');
      } catch (error) {
        if (error && error.message === 'expected rejection') throw error;
        if (!error || error.operation !== 'GetFrequency' || error.code !== 'HAMLIB_ERROR') {
          throw new Error('unexpected rejection metadata: ' + JSON.stringify({
            message: error && error.message,
            operation: error && error.operation,
            code: error && error.code,
            hamlibCode: error && error.hamlibCode,
          }));
        }
      } finally {
        await rig.destroy().catch(() => {});
      }
    })().catch((error) => {
      console.error(error && error.stack || error);
      process.exit(1);
    });
  `));
  test('构造失败或异常构造后 GC 不访问未初始化 rig 指针', () => runLoaderChild(`
    const { HamLib } = require('./index.js');
    try {
      new HamLib(999999999);
    } catch {}
    if (global.gc) {
      for (let i = 0; i < 5; i += 1) global.gc();
    }
  `, ['--expose-gc']));

  console.log('\n🛰️ Rotator 方法存在性测试:');
  const rotatorMethods = [
    'open', 'close', 'destroy', 'getConnectionInfo',
    'setPosition', 'getPosition', 'move', 'stop', 'park', 'reset',
    'getInfo', 'getStatus',
    'setConf', 'getConf', 'getConfigSchema', 'getPortCaps', 'getRotatorCaps',
    'setLevel', 'getLevel', 'getSupportedLevels',
    'setFunction', 'getFunction', 'getSupportedFunctions',
    'setParm', 'getParm', 'getSupportedParms'
  ];

  rotatorMethods.forEach(method => {
    test(`Rotator 方法 ${method} 存在`, () => typeof testRotator[method] === 'function');
  });

  const rotatorStaticMethods = [
    'getSupportedRotators', 'getHamlibVersion', 'setDebugLevel',
    'getDebugLevel', 'getCopyright', 'getLicense'
  ];

  rotatorStaticMethods.forEach(method => {
    test(`Rotator 静态方法 ${method} 存在`, () => typeof Rotator[method] === 'function');
  });

  // 6. 方法计数测试
  console.log('\n📊 API完整性测试:');
  const instanceMethods = Object.getOwnPropertyNames(Object.getPrototypeOf(testRig))
    .filter(name => name !== 'constructor' && typeof testRig[name] === 'function');

  const staticMethods = Object.getOwnPropertyNames(HamLib)
    .filter(name => typeof HamLib[name] === 'function');

  const totalMethods = instanceMethods.length + staticMethods.length;

  test('实例方法数量不少于97个', () => instanceMethods.length >= 97);
  test('静态方法数量不少于10个', () => staticMethods.length >= 10);
  test('总方法数量不少于105个', () => totalMethods >= 105);

  console.log(`   📊 实例方法: ${instanceMethods.length}个`);
  console.log(`   📊 静态方法: ${staticMethods.length}个`);
  console.log(`   📊 总计: ${totalMethods}个方法`);
  
  // 7. 连接信息测试
  console.log('\n🔗 连接信息测试:');
  try {
    const connInfo = testRig.getConnectionInfo();
    test('getConnectionInfo返回对象', () => connInfo && typeof connInfo === 'object');
    test('连接信息包含必要字段', () => {
      return connInfo.hasOwnProperty('connectionType') && 
             connInfo.hasOwnProperty('portPath') && 
             connInfo.hasOwnProperty('isOpen');
    });
  } catch (e) {
    console.log(`❌ getConnectionInfo调用失败: ${e.message}`);
    testsFailed++;
  }
  
  // 输出测试结果
  console.log(`\n📋 测试完成: ${testsPassed}个通过, ${testsFailed}个失败`);
  
  if (testsFailed === 0) {
    console.log('🎉 所有基础功能测试通过！');
    console.log('💡 注意: 此测试仅验证模块加载和方法存在，不进行实际硬件通信。');
  } else {
    console.log('⚠️  存在测试失败项，请检查模块完整性。');
    process.exit(1);
  }
  
} catch (error) {
  console.error('❌ 测试运行失败:', error.message);
  console.error(error.stack);
  process.exit(1);
} 
