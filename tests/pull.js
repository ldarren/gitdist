#!/usr/bin/env node

var
gitdist = require('../build/Release/gitdist'),
common = require('./common'),
rname = process.argv[2] || 'repo'

gitdist.open(rname, function(err, repo){
    if (err) return console.error(err)
    repo.fetch(function(err){
        if (err) return console.error(err)
        repo.merge(function(err){
            if (err) return console.error(err)
            repo.free()
        })
    }) 
})
