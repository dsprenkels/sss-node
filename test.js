#!/usr/bin/env node

const sss = require("./lib");
const jsc = require("jsverify");
const shuffle = require('shuffle-array');

const data = Buffer.alloc(64, 0x42);
const key = data.slice(32);


let standardOperation = jsc.forall(jsc.integer(1, 255), jsc.integer(1, 255),
                                   jsc.integer(1, 255), (n, k, k2) => {
  // We can't really test anything if not n is not larger than k and k2
  if (k > n || k2 > n) return true;
  let sharesP = sss.createShares(data, n, k);
  let restoredP = sharesP.then(shares => {
    let newShares = shuffle(shares).slice(0, k2);
    return sss.combineShares(newShares);
  });
  return restoredP.then((restored) => {
    return (k2 >= k) && restored.equals(data);
  }).catch((error) => {
    return k2 < k;
  });
});

let hazmatOperation = jsc.forall(jsc.integer(1, 255), jsc.integer(1, 255),
                                 jsc.integer(1, 255), (n, k, k2) => {
  // Unable to test if `k` or `k2` is larger than `n`
  if (k > n || k2 > n) return true;
  let keysharesP = sss.createKeyshares(key, n, k);
  let restoredP = keysharesP.then((keyshares) => {
    let newKeyshares = shuffle(keyshares).slice(0, k2);
    return sss.combineKeyshares(newKeyshares);
  });
  return restoredP.then((restored) => {
    return (k2 >= k) === restored.equals(key);
  });
});

jsc.check(standardOperation, { tests: 1000 });
jsc.check(hazmatOperation, { tests: 1000 });
