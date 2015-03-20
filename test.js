#!/usr/bin/env node

var gitdist = require('./build/Release/gitdist')

gitdist.init('repo1', {shared:'false'}, function(err, repo){
    if (err) return console.error(err)
    repo.pull() 
    repo.free()
})

gitdist.open('repo1', function(err, repo){
    if (err) return console.error(err)
    repo.pull() 
    repo.free()
})

//gitdist.clone('https://github.com/ldarren/gitdist.git', 'repo2', function(err, repo){
gitdist.clone('git@github.com:ldarren/gitdist.git', 'repo2', function(err, repo){
    if (err) return console.error(err)
    repo.pull() 
    repo.free()
})
