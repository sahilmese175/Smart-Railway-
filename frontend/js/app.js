// app.js : starts the app, draws the navigation and switches pages when the URL (#/...) changes.
import { initConsole } from "./core/console.js";
import { api, ApiError } from "./core/api.js";
import { allPages } from "./core/registry.js";
import { h, clear, toast } from "./core/ui.js";
import "./modules/index.js";

const main = document.getElementById("main");
const nav = document.getElementById("nav");
const counts = {};   // page id -> small number shown next to the nav label
let pageLife = new AbortController();   // aborted when you leave a page, so its listeners go away

function parseHash() {
  const raw = location.hash.replace(/^#\/?/, "");
  const [pathPart, queryPart = ""] = raw.split("?");
  const segs = pathPart.split("/").filter(Boolean).map(decodeURIComponent);
  return { path: segs[0] || "overview", params: segs.slice(1), query: new URLSearchParams(queryPart) };
}

export const go = (path) => { location.hash = "#/" + path; };

async function refreshCounts() {
  try {
    const s = await api.stats();
    counts.trains = s.trains;
    counts.stations = s.stations;
    drawNav();
  } catch { /* the status light already shows the problem */ }
}

function drawNav() {
  const { path } = parseHash();
  clear(nav);
  for (const p of allPages()) {
    const tag = counts[p.id];
    nav.append(h("li", null,
      h("a", { href: "#/" + p.path, "aria-current": p.path === path ? "page" : false },
        p.label, tag != null ? h("span", { class: "tag" }, tag) : null)));
  }
}

async function render() {
  const { path, params, query } = parseHash();
  const page = allPages().find((p) => p.path === path) || allPages()[0];
  drawNav();
  clear(main);
  pageLife.abort();
  pageLife = new AbortController();
  document.title = `${page.label} – SmartRailway`;
  try {
    await page.render(main, { params, query, go, refreshCounts, signal: pageLife.signal });
  } catch (e) {
    main.append(h("div", { class: "error-box", role: "alert" }, e instanceof ApiError ? e.message : `This page failed to load: ${e.message}`));
    console.error(e);
  }
  window.scrollTo(0, 0);
}

async function pingServer() {
  const box = document.getElementById("status"), text = document.getElementById("status-text");
  try {
    await fetch("/api/health", { cache: "no-store" });
    box.className = "status up"; text.textContent = "Server running";
  } catch {
    box.className = "status down"; text.textContent = "Server not reachable";
  }
}

initConsole();
window.addEventListener("hashchange", render);
render();
refreshCounts();
pingServer();
setInterval(pingServer, 8000);
window.addEventListener("railway:changed", refreshCounts);   // pages fire this after add / edit / delete
window.addEventListener("unhandledrejection", (e) => { if (e.reason instanceof ApiError) { toast(e.reason.message, true); e.preventDefault(); } });
