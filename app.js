const cats = document.getElementById("cats");
const root = document.getElementById("menu");
const zoom = document.getElementById("zoom");
const zoomImg = document.getElementById("zoomImg");

let menuData = null;
let lang = localStorage.getItem("fk_lang") === "en" ? "en" : "tr";
let catIndex = 0;
let syncing = false;

function money(n) {
  return Number(n).toLocaleString("tr-TR") + " ₺";
}

function escapeHtml(value) {
  return String(value ?? "")
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;");
}

function txt(obj, key) {
  if (!obj) return "";
  if (lang === "en") {
    const en = obj[key + "En"];
    if (en && String(en).trim()) return en;
  }
  return obj[key] || "";
}

function setLine(el, text) {
  const t = (text || "").trim();
  el.hidden = !t;
  el.textContent = t;
}

function syncLangButtons() {
  document.querySelectorAll(".lang button").forEach((b) => {
    b.classList.toggle("on", b.dataset.lang === lang);
  });
  document.documentElement.lang = lang;
}

function setActiveCat(i, scrollCats = true) {
  const n = cats.children.length;
  if (!n) return;
  catIndex = Math.max(0, Math.min(i, n - 1));
  [...cats.children].forEach((el, idx) => el.classList.toggle("on", idx === catIndex));
  if (scrollCats) {
    cats.children[catIndex].scrollIntoView({ inline: "center", block: "nearest", behavior: "smooth" });
  }
}

function goToCat(i, behavior = "smooth") {
  const panels = root.children;
  if (!panels.length) return;
  catIndex = Math.max(0, Math.min(i, panels.length - 1));
  setActiveCat(catIndex);
  syncing = true;
  root.scrollTo({ left: catIndex * root.clientWidth, behavior });
  setTimeout(() => {
    syncing = false;
  }, behavior === "smooth" ? 380 : 50);
}

function render(data) {
  if (data) menuData = data;
  if (!menuData) return;

  const sections = menuData.sections || [];
  document.querySelector(".hero h1").textContent = txt(menuData, "name") || "Fırat Kebap";
  setLine(document.getElementById("subtitle"), txt(menuData, "subtitle"));
  setLine(document.getElementById("kicker"), txt(menuData, "kicker"));
  setLine(document.getElementById("foot"), txt(menuData, "note"));
  document.title = txt(menuData, "name") || "Menu";
  cats.setAttribute("aria-label", lang === "en" ? "Categories" : "Kategoriler");
  syncLangButtons();

  cats.innerHTML = "";
  root.innerHTML = "";

  sections.forEach((section, sIndex) => {
    const title = txt(section, "title");
    const tab = document.createElement("button");
    tab.type = "button";
    tab.textContent = title;
    tab.addEventListener("click", () => goToCat(sIndex));
    cats.appendChild(tab);

    const wrap = document.createElement("section");
    wrap.className = "section";
    wrap.dataset.index = String(sIndex);
    const inner = document.createElement("div");
    inner.className = "sheet-list";
    inner.innerHTML = `<h2>${escapeHtml(title)}</h2>`;

    (section.items || []).forEach((item) => {
      const name = txt(item, "name");
      const desc = txt(item, "desc");
      const row = document.createElement("article");
      row.className = item.image ? "item with-photo" : "item";
      const src = item.image ? escapeHtml(mediaUrl(item.image)) : "";
      const photo = item.image
        ? `<button type="button" class="dish-btn" data-src="${src}" aria-label="${escapeHtml(name)}"><img class="dish" src="${src}" alt="" /></button>`
        : "";
      row.innerHTML = `
        ${photo}
        <div class="copy">
          <div class="line">
            <h3>${escapeHtml(name)}</h3>
            <span class="price">${money(item.price)}</span>
          </div>
          <p>${escapeHtml(desc)}</p>
        </div>
      `;
      inner.appendChild(row);
    });

    wrap.appendChild(inner);
    root.appendChild(wrap);
  });

  requestAnimationFrame(() => goToCat(Math.min(catIndex, Math.max(0, sections.length - 1)), "auto"));
}

document.getElementById("lang").addEventListener("click", (e) => {
  const btn = e.target.closest("button[data-lang]");
  if (!btn) return;
  lang = btn.dataset.lang === "en" ? "en" : "tr";
  localStorage.setItem("fk_lang", lang);
  render();
});

root.addEventListener(
  "scroll",
  () => {
    if (syncing || !root.clientWidth) return;
    const i = Math.round(root.scrollLeft / root.clientWidth);
    if (i !== catIndex) setActiveCat(i);
  },
  { passive: true }
);

window.addEventListener("resize", () => goToCat(catIndex, "auto"));

root.addEventListener("click", (e) => {
  const btn = e.target.closest(".dish-btn");
  if (!btn) return;
  zoomImg.src = btn.dataset.src;
  zoom.showModal();
});

zoom.addEventListener("click", () => zoom.close());

async function loadMenu() {
  try {
    return await loadMenuFile();
  } catch {
    return emptyMenu();
  }
}

loadMenu().then(render);
