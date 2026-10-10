/* SmartRailway - Train & Route UI. Plain JS, no build step. All data comes from the C++ backend (/api/...). */
const $ = (s, r = document) => r.querySelector(s);
const esc = s => String(s).replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
const money = n => '₹' + Number(n).toLocaleString('en-IN');
const dur = m => `${Math.floor(m / 60)}h ${String(m % 60).padStart(2, '0')}m`;

async function api(path, opts) {
  let r;
  try { r = await fetch('/api' + path, opts); } catch { throw new Error('Cannot reach the backend. Is ./railway running?'); }
  const d = await r.json().catch(() => ({}));
  if (!r.ok) throw new Error(d.error || 'Request failed.');
  return d;
}
const send = (method, path, obj) => api(path, { method, body: new URLSearchParams(obj) });

let toastTimer;
function toast(msg, bad) {
  const t = $('#toast'); t.textContent = msg; t.className = 'show' + (bad ? ' bad' : '');
  clearTimeout(toastTimer); toastTimer = setTimeout(() => t.className = '', 3200);
}

const state = { trains: [], stations: [], q: '', sort: 'number', order: 'asc', open: new Set(), find: { s: '', d: '', sort: 'departure', order: 'asc', res: null, err: '' }, picked: '' };
const app = $('#app');

/* ---------- shared pieces ---------- */
function routeHtml(stops, from = 0, to = stops.length - 1, showFare = true) {
  return `<ol class="route">` + stops.map((s, i) => {
    const cls = [i < from || i > to ? 'off' : '', i === from ? 'start' : '', i === to ? 'end' : ''].join(' ');
    const first = s.arrival || s.departure, second = s.arrival && s.departure && s.arrival !== s.departure ? s.departure : '';
    return `<li class="${cls}"><span class="t">${first}${second ? `<small>${second}</small>` : ''}</span><span class="dot"></span>
      <span><span class="plate">${esc(s.station)}</span></span><span class="rf">${showFare ? money(s.fare) : ''}</span></li>`;
  }).join('') + `</ol>`;
}

/* ---------- Trains view ---------- */
async function loadTrains() { state.trains = await api(`/trains?sort=${state.sort}&order=${state.order}`); }

function trainRow(t) {
  const first = t.stops[0], last = t.stops[t.stops.length - 1], open = state.open.has('t' + t.trainNo);
  return `<article class="row ${open ? 'open' : ''}" data-id="t${t.trainNo}">
    <button class="row-head" aria-expanded="${open}">
      <span class="plate no">${t.trainNo}</span>
      <span><span class="tname">${esc(t.name)}</span><span class="cap">${t.stops.length} stations · ${t.totalSeats} seats</span></span>
      <span><span class="times">${first.departure}<i>→</i>${last.arrival}</span><span class="cap">${esc(first.station)} to ${esc(last.station)}</span></span>
      <span></span><span class="fare">${money(t.totalFare)}<span class="cap">end to end</span></span><span class="chev"></span>
    </button>
    <div class="detail">${routeHtml(t.stops)}
      <div class="detail-actions"><span class="muted">Route is stored as a linked list, segments 0 to ${t.stops.length - 2}.</span>
        <button class="btn sm" data-edit="${t.trainNo}">Edit train</button><button class="btn sm danger" data-del="${t.trainNo}">Delete</button></div>
    </div></article>`;
}

function renderTrains() {
  const q = state.q.trim().toLowerCase();
  const list = !q ? state.trains : state.trains.filter(t => String(t.trainNo).includes(q) || t.name.toLowerCase().includes(q) || t.stops.some(s => s.station.toLowerCase().includes(q)));
  app.innerHTML = `
    <div class="page-head"><div><h1>Trains</h1><p>${state.trains.length} trains on file. Open a train to see its route.</p></div>
      <button class="btn primary" id="addTrain">Add train</button></div>
    <div class="toolbar">
      <input type="search" id="q" placeholder="Filter by train number, name or station" value="${esc(state.q)}" aria-label="Filter trains">
      <select id="sort" aria-label="Sort trains by">${[['number', 'Train number'], ['name', 'Name'], ['departure', 'Departure time'], ['fare', 'Fare'], ['stops', 'Number of stops'], ['seats', 'Seats']].map(([v, l]) => `<option value="${v}" ${state.sort === v ? 'selected' : ''}>Sort: ${l}</option>`).join('')}</select>
      <select id="order" aria-label="Sort order"><option value="asc" ${state.order === 'asc' ? 'selected' : ''}>Ascending</option><option value="desc" ${state.order === 'desc' ? 'selected' : ''}>Descending</option></select>
    </div>
    <div id="list">${list.length ? list.map(trainRow).join('') : `<div class="empty"><h3>${state.trains.length ? 'No trains match that filter' : 'No trains yet'}</h3><p class="muted">${state.trains.length ? 'Try a different number, name or station.' : 'Add the first train to build your timetable.'}</p></div>`}</div>`;
  $('#addTrain').onclick = () => openForm();
  $('#q').oninput = e => { state.q = e.target.value; const pos = e.target.selectionStart; renderTrains(); const el = $('#q'); el.focus(); el.setSelectionRange(pos, pos); };
  $('#sort').onchange = async e => { state.sort = e.target.value; await loadTrains(); renderTrains(); };
  $('#order').onchange = async e => { state.order = e.target.value; await loadTrains(); renderTrains(); };
}

/* ---------- Find view ---------- */
function resultRow(m, i) {
  const t = m.train, open = state.open.has('r' + t.trainNo);
  return `<article class="row ${open ? 'open' : ''}" data-id="r${t.trainNo}">
    <button class="row-head res" aria-expanded="${open}">
      <span class="plate no">${t.trainNo}</span>
      <span><span class="tname">${esc(t.name)}</span><span class="cap">${m.to - m.from} segment${m.to - m.from > 1 ? "s" : ""} travelled</span></span>
      <span><span class="times">${m.departure}<i>→</i>${m.arrival}</span></span>
      <span class="dur">${dur(m.duration)}</span><span class="fare">${money(m.fare)}</span><span class="chev"></span>
    </button>
    <div class="detail">${routeHtml(t.stops, m.from, m.to)}
      <div class="detail-actions"><span class="muted">${esc(state.find.s)} is stop ${m.from}, ${esc(state.find.d)} is stop ${m.to}. This journey uses segments ${m.from} to ${m.to - 1}.</span></div></div></article>`;
}

function renderFind() {
  const f = state.find;
  app.innerHTML = `
    <div class="page-head"><div><h1>Find trains</h1><p>Trains that run from one station to another, in that direction.</p></div></div>
    <form class="finder" id="finder">
      <label>From<input id="src" list="stlist" value="${esc(f.s)}" placeholder="Pune" autocomplete="off"></label>
      <button type="button" class="btn swap" id="swap" aria-label="Swap stations">⇄</button>
      <label>To<input id="dst" list="stlist" value="${esc(f.d)}" placeholder="Mumbai" autocomplete="off"></label>
      <button class="btn go">Search trains</button>
      <datalist id="stlist">${state.stations.map(s => `<option value="${esc(s.name)}">`).join('')}</datalist>
    </form>
    <div id="results">${resultsHtml()}</div>`;
  $('#src').oninput = e => f.s = e.target.value; $('#dst').oninput = e => f.d = e.target.value;
  $('#swap').onclick = () => { [f.s, f.d] = [f.d, f.s]; $('#src').value = f.s; $('#dst').value = f.d; if (f.s && f.d) runSearch(); };
  $('#finder').onsubmit = e => { e.preventDefault(); runSearch(); };
  bindResultControls();
}
function resultsHtml() {
  const f = state.find;
  if (f.err) return `<div class="empty"><h3>${esc(f.err)}</h3><p class="muted">Check the station names or pick from the suggestions.</p></div>`;
  if (!f.res) return `<div class="empty"><h3>Where to?</h3><p class="muted">Enter a source and destination to see matching trains, their times and fares.</p></div>`;
  if (!f.res.length) return `<div class="empty"><h3>No direct train from ${esc(f.s)} to ${esc(f.d)}</h3><p class="muted">Trains may run the other way. Try the swap button.</p></div>`;
  return `<div class="result-meta"><h2>${f.res.length} train${f.res.length > 1 ? 's' : ''}, ${esc(f.s)} to ${esc(f.d)}</h2>
    <span><select id="rsort" aria-label="Sort results">${[['departure', 'Departure time'], ['fare', 'Fare'], ['duration', 'Duration'], ['number', 'Train number']].map(([v, l]) => `<option value="${v}" ${f.sort === v ? 'selected' : ''}>Sort: ${l}</option>`).join('')}</select></span></div>
    ${f.res.map(resultRow).join('')}`;
}
function bindResultControls() { const s = $('#rsort'); if (s) s.onchange = e => { state.find.sort = e.target.value; runSearch(); }; }
async function runSearch() {
  const f = state.find; f.err = ''; f.res = null;
  try { f.res = await api(`/search?source=${encodeURIComponent(f.s.trim())}&destination=${encodeURIComponent(f.d.trim())}&sort=${f.sort}&order=${f.order}`); }
  catch (e) { f.err = e.message; }
  $('#results').innerHTML = resultsHtml(); bindResultControls();
}

/* ---------- Stations view ---------- */
function renderStations() {
  const sel = state.stations.find(s => s.name === state.picked);
  app.innerHTML = `
    <div class="page-head"><div><h1>Stations</h1><p>${state.stations.length} stations across ${state.trains.length} routes. The number is how many trains stop there.</p></div></div>
    <div class="chips">${state.stations.map(s => `<button class="chip" data-st="${esc(s.name)}" aria-pressed="${s.name === state.picked}"><span class="plate">${esc(s.name)}</span><b>${s.trains.length}</b></button>`).join('') || '<div class="empty" style="width:100%"><h3>No stations yet</h3><p class="muted">Stations appear when you add a train.</p></div>'}</div>
    ${sel ? `<div class="panel"><h2>Trains stopping at ${esc(sel.name)}</h2><div style="margin-top:12px">${sel.trains.map(n => { const t = state.trains.find(x => x.trainNo === n); return t ? `<p style="margin:8px 0"><button class="chip" data-go="${n}"><span class="plate">${n}</span> ${esc(t.name)}</button></p>` : ''; }).join('')}
      <button class="btn sm" id="fromHere">Search trains from ${esc(sel.name)}</button></div></div>` : ''}`;
  app.querySelectorAll('[data-st]').forEach(b => b.onclick = () => { state.picked = b.dataset.st; renderStations(); });
  app.querySelectorAll('[data-go]').forEach(b => b.onclick = () => { state.q = b.dataset.go; state.open.add('t' + b.dataset.go); location.hash = '#trains'; });
  const fh = $('#fromHere'); if (fh) fh.onclick = () => { state.find.s = state.picked; state.find.d = ''; state.find.res = null; state.find.err = ''; location.hash = '#find'; };
}

/* ---------- DSA inspector ---------- */
function barsHtml(lens) { const mx = Math.max(1, ...lens); return `<div class="bars" role="img" aria-label="Chain length per bucket">${lens.map((l, i) => `<span class="${l ? '' : 'zero'}" style="height:${l ? 14 + 86 * l / mx : 3}%" title="Bucket ${i}: ${l} item${l === 1 ? '' : 's'}"></span>`).join('')}</div>`; }
async function renderInspect() {
  const d = await api('/inspect');
  const card = (title, h, note) => `<div class="panel" style="margin:0"><h3>${title}</h3>
    <div class="stat"><div>${h.size}<span>items</span></div><div>${h.buckets}<span>buckets</span></div><div>${(h.size / h.buckets).toFixed(2)}<span>load factor</span></div><div>${Math.max(0, ...h.lengths)}<span>longest chain</span></div></div>
    ${barsHtml(h.lengths)}<p class="muted">${note}</p></div>`;
  app.innerHTML = `
    <div class="page-head"><div><h1>DSA inspector</h1><p>See what the C++ data structures are doing. Each bar is one hash bucket; taller means more items chained there.</p></div></div>
    <div class="grid2">${card('Train table', d.trains, 'Key: train number. Value: pointer to the Train.')}${card('Station index', d.stations, 'Key: station name (lower case). Value: list of trains stopping there.')}</div>
    <div class="panel"><h3>Find a station on a route</h3><p class="muted">Calls <code>findStationIndex(trainNo, station)</code>. Member 3 uses this index to pick segments.</p>
      <form class="lookup" id="lk"><input id="lkT" placeholder="Train number" inputmode="numeric" style="width:150px"><input id="lkS" placeholder="Station" list="stl2"><datalist id="stl2">${state.stations.map(s => `<option value="${esc(s.name)}">`).join('')}</datalist><button class="btn primary">Find index</button></form>
      <div class="answer" id="lkA" aria-live="polite"></div></div>
    <div class="panel"><h3>Where each structure is used</h3><table><thead><tr><th>Feature</th><th>Structure</th><th>Time</th></tr></thead><tbody>
      <tr><td>Search a train by number</td><td>Hash table (chaining, auto-resize)</td><td>O(1) average</td></tr>
      <tr><td>Stations on a route, in order</td><td>Singly linked list</td><td>O(1) append, O(n) walk</td></tr>
      <tr><td>Trains stopping at a station</td><td>Hash table of dynamic arrays (inverted index)</td><td>O(1) lookup</td></tr>
      <tr><td>Find trains between two stations</td><td>Index lookup, then check stop order</td><td>O(k · stops), k = trains at source</td></tr>
      <tr><td>Display and sort trains</td><td>Dynamic array and merge sort</td><td>O(n log n), stable</td></tr></tbody></table></div>`;
  $('#lk').onsubmit = async e => { e.preventDefault(); const a = $('#lkA');
    try { const r = await api(`/station-index?train=${encodeURIComponent($('#lkT').value.trim())}&station=${encodeURIComponent($('#lkS').value.trim())}`);
      a.textContent = r.index < 0 ? 'That train does not stop at this station (-1).' : `Stop index ${r.index} of ${r.stops - 1}`; } catch (er) { a.innerHTML = `<span class="err">${esc(er.message)}</span>`; } };
}

/* ---------- Add / edit form ---------- */
const dlg = $('#dlg'); let editNo = null, rows = [];
function blankRow() { return { station: '', arrival: '', departure: '', fare: '' }; }
function openForm(t) {
  editNo = t ? t.trainNo : null;
  $('#dlgTitle').textContent = t ? `Edit train ${t.trainNo}` : 'Add train';
  $('#fNo').value = t ? t.trainNo : ''; $('#fNo').disabled = !!t; $('#fName').value = t ? t.name : ''; $('#fSeats').value = t ? t.totalSeats : '';
  rows = t ? t.stops.map(s => ({ ...s })) : [blankRow(), blankRow()];
  $('#formErr').textContent = ''; drawRows(); dlg.showModal(); (t ? $('#fName') : $('#fNo')).focus();
}
function drawRows() {
  $('#stopRows').innerHTML = rows.map((r, i) => `<div class="stop-row" data-i="${i}">
    <input data-k="station" value="${esc(r.station)}" placeholder="Station ${i + 1}" list="stl3" aria-label="Station ${i + 1}">
    <input data-k="arrival" type="time" value="${r.arrival}" ${i === 0 ? 'disabled' : ''} aria-label="Arrival at station ${i + 1}">
    <input data-k="departure" type="time" value="${r.departure}" ${i === rows.length - 1 ? 'disabled' : ''} aria-label="Departure from station ${i + 1}">
    <input data-k="fare" inputmode="numeric" value="${r.fare}" placeholder="Fare ₹" aria-label="Fare to station ${i + 1}">
    <span class="mini"><button type="button" data-mv="-1" aria-label="Move up" ${i === 0 ? 'disabled' : ''}>↑</button><button type="button" data-mv="1" aria-label="Move down" ${i === rows.length - 1 ? 'disabled' : ''}>↓</button><button type="button" data-rm aria-label="Remove station" ${rows.length <= 2 ? 'disabled' : ''}>✕</button></span></div>`).join('') +
    `<datalist id="stl3">${state.stations.map(s => `<option value="${esc(s.name)}">`).join('')}</datalist>`;
}
$('#stopRows').addEventListener('input', e => { const k = e.target.dataset.k; if (k) rows[+e.target.closest('.stop-row').dataset.i][k] = e.target.value; });
$('#stopRows').addEventListener('click', e => {
  const b = e.target.closest('button'); if (!b) return; const i = +b.closest('.stop-row').dataset.i;
  if (b.hasAttribute('data-rm')) rows.splice(i, 1);
  else { const j = i + +b.dataset.mv; [rows[i], rows[j]] = [rows[j], rows[i]]; }
  drawRows();
});
$('#addStop').onclick = () => { rows.push(blankRow()); drawRows(); const l = $('#stopRows').querySelectorAll('.stop-row'); l[l.length - 1].querySelector('input').focus(); };
$('#cancelBtn').onclick = () => dlg.close();
$('#trainForm').onsubmit = async e => {
  e.preventDefault();
  const stops = rows.map((r, i) => [r.station.trim(), i === 0 ? '' : r.arrival, i === rows.length - 1 ? '' : r.departure, String(r.fare).trim()].join('~')).join(';');
  const body = { name: $('#fName').value.trim(), totalSeats: $('#fSeats').value.trim(), stops };
  try {
    if (editNo) await send('PUT', '/trains/' + editNo, body); else await send('POST', '/trains', { ...body, trainNo: $('#fNo').value.trim() });
    dlg.close(); toast(editNo ? 'Train updated' : 'Train added'); await refresh();
  } catch (er) { $('#formErr').textContent = er.message; }
};

/* ---------- routing ---------- */
async function refresh() { [state.stations] = await Promise.all([api('/stations'), loadTrains()]); render(); }
function render() {
  const v = (location.hash || '#trains').slice(1);
  document.querySelectorAll('#nav a').forEach(a => a.removeAttribute('aria-current')); const cur = $(`#nav a[href="#${v}"]`); if (cur) cur.setAttribute('aria-current', 'page');
  ({ find: renderFind, stations: renderStations, inspect: renderInspect }[v] || renderTrains)();
}
window.addEventListener('hashchange', async () => { try { await refresh(); } catch (e) { app.innerHTML = `<div class="empty"><h3>${esc(e.message)}</h3></div>`; } });

app.addEventListener('click', async e => {
  const head = e.target.closest('.row-head');
  if (head) { const row = head.parentElement, id = row.dataset.id; row.classList.toggle('open'); const o = row.classList.contains('open'); head.setAttribute('aria-expanded', o); o ? state.open.add(id) : state.open.delete(id); return; }
  const ed = e.target.closest('[data-edit]'); if (ed) return openForm(state.trains.find(t => t.trainNo == ed.dataset.edit));
  const del = e.target.closest('[data-del]');
  if (del && confirm(`Delete train ${del.dataset.del}? This cannot be undone.`)) {
    try { await send('DELETE', '/trains/' + del.dataset.del); toast('Train deleted'); await refresh(); } catch (er) { toast(er.message, true); }
  }
});

refresh().catch(e => { app.innerHTML = `<div class="empty"><h3>${esc(e.message)}</h3><p class="muted">Start the server from the backend folder with <code>make run</code>, then reload.</p></div>`; });
