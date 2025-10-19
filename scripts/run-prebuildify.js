#!/usr/bin/env node
// Windows-safe prebuildify runner that wraps child_process.spawn for node-gyp
const os = require('os');
const path = require('path');
const cp = require('child_process');

const origSpawn = cp.spawn;
cp.spawn = function patchedSpawn(command, args, options) {
  if (process.platform === 'win32') {
    // If prebuildify tries to spawn a .cmd (node-gyp.cmd), wrap via cmd.exe
    const isCmd = /\.cmd$/i.test(command) || /node-gyp(\.js)?$/i.test(command);
    if (isCmd) {
      const cmdExe = process.env.ComSpec || 'cmd.exe';
      const cmdline = [command].concat(args || []).join(' ');
      return origSpawn(cmdExe, ['/d', '/s', '/c', cmdline], Object.assign({ stdio: 'inherit' }, options, { shell: false }));
    }
  }
  return origSpawn(command, args, options);
};

const prebuildify = require('prebuildify');

const opts = {
  napi: true,
  strip: true,
  cwd: process.cwd(),
  arch: process.env.PREBUILD_ARCH || os.arch(),
  platform: process.env.PREBUILD_PLATFORM || os.platform(),
  nodeGyp: process.env.PREBUILD_NODE_GYP || path.join(process.cwd(), 'node_modules', '.bin', process.platform === 'win32' ? 'node-gyp.cmd' : 'node-gyp')
};

prebuildify(opts, (err) => {
  if (err) {
    console.error(err.stack || err.message || String(err));
    process.exit(1);
  }
  console.log('prebuildify completed successfully');
});

