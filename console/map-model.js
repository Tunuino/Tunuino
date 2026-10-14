// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
export const rpmAxis = Object.freeze([500, 750, 1000, 1250, 1500, 2000, 2500, 3000, 3500, 4000, 4500, 5000, 5500, 6000, 7000, 8000]);
export const loadAxis = Object.freeze([20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 140, 160, 180, 220, 250]);
export function makeDemoMap(type = "ve") {
  if (type !== "ve" && type !== "ignition") throw new RangeError("Unknown map");
  return loadAxis.map((load, row) => rpmAxis.map((rpm, col) => type === "ve"
    ? Math.min(110, Math.round(36 + row * 9 + col * 3))
    : Math.max(4, Math.round(32 - row * 4 + col * 1.5))));
}
export function setCell(map, row, col, value, type = "ve") {
  if (!Array.isArray(map) || !Array.isArray(map[row]) || !Number.isInteger(row) ||
      !Number.isInteger(col) || col < 0 || col >= map[row].length) throw new RangeError("Invalid cell");
  const numeric = Number(value);
  const min = type === "ve" ? 0 : -20;
  const max = type === "ve" ? 200 : 80;
  if (!Number.isFinite(numeric) || numeric < min || numeric > max) throw new RangeError("Invalid value");
  map[row][col] = Math.round(numeric * 10) / 10;
}
export function mapToCsv(map) {
  return [["kPa / RPM", ...rpmAxis].join(","), ...map.map((row, i) =>
    [loadAxis[i], ...row].join(","))].join("\r\n") + "\r\n";
}
