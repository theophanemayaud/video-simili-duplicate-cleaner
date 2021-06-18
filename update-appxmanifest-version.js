const fs = require('fs');
const readline = require('readline');

var appversion = require('./package.json').version;

var outputString = '';
var foundIdentity = false;

function getPosition(string, subString, index) {
  return string.split(subString, index).join(subString).length;
}

require('fs').readFileSync('./QtProject/release-build/windows/appxmanifest.xml', 'utf-8').split(/\r?\n/).forEach(function(line){
    if( foundIdentity == true){
            Version="1.3.0.0" 
        var beforeString = line.slice(0, getPosition(line, '"', 1) + 1);
        var afterString = line.slice(getPosition(line, '"', 2));
        line = beforeString + appversion + '.0' + afterString;
        foundIdentity = false;
    }

    if(line.includes('Identity')){
        foundIdentity = true;
    }

    outputString += line + "\n";
    })

outputString = outputString.substring(0, outputString.length -1); //remove last new line
fs.writeFile('./QtProject/release-build/windows/appxmanifest.xml', outputString, 'utf-8', function (err) {
    if (err) return console.log(err);
    console.log('Wrote updated appxmanifest file');
});
