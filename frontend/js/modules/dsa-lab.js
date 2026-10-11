// modules/dsa-lab.js : see the data structures work. Hash buckets, three ways to search, four ways to sort.   (Member 1)
// Good for the viva: every number on this page comes from the C++ code, not from JavaScript.
import { api } from "../core/api.js";
import { registerPage } from "../core/registry.js";
import { h, toast } from "../core/ui.js";
import { renderBuckets } from "../components/train-sheet.js";

const reduceMotion = () => window.matchMedia("(prefers-reduced-motion: reduce)").matches;
const sleep = (ms) => new Promise((r) => setTimeout(r, reduceMotion() ? 0 : ms));
const BIG_O = {
  insertion: ["Insertion sort", "ins", "O(n²) comparisons, but very fast when the data is almost sorted"],
  merge: ["Merge sort", "mer", "O(n log n) always, and stable (equal items keep their order)"],
  quick: ["Quick sort", "qui", "O(n log n) on average, O(n²) in the worst case"],
  heap: ["Heap sort", "hea", "O(n log n) always, sorts in place"],
};

registerPage({
  id: "lab", path: "lab", label: "DSA lab", order: 50,
  async render(root, ctx) {
    const { trains } = await api.trains();
    root.replaceChildren(h("header", { class: "page-head" }, h("div", null, h("h1", null, "DSA lab"),
      h("p", null, "Watch the structures behind the app. Everything here is measured by the C++ server."))));
    const grid = h("div", { class: "lab-grid" });
    root.append(grid);
    grid.append(hashCard(trains), searchCard(trains), sortCard());
  },
});

// ---------------------------------------------------------------- 1. hash table
function hashCard(trains) {
  const view = h("div", { "aria-live": "polite" });
  const note = h("p", { class: "hint", style: "margin-top:10px" });
  let which = "trains";
  const trainSel = h("select", { "aria-label": "Train for its stop table", hidden: true, onchange: () => load() }, trains.map((t) => h("option", { value: t.trainNo }, `${t.trainNo} ${t.name}`)));
  const seg = h("div", { class: "seg", role: "group", "aria-label": "Which hash table" },
    [["trains", "Trains"], ["stations", "Stations"], ["stops", "Stops of one train"]].map(([k, l]) =>
      h("button", { type: "button", "aria-pressed": String(k === which), onclick: () => { which = k; [...seg.children].forEach((b) => b.setAttribute("aria-pressed", String(b.textContent === l))); trainSel.hidden = k !== "stops"; load(); } }, l)));
  const noIn = h("input", { type: "number", placeholder: "Train number", min: 1, "aria-label": "Train number to look up", value: trains[0] ? trains[0].trainNo : "" });
  const lookup = h("form", { class: "toolbar", style: "margin:14px 0 0", onsubmit: async (e) => {
    e.preventDefault();
    try {
      const r = await api.lookup(noIn.value);
      which = "trains"; [...seg.children].forEach((b) => b.setAttribute("aria-pressed", String(b.textContent === "Trains"))); trainSel.hidden = true;
      await load(r.hash.bucket);
      const hit = view.querySelector(".bucket.hit");
      if (hit) hit.scrollIntoView({ block: "nearest", behavior: reduceMotion() ? "auto" : "smooth" });
      note.textContent = r.found
        ? `Train ${r.trainNo} hashes to bucket #${r.hash.bucket} and was found after ${r.hash.probes} ${r.hash.probes === 1 ? "probe" : "probes"}.`
        : `Train ${r.trainNo} hashes to bucket #${r.hash.bucket}. The bucket does not contain it, so it is not in the table (${r.hash.probes} probes).`;
    } catch (err) { toast(err.message, true); }
  } },
    h("label", { class: "field fixed" }, "Look up a train", noIn), h("button", { class: "btn", type: "submit" }, "Find bucket"));

  async function load(hitBucket = -1) {
    try {
      const { table } = await api.hashDump(which === "stops" ? { table: "stops", train: trainSel.value } : { table: which });
      view.replaceChildren(h("p", { class: "mono", style: "margin-bottom:10px" }, table.name), renderBuckets(table, hitBucket));
      note.textContent = "";
    } catch (e) { view.replaceChildren(h("div", { class: "error-box" }, e.message)); }
  }
  load();
  return h("section", { class: "card lab-card" },
    h("h3", null, "Hash table with chaining"),
    h("p", null, "A key is turned into a bucket number. Keys that land in the same bucket form a chain (highlighted yellow). When the table gets too full it doubles in size and every key is placed again."),
    h("div", { class: "toolbar", style: "margin-bottom:0" }, seg, trainSel), view, note, lookup);
}

// ---------------------------------------------------------------- 2. searching
function searchCard(trains) {
  const lanes = h("div", { class: "lanes", "aria-live": "polite" });
  const noIn = h("input", { type: "number", min: 1, value: trains.length ? trains[trains.length - 1].trainNo : "", "aria-label": "Train number" });
  let running = 0;

  function lane(title, sub, values) {
    const cells = values.map((v) => h("div", { class: "cell" }, v));
    const head = h("div", { class: "lane-head" }, title, h("span", null, sub));
    return { el: h("div", { class: "lane" }, head, h("div", { class: "cells" }, cells)), cells, head, sub: head.querySelector("span") };
  }

  async function run(e) {
    e && e.preventDefault();
    const token = ++running;
    let r;
    try { r = await api.lookup(noIn.value); } catch (err) { toast(err.message, true); return; }
    const n = r.arraySize;
    const lin = lane("Linear search", "walks the array from the first element", r.linear.order);
    const bin = lane("Binary search", "needs a sorted array, halves the range each time", r.binary.order);
    const hs = lane("Hash table", "jumps straight to one bucket", [`bucket #${r.hash.bucket} of ${r.hash.buckets}`]);
    lanes.replaceChildren(lin.el, bin.el, hs.el);
    const lastLin = r.linear.steps - 1;
    for (let i = 0; i <= lastLin; i++) {
      if (token !== running) return;
      lin.cells[i].classList.add(i === lastLin && r.found ? "found" : "seen");
      lin.sub.textContent = `${i + 1} of ${n} steps`;
      await sleep(180);
    }
    lin.sub.textContent = r.found ? `found after ${r.linear.steps} ${r.linear.steps === 1 ? "step" : "steps"}` : `not found after ${r.linear.steps} steps`;
    const probes = r.binary.probes;
    for (let i = 0; i < probes.length; i++) {
      if (token !== running) return;
      bin.cells[probes[i]].classList.add(i === probes.length - 1 && r.found ? "found" : "probe");
      bin.sub.textContent = `${i + 1} ${i === 0 ? "probe" : "probes"}`;
      await sleep(320);
    }
    bin.sub.textContent = r.found ? `found after ${probes.length} ${probes.length === 1 ? "probe" : "probes"}` : `not found after ${probes.length} probes`;
    hs.cells[0].classList.add(r.found ? "found" : "probe");
    hs.sub.textContent = r.found ? `found after ${r.hash.probes} ${r.hash.probes === 1 ? "probe" : "probes"} in the bucket` : "not found: the bucket does not hold it";
  }
  setTimeout(run, 0);
  return h("section", { class: "card lab-card" },
    h("h3", null, "Three ways to find a train"),
    h("p", null, `Same question, three structures: where is this train? With only ${trains.length} trains the gap is small. Add more and linear search keeps growing while the hash table stays flat.`),
    h("form", { class: "toolbar", style: "margin-bottom:0", onsubmit: run }, h("label", { class: "field fixed" }, "Train number", noIn), h("button", { class: "btn", type: "submit" }, "Run search")),
    lanes);
}

// ---------------------------------------------------------------- 3. sorting benchmark
function sortCard() {
  const out = h("div", { "aria-live": "polite" });
  const nOut = h("output", { class: "mono" }, "500");
  const range = h("input", { type: "range", min: 10, max: 5000, step: 10, value: 500, "aria-label": "Number of random items", oninput: () => { nOut.textContent = range.value; } });
  async function run(e) {
    e && e.preventDefault();
    try {
      const r = await api.benchmark(range.value);
      const max = Math.max(...r.rows.map((x) => x.comparisons), 1);
      const bars = h("div", { class: "bars" }, r.rows.map((row) => {
        const [name, cls] = BIG_O[row.algorithm];
        return h("div", { class: "bar-row" }, h("span", null, name),
          h("div", { class: "bar-track" }, h("div", { class: "bar-fill " + cls, style: `width:${(row.comparisons / max) * 100}%` })),
          h("span", { class: "bar-val" }, `${row.comparisons.toLocaleString("en-IN")} comparisons`));
      }));
      const table = h("div", { class: "card flush table-wrap bench-table" }, h("table", { class: "data" },
        h("thead", null, h("tr", null, ["Algorithm", "Comparisons", "Moves", "Time", "Sorted?", "Good to know"].map((t) => h("th", { scope: "col" }, t)))),
        h("tbody", null, r.rows.map((row) => h("tr", { style: "cursor:default" },
          h("td", null, BIG_O[row.algorithm][0]), h("td", { class: "num" }, row.comparisons.toLocaleString("en-IN")), h("td", { class: "num" }, row.moves.toLocaleString("en-IN")),
          h("td", { class: "num" }, row.micros >= 1000 ? `${(row.micros / 1000).toFixed(2)} ms` : `${row.micros.toFixed(0)} µs`),
          h("td", null, row.sorted ? h("span", { class: "chip go" }, "yes") : h("span", { class: "chip warn" }, "NO")),
          h("td", { class: "muted" }, BIG_O[row.algorithm][2]))))));
      out.replaceChildren(bars, table);
    } catch (err) { out.replaceChildren(h("div", { class: "error-box" }, err.message)); }
  }
  setTimeout(run, 0);
  return h("section", { class: "card lab-card" },
    h("h3", null, "Sorting race"),
    h("p", null, "The server fills an array with random train-like records and sorts the same data with all four algorithms. Comparisons are counted inside the sort, so the numbers do not depend on how fast your computer is."),
    h("form", { class: "toolbar", style: "margin-bottom:0", onsubmit: run },
      h("label", { class: "field grow" }, h("span", null, "Items to sort: ", nOut), range), h("button", { class: "btn", type: "submit" }, "Run again")),
    out);
}
