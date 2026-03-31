import { createRequire } from 'module';

const require = createRequire(import.meta.url);
const { SpectrumController } = require('./spectrum.js');

export { SpectrumController };
export default { SpectrumController };
