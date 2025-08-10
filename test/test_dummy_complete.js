/**
 * 完整功能测试 - 基于Hamlib Dummy设备 (Model 1)
 * 全面验证所有API方法的正确调用和基本功能
 * 
 * Dummy设备是Hamlib的内置测试设备，不需要实际硬件连接
 * 可以模拟大多数无线电设备的功能，是进行API测试的理想选择
 */

const { HamLib } = require('../index.js');

console.log('🧪 启动基于Dummy设备的完整API功能测试...\n');

let testsPassed = 0;
let testsFailed = 0;
let testsSkipped = 0;

function logTest(description, status, details = '') {
  const icons = { pass: '✅', fail: '❌', skip: '⏭️' };
  const icon = icons[status] || '❓';
  console.log(`${icon} ${description}${details ? ' - ' + details : ''}`);
  
  switch(status) {
    case 'pass': testsPassed++; break;
    case 'fail': testsFailed++; break;
    case 'skip': testsSkipped++; break;
  }
}

async function testWithErrorHandling(description, testFn) {
  try {
    const result = await testFn();
    if (result !== false) {
      logTest(description, 'pass', typeof result === 'string' ? result : '');
      return true;
    } else {
      logTest(description, 'fail', 'Test returned false');
      return false;
    }
  } catch (error) {
    // 对于某些在dummy设备上不支持的功能，我们标记为跳过而不是失败
    if (error.message.includes('not implemented') || 
        error.message.includes('not supported') ||
        error.message.includes('not available') ||
        error.message.includes('RIG_ENAVAIL') ||
        error.message.includes('RIG_ENIMPL')) {
      logTest(description, 'skip', `Not implemented in dummy: ${error.message}`);
      return null;
    } else {
      logTest(description, 'fail', error.message);
      return false;
    }
  }
}

async function runCompleteTest() {
  // 使用Model 1 (Dummy设备) 进行测试
  const rig = new HamLib(1, 'localhost:4532'); // 假设运行rigctld -m 1 -t 4532
  
  try {
    // ========== 第一部分：基础连接和信息获取 ==========
    console.log('🔌 第一部分：基础连接和设备信息');
    
    await testWithErrorHandling('打开设备连接', async () => {
      const result = await rig.open();
      return result >= 0 ? '连接成功' : false;
    });
    
    await testWithErrorHandling('获取连接信息', async () => {
      const info = rig.getConnectionInfo();
      return info && info.connectionType ? `类型: ${info.connectionType}, 端口: ${info.portPath}` : false;
    });
    
    await testWithErrorHandling('获取支持的电台列表', async () => {
      const rigs = HamLib.getSupportedRigs();
      return Array.isArray(rigs) && rigs.length > 0 ? `找到${rigs.length}个支持的电台` : false;
    });
    
    // ========== 第二部分：VFO操作 ==========
    console.log('\n📡 第二部分：VFO (可变频率振荡器) 操作');
    
    await testWithErrorHandling('设置VFO-A', async () => {
      await rig.setVfo('VFO-A');
      return '设置成功';
    });
    
    await testWithErrorHandling('获取当前VFO', async () => {
      const vfo = await rig.getVfo();
      return `当前VFO: ${vfo}`;
    });
    
    await testWithErrorHandling('设置VFO-B', async () => {
      await rig.setVfo('VFO-B');
      return '设置成功';
    });
    
    // ========== 第三部分：频率控制 ==========
    console.log('\n📻 第三部分：频率控制');
    
    await testWithErrorHandling('设置频率 (144.39MHz)', async () => {
      await rig.setFrequency(144390000);
      return '设置成功';
    });
    
    await testWithErrorHandling('获取当前频率', async () => {
      const freq = await rig.getFrequency();
      return `频率: ${(freq/1000000).toFixed(3)} MHz`;
    });
    
    await testWithErrorHandling('设置VFO-A频率', async () => {
      await rig.setFrequency(145500000, 'VFO-A');
      return '设置成功';
    });
    
    await testWithErrorHandling('获取VFO-A频率', async () => {
      const freq = await rig.getFrequency('VFO-A');
      return `VFO-A频率: ${(freq/1000000).toFixed(3)} MHz`;
    });
    
    // ========== 第四部分：模式控制 ==========
    console.log('\n🎛️ 第四部分：模式控制');
    
    await testWithErrorHandling('设置FM模式', async () => {
      await rig.setMode('FM');
      return '设置成功';
    });
    
    await testWithErrorHandling('获取当前模式', async () => {
      const mode = await rig.getMode();
      return `模式: ${mode.mode}, 带宽: ${mode.width}`;
    });
    
    await testWithErrorHandling('设置USB模式', async () => {
      await rig.setMode('USB', 'wide');
      return '设置成功';
    });
    
    await testWithErrorHandling('设置VFO-A模式', async () => {
      await rig.setMode('LSB', 'narrow', 'VFO-A');
      return '设置成功';
    });
    
    // ========== 第五部分：PTT控制 ==========
    console.log('\n📢 第五部分：PTT (发射) 控制');
    
    await testWithErrorHandling('启用PTT', async () => {
      await rig.setPtt(true);
      return '发射启用';
    });
    
    await testWithErrorHandling('获取PTT状态', async () => {
      const ptt = await rig.getPtt();
      return `PTT状态: ${ptt ? '发射中' : '接收中'}`;
    });
    
    await testWithErrorHandling('禁用PTT', async () => {
      await rig.setPtt(false);
      return '发射禁用';
    });
    
    // ========== 第六部分：信号强度和载波检测 ==========
    console.log('\n📊 第六部分：信号强度和载波检测');
    
    await testWithErrorHandling('获取信号强度', async () => {
      const strength = await rig.getStrength();
      return `信号强度: ${strength}`;
    });
    
    await testWithErrorHandling('获取VFO-A信号强度', async () => {
      const strength = await rig.getStrength('VFO-A');
      return `VFO-A信号强度: ${strength}`;
    });
    
    await testWithErrorHandling('获取载波检测状态', async () => {
      const dcd = await rig.getDcd();
      return `载波检测: ${dcd ? '有载波' : '无载波'}`;
    });
    
    // ========== 第七部分：电平控制 ==========
    console.log('\n🔊 第七部分：电平控制');
    
    await testWithErrorHandling('获取支持的电平类型', async () => {
      const levels = rig.getSupportedLevels();
      return `支持${levels.length}种电平: ${levels.slice(0, 5).join(', ')}...`;
    });
    
    await testWithErrorHandling('设置音频电平 (70%)', async () => {
      await rig.setLevel('AF', 0.7);
      return '设置成功';
    });
    
    await testWithErrorHandling('获取音频电平', async () => {
      const level = await rig.getLevel('AF');
      return `AF电平: ${(level * 100).toFixed(1)}%`;
    });
    
    await testWithErrorHandling('设置发射功率 (50%)', async () => {
      await rig.setLevel('RFPOWER', 0.5);
      return '设置成功';
    });
    
    await testWithErrorHandling('获取发射功率', async () => {
      const level = await rig.getLevel('RFPOWER');
      return `发射功率: ${(level * 100).toFixed(1)}%`;
    });
    
    // ========== 第八部分：功能控制 ==========
    console.log('\n⚙️ 第八部分：功能控制');
    
    await testWithErrorHandling('获取支持的功能', async () => {
      const functions = rig.getSupportedFunctions();
      return `支持${functions.length}种功能: ${functions.slice(0, 5).join(', ')}...`;
    });
    
    await testWithErrorHandling('启用VOX功能', async () => {
      await rig.setFunction('VOX', true);
      return '启用成功';
    });
    
    await testWithErrorHandling('获取VOX功能状态', async () => {
      const enabled = await rig.getFunction('VOX');
      return `VOX: ${enabled ? '启用' : '禁用'}`;
    });
    
    await testWithErrorHandling('禁用VOX功能', async () => {
      await rig.setFunction('VOX', false);
      return '禁用成功';
    });
    
    // ========== 第九部分：分频操作 ==========
    console.log('\n🔀 第九部分：分频 (Split) 操作');
    
    await testWithErrorHandling('启用分频模式', async () => {
      await rig.setSplit(true, 'VFO-A', 'VFO-B');
      return '分频启用';
    });
    
    await testWithErrorHandling('获取分频状态', async () => {
      const split = await rig.getSplit();
      return `分频: ${split.enabled ? '启用' : '禁用'}, TX VFO: ${split.txVfo}`;
    });
    
    await testWithErrorHandling('设置分频TX频率', async () => {
      await rig.setSplitFreq(145600000);
      return '设置成功';
    });
    
    await testWithErrorHandling('获取分频TX频率', async () => {
      const freq = await rig.getSplitFreq();
      return `TX频率: ${(freq/1000000).toFixed(3)} MHz`;
    });
    
    await testWithErrorHandling('设置分频TX模式', async () => {
      await rig.setSplitMode('USB');
      return '设置成功';
    });
    
    await testWithErrorHandling('获取分频TX模式', async () => {
      const mode = await rig.getSplitMode();
      return `TX模式: ${mode.mode}, 带宽: ${mode.width}`;
    });
    
    await testWithErrorHandling('禁用分频模式', async () => {
      await rig.setSplit(false);
      return '分频禁用';
    });
    
    // ========== 第十部分：VFO操作 ==========
    console.log('\n🔄 第十部分：VFO操作');
    
    await testWithErrorHandling('复制VFO (A→B)', async () => {
      await rig.vfoOperation('CPY');
      return '复制成功';
    });
    
    await testWithErrorHandling('交换VFO (A⇄B)', async () => {
      await rig.vfoOperation('XCHG');
      return '交换成功';
    });
    
    await testWithErrorHandling('切换VFO', async () => {
      await rig.vfoOperation('TOGGLE');
      return '切换成功';
    });
    
    // ========== 第十一部分：调谐步进 ==========
    console.log('\n⬆️ 第十一部分：调谐步进');
    
    await testWithErrorHandling('设置调谐步进 (25kHz)', async () => {
      await rig.setTuningStep(25000);
      return '设置成功';
    });
    
    await testWithErrorHandling('获取调谐步进', async () => {
      const step = await rig.getTuningStep();
      return `调谐步进: ${step} Hz`;
    });
    
    await testWithErrorHandling('设置调谐步进 (12.5kHz)', async () => {
      await rig.setTuningStep(12500, 'VFO-A');
      return '设置成功';
    });
    
    // ========== 第十二部分：中继器设置 ==========
    console.log('\n📡 第十二部分：中继器设置');
    
    await testWithErrorHandling('设置中继器偏移方向 (+)', async () => {
      await rig.setRepeaterShift('PLUS');
      return '设置正偏移';
    });
    
    await testWithErrorHandling('获取中继器偏移方向', async () => {
      const shift = await rig.getRepeaterShift();
      return `偏移方向: ${shift}`;
    });
    
    await testWithErrorHandling('设置中继器偏移频率 (600kHz)', async () => {
      await rig.setRepeaterOffset(600000);
      return '设置成功';
    });
    
    await testWithErrorHandling('获取中继器偏移频率', async () => {
      const offset = await rig.getRepeaterOffset();
      return `偏移频率: ${offset} Hz`;
    });
    
    await testWithErrorHandling('设置中继器偏移方向 (-)', async () => {
      await rig.setRepeaterShift('MINUS');
      return '设置负偏移';
    });
    
    await testWithErrorHandling('关闭中继器偏移', async () => {
      await rig.setRepeaterShift('NONE');
      return '偏移关闭';
    });
    
    // ========== 第十三部分：RIT/XIT控制 ==========
    console.log('\n🎯 第十三部分：RIT/XIT 控制');
    
    await testWithErrorHandling('设置RIT偏移 (+100Hz)', async () => {
      await rig.setRit(100);
      return '设置成功';
    });
    
    await testWithErrorHandling('获取RIT偏移', async () => {
      const rit = await rig.getRit();
      return `RIT偏移: ${rit} Hz`;
    });
    
    await testWithErrorHandling('设置XIT偏移 (-50Hz)', async () => {
      await rig.setXit(-50);
      return '设置成功';
    });
    
    await testWithErrorHandling('获取XIT偏移', async () => {
      const xit = await rig.getXit();
      return `XIT偏移: ${xit} Hz`;
    });
    
    await testWithErrorHandling('清除RIT/XIT偏移', async () => {
      await rig.clearRitXit();
      return '清除成功';
    });
    
    // ========== 第十四部分：扫描功能 ==========
    console.log('\n🔍 第十四部分：扫描功能');
    
    await testWithErrorHandling('启动VFO扫描', async () => {
      await rig.startScan('VFO');
      return '扫描启动';
    });
    
    await testWithErrorHandling('停止扫描', async () => {
      await rig.stopScan();
      return '扫描停止';
    });
    
    // ========== 第十五部分：内存通道操作 ==========
    console.log('\n💾 第十五部分：内存通道操作');
    
    await testWithErrorHandling('设置内存通道1', async () => {
      await rig.setMemoryChannel(1, {
        frequency: 144390000,
        mode: 'FM',
        description: '测试通道1'
      });
      return '设置成功';
    });
    
    await testWithErrorHandling('获取内存通道1', async () => {
      const channel = await rig.getMemoryChannel(1);
      return `频率: ${(channel.frequency/1000000).toFixed(3)}MHz, 模式: ${channel.mode}`;
    });
    
    await testWithErrorHandling('选择内存通道1', async () => {
      await rig.selectMemoryChannel(1);
      return '选择成功';
    });
    
    await testWithErrorHandling('获取当前内存通道号', async () => {
      const mem = await rig.getMem();
      return `当前通道: ${mem}`;
    });
    
    // ========== 第十六部分：电源和状态 ==========
    console.log('\n🔋 第十六部分：电源和状态');
    
    await testWithErrorHandling('设置电源状态 (开机)', async () => {
      await rig.setPowerstat(1);
      return '设置成功';
    });
    
    await testWithErrorHandling('获取电源状态', async () => {
      const power = await rig.getPowerstat();
      const states = {0: '关机', 1: '开机', 2: '待机', 4: '工作', 8: '未知'};
      return `电源状态: ${states[power] || power}`;
    });
    
    // ========== 第十七部分：CTCSS/DCS控制 ==========
    console.log('\n🎵 第十七部分：CTCSS/DCS 控制');
    
    await testWithErrorHandling('设置CTCSS音调 (100.0Hz)', async () => {
      await rig.setCtcssTone(1000); // 1000 = 100.0Hz
      return '设置成功';
    });
    
    await testWithErrorHandling('获取CTCSS音调', async () => {
      const tone = await rig.getCtcssTone();
      return `CTCSS音调: ${tone/10} Hz`;
    });
    
    await testWithErrorHandling('设置CTCSS SQL音调 (131.8Hz)', async () => {
      await rig.setCtcssSql(1318); // 1318 = 131.8Hz
      return '设置成功';
    });
    
    await testWithErrorHandling('获取CTCSS SQL音调', async () => {
      const tone = await rig.getCtcssSql();
      return `CTCSS SQL音调: ${tone/10} Hz`;
    });
    
    await testWithErrorHandling('设置DCS码 (023)', async () => {
      await rig.setDcsCode(23);
      return '设置成功';
    });
    
    await testWithErrorHandling('获取DCS码', async () => {
      const code = await rig.getDcsCode();
      return `DCS码: ${code.toString().padStart(3, '0')}`;
    });
    
    // ========== 第十八部分：天线控制 ==========
    console.log('\n📶 第十八部分：天线控制');
    
    await testWithErrorHandling('设置天线1', async () => {
      await rig.setAntenna(1);
      return '设置成功';
    });
    
    await testWithErrorHandling('获取天线信息', async () => {
      const antenna = await rig.getAntenna();
      return `当前天线: ${antenna.currentAntenna}, TX: ${antenna.txAntenna}, RX: ${antenna.rxAntenna}`;
    });
    
    await testWithErrorHandling('设置天线2', async () => {
      await rig.setAntenna(2);
      return '设置成功';
    });
    
    // ========== 第十九部分：串口配置 ==========
    console.log('\n🔌 第十九部分：串口配置');
    
    await testWithErrorHandling('获取支持的串口配置', async () => {
      const configs = rig.getSupportedSerialConfigs();
      return `数据位选项: ${configs.serial.data_bits.length}种, PTT类型: ${configs.ptt_type.length}种`;
    });
    
    await testWithErrorHandling('设置串口数据位', async () => {
      await rig.setSerialConfig('data_bits', '8');
      return '设置成功';
    });
    
    await testWithErrorHandling('获取串口数据位', async () => {
      const bits = await rig.getSerialConfig('data_bits');
      return `数据位: ${bits}`;
    });
    
    await testWithErrorHandling('设置PTT类型', async () => {
      await rig.setPttType('RIG');
      return '设置成功';
    });
    
    await testWithErrorHandling('获取PTT类型', async () => {
      const type = await rig.getPttType();
      return `PTT类型: ${type}`;
    });
    
    await testWithErrorHandling('设置DCD类型', async () => {
      await rig.setDcdType('RIG');
      return '设置成功';
    });
    
    await testWithErrorHandling('获取DCD类型', async () => {
      const type = await rig.getDcdType();
      return `DCD类型: ${type}`;
    });
    
    // ========== 第二十部分：参数控制 ==========
    console.log('\n🛠️ 第二十部分：参数控制');
    
    await testWithErrorHandling('设置参数 (背光亮度)', async () => {
      await rig.setParm('BACKLIGHT', 0.8);
      return '设置成功';
    });
    
    await testWithErrorHandling('获取参数 (背光亮度)', async () => {
      const value = await rig.getParm('BACKLIGHT');
      return `背光亮度: ${(value * 100).toFixed(1)}%`;
    });
    
    // ========== 第二十一部分：DTMF支持 ==========
    console.log('\n☎️ 第二十一部分：DTMF支持');
    
    await testWithErrorHandling('发送DTMF数字', async () => {
      await rig.sendDtmf('1234');
      return '发送成功';
    });
    
    await testWithErrorHandling('接收DTMF数字', async () => {
      const result = await rig.recvDtmf(10);
      return `接收到: "${result.digits}" (${result.length}位数字)`;
    });
    
    // ========== 第二十二部分：高级功能 ==========
    console.log('\n🚀 第二十二部分：高级功能');
    
    await testWithErrorHandling('获取内存通道总数', async () => {
      const count = await rig.memCount();
      return `总内存通道数: ${count}`;
    });
    
    await testWithErrorHandling('设置内存组/银行', async () => {
      await rig.setBank(1);
      return '设置成功';
    });
    
    await testWithErrorHandling('功率转换 (功率→毫瓦)', async () => {
      const mw = await rig.power2mW(0.5, 144390000, 'FM');
      return `50%功率 = ${mw} mW`;
    });
    
    await testWithErrorHandling('功率转换 (毫瓦→功率)', async () => {
      const power = await rig.mW2power(1000, 144390000, 'FM');
      return `1000mW = ${(power * 100).toFixed(1)}%功率`;
    });
    
    await testWithErrorHandling('发送摩尔斯电码', async () => {
      await rig.sendMorse('TEST');
      return '发送成功';
    });
    
    await testWithErrorHandling('停止摩尔斯电码', async () => {
      await rig.stopMorse();
      return '停止成功';
    });
    
    await testWithErrorHandling('等待摩尔斯电码完成', async () => {
      await rig.waitMorse();
      return '等待完成';
    });
    
    await testWithErrorHandling('播放语音内存', async () => {
      await rig.sendVoiceMem(1);
      return '播放成功';
    });
    
    await testWithErrorHandling('停止语音内存', async () => {
      await rig.stopVoiceMem();
      return '停止成功';
    });
    
    await testWithErrorHandling('设置分频频率和模式', async () => {
      await rig.setSplitFreqMode(145500000, 'USB', 2400);
      return '设置成功';
    });
    
    await testWithErrorHandling('获取分频频率和模式', async () => {
      const result = await rig.getSplitFreqMode();
      return `TX: ${(result.txFrequency/1000000).toFixed(3)}MHz, ${result.txMode}, ${result.txWidth}Hz`;
    });
    
    // ========== 第二十三部分：系统重置 ==========
    console.log('\n🔄 第二十三部分：系统重置 (谨慎操作)');
    
    await testWithErrorHandling('软复位 (测试)', async () => {
      await rig.reset('SOFT');
      return '软复位成功';
    });
    
    console.log('\n🏁 测试完成，正在关闭连接...');
    
    // ========== 清理和关闭 ==========
    await testWithErrorHandling('关闭设备连接', async () => {
      await rig.close();
      return '连接已关闭';
    });
    
    await testWithErrorHandling('销毁设备对象', async () => {
      await rig.destroy();
      return '对象已销毁';
    });
    
  } catch (error) {
    console.error('❌ 测试过程中发生严重错误:', error.message);
    console.error(error.stack);
    testsFailed++;
  }
  
  // 输出测试总结
  console.log('\n' + '='.repeat(60));
  console.log('🧪 测试总结报告');
  console.log('='.repeat(60));
  console.log(`✅ 通过: ${testsPassed} 个测试`);
  console.log(`❌ 失败: ${testsFailed} 个测试`);
  console.log(`⏭️ 跳过: ${testsSkipped} 个测试 (Dummy设备不支持)`);
  console.log(`📊 总计: ${testsPassed + testsFailed + testsSkipped} 个测试`);
  
  const successRate = testsPassed / (testsPassed + testsFailed) * 100;
  console.log(`📈 成功率: ${successRate.toFixed(1)}% (不包含跳过的测试)`);
  
  if (testsFailed === 0) {
    console.log('\n🎉 所有实现的API功能测试通过！');
    console.log('💡 注意：部分功能在Dummy设备上不可用，这是正常现象。');
    console.log('🔧 要测试完整功能，请使用实际的无线电设备。');
  } else {
    console.log(`\n⚠️ 有 ${testsFailed} 个测试失败，请检查具体错误信息。`);
  }
  
  console.log('\n📝 测试说明：');
  console.log('- 本测试使用Hamlib Model 1 (Dummy设备)');
  console.log('- Dummy设备模拟大部分功能但不包含所有特性');
  console.log('- "跳过"的测试表示该功能在Dummy设备上未实现');
  console.log('- 实际使用时需要对应的无线电设备和正确的模型号');
  console.log('- 运行前请确保已启动: rigctld -m 1 -t 4532');
  
  console.log('\n✨ 测试完成！');
  
  // 根据测试结果设置退出代码
  process.exit(testsFailed > 0 ? 1 : 0);
}

// 启动测试
runCompleteTest().catch(error => {
  console.error('❌ 测试启动失败:', error);
  process.exit(1);
});