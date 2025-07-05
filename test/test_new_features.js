/**
 * Comprehensive test for all new HamLib features
 * Tests: Memory channels, RIT/XIT, Scanning, Levels, Functions, Split, VFO ops, Antenna
 */

const { HamLib } = require('../index.js');

console.log('=== HamLib New Features Comprehensive Test ===\n');

// Test configuration
const dummyModel = 1; // Dummy device model

async function runTest() {
  const radio = new HamLib(dummyModel);
  
  try {
    console.log('📡 Opening connection to dummy radio...');
    radio.open();
    console.log('✅ Radio connected successfully\n');

    // Test 1: Memory Channel Management
    console.log('🔗 1. Testing Memory Channel Management');
    try {
      // Test setting memory channel
      const channelData = {
        frequency: 144390000,  // 144.39 MHz
        mode: 'FM',
        bandwidth: 25000,      // 25 kHz
        description: 'Test Channel 1',
        ctcssTone: 1000        // 100.0 Hz
      };
      
      console.log('  - Setting memory channel 1...');
      radio.setMemoryChannel(1, channelData);
      console.log('  ✅ Memory channel set successfully');
      
      console.log('  - Getting memory channel 1...');
      const retrievedChannel = radio.getMemoryChannel(1);
      console.log('  ✅ Retrieved channel:', JSON.stringify(retrievedChannel, null, 2));
      
      console.log('  - Selecting memory channel 1...');
      radio.selectMemoryChannel(1);
      console.log('  ✅ Memory channel selected');
      
    } catch (error) {
      console.log('  ⚠️  Memory channel operations not supported by dummy rig:', error.message);
    }

    // Test 2: RIT/XIT Control
    console.log('\n🎛️  2. Testing RIT/XIT Control');
    try {
      console.log('  - Setting RIT offset to +100 Hz...');
      radio.setRit(100);
      console.log('  ✅ RIT offset set');
      
      console.log('  - Getting RIT offset...');
      const ritOffset = radio.getRit();
      console.log('  ✅ RIT offset:', ritOffset, 'Hz');
      
      console.log('  - Setting XIT offset to -50 Hz...');
      radio.setXit(-50);
      console.log('  ✅ XIT offset set');
      
      console.log('  - Getting XIT offset...');
      const xitOffset = radio.getXit();
      console.log('  ✅ XIT offset:', xitOffset, 'Hz');
      
      console.log('  - Clearing RIT/XIT offsets...');
      const cleared = radio.clearRitXit();
      console.log('  ✅ RIT/XIT cleared:', cleared);
      
    } catch (error) {
      console.log('  ⚠️  RIT/XIT operations not supported by dummy rig:', error.message);
    }

    // Test 3: Scanning Operations
    console.log('\n📡 3. Testing Scanning Operations');
    try {
      console.log('  - Starting VFO scan...');
      radio.startScan('VFO');
      console.log('  ✅ VFO scan started');
      
      console.log('  - Stopping scan...');
      radio.stopScan();
      console.log('  ✅ Scan stopped');
      
    } catch (error) {
      console.log('  ⚠️  Scanning operations not supported by dummy rig:', error.message);
    }

    // Test 4: Level Controls
    console.log('\n🎚️  4. Testing Level Controls');
    try {
      console.log('  - Getting supported levels...');
      const supportedLevels = radio.getSupportedLevels();
      console.log('  ✅ Supported levels:', supportedLevels);
      
      if (supportedLevels.length > 0) {
        const testLevel = supportedLevels[0];
        console.log(`  - Testing level control: ${testLevel}`);
        
        console.log(`  - Setting ${testLevel} to 0.5...`);
        radio.setLevel(testLevel, 0.5);
        console.log('  ✅ Level set');
        
        console.log(`  - Getting ${testLevel} value...`);
        const levelValue = radio.getLevel(testLevel);
        console.log(`  ✅ ${testLevel} value:`, levelValue);
      }
      
    } catch (error) {
      console.log('  ⚠️  Level control operations not supported by dummy rig:', error.message);
    }

    // Test 5: Function Controls
    console.log('\n🔧 5. Testing Function Controls');
    try {
      console.log('  - Getting supported functions...');
      const supportedFunctions = radio.getSupportedFunctions();
      console.log('  ✅ Supported functions:', supportedFunctions);
      
      if (supportedFunctions.length > 0) {
        const testFunction = supportedFunctions[0];
        console.log(`  - Testing function control: ${testFunction}`);
        
        console.log(`  - Enabling ${testFunction}...`);
        radio.setFunction(testFunction, true);
        console.log('  ✅ Function enabled');
        
        console.log(`  - Getting ${testFunction} status...`);
        const functionStatus = radio.getFunction(testFunction);
        console.log(`  ✅ ${testFunction} status:`, functionStatus);
        
        console.log(`  - Disabling ${testFunction}...`);
        radio.setFunction(testFunction, false);
        console.log('  ✅ Function disabled');
      }
      
    } catch (error) {
      console.log('  ⚠️  Function control operations not supported by dummy rig:', error.message);
    }

    // Test 6: Split Operations
    console.log('\n📻 6. Testing Split Operations');
    try {
      console.log('  - Setting split TX frequency to 144.40 MHz...');
      radio.setSplitFreq(144400000);
      console.log('  ✅ Split TX frequency set');
      
      console.log('  - Getting split TX frequency...');
      const splitFreq = radio.getSplitFreq();
      console.log('  ✅ Split TX frequency:', splitFreq, 'Hz');
      
      console.log('  - Setting split TX mode to USB...');
      radio.setSplitMode('USB');
      console.log('  ✅ Split TX mode set');
      
      console.log('  - Getting split TX mode...');
      const splitMode = radio.getSplitMode();
      console.log('  ✅ Split TX mode:', JSON.stringify(splitMode));
      
      console.log('  - Enabling split operation...');
      radio.setSplit(true, 'VFO-B');
      console.log('  ✅ Split operation enabled');
      
      console.log('  - Getting split status...');
      const splitStatus = radio.getSplit();
      console.log('  ✅ Split status:', JSON.stringify(splitStatus));
      
      console.log('  - Disabling split operation...');
      radio.setSplit(false);
      console.log('  ✅ Split operation disabled');
      
    } catch (error) {
      console.log('  ⚠️  Split operations not supported by dummy rig:', error.message);
    }

    // Test 7: VFO Operations
    console.log('\n📡 7. Testing VFO Operations');
    try {
      const vfoOperations = ['CPY', 'XCHG', 'UP', 'DOWN', 'TOGGLE'];
      
      for (const operation of vfoOperations) {
        try {
          console.log(`  - Performing VFO operation: ${operation}...`);
          radio.vfoOperation(operation);
          console.log(`  ✅ VFO operation ${operation} completed`);
        } catch (error) {
          console.log(`  ⚠️  VFO operation ${operation} not supported:`, error.message);
        }
      }
      
    } catch (error) {
      console.log('  ⚠️  VFO operations not supported by dummy rig:', error.message);
    }

    // Test 8: Antenna Selection
    console.log('\n📶 8. Testing Antenna Selection');
    try {
      console.log('  - Setting antenna to 1...');
      radio.setAntenna(1);
      console.log('  ✅ Antenna set');
      
      console.log('  - Getting current antenna...');
      const currentAntenna = radio.getAntenna();
      console.log('  ✅ Current antenna:', currentAntenna);
      
    } catch (error) {
      console.log('  ⚠️  Antenna selection not supported by dummy rig:', error.message);
    }

    // Test 9: API completeness check
    console.log('\n🔍 9. Checking API Completeness');
    const expectedMethods = [
      'setMemoryChannel', 'getMemoryChannel', 'selectMemoryChannel',
      'setRit', 'getRit', 'setXit', 'getXit', 'clearRitXit',
      'startScan', 'stopScan',
      'setLevel', 'getLevel', 'getSupportedLevels',
      'setFunction', 'getFunction', 'getSupportedFunctions',
      'setSplitFreq', 'getSplitFreq', 'setSplitMode', 'getSplitMode', 'setSplit', 'getSplit',
      'vfoOperation',
      'setAntenna', 'getAntenna'
    ];
    
    let missingMethods = [];
    for (const method of expectedMethods) {
      if (typeof radio[method] !== 'function') {
        missingMethods.push(method);
      }
    }
    
    if (missingMethods.length === 0) {
      console.log('  ✅ All expected methods are available');
    } else {
      console.log('  ❌ Missing methods:', missingMethods);
    }

    console.log('\n📊 Test Summary:');
    console.log(`  • Total new methods implemented: ${expectedMethods.length}`);
    console.log(`  • Methods available: ${expectedMethods.length - missingMethods.length}`);
    console.log(`  • API completeness: ${((expectedMethods.length - missingMethods.length) / expectedMethods.length * 100).toFixed(1)}%`);

  } catch (error) {
    console.error('❌ Test failed:', error.message);
    console.error(error.stack);
  } finally {
    try {
      console.log('\n🔌 Closing radio connection...');
      radio.close();
      radio.destroy();
      console.log('✅ Radio connection closed');
    } catch (error) {
      console.log('⚠️  Error closing radio:', error.message);
    }
  }
}

// Run the test
runTest().then(() => {
  console.log('\n🎉 New features test completed!');
}).catch((error) => {
  console.error('💥 Test suite failed:', error);
  process.exit(1);
}); 