/**
 * Demo script to show how to use getSupportedRigs() API
 */

const { HamLib } = require('../index.js');

console.log('=== HamLib getSupportedRigs() Demo ===\n');

try {
  // Get all supported rigs
  const supportedRigs = HamLib.getSupportedRigs();
  
  console.log(`🎯 Found ${supportedRigs.length} supported radio models\n`);
  
  // Show some popular manufacturers
  console.log('📻 Popular manufacturers:');
  const manufacturers = [...new Set(supportedRigs.map(rig => rig.mfgName))];
  const popularManufacturers = ['Yaesu', 'Icom', 'Kenwood', 'Elecraft', 'FlexRadio'];
  
  popularManufacturers.forEach(mfg => {
    const rigs = supportedRigs.filter(rig => rig.mfgName === mfg);
    if (rigs.length > 0) {
      console.log(`  ${mfg}: ${rigs.length} models`);
    }
  });
  
  console.log('\n📋 Sample Yaesu radios:');
  const yaesus = supportedRigs.filter(rig => rig.mfgName === 'Yaesu').slice(0, 5);
  yaesus.forEach(rig => {
    console.log(`  ID: ${rig.rigModel} - ${rig.modelName} (${rig.status})`);
  });
  
  console.log('\n📋 Sample Icom radios:');
  const icoms = supportedRigs.filter(rig => rig.mfgName === 'Icom').slice(0, 5);
  icoms.forEach(rig => {
    console.log(`  ID: ${rig.rigModel} - ${rig.modelName} (${rig.status})`);
  });
  
  console.log('\n🔍 How to find your radio:');
  console.log('1. Look for your manufacturer in the list above');
  console.log('2. Find your specific model using the ID number');
  console.log('3. Use the rigModel ID when creating a HamLib instance');
  
  console.log('\n💡 Example usage:');
  console.log('```javascript');
  console.log('const { HamLib } = require("hamlib");');
  console.log('');
  console.log('// Get all supported rigs');
  console.log('const rigs = HamLib.getSupportedRigs();');
  console.log('');
  console.log('// Find a specific rig (e.g., Yaesu FT-847)');
  console.log('const ft847 = rigs.find(rig => rig.modelName === "FT-847");');
  console.log('console.log("FT-847 Model ID:", ft847.rigModel);');
  console.log('');
  console.log('// Create HamLib instance with the model ID');
  console.log('const radio = new HamLib(ft847.rigModel, "/dev/ttyUSB0");');
  console.log('```');
  
} catch (error) {
  console.error('❌ Error:', error.message);
} 