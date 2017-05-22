#!/usr/bin/env node

const sss = require("./lib");

const data = Buffer.alloc(64, 0x42);
const n = 200;
const k = 100;

let p = sss.createShares(data, n, k);
p.then((x) => {
    for (let idx = 0; idx < n; ++idx) console.log(x[idx]);
    process.exit();
});
