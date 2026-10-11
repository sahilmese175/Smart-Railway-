// core/registry.js : how a module adds a page to the app.
//
//   registerPage({
//     id: "booking",              // unique
//     path: "booking",            // URL becomes  #/booking
//     label: "Bookings",          // text in the left navigation
//     order: 40,                  // position in the navigation (lower = higher up)
//     render(root, ctx) { ... }   // fill the <main> element; return nothing
//   });
//
// ctx = { params: ["12001"], query: URLSearchParams, go(path), refreshCounts() }
const pages = [];
export function registerPage(page) {
  if (pages.some((p) => p.id === page.id)) throw new Error(`Page '${page.id}' is registered twice`);
  pages.push(page);
}
export const allPages = () => [...pages].sort((a, b) => (a.order ?? 100) - (b.order ?? 100));
