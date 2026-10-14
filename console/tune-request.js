// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
import {encodeFrame} from "./protocol.js";
import {TUNE_SIZE, TUNE_CHUNK_MAX} from "./tune-snapshot.js";
export const TYPE_TUNE_REQUEST = 5;
// Read-only requests; transport writes must be explicitly invoked by the user.
export function makeTuneReadRequest(offset, count = TUNE_CHUNK_MAX) {
  if (!Number.isInteger(offset) || !Number.isInteger(count) ||
      offset < 0 || offset >= TUNE_SIZE ||
      count < 1 || count > TUNE_CHUNK_MAX || offset + count > TUNE_SIZE)
    throw new RangeError("Invalid tune request");
  return encodeFrame(TYPE_TUNE_REQUEST, Uint8Array.of(offset & 255, offset >>> 8, count));
}
export function allTuneReadRequests() {
  const frames = [];
  for (let offset = 0; offset < TUNE_SIZE; offset += TUNE_CHUNK_MAX)
    frames.push(makeTuneReadRequest(offset, Math.min(TUNE_CHUNK_MAX, TUNE_SIZE - offset)));
  return frames;
}
