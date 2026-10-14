// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
import {test} from "node:test";
import {strict as assert} from "node:assert";
import {crc32, decodeTuneSnapshot, TuneSnapshotAssembler, TUNE_SIZE} from "../tune-snapshot.js";
function fixture() {
  const b = new Uint8Array(TUNE_SIZE);
  const v = new DataView(b.buffer);
  b.set([84,85,78,69]); v.setUint16(4,2,true); v.setUint16(6,TUNE_SIZE,true); v.setUint32(8,7,true);
  let at = 12;
  for (let t = 0; t < 2; t++) {
    for (let i = 0; i < 16; i++) {v.setUint16(at,1000+i*500,true);at+=2;}
    for (let i = 0; i < 16; i++) {v.setUint16(at,200+i*100,true);at+=2;}
    for (let i = 0; i < 256; i++) {v.setInt16(at,t ? -100 : 850,true);at+=2;}
  }
  assert.equal(at,TUNE_SIZE-4);
  v.setUint32(at,crc32(b.subarray(0,at)),true);
  return b;
}
test("reassemble out of order chunks and validate 16x16 maps", () => {
  const bytes=fixture(), assembler=new TuneSnapshotAssembler(), chunks=[];
  for(let offset=0;offset<TUNE_SIZE;offset+=32) {
    const n=Math.min(32,TUNE_SIZE-offset);
    const p=new Uint8Array(n+3);
    p[0]=offset&255;p[1]=offset>>>8;p[2]=n;p.set(bytes.subarray(offset,offset+n),3);
    chunks.push(p);
  }
  assert.throws(()=>assembler.finish(),/Incomplete/);
  for(const p of chunks.reverse()) assembler.addChunk(p);
  const result=assembler.finish();
  assert.equal(result.revision,7);
  assert.equal(result.ve.cells.length,16);
  assert.equal(result.ve.cells[0].length,16);
  assert.equal(result.ve.cells[0][0],85);
  assert.equal(result.ignition.cells[15][15],-10);
});
test("reject corrupted and conflicting snapshots",()=>{
  const b=fixture();b[40]^=1;
  assert.throws(()=>decodeTuneSnapshot(b),/CRC32/);
  const a=new TuneSnapshotAssembler();
  a.addChunk(Uint8Array.of(0,0,1,4));
  assert.throws(()=>a.addChunk(Uint8Array.of(0,0,1,5)),/Conflicting/);
  assert.throws(()=>a.addChunk(Uint8Array.of(0xff,0x04,2,0,0)),/range/);
});
