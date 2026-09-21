const cats = document.getElementById("cats");
const root = document.getElementById("menu");
const zoom = document.getElementById("zoom");
const zoomImg = document.getElementById("zoomImg");

let menuData = null;
let catObserver = null;
let lang = localStorage.getItem("fk_lang") === "en" ? "en" : "tr";

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

function render(data) {
  if (data) menuData = data;
  if (!menuData) return;
  if (catObserver) {
    catObserver.disconnect();
    catObserver = null;
  }

  const sections = menuData.sections || [];
  document.querySelector(".hero h1").textContent = txt(menuData, "name") || "Fırat Kebap";
  setLine(document.getElementById("subtitle"), txt(menuData, "subtitle"));
  setLine(document.getElementById("kicker"), txt(menuData, "kicker"));
  setLine(document.getElementById("foot"), txt(menuData, "note"));
  document.title = txt(menuData, "name") || "Menu";
  document.getElementById("cats").setAttribute("aria-label", lang === "en" ? "Categories" : "Kategoriler");
  syncLangButtons();

  cats.innerHTML = "";
  root.innerHTML = "";

  sections.forEach((section) => {
    const title = txt(section, "title");
    const link = document.createElement("a");
    link.href = "#" + section.id;
    link.textContent = title;
    cats.appendChild(link);

    const wrap = document.createElement("section");
    wrap.className = "section";
    wrap.id = section.id;
    wrap.innerHTML = `<h2>${escapeHtml(title)}</h2>`;

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
          <h3>${escapeHtml(name)}</h3>
          <p>${escapeHtml(desc)}</p>
        </div>
        <span class="price">${money(item.price)}</span>
      `;
      wrap.appendChild(row);
    });

    root.appendChild(wrap);
  });

  const links = [...cats.querySelectorAll("a")];
  links[0]?.classList.add("active");

  catObserver = new IntersectionObserver(
    (entries) => {
      entries.forEach((entry) => {
        if (!entry.isIntersecting) return;
        links.forEach((a) =>
          a.classList.toggle("active", a.getAttribute("href") === "#" + entry.target.id)
        );
      });
    },
    { rootMargin: "0px 0px -70% 0px", threshold: 0.2 }
  );

  sections.forEach((section) => {
    const el = document.getElementById(section.id);
    if (el) catObserver.observe(el);
  });
}

document.getElementById("lang").addEventListener("click", (e) => {
  const btn = e.target.closest("button[data-lang]");
  if (!btn) return;
  lang = btn.dataset.lang === "en" ? "en" : "tr";
  localStorage.setItem("fk_lang", lang);
  render();
});

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
