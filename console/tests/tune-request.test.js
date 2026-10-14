// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
import {test} from "node:test";
import {strict as assert} from "node:assert";
import {allTuneReadRequests,makeTuneReadRequest} from "../tune-request.js";
import {decodeFrame} from "../protocol.js";
test("requests cover entire 16x16 snapshot without overlap",()=>{
  const requests=allTuneReadRequests();
  assert.equal(requests.length,37);
  let offset=0;
  for(const bytes of requests){
    const frame=decodeFrame(bytes);
    assert.equal(frame.type,5);
    assert.equal(frame.payload[0]+256*frame.payload[1],offset);
    offset+=frame.payload[2];
  }
  assert.equal(offset,1168);
  assert.throws(()=>makeTuneReadRequest(1167,2),RangeError);
  assert.throws(()=>makeTuneReadRequest(0,33),RangeError);
});
