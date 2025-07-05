// Test script for network connection to rigctld
const nodeham = require('../build/Release/hamlib');

console.log('=== NodeHamLib Network Connection Test ===\n');

async function delay(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

async function testNetworkConnection() {
    try {
        console.log('🔗 Testing network connection to localhost:5001...\n');
        
        // Create HamLib instance with network address
        // Using model 1035 (Yaesu FT-991A) but it will auto-switch to NETRIGCTL (model 2)
        const networkRig = new nodeham.HamLib(2, 'localhost:5001');
        
        // Get connection info before opening
        console.log('📋 Connection Info (before open):');
        console.log(networkRig.getConnectionInfo());
        console.log('');
        
        // Test opening connection
        console.log('🚀 Opening network connection...');
        const openResult = networkRig.open();
        console.log(`Open result: ${openResult}`);
        
        if (openResult !== 0) {
            console.log('❌ Failed to open network connection. Make sure rigctld is running on port 5001.');
            console.log('   Start rigctld with: rigctld -m 1035 -r /dev/ttyUSB0 -t 5001');
            return;
        }
        
        console.log('✅ Network connection established!\n');
        
        // Get connection info after opening
        console.log('📋 Connection Info (after open):');
        console.log(networkRig.getConnectionInfo());
        console.log('');
        
        // Test various API functions
        console.log('🧪 Testing API functions...\n');
        
        // Test 1: Get current frequency
        console.log('📻 Test 1: Get current frequency');
        try {
            const freq = networkRig.getFrequency();
            console.log(`   Current frequency: ${freq} Hz (${(freq / 1000000).toFixed(3)} MHz)`);
        } catch (error) {
            console.log(`   Error: ${error.message}`);
        }
        console.log('');
        
        // Test 2: Get current mode
        console.log('🎛️  Test 2: Get current mode');
        try {
            const mode = networkRig.getMode();
            console.log(`   Current mode:`, mode);
        } catch (error) {
            console.log(`   Error: ${error.message}`);
        }
        console.log('');
        
        // Test 3: Get VFO
        console.log('📡 Test 3: Get current VFO');
        try {
            const vfo = networkRig.getVfo();
            console.log(`   Current VFO: ${vfo}`);
        } catch (error) {
            console.log(`   Error: ${error.message}`);
        }
        console.log('');
        
        // Test 4: Get signal strength
        console.log('📶 Test 4: Get signal strength');
        try {
            const strength = networkRig.getStrength();
            console.log(`   Signal strength: ${strength}`);
        } catch (error) {
            console.log(`   Error: ${error.message}`);
        }
        console.log('');
        
        // Test 5: Set frequency
        console.log('⚙️  Test 5: Set frequency to 14.205 MHz');
        try {
            const setFreqResult = networkRig.setFrequency(14205000);
            console.log(`   Set frequency result: ${setFreqResult}`);
            
            await delay(1000); // Wait a bit
            
            const newFreq = networkRig.getFrequency();
            console.log(`   Verified frequency: ${newFreq} Hz (${(newFreq / 1000000).toFixed(3)} MHz)`);
        } catch (error) {
            console.log(`   Error: ${error.message}`);
        }
        console.log('');
        
        // Test 7: PTT control (be careful with this!)
        console.log('🎙️  Test 7: PTT control test');
        try {
            console.log('   Setting PTT ON for 1 second...');
            const pttOnResult = networkRig.setPtt(true);
            console.log(`   PTT ON result: ${pttOnResult}`);
            
            await delay(5000); // Keep PTT on for 1 second
            
            console.log('   Setting PTT OFF...');
            const pttOffResult = networkRig.setPtt(false);
            console.log(`   PTT OFF result: ${pttOffResult}`);
        } catch (error) {
            console.log(`   Error: ${error.message}`);
        }
        console.log('');
        
        // Test 8: VFO switching (if supported)
        console.log('🔄 Test 8: VFO switching');
        try {
            const setVfoResult = networkRig.setVfo('VFO-A');
            console.log(`   Set VFO-A result: ${setVfoResult}`);
        } catch (error) {
            console.log(`   Error: ${error.message}`);
        }
        console.log('');
        
        // Close connection
        console.log('🔒 Closing network connection...');
        const closeResult = networkRig.close();
        console.log(`Close result: ${closeResult}`);
        
        console.log('✅ Network test completed successfully!\n');
        
    } catch (error) {
        console.error('❌ Network test failed:', error.message);
        console.error('Stack trace:', error.stack);
    }
}

async function testSerialVsNetwork() {
    console.log('🔄 Comparing Serial vs Network connections...\n');
    
    try {
        // Create serial connection instance
        console.log('📡 Creating serial connection instance...');
        const serialRig = new nodeham.HamLib(1035, '/dev/ttyUSB0');
        console.log('Serial connection info:');
        console.log(serialRig.getConnectionInfo());
        console.log('');
        
        // Create network connection instance
        console.log('🌐 Creating network connection instance...');
        const networkRig = new nodeham.HamLib(1035, 'localhost:5001');
        console.log('Network connection info:');
        console.log(networkRig.getConnectionInfo());
        console.log('');
        
        console.log('📊 Comparison complete!');
        
    } catch (error) {
        console.error('❌ Comparison test failed:', error.message);
    }
}

async function main() {
    console.log('Starting NodeHamLib network tests...\n');
    
    // Test 1: Compare connection types
    await testSerialVsNetwork();
    
    console.log(''.padEnd(60, '=') + '\n');
    
    // Test 2: Actual network connection test
    await testNetworkConnection();
    
    console.log('🎉 All tests completed!\n');
    console.log('💡 Tips:');
    console.log('   - To start rigctld: rigctld -m 1035 -r /dev/ttyUSB0 -t 5001');
    console.log('   - To test different models, change the first parameter');
    console.log('   - Network addresses: "localhost:4532", "192.168.1.100:4532", etc.');
    console.log('   - Serial ports: "/dev/ttyUSB0", "/dev/cu.usbserial-1420", "COM1", etc.');
}

// Run the tests
main().catch(console.error); 