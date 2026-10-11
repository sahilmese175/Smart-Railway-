// core/console.js : the "Data structure console" drawer at the bottom of the page.
// Every API call that returns a "trace" array is printed here, so you can SEE which
// data structure operation (hash lookup, array shift, merge sort ...) the C++ code did.
import { h } from "./ui.js";

let count = 0;
const $ = (id) => document.getElementById(id);

export function initConsole() {
  const box = $("console"), btn = $("console-toggle");
  btn.addEventListener("click", () => {
    const open = box.classList.toggle("collapsed") === false;
    btn.setAttribute("aria-expanded", String(open));
  });
  $("console-clear").addEventListener("click", () => {
    $("console-log").replaceChildren(h("div", { class: "log-empty" }, "Nothing yet. Do something and the data structure steps appear here."));
    count = 0; $("console-count").textContent = "0";
  });
  $("console-log").append(h("div", { class: "log-empty" }, "Nothing yet. Do something and the data structure steps appear here."));
}

export function logCall(method, url, status, trace) {
  if (!trace || !trace.length) return;
  const log = $("console-log");
  const empty = log.querySelector(".log-empty");
  if (empty) empty.remove();
  const bad = status >= 400;
  const group = h("div", { class: "log-group" },
    h("div", { class: "log-req" }, `${method} ${url}`, h("span", { class: "st" + (bad ? " bad" : "") }, String(status))),
    trace.map((line) => h("div", { class: "log-line" }, line)));
  log.append(group);
  log.scrollTop = log.scrollHeight;
  count += 1;
  $("console-count").textContent = String(count);
  $("console-hint").textContent = trace[trace.length - 1];
}
