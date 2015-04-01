#!/usr/bin/env node

var
fs = require('fs'),
p = require('path'),
gitdist = require('../build/Release/gitdist'),
rmrf = function(path) {
  if( fs.existsSync(path) ) {
    fs.readdirSync(path).forEach(function(file,index){
      var curPath = path + p.sep + file
      if(fs.lstatSync(curPath).isDirectory()) { // recurse
        deleteFolderRecursive(curPath)
      } else { // delete file
        fs.unlinkSync(curPath)
      }
    })
    fs.rmdirSync(path)
  }
}

rmrf(p.resolve(__dirname, 'repo1'))
rmrf(p.resolve(__dirname, 'repo2'))

/*
gitdist.init('repo1', {shared:'false'}, function(err, repo){
    if (err) return console.error(err)
    repo.pull() 
    repo.free()
})
*/
function cred(){
}

function progress(){
}

function transfer(){
}

//gitdist.clone('https://github.com/ldarren/gitdist.git', 'repo2', function(err, repo){
gitdist.clone('git@github.com:ldarren/gitdist_test.git', 'repo2', {credentials:cred, transfer:transfer, progress:progress}, function(err, repo){
    if (err) return console.error(err)
    fs.writeFileSync(p.resolve(__dirname, 'repo2/test.txt'), ''+Date.now())
    repo.commit('test.txt', 'a test comment 1', function(err){
        if (err) return console.error(err)
        repo.push(function(err){
            if (err) return console.error(err)
            repo.free()
        })
    }) 
})
/*
gitdist.open('repo2', function(err, repo){
    if (err) return console.error(err)
    repo.pull() 
    repo.free()
})
*/
