// modules/trains.js : table of all trains with sorting, filtering, add / edit / delete.   (Member 1)
import { api } from "../core/api.js";
import { registerPage } from "../core/registry.js";
import { h, toast, fmtDuration, rupees, dayNote, debounce } from "../core/ui.js";
import { openTrainSheet } from "../components/train-sheet.js";
import { openTrainForm } from "../components/train-form.js";

const ALGOS = [["merge", "Merge"], ["quick", "Quick"], ["insertion", "Insertion"], ["heap", "Heap"]];
const ALGO_NAMES = { merge: "Merge sort", quick: "Quick sort", insertion: "Insertion sort", heap: "Heap sort" };
const COLUMNS = [   // [sort key or null, label, align]
  ["trainNo", "Train"], ["name", "Name"], [null, "Route"], ["departure", "Departs"], ["arrival", "Arrives"],
  ["duration", "Duration"], ["stops", "Stops"], ["distance", "Distance"], ["fare", "Full fare"], [null, "Seats"],
];
const state = { sort: "trainNo", order: "asc", algo: "merge", filter: "" };   // remembered while you move between pages

registerPage({
  id: "trains", path: "trains", label: "Trains", order: 20,
  async render(root, ctx) {
    window.addEventListener("railway:changed", () => load(), { signal: ctx.signal });
    const tableHolder = h("div", { class: "card flush table-wrap" });
    const statsChip = h("span", { class: "chip", "aria-live": "polite" });
    const countText = h("span", { class: "hint" });
    let trains = [];

    const search = h("input", { type: "search", placeholder: "Train number, name or station", value: state.filter, "aria-label": "Filter trains",
      oninput: debounce((e) => { state.filter = e.target.value; draw(); }) });
    const algoSeg = h("div", { class: "seg", role: "group", "aria-label": "Sorting algorithm" },
      ALGOS.map(([key, label]) => h("button", { type: "button", "aria-pressed": String(state.algo === key), onclick: () => { state.algo = key; refreshSeg(); load(); } }, label)));
    const refreshSeg = () => [...algoSeg.children].forEach((b, i) => b.setAttribute("aria-pressed", String(ALGOS[i][0] === state.algo)));

    root.replaceChildren(
      h("header", { class: "page-head" },
        h("div", null, h("h1", null, "Trains"),
          h("p", null, "Search is instant (hash table). Sorting runs in C++ with the algorithm you pick, so you can compare how much work each one does.")),
        h("div", { class: "page-actions" }, h("button", { class: "btn accent", type: "button", onclick: () => openTrainForm() }, "Add train"))),
      h("div", { class: "toolbar" },
        h("label", { class: "field grow" }, "Filter", search),
        h("div", { class: "field fixed" }, h("span", null, "Sort with"), algoSeg)),
      h("div", { class: "toolbar", style: "margin:-6px 0 12px" }, statsChip, countText),
      tableHolder);

    async function load() {
      try {
        const r = await api.trains({ sort: state.sort, order: state.order, algo: state.algo });
        trains = r.trains;
        const s = r.sortStats;
        statsChip.textContent = `${ALGO_NAMES[s.algorithm]}: ${s.comparisons} comparisons, ${s.moves} moves, ${s.micros.toFixed(1)} µs`;
        draw();
      } catch (e) { tableHolder.replaceChildren(h("div", { class: "error-box", style: "margin:16px" }, e.message)); }
    }

    function draw() {
      const f = state.filter.trim().toLowerCase();
      const shown = !f ? trains : trains.filter((t) => String(t.trainNo).includes(f) || t.name.toLowerCase().includes(f) || t.stops.some((s) => s.station.toLowerCase().includes(f)));
      countText.textContent = `Showing ${shown.length} of ${trains.length} trains`;
      if (!shown.length) {
        tableHolder.replaceChildren(h("div", { class: "empty" },
          h("h3", null, trains.length ? "No train matches that filter" : "No trains yet"),
          h("p", null, trains.length ? "Try a train number, a name, or a station on the route." : "Use Add train to enter the first one.")));
        return;
      }
      const head = h("tr", null, COLUMNS.map(([key, label]) => {
        const th = h("th", { scope: "col", "aria-sort": key && state.sort === key ? (state.order === "asc" ? "ascending" : "descending") : false });
        if (!key) { th.append(label); return th; }
        const arrow = state.sort === key ? (state.order === "asc" ? "▲" : "▼") : "";
        th.append(h("button", { type: "button", onclick: () => {
          if (state.sort === key) state.order = state.order === "asc" ? "desc" : "asc"; else { state.sort = key; state.order = "asc"; }
          load();
        } }, label, h("span", { class: "arrow", "aria-hidden": "true" }, arrow)));
        return th;
      }));
      const body = shown.map((t) => {
        const open = () => openTrainSheet(t.trainNo).catch((e) => toast(e.message, true));
        return h("tr", { tabindex: 0, onclick: open, onkeydown: (e) => { if (e.key === "Enter") open(); }, "aria-label": `Train ${t.trainNo} ${t.name}` },
          h("td", { class: "train-no" }, t.trainNo),
          h("td", null, t.name),
          h("td", null, `${t.source} → ${t.destination}`),
          h("td", { class: "num" }, t.departure),
          h("td", { class: "num" }, t.arrival + dayNote(t.arrivalDay)),
          h("td", { class: "num" }, fmtDuration(t.durationMin)),
          h("td", { class: "num" }, t.stopCount),
          h("td", { class: "num" }, `${t.totalDistanceKm} km`),
          h("td", { class: "num" }, rupees(t.totalFare)),
          h("td", { class: "num" }, t.totalSeats));
      });
      tableHolder.replaceChildren(h("table", { class: "data" }, h("caption", { class: "sr" }, "All trains"), h("thead", null, head), h("tbody", null, body)));
    }
    await load();
  },
});
