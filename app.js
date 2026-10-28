const cats = document.getElementById("cats");
const root = document.getElementById("menu");
const zoom = document.getElementById("zoom");
const zoomImg = document.getElementById("zoomImg");

let menuData = null;
let i18n = null;
let lang = localStorage.getItem("fk_lang") || "tr";
let catIndex = 0;
let syncing = false;

const FALLBACK_LANGS = [
  { id: "tr", label: "Türkçe", dir: "ltr" },
  { id: "en", label: "English", dir: "ltr" },
];

function ingParts(item) {
  const raw = String((item && item.ings) || "").trim();
  if (!raw) return [];
  const out = [];
  for (const bit of raw.split(",")) {
    const cut = bit.indexOf(":");
    if (cut <= 0) continue;
    const id = bit.slice(0, cut).trim();
    const pct = Number(bit.slice(cut + 1));
    if (!id || !pct) continue;
    out.push({ id, pct });
  }
  return out;
}

const ING_TR = {
  sogan: "Soğan",
  domates: "Domates",
  patlican: "Patlıcan",
  biber: "Biber",
  isot: "İsot",
  "pul-biber": "Pul biber",
  "sivi-yag": "Sıvı yağ",
  kiyma: "Kıyma (dana etinden)",
  kuzu: "Kuzu eti",
  bulgur: "Bulgur",
  tuz: "Tuz",
  sucuk: "Sucuk",
  kasar: "Kaşar",
  yogurt: "Yoğurt",
  maydanoz: "Maydanoz",
  kapya: "Kapya biber",
  dana: "Dana eti",
};

function ingName(id) {
  if (lang === "tr") return ING_TR[id] || id;
  const bag = i18n && i18n.ings && i18n.ings[id];
  const value = bag && (bag[lang] || bag.en);
  return (value && String(value).trim()) || ING_TR[id] || id;
}

function ingHeading() {
  if (lang === "tr") return "İçindekiler";
  const bag = i18n && i18n.ui && i18n.ui.ingredients;
  return (bag && (bag[lang] || bag.en)) || "Ingredients";
}

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

function langList() {
  return (i18n && i18n.langs && i18n.langs.length ? i18n.langs : FALLBACK_LANGS);
}

function langMeta(id) {
  return langList().find((l) => l.id === id) || null;
}

function translated(bag) {
  if (!bag || lang === "tr") return "";
  const value = bag[lang] || bag.en || "";
  return String(value || "").trim();
}

function fromI18n(obj, key) {
  if (!i18n || !obj || lang === "tr") return "";
  if (obj === menuData) return translated(i18n.site && i18n.site[key]);
  if (!obj.id) return "";
  if (Array.isArray(obj.items)) return translated(i18n.sections && i18n.sections[obj.id]);
  const row = i18n.items && i18n.items[obj.id];
  const pack = row && (row[lang] || row.en);
  return pack && pack[key] ? String(pack[key]).trim() : "";
}

function txt(obj, key) {
  if (!obj) return "";
  const own = fromI18n(obj, key);
  if (own) return own;
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

function catRank(section) {
  const blob = ((section.title || "") + " " + (section.id || "")).toLocaleLowerCase("tr");
  if (blob.includes("lahmacun")) return 0;
  if (blob.includes("kebap")) return 1;
  if (blob.includes("pide")) return 2;
  if (blob.includes("dürüm") || blob.includes("durum")) return 3;
  if (blob.includes("tatlı") || blob.includes("tatli")) return 4;
  if (blob.includes("içecek") || blob.includes("icecek")) return 5;
  return 50;
}

function langCode(id) {
  if (id === "zh-Hant") return "ZHT";
  return id.slice(0, 2).toUpperCase();
}

function syncLangButtons() {
  const box = document.getElementById("lang");
  if (box) {
    box.querySelectorAll("button").forEach((b) => {
      const on = b.dataset.lang === lang;
      b.classList.toggle("on", on);
      if (on) b.scrollIntoView({ inline: "center", block: "nearest" });
    });
  }
  const meta = langMeta(lang);
  document.documentElement.lang = lang === "zh-Hant" ? "zh-Hant" : lang;
  document.documentElement.dir = meta && meta.dir === "rtl" ? "rtl" : "ltr";
}

function fillLangButtons() {
  const box = document.getElementById("lang");
  if (!box) return;
  const langs = langList();
  if (!langs.some((l) => l.id === lang)) lang = "tr";
  box.innerHTML = langs
    .map((l) => `<button type="button" data-lang="${escapeHtml(l.id)}" title="${escapeHtml(l.label)}">${escapeHtml(langCode(l.id))}</button>`)
    .join("");
  syncLangButtons();
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

  const sections = (menuData.sections || []).slice().sort((a, b) => catRank(a) - catRank(b));
  document.querySelector(".hero h1").textContent = txt(menuData, "name") || "Fırat Kebap";
  setLine(document.getElementById("subtitle"), txt(menuData, "subtitle"));
  setLine(document.getElementById("kicker"), txt(menuData, "kicker"));
  setLine(document.getElementById("foot"), txt(menuData, "note"));
  document.title = txt(menuData, "name") || "Menu";
  const catLabel = (i18n && i18n.ui && i18n.ui.categories && (i18n.ui.categories[lang] || (lang === "tr" ? "" : i18n.ui.categories.en))) || "";
  cats.setAttribute("aria-label", lang === "tr" ? "Kategoriler" : catLabel || "Categories");
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
          ${ingParts(item).length ? `<div class="ings" aria-label="${escapeHtml(ingHeading())}">${ingParts(item).map((p) => `<span>${escapeHtml(ingName(p.id))} <b>%${p.pct}</b></span>`).join("")}</div>` : ""}
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
  lang = btn.dataset.lang || "tr";
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

async function loadI18n() {
  try {
    const res = await fetch("data/i18n.json?t=" + Date.now(), { cache: "no-store" });
    if (!res.ok) return null;
    return await res.json();
  } catch {
    return null;
  }
}

Promise.all([loadMenu(), loadI18n()]).then(([menu, dict]) => {
  i18n = dict;
  fillLangButtons();
  render(menu);
});
