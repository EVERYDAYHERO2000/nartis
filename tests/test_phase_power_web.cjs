const assert = require("node:assert/strict");
const fs = require("node:fs");
const vm = require("node:vm");
const path = require("node:path");

const values = [
  { obis: "1.0.32.7.0.255", value: 230 }, { obis: "1.0.31.7.0.255", value: 2 },
  { obis: "1.0.52.7.0.255", value: 220 }, { obis: "1.0.51.7.0.255", value: 3 },
  { obis: "1.0.72.7.0.255", value: 240 }, { obis: "1.0.71.7.0.255", value: 0 },
  { obis: "1.0.14.7.0.255", value: 50, label: "Частота", unit: "Гц" },
  { obis: "1.0.1.7.0.255", value: 1000, label: "Мощность", unit: "Вт" },
];
const elements = new Map();
const element = () => ({ children: [], appendChild(child) { this.children.push(child); } });
const context = vm.createContext({
  document: {
    querySelector(selector) {
      if (!elements.has(selector)) elements.set(selector, element());
      return elements.get(selector);
    },
    querySelectorAll: () => [], createElement: element,
  },
  fetch: async () => ({ ok: true, json: async () => ({ values }) }),
  setInterval: () => {},
});
vm.runInContext(fs.readFileSync(path.join(__dirname, "../web/app.js"), "utf8"), context);

(async () => {
  const powers = context.phasePowerValues(values);
  assert.deepEqual(Array.from(powers, p => p.value), [460, 660, 0]);
  assert.deepEqual(Array.from(powers, p => p.label), ["Расчётная мощность L1", "Расчётная мощность L2", "Расчётная мощность L3"]);
  assert.ok(powers.every(p => p.unit === "В·А"));
  for (const bad of [null, NaN, Infinity]) {
    assert.equal(context.phasePowerValues([{ ...values[0], value: bad }, values[1]])[0].value, null);
  }
  assert.equal(context.phasePowerValues([{ ...values[0], stale: true }, values[1]])[0].value, null);
  assert.equal(context.phasePowerValues([values[0]])[0].value, null);
  await context.loadReadings();
  const cards = elements.get("#grid").children;
  assert.ok(cards[0].innerHTML.includes('class="l">Мощность'));
  assert.ok(cards[1].innerHTML.includes('class="l">Частота'));
  assert.ok(cards.some(card => card.innerHTML.includes("Расчётная мощность L1") && card.innerHTML.includes("460")));
  console.log("Dashboard phase power checks passed");
})().catch(error => { console.error(error); process.exitCode = 1; });
