// modules/search.js : find trains between two stations.   (Member 1)
// Shows the route positions (srcIdx / dstIdx) that Member 2 and 3 will use for bookings and seat segments.
import { api } from "../core/api.js";
import { registerPage } from "../core/registry.js";
import { h, toast, fmtDuration, rupees, dayNote } from "../core/ui.js";
import { openTrainSheet } from "../components/train-sheet.js";

const SORTS = [["departure", "Departure time"], ["arrival", "Arrival time"], ["duration", "Journey time"], ["fare", "Fare"], ["distance", "Distance"], ["stops", "Number of stops"], ["trainNo", "Train number"]];

registerPage({
  id: "search", path: "search", label: "Journey search", order: 30,
  async render(root, ctx) {
    let names = [];
    try { names = (await api.stations()).stations.map((s) => s.name); } catch { /* datalist is optional */ }
    const from = h("input", { type: "text", list: "search-stations", placeholder: "e.g. Pune", autocomplete: "off", value: ctx.query.get("from") || "", "aria-label": "From" });
    const to = h("input", { type: "text", list: "search-stations", placeholder: "e.g. Mumbai", autocomplete: "off", value: ctx.query.get("to") || "", "aria-label": "To" });
    const sort = h("select", { "aria-label": "Sort by" }, SORTS.map(([v, l]) => h("option", { value: v }, l)));
    const order = h("select", { "aria-label": "Order" }, h("option", { value: "asc" }, "Lowest first"), h("option", { value: "desc" }, "Highest first"));
    const algo = h("select", { "aria-label": "Sorting algorithm" }, [["merge", "Merge sort"], ["quick", "Quick sort"], ["insertion", "Insertion sort"], ["heap", "Heap sort"]].map(([v, l]) => h("option", { value: v }, l)));
    const results = h("div", { "aria-live": "polite" });
    let searched = false;

    const form = h("form", { class: "card", onsubmit: (e) => { e.preventDefault(); run(); } },
      h("div", { class: "search-form" },
        h("label", { class: "field" }, "From", from),
        h("button", { class: "btn ghost swap", type: "button", "aria-label": "Swap stations", onclick: () => { [from.value, to.value] = [to.value, from.value]; if (searched) run(); } }, "⇄"),
        h("label", { class: "field" }, "To", to)),
      h("div", { class: "search-opts" },
        h("label", { class: "field" }, "Sort by", sort), h("label", { class: "field" }, "Order", order), h("label", { class: "field" }, "Algorithm", algo),
        h("button", { class: "btn accent", type: "submit" }, "Find trains")),
      h("datalist", { id: "search-stations" }, names.map((n) => h("option", { value: n }))));
    for (const el of [sort, order, algo]) el.addEventListener("change", () => { if (searched) run(); });

    root.replaceChildren(
      h("header", { class: "page-head" }, h("div", null, h("h1", null, "Journey search"),
        h("p", null, "Pick a boarding and a destination station. Only trains that stop at the first station before the second are listed."))),
      form, h("div", { style: "height:22px" }), results);

    async function run() {
      searched = true;
      history.replaceState(null, "", "#/search?" + new URLSearchParams({ from: from.value, to: to.value }));
      try {
        const r = await api.search({ from: from.value, to: to.value, sort: sort.value, order: order.value, algo: algo.value });
        draw(r);
      } catch (e) { results.replaceChildren(h("div", { class: "error-box", role: "alert" }, e.message)); }
    }

    function draw(r) {
      const s = r.sortStats;
      const summary = h("div", { class: "toolbar", style: "margin-bottom:12px" },
        h("span", { class: "chip go" }, `${r.count} ${r.count === 1 ? "train runs" : "trains run"} ${r.from} to ${r.to}`),
        h("span", { class: "chip plain" }, `${r.candidates} stop at ${r.from}`),
        h("span", { class: "chip plain" }, `${s.algorithm} sort: ${s.comparisons} comparisons`));
      if (!r.count) {
        results.replaceChildren(summary, h("div", { class: "card empty" }, h("h3", null, `No train goes from ${r.from} to ${r.to}`),
          h("p", null, r.candidates ? `${r.candidates} trains stop at ${r.from}, but none reach ${r.to} afterwards.` : `No train stops at ${r.from}.`)));
        return;
      }
      const card = h("div", { class: "card flush" }, r.results.map((m) => {
        const open = () => openTrainSheet(m.trainNo, { highlightFrom: m.from, highlightTo: m.to }).catch((e) => toast(e.message, true));
        const strip = h("div", { class: "strip" });
        m.stopsBetween.forEach((st, i) => {
          if (i) strip.append(h("span", { class: "bar" }));
          strip.append(h("span", { class: "pt" + (i === m.stopsBetween.length - 1 ? " end" : "") }, st));
        });
        return h("div", { class: "result", tabindex: 0, role: "button", "aria-label": `${m.name}, ${m.departure} to ${m.arrival}, ${rupees(m.fare)}`, onclick: open, onkeydown: (e) => { if (e.key === "Enter") open(); } },
          h("div", null,
            h("h3", null, m.name, " ", h("span", { class: "muted num" }, m.trainNo)),
            h("div", { class: "times" }, m.departure, "→", m.arrival + dayNote(m.arrivalDay), h("small", null, fmtDuration(m.durationMin)))),
          h("div", { class: "fare" }, h("b", null, rupees(m.fare)), h("span", { class: "muted" }, `${m.distanceKm} km`)),
          strip,
          h("div", { class: "meta-row", style: "grid-column:1/-1;margin:0" },
            h("span", { class: "stat-chip" }, `route positions ${m.srcIdx} → ${m.dstIdx}`),
            h("span", { class: "stat-chip" }, `seat segments ${m.srcIdx}..${m.dstIdx - 1}`),
            h("span", { class: "stat-chip" }, `${m.dstIdx - m.srcIdx} hops`)));
      }));
      results.replaceChildren(summary, card);
    }
    if (from.value && to.value) run();
  },
});
