const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const { test } = require('node:test');

const project = path.resolve(__dirname, '../..');
const plistRelativePath = 'QtProject/release-build/macos/Info.plist';

function fixture(t) {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'video-simili-version-'));
  t.after(() => fs.rmSync(directory, { recursive: true, force: true }));
  const plistPath = path.join(directory, plistRelativePath);
  fs.mkdirSync(path.dirname(plistPath), { recursive: true });
  fs.copyFileSync(path.join(project, plistRelativePath), plistPath);
  fs.copyFileSync(path.join(project, 'update-plist-version.js'), path.join(directory, 'update-plist-version.js'));
  const setVersion = version => fs.writeFileSync(path.join(directory, 'package.json'), JSON.stringify({ version }));
  setVersion('1.15.2');
  const read = () => fs.readFileSync(plistPath, 'utf8');
  const value = key => read().match(new RegExp(`<key>${key}</key>\\s*<string>([^<]*)</string>`))[1];
  const run = buildNumber => {
    const env = { ...process.env };
    delete env.MACOS_BUILD_NUMBER;
    if (buildNumber !== undefined) env.MACOS_BUILD_NUMBER = buildNumber;
    return spawnSync(process.execPath, ['update-plist-version.js'], { cwd: directory, env, encoding: 'utf8' });
  };
  return { plistPath, setVersion, read, value, run };
}

test('build counter increases across public versions without changing the public version', t => {
  const f = fixture(t);
  const originalBuild = Number(f.value('CFBundleVersion'));
  assert.equal(f.run().status, 0);
  assert.equal(f.value('CFBundleVersion'), String(originalBuild + 1));
  assert.equal(f.value('CFBundleShortVersionString'), '1.15.2');
  f.setVersion('1.16.0');
  assert.equal(f.run().status, 0);
  assert.equal(f.value('CFBundleVersion'), String(originalBuild + 2));
  assert.equal(f.value('CFBundleShortVersionString'), '1.16.0');
});

test('an explicit build can be reproduced, then automatic numbering resumes', t => {
  const f = fixture(t);
  for (let i = 0; i < 2; ++i) {
    assert.equal(f.run('17').status, 0);
    assert.equal(f.value('CFBundleVersion'), '17');
    assert.equal(f.value('CFBundleShortVersionString'), '1.15.2');
  }
  assert.equal(f.run().status, 0);
  assert.equal(f.value('CFBundleVersion'), '18');
});

test('invalid build overrides fail without modifying the plist', t => {
  const f = fixture(t);
  const before = f.read();
  for (const invalid of ['', '0', '-1', 'abc', '1.15.2.1', '9007199254740992']) {
    assert.notEqual(f.run(invalid).status, 0, invalid);
    assert.equal(f.read(), before);
  }
});

test('a missing version entry fails without partially updating the plist', t => {
  const f = fixture(t);
  fs.writeFileSync(f.plistPath, f.read().replace('CFBundleShortVersionString', 'MissingVersion'));
  const before = f.read();
  assert.notEqual(f.run().status, 0);
  assert.equal(f.read(), before);
});
