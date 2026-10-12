const $ = (s) => document.querySelector(s);
const $$ = (s) => document.querySelectorAll(s);

function view(name) {
  $$(".tab").forEach((t) => t.classList.toggle("active", t.dataset.view === name));
  $$(".view").forEach((v) => v.classList.toggle("active", v.id === name));
  if (name === "settings") loadConfig();
}
$$(".tab").forEach((t) => (t.onclick = () => view(t.dataset.view)));

async function getJSON(url, opts) {
  const r = await fetch(url, opts);
  if (!r.ok) throw new Error(r.status);
  return r.json();
}

// ---- readings ----
function ago(sec) {
  if (sec == null) return "нет данных";
  if (sec < 60) return `${sec} с назад`;
  if (sec < 3600) return `${Math.floor(sec / 60)} мин назад`;
  return `${Math.floor(sec / 3600)} ч назад`;
}

function fmt(v, d) {
  if (v == null) return "—";
  return v.toFixed(d ?? 1);
}

async function loadReadings() {
  let d;
  try {
    d = await getJSON("/api/readings");
  } catch {
    $("#dot").className = "dot off";
    return;
  }
  $("#dev").textContent = d.name || "nartis";
  $("#dot").className = "dot " + (d.online ? "on" : "off");
  const parts = [];
  if (d.serial) parts.push(`№ ${d.serial}`);
  parts.push(d.paired ? "сопряжён" : "не сопряжён");
  parts.push(ago(d.age));
  $("#meta").textContent = parts.join("  ·  ");

  const grid = $("#grid");
  grid.innerHTML = "";
  (d.values || []).forEach((m) => {
    const el = document.createElement("div");
    el.className = "cell" + (m.stale ? " stale" : "");
    el.innerHTML =
      `<div><span class="v">${fmt(m.value, m.decimals)}</span><span class="u">${m.unit || ""}</span></div>` +
      `<div class="l">${m.label}</div>`;
    grid.appendChild(el);
  });
}

// ---- settings ----
async function loadConfig() {
  try {
    const c = await getJSON("/api/config");
    $("#serial").value = c.meter_serial || "";
    $("#channel").value = c.channel ?? "auto";
    $("#interval").value = String(c.update_interval || 600);
    $("#apiUrl").textContent = (location.origin || "") + "/api/readings";
  } catch {}
  scan();
}

async function scan() {
  const sel = $("#ssid");
  sel.innerHTML = "<option>поиск…</option>";
  try {
    const d = await getJSON("/api/wifi/scan");
    sel.innerHTML = "";
    (d.networks || []).forEach((n) => {
      const o = document.createElement("option");
      o.value = n.ssid;
      o.textContent = n.ssid + (n.secure ? "" : "  (открытая)");
      sel.appendChild(o);
    });
    if (!sel.children.length) sel.innerHTML = "<option value=''>сети не найдены</option>";
  } catch {
    sel.innerHTML = "<option value=''>ошибка поиска</option>";
  }
}
$("#rescan").onclick = scan;

$("#wifiForm").onsubmit = async (e) => {
  e.preventDefault();
  const msg = $("#wifiMsg");
  msg.className = "msg";
  msg.textContent = "Подключение…";
  try {
    await getJSON("/api/wifi", {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ ssid: $("#ssid").value, password: $("#pass").value }),
    });
    msg.className = "msg ok";
    msg.textContent = "Сохранено. Устройство переподключается.";
  } catch {
    msg.className = "msg bad";
    msg.textContent = "Не удалось сохранить.";
  }
};

$("#cfgForm").onsubmit = async (e) => {
  e.preventDefault();
  const msg = $("#cfgMsg");
  msg.className = "msg";
  msg.textContent = "Сохранение…";
  try {
    await getJSON("/api/config", {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        meter_serial: $("#serial").value,
        channel: $("#channel").value,
        update_interval: parseInt($("#interval").value, 10),
      }),
    });
    msg.className = "msg ok";
    msg.textContent = "Сохранено. Устройство перезагружается.";
  } catch {
    msg.className = "msg bad";
    msg.textContent = "Ошибка сохранения.";
  }
};

loadReadings();
setInterval(loadReadings, 5000);
