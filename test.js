#!/usr/bin/env node

var gitdist = require('./build/Release/gitdist')
console.log(gitdist.init({dir:'',quiet:1, bare:1}))
console.log(gitdist.lastError())
