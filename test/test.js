#!/usr/bin/env node

var
fs = require('fs'),
gitdist = require('../build/Release/gitdist')

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
    fs.writeFileSync(__dirname+'/repo2/test.txt', fs.readFileSync(__dirname+'/test.txt'))
    repo.commit('test.txt', 'a test comment 1', function(err){
        if (err) return console.error(err)
        repo.free()
    }) 
})
/*
gitdist.open('repo2', function(err, repo){
    if (err) return console.error(err)
    repo.pull() 
    repo.free()
})
*/
