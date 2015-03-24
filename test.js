#!/usr/bin/env node

var gitdist = require('./build/Release/gitdist')

gitdist.init('repo1', {shared:'false'}, function(err, repo){
    if (err) return console.error(err)
    repo.pull() 
    repo.free()
})

function cred(){
}

function progress(){
}

function transfer(){
}

gitdist.clone('https://github.com/ldarren/gitdist.git', 'repo2', function(err, repo){
//gitdist.clone('git@github.com:ldarren/gitdist.git', 'repo2', {credentials:cred, transfer:transfer, progress:progress}, function(err, repo){
    if (err) return console.error(err)
    repo.pull() 
    repo.free()
})

gitdist.open('repo2', function(err, repo){
    if (err) return console.error(err)
    repo.pull() 
    repo.free()
})
