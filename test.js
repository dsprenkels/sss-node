#!/usr/bin/env node

const sss = require("./lib");

const data = Buffer.alloc(64, 0x42);
const n = 200;
const k = 100;

let p = sss.createShares(data, n, k)
    .then((x) => {
        return sss.combineShares(x);
    })
    .then((x) => {
        console.log(x);
        process.exit();
    });
