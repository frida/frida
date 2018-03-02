'use strict';

const fs = require('fs');
const gadget = require('.');
const _glob = require('glob');
const _simpleGet = require('simple-get');
const path = require('path');
const _pump = require('pump');
const util = require('util');

const access = util.promisify(fs.access);
const glob = util.promisify(_glob);
const pump = util.promisify(_pump);
const rename = util.promisify(fs.rename);
const simpleGet = util.promisify(_simpleGet);
const unlink = util.promisify(fs.unlink);

async function run() {
  await pruneOldVersions();

  if (await alreadyDownloaded())
    return;

  await download();
}

async function alreadyDownloaded() {
  try {
    await access(gadget.path, fs.constants.F_OK);
    return true;
  } catch (e) {
    return false;
  }
}

async function download() {
  const response = await simpleGet({
    url: `https://github.com/frida/frida/releases/download/${gadget.version}/frida-gadget-${gadget.version}-ios-universal.dylib.xz`
  });
  if (response.statusCode !== 200) {
    throw new Error(`Unable to download: ${response.statusMessage} (status code: ${response.statusCode})`);
  }

  const tempGadgetPath = gadget.path + '.download';
  const tempGadgetStream = fs.createWriteStream(tempGadgetPath);
  await pump(response, tempGadgetStream);

  await rename(tempGadgetPath, gadget.path);
}

async function pruneOldVersions() {
  const currentLib = gadget.path;
  const libs = await glob(path.join(path.dirname(currentLib), '*.dylib'), {});
  const obsoleteLibs = libs.filter(lib => lib !== currentLib);
  for (const lib of obsoleteLibs) {
    await unlink(lib);
  }
}

run().catch(onError);

function onError(error) {
  console.error(error.message);
  process.exitCode = 1;
}
