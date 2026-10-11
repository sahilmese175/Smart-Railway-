// modules/stations.js : the station registry, which trains stop where, and the "find station index" tool.   (Member 1)
import { api } from "../core/api.js";
import { registerPage } from "../core/registry.js";
import { h, toast, plural, confirmDialog, notifyChanged, debounce } from "../core/ui.js";
import { openTrainSheet } from "../components/train-sheet.js";

let selected = "";

registerPage({
  id: "stations", path: "stations", label: "Stations", order: 40,
  async render(root, ctx) {
    if (ctx.query.get("select")) selected = ctx.query.get("select");
    window.addEventListener("railway:changed", () => this.render(root, ctx), { signal: ctx.signal, once: true });
    const [{ stations, hash }, { trains }] = await Promise.all([api.stations(), api.trains()]);
    const trainByNo = new Map(trains.map((t) => [t.trainNo, t]));
    if (!stations.some((s) => s.name === selected)) selected = "";

    const grid = h("div", { class: "station-grid" });
    const side = h("div", { class: "stack" });
    let filter = "";

    function drawGrid() {
      const f = filter.toLowerCase();
      const shown = stations.filter((s) => s.name.toLowerCase().includes(f));
      grid.replaceChildren(...(shown.length ? shown.map((s) =>
        h("button", { class: "station", type: "button", "aria-pressed": String(s.name === selected), onclick: () => { selected = s.name; drawGrid(); drawSide(); } },
          h("b", null, s.name), h("span", null, s.trainCount ? plural(s.trainCount, "train") + " stop here" : "No trains stop here")))
        : [h("div", { class: "empty", style: "grid-column:1/-1" }, h("h3", null, "No station matches"), h("p", null, "Check the spelling, or add it on the right."))]));
    }

    function drawSide() {
      const sel = stations.find((s) => s.name === selected);
      const detail = h("div", { class: "card" });
      if (!sel) {
        detail.append(h("h3", { style: "font-size:22px;margin-bottom:6px" }, "Select a station"), h("p", { class: "muted" }, "See which trains stop there, in the order the inverted index stores them."));
      } else {
        detail.append(h("h3", { style: "font-size:26px" }, sel.name),
          h("p", { class: "muted", style: "margin:4px 0 12px" }, sel.trainCount ? `${plural(sel.trainCount, "train")} stop here` : "No train stops here yet. You can delete this station."));
        for (const no of sel.trains) {
          const t = trainByNo.get(no); if (!t) continue;
          const stop = t.stops.find((x) => x.station === sel.name);
          detail.append(h("button", { class: "btn ghost", type: "button", style: "display:flex;width:100%;justify-content:space-between;margin-bottom:6px;text-align:left",
            onclick: () => openTrainSheet(no).catch((e) => toast(e.message, true)) },
            h("span", null, `${no}  ${t.name}`), h("span", { class: "num" }, stop ? (stop.index === 0 ? stop.departure : stop.arrival) : "")));
        }
        detail.append(h("div", { style: "margin-top:10px" }, h("button", { class: "btn danger small", type: "button", disabled: sel.trainCount > 0, title: sel.trainCount ? "Remove it from every route first" : "", onclick: async () => {
          if (!(await confirmDialog(`Delete station ${sel.name}?`))) return;
          try { await api.deleteStation(sel.name); toast(`${sel.name} deleted`); selected = ""; notifyChanged(); } catch (e) { toast(e.message, true); }
        } }, "Delete station")));
      }

      // find station index
      const trainSel = h("select", { "aria-label": "Train" }, trains.map((t) => h("option", { value: t.trainNo }, `${t.trainNo}  ${t.name}`)));
      const stationIn = h("input", { type: "text", list: "idx-stations", placeholder: "Station name", value: selected, "aria-label": "Station" });
      const out = h("div", { "aria-live": "polite", style: "margin-top:10px" });
      const idxForm = h("form", { class: "card stack", onsubmit: async (e) => {
        e.preventDefault();
        try {
          const r = await api.stationIndex(trainSel.value, stationIn.value);
          out.replaceChildren(r.found
            ? h("div", { class: "ok-box" }, `${r.station} is stop ${r.index} of ${r.stationCount - 1} on train ${r.trainNo} (counting from 0).`)
            : h("div", { class: "error-box" }, `Train ${r.trainNo} does not stop at ${r.station}.`));
        } catch (err) { out.replaceChildren(h("div", { class: "error-box" }, err.message)); }
      } },
        h("h3", { style: "font-size:22px" }, "Where is a station on a route?"),
        h("p", { class: "hint" }, "Returns the position used as sourceIndex / destinationIndex in a booking."),
        h("label", { class: "field" }, "Train", trainSel), h("label", { class: "field" }, "Station", stationIn),
        h("datalist", { id: "idx-stations" }, stations.map((s) => h("option", { value: s.name }))),
        h("button", { class: "btn", type: "submit", disabled: !trains.length }, "Find index"), out);

      // add station
      const nameIn = h("input", { type: "text", placeholder: "New station name", maxlength: 60, "aria-label": "New station name" });
      const addForm = h("form", { class: "card stack", onsubmit: async (e) => {
        e.preventDefault();
        try { await api.addStation(nameIn.value); toast(`${nameIn.value.trim()} added`); selected = nameIn.value.trim(); notifyChanged(); }
        catch (err) { toast(err.message, true); }
      } }, h("h3", { style: "font-size:22px" }, "Add a station"), nameIn, h("button", { class: "btn accent", type: "submit" }, "Add station"));

      side.replaceChildren(detail, idxForm, addForm);
    }

    const filterIn = h("input", { type: "search", placeholder: "Filter stations", "aria-label": "Filter stations", oninput: debounce((e) => { filter = e.target.value; drawGrid(); }, 120) });
    root.replaceChildren(
      h("header", { class: "page-head" }, h("div", null, h("h1", null, "Stations"),
        h("p", null, "Names go through a hash table, so \"does Karjat exist?\" is one lookup. Each station also keeps a list of the trains that stop there."))),
      h("div", { class: "toolbar" }, h("span", { class: "chip" }, `${hash.size} stations in ${hash.buckets} buckets`),
        h("span", { class: "chip plain" }, `load ${hash.loadFactor.toFixed(2)}`), h("span", { class: "chip plain" }, `${hash.collisions} collisions`)),
      h("div", { class: "two-col" }, h("div", null, h("label", { class: "field", style: "margin-bottom:14px" }, "Filter", filterIn), grid), side));
    drawGrid(); drawSide();
  },
});
