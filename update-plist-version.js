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

// ISO weeks belong to the year containing their Thursday. Use UTC so builds
// agree across machines, including around New Year.
const date = new Date();
date.setUTCHours(0, 0, 0, 0);
date.setUTCDate(date.getUTCDate() + 4 - (date.getUTCDay() || 7));
const year = date.getUTCFullYear();
const week = Math.ceil((date - Date.UTC(year, 0, 1) + 86400000) / 604800000);

// The public version follows package.json; macOS builds use YYYY.WW.N.
// Increment within the same week, otherwise start at 1. An explicit value
// permits reproducing a build without incrementing it.
let buildNumber = process.env.MACOS_BUILD_NUMBER;
if (buildNumber === undefined) {
  const [previousYear, previousWeek, previousBuild] = plist.match(buildEntry)[2].split('.').map(Number);
  // Apple requires increasing macOS build versions across public releases.
  if (previousYear > year || (previousYear === year && previousWeek > week))
    throw new Error('The previous macOS build is dated in the future; check the clock or set MACOS_BUILD_NUMBER');
  const build = previousYear === year && previousWeek === week ? previousBuild + 1 : 1;
  buildNumber = `${year}.${String(week).padStart(2, '0')}.${build}`;
}
if (!/^[1-9][0-9]{3}\.(?:0[1-9]|[1-4][0-9]|5[0-3])\.[1-9][0-9]?$/.test(buildNumber))
  throw new Error('MACOS_BUILD_NUMBER must use YYYY.WW.N (ISO week 01-53, build 1-99)');

const output = plist
  .replace(buildEntry, (_, before, previous, after) => before + buildNumber + after)
  .replace(publicVersionEntry, (_, before, previous, after) => before + appVersion + after);
fs.writeFileSync(plistPath, output, 'utf8');
console.log(`macOS version ${appVersion}, build ${buildNumber}`);
