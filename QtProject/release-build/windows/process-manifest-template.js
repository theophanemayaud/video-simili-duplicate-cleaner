const fs = require('fs');
const path = require('path');

function processManifestTemplate(templateFile, outputFile, architecture) {
  // Read version from root package.json
  const rootPackageJson = JSON.parse(fs.readFileSync(path.join(__dirname, '../../../package.json'), 'utf-8'));
  const version = rootPackageJson.version + '.0';
  
  let content = fs.readFileSync(templateFile, 'utf-8');
  
  // Replace the placeholders
  content = content.replace(/{{VERSION}}/g, version);
  content = content.replace(/{{ARCHITECTURE}}/g, architecture);
  
  fs.writeFileSync(outputFile, content, 'utf-8');
  console.log(`Processed manifest template: version=${version}, architecture=${architecture} -> ${outputFile}`);
}

// Get the arguments from command line
const architecture = process.argv[2];
const templateFile = process.argv[3];
const outputFile = process.argv[4];

if (!architecture || !templateFile || !outputFile) {
  console.error('Usage: node process-manifest-template.js <architecture> <template> <output>');
  console.error('Example: node process-manifest-template.js arm64 appxmanifest.template.xml final/appxmanifest.xml');
  process.exit(1);
}

processManifestTemplate(templateFile, outputFile, architecture); 