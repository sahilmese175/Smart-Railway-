// modules/team.js : who owns which module, what is finished, and how to add yours.
import { api } from "../core/api.js";
import { registerPage } from "../core/registry.js";
import { h } from "../core/ui.js";

const SNIPPET = `// frontend/js/modules/booking.js
import { registerPage } from "../core/registry.js";
import { api } from "../core/api.js";
import { h } from "../core/ui.js";

registerPage({
  id: "booking", path: "booking", label: "Bookings", order: 60,
  async render(root, ctx) {
    const data = await api.get("/api/bookings");      // your C++ endpoint
    root.replaceChildren(h("h1", null, "Bookings"), h("p", null, data.count + " bookings"));
  },
});`;

registerPage({
  id: "team", path: "team", label: "Team", order: 90,
  async render(root) {
    const meta = await api.meta();
    root.replaceChildren(
      h("header", { class: "page-head" }, h("div", null, h("h1", null, "Team and modules"),
        h("p", null, "Four modules, one app. Each member works in their own folder and plugs in through a router and a page registry. Nothing else needs to change."))),
      h("div", { class: "modules" }, meta.modules.map((m) =>
        h("article", { class: "module " + m.status },
          h("div", { style: "display:flex;gap:8px;flex-wrap:wrap" },
            h("span", { class: "chip" + (m.status === "live" ? " go" : " plain") }, m.status === "live" ? "Live" : "Planned"), h("span", { class: "chip plain" }, m.owner)),
          h("h3", null, m.title), h("p", null, m.summary),
          h("span", { class: "hint" }, m.routeCount ? `${m.routeCount} endpoints` : "No endpoints yet")))),

      h("h2", { class: "section" }, "Add your module"),
      h("div", { class: "two-col" },
        h("ol", { class: "steps" },
          h("li", null, "Put your data structures and logic in backend/<your module>/. Keep to the shared structs in backend/common/contract.h."),
          h("li", null, "Write backend/api/routes_<module>.cpp with a register function. Call router.addModule(...) and set the status to \"live\"."),
          h("li", null, "Add your .cpp files to the Makefile, and call your register function in backend/main.cpp."),
          h("li", null, "Create frontend/js/modules/<module>.js with registerPage(...) and import it in frontend/js/modules/index.js."),
          h("li", null, "Run make test, then open a pull request. CONTRIBUTING.md has the branch names and checklist.")),
        h("pre", { class: "code" }, SNIPPET)),

      h("h2", { class: "section" }, "Endpoints", h("small", null, `${meta.routes.length} registered`)),
      h("div", { class: "card flush table-wrap" }, h("table", { class: "data route-list" },
        h("thead", null, h("tr", null, ["Endpoint", "Module", "What it does"].map((t) => h("th", { scope: "col" }, t)))),
        h("tbody", null, meta.routes.map((r) => h("tr", null,
          h("td", null, h("span", { class: "method " + r.method }, r.method), r.pattern), h("td", null, r.module), h("td", { class: "muted" }, r.doc)))))));
  },
});
