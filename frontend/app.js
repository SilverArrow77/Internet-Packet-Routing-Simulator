"use strict";

const ui = {
  cy: document.getElementById("cy"),
  routers: document.getElementById("router-count"),
  links: document.getElementById("link-count"),
  components: document.getElementById("component-count"),
  sample: document.getElementById("sample-packet"),
  ip: document.getElementById("destination-ip"),
  priority: document.getElementById("packet-priority"),
  send: document.getElementById("send-packet"),
  sendLabel: document.getElementById("send-label"),
  file: document.getElementById("simulation-file"),
  error: document.getElementById("form-error"),
  state: document.getElementById("route-state"),
  hops: document.getElementById("hop-count"),
  cost: document.getElementById("route-cost"),
  result: document.getElementById("route-result"),
  track: document.getElementById("route-track"),
  progress: document.getElementById("progress-text"),
  progressBar: document.getElementById("progress-bar"),
  queueLabel: document.getElementById("queue-label"),
  queueSlots: document.getElementById("queue-slots"),
  eventLog: document.getElementById("event-log")
};

const componentColors = ["#70e3d0", "#a99aff", "#f1c879", "#78a9ff", "#f293bd"];
let graph;
let simulation;
let playbackId = 0;

function findComponents(routers, edges) {
  const adjacency = new Map(routers.map(router => [router, []]));
  edges.forEach(edge => {
    adjacency.get(edge.source).push(edge.target);
    adjacency.get(edge.target).push(edge.source);
  });
  const componentOf = new Map();
  const components = [];
  for (const router of routers) {
    if (componentOf.has(router)) continue;
    const componentIndex = components.length;
    const members = [];
    const pending = [router];
    componentOf.set(router, componentIndex);
    while (pending.length) {
      const current = pending.pop();
      members.push(current);
      for (const next of adjacency.get(current)) {
        if (!componentOf.has(next)) {
          componentOf.set(next, componentIndex);
          pending.push(next);
        }
      }
    }
    components.push(members);
  }
  return { componentOf, components };
}

function validateSimulation(data) {
  if (!data || data.version !== 1 || !Array.isArray(data.routers) ||
      !Array.isArray(data.links) || !Array.isArray(data.packets) ||
      !Array.isArray(data.demos) || data.demos.length !== 3) {
    throw new Error("This is not a supported simulation.json file. Run the C++ simulator first.");
  }
  const routers = new Set(data.routers);
  if (data.links.some(link =>
    !routers.has(link.source) || !routers.has(link.target) ||
    !Number.isFinite(link.weight) || link.weight <= 0)) {
    throw new Error("simulation.json contains an invalid network link.");
  }
  if (data.packets.some(packet =>
    !Array.isArray(packet.route) || !Number.isFinite(packet.id))) {
    throw new Error("simulation.json contains an invalid packet result.");
  }
  data.demos.forEach(demo => {
    if (!demo.packet || !Array.isArray(demo.initialRoute) ||
        !Array.isArray(demo.finalRoute) || !Array.isArray(demo.events) ||
        demo.events.some(event => typeof event.type !== "string")) {
      throw new Error("simulation.json contains an invalid stress demo.");
    }
  });
  return data;
}

function setError(message) {
  ui.error.textContent = message;
  ui.error.classList.add("visible");
}

function clearError() {
  ui.error.textContent = "";
  ui.error.classList.remove("visible");
}

function setProgress(done, total) {
  const percent = total <= 0 ? 100 : Math.round(done / total * 100);
  ui.progress.textContent = `${Math.min(100, percent)}%`;
  ui.progressBar.style.width = `${Math.min(100, percent)}%`;
}

function renderTrack(path, currentIndex) {
  ui.track.replaceChildren();
  path.forEach((router, index) => {
    const stop = document.createElement("span");
    stop.className = "route-stop";
    stop.textContent = router;
    if (index < currentIndex) stop.classList.add("complete");
    if (index === currentIndex) stop.classList.add("current");
    ui.track.append(stop);
    if (index < path.length - 1) {
      const connector = document.createElement("i");
      connector.className = "route-connector";
      if (index < currentIndex) connector.classList.add("complete");
      ui.track.append(connector);
    }
  });
}

function resetGraphPlayback() {
  if (!graph) return;
  graph.nodes().removeClass("on-route packet-at");
  graph.edges().removeClass("on-route packet-flash failed-link");
  graph.$("#__packet-token").remove();
}

function highlightPath(path) {
  if (!graph) return;
  graph.nodes().removeClass("on-route");
  graph.edges().removeClass("on-route");
  const pathNodes = new Set(path);
  graph.nodes().forEach(node => node.toggleClass("on-route", pathNodes.has(node.id())));
  for (let index = 0; index < path.length - 1; index += 1) {
    findEdge(path[index], path[index + 1]).addClass("on-route");
  }
}

function findEdge(source, target) {
  return graph.edges().filter(edge =>
    (edge.data("source") === source && edge.data("target") === target) ||
    (edge.data("source") === target && edge.data("target") === source));
}

function setQueue(size, capacity, label, full) {
  ui.queueLabel.textContent = label;
  ui.queueSlots.replaceChildren();
  for (let i = 0; i < capacity; i += 1) {
    const slot = document.createElement("i");
    slot.className = "queue-slot";
    if (i < size) slot.classList.add(full ? "occupied-full" : "occupied");
    ui.queueSlots.append(slot);
  }
  ui.queueLabel.classList.toggle("queue-full-text", full);
}

function addLog(message, type) {
  const entry = document.createElement("div");
  entry.className = `event-entry ${type || ""}`;
  entry.textContent = message;
  ui.eventLog.append(entry);
  ui.eventLog.scrollTop = ui.eventLog.scrollHeight;
}

function renderGraph() {
  const { routers, links } = simulation;
  const { componentOf, components } = findComponents(routers, links);
  ui.routers.textContent = String(routers.length);
  ui.links.textContent = String(links.length);
  ui.components.textContent = String(components.length);

  const cyElements = [
    ...routers.map(router => ({
      data: { id: router, label: router, component: componentOf.get(router) }
    })),
    ...links.map((link, index) => ({
      data: {
        id: `link-${index}`,
        source: link.source,
        target: link.target,
        weight: link.weight,
        component: componentOf.get(link.source)
      }
    }))
  ];

  if (graph) graph.destroy();
  graph = cytoscape({
    container: ui.cy,
    elements: cyElements,
    minZoom: .35,
    maxZoom: 2.5,
    style: [
      {
        selector: "node",
        style: {
          label: "data(label)",
          color: "#dce8f7",
          "font-family": "DM Mono, monospace",
          "font-size": 9,
          "text-valign": "center",
          "text-halign": "center",
          width: 33,
          height: 33,
          "background-color": "#293b55",
          "border-width": 1,
          "border-color": "#6e89ab",
          "underlay-color": "#49728b",
          "underlay-opacity": .2,
          "underlay-padding": 5
        }
      },
      {
        selector: "edge",
        style: {
          "curve-style": "bezier",
          "line-color": "#43536a",
          width: 1.4,
          label: "data(weight)",
          "font-family": "DM Mono, monospace",
          "font-size": 7,
          color: "#8293aa",
          "text-background-color": "#121c2b",
          "text-background-opacity": .9,
          "text-background-padding": 2,
          "text-rotation": "autorotate"
        }
      },
      {
        selector: "node.on-route",
        style: {
          "background-color": "#1f736f",
          "border-color": "#70e3d0",
          "border-width": 2,
          "underlay-color": "#70e3d0",
          "underlay-opacity": .55,
          "underlay-padding": 8
        }
      },
      { selector: "edge.on-route", style: { "line-color": "#70e3d0", width: 3, "z-index": 2 } },
      { selector: "edge.packet-flash", style: { "line-color": "#f4bd68", width: 4 } },
      { selector: "edge.failed-link", style: { "line-color": "#ff7185", width: 4, "line-style": "dashed", "z-index": 4 } },
      {
        selector: "node.packet-at",
        style: { "border-color": "#f4bd68", "border-width": 3, "underlay-color": "#f4bd68", "underlay-opacity": .8, "underlay-padding": 10 }
      },
      {
        selector: "node.packet-token",
        style: {
          label: "data(label)",
          width: 20,
          height: 20,
          "font-size": 13,
          color: "#15202c",
          "background-color": "#f4bd68",
          "border-width": 2,
          "border-color": "#fff0c9",
          "underlay-color": "#f4bd68",
          "underlay-opacity": .8,
          "underlay-padding": 8,
          "z-index": 100
        }
      }
    ],
    layout: {
      name: "cose",
      animate: false,
      randomize: true,
      componentSpacing: 85,
      nodeRepulsion: 6200,
      idealEdgeLength: 80,
      edgeElasticity: 90,
      gravity: .25,
      numIter: 900,
      padding: 35
    }
  });

  graph.on("mouseover", "node", event => {
    const node = event.target;
    node.data("previousLabel", node.data("label"));
    node.data("label", `${node.id()} · C${Number(node.data("component")) + 1}`);
  });
  graph.on("mouseout", "node", event => {
    const node = event.target;
    node.data("label", node.data("previousLabel") || node.id());
  });
  graph.fit(undefined, 32);

  const summary = document.createElement("div");
  summary.className = "component-summary";
  summary.setAttribute("aria-label", "Connected components");
  components.forEach((members, index) => {
    const chip = document.createElement("button");
    chip.type = "button";
    chip.className = "component-chip";
    chip.style.setProperty("--component-color", componentColors[index % componentColors.length]);
    chip.innerHTML = `<i></i><span>C${index + 1}</span><small>${members.length} ${members.length === 1 ? "router" : "routers"}</small>`;
    chip.title = `Highlight ${members.join(", ")}`;
    chip.onclick = () => {
      graph.elements().addClass("faded");
      graph.nodes().filter(node => node.data("component") === index).removeClass("faded");
      graph.edges().filter(edge => edge.data("component") === index).removeClass("faded");
      setTimeout(() => graph.elements().removeClass("faded"), 1800);
    };
    summary.append(chip);
  });
  document.querySelector(".component-summary")?.remove();
  document.querySelector(".graph-legend").insertAdjacentElement("beforebegin", summary);
}

function selectedItem() {
  if (ui.sample.value.indexOf("demo-") === 0) {
    const demoIndex = Number(ui.sample.value.slice(5));
    return { kind: "demo", demo: simulation.demos[demoIndex] };
  }
  return {
    kind: "packet",
    packet: simulation.packets[Number(ui.sample.value)]
  };
}

function showSelection() {
  const selection = selectedItem();
  const packet = selection.kind === "demo"
    ? selection.demo && selection.demo.packet
    : selection.packet;
  if (!packet) return;
  ui.ip.textContent = packet.ip || "—";
  ui.priority.textContent = `PRIORITY ${packet.priority}`;
  ui.send.disabled = false;
  ui.sendLabel.textContent = selection.kind === "demo"
    ? "Play congestion + reroute"
    : "Play C++ packet result";

  resetGraphPlayback();
  const route = selection.kind === "demo"
    ? selection.demo.initialRoute
    : packet.route;
  highlightPath(route);
  renderTrack(route, -1);
  ui.hops.textContent = String(Math.max(0, route.length - 1));
  ui.cost.textContent = String(
    selection.kind === "demo" ? selection.demo.initialCost : packet.cost);
  ui.state.textContent = selection.kind === "demo" ? "DEMO READY" : "C++ RESULT";
  ui.state.classList.remove("error");
  ui.result.textContent = selection.kind === "demo"
    ? "Queue + link event"
    : (packet.delivered ? "Delivered" : "Not delivered");
  setProgress(0, Math.max(1, route.length - 1));
  ui.eventLog.replaceChildren();
  addLog(
    selection.kind === "demo"
      ? "Ready: queue burst, overflow, link failure, and alternate route."
      : `Loaded C++ result: ${packet.message}`,
    "info");
  const queueCapacity = selection.kind === "demo"
    ? selection.demo.queueCapacity
    : simulation.demos[0].queueCapacity;
  setQueue(0, queueCapacity, "Ready to play", false);
}

function delay(milliseconds, currentPlayback) {
  return new Promise(resolve => {
    setTimeout(() => resolve(currentPlayback === playbackId), milliseconds);
  });
}

function animateHop(from, to, currentPlayback) {
  return new Promise(resolve => {
    if (currentPlayback !== playbackId) return resolve(false);
    const sourceNode = graph.getElementById(from);
    const targetNode = graph.getElementById(to);
    const edge = findEdge(from, to);
    if (!sourceNode.length || !targetNode.length || !edge.length) return resolve(false);

    let token = graph.$("#__packet-token");
    if (!token.length) {
      token = graph.add({
        group: "nodes",
        data: { id: "__packet-token", label: "✦" },
        position: sourceNode.position(),
        classes: "packet-token"
      });
      token.lock();
    } else {
      token.position(sourceNode.position());
    }
    graph.nodes().removeClass("packet-at");
    sourceNode.addClass("packet-at");
    edge.addClass("packet-flash");
    token.animate({ position: targetNode.position() }, {
      duration: 620,
      easing: "ease-in-out-cubic",
      complete: () => {
        edge.removeClass("packet-flash");
        sourceNode.removeClass("packet-at");
        targetNode.addClass("packet-at");
        resolve(currentPlayback === playbackId);
      }
    });
  });
}

async function playPacket(packet, currentPlayback) {
  const route = packet.route;
  if (!packet.delivered || route.length < 2) {
    ui.state.textContent = "NOT DELIVERED";
    ui.state.classList.add("error");
    ui.result.textContent = packet.message;
    addLog(`C++ result: ${packet.message}`, "warning");
    return;
  }
  highlightPath(route);
  ui.state.textContent = "IN TRANSIT";
  ui.result.textContent = "C++ playback";
  graph.getElementById(route[0]).addClass("packet-at");
  for (let i = 0; i < route.length - 1; i += 1) {
    if (!await animateHop(route[i], route[i + 1], currentPlayback)) return;
    renderTrack(route, i + 1);
    setProgress(i + 1, route.length - 1);
    addLog(`${packet.id}: ${route[i]} → ${route[i + 1]}`, "move");
    if (!await delay(180, currentPlayback)) return;
  }
  graph.$("#__packet-token").remove();
  ui.state.textContent = "DELIVERED";
  ui.result.textContent = "Delivered by C++";
}

async function playDemo(demo, currentPlayback) {
  const packet = demo.packet;
  let currentRoute = demo.initialRoute;
  let travelled = 0;
  ui.state.textContent = "SIMULATING";
  ui.result.textContent = "Queue burst";

  for (const event of demo.events) {
    if (currentPlayback !== playbackId) return;
    if (event.type === "queue") {
      const full = event.action === "full";
      const dequeued = event.action === "dequeued";
      setQueue(
        event.size,
        event.capacity,
        full ? `${event.router} FULL · packet ${event.packetId} dropped`
          : dequeued ? `${event.router} · packet ${event.packetId} scheduled (${event.size}/${event.capacity})`
          : `${event.router} · ${event.size}/${event.capacity} packets`,
        full);
      addLog(
        full
          ? `Queue full at ${event.router}: packet ${event.packetId} dropped`
          : dequeued
            ? `Scheduler removed packet ${event.packetId}; queue now ${event.size}/${event.capacity}`
          : `Packet ${event.packetId} entered ${event.router} (${event.size}/${event.capacity})`,
        full ? "warning" : dequeued ? "move" : "queue");
      const queuePause = full ? 1200 : dequeued ? 420 : 180;
      if (!await delay(queuePause, currentPlayback)) return;
    } else if (event.type === "move") {
      if (!await animateHop(event.from, event.to, currentPlayback)) return;
      travelled += 1;
      renderTrack(currentRoute, Math.min(travelled, currentRoute.length - 1));
      setProgress(travelled, demo.finalRoute.length - 1);
      ui.state.textContent = "IN TRANSIT";
      ui.result.textContent = `${event.from} → ${event.to}`;
      addLog(`Packet ${packet.id}: ${event.from} → ${event.to}`, "move");
      if (!await delay(180, currentPlayback)) return;
    } else if (event.type === "reroute") {
      findEdge(event.failedSource, event.failedTarget).addClass("failed-link");
      currentRoute = demo.finalRoute;
      highlightPath(currentRoute);
      ui.cost.textContent = String(demo.finalCost);
      ui.hops.textContent = String(currentRoute.length - 1);
      renderTrack(currentRoute, travelled);
      ui.state.textContent = "REROUTED";
      ui.result.textContent = `${event.router} chose alternate path`;
      addLog(
        `Link ${event.failedSource}–${event.failedTarget} failed; recalculated route at ${event.router}`,
        "warning");
      await delay(1100, currentPlayback);
    } else if (event.type === "delivered") {
      graph.$("#__packet-token").remove();
      graph.nodes().removeClass("packet-at");
      ui.state.textContent = "DELIVERED";
      ui.result.textContent = `Arrived at ${event.router}`;
      setProgress(demo.finalRoute.length - 1, demo.finalRoute.length - 1);
      setQueue(demo.queueSize, demo.queueCapacity, `${demo.queueSize}/${demo.queueCapacity} · ${demo.droppedPackets} dropped`, false);
      addLog(`Packet ${packet.id} delivered at ${event.router}`, "success");
    }
  }
}

async function playSelected() {
  if (!simulation) return;
  clearError();
  playbackId += 1;
  const currentPlayback = playbackId;
  resetGraphPlayback();
  const selection = selectedItem();
  if (selection.kind === "demo" ? !selection.demo : !selection.packet) {
    setError("Select a packet from the loaded C++ simulation.");
    return;
  }
  ui.send.disabled = true;
  ui.sendLabel.textContent = "Playback running…";
  if (selection.kind === "demo") {
    await playDemo(selection.demo, currentPlayback);
  } else {
    await playPacket(selection.packet, currentPlayback);
  }
  if (currentPlayback === playbackId) {
    ui.send.disabled = false;
    ui.sendLabel.textContent = selection.kind === "demo"
      ? "Replay congestion + reroute"
      : "Replay C++ packet";
  }
}

function loadSimulation(data) {
  simulation = validateSimulation(data);
  renderGraph();
  ui.sample.replaceChildren();

  simulation.packets.forEach((packet, index) => {
    const option = document.createElement("option");
    option.value = String(index);
    option.textContent = `C++ packet ${packet.id} · ${packet.source} → ${packet.destination}`;
    ui.sample.append(option);
  });
  simulation.demos.forEach((demo, index) => {
    const demoOption = document.createElement("option");
    demoOption.value = `demo-${index}`;
    demoOption.textContent =
      `Stress demo ${index + 1} · queue full + reroute (${demo.packet.source} → ${demo.packet.destination})`;
    ui.sample.append(demoOption);
  });
  ui.sample.disabled = false;
  ui.sample.value = simulation.packets.length ? "0" : "demo-0";
  showSelection();
}

async function initialize() {
  ui.send.addEventListener("click", playSelected);
  ui.sample.addEventListener("change", showSelection);
  ui.file.addEventListener("change", async event => {
    const file = event.target.files && event.target.files[0];
    if (!file) return;
    try {
      loadSimulation(JSON.parse(await file.text()));
      clearError();
    } catch (error) {
      console.error(error);
      setError(error.message);
    }
  });
  document.getElementById("fit-graph").addEventListener("click", () => {
    if (graph) graph.fit(undefined, 32);
  });

  if (typeof window.cytoscape !== "function") {
    throw new Error("Cytoscape.js did not load. Check your internet connection and reload.");
  }
  try {
    const response = await fetch("../simulation.json", { cache: "no-store" });
    if (!response.ok) throw new Error("Run main.cpp or use “Load simulation.json” to load a replay.");
    loadSimulation(await response.json());
  } catch (error) {
    ui.eventLog.textContent = "Run main.cpp, then upload simulation.json here.";
    setError(error.message);
  }
}

initialize().catch(error => {
  console.error(error);
  setError(error.message);
});
