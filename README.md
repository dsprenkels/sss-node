# Shamir secret sharing for Node.js

This repository contains a Node.js binding for my general-purpose [Shamir secret
sharing library][sss]. This library allow a user to split secret data into
a number of different _shares_. With the posession of some or all of these
shares, the original secret can be restored. It's basiscly a _double-key_
system, but then cryptographically.

An example use case is a beer brewery which has a vault which conains their
precious super secret recipe. The 5 board members of this brewery do not trust
all the others well enough that they won't secretly break into the vault and
sell the recipe to a competitor. So they split the code into 5 shares, and
allow 3 shares to restore the original code. Now they are sure that the
majority of the staff will know when the vault is opened, but they also don't
need _all_ the board members to be present if they wany to open the vault.


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
const theshold = 3;

// Creating 5 shares (need 3 to restore)
let sharesPromise = sss.createShares(data, amount, theshold);

// Write the shares to the screen
sharesPromise.then((x) => {
    console.log(x);
    return x;
});

// For demonstrational purpose, lose some of the shares
let newSharesPromise = sharesPromise.then((x) => {
    return [x[3], x[2], x[0]];
});

// Restore the original secret
let restoredPromise = newSharesPromise.then((x) => {
    return sss.combineShares(x);
});

// Dump the original secret back to the screen
restoredPromise.then((x) => {
    console.log(x);
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

Note: The Node.js bindings use a different random source than the original
sss library, namely the builtin Node.js random generator rather than calling
the OS directly. This is done to make these bindings as portable as possible.
The Node.js runtime calls OpenSSL for its cryptographic functions, so it can be
considered secure.

## Questions

Feel free to send me an email on my GitHub associated e-mail address.

[sss]: https://github.com/dsprenkels/sss
[Promise]: https://developer.mozilla.org/en/docs/Web/JavaScript/Reference/Global_Objects/Promise
