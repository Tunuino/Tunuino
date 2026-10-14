// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
// Tracks receipt freshness, not sensor measurement age.
export class TelemetryFreshness {
  constructor(onExpired, timeoutMs = 500) {
    if (typeof onExpired !== "function" || !Number.isFinite(timeoutMs) || timeoutMs <= 0)
      throw new TypeError("Invalid freshness configuration");
    this.onExpired = onExpired;
    this.timeoutMs = timeoutMs;
    this.lastReceipt = null;
    this.expired = true;
  }
  received(nowMs) {
    this.lastReceipt = nowMs;
    this.expired = false;
  }
  check(nowMs) {
    if (this.lastReceipt !== null && !this.expired &&
        nowMs - this.lastReceipt > this.timeoutMs) {
      this.expired = true;
      this.onExpired();
      return true;
    }
    return false;
  }
  reset() {
    this.lastReceipt = null;
    this.expired = true;
  }
}
