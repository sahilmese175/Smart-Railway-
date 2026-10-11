// modules/overview.js : landing page. Numbers, departure board and the network map.   (Member 1)
import { api } from "../core/api.js";
import { registerPage } from "../core/registry.js";
import { h, toast, confirmDialog, fmtDuration, dayNote, notifyChanged } from "../core/ui.js";
import { openTrainSheet } from "../components/train-sheet.js";
import { openTrainForm } from "../components/train-form.js";
import { drawNetworkMap } from "../components/network-map.js";

registerPage({
  id: "overview", path: "overview", label: "Overview", order: 10,
  async render(root, ctx) {
    const reload = () => this.render(root, ctx).then(() => {});
    root.replaceChildren();
    const [stats, list, stationData] = await Promise.all([api.stats(), api.trains({ sort: "departure", algo: "merge" }), api.stations()]);
    window.addEventListener("railway:changed", () => { root.replaceChildren(); this.render(root, ctx); }, { signal: ctx.signal, once: true });

    root.append(
      h("header", { class: "page-head" },
        h("div", null, h("h1", null, "Network overview"),
          h("p", null, "Every train and station below lives in data structures written by hand in C++: linked lists, arrays and hash tables.")),
        h("div", { class: "page-actions" },
          h("button", { class: "btn ghost", type: "button", onclick: async () => {
            if (!(await confirmDialog("Replace all trains and stations with the sample data? Your changes will be lost.", "Reset data"))) return;
            try { await api.reset(); toast("Sample data restored"); notifyChanged(); } catch (e) { toast(e.message, true); }
          } }, "Reset sample data"),
          h("button", { class: "btn accent", type: "button", onclick: () => openTrainForm() }, "Add train"))));

    const fmt = (n) => Number(n).toLocaleString("en-IN");
    const stat = (value, label) => h("div", { class: "stat" }, h("b", null, value), h("span", null, label));
    root.append(h("div", { class: "stats" },
      stat(stats.trains, "trains"),
      stat(stats.stations, "stations"),
      stat(fmt(stats.totalSeats), "seats across all trains"),
      stats.farthest ? stat(`${fmt(stats.farthest.distanceKm)} km`, `longest run: ${stats.farthest.name}`) : null,
      stats.busiestStation ? stat(stats.busiestStation.trains, `trains stop at ${stats.busiestStation.name}, the busiest station`) : null));

    // ------------------------------------------------------------ departure board
    root.append(h("h2", { class: "section" }, "Departures", h("small", null, `sorted by first departure with merge sort, ${list.sortStats.comparisons} comparisons`)));
    let map;
    const grid = h("div", { class: "board-grid", role: "table", "aria-label": "Departures" });
    grid.append(h("div", { class: "board-head", role: "row" }, ["Train", "Name", "Route", "Departs", "Arrives", "Stops"].map((t) => h("span", { role: "columnheader" }, t))));
    if (!list.trains.length) {
      root.append(h("div", { class: "card empty" }, h("h3", null, "No trains yet"), h("p", null, "Add a train to see it on the board and the map.")));
    } else {
      for (const t of list.trains) {
        const open = () => openTrainSheet(t.trainNo).catch((e) => toast(e.message, true));
        const row = h("div", { class: "board-row", role: "row", onclick: open, onmouseover: () => map && map.highlight(t.trainNo) },
          h("span", { role: "cell" }, h("button", { type: "button", class: "board-btn", onclick: (e) => { e.stopPropagation(); open(); }, onfocus: () => map && map.highlight(t.trainNo), "aria-label": `Open train ${t.trainNo} ${t.name}` }, t.trainNo)),
          h("span", { role: "cell" }, t.name),
          h("span", { class: "sub", role: "cell" }, `${t.source} → ${t.destination}`),
          h("span", { class: "t", role: "cell" }, t.departure),
          h("span", { class: "t", role: "cell" }, t.arrival, t.arrivalDay ? h("small", null, `+${t.arrivalDay}d`) : null),
          h("span", { role: "cell" }, t.stopCount));
        grid.append(row);
      }
      const board = h("div", { class: "board", onmouseleave: () => map && map.highlight(null) }, grid,
        h("p", { class: "board-note" }, "Point at a train to light up its route on the map. Select one for its stops, fares and data structures."));
      root.append(board);
    }

    // ------------------------------------------------------------ map
    root.append(h("h2", { class: "section" }, "Network map", h("small", null, "scroll to zoom, drag to pan")));
    const wrap = h("div", { class: "map-wrap" });
    const zoomSel = h("select", { "aria-label": "Zoom to a route", onchange: () => map.focus(zoomSel.value ? Number(zoomSel.value) : null) },
      h("option", { value: "" }, "Whole network"), list.trains.map((t) => h("option", { value: t.trainNo }, `${t.trainNo} ${t.name}`)));
    const busiest = [...stationData.stations].sort((a, b) => b.trainCount - a.trainCount || a.name.localeCompare(b.name)).slice(0, 7);
    const top = busiest.length ? busiest[0].trainCount : 1;
    const side = h("aside", { class: "map-side" },
      h("label", { class: "field" }, "Zoom to a route", zoomSel),
      h("div", null, h("h3", null, "Busiest stations"),
        h("p", { class: "hint", style: "margin:2px 0 8px" }, "Trains per station, from the inverted index."),
        busiest.map((s) => h("div", { class: "rank-row" }, h("span", null, s.name), h("div", { class: "bar-track" }, h("div", { class: "bar-fill", style: `width:${(s.trainCount / top) * 100}%` })), h("b", null, s.trainCount)))),
      h("p", { class: "hint" }, `${stats.stations} stations. Thicker lines carry more trains. Click a station to open it.`));
    root.append(h("div", { class: "card map-card" }, h("div", { class: "map-body" }, wrap, side)));
    map = drawNetworkMap(wrap, list.trains, stationData.stations, { onStation: (name) => ctx.go("stations?select=" + encodeURIComponent(name)) });
  },
});
