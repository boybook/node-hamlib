import { createRequire } from 'module';
import { fileURLToPath } from 'url';
import path from 'path';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const require = createRequire(import.meta.url);

// Import the CommonJS module
const { HamLib, Rotator, PASSBAND } = require('./index.js');

// Export for ES modules
export { HamLib, Rotator, PASSBAND };
export default { HamLib, Rotator, PASSBAND }; 
