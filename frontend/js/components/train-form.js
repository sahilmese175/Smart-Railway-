// components/train-form.js : the add / edit dialog for a train and its route.
//   openTrainForm()               -> add a new train
//   openTrainForm({ train })      -> edit an existing train (train number is fixed)
import { api } from "../core/api.js";
import { h, openDialog, toast, rupees, notifyChanged } from "../core/ui.js";

const blankStop = () => ({ station: "", arrival: "", departure: "", distanceKm: 0 });

export async function openTrainForm({ train = null } = {}) {
  const editing = !!train;
  let stations = [];
  try { stations = (await api.stations()).stations.map((s) => s.name); } catch { /* datalist is optional */ }

  // working copy of the stops; the first stop has no arrival and the last has no departure
  const rows = editing
    ? train.stops.map((s, i) => ({ station: s.station, arrival: i === 0 ? "" : s.arrival, departure: i === train.stops.length - 1 ? "" : s.departure, distanceKm: s.distanceKm }))
    : [blankStop(), blankStop()];

  const f = {
    no: h("input", { type: "number", min: 1, required: true, value: train ? train.trainNo : "", disabled: editing, "aria-label": "Train number" }),
    name: h("input", { type: "text", maxlength: 49, required: true, value: train ? train.name : "", "aria-label": "Train name" }),
    seats: h("input", { type: "number", min: 1, value: train ? train.totalSeats : 120, "aria-label": "Seats" }),
    fare: h("input", { type: "number", min: 0, step: "0.05", value: train ? train.farePerKm : 1.5, "aria-label": "Fare per km" }),
  };
  const listId = "station-options";
  const options = h("datalist", { id: listId }, stations.map((s) => h("option", { value: s })));
  const editor = h("div", { class: "stops-editor" });
  const summary = h("p", { class: "hint", "aria-live": "polite" });
  const errorBox = h("div", { class: "error-box", role: "alert", hidden: true });

  function updateSummary() {
    const km = rows.length ? Number(rows[rows.length - 1].distanceKm) || 0 : 0;
    summary.textContent = `${rows.length} stops, ${km} km, full-journey fare ${rupees(Math.round(km * (Number(f.fare.value) || 0)))}. Distance is counted from the first station.`;
  }

  function draw() {
    const last = rows.length - 1;
    editor.replaceChildren(
      h("div", { class: "stop-row head" }, h("span"), h("span", null, "Station"), h("span", null, "Arrives"), h("span", null, "Leaves"), h("span", null, "Km from start"), h("span")),
      ...rows.map((r, i) => {
        const input = (key, type, extra = {}) => h("input", { type, value: r[key], "aria-label": `Stop ${i + 1} ${key}`, ...extra,
          oninput: (e) => { r[key] = e.target.value; if (key === "distanceKm") updateSummary(); } });
        return h("div", { class: "stop-row" },
          h("span", { class: "idx" }, i),
          h("span", { class: "st-name" }, input("station", "text", { list: listId, placeholder: "Station name", autocomplete: "off" })),
          input("arrival", "time", { disabled: i === 0 }),
          input("departure", "time", { disabled: i === last }),
          input("distanceKm", "number", { min: 0, disabled: i === 0 }),
          h("span", { class: "stop-actions" },
            h("button", { class: "icon-btn", type: "button", "aria-label": `Move stop ${i + 1} up`, disabled: i === 0, onclick: () => move(i, -1) }, "↑"),
            h("button", { class: "icon-btn", type: "button", "aria-label": `Move stop ${i + 1} down`, disabled: i === last, onclick: () => move(i, 1) }, "↓"),
            h("button", { class: "icon-btn", type: "button", "aria-label": `Remove stop ${i + 1}`, disabled: rows.length <= 2, onclick: () => { rows.splice(i, 1); draw(); } }, "✕")));
      }));
    updateSummary();
  }
  function move(i, d) { [rows[i], rows[i + d]] = [rows[i + d], rows[i]]; draw(); }
  rows[0].distanceKm = 0;
  f.fare.addEventListener("input", updateSummary);

  const saveBtn = h("button", { class: "btn accent", type: "submit" }, editing ? "Save changes" : "Add train");
  const form = h("form", { novalidate: true },
    h("div", { class: "form-grid" },
      h("label", { class: "field" }, "Train number", f.no),
      h("label", { class: "field" }, "Train name", f.name),
      h("label", { class: "field" }, "Seats", f.seats),
      h("label", { class: "field" }, "Fare per km (₹)", f.fare)),
    editor,
    h("div", { style: "margin-top:10px;display:flex;gap:12px;align-items:center;flex-wrap:wrap" },
      h("button", { class: "btn ghost small", type: "button", onclick: () => { rows.push(blankStop()); draw(); } }, "Add stop"), summary),
    options, h("div", { style: "margin-top:14px" }, errorBox));

  let ref;
  form.addEventListener("submit", async (e) => {
    e.preventDefault();
    errorBox.hidden = true;
    const last = rows.length - 1;
    const body = {
      name: f.name.value, totalSeats: Number(f.seats.value), farePerKm: Number(f.fare.value),
      stops: rows.map((r, i) => ({ station: r.station, arrival: i === 0 ? "" : r.arrival, departure: i === last ? "" : r.departure, distanceKm: i === 0 ? 0 : Number(r.distanceKm) })),
    };
    if (!editing) body.trainNo = Number(f.no.value);
    saveBtn.disabled = true;
    try {
      const res = await api.saveTrain(editing ? train.trainNo : 0, body);
      toast(res.message || "Saved");
      ref.close();
      notifyChanged();
    } catch (err) {
      errorBox.textContent = err.message; errorBox.hidden = false;
      errorBox.scrollIntoView({ block: "nearest" });
    } finally { saveBtn.disabled = false; }
  });

  const footer = [h("button", { class: "btn ghost", type: "button", onclick: () => ref.close() }, "Cancel"), saveBtn];
  saveBtn.setAttribute("form", "train-form");
  form.id = "train-form";
  ref = openDialog({
    title: editing ? `Edit ${train.name}` : "Add a train",
    subtitle: editing ? `Train ${train.trainNo}. The train number cannot be changed.` : "Enter the route in order, from the first station to the last.",
    body: form, footer,
  });
  ref.dialog.style.maxWidth = "min(900px, 96vw)";
  draw();
  (editing ? f.name : f.no).focus();
}
