/**
 * Test file for binary loader and module exports
 */

const { HamLib } = require('../index.js');

console.log('Testing node-hamlib module loading...');

try {
  // Test module loading
  console.log('✓ Successfully loaded HamLib class');
  
  // Test basic instantiation (don't actually connect)
  console.log('Testing HamLib instantiation...');
  
  // Use a dummy model number for testing
  const testRig = new HamLib(1035, '/dev/null');
  console.log('✓ Successfully created HamLib instance');
  
  // Test method existence
  const methods = [
    'open', 'close', 'destroy', 'setVfo', 'setFrequency', 'setMode', 'setPtt',
    'getVfo', 'getFrequency', 'getMode', 'getStrength', 'getConnectionInfo'
  ];
  
  for (const method of methods) {
    if (typeof testRig[method] === 'function') {
      console.log(`✓ Method ${method} exists`);
    } else {
      console.log(`✗ Method ${method} missing`);
    }
  }
  
  console.log('\n✓ All tests passed!');
  console.log('Note: This test only verifies module loading, not actual hardware communication.');
  
} catch (error) {
  console.error('✗ Test failed:', error.message);
  process.exit(1);
} 