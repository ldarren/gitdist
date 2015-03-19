#!/usr/bin/env node

var gitdist = require('./build/Release/gitdist')

gitdist.init('repo1', {shared:'false'}, function(err, repo){
    if (err) return console.error(err)
    repo.pull() 
    repo.free()
})
