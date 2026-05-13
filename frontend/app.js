// ============================================================
// Matter Panel — 前端控制逻辑
// 冻结规格：4 路主版 / 2 路裁剪，实体按键 + 小屏
// ============================================================

// 自动检测 mock 环境
const IS_MOCK = location.hostname === 'localhost' || location.hostname === '127.0.0.1';
const HA_URL = localStorage.getItem('ha_url') || (IS_MOCK ? 'ws://localhost:8123/api/websocket' : 'ws://homeassistant.local:8123/api/websocket');
const HA_TOKEN = localStorage.getItem('ha_token') || '';

// === 常量定义 ===
const CHANNEL_COUNT = 4; // 2 或 4
const ROOM_NAME = '客厅';
const CHANNEL_NAMES = ['主灯', '灯带', '壁灯', '吊灯'].slice(0, CHANNEL_COUNT);
const ENTITY_PREFIX = 'light.matter_panel_ch';

// panelStatus 枚举
const PANEL_STATUS = {
    UNPAIRED:  'unpaired',
    PAIRING:   'pairing',
    ONLINE:    'online',
    OFFLINE:   'offline',
    SYNCING:   'syncing',
    ERROR:     'error',
};

// powerState 枚举
const POWER_STATE = {
    OFF:         'off',
    ON:          'on',
    PENDING:     'pending',
    NO_RESPONSE: 'no_response',
};

// pairingStep 枚举
const PAIRING_STEP = {
    IDLE:           'idle',
    DISCOVERING:    'discovering',
    CODE_REQUIRED:  'code_required',
    PAIRING:        'pairing',
    PAIRED:         'paired',
    SYNC_DONE:      'sync_done',
    FAILED:         'failed',
};

// errorCode 枚举
const ERROR_CODE = {
    NETWORK_ERROR:        'NETWORK_ERROR',
    HA_UNAVAILABLE:       'HA_UNAVAILABLE',
    DEVICE_OFFLINE:       'DEVICE_OFFLINE',
    PAIRING_CODE_INVALID: 'PAIRING_CODE_INVALID',
    PAIRING_TIMEOUT:      'PAIRING_TIMEOUT',
    COMMAND_TIMEOUT:      'COMMAND_TIMEOUT',
    STATE_SYNC_FAILED:    'STATE_SYNC_FAILED',
};

// 文案映射
const STATUS_LABEL = {
    [PANEL_STATUS.UNPAIRED]: '未配网',
    [PANEL_STATUS.PAIRING]:  '配对中',
    [PANEL_STATUS.ONLINE]:   '在线',
    [PANEL_STATUS.OFFLINE]:  '离线',
    [PANEL_STATUS.SYNCING]:  '同步中',
    [PANEL_STATUS.ERROR]:    '异常',
};

const POWER_LABEL = {
    [POWER_STATE.OFF]:         '关闭',
    [POWER_STATE.ON]:          '开启',
    [POWER_STATE.PENDING]:     '执行中',
    [POWER_STATE.NO_RESPONSE]: '无响应',
};

// === 运行时状态 ===
let ws = null;
let msgId = 1;
const pending = new Map();
let reconnectTimer = null;
let panelState = PANEL_STATUS.UNPAIRED;

// ============================================================
// 初始化
// ============================================================
function init() {
    document.getElementById('room-name').textContent = ROOM_NAME;
    renderChannels();
    bindEvents();
    connect();
}

function renderChannels() {
    const grid = document.getElementById('channel-grid');
    grid.innerHTML = '';
    grid.classList.toggle('channels-2', CHANNEL_COUNT === 2);

    for (let i = 0; i < CHANNEL_COUNT; i++) {
        const card = document.createElement('div');
        card.className = 'channel-card';
        card.dataset.index = i;
        card.dataset.entity = `${ENTITY_PREFIX}${i + 1}`;
        card.innerHTML = `
            <span class="channel-led"></span>
            <span class="channel-num">路${i + 1}</span>
            <span class="channel-name">${CHANNEL_NAMES[i]}</span>
            <span class="channel-state">${POWER_LABEL[POWER_STATE.OFF]}</span>
        `;
        grid.appendChild(card);
    }
}

function bindEvents() {
    document.querySelectorAll('.channel-card').forEach(card => {
        card.addEventListener('click', () => {
            const idx = parseInt(card.dataset.index);
            const entityId = card.dataset.entity;
            toggleChannel(idx, entityId);
        });
    });

    document.querySelectorAll('.foot-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            const action = btn.dataset.action;
            if (action === 'all-on') allOn();
            if (action === 'all-off') allOff();
            if (action === 'scene') showScenePicker();
        });
    });

    document.getElementById('overlay-action').addEventListener('click', () => {
        const btn = document.getElementById('overlay-action');
        const text = btn.textContent;
        if (text === '去配网') startPairing();
        if (text === '重试' || text === '重新连接') reconnect();
    });
}

// ============================================================
// WebSocket 连接 HA
// ============================================================
function connect() {
    if (reconnectTimer) clearTimeout(reconnectTimer);
    try {
        ws = new WebSocket(HA_URL);
    } catch (e) {
        setPanelState(PANEL_STATUS.ERROR, ERROR_CODE.HA_UNAVAILABLE);
        showOverlay('连接异常', 'Home Assistant 不可用', '重新连接');
        return;
    }

    ws.onopen = () => {
        ws.send(JSON.stringify({ type: 'auth', access_token: HA_TOKEN }));
    };

    ws.onmessage = (e) => {
        const data = JSON.parse(e.data);
        handleMessage(data);
    };

    ws.onclose = () => {
        setPanelState(PANEL_STATUS.OFFLINE);
        reconnectTimer = setTimeout(connect, 3000);
    };

    ws.onerror = () => {
        ws.close();
    };
}

function send(msg) {
    if (!ws || ws.readyState !== WebSocket.OPEN) return;
    ws.send(JSON.stringify(msg));
}

function handleMessage(data) {
    switch (data.type) {
        case 'auth_required':
            break;
        case 'auth_ok':
            setPanelState(PANEL_STATUS.ONLINE);
            hideOverlay();
            subscribeStates();
            fetchStates();
            break;
        case 'auth_invalid':
            setPanelState(PANEL_STATUS.ERROR, ERROR_CODE.HA_UNAVAILABLE);
            showOverlay('连接异常', '认证失败', '重新连接');
            break;
        case 'result':
            if (pending.has(data.id)) {
                const { resolve, reject } = pending.get(data.id);
                pending.delete(data.id);
                data.success ? resolve(data.result) : reject(data.error);
            }
            break;
        case 'event':
            if (data.event?.event_type === 'state_changed') {
                updateChannelState(data.event.data.new_state);
            }
            break;
    }
}

function subscribeStates() {
    send({ id: msgId++, type: 'subscribe_events', event_type: 'state_changed' });
}

function fetchStates() {
    const id = msgId++;
    send({ id, type: 'get_states' });
    pending.set(id, {
        resolve: (states) => states.forEach(s => updateChannelState(s)),
        reject: console.error
    });
}

// ============================================================
// 控制逻辑
// ============================================================
async function toggleChannel(idx, entityId) {
    const card = document.querySelector(`.channel-card[data-index="${idx}"]`);
    if (!card) return;

    // 防抖：pending 状态不可再次点击
    if (card.classList.contains(POWER_STATE.PENDING)) return;

    setChannelVisual(idx, POWER_STATE.PENDING);
    showStatus(`路${idx + 1} ${POWER_LABEL[POWER_STATE.PENDING]}`);

    const isOn = card.classList.contains(POWER_STATE.ON);
    const domain = entityId.split('.')[0];
    const action = isOn ? 'turn_off' : 'turn_on';

    const timeout = setTimeout(() => {
        setChannelVisual(idx, POWER_STATE.NO_RESPONSE);
        showStatus(`路${idx + 1} ${POWER_LABEL[POWER_STATE.NO_RESPONSE]}`);
    }, 3000);

    try {
        await callService(domain, action, { entity_id: entityId });
        clearTimeout(timeout);
        showStatus(`路${idx + 1} ${isOn ? '关' : '开'}`);
    } catch (e) {
        clearTimeout(timeout);
        setChannelVisual(idx, POWER_STATE.NO_RESPONSE);
        showStatus('操作失败');
    }
}

async function allOn() {
    showStatus('全部开启中…');
    const promises = [];
    for (let i = 0; i < CHANNEL_COUNT; i++) {
        const entityId = `${ENTITY_PREFIX}${i + 1}`;
        promises.push(callService('light', 'turn_on', { entity_id: entityId }));
    }
    try {
        await Promise.all(promises);
        showStatus('已全部开启');
    } catch {
        showStatus('操作失败');
    }
}

async function allOff() {
    showStatus('全部关闭中…');
    const promises = [];
    for (let i = 0; i < CHANNEL_COUNT; i++) {
        const entityId = `${ENTITY_PREFIX}${i + 1}`;
        promises.push(callService('light', 'turn_off', { entity_id: entityId }));
    }
    try {
        await Promise.all(promises);
        showStatus('已全部关闭');
    } catch {
        showStatus('操作失败');
    }
}

function showScenePicker() {
    showStatus('场景选择');
}

function callService(domain, service, target, serviceData = {}) {
    const id = msgId++;
    return new Promise((resolve, reject) => {
        pending.set(id, { resolve, reject });
        send({
            id,
            type: 'call_service',
            domain,
            service,
            target,
            service_data: serviceData
        });
    });
}

// ============================================================
// UI 更新
// ============================================================
function updateChannelState(state) {
    if (!state) return;
    const card = document.querySelector(`[data-entity="${state.entity_id}"]`);
    if (!card) return;

    const idx = parseInt(card.dataset.index);
    const isOn = state.state === 'on';
    setChannelVisual(idx, isOn ? POWER_STATE.ON : POWER_STATE.OFF);
}

function setChannelVisual(idx, powerState) {
    const card = document.querySelector(`.channel-card[data-index="${idx}"]`);
    if (!card) return;

    card.classList.remove(POWER_STATE.ON, POWER_STATE.OFF, POWER_STATE.PENDING, POWER_STATE.NO_RESPONSE);
    card.classList.add(powerState);
    card.querySelector('.channel-state').textContent = POWER_LABEL[powerState] || powerState;
}

function setPanelState(state, errorCode = null) {
    panelState = state;
    const el = document.getElementById('conn-status');
    el.className = 'status ' + state;
    el.textContent = STATUS_LABEL[state] || state;

    if (state === PANEL_STATUS.ERROR && errorCode) {
        console.log('[ERROR]', errorCode);
    }
}

function showStatus(text) {
    const bar = document.getElementById('status-bar');
    bar.textContent = text;
    bar.classList.add('show');
    clearTimeout(bar._timer);
    bar._timer = setTimeout(() => bar.classList.remove('show'), 1500);
}

function showOverlay(title, desc, btnText) {
    document.getElementById('overlay-title').textContent = title;
    document.getElementById('overlay-desc').textContent = desc;
    document.getElementById('overlay-action').textContent = btnText;
    document.getElementById('overlay').classList.remove('hidden');
}

function hideOverlay() {
    document.getElementById('overlay').classList.add('hidden');
}

function startPairing() {
    setPanelState(PANEL_STATUS.PAIRING);
    showStatus('配对中…');
}

function reconnect() {
    hideOverlay();
    connect();
}

// ============================================================
// 启动
// ============================================================
init();
