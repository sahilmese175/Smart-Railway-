import { api } from "../core/api.js";
import { registerPage } from "../core/registry.js";
import { confirmDialog, h, rupees, toast } from "../core/ui.js";

async function renderBookings(root, passengerQuery = "") {
  let passengers, allPassengers, trains, bookings;
  try {
    [passengers, allPassengers, trains, bookings] = await Promise.all([
      api.passengers(passengerQuery), api.passengers(), api.bookingTrains(), api.bookings(),
    ]);
  } catch (error) {
    root.replaceChildren(h("div", { class: "error-box" }, error.message));
    return;
  }

  const passengerList = passengers.passengers;
  const allPassengerList = allPassengers.passengers;
  const demoTrains = trains.trains;
  const bookingList = bookings.bookings;
  const firstTrain = demoTrains[0];
  const pnrResult = h("div", { "aria-live": "polite" });

  const addForm = h("form", { class: "card stack", onsubmit: async (event) => {
    event.preventDefault();
    const form = event.currentTarget;
    try {
      await api.addPassenger({ name: form.elements.name.value, age: Number(form.elements.age.value) });
      toast("Passenger added");
      await renderBookings(root, passengerQuery);
    } catch (error) { toast(error.message, true); }
  } },
    h("h2", null, "Add passenger"),
    h("label", { class: "field" }, "Name", h("input", { name: "name", maxlength: 49, required: true })),
    h("label", { class: "field" }, "Age", h("input", { name: "age", type: "number", min: 1, max: 120, required: true })),
    h("button", { class: "btn", type: "submit" }, "Add passenger"));

  const passengerCards = passengerList.map((passenger) => {
    const editForm = h("form", { class: "stack", onsubmit: async (event) => {
      event.preventDefault();
      const form = event.currentTarget;
      try {
        await api.updatePassenger(passenger.passengerId, {
          name: form.elements.name.value, age: Number(form.elements.age.value),
        });
        toast("Passenger updated");
        await renderBookings(root, passengerQuery);
      } catch (error) { toast(error.message, true); }
    } },
      h("label", { class: "field" }, "Name", h("input", { name: "name", value: passenger.name, maxlength: 49, required: true })),
      h("label", { class: "field" }, "Age", h("input", { name: "age", value: passenger.age, type: "number", min: 1, max: 120, required: true })),
      h("div", { style: "display:flex;gap:8px;flex-wrap:wrap" },
        h("button", { class: "btn ghost small", type: "submit" }, "Save"),
        h("button", { class: "btn danger small", type: "button", onclick: async () => {
          if (!(await confirmDialog(`Delete passenger ${passenger.name}?`))) return;
          try {
            await api.deletePassenger(passenger.passengerId);
            toast("Passenger deleted");
            await renderBookings(root, passengerQuery);
          } catch (error) { toast(error.message, true); }
        } }, "Delete")));
    return h("article", { class: "card stack" },
      h("div", { class: "toolbar" }, h("strong", null, `Passenger #${passenger.passengerId}`)),
      editForm);
  });

  const passengerSearch = h("form", { class: "toolbar", onsubmit: async (event) => {
    event.preventDefault();
    await renderBookings(root, event.currentTarget.elements.query.value);
  } },
    h("label", { class: "field" }, "Search passengers",
      h("input", { name: "query", type: "search", value: passengerQuery, placeholder: "Name contains..." })),
    h("button", { class: "btn ghost", type: "submit" }, "Search"));

  const passengerSelect = h("select", { name: "passengerId", required: true },
    allPassengerList.map((p) => h("option", { value: p.passengerId }, `${p.name} (#${p.passengerId})`)));
  const trainSelect = h("select", { name: "trainNo", required: true },
    demoTrains.map((train) => h("option", { value: train.trainNo }, `${train.trainNo} · ${train.name}`)));
  const sourceSelect = h("select", { name: "source", required: true },
    firstTrain ? firstTrain.stops.slice(0, -1).map((stop) => h("option", { value: stop }, stop)) : []);
  const destinationSelect = h("select", { name: "destination", required: true },
    firstTrain ? firstTrain.stops.slice(1).map((stop) => h("option", { value: stop }, stop)) : []);
  const bookingForm = h("form", { class: "card stack", onsubmit: async (event) => {
    event.preventDefault();
    const form = event.currentTarget;
    try {
      const result = await api.createBooking({
        passengerId: Number(form.elements.passengerId.value),
        trainNo: Number(form.elements.trainNo.value),
        source: form.elements.source.value,
        destination: form.elements.destination.value,
      });
      toast(`Booking confirmed · PNR ${result.booking.pnr}`);
      await renderBookings(root, passengerQuery);
    } catch (error) { toast(error.message, true); }
  } },
    h("h2", null, "Create booking"),
    h("label", { class: "field" }, "Passenger", passengerSelect),
    h("label", { class: "field" }, "Train", trainSelect),
    h("label", { class: "field" }, "Source", sourceSelect),
    h("label", { class: "field" }, "Destination", destinationSelect),
    h("button", { class: "btn accent", type: "submit", disabled: !allPassengerList.length || !demoTrains.length },
      allPassengerList.length ? "Book ticket" : "Add a passenger first"));

  const enquiryForm = h("form", { class: "toolbar", onsubmit: async (event) => {
    event.preventDefault();
    const pnr = event.currentTarget.elements.pnr.value;
    try {
      const { booking } = await api.getBooking(pnr);
      pnrResult.replaceChildren(
        h("article", { class: "card" }, bookingSummary(booking),
          booking.status === "CONFIRMED"
            ? h("button", { class: "btn danger small", type: "button", onclick: () => cancelTicket(booking.pnr, root) }, "Cancel ticket")
            : null));
    } catch (error) { pnrResult.replaceChildren(h("div", { class: "error-box" }, error.message)); }
  } },
    h("label", { class: "field" }, "PNR enquiry",
      h("input", { name: "pnr", inputmode: "numeric", pattern: "[0-9]+", required: true, placeholder: "e.g. 10001" })),
    h("button", { class: "btn ghost", type: "submit" }, "Search PNR"));

  const rows = bookingList.length ? bookingList.map((booking) =>
    h("tr", null,
      h("td", null, booking.pnr),
      h("td", null, booking.passenger),
      h("td", null, `${booking.trainNo} · ${booking.source} → ${booking.destination}`),
      h("td", null, booking.seat),
      h("td", null, rupees(booking.fare)),
      h("td", null, h("span", { class: "chip" + (booking.status === "CONFIRMED" ? " go" : " plain") }, booking.status)),
      h("td", null, booking.status === "CONFIRMED"
        ? h("button", { class: "btn danger small", type: "button", onclick: () => cancelTicket(booking.pnr, root) }, "Cancel")
        : "—")))
    : [h("tr", null, h("td", { colspan: "7", class: "muted" }, "No bookings yet."))];

  root.replaceChildren(
    h("header", { class: "page-head" }, h("div", null,
      h("h1", null, "Passengers and bookings"),
      h("p", null, "Manage passenger records, book a seat, calculate the fare and look up or cancel a ticket by PNR."))),
    h("div", { class: "toolbar" }, demoTrains.map((train) =>
      h("span", { class: "chip" }, `${train.trainNo} · ${train.stops.join(" → ")} · ${train.totalSeats} seats`))),
    h("div", { class: "two-col" },
      h("div", { class: "stack" }, addForm, passengerSearch, h("h2", { class: "section" }, "Passengers"),
        passengerCards.length ? h("div", { class: "stack" }, passengerCards)
          : h("div", { class: "empty" }, h("p", null, passengerQuery ? "No passengers match this search." : "No passengers yet. Add a passenger to start booking."))),
      h("div", { class: "stack" }, bookingForm,
        h("section", { class: "card stack" }, enquiryForm, pnrResult))),
    h("h2", { class: "section" }, "Bookings"),
    h("div", { class: "card flush table-wrap" }, h("table", { class: "data" },
      h("thead", null, h("tr", null, ["PNR", "Passenger", "Journey", "Seat", "Fare", "Status", "Action"]
        .map((label) => h("th", { scope: "col" }, label)))),
      h("tbody", null, rows))));
}

function bookingSummary(booking) {
  return h("div", { class: "stack" },
    h("h3", null, `PNR: ${booking.pnr}`),
    h("p", null, `Passenger: ${booking.passenger}`),
    h("p", null, `Train: ${booking.trainNo} · ${booking.trainName}`),
    h("p", null, `Journey: ${booking.source} → ${booking.destination}`),
    h("p", null, `Seat: ${booking.seat} · Fare: ${rupees(booking.fare)}`),
    h("p", null, `Status: ${booking.status}`));
}

async function cancelTicket(pnr, root) {
  if (!(await confirmDialog(`Cancel ticket PNR ${pnr}?`))) return;
  try {
    await api.cancelBooking(pnr);
    toast(`PNR ${pnr} cancelled`);
    await renderBookings(root);
  } catch (error) { toast(error.message, true); }
}

registerPage({
  id: "booking", path: "booking", label: "Bookings", order: 60,
  render(root) { return renderBookings(root); },
});
