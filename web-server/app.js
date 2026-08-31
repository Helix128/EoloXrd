// EOLO Dron - Panel de Control & Gestor de Captura
// 100% Flat, Sin Emojis, Modular y Optimizado

const $ = id => document.getElementById(id);
const MAX_BLOCKS = 8;
const INF = 4294967295;

let flowBlocks = [];
let logFiles = [];
let presets = [];
let _debugActive = false;
let _debugTimer = null;
let _diagTimer = null;
let _debugMaxPwm = 2047;

// Formateo de bytes
function fmtBytes(bytes) {
  if (!bytes || bytes <= 0) return '0 B';
  if (bytes > 1048576) return (bytes / 1048576).toFixed(1) + ' MB';
  if (bytes > 1024) return (bytes / 1024).toFixed(1) + ' KB';
  return bytes + ' B';
}

// Notificaciones Toast (sin emojis)
function notify(msg, type = 'info', timeout = 3500) {
  const container = $('toastContainer') || document.body;
  const el = document.createElement('div');
  el.className = 'toast ' + (type === 'success' ? 'success' : type === 'error' ? 'error' : '');
  el.textContent = msg;
  container.appendChild(el);
  setTimeout(() => {
    el.style.opacity = '0';
    setTimeout(() => el.remove(), 250);
  }, timeout);
}

// Utilidades de Tiempo
function secToMinSec(totalSeconds) {
  if (totalSeconds === INF || totalSeconds === '' || totalSeconds === null || totalSeconds === undefined || isNaN(totalSeconds)) {
    return { min: 0, sec: 0 };
  }
  const s = Math.max(0, Math.floor(Number(totalSeconds)));
  return {
    min: Math.floor(s / 60),
    sec: s % 60
  };
}

function minSecToSec(min, sec) {
  const m = Math.max(0, Math.floor(Number(min || 0)));
  const s = Math.max(0, Math.min(59, Math.floor(Number(sec || 0))));
  return (m * 60) + s;
}

function formatDuration(totalSeconds) {
  if (totalSeconds === INF || Number(totalSeconds) === INF) return 'Sin limite';
  const n = Number(totalSeconds || 0);
  if (n <= 0) return '0 s';
  const h = Math.floor(n / 3600);
  const m = Math.floor((n % 3600) / 60);
  const s = n % 60;
  return [h ? `${h} h` : '', m ? `${m} min` : '', s ? `${s} s` : ''].filter(Boolean).join(' ') || '0 s';
}

function formatTime(totalSeconds) {
  if (totalSeconds === INF || Number(totalSeconds) === INF) return '--:--';
  const n = Math.max(0, Math.floor(Number(totalSeconds || 0)));
  const h = Math.floor(n / 3600);
  const m = Math.floor((n % 3600) / 60);
  const s = n % 60;
  const mm = String(m).padStart(2, '0');
  const ss = String(s).padStart(2, '0');
  if (h > 0) {
    return `${h}:${mm}:${ss}`;
  }
  return `${mm}:${ss}`;
}

function formatRtc(raw) {
  try {
    const d = new Date(raw);
    if (isNaN(d.getTime())) return raw || '--';
    return d.toLocaleString('es-CL', { hour12: false });
  } catch (_) {
    return raw || '--';
  }
}

// Color según caudal para el timeline (Flat Palette)
function getFlowColor(flow) {
  const f = Number(flow || 0);
  if (f <= 0.05) return '#475569'; // Detenido / purga
  if (f <= 2.5) return '#0284c7';  // Caudal bajo (azul cielo)
  if (f <= 4.5) return '#0d9488';  // Caudal medio-bajo (verde azulado)
  if (f <= 6.0) return '#10b981';  // Caudal nominal 5.0 (esmeralda)
  if (f <= 7.2) return '#f59e0b';  // Caudal alto (ámbar)
  return '#ef4444';                // Caudal máximo 8.0 (rojo coral)
}

// Gestión de Tema Claro / Oscuro
function initTheme() {
  const saved = localStorage.getItem('eolo_theme');
  const theme = saved || 'dark';
  setTheme(theme);
}

function setTheme(theme) {
  document.documentElement.setAttribute('data-theme', theme);
  localStorage.setItem('eolo_theme', theme);
  const isLight = theme === 'light';
  document.querySelector('.icon-sun').classList.toggle('hidden', !isLight);
  document.querySelector('.icon-moon').classList.toggle('hidden', isLight);
}

$('themeToggle').addEventListener('click', () => {
  const current = document.documentElement.getAttribute('data-theme');
  setTheme(current === 'light' ? 'dark' : 'light');
});

// Navegación por pestañas
document.querySelectorAll('.tab-btn').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
    btn.classList.add('active');

    const target = btn.dataset.target;
    document.querySelectorAll('.view-panel').forEach(p => p.classList.remove('active'));
    if ($(target)) $(target).classList.add('active');

    if (target === 'view-debug') {
      startDiagnosticsRefresh();
      if (_debugActive) startDebugRefresh();
    } else {
      stopDebugRefresh();
      stopDiagnosticsRefresh();
    }
  });
});

// Estado y Modo de Flujo
function activeMode() {
  const checked = document.querySelector('input[name="flowMode"]:checked');
  return checked ? checked.value : 'fixed';
}

function setMode(mode) {
  const radio = document.querySelector(`input[name="flowMode"][value="${mode}"]`);
  if (radio) radio.checked = true;
  updateModeView();
}

function updateModeView() {
  const isPeriods = activeMode() === 'periods';
  $('fixedFlowPanel').classList.toggle('hidden', isPeriods);
  $('periodFlowPanel').classList.toggle('hidden', !isPeriods);
  syncHiddenSchedule();
  updateMetrics();
  validateForm();
}

document.querySelectorAll('input[name="flowMode"]').forEach(radio => {
  radio.addEventListener('change', updateModeView);
});

// Duración y Espera en Modo Fijo
function getWaitSeconds() {
  return minSecToSec($('waitMin').value, $('waitSec').value);
}

function getFixedDurationSeconds() {
  if ($('durationMode').value === 'infinite') return INF;
  return minSecToSec($('durMin').value, $('durSec').value);
}

function setFixedDuration(seconds) {
  if (seconds === 'infinite' || Number(seconds) === INF) {
    $('durationMode').value = 'infinite';
    $('durationTimeField').classList.add('hidden');
  } else {
    $('durationMode').value = 'finite';
    $('durationTimeField').classList.remove('hidden');
    const t = secToMinSec(seconds);
    $('durMin').value = t.min;
    $('durSec').value = t.sec;
  }
  updateMetrics();
  validateForm();
}

$('durationMode').addEventListener('change', e => {
  if (e.target.value === 'infinite') {
    $('durationTimeField').classList.add('hidden');
  } else {
    $('durationTimeField').classList.remove('hidden');
  }
  updateMetrics();
  validateForm();
});

// Sincronización del input oculto para el backend
function syncHiddenSchedule() {
  if (activeMode() === 'periods') {
    const arr = flowBlocks.map(b => ({
      durationSeconds: Number(b.durationSeconds || 0),
      targetFlow: Number(b.targetFlow || 0)
    }));
    $('flowSchedule').value = JSON.stringify(arr);
  } else {
    $('flowSchedule').value = '[]';
  }
}

// Métricas en Tiempo Real (Duración, Caudal Ponderado, Volumen Teórico, Espera)
function updateMetrics() {
  const isPeriods = activeMode() === 'periods';
  const waitSec = getWaitSeconds();

  // Espera
  $('metricWait').textContent = formatDuration(waitSec);
  $('metricWaitSub').textContent = waitSec > 0 ? `Inicio en +${formatDuration(waitSec)}` : 'Arranque inmediato';

  if (!isPeriods) {
    // Modo Flujo Fijo
    const durSec = getFixedDurationSeconds();
    const flow = parseFloat($('targetFlow').value) || 0;

    $('metricDuration').textContent = formatDuration(durSec);
    $('metricDurationSub').textContent = durSec === INF ? 'Sin límite de tiempo' : 'Fija';

    $('metricAvgFlow').textContent = `${flow.toFixed(1)} L/min`;
    $('metricAvgFlowSub').textContent = 'Constante';

    if (durSec === INF) {
      $('metricVolume').textContent = '—';
    } else {
      const vol = (flow * durSec) / 60.0;
      $('metricVolume').textContent = `${vol.toFixed(1)} L`;
    }
  } else {
    // Modo Perfil por Bloques Continuos
    const totalSec = flowBlocks.reduce((acc, b) => acc + Number(b.durationSeconds || 0), 0);
    const count = flowBlocks.length;

    $('metricDuration').textContent = formatDuration(totalSec);
    $('metricDurationSub').textContent = `${count} bloque${count === 1 ? '' : 's'} en secuencia`;

    if (totalSec > 0) {
      const weightedSum = flowBlocks.reduce((acc, b) => acc + (Number(b.targetFlow || 0) * Number(b.durationSeconds || 0)), 0);
      const avgFlow = weightedSum / totalSec;
      const totalVol = weightedSum / 60.0;

      $('metricAvgFlow').textContent = `${avgFlow.toFixed(1)} L/min`;
      $('metricAvgFlowSub').textContent = 'Ponderado por tiempo';
      $('metricVolume').textContent = `${totalVol.toFixed(1)} L`;
    } else {
      $('metricAvgFlow').textContent = '0.0 L/min';
      $('metricAvgFlowSub').textContent = 'Sin duración';
      $('metricVolume').textContent = '0.0 L';
    }
  }
}

// ===== RENDERIZADO DEL TIMELINE INTERACTIVO =====
function renderTimeline() {
  const bar = $('timelineBar');
  const ticks = $('timelineTicks');
  const meta = $('timelineMeta');
  const counter = $('blocksCounter');

  if (!bar || !ticks) return;
  counter.textContent = `${flowBlocks.length} / ${MAX_BLOCKS} bloques`;

  if (flowBlocks.length === 0) {
    bar.innerHTML = '<div style="flex:1; display:flex; align-items:center; justify-content:center; color:var(--text-dim); font-size:12px;">Sin bloques configurados</div>';
    ticks.innerHTML = '<span>00:00</span><span>--:--</span>';
    meta.textContent = 'Agrega un bloque o carga una plantilla para comenzar';
    return;
  }

  const totalSec = flowBlocks.reduce((acc, b) => acc + Number(b.durationSeconds || 0), 0);
  meta.textContent = `Secuencia continua · ${formatDuration(totalSec)} total`;

  let accumulatedSec = 0;
  let barHtml = '';
  let tickList = ['00:00'];

  flowBlocks.forEach((b, idx) => {
    const dur = Number(b.durationSeconds || 0);
    const flow = Number(b.targetFlow || 0);
    const startSec = accumulatedSec;
    accumulatedSec += dur;
    const endSec = accumulatedSec;
    tickList.push(formatTime(endSec));

    const flexWeight = Math.max(dur, 20);
    const bgColor = getFlowColor(flow);

    barHtml += `
      <div class="timeline-segment" data-block-index="${idx}" style="flex:${flexWeight}; background-color:${bgColor};" title="Bloque ${idx + 1}: ${formatTime(startSec)} a ${formatTime(endSec)} (${flow.toFixed(1)} L/min)">
        <span class="timeline-seg-title">B${idx + 1} · ${formatTime(dur)}</span>
        <span class="timeline-seg-flow">${flow.toFixed(1)} L/m</span>
      </div>
    `;
  });

  bar.innerHTML = barHtml;

  // Renderizar marcas de tiempo (inicio, hitos y fin)
  if (tickList.length <= 5) {
    ticks.innerHTML = tickList.map(t => `<span>${t}</span>`).join('');
  } else {
    ticks.innerHTML = `<span>${tickList[0]}</span><span>${tickList[Math.floor(tickList.length / 2)]}</span><span>${tickList[tickList.length - 1]}</span>`;
  }

  // Interacción al hacer click en el timeline
  bar.querySelectorAll('.timeline-segment').forEach(seg => {
    seg.addEventListener('click', () => {
      const idx = Number(seg.dataset.blockIndex);
      highlightBlock(idx);
    });
  });
}

function highlightBlock(index) {
  document.querySelectorAll('.block-card').forEach((card, idx) => {
    card.classList.toggle('focused', idx === index);
  });
  const targetCard = $(`blockCard_${index}`);
  if (targetCard) {
    targetCard.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
  }
}

// ===== RENDERIZADO DE TARJETAS DE BLOQUES =====
function renderBlocks() {
  const container = $('blocksList');
  if (!container) return;

  if (flowBlocks.length === 0) {
    container.innerHTML = `
      <div class="empty-state">
        <p>No hay bloques continuos definidos.</p>
        <p style="margin-top:4px; font-size:12px; color:var(--text-dim);">Haz clic en &ldquo;+ Agregar bloque&rdquo; o selecciona una plantilla rápida superior.</p>
      </div>
    `;
    $('addBlockBtn').disabled = false;
    renderTimeline();
    updateMetrics();
    syncHiddenSchedule();
    validateForm();
    return;
  }

  $('addBlockBtn').disabled = flowBlocks.length >= MAX_BLOCKS;

  let accumulatedSec = 0;
  container.innerHTML = flowBlocks.map((b, idx) => {
    const dur = Number(b.durationSeconds || 0);
    const startSec = accumulatedSec;
    accumulatedSec += dur;
    const endSec = accumulatedSec;
    const timeObj = secToMinSec(dur);
    const flowVal = Number(b.targetFlow || 0).toFixed(1);

    return `
      <div class="block-card" id="blockCard_${idx}">
        <div class="block-card-header">
          <div class="block-meta">
            <span class="block-index-badge">Bloque ${idx + 1}</span>
            <span class="block-window">${formatTime(startSec)} &rarr; ${formatTime(endSec)} (${formatDuration(dur)})</span>
          </div>
          <div class="block-actions">
            <button type="button" class="btn-icon-sm" data-move-block="${idx}" data-dir="-1" title="Mover antes" ${idx === 0 ? 'disabled' : ''}>&uarr;</button>
            <button type="button" class="btn-icon-sm" data-move-block="${idx}" data-dir="1" title="Mover despues" ${idx === flowBlocks.length - 1 ? 'disabled' : ''}>&darr;</button>
            <button type="button" class="btn-icon-sm" data-dup-block="${idx}" title="Duplicar bloque" ${flowBlocks.length >= MAX_BLOCKS ? 'disabled' : ''}>Duplicar</button>
            <button type="button" class="btn-icon-sm btn-icon-danger" data-del-block="${idx}" title="Eliminar bloque">Quitar</button>
          </div>
        </div>

        <div class="block-card-body">
          <div class="field-col">
            <label class="field-label">Duraci&oacute;n del bloque</label>
            <div class="time-control-group">
              <div class="time-inputs">
                <div class="time-input-wrap">
                  <input type="number" min="0" max="1440" value="${timeObj.min}" placeholder="0" data-blk-min="${idx}" aria-label="Minutos bloque ${idx + 1}">
                  <span class="unit">min</span>
                </div>
                <span class="time-sep">:</span>
                <div class="time-input-wrap">
                  <input type="number" min="0" max="59" value="${timeObj.sec}" placeholder="0" data-blk-sec="${idx}" aria-label="Segundos bloque ${idx + 1}">
                  <span class="unit">seg</span>
                </div>
              </div>
              <div class="quick-chips">
                <button type="button" class="chip" data-blk-add-min="${idx}" data-val="1">+1m</button>
                <button type="button" class="chip" data-blk-add-min="${idx}" data-val="5">+5m</button>
                <button type="button" class="chip" data-blk-set-min="${idx}" data-val="1">1 min</button>
                <button type="button" class="chip" data-blk-set-min="${idx}" data-val="3">3 min</button>
                <button type="button" class="chip" data-blk-set-min="${idx}" data-val="5">5 min</button>
              </div>
            </div>
          </div>

          <div class="field-col">
            <label class="field-label">Caudal objetivo (L/min)</label>
            <div class="flow-input-group">
              <input type="number" min="0" max="8" step="0.1" value="${flowVal}" placeholder="5.0" data-blk-flow="${idx}" class="text-input flow-num" aria-label="Caudal bloque ${idx + 1}">
              <div class="quick-chips">
                <button type="button" class="chip flow-chip" data-blk-set-flow="${idx}" data-val="2.0">2.0</button>
                <button type="button" class="chip flow-chip" data-blk-set-flow="${idx}" data-val="4.0">4.0</button>
                <button type="button" class="chip flow-chip" data-blk-set-flow="${idx}" data-val="5.0">5.0</button>
                <button type="button" class="chip flow-chip" data-blk-set-flow="${idx}" data-val="6.0">6.0</button>
                <button type="button" class="chip flow-chip" data-blk-set-flow="${idx}" data-val="8.0">8.0</button>
              </div>
            </div>
          </div>
        </div>
      </div>
    `;
  }).join('');

  renderTimeline();
  updateMetrics();
  syncHiddenSchedule();
  validateForm();
}

function setBlocks(blocks) {
  flowBlocks = (blocks || []).slice(0, MAX_BLOCKS).map(b => ({
    durationSeconds: Math.max(1, Number(b.durationSeconds || 300)),
    targetFlow: Math.max(0, Math.min(8.0, Number(b.targetFlow !== undefined ? b.targetFlow : 5.0)))
  }));
  renderBlocks();
}

function addBlock(duration = 300, flow = 5.0) {
  if (flowBlocks.length >= MAX_BLOCKS) {
    notify(`Límite máximo de ${MAX_BLOCKS} bloques alcanzado`, 'error');
    return;
  }
  flowBlocks.push({ durationSeconds: duration, targetFlow: flow });
  renderBlocks();
  highlightBlock(flowBlocks.length - 1);
}

function duplicateBlock(index) {
  if (flowBlocks.length >= MAX_BLOCKS) {
    notify(`Límite máximo de ${MAX_BLOCKS} bloques alcanzado`, 'error');
    return;
  }
  const source = flowBlocks[index];
  flowBlocks.splice(index + 1, 0, { ...source });
  renderBlocks();
  highlightBlock(index + 1);
}

function removeBlock(index) {
  flowBlocks.splice(index, 1);
  renderBlocks();
}

function moveBlock(index, direction) {
  const target = index + direction;
  if (target >= 0 && target < flowBlocks.length) {
    const [item] = flowBlocks.splice(index, 1);
    flowBlocks.splice(target, 0, item);
    renderBlocks();
    highlightBlock(target);
  }
}

function applyTemplate(name) {
  const currentFixedFlow = parseFloat($('targetFlow').value) || 5.0;
  const templates = {
    constant: [{ durationSeconds: 300, targetFlow: currentFixedFlow }],
    stairs: [
      { durationSeconds: 180, targetFlow: 2.0 },
      { durationSeconds: 180, targetFlow: 4.0 },
      { durationSeconds: 180, targetFlow: 6.0 }
    ],
    sample15: [
      { durationSeconds: 300, targetFlow: 5.0 },
      { durationSeconds: 300, targetFlow: 8.0 },
      { durationSeconds: 300, targetFlow: 5.0 }
    ],
    empty: []
  };

  setBlocks(templates[name] || []);
  setMode('periods');
}

// ===== VALIDACIÓN DEL FORMULARIO =====
function validateForm() {
  let error = '';
  const isPeriods = activeMode() === 'periods';

  if (!isPeriods) {
    const flow = parseFloat($('targetFlow').value);
    if (isNaN(flow) || flow < 0 || flow > 8) {
      error = 'El caudal objetivo debe estar entre 0.0 y 8.0 L/min.';
    } else if ($('durationMode').value !== 'infinite' && getFixedDurationSeconds() <= 0) {
      error = 'La duración debe ser mayor a 0 segundos.';
    }
  } else {
    if (flowBlocks.length === 0) {
      error = 'Agrega al menos un bloque de captura.';
    } else {
      for (let i = 0; i < flowBlocks.length; i++) {
        const b = flowBlocks[i];
        if (!b.durationSeconds || b.durationSeconds <= 0) {
          error = `El bloque ${i + 1} debe tener una duración mayor a 0 s.`;
          break;
        }
        if (isNaN(b.targetFlow) || b.targetFlow < 0 || b.targetFlow > 8) {
          error = `El caudal del bloque ${i + 1} debe estar entre 0.0 y 8.0 L/min.`;
          break;
        }
      }
    }
  }

  const warnEl = $('flowWarning');
  const statusEl = $('setupStatus');
  if (error) {
    warnEl.textContent = error;
    warnEl.classList.remove('hidden');
    statusEl.textContent = 'Parámetros incompletos';
    $('confirmBtn').disabled = true;
    return false;
  } else {
    warnEl.textContent = '';
    warnEl.classList.add('hidden');
    statusEl.textContent = 'Listo para iniciar captura';
    $('confirmBtn').disabled = false;
    return true;
  }
}

// ===== DELEGACIÓN DE EVENTOS DE CLIC & INPUT =====
document.addEventListener('click', e => {
  const btn = e.target.closest('button');
  if (!btn) return;

  // Espera inicial quick chips
  if (btn.dataset.wait !== undefined) {
    const s = Number(btn.dataset.wait);
    const t = secToMinSec(s);
    $('waitMin').value = t.min;
    $('waitSec').value = t.sec;
    updateMetrics();
    validateForm();
  }

  // Duración fija quick chips
  if (btn.dataset.duration !== undefined) {
    setFixedDuration(Number(btn.dataset.duration));
  }

  // Caudal fijo quick chips
  if (btn.dataset.setFlow !== undefined) {
    $('targetFlow').value = Number(btn.dataset.setFlow).toFixed(1);
    updateMetrics();
    validateForm();
  }

  // Plantillas de perfil
  if (btn.dataset.template !== undefined) {
    applyTemplate(btn.dataset.template);
  }

  // Acciones de bloque
  if (btn.dataset.delBlock !== undefined) {
    removeBlock(Number(btn.dataset.delBlock));
  }
  if (btn.dataset.dupBlock !== undefined) {
    duplicateBlock(Number(btn.dataset.dupBlock));
  }
  if (btn.dataset.moveBlock !== undefined) {
    moveBlock(Number(btn.dataset.moveBlock), Number(btn.dataset.dir || 0));
  }

  // Ajustes de minutos en bloque (+1m, +5m)
  if (btn.dataset.blkAddMin !== undefined) {
    const idx = Number(btn.dataset.blkAddMin);
    const addSec = Number(btn.dataset.val) * 60;
    flowBlocks[idx].durationSeconds = (Number(flowBlocks[idx].durationSeconds) || 0) + addSec;
    renderBlocks();
  }

  // Fijar minutos en bloque (1m, 3m, 5m)
  if (btn.dataset.blkSetMin !== undefined) {
    const idx = Number(btn.dataset.blkSetMin);
    const min = Number(btn.dataset.val);
    const curSec = secToMinSec(flowBlocks[idx].durationSeconds).sec;
    flowBlocks[idx].durationSeconds = (min * 60) + curSec;
    renderBlocks();
  }

  // Fijar caudal en bloque
  if (btn.dataset.blkSetFlow !== undefined) {
    const idx = Number(btn.dataset.blkSetFlow);
    flowBlocks[idx].targetFlow = Number(btn.dataset.val);
    renderBlocks();
  }
});

// Eventos de entrada de texto/números
document.addEventListener('input', e => {
  const t = e.target;

  // Espera o duración fija
  if (t.id === 'waitMin' || t.id === 'waitSec' || t.id === 'durMin' || t.id === 'durSec' || t.id === 'targetFlow') {
    updateMetrics();
    validateForm();
  }

  // Inputs en bloques individuales
  if (t.dataset.blkMin !== undefined || t.dataset.blkSec !== undefined) {
    const idx = Number(t.dataset.blkMin !== undefined ? t.dataset.blkMin : t.dataset.blkSec);
    const minEl = document.querySelector(`input[data-blk-min="${idx}"]`);
    const secEl = document.querySelector(`input[data-blk-sec="${idx}"]`);
    if (minEl && secEl) {
      flowBlocks[idx].durationSeconds = minSecToSec(minEl.value, secEl.value);
      renderTimeline();
      updateMetrics();
      syncHiddenSchedule();
      validateForm();
    }
  }

  if (t.dataset.blkFlow !== undefined) {
    const idx = Number(t.dataset.blkFlow);
    flowBlocks[idx].targetFlow = t.value === '' ? 0 : Number(t.value);
    renderTimeline();
    updateMetrics();
    syncHiddenSchedule();
    validateForm();
  }
});

document.addEventListener('focusout', e => {
  const t = e.target;
  if (t.dataset.blkMin !== undefined || t.dataset.blkSec !== undefined || t.dataset.blkFlow !== undefined) {
    renderBlocks();
  }
});

$('addBlockBtn').addEventListener('click', () => addBlock(300, 5.0));

// Aplicar configuración cargada
function applyLoadedConfig(cfg) {
  if (!cfg) return;
  const w = secToMinSec(cfg.waitSeconds || 0);
  $('waitMin').value = w.min;
  $('waitSec').value = w.sec;
  $('targetFlow').value = Number(cfg.targetFlow !== undefined ? cfg.targetFlow : 5.0).toFixed(1);

  const sections = cfg.flowSections || [];
  if (sections.length > 0) {
    setBlocks(sections);
    setMode('periods');
  } else {
    setFixedDuration(Number(cfg.durationSeconds) === INF ? 'infinite' : (cfg.durationSeconds || 300));
    setBlocks([]);
    setMode('fixed');
  }
}

// ===== CARGAR ESTADO INICIAL DEL DRON =====
async function loadStatus() {
  try {
    const res = await fetch('/api/status');
    if (!res.ok) throw new Error('Error al consultar estado');
    const data = await res.json();

    const sdReady = !!data.sdReady;
    const sdStatus = String(data.sdStatus || 'ok').toUpperCase();
    const rtcStr = formatRtc(data.rtc);

    $('sdBadge').textContent = `SD: ${sdStatus}`;
    $('sdBadge').className = `status-badge ${sdReady ? 'ok' : 'bad'}`;

    $('rtcBadge').textContent = `RTC: ${rtcStr}`;

    const staBadge = $('staBadge');
    if (staBadge) {
      if (data.staConnected && data.staIp) {
        staBadge.textContent = `LAN: ${data.staIp}`;
        staBadge.className = 'status-badge ok';
        staBadge.classList.remove('hidden');
      } else {
        staBadge.classList.add('hidden');
      }
    }

    if (data.defaults) {
      applyLoadedConfig(data.defaults);
    }
  } catch (err) {
    console.warn('Fallo al obtener status:', err);
    $('sdBadge').textContent = 'SD: DESCONECTADO';
    $('sdBadge').className = 'status-badge bad';
    $('rtcBadge').textContent = 'RTC: --';
    if ($('staBadge')) $('staBadge').classList.add('hidden');
  }
}

// ===== CONFIRMAR E INICIAR SESIÓN DE CAPTURA =====
$('setupForm').addEventListener('submit', async e => {
  e.preventDefault();
  if (!validateForm()) return;

  const btn = $('confirmBtn');
  btn.disabled = true;
  btn.textContent = 'Guardando e iniciando...';

  syncHiddenSchedule();
  const isPeriods = activeMode() === 'periods';
  const waitVal = getWaitSeconds();
  const durVal = isPeriods ? flowBlocks.reduce((acc, b) => acc + Number(b.durationSeconds || 0), 0) : getFixedDurationSeconds();
  const targetVal = isPeriods && flowBlocks.length > 0 ? Number(flowBlocks[0].targetFlow || 5.0).toFixed(1) : $('targetFlow').value;

  const body = new URLSearchParams();
  body.set('waitSeconds', waitVal);
  body.set('durationSeconds', durVal);
  body.set('targetFlow', targetVal);
  body.set('flowSchedule', $('flowSchedule').value);
  if (!isPeriods && $('durationMode').value === 'infinite') {
    body.set('durationMode', 'infinite');
  }

  try {
    const res = await fetch('/api/confirm', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: body
    });

    if (res.ok) {
      notify('Configuración guardada. El Wi-Fi se desconectará e iniciará la captura.', 'success', 5000);
      $('setupStatus').textContent = 'Sesión confirmada';
    } else {
      const j = await res.json().catch(() => null);
      notify('Error al confirmar: ' + (j && j.error ? j.error : 'parámetros inválidos'), 'error');
      btn.disabled = false;
      btn.textContent = 'Iniciar sesión de captura';
    }
  } catch (err) {
    notify('Error de comunicación con el dron', 'error');
    btn.disabled = false;
    btn.textContent = 'Iniciar sesión de captura';
  }
});

// ===== PRESETS =====
async function loadPresets() {
  const grid = $('presetsGrid');
  grid.innerHTML = '<div class="empty-state">Cargando presets...</div>';
  try {
    const res = await fetch('/api/presets');
    if (!res.ok) throw new Error('Error al cargar presets');
    const data = await res.json();
    presets = data.presets || [];

    if (presets.length === 0) {
      grid.innerHTML = '<div class="empty-state">No hay presets guardados en el dispositivo.</div>';
      return;
    }

    grid.innerHTML = presets.map(p => {
      const durStr = Number(p.durationSeconds) === INF ? 'Sin límite' : formatDuration(p.durationSeconds);
      const isBlocks = p.flowSectionCount && p.flowSectionCount > 0;
      const typeStr = isBlocks ? `${p.flowSectionCount} bloques` : `${Number(p.targetFlow).toFixed(1)} L/min`;

      return `
        <div class="preset-card">
          <div>
            <div class="preset-title">${p.name}</div>
            <div class="preset-stats">
              <span>Modo: ${isBlocks ? 'Perfil por bloques' : 'Flujo constante'}</span>
              <span>Caudal: ${typeStr}</span>
              <span>Duración: ${durStr}</span>
            </div>
          </div>
          <div class="preset-actions">
            <button type="button" class="btn-primary btn-sm" data-load-preset="${p.name}">Cargar</button>
            <button type="button" class="btn-secondary btn-sm" data-del-preset="${p.name}">Eliminar</button>
          </div>
        </div>
      `;
    }).join('');

    grid.querySelectorAll('[data-load-preset]').forEach(b => {
      b.addEventListener('click', () => loadPreset(b.dataset.loadPreset));
    });

    grid.querySelectorAll('[data-del-preset]').forEach(b => {
      b.addEventListener('click', () => deletePreset(b.dataset.delPreset));
    });
  } catch (err) {
    grid.innerHTML = '<div class="empty-state">Error al consultar presets.</div>';
  }
}

async function loadPreset(name) {
  try {
    const res = await fetch('/api/presets/load?name=' + encodeURIComponent(name));
    const data = await res.json();
    if (res.ok && data.config) {
      applyLoadedConfig(data.config);
      $('presetName').value = data.name;
      notify(`Preset '${data.name}' cargado`, 'success');
      // Cambiar a la pestaña de configuración
      document.querySelector('.tab-btn[data-target="view-config"]').click();
    } else {
      notify('No se pudo cargar el preset', 'error');
    }
  } catch (err) {
    notify('Error al cargar preset', 'error');
  }
}

async function savePreset() {
  const name = $('presetName').value.trim();
  if (!name || !/^[A-Za-z0-9_-]{1,23}$/.test(name)) {
    notify('Nombre de preset inválido (solo letras, números, _ o -, máx 23 caracteres)', 'error');
    return;
  }
  if (!validateForm()) {
    notify('La configuración actual tiene parámetros inválidos', 'error');
    return;
  }

  syncHiddenSchedule();
  const isPeriods = activeMode() === 'periods';
  const waitVal = getWaitSeconds();
  const durVal = isPeriods ? flowBlocks.reduce((acc, b) => acc + Number(b.durationSeconds || 0), 0) : getFixedDurationSeconds();
  const targetVal = isPeriods && flowBlocks.length > 0 ? Number(flowBlocks[0].targetFlow || 5.0).toFixed(1) : $('targetFlow').value;

  const body = new URLSearchParams();
  body.set('name', name);
  body.set('waitSeconds', waitVal);
  body.set('durationSeconds', durVal);
  body.set('targetFlow', targetVal);
  body.set('flowSchedule', $('flowSchedule').value);
  if (!isPeriods && $('durationMode').value === 'infinite') {
    body.set('durationMode', 'infinite');
  }

  try {
    const res = await fetch('/api/presets/save', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: body
    });
    const j = await res.json();
    if (res.ok && j.ok) {
      notify(`Preset '${name}' guardado correctamente`, 'success');
      loadPresets();
    } else {
      notify('Error al guardar preset: ' + (j.error || ''), 'error');
    }
  } catch (err) {
    notify('Error de comunicación guardando preset', 'error');
  }
}

async function deletePreset(name) {
  if (!confirm(`¿Eliminar el preset '${name}'?`)) return;
  try {
    const body = new URLSearchParams();
    body.set('name', name);
    const res = await fetch('/api/presets/delete', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: body
    });
    const j = await res.json();
    if (res.ok && j.ok) {
      notify(`Preset '${name}' eliminado`, 'success');
      loadPresets();
    } else {
      notify('Error al eliminar preset', 'error');
    }
  } catch (err) {
    notify('Error al comunicar eliminación de preset', 'error');
  }
}

$('refreshPresets').addEventListener('click', loadPresets);
$('savePreset').addEventListener('click', savePreset);

// ===== REGISTROS CSV EN SD (EXPLORADOR & PREVIEW INTERPRETADA) =====
let currentPreviewFile = null;
let currentPreviewText = '';
let currentParsedData = null;
let previewViewMode = 'table';
let logSearchFilter = '';
let previewSearchFilter = '';

const CSV_COLUMN_META = {
  time: { label: 'Hora / Fecha', align: 'left', isMono: true },
  state: { label: 'Estado', align: 'center', isState: true },
  flow: { label: 'Caudal', unit: 'L/min', align: 'right', isNum: true, decimals: 2 },
  flow_target: { label: 'Obj.', unit: 'L/min', align: 'right', isNum: true, decimals: 1 },
  captured_volume: { label: 'Volumen', unit: 'L', align: 'right', isNum: true, decimals: 3 },
  captured_volume_l: { label: 'Volumen', unit: 'L', align: 'right', isNum: true, decimals: 3 },
  temperature: { label: 'Temp.', unit: '°C', align: 'right', isNum: true, decimals: 1 },
  humidity: { label: 'Humedad', unit: '%', align: 'right', isNum: true, decimals: 1 },
  pressure: { label: 'Presión', unit: 'hPa', align: 'right', isNum: true, decimals: 1 },
  pm1: { label: 'PM 1.0', unit: 'µg/m³', align: 'right', isNum: true, decimals: 0 },
  pm25: { label: 'PM 2.5', unit: 'µg/m³', align: 'right', isNum: true, decimals: 0 },
  pm10: { label: 'PM 10', unit: 'µg/m³', align: 'right', isNum: true, decimals: 0 },
  wind_speed: { label: 'Viento', unit: 'm/s', align: 'right', isNum: true, decimals: 1 },
  wind_direction: { label: 'Dir. Viento', unit: '°', align: 'right', isNum: true, decimals: 0 },
  ntc_temperature: { label: 'Temp. Motor', unit: '°C', align: 'right', isNum: true, decimals: 1 },
  battery_pct: { label: 'Batería', unit: '%', align: 'right', isNum: true, decimals: 0 },
  log_file: { label: 'Archivo de Registro', align: 'left', isMono: true, isLogLink: true },
  start_date: { label: 'Fecha Inicio', align: 'left', isMono: true },
  start_time: { label: 'Hora Inicio', align: 'left', isMono: true },
  end_date: { label: 'Fecha Fin', align: 'left', isMono: true },
  end_time: { label: 'Hora Fin', align: 'left', isMono: true },
  volume_source: { label: 'Origen Volumen', align: 'center', isVolumeSource: true }
};

function parseCsvLine(line) {
  const result = [];
  let cur = '';
  let inQuotes = false;
  for (let i = 0; i < line.length; i++) {
    const c = line[i];
    if (c === '"') {
      inQuotes = !inQuotes;
    } else if (c === ',' && !inQuotes) {
      result.push(cur.trim());
      cur = '';
    } else {
      cur += c;
    }
  }
  result.push(cur.trim());
  return result;
}

function escapeHtml(str) {
  if (str === null || str === undefined) return '';
  return String(str)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

function formatTableCell(colKey, rawVal) {
  if (rawVal === '' || rawVal === null || rawVal === undefined) {
    return '<span class="val-na">&mdash;</span>';
  }
  const cleanVal = String(rawVal).trim();
  if (cleanVal === '-1.0' || cleanVal === '-1' || cleanVal === '-1.000' || cleanVal === '-999' || cleanVal === '-999.0' || cleanVal === 'unavailable') {
    return '<span class="val-na">&mdash;</span>';
  }

  const meta = CSV_COLUMN_META[colKey] || {};

  if (meta.isState) {
    if (cleanVal.toLowerCase().includes('captur')) {
      return `<span class="badge-status badge-status-ok">${escapeHtml(cleanVal)}</span>`;
    }
    if (cleanVal.toLowerCase().includes('arranc')) {
      return `<span class="badge-status badge-status-warn">${escapeHtml(cleanVal)}</span>`;
    }
    return `<span class="badge-status badge-status-info">${escapeHtml(cleanVal)}</span>`;
  }

  if (meta.isVolumeSource) {
    if (cleanVal === 'recorded') {
      return '<span class="badge-status badge-status-ok">Medido</span>';
    }
    if (cleanVal === 'estimated_flow') {
      return '<span class="badge-status badge-status-warn">Estimado</span>';
    }
    return '<span class="badge-status val-na">N/D</span>';
  }

  if (meta.isLogLink) {
    return `<strong><a href="#view-logs" data-open-log="${escapeHtml(cleanVal)}" style="color:var(--primary); text-decoration:underline;">${escapeHtml(cleanVal)}</a></strong>`;
  }

  if (meta.isNum) {
    const num = parseFloat(cleanVal);
    if (!isNaN(num)) {
      return meta.decimals !== undefined ? num.toFixed(meta.decimals) : String(num);
    }
  }

  return escapeHtml(cleanVal);
}

function renderParsedDataTable() {
  const thead = $('logTableHead');
  const tbody = $('logTableBody');
  const table = $('logDataTable');
  const emptyState = $('tableEmptyState');
  if (!thead || !tbody || !table || !emptyState) return;

  if (!currentParsedData || !currentParsedData.columns || currentParsedData.columns.length === 0) {
    table.classList.add('hidden');
    emptyState.classList.remove('hidden');
    emptyState.textContent = 'Selecciona un archivo del explorador a la izquierda o haz clic en "Ver índice" para inspeccionar sus filas.';
    return;
  }

  emptyState.classList.add('hidden');
  table.classList.remove('hidden');

  const cols = currentParsedData.columns;
  const rows = currentParsedData.rows;

  // Render cabecera
  let headerHtml = '<tr><th class="col-num">#</th>';
  cols.forEach(colKey => {
    const meta = CSV_COLUMN_META[colKey] || {};
    const label = meta.label || colKey.replace(/_/g, ' ');
    const unitBadge = meta.unit ? ` <span style="font-size:10px; font-weight:normal; opacity:0.8;">(${meta.unit})</span>` : '';
    const alignClass = meta.align === 'right' ? 'text-right' : meta.align === 'center' ? 'cell-center' : '';
    headerHtml += `<th class="${alignClass}">${escapeHtml(label)}${unitBadge}</th>`;
  });
  headerHtml += '</tr>';
  thead.innerHTML = headerHtml;

  // Filtrado de filas
  const filter = (previewSearchFilter || '').toLowerCase();
  const filteredRows = filter
    ? rows.filter(r => r.some(cell => String(cell).toLowerCase().includes(filter)))
    : rows;

  const rowCountBadge = $('previewRowsCount');
  if (rowCountBadge) {
    if (filter) {
      rowCountBadge.textContent = `${filteredRows.length} de ${rows.length} filas`;
    } else {
      rowCountBadge.textContent = `${rows.length} fila${rows.length === 1 ? '' : 's'}`;
    }
  }

  if (filteredRows.length === 0) {
    tbody.innerHTML = `<tr><td colspan="${cols.length + 1}" class="empty-cell">Ninguna fila coincide con "${escapeHtml(previewSearchFilter)}"</td></tr>`;
    return;
  }

  // Render filas
  let bodyHtml = '';
  filteredRows.forEach((r, rowIdx) => {
    bodyHtml += `<tr><td class="col-num">${rowIdx + 1}</td>`;
    cols.forEach((colKey, colIdx) => {
      const cellVal = r[colIdx] !== undefined ? r[colIdx] : '';
      const meta = CSV_COLUMN_META[colKey] || {};
      const alignClass = meta.align === 'right' ? 'cell-num' : meta.align === 'center' ? 'cell-center' : '';
      bodyHtml += `<td class="${alignClass}">${formatTableCell(colKey, cellVal)}</td>`;
    });
    bodyHtml += '</tr>';
  });
  tbody.innerHTML = bodyHtml;

  // Delegar clicks en enlaces a otros logs (desde índice)
  tbody.querySelectorAll('[data-open-log]').forEach(link => {
    link.addEventListener('click', e => {
      e.preventDefault();
      const targetLog = link.dataset.openLog;
      if (targetLog) {
        previewLog(targetLog);
      }
    });
  });
}

function updatePreviewSummaryMetrics(columns, rows) {
  const timeIdx = columns.indexOf('time');
  const volIdx = columns.indexOf('captured_volume') !== -1 ? columns.indexOf('captured_volume') : columns.indexOf('captured_volume_l');
  const timeTag = $('previewTimeRange');
  const volTag = $('previewVolumeTag');

  if (timeTag) {
    if (timeIdx !== -1 && rows.length > 0) {
      const firstTime = rows[0][timeIdx] || '';
      const lastTime = rows[rows.length - 1][timeIdx] || '';
      const formatStamp = s => s.includes('T') ? s.split('T')[1] : s;
      timeTag.textContent = `${formatStamp(firstTime)} → ${formatStamp(lastTime)}`;
      timeTag.classList.remove('hidden');
    } else {
      timeTag.classList.add('hidden');
    }
  }

  if (volTag) {
    if (volIdx !== -1 && rows.length > 0) {
      const lastVol = parseFloat(rows[rows.length - 1][volIdx]);
      if (!isNaN(lastVol) && lastVol >= 0) {
        volTag.textContent = `${lastVol.toFixed(3)} L`;
        volTag.classList.remove('hidden');
      } else {
        volTag.classList.add('hidden');
      }
    } else {
      volTag.classList.add('hidden');
    }
  }
}

function setPreviewViewMode(mode) {
  previewViewMode = mode;
  const isTable = mode === 'table';
  $('btnViewTable').classList.toggle('active', isTable);
  $('btnViewRaw').classList.toggle('active', !isTable);
  $('previewTableWrap').classList.toggle('hidden', !isTable);
  $('previewRawWrap').classList.toggle('hidden', isTable);
}

$('btnViewTable').addEventListener('click', () => setPreviewViewMode('table'));
$('btnViewRaw').addEventListener('click', () => setPreviewViewMode('raw'));

function renderLogsTable() {
  const tbody = $('logsBody');
  if (!tbody) return;

  const countBadge = $('filesCountBadge');
  if (countBadge) {
    countBadge.textContent = `${logFiles.length} archivo${logFiles.length === 1 ? '' : 's'}`;
  }

  if (logFiles.length === 0) {
    tbody.innerHTML = '<tr><td colspan="3" class="empty-cell">No hay archivos CSV en la tarjeta SD</td></tr>';
    return;
  }

  const filter = (logSearchFilter || '').toLowerCase();
  const filtered = filter ? logFiles.filter(f => f.name.toLowerCase().includes(filter)) : logFiles;

  if (filtered.length === 0) {
    tbody.innerHTML = `<tr><td colspan="3" class="empty-cell">Ning&uacute;n archivo coincide con "${escapeHtml(logSearchFilter)}"</td></tr>`;
    return;
  }

  tbody.innerHTML = filtered.map(f => {
    const isActive = currentPreviewFile && currentPreviewFile.name === f.name;
    return `
      <tr class="${isActive ? 'active-row' : ''}" data-file-row="${f.name}">
        <td class="file-name-cell"><strong>${f.name}</strong></td>
        <td class="file-size-cell">${fmtBytes(f.size)}</td>
        <td class="text-right">
          <div class="table-actions">
            <button type="button" class="btn-secondary btn-sm" data-preview-log="${f.name}">Ver</button>
            <a href="/download?file=${encodeURIComponent(f.name)}" class="btn-primary btn-sm" download="${f.name}" style="text-decoration:none;">Descargar</a>
            <button type="button" class="btn-secondary btn-sm btn-icon-danger" data-del-log="${f.name}">Borrar</button>
          </div>
        </td>
      </tr>
    `;
  }).join('');

  // Delegación o asignación de eventos a botones y filas
  tbody.querySelectorAll('[data-preview-log]').forEach(b => {
    b.addEventListener('click', e => {
      e.stopPropagation();
      previewLog(b.dataset.previewLog);
    });
  });

  tbody.querySelectorAll('[data-del-log]').forEach(b => {
    b.addEventListener('click', e => {
      e.stopPropagation();
      deleteLog(b.dataset.delLog);
    });
  });

  tbody.querySelectorAll('tr[data-file-row]').forEach(tr => {
    tr.addEventListener('click', e => {
      if (e.target.closest('button') || e.target.closest('a')) return;
      previewLog(tr.dataset.fileRow);
    });
  });
}

async function loadLogs() {
  const tbody = $('logsBody');
  tbody.innerHTML = '<tr><td colspan="3" class="empty-cell">Consultando archivos en la tarjeta SD...</td></tr>';

  try {
    const res = await fetch('/api/logs');
    if (!res.ok) {
      tbody.innerHTML = '<tr><td colspan="3" class="empty-cell">Tarjeta SD no disponible</td></tr>';
      $('logStatSummary').textContent = 'SD no disponible';
      return;
    }
    const data = await res.json();
    logFiles = data.files || [];

    const totalBytes = logFiles.reduce((acc, f) => acc + (f.size || 0), 0);
    $('logStatSummary').textContent = `${logFiles.length} archivo${logFiles.length === 1 ? '' : 's'} (${fmtBytes(totalBytes)})`;

    renderLogsTable();
  } catch (err) {
    tbody.innerHTML = '<tr><td colspan="3" class="empty-cell">Error al listar registros CSV</td></tr>';
    $('logStatSummary').textContent = 'Error de lectura';
  }
}

async function previewLog(filename) {
  const isIndex = (filename === 'log_index.csv' || filename === 'index.csv');
  const fileObj = logFiles.find(f => f.name === filename);
  currentPreviewFile = { name: filename, size: fileObj ? fileObj.size : 0, isIndex };

  // Actualizar fila activa en la tabla de archivos
  document.querySelectorAll('#logsBody tr[data-file-row]').forEach(tr => {
    tr.classList.toggle('active-row', tr.dataset.fileRow === filename);
  });

  const displayName = isIndex ? 'Índice Maestro (log_index.csv)' : filename;
  $('previewFilename').textContent = displayName;
  $('previewFilename').title = displayName;

  const sizeTag = $('previewFileSize');
  if (sizeTag) {
    if (fileObj && fileObj.size !== undefined) {
      sizeTag.textContent = fmtBytes(fileObj.size);
      sizeTag.classList.remove('hidden');
    } else {
      sizeTag.classList.add('hidden');
    }
  }

  $('tableEmptyState').textContent = 'Cargando vista previa del archivo...';
  $('tableEmptyState').classList.remove('hidden');
  $('logDataTable').classList.add('hidden');
  $('preview').textContent = 'Cargando datos crudos...';
  $('copyPreviewBtn').disabled = true;
  $('copyTsvBtn').disabled = true;
  $('downloadPreviewBtn').classList.add('hidden');
  if ($('previewSearchInput')) {
    $('previewSearchInput').value = '';
    $('previewSearchInput').disabled = true;
  }
  previewSearchFilter = '';

  try {
    const res = await fetch('/api/logs/preview?file=' + encodeURIComponent(filename));
    if (!res.ok) {
      $('tableEmptyState').textContent = 'No se pudo leer la vista previa del archivo.';
      $('preview').textContent = 'No se pudo leer la vista previa.';
      $('previewRowsCount').textContent = '0 filas';
      return;
    }
    const data = await res.json();
    const headerStr = data.header || '';
    const rawRows = data.rows || [];
    const lines = [headerStr].concat(rawRows).filter(Boolean);
    currentPreviewText = lines.join('\n');
    $('preview').textContent = currentPreviewText;

    // Parsear datos CSV
    const columns = parseCsvLine(headerStr);
    const parsedRows = rawRows.map(r => parseCsvLine(r));
    currentParsedData = {
      header: headerStr,
      columns: columns,
      rows: parsedRows,
      rawLines: lines
    };

    updatePreviewSummaryMetrics(columns, parsedRows);
    renderParsedDataTable();

    $('copyPreviewBtn').disabled = false;
    $('copyTsvBtn').disabled = false;
    if ($('previewSearchInput')) $('previewSearchInput').disabled = false;

    const dlBtn = $('downloadPreviewBtn');
    dlBtn.href = '/download?file=' + encodeURIComponent(filename);
    dlBtn.download = isIndex ? 'log_index.csv' : filename;
    dlBtn.textContent = isIndex ? 'Descargar Índice' : 'Descargar';
    dlBtn.classList.remove('hidden');
  } catch (err) {
    $('tableEmptyState').textContent = 'Error al obtener la vista previa del registro.';
    $('preview').textContent = 'Error al obtener la vista previa del registro.';
    $('previewRowsCount').textContent = 'Error';
    $('copyPreviewBtn').disabled = true;
    $('copyTsvBtn').disabled = true;
    $('downloadPreviewBtn').classList.add('hidden');
  }
}

function resetPreview() {
  currentPreviewFile = null;
  currentPreviewText = '';
  currentParsedData = null;
  $('previewFilename').textContent = 'Ningún archivo seleccionado';
  $('previewFilename').title = '';
  if ($('previewFileSize')) $('previewFileSize').classList.add('hidden');
  if ($('previewTimeRange')) $('previewTimeRange').classList.add('hidden');
  if ($('previewVolumeTag')) $('previewVolumeTag').classList.add('hidden');
  $('previewRowsCount').textContent = '0 filas';
  $('tableEmptyState').textContent = 'Selecciona un archivo del explorador a la izquierda o haz clic en "Ver índice" para inspeccionar sus filas.';
  $('tableEmptyState').classList.remove('hidden');
  $('logDataTable').classList.add('hidden');
  $('preview').textContent = 'Selecciona un archivo del explorador a la izquierda para inspeccionar sus filas.';
  $('copyPreviewBtn').disabled = true;
  $('copyTsvBtn').disabled = true;
  if ($('previewSearchInput')) {
    $('previewSearchInput').value = '';
    $('previewSearchInput').disabled = true;
  }
  $('downloadPreviewBtn').classList.add('hidden');
}

async function deleteLog(filename) {
  if (filename === 'log_index.csv' || filename === 'index.csv') {
    notify('El índice maestro no puede ser eliminado', 'error');
    return;
  }
  if (!confirm(`¿Eliminar permanentemente el archivo '${filename}' de la SD?`)) return;
  try {
    const body = new URLSearchParams();
    body.set('file', filename);
    const res = await fetch('/api/logs/delete', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: body
    });
    const j = await res.json();
    if (res.ok && j.ok) {
      notify(`Archivo '${filename}' eliminado`, 'success');
      if (currentPreviewFile && currentPreviewFile.name === filename) {
        resetPreview();
      }
      loadLogs();
    } else {
      notify('No se pudo eliminar el archivo', 'error');
    }
  } catch (err) {
    notify('Error de comunicación eliminando archivo', 'error');
  }
}

// Búsqueda en explorador de archivos
if ($('logSearchInput')) {
  $('logSearchInput').addEventListener('input', e => {
    logSearchFilter = e.target.value.trim();
    renderLogsTable();
  });
}

// Búsqueda / filtrado en filas de vista previa
if ($('previewSearchInput')) {
  $('previewSearchInput').addEventListener('input', e => {
    previewSearchFilter = e.target.value.trim();
    renderParsedDataTable();
  });
}

// Copiar vista previa como CSV al portapapeles
if ($('copyPreviewBtn')) {
  $('copyPreviewBtn').addEventListener('click', () => {
    if (!currentPreviewText) return;
    copyToClipboard(currentPreviewText, 'Datos CSV copiados al portapapeles');
  });
}

// Copiar vista previa como TSV (para pegar en Excel / Sheets)
if ($('copyTsvBtn')) {
  $('copyTsvBtn').addEventListener('click', () => {
    if (!currentParsedData || !currentParsedData.columns) return;
    const headerTsv = currentParsedData.columns.join('\t');
    const rowsTsv = currentParsedData.rows.map(r => r.join('\t')).join('\n');
    const fullTsv = headerTsv + (rowsTsv ? '\n' + rowsTsv : '');
    copyToClipboard(fullTsv, 'Tabla copiada en formato TSV (lista para Excel)');
  });
}

function copyToClipboard(text, successMsg) {
  if (navigator.clipboard && navigator.clipboard.writeText) {
    navigator.clipboard.writeText(text).then(() => {
      notify(successMsg || 'Copiado al portapapeles', 'success');
    }).catch(() => {
      fallbackCopyText(text, successMsg);
    });
  } else {
    fallbackCopyText(text, successMsg);
  }
}

function fallbackCopyText(text, successMsg) {
  try {
    const ta = document.createElement('textarea');
    ta.value = text;
    ta.style.position = 'fixed';
    ta.style.opacity = '0';
    document.body.appendChild(ta);
    ta.select();
    document.execCommand('copy');
    ta.remove();
    notify(successMsg || 'Copiado al portapapeles', 'success');
  } catch (_) {
    notify('No se pudo copiar automáticamente al portapapeles', 'error');
  }
}

// Botón para previsualizar el índice maestro
if ($('previewIndexBtn')) {
  $('previewIndexBtn').addEventListener('click', () => {
    previewLog('log_index.csv');
  });
}

$('refreshLogs').addEventListener('click', loadLogs);
$('downloadAll').addEventListener('click', () => {
  if (!logFiles.length) {
    notify('No hay archivos para descargar', 'info');
    return;
  }
  logFiles.forEach((f, i) => {
    setTimeout(() => {
      const a = document.createElement('a');
      a.href = '/download?file=' + encodeURIComponent(f.name);
      a.download = f.name;
      document.body.appendChild(a);
      a.click();
      a.remove();
    }, i * 300);
  });
});

// ===== DIAGNÓSTICO & CONTROL DIRECTO DE MOTOR (DEBUG PWM) =====
async function loadSystemDiagnostics() {
  try {
    const res = await fetch('/api/diagnostics', { cache: 'no-store' });
    if (!res.ok) throw new Error('HTTP ' + res.status);
    const d = await res.json();
    $('diagLoop').textContent = `#${d.loop.heartbeat} · ${d.loop.lastDurationMs} ms · pausa máx ${d.loop.maxPauseMs} ms`;
    $('diagHeap').textContent = `${fmtBytes(d.heap.free)} / ${fmtBytes(d.heap.minimum)}`;
    $('diagWifi').textContent = `AP ${d.wifi.apActive ? 'OK' : 'OFF'} (${d.wifi.apClients}) · LAN ${d.wifi.staConnected ? d.wifi.staIp : 'sin conexión'}`;
    $('diagI2c').textContent = `${d.i2c.lastResult} · BME ${d.i2c.bmeFailure}`;
    const ix = d.sd.reconciliation;
    $('diagSd').textContent = `${d.sd.status} · ${ix.current} actuales, ${ix.recovered} recuperados, ${ix.errors} errores`;
    $('diagHttp').textContent = `${d.http.lastRequest || '—'} · ${d.http.lastDurationMs} ms · ${d.http.failed} fallidas`;
    $('diagUpdated').textContent = `Actualizado · uptime ${formatDuration(Math.floor(d.uptimeMs / 1000))}`;
  } catch (_) {
    $('diagUpdated').textContent = 'Diagnóstico no disponible';
  }
}

function startDiagnosticsRefresh() {
  stopDiagnosticsRefresh();
  loadSystemDiagnostics();
  _diagTimer = setInterval(loadSystemDiagnostics, 2000);
}

function stopDiagnosticsRefresh() {
  if (_diagTimer) {
    clearInterval(_diagTimer);
    _diagTimer = null;
  }
}

async function enterDebugMode() {
  const btn = $('enterDebugBtn');
  btn.disabled = true;
  btn.textContent = 'Activando...';

  try {
    const res = await fetch('/api/debug/enter', { method: 'POST' });
    const data = await res.json();
    if (res.ok && data.ok) {
      _debugActive = true;
      $('debugWarnCard').classList.add('hidden');
      $('debugActiveCard').classList.remove('hidden');
      notify('Modo diagnóstico activado', 'success');
      startDebugRefresh();
    } else {
      notify('No se pudo activar el modo diagnóstico', 'error');
      btn.disabled = false;
      btn.textContent = 'Activar diagnóstico';
    }
  } catch (err) {
    notify('Error activando diagnóstico', 'error');
    btn.disabled = false;
    btn.textContent = 'Activar diagnóstico';
  }
}

async function applyDebugPwm(pct) {
  try {
    const body = new URLSearchParams();
    body.set('pct', pct.toFixed(1));
    const res = await fetch('/api/debug/pwm', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: body
    });
    const data = await res.json();
    if (!res.ok || !data.ok) {
      notify('Error aplicando potencia: ' + (data.error || ''), 'error');
    }
  } catch (err) {
    notify('Error aplicando potencia', 'error');
  }
}

async function loadDebugTelemetry() {
  if (!_debugActive) return;
  try {
    const res = await fetch('/api/debug/status');
    if (!res.ok) return;
    const d = await res.json();

    _debugMaxPwm = d.maxPwm || 2047;
    $('debugMaxPwmLabel').textContent = _debugMaxPwm;
    $('debugPwmRawLabel').textContent = d.pwm !== undefined ? d.pwm : '--';
    $('debugReadPwm').textContent = d.pwm !== undefined ? `${d.pwm} / ${_debugMaxPwm}` : '--';
    $('debugReadPct').textContent = d.pct !== undefined ? `${d.pct.toFixed(1)}%` : '--';

    if (d.flow) {
      $('debugReadFlow').textContent = d.flow.valid ? `${d.flow.lpm.toFixed(2)} L/min` : 'Sin lectura';
    }

    if (d.motorTempValid) {
      $('debugReadTemp').textContent = `${Number(d.motorTemp).toFixed(1)} °C`;
    } else {
      $('debugReadTemp').textContent = 'N/A';
    }
  } catch (_) {}
}

function startDebugRefresh() {
  stopDebugRefresh();
  loadDebugTelemetry();
  _debugTimer = setInterval(loadDebugTelemetry, 1000);
}

function stopDebugRefresh() {
  if (_debugTimer) {
    clearInterval(_debugTimer);
    _debugTimer = null;
  }
}

$('enterDebugBtn').addEventListener('click', enterDebugMode);

$('debugPwmSlider').addEventListener('input', e => {
  $('debugPwmPctLabel').textContent = e.target.value;
});

$('applyDebugPwmBtn').addEventListener('click', () => {
  const pct = Number($('debugPwmSlider').value);
  applyDebugPwm(pct).then(loadDebugTelemetry);
});

$('stopDebugMotorBtn').addEventListener('click', () => {
  $('debugPwmSlider').value = 0;
  $('debugPwmPctLabel').textContent = '0';
  applyDebugPwm(0).then(loadDebugTelemetry);
});

$('igniteMotorBtn').addEventListener('click', async () => {
  const btn = $('igniteMotorBtn');
  btn.disabled = true;
  btn.textContent = 'Enviando pulso...';
  try {
    const res = await fetch('/api/motor/ignite', { method: 'POST' });
    const d = await res.json();
    if (res.ok && d.ok) {
      const dur = d.durationMs || 300;
      notify(`Pulso de arranque ejecutado (${dur} ms)`, 'success');
      $('debugPwmSlider').value = 0;
      $('debugPwmPctLabel').textContent = '0';
      setTimeout(loadDebugTelemetry, 350);
    } else {
      notify('Error en pulso: ' + (d.error || 'error'), 'error');
    }
  } catch (_) {
    notify('Error enviando pulso de arranque', 'error');
  } finally {
    btn.disabled = false;
    btn.textContent = 'Pulso de arranque';
  }
});

// ===== INICIALIZACIÓN DE LA APLICACIÓN =====
initTheme();
setMode('fixed');
loadStatus().then(loadPresets).then(loadLogs);
