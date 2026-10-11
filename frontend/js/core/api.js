// core/api.js : every call to the C++ server goes through here.
import { logCall } from "./console.js";

export class ApiError extends Error {
  constructor(message, status) { super(message); this.status = status; }
}

async function call(method, url, body) {
  let res;
  try {
    res = await fetch(url, {
      method,
      headers: body ? { "Content-Type": "application/json" } : undefined,
      body: body ? JSON.stringify(body) : undefined,
    });
  } catch (e) {
    throw new ApiError("Cannot reach the server. Is railway_server still running?", 0);
  }
  let data = null;
  try { data = await res.json(); } catch { /* not JSON */ }
  logCall(method, url, res.status, data && data.trace);
  if (!res.ok || !data || data.ok === false) {
    throw new ApiError((data && data.error) || `Server answered ${res.status}`, res.status);
  }
  return data;
}

export const qs = (obj) => {
  const p = new URLSearchParams();
  for (const [k, v] of Object.entries(obj)) if (v !== "" && v != null) p.set(k, v);
  const s = p.toString();
  return s ? "?" + s : "";
};

export const api = {
  get: (url) => call("GET", url),
  post: (url, body) => call("POST", url, body),
  put: (url, body) => call("PUT", url, body),
  del: (url) => call("DELETE", url),

  // ---- Member 1 endpoints (see docs/API.md) ----
  trains: (params = {}) => call("GET", "/api/trains" + qs(params)),
  train: (no) => call("GET", `/api/trains/${no}`),
  saveTrain: (no, body) => (no ? call("PUT", `/api/trains/${no}`, body) : call("POST", "/api/trains", body)),
  deleteTrain: (no) => call("DELETE", `/api/trains/${no}`),
  stations: () => call("GET", "/api/stations"),
  addStation: (name) => call("POST", "/api/stations", { name }),
  deleteStation: (name) => call("DELETE", `/api/stations/${encodeURIComponent(name)}`),
  stationIndex: (train, station) => call("GET", "/api/station-index" + qs({ train, station })),
  validate: (train, from, to) => call("GET", "/api/validate" + qs({ train, from, to })),
  search: (params) => call("GET", "/api/search" + qs(params)),
  stats: () => call("GET", "/api/stats"),
  hashDump: (params) => call("GET", "/api/dsa/hash" + qs(params)),
  lookup: (train) => call("GET", "/api/dsa/lookup" + qs({ train })),
  benchmark: (n) => call("GET", "/api/dsa/sort-benchmark" + qs({ n })),
  reset: () => call("POST", "/api/admin/reset"),
  meta: () => call("GET", "/api/meta"),
};
