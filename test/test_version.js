/**
 * Test script for getHamlibVersion API
 *
 * This test verifies that the Hamlib version API works correctly
 * and returns version information in the expected format.
 */

const { HamLib } = require('../index.js');

console.log('=== Testing getHamlibVersion API ===\n');

try {
  // Test 1: Call getHamlibVersion
  console.log('Test 1: Getting Hamlib version...');
  const version = HamLib.getHamlibVersion();

  // Test 2: Verify the return type
  console.log('Test 2: Verifying return type...');
  if (typeof version !== 'string') {
    throw new Error(`Expected string, got ${typeof version}`);
  }
  console.log('✓ Return type is string');

  // Test 3: Verify version format (should start with "Hamlib")
  console.log('\nTest 3: Verifying version format...');
  if (!version.startsWith('Hamlib')) {
    throw new Error(`Version should start with "Hamlib", got: ${version}`);
  }
  console.log('✓ Version format is correct');

  // Test 4: Display the version
  console.log('\n=== Hamlib Version Information ===');
  console.log(`Version: ${version}`);
  console.log('==================================\n');

  // Extract version details
  const parts = version.split(' ');
  if (parts.length >= 2) {
    console.log('Version breakdown:');
    console.log(`  Library: ${parts[0]}`);
    console.log(`  Version number: ${parts[1]}`);
    if (parts.length >= 4) {
      console.log(`  Build date: ${parts[2]} ${parts[3]}`);
    }
    if (parts.length >= 5) {
      console.log(`  Architecture: ${parts[4]}`);
    }
  }

  console.log('\n✓ All tests passed!');
  process.exit(0);

} catch (error) {
  console.error('\n✗ Test failed:', error.message);
  console.error(error.stack);
  process.exit(1);
}
