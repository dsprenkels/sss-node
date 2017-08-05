#!/usr/bin/env node

/*
 * Web API for creating and combining SSS shares
 *
 * Author: Daan Sprenkels <hello@dsprenkels.com>
 *
 * This module contains a proof of concept UI for splitting secrets and
 * restoring them afterwards. The respresention of the shares is plain hex.
 *
 * WARNING! **This code will leak the message length by timing oracle!**
 * The code in this module is not intended to be side-channel resistant, so do
 * not just use it if this is a requirement for you.
 *
 * Have a nice day. :)
 */


"use strict";


const bodyParser = require('body-parser');
const cors = require('cors')
const app = require('express')();
const morgan = require('morgan');
const sss = require('shamirsecretsharing');


function pad(dataBuf) {
  // Start padding with 0x80 character
  dataBuf = Buffer.concat([dataBuf, new Buffer([0x80])]);
  while (dataBuf.length < 64) {
    // Add zeros to padding
    dataBuf = Buffer.concat([dataBuf, new Buffer([0])]);
  }
  return dataBuf;
}

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


function unpad(dataBuf) {
  while (dataBuf[dataBuf.length-1] != 0x80) {
    dataBuf = dataBuf.slice(0, -1);
  }
  dataBuf = dataBuf.slice(0, -1);
  return dataBuf;
}


// Setup logging
app.use(morgan('short'))

// Setup body-parser
app.use(bodyParser.urlencoded({ extended: true }));

// Allow cross-origin requests
app.use(cors());

// -- REQUEST HANDLERS --

app.get('/', (req, res) => {
  res.sendFile(__dirname + '/index.html');
});


app.get('/help', (req, res) => {
  res.set('Content-Type', 'text/plain');
  res.send('Use `POST /split` with data `data=[Text]&count=[Int]&threshold=[Int]` or ' +
           '`POST /combine` with data `share=[Share]&share=[Share]&...`');
});


app.post('/split', (req, res) => {
  res.set('Content-Type', 'text/plain');
  if (!req.body.data) {
    res.status(400);
    res.send("No `data` field supplied");
    return;
  }
  if (!req.body.count) {
    res.status(400);
    res.send("No `count` field supplied");
    return;
  }
  if (!req.body.threshold) {
    res.status(400);
    res.send("No `threshold` field supplied");
    return;
  }

  // Pad the input data (exposes timing oracle)
  let dataBuf = new Buffer.from(req.body.data, 'utf8');
  if (dataBuf.length > 63) {
      res.send("Error: text too long (max 63 bytes)");
  }
  dataBuf = pad(dataBuf);

  let count = parseInt(req.body.count);
  let threshold = parseInt(req.body.threshold);
  console.log(1);
  sss.createShares(dataBuf, count, threshold)
    .then((shares) => {
      console.log("1/2");
      let lines = []
      for (let i = 0; i < shares.length; i++) {
        lines.push(shares[i].toString("hex"));
      }
      res.send(lines.join("\n"));
      res.end();
    }).catch((e) => {
      console.log("2/2");
      res.send(e.message);
      res.end();
      return;
    });
});


app.post('/combine', (req, res) => {
  res.set('Content-Type', 'text/plain');

  let shares = req.body.share;
  let shareBufs = [];
  for (let i = 0; i < shares.length; i++) {
    let share = shares[i];
    try {
      shareBufs.push(new Buffer(share, "hex"));
    } catch (e) {
      res.send(e.message + ": " + share);
      return;
    }
  }
  let p = sss.combineShares(removeDuplicates(shareBufs));
  p.then((dataBuf) => {
    dataBuf = unpad(dataBuf);
    res.send(dataBuf.toString('utf8'));
  }).catch((err) => {
    res.send(err.toString());
  });
});


app.listen(3000, function () {
  console.log('SSS web-ui listening on port 3000.')
})
