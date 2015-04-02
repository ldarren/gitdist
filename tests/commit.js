#!/usr/bin/env node

var
fs = require('fs'),
p = require('path'),
gitdist = require('../build/Release/gitdist'),
common = require('./common'),
rname = process.argv[2] || 'repo'

gitdist.open(rname, function(err, repo){
    if (err) return console.error(err)
    fs.writeFileSync(p.resolve(__dirname, rname+'/test.txt'), ''+Date.now())
    repo.commit('test.txt', 'comment '+Date.now(), function(err){
        if (err) return console.error(err)
        repo.push(function(err){
            if (err) return console.error(err)
            repo.fetch(function(err){
                if (err) return console.error(err)
                repo.free()
            })
        })
    }) 
})
