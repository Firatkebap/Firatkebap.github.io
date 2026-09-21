const cats = document.getElementById("cats");
const root = document.getElementById("menu");
const zoom = document.getElementById("zoom");
const zoomImg = document.getElementById("zoomImg");

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

function setLine(el, text) {
  const t = (text || "").trim();
  el.hidden = !t;
  el.textContent = t;
}

function render(data) {
  const sections = data.sections || [];
  document.querySelector(".hero h1").textContent = data.name || "Fırat Kebap";
  setLine(document.getElementById("subtitle"), data.subtitle);
  setLine(document.getElementById("kicker"), data.kicker);
  setLine(document.getElementById("foot"), data.note);
  document.title = data.name || "Menü";

  cats.innerHTML = "";
  root.innerHTML = "";

  sections.forEach((section) => {
    const link = document.createElement("a");
    link.href = "#" + section.id;
    link.textContent = section.title;
    cats.appendChild(link);

    const wrap = document.createElement("section");
    wrap.className = "section";
    wrap.id = section.id;
    wrap.innerHTML = `<h2>${escapeHtml(section.title)}</h2>`;

    (section.items || []).forEach((item) => {
      const row = document.createElement("article");
      row.className = item.image ? "item with-photo" : "item";
      const src = item.image ? escapeHtml(mediaUrl(item.image)) : "";
      const photo = item.image
        ? `<button type="button" class="dish-btn" data-src="${src}" aria-label="${escapeHtml(item.name)}"><img class="dish" src="${src}" alt="" /></button>`
        : "";
      row.innerHTML = `
        ${photo}
        <div class="copy">
          <h3>${escapeHtml(item.name)}</h3>
          <p>${escapeHtml(item.desc)}</p>
        </div>
        <span class="price">${money(item.price)}</span>
      `;
      wrap.appendChild(row);
    });

    root.appendChild(wrap);
  });

  const links = [...cats.querySelectorAll("a")];
  links[0]?.classList.add("active");

  const observer = new IntersectionObserver(
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
    if (el) observer.observe(el);
  });
}

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
