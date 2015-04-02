var
fs = require('fs'),
p = require('path')
rmrf = function(path) {
  if( fs.existsSync(path) ) {
    fs.readdirSync(path).forEach(function(file,index){
      var curPath = path + p.sep + file
      if(fs.lstatSync(curPath).isDirectory()) { // recurse
        rmrf(curPath)
      } else { // delete file
        fs.unlinkSync(curPath)
      }
    })
    fs.rmdirSync(path)
  }
}

module.exports = {
    rmrf: rmrf
}
