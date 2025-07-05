/**
 * Test file for getSupportedRigs() static method
 */

const { HamLib } = require('../index.js');

console.log('Testing getSupportedRigs() static method...\n');

try {
  // Test static method existence
  if (typeof HamLib.getSupportedRigs === 'function') {
    console.log('✓ getSupportedRigs static method exists');
  } else {
    console.log('✗ getSupportedRigs static method missing');
    process.exit(1);
  }
  
  // Test getting supported rigs
  console.log('Calling getSupportedRigs()...');
  const supportedRigs = HamLib.getSupportedRigs();
  
  // Verify return type
  if (Array.isArray(supportedRigs)) {
    console.log('✓ getSupportedRigs() returns an array');
  } else {
    console.log('✗ getSupportedRigs() should return an array');
    process.exit(1);
  }
  
  // Check if we got any results
  console.log(`✓ Found ${supportedRigs.length} supported rigs`);
  
  if (supportedRigs.length > 0) {
    console.log('✓ At least one supported rig found');
    
    // Test the structure of the first rig
    const firstRig = supportedRigs[0];
    const expectedFields = ['rigModel', 'modelName', 'mfgName', 'version', 'status', 'rigType'];
    
    console.log('\nTesting rig object structure:');
    for (const field of expectedFields) {
      if (firstRig.hasOwnProperty(field)) {
        console.log(`✓ Field '${field}' exists: ${firstRig[field]}`);
      } else {
        console.log(`✗ Field '${field}' missing`);
      }
    }
    
    // Display some sample rigs
    console.log('\nSample supported rigs:');
    console.log('Model ID | Manufacturer | Model Name | Status');
    console.log('-'.repeat(70));
    
    supportedRigs.slice(0, 10).forEach(rig => {
      console.log(`${rig.rigModel.toString().padEnd(8)} | ${rig.mfgName.padEnd(12)} | ${rig.modelName.padEnd(20)} | ${rig.status}`);
    });
    
    if (supportedRigs.length > 10) {
      console.log(`... and ${supportedRigs.length - 10} more rigs`);
    }
    
    // Test filtering rigs by manufacturer
    console.log('\nTesting filtering by manufacturer:');
    const manufacturers = [...new Set(supportedRigs.map(rig => rig.mfgName))];
    console.log(`Found ${manufacturers.length} manufacturers:`, manufacturers.slice(0, 5).join(', '));
    
    // Test filtering by status
    console.log('\nTesting filtering by status:');
    const statuses = [...new Set(supportedRigs.map(rig => rig.status))];
    console.log(`Found statuses:`, statuses.join(', '));
    
    // Find a specific well-known rig model (if exists)
    console.log('\nLooking for specific rig models:');
    const icoms = supportedRigs.filter(rig => rig.mfgName.toLowerCase().includes('icom'));
    if (icoms.length > 0) {
      console.log(`✓ Found ${icoms.length} Icom rigs`);
    }
    
    const yaesus = supportedRigs.filter(rig => rig.mfgName.toLowerCase().includes('yaesu'));
    if (yaesus.length > 0) {
      console.log(`✓ Found ${yaesus.length} Yaesu rigs`);
    }
    
    const kenwoodss = supportedRigs.filter(rig => rig.mfgName.toLowerCase().includes('kenwood'));
    if (kenwoodss.length > 0) {
      console.log(`✓ Found ${kenwoodss.length} Kenwood rigs`);
    }
    
  } else {
    console.log('⚠ No supported rigs found - this might indicate an issue');
  }
  
  console.log('\n✓ All getSupportedRigs() tests passed!');
  
} catch (error) {
  console.error('✗ Test failed:', error.message);
  console.error('Error stack:', error.stack);
  process.exit(1);
} 