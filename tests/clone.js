#!/usr/bin/env node

var
p = require('path'),
gitdist = require('../build/Release/gitdist'),
common = require('./common'),
rname = process.argv[2] || 'repo'

common.rmrf(p.resolve(__dirname, rname))

function cred(){
}

function progress(){
}

function transfer(){
}

//gitdist.clone('https://github.com/ldarren/gitdist.git', 'repo', function(err, repo){
gitdist.clone('git@github.com:ldarren/gitdist_test.git', rname, {credentials:cred, transfer:transfer, progress:progress}, function(err, repo){
    if (err) return console.error(err)
    repo.free()
})
