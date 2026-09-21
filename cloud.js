const TOKEN_KEY = "firat_gh_token";

function githubReady() {
  const c = window.GITHUB_CONFIG || {};
  return Boolean(c.owner && c.repo);
}

function ghToken() {
  return sessionStorage.getItem(TOKEN_KEY) || "";
}

function emptyMenu() {
  return {
    name: "Fırat Kebap",
    subtitle: "Pide · Lahmacun Salonu",
    kicker: "",
    note: "",
    sections: typeof MENU !== "undefined" ? MENU.map(copySection) : [],
  };
}

function copySection(section) {
  return {
    id: section.id,
    title: section.title,
    items: (section.items || []).map((item) => ({
      id: item.id || Math.random().toString(36).slice(2),
      name: item.name,
      desc: item.desc || "",
      price: Number(item.price || 0),
      image: item.image || "",
    })),
  };
}

function mediaUrl(path) {
  if (!path) return "";
  if (/^https?:/i.test(path)) return path;
  const c = window.GITHUB_CONFIG || {};
  const clean = String(path).replace(/^\.\//, "").replace(/^\//, "");
  if (c.owner && c.repo) {
    return `https://raw.githubusercontent.com/${c.owner}/${c.repo}/${c.branch || "main"}/${clean}`;
  }
  return clean;
}

function utf8ToBase64(text) {
  const bytes = new TextEncoder().encode(text);
  let binary = "";
  bytes.forEach((b) => {
    binary += String.fromCharCode(b);
  });
  return btoa(binary);
}

function dataUrlToBase64(dataUrl) {
  const i = dataUrl.indexOf(",");
  return (i >= 0 ? dataUrl.slice(i + 1) : dataUrl).replace(/\s/g, "");
}

function contentsUrl(path) {
  const c = window.GITHUB_CONFIG;
  const encoded = String(path)
    .split("/")
    .filter(Boolean)
    .map(encodeURIComponent)
    .join("/");
  return `https://api.github.com/repos/${c.owner}/${c.repo}/contents/${encoded}`;
}

async function ghContents(path, options = {}) {
  const { headers: extraHeaders, ...rest } = options;
  const headers = {
    Accept: "application/vnd.github+json",
    ...(extraHeaders || {}),
  };
  if (ghToken()) headers.Authorization = "Bearer " + ghToken();
  if (rest.body) headers["Content-Type"] = "application/json";
  const res = await fetch(contentsUrl(path), { ...rest, headers });
  const data = await res.json().catch(() => ({}));
  if (!res.ok) {
    const err = new Error(data.message || "GitHub kaydı başarısız");
    err.status = res.status;
    err.data = data;
    throw err;
  }
  return data;
}

async function loadMenuFile() {
  const res = await fetch("data/menu.json?t=" + Date.now(), { cache: "no-store" });
  if (!res.ok) return emptyMenu();
  const data = await res.json();
  if (!Array.isArray(data.sections)) data.sections = [];
  return data;
}

async function getSha(path) {
  try {
    const file = await ghContents(path);
    return file.sha;
  } catch (err) {
    if (err.status === 404) return undefined;
    throw err;
  }
}

async function putFile(path, contentBase64, message) {
  const c = window.GITHUB_CONFIG;
  const body = {
    message,
    content: contentBase64,
    branch: c.branch || "main",
  };
  const sha = await getSha(path);
  if (sha) body.sha = sha;
  return ghContents(path, { method: "PUT", body: JSON.stringify(body) });
}

async function saveMenuGithub(menu) {
  const path = window.GITHUB_CONFIG.menuPath || "data/menu.json";
  const json = JSON.stringify(menu, null, 2);
  await putFile(path, utf8ToBase64(json), "Menü güncellendi");
}

async function uploadPhotoGithub(dataUrl) {
  const name = "uploads/" + Date.now() + "-" + Math.random().toString(36).slice(2, 8) + ".jpg";
  const result = await putFile(name, dataUrlToBase64(dataUrl), "Urun fotografi eklendi");
  return (result.content && result.content.download_url) || mediaUrl(name);
}

async function verifyGithubToken() {
  const c = window.GITHUB_CONFIG;
  const headers = {
    Accept: "application/vnd.github+json",
    Authorization: "Bearer " + ghToken(),
  };
  const me = await fetch("https://api.github.com/user", { headers });
  if (!me.ok) throw Object.assign(new Error("auth"), { status: me.status });
  const repo = await fetch(`https://api.github.com/repos/${c.owner}/${c.repo}`, { headers });
  if (!repo.ok) throw Object.assign(new Error("auth"), { status: repo.status });
  const info = await repo.json();
  if (info.permissions && info.permissions.push === false) {
    throw Object.assign(new Error("no_push"), { status: 403 });
  }
  return info;
}
