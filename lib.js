#!/usr/bin/env node

const crypto = require("crypto");
const _sss = require("./build/Release/shamirsecretsharing");


function removeDuplicates(shares) {
  if (!shares) return shares;
  shares.sort();
  let i = 1;
  while (i < shares.length) {
    if (shares[i].equals(shares[i-1])) {
      shares.splice(i, 1);
    } else {
      i++;
    }
  }
  return shares;
}


exports.createShares = function createShares(data, n, k) {
  return new Promise((resolve) => {
    _sss.createShares(data, n, k, resolve);
  });
};


exports.combineShares = function combineShares(shares) {
  return new Promise((resolve) => {
    _sss.combineShares(removeDuplicates(shares), resolve);
  }).then((x) => {
    if (x === null) {
      throw "InvalidAccessError: invalid or too few shares provided";
    } else {
      return x;
    }
  });
};


exports.createKeyshares = function createKeyshares(key, n, k) {
  return new Promise((resolve) => {
    _sss.createKeyshares(key, n, k, resolve);
  });
}


exports.combineKeyshares = function combineKeyshares(keyshares) {
  return new Promise((resolve) => {
    _sss.combineKeyshares(keyshares, resolve);
  });
}
