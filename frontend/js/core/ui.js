// core/ui.js : tiny helpers shared by every page. No framework, just DOM.

/** h("div", {class: "card", onclick: fn}, "text", childNode, ...)  ->  HTMLElement */
export function h(tag, attrs, ...children) {
  const el = document.createElement(tag);
  if (attrs) {
    for (const [k, v] of Object.entries(attrs)) {
      if (v === false || v == null) continue;
      if (k === "class") el.className = v;
      else if (k === "html") el.innerHTML = v;
      else if (k.startsWith("on") && typeof v === "function") el.addEventListener(k.slice(2).toLowerCase(), v);
      else if (k === "dataset") Object.assign(el.dataset, v);
      else if (v === true) el.setAttribute(k, "");
      else el.setAttribute(k, v);
    }
  }
  append(el, children);
  return el;
}

function append(el, children) {
  for (const c of children.flat(Infinity)) {
    if (c == null || c === false) continue;
    el.append(c.nodeType ? c : document.createTextNode(String(c)));
  }
}

export function svg(tag, attrs, ...children) {
  const el = document.createElementNS("http://www.w3.org/2000/svg", tag);
  if (attrs) for (const [k, v] of Object.entries(attrs)) if (v != null && v !== false) el.setAttribute(k, v);
  append(el, children);
  return el;
}

export function clear(el) { while (el.firstChild) el.removeChild(el.firstChild); return el; }

export function toast(message, isError = false) {
  const box = document.getElementById("toasts");
  const t = h("div", { class: "toast" + (isError ? " err" : ""), role: isError ? "alert" : "status" }, message);
  box.append(t);
  setTimeout(() => t.remove(), isError ? 6000 : 3200);
}

// ---------------------------------------------------------------- formatting
export function fmtDuration(min) {
  const hrs = Math.floor(min / 60), m = min % 60;
  return hrs ? `${hrs}h ${String(m).padStart(2, "0")}m` : `${m}m`;
}
export const rupees = (n) => "₹" + Number(n).toLocaleString("en-IN");
export const plural = (n, one, many) => `${n} ${n === 1 ? one : many || one + "s"}`;
export const dayNote = (d) => (d > 0 ? ` +${d}d` : "");

/** Debounce for search boxes. */
export function debounce(fn, ms = 180) {
  let t;
  return (...a) => { clearTimeout(t); t = setTimeout(() => fn(...a), ms); };
}

// ---------------------------------------------------------------- dialogs
/** Opens a <dialog>. Returns {dialog, close}. Pass a `sheet:true` option for the right-hand panel. */
export function openDialog({ title, subtitle, body, footer, sheet = false, onClose }) {
  const dlg = h("dialog", { class: sheet ? "sheet" : "", "aria-label": title });
  const close = () => dlg.close();
  const head = h("div", { class: "dlg-head" },
    h("div", null, h("h2", null, title), subtitle ? h("p", null, subtitle) : null),
    h("button", { class: "icon-btn", type: "button", "aria-label": "Close", onclick: close }, "✕"));
  dlg.append(head, h("div", { class: "dlg-body" }, body));
  if (footer) dlg.append(h("div", { class: "dlg-foot" }, footer));
  dlg.addEventListener("close", () => { dlg.remove(); if (onClose) onClose(); });
  dlg.addEventListener("click", (e) => { if (e.target === dlg) close(); });   // click on the backdrop
  document.body.append(dlg);
  dlg.showModal();
  return { dialog: dlg, close };
}

export function confirmDialog(message, confirmLabel = "Delete") {
  return new Promise((resolve) => {
    let answered = false;
    const done = (v) => { answered = true; resolve(v); ref.close(); };
    const ref = openDialog({
      title: "Please confirm", body: h("p", null, message),
      footer: [h("button", { class: "btn ghost", type: "button", onclick: () => done(false) }, "Cancel"),
               h("button", { class: "btn danger", type: "button", onclick: () => done(true) }, confirmLabel)],
      onClose: () => { if (!answered) resolve(false); },
    });
  });
}

/** Tell every open page that trains or stations changed, so lists reload themselves. */
export const notifyChanged = () => window.dispatchEvent(new Event("railway:changed"));
