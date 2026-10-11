// components/train-sheet.js : the right-hand panel that shows ONE train.
// Open it from any page with  openTrainSheet(12001).
import { api } from "../core/api.js";
import { h, openDialog, confirmDialog, toast, fmtDuration, rupees, dayNote, notifyChanged } from "../core/ui.js";
import { openTrainForm } from "./train-form.js";

export async function openTrainSheet(trainNo, { highlightFrom, highlightTo } = {}) {
  const { train } = await api.train(trainNo);
  let ref;

  const tabs = { route: h("div"), ds: h("div") };
  const tabBar = h("div", { class: "tabs", role: "tablist" });
  const show = (name) => {
    for (const [k, panel] of Object.entries(tabs)) panel.hidden = k !== name;
    for (const b of tabBar.children) b.setAttribute("aria-selected", String(b.dataset.tab === name));
    if (name === "ds" && !tabs.ds.dataset.ready) { tabs.ds.dataset.ready = "1"; drawStructures(tabs.ds, train); }
  };
  for (const [name, label] of [["route", "Route"], ["ds", "Data structures"]]) {
    tabBar.append(h("button", { type: "button", role: "tab", dataset: { tab: name }, onclick: () => show(name) }, label));
  }

  drawRoute(tabs.route, train, highlightFrom, highlightTo);

  const body = h("div", null,
    h("div", { class: "meta-row", style: "margin:0 0 18px" },
      h("span", { class: "chip" }, fmtDuration(train.durationMin)),
      h("span", { class: "chip" }, `${train.totalDistanceKm} km`),
      h("span", { class: "chip" }, `${train.stopCount} stations`),
      h("span", { class: "chip plain" }, `${train.totalSeats} seats`),
      h("span", { class: "chip plain" }, `${rupees(train.farePerKm)}/km`)),
    tabBar, tabs.route, tabs.ds);

  const footer = [
    h("button", { class: "btn danger", type: "button", onclick: async () => {
      if (!(await confirmDialog(`Delete train ${train.trainNo} ${train.name}? This also removes it from every station list.`))) return;
      try { await api.deleteTrain(train.trainNo); toast(`Train ${train.trainNo} deleted`); ref.close(); notifyChanged(); }
      catch (e) { toast(e.message, true); }
    } }, "Delete train"),
    h("span", { class: "spacer" }),
    h("button", { class: "btn", type: "button", onclick: () => { ref.close(); openTrainForm({ train }); } }, "Edit train"),
  ];

  ref = openDialog({
    title: train.name,
    subtitle: `Train ${train.trainNo}  •  ${train.source} to ${train.destination}`,
    body, footer, sheet: true,
  });
  show("route");
}

// ---------------------------------------------------------------- route tab
function drawRoute(panel, train, hiFrom, hiTo) {
  const stops = train.stops;
  const last = stops.length - 1;
  const fromSel = h("select", { id: "fare-from", "aria-label": "From station" });
  const toSel = h("select", { id: "fare-to", "aria-label": "To station" });
  stops.forEach((s, i) => {
    fromSel.append(h("option", { value: s.station }, s.station));
    toSel.append(h("option", { value: s.station }, s.station));
  });
  fromSel.value = hiFrom || stops[0].station;
  toSel.value = hiTo || stops[last].station;

  const out = h("div", { class: "fare-out", "aria-live": "polite" });
  const list = h("ol", { class: "line-route" });

  function drawList(picked) {
    list.replaceChildren(...stops.map((s, i) => {
      const edge = i === 0 || i === last;
      const isPicked = picked && i >= picked[0] && i <= picked[1];
      const main = i === 0 ? s.departure : s.arrival;
      const day = i === 0 ? s.departureDay : s.arrivalDay;
      const stay = i > 0 && i < last ? `leaves ${s.departure}` : i === 0 ? "starts" : "ends";
      return h("li", { class: (edge ? "edge " : "") + (isPicked ? "picked" : "") },
        h("div", { class: "tm" }, main + dayNote(day), h("small", null, stay)),
        h("div", { class: "pin" }),
        h("div", { class: "nm" }, s.station, h("small", null, `${s.distanceKm} km  •  stop ${i}`)),
        h("div", { class: "rs" }, rupees(s.fareFromStart)));
    }));
  }

  async function recalc() {
    try {
      const r = await api.validate(train.trainNo, fromSel.value, toSel.value);
      if (!r.valid) {
        out.replaceChildren(h("div", { class: "error-box" }, r.message));
        drawList(null);
        return;
      }
      const j = r.journey;
      out.replaceChildren(
        h("b", null, rupees(j.fare)), `for ${j.distanceKm} km, ${fmtDuration(j.durationMin)} `,
        h("div", { class: "meta-row" },
          h("span", { class: "stat-chip" }, `route positions ${j.srcIdx} → ${j.dstIdx}`),
          h("span", { class: "stat-chip" }, `seat segments ${j.srcIdx}..${j.dstIdx - 1}`)));
      drawList([j.srcIdx, j.dstIdx]);
    } catch (e) { out.replaceChildren(h("div", { class: "error-box" }, e.message)); }
  }
  fromSel.addEventListener("change", recalc);
  toSel.addEventListener("change", recalc);

  panel.append(
    h("div", { class: "fare-box" },
      h("div", { class: "row" },
        h("label", { class: "field" }, "Boarding station", fromSel),
        h("label", { class: "field" }, "Getting off at", toSel)),
      out),
    list);
  recalc();
}

// ---------------------------------------------------------------- data structure tab
async function drawStructures(panel, train) {
  const stops = train.stops;
  const ll = h("div", { class: "ll" }, h("span", { class: "ll-cap" }, "head"));
  stops.forEach((s, i) => {
    ll.append(h("div", { class: "ll-node" }, h("b", null, s.station), h("span", null, `${s.distanceKm} km`)));
    ll.append(i < stops.length - 1 ? h("span", { class: "ll-link", "aria-hidden": "true" }, "⇆") : h("span", { class: "ll-cap" }, "tail"));
  });
  const arr = h("div", { class: "arr" }, stops.map((s, i) => h("div", { class: "arr-cell" }, h("i", null, `[${i}]`), h("b", null, s.station))));

  const hashHolder = h("div", { class: "ds-scroll" }, h("span", { class: "hint" }, "Loading…"));
  panel.append(
    h("div", { class: "ds-block" }, h("h3", null, "Route = doubly linked list"),
      h("p", null, "Each stop points to the next and previous one, so a stop can be inserted or removed in the middle without shifting anything."),
      h("div", { class: "ds-scroll" }, ll)),
    h("div", { class: "ds-block" }, h("h3", null, "Positions = dynamic array of pointers"),
      h("p", null, "A second view over the same nodes, so \"which station is stop 3?\" takes one step instead of walking the list."),
      h("div", { class: "ds-scroll" }, arr)),
    h("div", { class: "ds-block" }, h("h3", null, "Station to position = hash table"),
      h("p", null, "Stations are hashed to a bucket, so \"is Karjat on this route and where?\" is a lookup, not a scan. Buckets holding more than one station show a collision."),
      hashHolder));
  try {
    const { table } = await api.hashDump({ table: "stops", train: train.trainNo });
    hashHolder.replaceChildren(renderBuckets(table));
  } catch (e) { hashHolder.replaceChildren(h("div", { class: "error-box" }, e.message)); }
}

export function renderBuckets(table, hitBucket = -1) {
  const wrap = h("div");
  const s = table.stats;
  wrap.append(h("div", { class: "meta-row", style: "margin:0 0 10px" },
    h("span", { class: "stat-chip" }, `${s.size} keys`), h("span", { class: "stat-chip" }, `${s.buckets} buckets`),
    h("span", { class: "stat-chip" }, `load ${s.loadFactor.toFixed(2)}`), h("span", { class: "stat-chip" }, `${s.collisions} collisions`),
    h("span", { class: "stat-chip" }, `longest chain ${s.longestChain}`)));
  wrap.append(h("div", { class: "buckets" }, table.buckets.map((b) =>
    h("div", { class: "bucket" + (b.keys.length ? " used" : "") + (b.keys.length > 1 ? " chain" : "") + (b.index === hitBucket ? " hit" : ""), dataset: { bucket: b.index } },
      h("i", null, h("span", null, `#${b.index}`), b.keys.length > 1 ? h("span", null, `×${b.keys.length}`) : null),
      b.keys.map((k) => h("div", { class: "k", title: k }, k))))));
  return wrap;
}
