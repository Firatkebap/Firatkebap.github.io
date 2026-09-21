const catsEl = document.getElementById("cats");
const gate = document.getElementById("gate");
const app = document.getElementById("app");
const setup = document.getElementById("setup");
const saveState = document.getElementById("saveState");
const loginErr = document.getElementById("loginErr");

let menu = { name: "", subtitle: "", kicker: "", sections: [] };
let saveTimer = 0;

function uid() {
  return crypto.randomUUID ? crypto.randomUUID() : Math.random().toString(36).slice(2);
}

function compress(file) {
  return new Promise((resolve, reject) => {
    const img = new Image();
    const url = URL.createObjectURL(file);
    img.onload = () => {
      const max = 900;
      let w = img.width;
      let h = img.height;
      if (Math.max(w, h) > max) {
        const s = max / Math.max(w, h);
        w = Math.round(w * s);
        h = Math.round(h * s);
      }
      const canvas = document.createElement("canvas");
      canvas.width = w;
      canvas.height = h;
      canvas.getContext("2d").drawImage(img, 0, 0, w, h);
      URL.revokeObjectURL(url);
      resolve(canvas.toDataURL("image/jpeg", 0.82));
    };
    img.onerror = () => {
      URL.revokeObjectURL(url);
      reject(new Error("image"));
    };
    img.src = url;
  });
}

function scheduleSave() {
  saveState.textContent = "Kaydediliyor…";
  clearTimeout(saveTimer);
  saveTimer = setTimeout(saveMenu, 700);
}

async function saveMenu() {
  menu.name = document.getElementById("restName").value.trim();
  menu.subtitle = document.getElementById("restSub").value.trim();
  menu.kicker = document.getElementById("restKicker").value.trim();
  try {
    await saveMenuGithub(menu);
    saveState.textContent = "Kaydedildi. Menü yaklaşık 1 dk içinde güncellenir.";
  } catch (err) {
    saveState.textContent = err.status === 401 ? "Token geçersiz" : "Kayıt olmadı";
  }
}

function render() {
  document.getElementById("restName").value = menu.name || "";
  document.getElementById("restSub").value = menu.subtitle || "";
  document.getElementById("restKicker").value = menu.kicker || "";
  catsEl.innerHTML = "";
  menu.sections.forEach((section, sIndex) => {
    const wrap = document.createElement("section");
    wrap.className = "card cat";
    wrap.innerHTML = `
      <div class="cat-head">
        <input data-cat-title="${sIndex}" value="${escapeAttr(section.title)}" />
        <button type="button" class="ghost" data-add-item="${sIndex}">+ Ürün</button>
        <button type="button" class="ghost danger" data-del-cat="${sIndex}">Sil</button>
      </div>
    `;
    section.items.forEach((item, iIndex) => {
      const row = document.createElement("article");
      row.className = "item";
      const img = item.image
        ? `<img class="thumb" src="${escapeAttr(item.image)}" alt="" />`
        : `<div class="thumb empty">Foto yok</div>`;
      row.innerHTML = `
        ${img}
        <div class="fields">
          <div class="row">
            <input data-name="${sIndex}:${iIndex}" value="${escapeAttr(item.name)}" placeholder="Ürün adı" />
            <input data-price="${sIndex}:${iIndex}" type="number" min="0" step="1" value="${item.price}" placeholder="Fiyat" />
          </div>
          <input data-desc="${sIndex}:${iIndex}" value="${escapeAttr(item.desc)}" placeholder="Kısa açıklama" />
          <div class="mini">
            <label class="file">Fotoğraf
              <input type="file" accept="image/*" data-photo="${sIndex}:${iIndex}" />
            </label>
            <button type="button" data-del-item="${sIndex}:${iIndex}" class="danger">Ürünü sil</button>
          </div>
        </div>
      `;
      wrap.appendChild(row);
    });
    catsEl.appendChild(wrap);
  });
}

function escapeAttr(value) {
  return String(value ?? "")
    .replaceAll("&", "&amp;")
    .replaceAll('"', "&quot;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;");
}

function applyField(t) {
  if (t.dataset.catTitle != null) {
    menu.sections[+t.dataset.catTitle].title = t.value;
    scheduleSave();
  }
  if (t.dataset.name) {
    const [s, i] = t.dataset.name.split(":").map(Number);
    menu.sections[s].items[i].name = t.value;
    scheduleSave();
  }
  if (t.dataset.desc) {
    const [s, i] = t.dataset.desc.split(":").map(Number);
    menu.sections[s].items[i].desc = t.value;
    scheduleSave();
  }
  if (t.dataset.price) {
    const [s, i] = t.dataset.price.split(":").map(Number);
    menu.sections[s].items[i].price = Number(t.value || 0);
    scheduleSave();
  }
}

catsEl.addEventListener("input", (e) => applyField(e.target));
catsEl.addEventListener("change", (e) => {
  if (!e.target.dataset.photo) applyField(e.target);
});

catsEl.addEventListener("click", (e) => {
  const t = e.target.closest("button");
  if (!t) return;
  if (t.dataset.addItem != null) {
    menu.sections[+t.dataset.addItem].items.push({
      id: uid(),
      name: "Yeni ürün",
      desc: "",
      price: 0,
      image: "",
    });
    render();
    scheduleSave();
  }
  if (t.dataset.delCat != null) {
    if (!confirm("Kategori silinsin mi?")) return;
    menu.sections.splice(+t.dataset.delCat, 1);
    render();
    scheduleSave();
  }
  if (t.dataset.delItem) {
    const [s, i] = t.dataset.delItem.split(":").map(Number);
    menu.sections[s].items.splice(i, 1);
    render();
    scheduleSave();
  }
});

catsEl.addEventListener("change", async (e) => {
  const t = e.target;
  if (!t.dataset.photo || !t.files?.[0]) return;
  const [s, i] = t.dataset.photo.split(":").map(Number);
  saveState.textContent = "Fotoğraf yükleniyor…";
  try {
    const data = await compress(t.files[0]);
    menu.sections[s].items[i].image = await uploadPhotoGithub(data);
    render();
    scheduleSave();
  } catch {
    saveState.textContent = "Fotoğraf yüklenemedi";
  }
});

document.getElementById("addCat").addEventListener("click", () => {
  menu.sections.push({ id: uid(), title: "Yeni kategori", items: [] });
  render();
  scheduleSave();
});

["restName", "restSub", "restKicker"].forEach((id) => {
  document.getElementById(id).addEventListener("input", scheduleSave);
});

document.getElementById("logoutBtn").addEventListener("click", () => {
  sessionStorage.removeItem(TOKEN_KEY);
  location.reload();
});

document.getElementById("loginForm").addEventListener("submit", async (e) => {
  e.preventDefault();
  loginErr.hidden = true;
  sessionStorage.setItem(TOKEN_KEY, document.getElementById("pin").value.trim());
  try {
    await verifyGithubToken();
    await showEditor();
  } catch {
    sessionStorage.removeItem(TOKEN_KEY);
    loginErr.hidden = false;
    loginErr.textContent = "Token geçersiz veya bu repoya yetkisi yok.";
  }
});

async function showEditor() {
  menu = await loadMenuFile();
  setup.hidden = true;
  gate.hidden = true;
  app.hidden = false;
  render();
}

async function start() {
  if (!githubReady()) {
    gate.hidden = true;
    setup.hidden = false;
    return;
  }
  if (ghToken()) {
    try {
      await verifyGithubToken();
      await showEditor();
      return;
    } catch {
      sessionStorage.removeItem(TOKEN_KEY);
    }
  }
}

start();
