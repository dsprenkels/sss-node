#!/usr/bin/env node

const crypto = require("crypto");
const _sss = require("./build/Release/shamirsecretsharing");


exports.createShares = function createShares(data, n, k) {
  return new Promise((resolve) => {
    // Use node.js native random source for a key
    crypto.randomBytes(32, (err, random) => {
      if (err) throw err;
      _sss.createShares(data, n, k, random, resolve);
    });
  });
}
