/* Set this to your deployed Render service URL, e.g. https://your-api.onrender.com */
const API_BASE = window.APP_CONFIG?.API_BASE || "http://localhost:8080";
const $ = (id) => document.getElementById(id);
let readings = [];

const sliders = [
  ["current", "currentLabel"], ["ambient", "ambientLabel"],
  ["initialTemp", "initialLabel"], ["cooling", "coolingLabel"],
  ["duration", "durationLabel"]
];
sliders.forEach(([input, label]) => {
  $(input).addEventListener("input", () => $(label).textContent = $(input).value);
});

function setStatus(status) {
  const el = $("tempStatus");
  el.textContent = status;
  el.className = "status-pill " + status.toLowerCase();
}

function setInsight(data) {
  const t = data.temperature;
  let text;
  if (t >= 60) text = `Critical temperature detected at ${t.toFixed(1)}°C. In a real battery management system, charging/discharging should be limited and the thermal management system should be checked immediately.`;
  else if (t >= 45) text = `The pack reached ${t.toFixed(1)}°C, entering the warning zone. Increase cooling or reduce current to help control temperature.`;
  else if (t < 5) text = `The pack is cold at ${t.toFixed(1)}°C. Low temperatures can reduce battery power and charging performance.`;
  else text = `The pack is operating in the normal temperature zone at ${t.toFixed(1)}°C. Current heat generation is ${data.heatGenerationW.toFixed(0)} W. Keep monitoring as operating conditions change.`;
  $("insight").textContent = text;
}

async function api(path, options = {}) {
  const response = await fetch(API_BASE + path, {
    ...options,
    headers: { "Content-Type": "application/json", ...(options.headers || {}) }
  });
  const data = await response.json();
  if (!response.ok) throw new Error(data.error || "API request failed");
  return data;
}

async function loadReadings() {
  try {
    const data = await api("/api/readings");
    readings = data.readings.reverse();
    renderHistory();
    drawChart();
  } catch (error) {
    $("message").textContent = "Backend not reachable. Start the C API or check API_BASE.";
  }
}

function renderHistory() {
  const tbody = $("historyRows");
  if (!readings.length) {
    tbody.innerHTML = '<tr><td colspan="6" class="no-data">No readings yet. Run your first simulation.</td></tr>';
    return;
  }
  tbody.innerHTML = readings.slice().reverse().slice(0, 12).map(r => `
    <tr>
      <td>${escapeHtml(r.createdAt)}</td><td><b>${Number(r.temperature).toFixed(1)} °C</b></td>
      <td>${Number(r.current).toFixed(0)} A</td><td>${Number(r.ambient).toFixed(0)} °C</td>
      <td>${Number(r.cooling).toFixed(0)} W/K</td>
      <td><span class="status-pill ${String(r.status).toLowerCase()}">${escapeHtml(r.status)}</span></td>
    </tr>`).join("");
}

function escapeHtml(value) {
  return String(value).replace(/[&<>"']/g, c => ({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[c]));
}

function drawChart() {
  const canvas = $("chart");
  const ctx = canvas.getContext("2d");
  const rect = canvas.getBoundingClientRect();
  const dpr = window.devicePixelRatio || 1;
  canvas.width = Math.max(1, rect.width * dpr);
  canvas.height = Math.max(1, rect.height * dpr);
  ctx.scale(dpr, dpr);
  const w = rect.width, h = rect.height;
  ctx.clearRect(0, 0, w, h);
  $("emptyChart").style.display = readings.length ? "none" : "grid";
  if (!readings.length) return;

  const values = readings.slice(-20).map(r => Number(r.temperature));
  let min = Math.min(0, ...values) - 2, max = Math.max(65, ...values) + 2;
  const left = 38, right = 12, top = 12, bottom = 28;
  const cw = w-left-right, ch = h-top-bottom;
  ctx.font = "10px DM Sans";
  ctx.fillStyle = "#9aa5b3";
  for (let i=0;i<5;i++) {
    const y = top + ch*i/4;
    const val = max - (max-min)*i/4;
    ctx.strokeStyle = "#edf1f6"; ctx.lineWidth = 1;
    ctx.beginPath(); ctx.moveTo(left,y); ctx.lineTo(w-right,y); ctx.stroke();
    ctx.fillText(val.toFixed(0)+"°", 2, y+3);
  }
  ctx.strokeStyle = "#416cf3"; ctx.lineWidth = 2.5; ctx.lineJoin = "round";
  ctx.beginPath();
  values.forEach((v,i) => {
    const x = left + (values.length===1 ? cw/2 : cw*i/(values.length-1));
    const y = top + (max-v)*ch/(max-min);
    if (i===0) ctx.moveTo(x,y); else ctx.lineTo(x,y);
  });
  ctx.stroke();
  values.forEach((v,i) => {
    const x = left + (values.length===1 ? cw/2 : cw*i/(values.length-1));
    const y = top + (max-v)*ch/(max-min);
    ctx.fillStyle="#fff";ctx.strokeStyle="#416cf3";ctx.lineWidth=2;
    ctx.beginPath();ctx.arc(x,y,3.5,0,Math.PI*2);ctx.fill();ctx.stroke();
  });
}

$("simulationForm").addEventListener("submit", async (event) => {
  event.preventDefault();
  const button = $("runButton");
  button.disabled = true; button.textContent = "Running simulation...";
  $("message").textContent = "Calculating battery thermal response...";
  const payload = Object.fromEntries(sliders.map(([id]) => [id === "initialTemp" ? "initialTemp" : id, Number($(id).value)]));
  try {
    const data = await api("/api/simulate", { method: "POST", body: JSON.stringify(payload) });
    $("temperature").textContent = data.temperature.toFixed(1);
    $("currentValue").textContent = data.current.toFixed(0);
    $("heatValue").textContent = data.heatGenerationW.toFixed(0);
    $("ambientValue").textContent = data.ambient.toFixed(0);
    setStatus(data.status); setInsight(data);
    $("message").textContent = `Simulation complete · final pack temperature ${data.temperature.toFixed(2)} °C`;
    await loadReadings();
  } catch (error) {
    $("message").textContent = error.message + " — check that the C backend is running.";
  } finally {
    button.disabled = false; button.innerHTML = "<span>▶</span> Run simulation";
  }
});

$("clearButton").addEventListener("click", async () => {
  if (!confirm("Clear all saved simulation readings?")) return;
  try {
    await api("/api/reset", { method: "POST", body: "{}" });
    await loadReadings();
    $("message").textContent = "Reading history cleared.";
  } catch (error) { $("message").textContent = error.message; }
});

window.addEventListener("resize", drawChart);
loadReadings();
