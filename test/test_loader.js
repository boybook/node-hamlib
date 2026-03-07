/**
 * 基础模块加载和功能测试
 * Tests module loading, instantiation, method existence, and static methods
 */

const { HamLib } = require('../index.js');

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

try {
  // 1. 模块加载测试
  console.log('📦 模块加载测试:');
  test('HamLib类成功加载', () => HamLib && typeof HamLib === 'function');
  test('HamLib构造函数可用', () => typeof HamLib === 'function');
  
  // 2. 静态方法测试
  console.log('\n📊 静态方法测试:');
  test('getHamlibVersion静态方法存在', () => typeof HamLib.getHamlibVersion === 'function');
  test('getSupportedRigs静态方法存在', () => typeof HamLib.getSupportedRigs === 'function');

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
  
  // 3. 实例化测试
  console.log('\n🔧 实例化测试:');
  const testRig = new HamLib(1035, '/dev/null');
  test('HamLib实例创建成功', () => testRig && typeof testRig === 'object');
  
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

  // 6. 方法计数测试
  console.log('\n📊 API完整性测试:');
  const instanceMethods = Object.getOwnPropertyNames(Object.getPrototypeOf(testRig))
    .filter(name => name !== 'constructor' && typeof testRig[name] === 'function');

  const staticMethods = Object.getOwnPropertyNames(HamLib)
    .filter(name => typeof HamLib[name] === 'function');

  const totalMethods = instanceMethods.length + staticMethods.length;

  test(`实例方法数量正确 (85个)`, () => instanceMethods.length === 85);
  test(`静态方法数量正确 (4个)`, () => staticMethods.length === 4);
  test(`总方法数量正确 (89个)`, () => totalMethods === 89);

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