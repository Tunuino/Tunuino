// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
import {rpmAxis, loadAxis, makeDemoMap, setCell, mapToCsv} from "./map-model.js";
import {TelemetryStream} from "./telemetry-stream.js";
const metrics = [
  ["RPM", "rpm", 0, 8000], ["MAP", "kPa", 0, 250],
  ["TPS", "%", 0, 100], ["CLT", "°C", -20, 130],
  ["IAT", "°C", -20, 100], ["AFR", ":1", 8, 22],
  ["Bateria", "V", 0, 18]
];
const gauges = document.querySelector("#gauges");
for (const [name, unit, min, max] of metrics) {
  const el = document.createElement("article");
  el.className = "gauge";
  el.innerHTML = `<div class="name"></div><div class="value">--</div><div class="unit"></div><div class="bar"><div class="fill"></div></div>`;
  el.querySelector(".name").textContent = name;
  el.querySelector(".unit").textContent = unit;
  el.querySelector(".fill").style.width = "0%";
  gauges.append(el);
}
document.querySelectorAll("nav button").forEach(button => button.addEventListener("click", () => {
  document.querySelectorAll("nav button").forEach(b => b.classList.toggle("active", b === button));
  document.querySelectorAll(".page").forEach(p => p.classList.toggle("active", p.id === button.dataset.page));
}));
let currentType = "ve";
let map = makeDemoMap(currentType);
function renderMap() {
  const table = document.querySelector("#map-table");
  table.replaceChildren();
  const header = document.createElement("tr");
  for (const label of ["kPa / RPM", ...rpmAxis]) {
    const th = document.createElement("th"); th.textContent = label; header.append(th);
  }
  table.append(header);
  map.forEach((values, row) => {
    const tr = document.createElement("tr");
    const th = document.createElement("th"); th.textContent = loadAxis[row]; tr.append(th);
    values.forEach((value, col) => {
      const td = document.createElement("td");
      const input = document.createElement("input");
      input.type = "number"; input.step = "0.1"; input.value = String(value);
      input.setAttribute("aria-label", `Carga ${loadAxis[row]} kPa, ${rpmAxis[col]} rpm`);
      input.addEventListener("change", () => {
        try { setCell(map, row, col, input.value, currentType); input.value = String(map[row][col]); }
        catch { input.value = String(map[row][col]); document.querySelector("#map-notice").textContent = "Valor inválido, alteração rejeitada."; }
      });
      td.append(input); tr.append(td);
    });
    table.append(tr);
  });
}
document.querySelector("#map-type").addEventListener("change", e => {
  currentType = e.target.value; map = makeDemoMap(currentType); renderMap();
});
document.querySelector("#reset-map").addEventListener("click", () => {
  map = makeDemoMap(currentType); renderMap();
  document.querySelector("#map-notice").textContent = "Mapa demonstrativo reposto.";
});
document.querySelector("#export-map").addEventListener("click", () => {
  const blob = new Blob([mapToCsv(map)], {type: "text/csv;charset=utf-8"});
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a"); a.href = url; a.download = `tunuino-${currentType}-demo.csv`; a.click();
  URL.revokeObjectURL(url);
});
renderMap();

// Validated telemetry is rendered only when explicitly delivered by a transport.
// No transport is attached in this prototype, so the dashboard remains unavailable.
const telemetryKeys = ["rpm", "map", "tps", "clt", "iat", "afr", "battery"];
export function renderTelemetry(values) {
  const cards = document.querySelectorAll("#gauges .gauge");
  telemetryKeys.forEach((key, index) => {
    const value = values[key];
    if (!Number.isFinite(value)) return;
    cards[index].querySelector(".value").textContent = String(value);
    const min = metrics[index][2], max = metrics[index][3];
    cards[index].querySelector(".fill").style.width =
      `${Math.max(0, Math.min(100, (value - min) / (max - min) * 100))}%`;
  });
}
export const telemetryReceiver = new TelemetryStream(renderTelemetry);
