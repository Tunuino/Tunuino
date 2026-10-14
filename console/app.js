// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
import {rpmAxis, loadAxis, makeDemoMap, setCell, mapToCsv} from "./map-model.js";
import {TelemetryStream} from "./telemetry-stream.js";
import {TunuinoSerialTransport} from "./serial-transport.js";
import {TelemetryFreshness} from "./telemetry-freshness.js";
import {TuneSnapshotAssembler} from "./tune-snapshot.js";
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
const demoMaps = {ve: map, ignition: makeDemoMap("ignition")};
let ecuMaps = null;
let showingEcuMap = false;
let tuneAssembler = new TuneSnapshotAssembler();
function renderMap() {
  const table = document.querySelector("#map-table");
  table.replaceChildren();
  const header = document.createElement("tr");
  const activeRpmAxis = showingEcuMap ? ecuMaps[currentType].rpmAxis : rpmAxis;
  const activeLoadAxis = showingEcuMap ? ecuMaps[currentType].loadAxis : loadAxis;
  for (const label of ["kPa / RPM", ...activeRpmAxis]) {
    const th = document.createElement("th"); th.textContent = label; header.append(th);
  }
  table.append(header);
  map.forEach((values, row) => {
    const tr = document.createElement("tr");
    const th = document.createElement("th"); th.textContent = activeLoadAxis[row]; tr.append(th);
    values.forEach((value, col) => {
      const td = document.createElement("td");
      const input = document.createElement("input");
      input.type = "number"; input.step = "0.1"; input.disabled = showingEcuMap; input.value = String(value);
      input.setAttribute("aria-label", `Carga ${activeLoadAxis[row]} kPa, ${activeRpmAxis[col]} rpm`);
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
  currentType = e.target.value;
  map = showingEcuMap ? ecuMaps[currentType].cells : demoMaps[currentType]; renderMap();
});
document.querySelector("#reset-map").addEventListener("click", () => {
  showingEcuMap = false;
  demoMaps[currentType] = makeDemoMap(currentType);
  map = demoMaps[currentType]; renderMap();
  document.querySelector("#map-notice").textContent = "Mapa demonstrativo reposto. Sem escrita na ECU.";
});
document.querySelector("#export-map").addEventListener("click", () => {
  const blob = new Blob([mapToCsv(map)], {type: "text/csv;charset=utf-8"});
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a"); a.href = url; a.download = `tunuino-${currentType}-${showingEcuMap ? "ecu" : "demo"}.csv`; a.click();
  URL.revokeObjectURL(url);
});
renderMap();

// Validated telemetry is rendered only when explicitly delivered by a transport.
// No transport is attached in this prototype, so the dashboard remains unavailable.
const telemetryKeys = ["rpm", "map", "tps", "clt", "iat", "afr", "battery"];
const freshness = new TelemetryFreshness(() => clearGauges(), 500);
function clearGauges() {
  document.querySelectorAll("#gauges .gauge").forEach(card => {
    card.querySelector(".value").textContent = "--";
    card.querySelector(".fill").style.width = "0%";
  });
}
export function renderTelemetry(values) {
  freshness.received(performance.now());
  const cards = document.querySelectorAll("#gauges .gauge");
  telemetryKeys.forEach((key, index) => {
    const value = values[key];
    if (!Number.isFinite(value)) {
      cards[index].querySelector(".value").textContent = "--";
      cards[index].querySelector(".fill").style.width = "0%";
      return;
    }
    cards[index].querySelector(".value").textContent = String(value);
    const min = metrics[index][2], max = metrics[index][3];
    cards[index].querySelector(".fill").style.width =
      `${Math.max(0, Math.min(100, (value - min) / (max - min) * 100))}%`;
  });
}
export const telemetryReceiver = new TelemetryStream(renderTelemetry);

const connectionLabel = document.querySelector("#connection");
const connectButton = document.querySelector("#connect-serial");
const disconnectButton = document.querySelector("#disconnect-serial");
const readTuneButton = document.querySelector("#read-ecu-map");
const serialTransport = new TunuinoSerialTransport({
  onTelemetry: renderTelemetry,
  onTuneChunk(payload) {
    if (!tuneAssembler.addChunk(payload)) return;
      ecuMaps = tuneAssembler.finish();
      showingEcuMap = true;
      map = ecuMaps[currentType].cells;
      renderMap();
      document.querySelector("#map-notice").textContent =
        "Mapa da ECU, revisão " + ecuMaps.revision + ". Apenas leitura.";
  },
  onStatus(status) {
    if (status === "disconnected" || status === "error") {
      freshness.reset();
      tuneAssembler = new TuneSnapshotAssembler();
      ecuMaps = null;
      showingEcuMap = false;
      map = demoMaps[currentType];
      renderMap();
      document.querySelector("#map-notice").textContent = "Sem mapa da ECU. Demonstração local.";
    }
    const labels = {
      "serial-open": "SERIAL ABERTA · NÃO IDENTIFICADA",
      identified: "TUNUINO IDENTIFICADA · LEITURA",
      disconnected: "DEMO · SEM ECU",
      error: "ERRO NA LIGAÇÃO"
    };
    connectionLabel.textContent = labels[status] ?? "SEM ECU";
    readTuneButton.disabled = status !== "identified";
    disconnectButton.disabled = status === "disconnected" || status === "error";
    if (status === "disconnected" || status === "error") {
      document.querySelectorAll("#gauges .gauge").forEach(card => {
        card.querySelector(".value").textContent = "--";
        card.querySelector(".fill").style.width = "0%";
      });
    }
  }
});
readTuneButton.addEventListener("click", async () => {
  readTuneButton.disabled = true;
  tuneAssembler = new TuneSnapshotAssembler();
  document.querySelector("#map-notice").textContent = "A ler 37 blocos com confirmação e repetição em caso de falha...";
  try {
    await serialTransport.requestTune();
    if (!ecuMaps) throw new Error("Leitura incompleta: nenhum mapa validado");
  } catch (error) {
    tuneAssembler = new TuneSnapshotAssembler();
    ecuMaps = null;
    showingEcuMap = false;
    map = demoMaps[currentType];
    renderMap();
    document.querySelector("#map-notice").textContent = "Leitura falhou: " + error.message;
  } finally {
    readTuneButton.disabled = !serialTransport.identified;
  }
});
connectButton.addEventListener("click", async () => {
  connectButton.disabled = true;
  disconnectButton.disabled = false;
  try { await serialTransport.connect(); }
  catch (error) { document.querySelector("#engine-status").textContent = `Ligação falhou: ${error.message}`; }
  finally { connectButton.disabled = false; disconnectButton.disabled = true; }
});
disconnectButton.addEventListener("click", async () => {
  disconnectButton.disabled = true;
  await serialTransport.disconnect();
});

// Watchdog runs only while the page is active; after tab suspension it expires on resume.
setInterval(() => freshness.check(performance.now()), 100);
