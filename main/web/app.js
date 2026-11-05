'use strict';
const $ = id => document.getElementById(id);
const controls = ['text', 'font-size', 'fall-ms', 'scroll-speed', 'brightness', 'show-grid'];
let serverState = null;
let mode = 'current';
let busy = false;
let dirty = false;
let layoutValid = true;
let previewAnimation = 0;

function feedback(message, error = false) {
  $('feedback').textContent = message;
  $('feedback').dataset.error = String(error);
}

function setBusy(value) {
  busy = value;
  $('fields').disabled = value || !serverState;
  for (const id of ['reload', 'reset', 'factory', 'current-tab', 'defaults-tab']) {
    $(id).disabled = value || (id !== 'reload' && !serverState);
  }
  $('save').disabled = value || !layoutValid;
}

function readForm() {
  return {
    text: $('text').value.replace(/\r\n?/g, '\n'),
    font_size: Number($('font-size').value),
    fall_ms: Number($('fall-ms').value),
    scroll_speed: Number($('scroll-speed').value),
    brightness: Number($('brightness').value),
    show_grid: $('show-grid').checked
  };
}

function fillForm() {
  if (!serverState) return;
  const config = serverState[mode];
  $('text').value = config.text;
  $('font-size').value = config.font_size;
  $('fall-ms').value = config.fall_ms;
  $('scroll-speed').value = config.scroll_speed ?? 40;
  $('brightness').value = config.brightness;
  $('show-grid').checked = config.show_grid;
  $('current-tab').setAttribute('aria-selected', String(mode === 'current'));
  $('defaults-tab').setAttribute('aria-selected', String(mode === 'defaults'));
  $('text-label').textContent = mode === 'current' ? '显示内容' : '默认欢迎语';
  $('save').textContent = mode === 'current' ? '保存并显示' : '保存默认配置';
  $('mode-description').textContent = mode === 'current'
    ? '保存后立即显示，断电重启后恢复。'
    : '设置欢迎语和默认样式。恢复默认时应用；本次保存不改变当前屏幕。';
  dirty = false;
  drawPreview();
}

// Mirrors the firmware's public layout rule; the firmware always validates again.
function layout(config) {
  const cellWidth = config.font_size + 16;
  const cellHeight = Math.floor((config.font_size * 3 + 1) / 2) + 16;
  const columns = Math.floor(992 / cellWidth);
  const rows = Math.floor(568 / cellHeight);
  const cells = [];
  let row = 0, col = 0;
  for (const char of config.text) {
    const cp = char.codePointAt(0);
    if (char === '\n') { row++; col = 0; continue; }
    if (cp === 0xfe0e || cp === 0xfe0f) continue;
    if (col === columns) { row++; col = 0; }
    cells.push({char, row, col});
    col++;
  }
  const contentRows = row + 1;
  return {cellWidth, cellHeight, columns, rows, contentRows, cells, overflow: contentRows > rows};
}

function drawPreview() {
  cancelAnimationFrame(previewAnimation);
  const config = readForm();
  $('font-value').textContent = `${config.font_size} px`;
  $('fall-value').textContent = config.fall_ms === 0 ? '直接显示' : `${config.fall_ms} ms`;
  $('brightness-value').textContent = `${config.brightness}%`;
  $('scroll-value').textContent = `${config.scroll_speed} px/s`;
  const grid = layout(config);
  const bytes = new TextEncoder().encode(config.text).length;
  layoutValid = bytes <= 2048;
  $('save').disabled = busy || !layoutValid;
  $('capacity').dataset.level = !layoutValid ? 'error' : grid.overflow ? 'warning' : 'normal';
  $('capacity').textContent = `${grid.columns} 列 × ${grid.rows} 行 · ${grid.cells.length} 个字符 · ${bytes}/2048 字节` +
    (!layoutValid ? ' · 超出 2048 字节限制，请减少文字后保存' : grid.overflow
      ? ` · 提醒：内容需要 ${grid.contentRows} 行，超过一屏。可以直接保存，完整内容将以 ${config.scroll_speed} px/s 上下滚动，不会截断。` : '');
  paintPreview(config, grid, 0);
  if (!grid.overflow || !layoutValid) return;
  const distance = (grid.contentRows - grid.rows) * grid.cellHeight;
  const duration = distance * 1000 / config.scroll_speed;
  const pause = 1500;
  const started = performance.now();
  function animate(now) {
    const phase = (now - started) % (2 * duration + 2 * pause);
    let offset = 0;
    if (phase > pause && phase < pause + duration) offset = (phase - pause) * config.scroll_speed / 1000;
    else if (phase >= pause + duration && phase <= 2 * pause + duration) offset = distance;
    else if (phase > 2 * pause + duration) offset = distance - (phase - 2 * pause - duration) * config.scroll_speed / 1000;
    paintPreview(config, grid, Math.floor(offset));
    previewAnimation = requestAnimationFrame(animate);
  }
  previewAnimation = requestAnimationFrame(animate);
}

function paintPreview(config, grid, offset) {
  const ctx = $('preview').getContext('2d');
  ctx.fillStyle = '#101317'; ctx.fillRect(0, 0, 1024, 600);
  const x = (1024 - grid.columns * grid.cellWidth) / 2;
  const y = (600 - grid.rows * grid.cellHeight) / 2;
  ctx.save();
  ctx.beginPath();
  ctx.rect(x, y, grid.columns * grid.cellWidth, grid.rows * grid.cellHeight);
  ctx.clip();
  if (config.show_grid) {
    const firstRow = Math.floor(offset / grid.cellHeight);
    const lastRow = Math.min(Math.max(grid.rows, grid.contentRows), firstRow + grid.rows + 1);
    for (let r = firstRow; r < lastRow; r++) for (let c = 0; c < grid.columns; c++) {
      const left = x + c * grid.cellWidth + 2, top = y + r * grid.cellHeight - offset + 2;
      ctx.fillStyle = '#252a30'; ctx.strokeStyle = '#404750';
      ctx.fillRect(left, top, grid.cellWidth - 4, grid.cellHeight - 4);
      ctx.strokeRect(left + .5, top + .5, grid.cellWidth - 5, grid.cellHeight - 5);
      ctx.fillStyle = '#101317'; ctx.fillRect(left, top + (grid.cellHeight - 4) / 2, grid.cellWidth - 4, 1);
    }
  }
  ctx.fillStyle = '#f3f4f6'; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
  ctx.font = `${config.font_size}px system-ui, "Noto Sans SC", "Microsoft YaHei", sans-serif`;
  for (const cell of grid.cells) {
    const top = cell.row * grid.cellHeight - offset;
    if (top + grid.cellHeight <= 0 || top >= grid.rows * grid.cellHeight) continue;
    ctx.fillText(cell.char, x + (cell.col + .5) * grid.cellWidth,
      y + (cell.row + .5) * grid.cellHeight - offset, grid.cellWidth - 8);
  }
  ctx.restore();
}

async function request(path, body) {
  const options = {cache: 'no-store'};
  if (body !== undefined) {
    options.method = 'POST';
    options.headers = {'Content-Type': 'application/json', 'If-Match': `"${serverState.revision}"`};
    options.body = JSON.stringify(body);
  }
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 20000);
  options.signal = controller.signal;
  try {
    const response = await fetch(path, options);
    const result = await response.json();
    if (!response.ok) throw new Error(result.error || `设备返回 ${response.status}`);
    return result;
  } finally { clearTimeout(timeout); }
}

function acceptState(state) {
  serverState = state;
  $('connection').textContent = state.device.persistence_ready && state.device.display_ready
    ? '● 设备已连接' : '● 设备需要检查';
  $('network').textContent = `${state.device.ssid} · ${state.device.ip}`;
  fillForm();
}

function deviceWarning() {
  if (!serverState.device.persistence_ready) return '设备存储未就绪，无法保存配置。请检查串口日志。';
  if (!serverState.device.display_ready) return '屏幕未就绪，请检查屏幕和串口日志。';
  if (serverState.device.recovered_defaults) return '上次配置无法读取，暂用出厂配置。保存后可重新建立有效配置。';
  if (serverState.device.missing_glyphs) return '部分字符不在内置字库中，屏幕将显示问号。组合表情不受支持。';
  return '';
}

async function refresh() {
  if (dirty && !window.confirm('重新读取会放弃尚未保存的编辑，是否继续？')) return;
  setBusy(true);
  try {
    acceptState(await request('/api/config'));
    const warning = deviceWarning();
    feedback(warning || '已读取设备保存的内容与配置。', Boolean(warning));
  } catch (error) {
    $('connection').textContent = '设备连接失败';
    feedback(`读取失败：${error.message}。请连接设备热点，使用 http://192.168.4.1 访问。`, true);
  } finally { setBusy(false); }
}

async function saveTo(path, body, message) {
  setBusy(true);
  feedback('正在写入设备，请稍候…');
  try {
    acceptState(await request(path, body));
    const warning = deviceWarning();
    feedback(warning ? `${message} ${warning}` : message, Boolean(warning));
  } catch (error) {
    feedback(`未确认保存成功：${error.message}。网络中断时设备可能已保存，请重新读取确认。`, true);
  } finally { setBusy(false); }
}

$('config-form').addEventListener('submit', event => {
  event.preventDefault();
  if (!busy && serverState && layoutValid) saveTo(mode === 'current' ? '/api/config' : '/api/defaults',
    readForm(), mode === 'current' ? '已保存并显示，重启后会恢复。' : '默认配置已保存，当前屏幕保持原显示。');
});
for (const id of controls) $(id).addEventListener('input', () => { dirty = true; drawPreview(); });
for (const target of ['current', 'defaults']) $(`${target}-tab`).addEventListener('click', () => {
  if (target === mode || busy) return;
  if (dirty && !window.confirm('切换会放弃尚未保存的编辑，是否继续？')) return;
  mode = target; fillForm(); feedback('');
});
$('reload').addEventListener('click', refresh);
$('reset').addEventListener('click', () => {
  if (window.confirm('将当前屏幕恢复为已保存的默认欢迎语和样式，是否继续？')) {
    mode = 'current'; saveTo('/api/reset', {}, '已恢复默认配置并保存。');
  }
});
$('factory').addEventListener('click', () => {
  if (window.confirm('将删除当前内容和自定义默认配置，恢复内置中文欢迎语及全部出厂显示参数，是否继续？')) {
    mode = 'current'; saveTo('/api/factory-reset', {}, '已恢复出厂配置并保存。');
  }
});
window.addEventListener('beforeunload', event => { if (dirty) { event.preventDefault(); event.returnValue = ''; } });
drawPreview();
refresh();
