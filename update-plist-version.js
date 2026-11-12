const fs = require('fs');

var appversion = require('./package.json').version;
// CFBundleVersion is an independent build number maintained in Info.plist.

var outputString = '';
var foundCFBundleShortVersionString = false;

function getPosition(string, subString, index) {
  return string.split(subString, index).join(subString).length;
}

require('fs').readFileSync('./QtProject/release-build/macos/Info.plist', 'utf-8').split(/\r?\n/).forEach(function(line){
    if(foundCFBundleShortVersionString == true){
        var beforeString = line.slice(0, getPosition(line, '>', 1) + 1);
        var afterString =line.slice(getPosition(line, '<', 2));
        line = beforeString + appversion + afterString;
        foundCFBundleShortVersionString = false;
    }
    if(line.includes('CFBundleShortVersionString')){
        foundCFBundleShortVersionString = true;
    }

    outputString += line + "\n";
    })

outputString = outputString.substring(0, outputString.length -1); //remove last new line
fs.writeFile('./QtProject/release-build/macos/Info.plist', outputString, 'utf-8', function (err) {
    if (err) return console.log(err);
    console.log('Wrote updated Plist file');
});
