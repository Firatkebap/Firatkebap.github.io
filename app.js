const cats = document.getElementById("cats");
const root = document.getElementById("menu");
const qrBtn = document.getElementById("qrBtn");
const qrSheet = document.getElementById("qrSheet");
const qrBox = document.getElementById("qrBox");
const pageUrl = document.getElementById("pageUrl");

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

function render(data) {
  const sections = data.sections || [];
  document.querySelector(".hero h1").textContent = data.name || "Fırat Kebap";
  document.querySelector(".subtitle").textContent = data.subtitle || "";
  document.querySelector(".kicker").textContent = data.kicker || "";
  document.title = (data.name || "Menü") + " · QR Menü";

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
      const photo = item.image
        ? `<img class="dish" src="${escapeHtml(item.image)}" alt="" />`
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

async function loadMenu() {
  try {
    return await loadMenuFile();
  } catch {
    return emptyMenu();
  }
}

loadMenu().then(render);

qrBtn.addEventListener("click", async () => {
  const url = location.href.split("#")[0];
  pageUrl.textContent = url;
  qrBox.innerHTML = "";
  QRCode.toCanvas(url, { width: 220, margin: 1 }, (err, canvas) => {
    if (err) {
      qrBox.textContent = "QR oluşturulamadı. Sayfayı internete koyunca tekrar deneyin.";
      return;
    }
    qrBox.appendChild(canvas);
  });
  qrSheet.showModal();
});
