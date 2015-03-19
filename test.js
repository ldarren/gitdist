#!/usr/bin/env node

var gitdist = require('./build/Release/gitdist')
console.log(gitdist.init('repo1', {shared:'false'}))
//console.log(gitdist.lastError())
