// components/network-map.js : draws every station and every route segment as a zoomable map.
//
// Stations the app already knows (see GEO below) are placed at their real position.
// Any other station is placed automatically by a small spring layout, so the map
// still works for stations you add yourself.  Add more entries to GEO if you like.
import { h, svg } from "../core/ui.js";

const GEO = {   // lower-case name: [latitude, longitude]
  "pune": [18.52, 73.86], "lonavala": [18.75, 73.41], "karjat": [18.91, 73.32], "kalyan": [19.24, 73.13],
  "mumbai": [18.97, 72.82], "kolhapur": [16.70, 74.24], "sangli": [16.85, 74.57], "satara": [17.68, 74.00],
  "igatpuri": [19.70, 73.56], "nashik road": [19.95, 73.84], "manmad": [20.25, 74.44], "solapur": [17.66, 75.91],
  "kalaburagi": [17.33, 76.83], "raichur": [16.20, 77.36], "guntakal": [15.17, 77.37], "bengaluru": [12.97, 77.59],
  "surat": [21.17, 72.83], "vadodara": [22.31, 73.18], "ratlam": [23.33, 75.04], "kota": [25.18, 75.84],
  "delhi": [28.61, 77.21], "nagpur": [21.15, 79.09], "thane": [19.19, 72.97], "dadar": [19.02, 72.84],
  "bhusawal": [21.05, 75.79], "daund": [18.46, 74.58], "hyderabad": [17.38, 78.48], "chennai": [13.08, 80.27],
  "ahmedabad": [23.02, 72.57], "bhopal": [23.26, 77.41], "jaipur": [26.91, 75.79], "kolkata": [22.57, 88.36],
};
const W = 1000, H = 680, PAD = 56;

export function drawNetworkMap(container, trains, stations, { onStation } = {}) {
  const info = new Map();                       // lower name -> {name, trains:Set}
  const nodeFor = (name) => {
    const k = name.toLowerCase();
    if (!info.has(k)) info.set(k, { name, key: k, trains: new Set(), nbrs: new Set() });
    return info.get(k);
  };
  stations.forEach((s) => nodeFor(s.name));
  const edges = new Map();                      // "a|b" -> {a, b, km, trains:Set}
  for (const t of trains) {
    t.stops.forEach((s, i) => {
      nodeFor(s.station).trains.add(t.trainNo);
      if (i === 0) return;
      const p = t.stops[i - 1], a = p.station.toLowerCase(), b = s.station.toLowerCase();
      const key = a < b ? a + "|" + b : b + "|" + a;
      if (!edges.has(key)) edges.set(key, { a, b, km: s.distanceKm - p.distanceKm, trains: new Set() });
      edges.get(key).trains.add(t.trainNo);
      info.get(a).nbrs.add(b); info.get(b).nbrs.add(a);
    });
  }
  const nodes = [...info.values()];
  placeNodes(nodes, edges);

  // ---------------------------------------------------------------- svg
  const root = svg("svg", { viewBox: `0 0 ${W} ${H}`, role: "img", "aria-label": `Network map with ${nodes.length} stations and ${edges.size} route segments`, style: "touch-action:none; cursor:grab" });
  const edgeLayer = svg("g"), nodeLayer = svg("g");
  root.append(edgeLayer, nodeLayer);
  const lines = [...edges.values()].map((e) => {
    const A = info.get(e.a), B = info.get(e.b);
    const ln = svg("line", { class: "map-edge", x1: A.x, y1: A.y, x2: B.x, y2: B.y, "vector-effect": "non-scaling-stroke" });
    ln.style.setProperty("--w", `${1.6 + Math.min(e.trains.size, 5) * 0.8}px`);   // busier segments are thicker
    edgeLayer.append(ln);
    return { ln, e };
  });
  for (const n of nodes) {
    n.circle = svg("circle", { cx: n.x, cy: n.y, r: 5 });
    n.label = svg("text", { x: n.x, y: n.y }, n.name);
    n.g = svg("g", { class: "map-node" }, svg("title", null, `${n.name}: ${n.trains.size} train${n.trains.size === 1 ? "" : "s"}`), n.circle, n.label);
    if (onStation) { n.g.style.cursor = "pointer"; n.g.addEventListener("click", () => onStation(n.name)); }
    nodeLayer.append(n.g);
  }

  // ---------------------------------------------------------------- zoom and pan
  let view = { x: 0, y: 0, w: W, h: H };
  let hot = null;   // highlighted train number
  const fit = () => { view = { x: 0, y: 0, w: W, h: H }; update(); };
  function update() {
    root.setAttribute("viewBox", `${view.x} ${view.y} ${view.w} ${view.h}`);
    const rect = root.getBoundingClientRect();
    const k = (rect.width || 800) / view.w;               // screen pixels per svg unit
    const placed = [];
    const order = [...nodes].sort((a, b) => (isHot(b) - isHot(a)) || (b.trains.size - a.trains.size) || a.name.localeCompare(b.name));
    for (const n of order) {
      const r = 5 / k;
      n.circle.setAttribute("r", isHot(n) ? r * 1.35 : r);
      n.circle.style.strokeWidth = `${2.2 / k}px`;
      n.label.setAttribute("x", n.x + 9 / k); n.label.setAttribute("y", n.y + 4 / k);
      n.label.style.fontSize = `${12.5 / k}px`; n.label.style.strokeWidth = `${4 / k}px`;
      const sx = (n.x - view.x) * k, sy = (n.y - view.y) * k;
      const box = { l: sx + 8, r: sx + 12 + n.name.length * 6.9, t: sy - 8, b: sy + 8 };
      const inView = sx > -20 && sx < rect.width + 20 && sy > -20 && sy < rect.height + 20;
      const clash = placed.some((p) => box.l < p.r && box.r > p.l && box.t < p.b && box.b > p.t);
      const show = inView && (!clash || isHot(n));
      n.label.style.display = show ? "" : "none";
      if (show) placed.push(box);
    }
  }
  const isHot = (n) => (hot != null && n.trains.has(hot) ? 1 : 0);

  root.addEventListener("wheel", (e) => {
    e.preventDefault();
    const rect = root.getBoundingClientRect();
    const f = e.deltaY < 0 ? 0.8 : 1.25;
    zoomAt(((e.clientX - rect.left) / rect.width), ((e.clientY - rect.top) / rect.height), f);
  }, { passive: false });
  function zoomAt(fx, fy, f) {
    const nw = Math.min(W, Math.max(W / 40, view.w * f)), nh = nw * (H / W);
    view = { x: view.x + (view.w - nw) * fx, y: view.y + (view.h - nh) * fy, w: nw, h: nh };
    clampView(); update();
  }
  function clampView() {
    view.x = Math.min(Math.max(view.x, -50), W + 50 - view.w);
    view.y = Math.min(Math.max(view.y, -50), H + 50 - view.h);
  }
  let drag = null;
  root.addEventListener("pointerdown", (e) => { drag = { x: e.clientX, y: e.clientY, vx: view.x, vy: view.y }; root.setPointerCapture(e.pointerId); root.style.cursor = "grabbing"; });
  root.addEventListener("pointermove", (e) => {
    if (!drag) return;
    const rect = root.getBoundingClientRect();
    view.x = drag.vx - ((e.clientX - drag.x) / rect.width) * view.w;
    view.y = drag.vy - ((e.clientY - drag.y) / rect.height) * view.h;
    clampView(); update();
  });
  const endDrag = () => { drag = null; root.style.cursor = "grab"; };
  root.addEventListener("pointerup", endDrag); root.addEventListener("pointercancel", endDrag);

  const controls = h("div", { class: "map-controls" },
    h("button", { class: "icon-btn", type: "button", "aria-label": "Zoom in", onclick: () => zoomAt(0.5, 0.5, 0.7) }, "+"),
    h("button", { class: "icon-btn", type: "button", "aria-label": "Zoom out", onclick: () => zoomAt(0.5, 0.5, 1.4) }, "−"),
    h("button", { class: "icon-btn", type: "button", onclick: fit }, "Fit"));
  container.append(root, controls);
  new ResizeObserver(update).observe(root);
  update();

  return {
    /** Zoom so one train's whole route fills the map (pass null for the whole network). */
    focus(trainNo) {
      const on = trainNo == null ? [] : nodes.filter((n) => n.trains.has(trainNo));
      if (!on.length) return fit();
      const minX = Math.min(...on.map((n) => n.x)), maxX = Math.max(...on.map((n) => n.x));
      const minY = Math.min(...on.map((n) => n.y)), maxY = Math.max(...on.map((n) => n.y));
      const w = Math.max((maxX - minX) * 1.15 + 70, ((maxY - minY) * 1.15 + 70) * (W / H), W / 40), cx = (minX + maxX) / 2, cy = (minY + maxY) / 2;
      view = { x: cx - w / 2, y: cy - (w * H) / W / 2, w, h: (w * H) / W };
      clampView(); update();
    },
    /** Light up one train's route (pass null to clear). */
    highlight(trainNo) {
      hot = trainNo;
      for (const { ln, e } of lines) {
        const on = hot != null && e.trains.has(hot);
        ln.classList.toggle("on", on); ln.classList.toggle("dim", hot != null && !on);
      }
      for (const n of nodes) {
        n.g.classList.toggle("on", isHot(n) === 1); n.g.classList.toggle("dim", hot != null && !isHot(n));
      }
      update();
    },
  };
}

// ---------------------------------------------------------------- layout
function placeNodes(nodes, edges) {
  const known = nodes.filter((n) => GEO[n.key]);
  const midLat = known.length ? known.reduce((s, n) => s + GEO[n.key][0], 0) / known.length : 20;
  const cosLat = Math.cos((midLat * Math.PI) / 180);
  known.forEach((n) => { n.px = GEO[n.key][1] * cosLat; n.py = -GEO[n.key][0]; n.fixed = true; });

  // Unknown stations start next to a known neighbour (or on a circle) and are then relaxed.
  const spread = known.length > 1 ? Math.hypot(Math.max(...known.map((n) => n.px)) - Math.min(...known.map((n) => n.px)), Math.max(...known.map((n) => n.py)) - Math.min(...known.map((n) => n.py))) : 10;
  const cx = known.length ? known.reduce((s, n) => s + n.px, 0) / known.length : 0, cy = known.length ? known.reduce((s, n) => s + n.py, 0) / known.length : 0;
  let i = 0;
  for (const n of nodes) {
    if (n.fixed) continue;
    const nb = [...n.nbrs].map((k) => nodes.find((m) => m.key === k)).filter((m) => m && m.px != null);
    if (nb.length) { n.px = nb.reduce((s, m) => s + m.px, 0) / nb.length + Math.cos(i) * spread * 0.03; n.py = nb.reduce((s, m) => s + m.py, 0) / nb.length + Math.sin(i) * spread * 0.03; }
    else { const a = (i / Math.max(1, nodes.length)) * Math.PI * 2; n.px = cx + Math.cos(a) * spread * 0.4; n.py = cy + Math.sin(a) * spread * 0.4; }
    i++;
  }
  // rest length of a spring = km * (map units per km), measured from fixed pairs when we have them
  let ratio = 0, cnt = 0;
  for (const e of edges.values()) {
    const A = nodes.find((n) => n.key === e.a), B = nodes.find((n) => n.key === e.b);
    if (A.fixed && B.fixed && e.km > 0) { ratio += Math.hypot(A.px - B.px, A.py - B.py) / e.km; cnt++; }
  }
  ratio = cnt ? ratio / cnt : spread / 800;
  const move = nodes.filter((n) => !n.fixed);
  const byKey = new Map(nodes.map((n) => [n.key, n]));
  for (let it = 0; it < 250 && move.length; it++) {
    for (const n of move) {
      let fx = 0, fy = 0;
      for (const m of nodes) {
        if (m === n) continue;
        const dx = n.px - m.px, dy = n.py - m.py, d = Math.max(Math.hypot(dx, dy), 1e-6);
        const rep = (ratio * 30) ** 2 / (d * d);
        fx += (dx / d) * rep * ratio * 30; fy += (dy / d) * rep * ratio * 30;
      }
      for (const k of n.nbrs) {
        const m = byKey.get(k);
        const e = edges.get(n.key < k ? n.key + "|" + k : k + "|" + n.key);
        const rest = Math.max(e.km, 20) * ratio;
        const dx = m.px - n.px, dy = m.py - n.py, d = Math.max(Math.hypot(dx, dy), 1e-6);
        const pull = (d - rest) * 0.12;
        fx += (dx / d) * pull; fy += (dy / d) * pull;
      }
      n.px += fx * 0.5; n.py += fy * 0.5;
    }
  }
  // fit into the svg box, keeping the aspect ratio
  const xs = nodes.map((n) => n.px), ys = nodes.map((n) => n.py);
  const minX = Math.min(...xs), maxX = Math.max(...xs), minY = Math.min(...ys), maxY = Math.max(...ys);
  const s = Math.min((W - PAD * 2) / Math.max(maxX - minX, 1e-6), (H - PAD * 2) / Math.max(maxY - minY, 1e-6));
  const ox = (W - (maxX - minX) * s) / 2, oy = (H - (maxY - minY) * s) / 2;
  for (const n of nodes) { n.x = ox + (n.px - minX) * s; n.y = oy + (n.py - minY) * s; }
}
