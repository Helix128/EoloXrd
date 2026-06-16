const $ = id => document.getElementById(id);
let logFiles = [], flowSections = [], presets = [];
const MAX_SECTIONS = 8, INF = 4294967295;

// Helper: Format bytes to human readable
function fmtSize(n) {
  if (n > 1048576) return (n / 1048576).toFixed(1) + ' MB';
  if (n > 1024) return (n / 1024).toFixed(1) + ' KB';
  return n + ' B';
}

// Toast Notifications
function notify(msg, type = 'info', timeout = 3500) {
  if (!document.querySelector('.toast-container')) {
    const tc = document.createElement('div');
    tc.className = 'toast-container';
    document.body.appendChild(tc);
  }
  const el = document.createElement('div');
  el.className = 'toast ' + (type === 'success' ? 'success' : type === 'error' ? 'error' : '');
  el.textContent = msg;
  document.querySelector('.toast-container').appendChild(el);
  setTimeout(() => {
    el.style.opacity = '0';
    setTimeout(() => el.remove(), 300);
  }, timeout);
}

// Time Split Helpers
function secToMinSec(totalSeconds) {
  if (totalSeconds === INF || totalSeconds === '' || totalSeconds === null || totalSeconds === undefined || isNaN(totalSeconds)) {
    return { min: '', sec: '' };
  }
  return {
    min: Math.floor(totalSeconds / 60),
    sec: totalSeconds % 60
  };
}

function minSecToSec(min, sec) {
  return (Number(min || 0) * 60) + Number(sec || 0);
}

function formatDuration(totalSeconds) {
  if (totalSeconds === INF || Number(totalSeconds) === INF) return 'Sin límite';
  const n = Number(totalSeconds || 0);
  if (n <= 0) return '0 s';
  const h = Math.floor(n / 3600);
  const m = Math.floor((n % 3600) / 60);
  const s = n % 60;
  return [h ? `${h} h` : '', m ? `${m} min` : '', s ? `${s} s` : ''].filter(Boolean).join(' ') || '0 s';
}

function clampNumber(value, min, max) {
  if (value === '') return '';
  const n = Number(value);
  if (isNaN(n)) return '';
  return Math.max(min, Math.min(max, Math.floor(n)));
}

function normalizeTimePair(minInput, secInput) {
  minInput.value = clampNumber(minInput.value, 0, 1440);
  secInput.value = clampNumber(secInput.value, 0, 59);
  return minSecToSec(minInput.value, secInput.value);
}

function sectionStart(index) {
  return flowSections.slice(0, index).reduce((sum, s) => sum + Number(s.durationSeconds || 0), 0);
}

// Theme Toggle Logic
function initTheme() {
  const savedTheme = localStorage.getItem('eolo-theme');
  const prefersLight = window.matchMedia('(prefers-color-scheme: light)').matches;
  const activeTheme = savedTheme || (prefersLight ? 'light' : 'dark');
  setTheme(activeTheme);
}

function setTheme(theme) {
  document.documentElement.setAttribute('data-theme', theme);
  localStorage.setItem('eolo-theme', theme);
  if (theme === 'light') {
    document.querySelector('.icon-sun').classList.remove('hidden');
    document.querySelector('.icon-moon').classList.add('hidden');
  } else {
    document.querySelector('.icon-sun').classList.add('hidden');
    document.querySelector('.icon-moon').classList.remove('hidden');
  }
}

$('themeToggle').addEventListener('click', () => {
  const current = document.documentElement.getAttribute('data-theme');
  setTheme(current === 'light' ? 'dark' : 'light');
});

// Drawer Navigation Router
const menuToggle = $('menuToggle');
const closeDrawer = $('closeDrawer');
const navDrawer = $('navDrawer');
const drawerOverlay = $('drawerOverlay');

function openMenu() {
  navDrawer.classList.add('open');
  drawerOverlay.classList.add('open');
}

function closeMenu() {
  navDrawer.classList.remove('open');
  drawerOverlay.classList.remove('open');
}

menuToggle.addEventListener('click', openMenu);
closeDrawer.addEventListener('click', closeMenu);
drawerOverlay.addEventListener('click', closeMenu);

document.querySelectorAll('.nav-item').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.nav-item').forEach(b => b.classList.remove('active'));
    btn.classList.add('active');
    
    const target = btn.dataset.target;
    document.querySelectorAll('.section-view').forEach(view => view.classList.remove('active'));
    $(target).classList.add('active');
    closeMenu();
  });
});

// Time split duration managers
function setDuration(v) {
  if (v === 'infinite' || Number(v) === INF) {
    $('durationMode').value = 'infinite';
    $('durationField').classList.add('hidden');
  } else {
    $('durationMode').value = 'finite';
    $('durationField').classList.remove('hidden');
    const t = secToMinSec(v);
    $('durMin').value = t.min;
    $('durSec').value = t.sec;
  }
  updateScheduleStatus();
}

function durationSeconds() {
  return $('durationMode').value === 'infinite' ? INF : minSecToSec($('durMin').value, $('durSec').value);
}

function waitSeconds() {
  return minSecToSec($('waitMin').value, $('waitSec').value);
}

function flowMode() {
  const checked = document.querySelector('input[name="flowMode"]:checked');
  return checked ? checked.value : 'fixed';
}

function setFlowMode(mode) {
  const option = document.querySelector(`input[name="flowMode"][value="${mode}"]`);
  if (option) option.checked = true;
  updateFlowModeUI();
}

function updateFlowModeUI() {
  const periods = flowMode() === 'periods';
  document.querySelectorAll('.fixed-flow-field').forEach(el => el.classList.toggle('hidden', periods));
  $('periodFlowFields').classList.toggle('hidden', !periods);
  updateScheduleStatus();
}

function formatRtc(s) {
  try {
    const d = new Date(s);
    if (isNaN(d)) return s;
    return d.toLocaleString();
  } catch (e) {
    return s;
  }
}

function safePresetName(name) {
  return /^[A-Za-z0-9_-]{1,23}$/.test(name) && !name.includes('..');
}

function sectionTotal() {
  return flowSections.reduce((sum, s) => sum + Number(s.durationSeconds || 0), 0);
}

function validateFlowConfig() {
  let msg = '';
  if (flowMode() === 'fixed') {
    const f = parseFloat($('targetFlow').value);
    if ($('durationMode').value !== 'infinite' && durationSeconds() <= 0) msg = 'Duración total debe ser mayor a 0 o elegir sin límite.';
    if (isNaN(f) || f < 0 || f > 8) msg = 'Flujo fijo debe estar entre 0 y 8 L/min.';
  } else if (!flowSections.length) {
    msg = 'Agregue al menos un tramo o use una plantilla.';
  } else {
    for (let i = 0; i < flowSections.length; i++) {
      const s = flowSections[i];
      if (!s.durationSeconds || s.durationSeconds <= 0) {
        msg = `Tramo ${i + 1}: duración debe ser mayor a 0.`;
        break;
      }
      if (s.targetFlow === '' || isNaN(s.targetFlow) || s.targetFlow < 0 || s.targetFlow > 8) {
        msg = `Tramo ${i + 1}: flujo debe estar entre 0 y 8 L/min.`;
        break;
      }
    }
    if (!msg && sectionTotal() <= 0) msg = 'La suma de tramos debe ser mayor a 0.';
  }
  $('flowWarning').textContent = msg;
  return !msg;
}

function syncScheduleField() {
  $('flowSchedule').value = flowMode() === 'periods'
    ? JSON.stringify(flowSections.map(s => ({
        durationSeconds: Number(s.durationSeconds),
        targetFlow: Number(s.targetFlow)
      })))
    : '[]';
}

function updateCaptureSummary() {
  if (!$('captureSummary')) return;
  const periods = flowMode() === 'periods';
  const last = flowSections[flowSections.length - 1];
  const finalFlow = periods
    ? (last && last.targetFlow !== '' && !isNaN(last.targetFlow) ? `${Number(last.targetFlow).toFixed(1)} L/min` : 'Sin definir')
    : ($('targetFlow').value ? `${Number($('targetFlow').value).toFixed(1)} L/min` : 'Sin definir');
  $('summaryWait').textContent = formatDuration(waitSeconds());
  $('summaryDuration').textContent = periods ? `${formatDuration(sectionTotal())} calculada` : formatDuration(durationSeconds());
  $('summaryMode').textContent = periods ? 'Perfil por tramos' : 'Flujo fijo';
  $('summaryFinalFlow').textContent = finalFlow;
}

function updateScheduleStatus() {
  if (!$('scheduleStatus')) return;
  const total = sectionTotal();
  if (flowMode() === 'periods') {
    const last = flowSections[flowSections.length - 1];
    const lastFlow = last && last.targetFlow !== '' && !isNaN(last.targetFlow) ? ` · flujo final ${Number(last.targetFlow).toFixed(1)} L/min` : '';
    $('scheduleStatus').textContent = flowSections.length
      ? `${flowSections.length} tramo${flowSections.length === 1 ? '' : 's'} · duración total calculada: ${formatDuration(total)}${lastFlow}`
      : 'Sin tramos: agregue uno o use una plantilla.';
  } else {
    $('scheduleStatus').textContent = `Flujo fijo${$('targetFlow').value ? ': ' + $('targetFlow').value + ' L/min' : ' sin definir'}`;
  }
  syncScheduleField();
  updateCaptureSummary();
  validateFlowConfig();
}

function renderSections() {
  const body = $('sectionsBody');
  if (!flowSections.length) {
    body.innerHTML = '<div class="sections-empty"><strong>Sin tramos</strong><span>Agrega uno o usa una plantilla para armar la secuencia.</span></div>';
    updateScheduleStatus();
    return;
  }
  body.innerHTML = `
    <div class="sections-table" role="table" aria-label="Perfil de flujo por tramos">
      <div class="sections-row sections-head" role="row">
        <span>Tramo</span><span>Inicio</span><span>Duración</span><span>Fin</span><span>Flujo</span><span>Acciones</span>
      </div>
      ${flowSections.map((s, i) => {
        const start = sectionStart(i);
        const duration = Number(s.durationSeconds || 0);
        const end = start + duration;
        const timeObj = duration ? secToMinSec(duration) : { min: '', sec: '' };
        const flowValue = s.targetFlow === '' || s.targetFlow === null || s.targetFlow === undefined ? '' : Number(s.targetFlow || 0).toFixed(1);
        const fill = Math.max(0, Math.min(100, Number(s.targetFlow || 0) / 8 * 100));
        return `
          <div class="sections-row flow-section-card" role="row">
            <div class="section-index" data-label="Tramo"><span>${i + 1}</span></div>
            <div class="section-time" data-label="Inicio"><strong>${formatDuration(start)}</strong></div>
            <div class="section-duration" data-label="Duración">
              <div class="time-inputs-container compact-time">
                <div class="time-field">
                  <input data-section-min="${i}" type="number" min="0" max="1440" placeholder="Min" value="${timeObj.min}">
                  <span class="time-unit">M</span>
                </div>
                <div class="time-field-separator">:</div>
                <div class="time-field">
                  <input data-section-sec="${i}" type="number" min="0" max="59" placeholder="Seg" value="${timeObj.sec}">
                  <span class="time-unit">S</span>
                </div>
              </div>
            </div>
            <div class="section-time" data-label="Fin"><strong>${formatDuration(end)}</strong></div>
            <div class="section-flow" data-label="Flujo">
              <input id="sectionFlow${i}" data-section-flow="${i}" type="number" min="0" max="8" step="0.1" placeholder="0.0" value="${flowValue}" aria-label="Flujo tramo ${i + 1}">
              <div class="flow-rail" aria-hidden="true"><span style="width:${fill}%"></span></div>
            </div>
            <div class="section-actions" data-label="Acciones">
              <button type="button" title="Subir" data-section-move="${i}" data-direction="-1" ${i === 0 ? 'disabled' : ''}>↑</button>
              <button type="button" title="Bajar" data-section-move="${i}" data-direction="1" ${i === flowSections.length - 1 ? 'disabled' : ''}>↓</button>
              <button type="button" title="Duplicar" data-section-duplicate="${i}">Duplicar</button>
              <button type="button" class="btn-section-remove" data-section-remove="${i}">Quitar</button>
            </div>
          </div>
        `;
      }).join('')}
    </div>
  `;
  updateScheduleStatus();
}

function setSections(sections) {
  flowSections = (sections || []).slice(0, MAX_SECTIONS).map(s => ({
    durationSeconds: Number(s.durationSeconds || 0),
    targetFlow: s.targetFlow === '' ? '' : Number(s.targetFlow || 0)
  }));
  renderSections();
}

function applyConfig(c) {
  const w = secToMinSec(c.waitSeconds || 0);
  $('waitMin').value = w.min;
  $('waitSec').value = w.sec;
  $('targetFlow').value = Number(c.targetFlow || 0).toFixed(1);
  const loadedSections = c.flowSections || [];
  setDuration(Number(c.durationSeconds) === INF ? 'infinite' : c.durationSeconds || 300);
  setSections(loadedSections);
  setFlowMode(loadedSections.length ? 'periods' : 'fixed');
  updateScheduleStatus();
}

function setSystemState(cls) {
  const el = $('systemSummary');
  el.classList.remove('state-ok', 'state-warn', 'state-error');
  if (cls) el.classList.add(cls);
}

async function loadStatus() {
  try {
    const r = await fetch('/api/status');
    if (!r.ok) throw new Error('status_failed');
    const s = await r.json();
    const rtcText = formatRtc(s.rtc);
    $('statusPills').innerHTML = `
      <span class="pill ${s.sdReady ? 'ok' : 'bad'}">SD: ${s.sdStatus.toLowerCase()}</span>
      <span class="pill">RTC: ${rtcText}</span>
    `;
    $('systemStateText').textContent = s.sdReady ? 'Listo para configurar' : 'Revisar la tarjeta SD';
    $('systemStateDetails').textContent = `SD: ${s.sdStatus.toLowerCase()} · RTC: ${rtcText}`;
    setSystemState(s.sdReady ? 'state-ok' : 'state-warn');
    if (s.defaults) applyConfig(s.defaults);
    $('setupStatus').textContent = 'Listo';
  } catch (e) {
    console.warn('Error fetching status', e);
    $('statusPills').innerHTML = '<span class="pill bad">Sin conexión</span>';
    $('systemStateText').textContent = 'No se pudo leer el estado';
    $('systemStateDetails').textContent = 'Asegúrate de estar conectado a la red del dispositivo.';
    setSystemState('state-error');
    $('setupStatus').textContent = 'Error de conexión';
  }
}

function updateLogStatBar() {
  const bar = $('logStatBar');
  if (!bar) return;
  if (!logFiles.length) {
    bar.innerHTML = '<span>Sin archivos en la SD</span>';
    return;
  }
  const totalBytes = logFiles.reduce((sum, f) => sum + (f.size || 0), 0);
  bar.innerHTML = `<strong>${logFiles.length}</strong> archivo${logFiles.length !== 1 ? 's' : ''} <span class="log-stat-sep">·</span> <strong>${fmtSize(totalBytes)}</strong> en total`;
}

async function loadLogs() {
  const body = $('logsBody');
  body.innerHTML = '<tr><td colspan="3" class="empty-table">Cargando registros...</td></tr>';
  try {
    const r = await fetch('/api/logs');
    if (!r.ok) {
      body.innerHTML = '<tr><td colspan="3" class="empty-table">SD no disponible</td></tr>';
      logFiles = [];
      updateLogStatBar();
      return;
    }
    const j = await r.json();
    logFiles = j.files || [];
    updateLogStatBar();
    if (!logFiles.length) {
      body.innerHTML = '<tr><td colspan="3" class="empty-table">No hay archivos CSV en la SD</td></tr>';
      return;
    }
    body.innerHTML = logFiles.map(f => `
      <tr>
        <td data-label="Archivo">${f.name}</td>
        <td data-label="Tamaño">${fmtSize(f.size)}</td>
        <td class="text-right" data-label="Acciones">
          <div class="row-actions">
            <button class="log-action action-view" data-preview="${f.name}">Ver</button>
            <a class="log-action action-download" href="/download?file=${encodeURIComponent(f.name)}">Descargar</a>
            <button class="log-action action-delete" data-delete-log="${f.name}">Borrar</button>
          </div>
        </td>
      </tr>
    `).join('');
  } catch (e) {
    body.innerHTML = '<tr><td colspan="3" class="empty-table">Error al listar registros</td></tr>';
    logFiles = [];
    updateLogStatBar();
  }
}

async function preview(name) {
  $('preview').textContent = 'Cargando...';
  try {
    const r = await fetch('/api/logs/preview?file=' + encodeURIComponent(name));
    if (!r.ok) {
      $('preview').textContent = 'No se pudo cargar la vista previa.';
      return;
    }
    const j = await r.json();
    $('preview').textContent = [j.header].concat(j.rows || []).join('\n');
  } catch (e) {
    $('preview').textContent = 'Error al procesar la vista previa.';
  }
}

async function deleteLog(name) {
  if (!confirm(`¿Está seguro de que desea eliminar el registro '${name}' permanentemente de la tarjeta SD?`)) return;
  try {
    const fd = new URLSearchParams();
    fd.set('file', name);
    const r = await fetch('/api/logs/delete', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: fd
    });
    const j = await r.json();
    if (r.ok && j.ok) {
      notify('Registro eliminado con éxito', 'success');
      loadLogs();
      if ($('preview').textContent.startsWith(name) || $('preview').textContent.includes(name)) {
        $('preview').textContent = 'Seleccione un archivo CSV del explorador superior.';
      }
    } else {
      notify('Error al eliminar: ' + (j.error || 'respuesta incorrecta'), 'error');
    }
  } catch (e) {
    notify('Error al enviar solicitud de eliminación', 'error');
  }
}

async function loadPresets() {
  const grid = $('presetsGrid');
  grid.innerHTML = '<div class="presets-empty">Cargando presets...</div>';
  try {
    const r = await fetch('/api/presets');
    if (!r.ok) throw new Error('presets_failed');
    const j = await r.json();
    presets = j.presets || [];
    if (!presets.length) {
      grid.innerHTML = '<div class="presets-empty">No hay presets guardados.</div>';
      return;
    }
    grid.innerHTML = presets.map(p => {
      const dFmt = secToMinSec(p.durationSeconds);
      const dStr = Number(p.durationSeconds) === INF ? 'Sin límite' : `${dFmt.min}m ${dFmt.sec}s`;
      return `
        <div class="preset-card">
          <div class="preset-card-title">${p.name}</div>
          <div class="preset-card-details">
            <span>Flujo: ${Number(p.targetFlow).toFixed(1)} L/min</span>
            <span>Duración: ${dStr}</span>
            <span>Tramos: ${p.flowSectionCount || 0}</span>
          </div>
          <div class="preset-card-actions">
            <button type="button" class="btn-card-load" data-load-preset="${p.name}">Cargar</button>
            <button type="button" class="btn-card-delete" data-delete-preset="${p.name}">Borrar</button>
          </div>
        </div>
      `;
    }).join('');
  } catch (e) {
    grid.innerHTML = '<div class="presets-empty">Presets no disponibles</div>';
  }
}

function formBody(extra) {
  syncScheduleField();
  const periods = flowMode() === 'periods';
  const waitVal = waitSeconds();
  const last = flowSections[flowSections.length - 1];
  const durVal = periods ? sectionTotal() : durationSeconds();
  const targetVal = periods && last ? Number(last.targetFlow || 0).toFixed(1) : $('targetFlow').value;
  const fd = new URLSearchParams();
  fd.set('waitSeconds', waitVal);
  fd.set('durationSeconds', durVal);
  fd.set('targetFlow', targetVal);
  fd.set('flowSchedule', $('flowSchedule').value);
  if (!periods && $('durationMode').value === 'infinite') fd.set('durationMode', 'infinite');
  if (extra) {
    Object.keys(extra).forEach(k => fd.set(k, extra[k]));
  }
  return fd;
}

async function loadPreset(name) {
  try {
    const r = await fetch('/api/presets/load?name=' + encodeURIComponent(name));
    const j = await r.json();
    if (r.ok && j.config) {
      applyConfig(j.config);
      $('presetName').value = j.name;
      notify('Preset cargado correctamente', 'success');
    } else {
      notify('No se pudo cargar el preset', 'error');
    }
  } catch (e) {
    notify('Error de conexión cargando preset', 'error');
  }
}

async function savePreset() {
  const name = $('presetName').value.trim();
  if (!safePresetName(name)) {
    notify('Nombre de preset inválido. Use solo letras, números, _ o - (máx 23)', 'error');
    return;
  }
  if (!validateFlowConfig()) {
    notify('Configuración de flujo o tramos inválida', 'error');
    return;
  }
  try {
    const r = await fetch('/api/presets/save', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: formBody({ name })
    });
    const j = await r.json();
    if (r.ok && j.ok) {
      notify('Preset guardado con éxito', 'success');
      loadPresets();
    } else {
      notify('Error al guardar preset: ' + (j.error || ''), 'error');
    }
  } catch (e) {
    notify('Error de red guardando preset', 'error');
  }
}

async function deletePreset(name) {
  if (!confirm(`¿Está seguro de que desea eliminar el preset '${name}'?`)) return;
  try {
    const fd = new URLSearchParams();
    fd.set('name', name);
    const r = await fetch('/api/presets/delete', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: fd
    });
    const j = await r.json();
    if (r.ok && j.ok) {
      notify('Preset borrado con éxito', 'success');
      loadPresets();
    } else {
      notify('Error al borrar preset: ' + (j.error || ''), 'error');
    }
  } catch (e) {
    notify('Error de red eliminando preset', 'error');
  }
}

function applySectionTemplate(name) {
  const templates = {
    constant: [{ durationSeconds: 300, targetFlow: Number($('targetFlow').value || 5) }],
    stairs: [
      { durationSeconds: 300, targetFlow: 2 },
      { durationSeconds: 300, targetFlow: 4 },
      { durationSeconds: 300, targetFlow: 6 }
    ],
    flush: [
      { durationSeconds: 120, targetFlow: 8 },
      { durationSeconds: 180, targetFlow: 5 },
      { durationSeconds: 60, targetFlow: 0 }
    ],
    empty: []
  };
  setSections(templates[name] || []);
  setFlowMode('periods');
}

// Event delegation listeners
document.addEventListener('click', e => {
  const t = e.target.closest('button');
  if (!t) return;
  
  if (t.dataset.wait !== undefined) {
    const waitSecs = Number(t.dataset.wait);
    const w = secToMinSec(waitSecs);
    $('waitMin').value = w.min;
    $('waitSec').value = w.sec;
    updateScheduleStatus();
  }
  if (t.dataset.duration !== undefined) {
    setDuration(t.dataset.duration);
  }
  if (t.dataset.preview !== undefined) preview(t.dataset.preview);
  if (t.dataset.deleteLog !== undefined) deleteLog(t.dataset.deleteLog);
  if (t.dataset.loadPreset !== undefined) loadPreset(t.dataset.loadPreset);
  if (t.dataset.deletePreset !== undefined) deletePreset(t.dataset.deletePreset);
  
  if (t.dataset.template !== undefined) {
    applySectionTemplate(t.dataset.template);
  }
  if (t.dataset.sectionRemove !== undefined) {
    flowSections.splice(Number(t.dataset.sectionRemove), 1);
    renderSections();
  }
  if (t.dataset.sectionDuplicate !== undefined) {
    if (flowSections.length >= MAX_SECTIONS) {
      notify('Máximo programable de ' + MAX_SECTIONS + ' tramos superado', 'error');
    } else {
      const idx = Number(t.dataset.sectionDuplicate);
      flowSections.splice(idx + 1, 0, { ...flowSections[idx] });
      renderSections();
    }
  }
  if (t.dataset.sectionMove !== undefined) {
    const idx = Number(t.dataset.sectionMove);
    const next = idx + Number(t.dataset.direction || 0);
    if (next >= 0 && next < flowSections.length) {
      const [item] = flowSections.splice(idx, 1);
      flowSections.splice(next, 0, item);
      renderSections();
    }
  }
  validateFlowConfig();
});

document.addEventListener('input', e => {
  const t = e.target;
  if (t.dataset.sectionMin !== undefined) {
    const idx = Number(t.dataset.sectionMin);
    const minInput = t;
    const secInput = document.querySelector(`[data-section-sec="${idx}"]`);
    flowSections[idx].durationSeconds = !minInput.value && !secInput.value ? '' : normalizeTimePair(minInput, secInput);
    updateScheduleStatus();
  }
  if (t.dataset.sectionSec !== undefined) {
    const idx = Number(t.dataset.sectionSec);
    const minInput = document.querySelector(`[data-section-min="${idx}"]`);
    const secInput = t;
    flowSections[idx].durationSeconds = !minInput.value && !secInput.value ? '' : normalizeTimePair(minInput, secInput);
    updateScheduleStatus();
  }
  if (t.dataset.sectionFlow !== undefined) {
    const idx = Number(t.dataset.sectionFlow);
    flowSections[idx].targetFlow = t.value === '' ? '' : Number(t.value);
    const card = t.closest('.flow-section-card');
    if (card) {
      const value = flowSections[idx].targetFlow;
      const fill = Math.max(0, Math.min(100, Number(value || 0) / 8 * 100));
      const rail = card.querySelector('.flow-rail span');
      if (rail) rail.style.width = fill + '%';
    }
    updateScheduleStatus();
  }
});

document.addEventListener('focusout', e => {
  const t = e.target;
  if (t.dataset && (t.dataset.sectionMin !== undefined || t.dataset.sectionSec !== undefined)) renderSections();
});

function bindTimePair(minId, secId) {
  const minInput = $(minId);
  const secInput = $(secId);
  [minInput, secInput].forEach(el => {
    el.addEventListener('input', () => {
      normalizeTimePair(minInput, secInput);
      updateScheduleStatus();
    });
    el.addEventListener('blur', () => {
      normalizeTimePair(minInput, secInput);
      updateScheduleStatus();
    });
  });
}

// Manual form input bindings
bindTimePair('waitMin', 'waitSec');
bindTimePair('durMin', 'durSec');
$('targetFlow').addEventListener('input', updateScheduleStatus);
document.querySelectorAll('input[name="flowMode"]').forEach(el => el.addEventListener('change', updateFlowModeUI));

$('durationMode').addEventListener('change', e => {
  setDuration(e.target.value === 'infinite' ? 'infinite' : minSecToSec($('durMin').value, $('durSec').value) || 300);
});

$('addSection').addEventListener('click', () => {
  if (flowSections.length >= MAX_SECTIONS) {
    notify('Máximo programable de ' + MAX_SECTIONS + ' tramos superado', 'error');
    return;
  }
  flowSections.push({ durationSeconds: '', targetFlow: '' });
  renderSections();
});

$('clearSections').addEventListener('click', () => setSections([]));
$('refreshLogs').addEventListener('click', loadLogs);
$('downloadAll').addEventListener('click', () => {
  if (!logFiles.length) {
    notify('No hay archivos CSV para descargar', 'info');
    return;
  }
  logFiles.forEach(f => {
    const a = document.createElement('a');
    a.href = '/download?file=' + encodeURIComponent(f.name);
    a.setAttribute('download', f.name);
    a.style.display = 'none';
    document.body.appendChild(a);
    a.click();
    a.remove();
  });
});
$('refreshPresets').addEventListener('click', loadPresets);
$('savePreset').addEventListener('click', savePreset);

$('setupForm').addEventListener('submit', async e => {
  e.preventDefault();
  if (!validateFlowConfig()) {
    notify('La configuración contiene parámetros inválidos', 'error');
    return;
  }
  const btn = $('confirmBtn');
  btn.disabled = true;
  const orig = btn.textContent;
  btn.textContent = 'Enviando...';
  try {
    const r = await fetch('/api/confirm', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: formBody()
    });
    if (r.ok) {
      notify('Configuración confirmada. El Wi‑Fi se desconectará y comenzará la sesión.', 'success');
      $('preview').textContent = 'Configuración guardada e inicio de sesión confirmado.';
    } else {
      const j = await r.json().catch(() => null);
      notify('Error al confirmar: ' + (j && j.error ? j.error : 'parámetros inválidos'), 'error');
    }
  } catch (e) {
    notify('Error de red al enviar confirmación', 'error');
  }
  btn.disabled = false;
  btn.textContent = orig;
});

// Initialization
initTheme();
updateFlowModeUI();
loadStatus().then(loadLogs).then(loadPresets);
