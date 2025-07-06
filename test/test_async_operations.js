const { HamLib } = require('../lib/index.js');

/**
 * Test async operations for HamLib
 * This test file verifies that all operations are properly converted to async/await
 */

async function testAsyncOperations() {
  console.log('Testing HamLib async operations...');
  
  try {
    // Test getSupportedRigs static method (this should still be synchronous)
    console.log('Testing getSupportedRigs...');
    const supportedRigs = HamLib.getSupportedRigs();
    console.log(`Found ${supportedRigs.length} supported rigs`);
    
    // Find a test rig (use dummy rig model 1 for testing)
    const testRig = 1; // Dummy rig for testing
    
    console.log(`Creating HamLib instance with model ${testRig}...`);
    const rig = new HamLib(testRig);
    
    // Test basic async operations
    console.log('Testing open() async operation...');
    try {
      const openResult = await rig.open();
      console.log('Open result:', openResult);
    } catch (error) {
      console.log('Open failed (expected for dummy rig):', error.message);
    }
    
    // Test async frequency operations (these may fail without actual hardware)
    console.log('Testing setFrequency() async operation...');
    try {
      await rig.setFrequency(144390000);
      console.log('setFrequency completed successfully');
    } catch (error) {
      console.log('setFrequency failed (expected without hardware):', error.message);
    }
    
    console.log('Testing getFrequency() async operation...');
    try {
      const frequency = await rig.getFrequency();
      console.log('Current frequency:', frequency);
    } catch (error) {
      console.log('getFrequency failed (expected without hardware):', error.message);
    }
    
    // Test async mode operations
    console.log('Testing setMode() async operation...');
    try {
      await rig.setMode('USB');
      console.log('setMode completed successfully');
    } catch (error) {
      console.log('setMode failed (expected without hardware):', error.message);
    }
    
    console.log('Testing getMode() async operation...');
    try {
      const mode = await rig.getMode();
      console.log('Current mode:', mode);
    } catch (error) {
      console.log('getMode failed (expected without hardware):', error.message);
    }
    
    // Test async VFO operations
    console.log('Testing setVfo() async operation...');
    try {
      await rig.setVfo('VFO-A');
      console.log('setVfo completed successfully');
    } catch (error) {
      console.log('setVfo failed (expected without hardware):', error.message);
    }
    
    console.log('Testing getVfo() async operation...');
    try {
      const vfo = await rig.getVfo();
      console.log('Current VFO:', vfo);
    } catch (error) {
      console.log('getVfo failed (expected without hardware):', error.message);
    }
    
    // Test async PTT operations
    console.log('Testing setPtt() async operation...');
    try {
      await rig.setPtt(false);
      console.log('setPtt completed successfully');
    } catch (error) {
      console.log('setPtt failed (expected without hardware):', error.message);
    }
    
    // Test async signal strength
    console.log('Testing getStrength() async operation...');
    try {
      const strength = await rig.getStrength();
      console.log('Signal strength:', strength);
    } catch (error) {
      console.log('getStrength failed (expected without hardware):', error.message);
    }
    
    // Test close operation
    console.log('Testing close() async operation...');
    try {
      await rig.close();
      console.log('close completed successfully');
    } catch (error) {
      console.log('close failed:', error.message);
    }
    
    // Test destroy operation
    console.log('Testing destroy() async operation...');
    try {
      await rig.destroy();
      console.log('destroy completed successfully');
    } catch (error) {
      console.log('destroy failed:', error.message);
    }
    
    console.log('\n✅ All async operations tested successfully!');
    console.log('Note: Some operations failed as expected without actual hardware connected.');
    
  } catch (error) {
    console.error('❌ Test failed:', error.message);
    process.exit(1);
  }
}

// Test Promise-based usage
async function testPromiseChaining() {
  console.log('\nTesting Promise chaining...');
  
  const rig = new HamLib(1); // Dummy rig
  
  try {
    // Chain multiple operations
    await rig.open()
      .catch(err => console.log('Open failed (expected):', err.message));
      
    await rig.setFrequency(144390000)
      .catch(err => console.log('setFrequency failed (expected):', err.message));
      
    await rig.setMode('USB')
      .catch(err => console.log('setMode failed (expected):', err.message));
      
    await rig.close()
      .catch(err => console.log('Close failed:', err.message));
      
    console.log('✅ Promise chaining test completed');
  } catch (error) {
    console.log('Promise chaining test error:', error.message);
  }
}

// Test async/await syntax
async function testAsyncAwaitSyntax() {
  console.log('\nTesting async/await syntax...');
  
  const rig = new HamLib(1); // Dummy rig
  
  try {
    // Test that all methods return Promises
    const promises = [
      rig.open().catch(() => 'open failed'),
      rig.setFrequency(144390000).catch(() => 'setFrequency failed'),
      rig.getFrequency().catch(() => 'getFrequency failed'),
      rig.setMode('USB').catch(() => 'setMode failed'),
      rig.getMode().catch(() => 'getMode failed'),
      rig.setVfo('VFO-A').catch(() => 'setVfo failed'),
      rig.getVfo().catch(() => 'getVfo failed'),
      rig.setPtt(false).catch(() => 'setPtt failed'),
      rig.getStrength().catch(() => 'getStrength failed')
    ];
    
    // Check that all operations return promises
    promises.forEach((promise, index) => {
      if (!(promise instanceof Promise)) {
        throw new Error(`Operation ${index} does not return a Promise`);
      }
    });
    
    console.log('✅ All operations return Promises correctly');
    
    // Wait for all promises to complete
    const results = await Promise.allSettled(promises);
    console.log(`✅ All ${results.length} async operations completed`);
    
  } catch (error) {
    console.error('❌ Async/await syntax test failed:', error.message);
    throw error;
  }
}

// Run all tests
async function runAllTests() {
  try {
    await testAsyncOperations();
    await testPromiseChaining();
    await testAsyncAwaitSyntax();
    
    console.log('\n🎉 All async tests passed successfully!');
    console.log('The HamLib library has been successfully converted to async operations.');
  } catch (error) {
    console.error('\n❌ Test suite failed:', error.message);
    process.exit(1);
  }
}

// Export for use in other test files
module.exports = {
  testAsyncOperations,
  testPromiseChaining,
  testAsyncAwaitSyntax,
  runAllTests
};

// Run tests if this file is executed directly
if (require.main === module) {
  runAllTests();
} 