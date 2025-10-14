#!/usr/bin/env node
// Patch prebuildify on Windows to spawn node-gyp via cmd.exe to avoid EINVAL
const fs = require('fs');
const path = require('path');

function main() {
  if (process.platform !== 'win32') {
    console.log('Not Windows, skipping prebuildify patch');
    return;
    }
  const file = path.join(process.cwd(), 'node_modules', 'prebuildify', 'index.js');
  if (!fs.existsSync(file)) {
    console.error('prebuildify index.js not found at', file);
    process.exit(0);
  }
  let src = fs.readFileSync(file, 'utf8');
  const needle = "var child = proc.spawn(opts.nodeGyp, args, {";
  if (!src.includes(needle)) {
    console.log('Spawn site not found or already patched');
    return;
  }
  const replacement = [
    "var child = (process.platform === 'win32') ?",
    "  proc.spawn(process.env.ComSpec || 'cmd.exe', ['/d','/s','/c', opts.nodeGyp + ' ' + args.join(' ')], {",
    "    cwd: opts.cwd,",
    "    env: opts.env,",
    "    stdio: opts.quiet ? 'ignore' : 'inherit'",
    "  }) :",
    "  proc.spawn(opts.nodeGyp, args, {"
  ].join('\n');
  src = src.replace(needle, replacement);
  fs.writeFileSync(file, src, 'utf8');
  console.log('Patched prebuildify to spawn via cmd.exe on Windows');
}

main();
