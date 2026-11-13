// Shared script of all ProjectUtils pages. Served at /pu/app.js; usable by project pages too.
// Draws the menu on every page and offers small helpers under the global `pu`.
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

  let toastTimer = null;
  // Short message at the bottom of the page.
  function toast(text, isError) {
    let box = document.getElementById("pu-toast");
    if (!box) {
      box = el("div", { id: "pu-toast" });
      document.body.append(box);
    }
    box.textContent = text;
    box.className = isError ? "show error" : "show";
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => { box.className = ""; }, isError ? 6000 : 2500);
  }

  // After the device was told to restart: wait until it answers again, then reload the page.
  function waitForRestart() {
    toast("Device is restarting...");
    const poll = () => {
      get("/api/pu/nav").then(() => location.reload()).catch(() => setTimeout(poll, 1000));
    };
    setTimeout(poll, 4000);
  }

  const builtIn = [
    { path: "/settings", title: "Settings" },
    { path: "/update", title: "Update" },
    { path: "/console", title: "Console" },
    { path: "/info", title: "Device info" },
  ];

  function buildMenu(nav) {
    const here = location.pathname;
    const item = (page) => el("li", null, el("a", {
      className: "menu__item" + (page.path === here ? " active" : ""),
      href: page.path,
    }, page.title));
    const box = el("ul", { className: "menu__box" });
    if (nav && nav.deviceName) {
      box.append(el("li", { className: "menu__device" }, nav.deviceName));
    }
    const project = nav ? nav.project : [];
    project.forEach((page) => box.append(item(page)));
    if (project.length > 0) {
      box.append(el("li", { className: "menu__sep" }));
    }
    (nav ? nav.device : builtIn).forEach((page) => box.append(item(page)));

    const toggle = el("input", { id: "menu__toggle", type: "checkbox", ariaLabel: "Menu" });
    const shade = el("div", { className: "menu__shade", onclick: () => { toggle.checked = false; } });
    document.body.prepend(
      toggle, el("label", { className: "menu__btn", htmlFor: "menu__toggle" }, el("span")),
      box, shade);
  }

  document.addEventListener("DOMContentLoaded", () => {
    get("/api/pu/nav").then(buildMenu).catch(() => buildMenu(null));
  });

  return { get, post, el, toast, waitForRestart };
})();
