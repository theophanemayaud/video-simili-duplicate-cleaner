const fs = require('fs');

const appVersion = require('./package.json').version;
const plistPath = './QtProject/release-build/macos/Info.plist';
const plist = fs.readFileSync(plistPath, 'utf8');

function versionEntry(key) {
  const pattern = new RegExp(`(<key>${key}</key>\\s*<string>)([^<]*)(</string>)`);
  if (!pattern.test(plist))
    throw new Error(`Missing ${key} string in ${plistPath}`);
  return pattern;
}

const buildEntry = versionEntry('CFBundleVersion');
const publicVersionEntry = versionEntry('CFBundleShortVersionString');

// The public version follows package.json. Apple compares macOS build numbers
// across all public versions, so increment a global counter for each build.
// An explicit number also permits reproducing a build without incrementing it.
const buildNumber = process.env.MACOS_BUILD_NUMBER
  ?? String(Number(plist.match(buildEntry)[2]) + 1);
if (!/^[1-9][0-9]*$/.test(buildNumber) || !Number.isSafeInteger(Number(buildNumber)))
  throw new Error('MACOS_BUILD_NUMBER must be a positive safe integer');

const output = plist
  .replace(buildEntry, (_, before, previous, after) => before + buildNumber + after)
  .replace(publicVersionEntry, (_, before, previous, after) => before + appVersion + after);
fs.writeFileSync(plistPath, output, 'utf8');
console.log(`macOS version ${appVersion}, build ${buildNumber}`);
