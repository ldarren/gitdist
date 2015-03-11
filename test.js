var gitdist = require('./build/Release/gitdist')
console.log(gitdist.init({dir:'./repo',quiet:1, bare:1}))
