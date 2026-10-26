// Página de flasheo: lee site.json (generado por scripts/build_flasher_site.py)
// y pinta una tarjeta por variante para la versión elegida.

const versionSelect = document.getElementById('version');
const versionBar = document.getElementById('version-bar');
const versionMeta = document.getElementById('version-meta');
const grid = document.getElementById('variants');
const empty = document.getElementById('empty');
const template = document.getElementById('card-template');

let site = null;

function formatDate(iso) {
  if (!iso) return '';
  const date = new Date(iso);
  if (Number.isNaN(date.getTime())) return '';
  return date.toLocaleDateString('es-CL', { year: 'numeric', month: 'long', day: 'numeric' });
}

function selectedVersion() {
  return site.versions.find((v) => v.tag === versionSelect.value) || site.versions[0];
}

function renderVersionMeta(version) {
  versionMeta.textContent = '';
  const date = formatDate(version.date);
  if (date) versionMeta.append(`Publicada el ${date}. `);
  if (version.url) {
    const link = document.createElement('a');
    link.href = version.url;
    link.textContent = 'Notas de la versión';
    versionMeta.append(link);
  }
}

function renderCards() {
  const version = selectedVersion();
  renderVersionMeta(version);
  grid.replaceChildren();

  for (const variant of site.variants) {
    if (!version.envs.includes(variant.env)) continue;
    const base = `firmware/${version.tag}/${variant.slug}`;
    const card = template.content.firstElementChild.cloneNode(true);
    card.querySelector('.card-name').textContent = variant.name;
    card.querySelector('.card-desc').textContent = variant.description;
    card.querySelector('.install').setAttribute('manifest', `${base}/manifest.json`);
    const zip = card.querySelector('.card-zip');
    zip.href = `${base}/${version.packages[variant.env]}`;
    const manual = card.querySelector('.card-manual');
    manual.href = variant.manual;
    grid.append(card);
  }
}

async function init() {
  document.getElementById('unsupported').hidden = 'serial' in navigator;

  const response = await fetch('site.json', { cache: 'no-cache' });
  site = await response.json();

  if (site.versions.length === 0) {
    empty.hidden = false;
    return;
  }

  for (const version of site.versions) {
    const option = document.createElement('option');
    option.value = version.tag;
    option.textContent = version === site.versions[0] ? `${version.tag} (última)` : version.tag;
    versionSelect.append(option);
  }

  // Permite enlazar una versión fija: ...?v=v1.2.0
  const requested = new URLSearchParams(location.search).get('v');
  if (requested && site.versions.some((v) => v.tag === requested)) {
    versionSelect.value = requested;
  }

  versionSelect.addEventListener('change', () => {
    const url = new URL(location.href);
    url.searchParams.set('v', versionSelect.value);
    history.replaceState(null, '', url);
    renderCards();
  });

  versionBar.hidden = false;
  renderCards();
}

init().catch(() => {
  empty.textContent = 'No se pudo cargar la lista de versiones.';
  empty.hidden = false;
});
