// Shared script of all ProjectUtils pages. Served at /pu/app.js; usable by project pages too.
// Draws the page list (a rail on wide screens, a drawer behind the menu button on phones) on
// every page and offers small helpers under the global `pu`.
"use strict";

const pu = (() => {
  async function request(path, options) {
    const response = await fetch(path, options);
    const type = response.headers.get("Content-Type") || "";
    const body = type.includes("json") ? await response.json() : await response.text();
    if (!response.ok) {
      const error = new Error((body && body.message) || "HTTP " + response.status);
      error.status = response.status;
      throw error;
    }
    return body;
  }

  // GET a JSON or text resource from the device.
  const get = (path) => request(path);

  // POST form parameters (an object or FormData) to the device.
  const post = (path, params) =>
    request(path, { method: "POST", body: new URLSearchParams(params || {}) });

  // Create an element: pu.el("button", { className: "red", onclick: f }, "Stop")
  function el(tag, props, ...children) {
    const node = document.createElement(tag);
    Object.entries(props || {}).forEach(([key, value]) => {
      if (key in node) {
        node[key] = value;
      } else {
        node.setAttribute(key, value);
      }
    });
    children.forEach((child) => node.append(child));
    return node;
  }

  const icons = {
    home: '<path d="M4 11l8-7 8 7v8a1 1 0 0 1-1 1h-4v-6h-6v6H5a1 1 0 0 1-1-1z"/>',
    page: '<path d="M7 3h7l4 4v13a1 1 0 0 1-1 1H7a1 1 0 0 1-1-1V4a1 1 0 0 1 1-1zM14 3v4h4"/>',
    wifi: '<path d="M3 9a14 14 0 0 1 18 0M6 12.5a9.5 9.5 0 0 1 12 0M9 16a5 5 0 0 1 6 0"/><circle cx="12" cy="19" r=".6"/>',
    upload: '<path d="M12 16V4m0 0l-4.5 4.5M12 4l4.5 4.5M4 16v3a1 1 0 0 0 1 1h14a1 1 0 0 0 1-1v-3"/>',
    terminal: '<rect x="3" y="4" width="18" height="16" rx="2.5"/><path d="M7 10l3 2.5L7 15M12.5 15H17"/>',
    chip: '<rect x="6" y="6" width="12" height="12" rx="2"/><path d="M9.5 3v3M14.5 3v3M9.5 18v3M14.5 18v3M3 9.5h3M3 14.5h3M18 9.5h3M18 14.5h3"/>',
    menu: '<path d="M4 7h16M4 12h16M4 17h16"/>',
    memory: '<ellipse cx="12" cy="6" rx="7" ry="2.8"/><path d="M5 6v12c0 1.5 3.1 2.8 7 2.8s7-1.3 7-2.8V6M5 12c0 1.5 3.1 2.8 7 2.8s7-1.3 7-2.8"/>',
    reset: '<path d="M4 12a8 8 0 1 0 2.4-5.7M4 4v4.5h4.5"/>',
    trash: '<path d="M4 7h16M10 11v6M14 11v6M6 7l1 12a1 1 0 0 0 1 1h8a1 1 0 0 0 1-1l1-12M9 7V4h6v3"/>',
    lock: '<rect x="5" y="11" width="14" height="9" rx="2"/><path d="M8 11V8a4 4 0 0 1 8 0v3"/>',
  };

  // Line icon by name: home, page, wifi, upload, terminal, chip, menu, lock, memory, reset, trash.
  function icon(name) {
    const holder = document.createElement("span");
    holder.innerHTML = '<svg class="icon" viewBox="0 0 24 24" aria-hidden="true">' + icons[name] + "</svg>";
    return holder.firstChild;
  }

  let toastTimer = null;
  // Short message at the bottom of the page.
  function toast(text, isError) {
    let box = document.getElementById("pu-toast");
    if (!box) {
      box = el("div", { id: "pu-toast", role: "status" });
      document.body.append(box);
    }
    box.textContent = text;
    box.className = isError ? "show error" : "show";
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => { box.className = ""; }, isError ? 6000 : 2500);
  }

  // Answers once `origin` (e.g. "http://192.168.1.20") responds. Other addresses than the
  // page's own cannot be read (no CORS), but an opaque answer is enough to know it is up.
  function probe(origin) {
    if (origin === location.origin) {
      // asking for a login (a password was just set) also means the device is back
      return get("/api/pu/nav").catch((error) => {
        if (error.status !== 401) {
          throw error;
        }
      });
    }
    const abort = new AbortController();
    const timer = setTimeout(() => abort.abort(), 3000);
    return fetch(origin + "/pu/app.css", { mode: "no-cors", cache: "no-store", signal: abort.signal })
      .finally(() => clearTimeout(timer));
  }

  // After the device was told to restart: wait until it answers again, then reload the page.
  // With `origins` the device may come back at another address: the first one that answers is
  // opened at `path` (default: this page).
  function waitForRestart(origins, path) {
    toast("Device is restarting...");
    const candidates = [...new Set(origins && origins.length > 0 ? origins : [location.origin])];
    const open = (origin) => {
      if (origin === location.origin && !path) {
        location.reload();
      } else {
        location.href = origin + (path || location.pathname + location.search);
      }
    };
    const poll = () => {
      Promise.any(candidates.map((origin) => probe(origin).then(() => origin)))
        .then(open).catch(() => setTimeout(poll, 1500));
    };
    setTimeout(poll, 4000);
  }

  // Per-browser preference; storage may be unavailable (private mode), then the default counts.
  function remember(key, fallback, value) {
    try {
      if (value !== undefined) {
        localStorage.setItem("pu." + key, JSON.stringify(value));
        return value;
      }
      const stored = localStorage.getItem("pu." + key);
      return stored === null ? fallback : JSON.parse(stored);
    } catch (error) {
      return value !== undefined ? value : fallback;
    }
  }

  // Connects a checkbox to a remembered "wrap long lines" setting for `target` (default off).
  function wrapToggle(checkbox, target) {
    const apply = () => target.classList.toggle("nowrap", !checkbox.checked);
    checkbox.checked = remember("wrapLines", false);
    apply();
    checkbox.addEventListener("change", () => { remember("wrapLines", false, checkbox.checked); apply(); });
  }

  const builtIn = [
    { path: "/settings", title: "Settings" },
    { path: "/update", title: "Update" },
    { path: "/console", title: "Console" },
    { path: "/memory", title: "Memory" },
    { path: "/info", title: "Device info" },
  ];
  const builtInIcons = { "/settings": "wifi", "/update": "upload", "/console": "terminal", "/memory": "memory", "/info": "chip" };

  let rail = null;
  let online = null;

  function fillRail(nav) {
    const here = location.pathname;
    const link = (page, iconName) => {
      const a = el("a", { href: page.path }, icon(iconName), page.title);
      if (page.path === here) {
        a.setAttribute("aria-current", "page");
      }
      return a;
    };
    online = el("i", { className: "pu-dot" + (nav ? " on" : "") });
    const status = el("span", { className: "pu-status" }, online, location.host);
    const project = nav ? nav.project : [];
    rail.replaceChildren(
      el("div", { className: "pu-device" }, el("b", null, nav ? nav.deviceName : "Device"), status),
      ...project.map((page) => link(page, page.path === "/" ? "home" : "page")),
      ...(project.length > 0 ? [el("hr")] : []),
      ...(nav ? nav.device : builtIn).map((page) => link(page, builtInIcons[page.path] || "page")));
  }

  document.addEventListener("DOMContentLoaded", () => {
    const close = () => document.body.classList.remove("pu-open");
    rail = el("nav", { className: "pu-rail", ariaLabel: "Pages" });
    const burger = el("button", {
      className: "pu-burger", ariaLabel: "Open menu",
      onclick: () => document.body.classList.add("pu-open"),
    }, icon("menu"));
    const bar = el("header", { className: "pu-bar" }, burger, el("b", null, document.title));
    new MutationObserver(() => { bar.lastChild.textContent = document.title; })
      .observe(document.querySelector("title"), { childList: true });
    document.body.prepend(bar, rail, el("button", { className: "pu-scrim", ariaLabel: "Close menu", onclick: close }));
    fillRail(null);
    get("/api/pu/nav").then(fillRail).catch(() => {});
    // the dot in the rail shows whether the device still answers
    setInterval(() => {
      get("/api/pu/nav").then(() => online.classList.add("on")).catch(() => online.classList.remove("on"));
    }, 10000);
  });

  return { get, post, el, icon, toast, waitForRestart, remember, wrapToggle };
})();
