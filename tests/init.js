#!/usr/bin/env node

var
p = require('path'),
gitdist = require('../build/Release/gitdist'),
common = require('./common'),
rname = process.argv[2] || 'repo'

common.rmrf(p.resolve(__dirname, rname))

gitdist.init(rname, {shared:'false'}, function(err, repo){
    if (err) return console.error(err)
    repo.free()
})
