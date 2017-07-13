# Shamir secret sharing for Node.js

[![Build Status](https://travis-ci.org/dsprenkels/sss-node.svg?branch=master)](https://travis-ci.org/dsprenkels/sss-node)

This repository contains a Node.js binding for my general-purpose [Shamir secret
sharing library][sss]. This library allows users to split secret data into
a number of different _shares_. With the possession of some or all of these
shares, the original secret can be restored. It's essentially a _double-key_
system, but then cryptographically.

An example use case is a beer brewery which has a vault which contains their
precious super secret recipe. The 5 board members of this brewery do not trust
all the others well enough that they won't secretly break into the vault and
sell the recipe to a competitor. So they split the code into 5 shares, and
allow 4 shares to restore the original code. Now they are sure that the
majority of the staff will know when the vault is opened, but they can still
open the vault when one of the staff members is abroad or sick at home.


## Installation

Install the library using npm:

```shell
npm install --save dsprenkels/sss-node
```

## Usage

```javascript
// Import the sss library
const sss = require("shamirsecretsharing");

// Create a buffer for the data that will be shared (must be 64 bytes long)
const data = Buffer.alloc(64, 0x42);
const amount = 5;
const theshold = 4;

// Creating 5 shares (need 3 to restore)
let sharesPromise = sss.createShares(data, amount, theshold);

// Write the shares to the screen
sharesPromise.then((x) => {
    console.log(x);
    return x;
});

// For demonstrational purpose, lose one of the shares
let newSharesPromise = sharesPromise.then((x) => {
    return [x[3], x[2], x[4], x[0]];
});

// Restore the original secret
let restoredPromise = newSharesPromise.then((x) => {
    return sss.combineShares(x);
});

// Dump the original secret back to the screen
let main = restoredPromise.then((x) => {
    console.log(x);
}).catch((x) => {
    console.log("Error: " + x);
});
```

## Technical details

As you may have noticed from the example above, all the functions in this
library return a [Promise] Object. Every call to this library will spin off a
job on a background thread, allowing the main thread to keep on running other
stuff.

This design choice does not mean that this library is slow. On the contrary,
sharing and recombining a secret using this library is very fast. But Node.js
is an asynchronous platform after all, so we should use it in that way.
Furthermore, in the case that the OS's entropy pool is not yet filled,
`CreateShares` will wait for the entropy pool to be sufficiently large, which
may take some time (seconds).

## Questions

Feel free to send me an email on my GitHub associated e-mail address.

[sss]: https://github.com/dsprenkels/sss
[Promise]: https://developer.mozilla.org/en/docs/Web/JavaScript/Reference/Global_Objects/Promise
