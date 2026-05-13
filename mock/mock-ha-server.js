// ============================================================
// Mock Home Assistant WebSocket Server
// 用于前端联调验证：状态同步、控制链路、异常恢复
// ============================================================

const WebSocket = require('ws');
const http = require('http');
const fs = require('fs');
const path = require('path');

const WS_PORT = 8123;
const HTTP_PORT = 8080;
const PROJECT_ROOT = path.join(__dirname, '..');

// === 模拟设备状态 ===
let deviceStates = {
  'light.matter_panel_ch1': { entity_id: 'light.matter_panel_ch1', state: 'off', attributes: { friendly_name: '客厅主灯' } },
  'light.matter_panel_ch2': { entity_id: 'light.matter_panel_ch2', state: 'off', attributes: { friendly_name: '客厅灯带' } },
  'light.matter_panel_ch3': { entity_id: 'light.matter_panel_ch3', state: 'off', attributes: { friendly_name: '客厅壁灯' } },
  'light.matter_panel_ch4': { entity_id: 'light.matter_panel_ch4', state: 'off', attributes: { friendly_name: '客厅吊灯' } },
};

let msgId = 10;
let clients = [];
let offlineMode = false;
let delayMs = 100; // 模拟正常网络延迟

function broadcastStateChange(entityId) {
  const state = deviceStates[entityId];
  if (!state) return;
  const event = {
    id: msgId++,
    type: 'event',
    event: {
      event_type: 'state_changed',
      data: {
        entity_id: entityId,
        new_state: state,
        old_state: { ...state, state: state.state === 'on' ? 'off' : 'on' }
      }
    }
  };
  clients.forEach(ws => {
    if (ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify(event));
    }
  });
}

function handleMessage(ws, data) {
  const msg = JSON.parse(data);

  switch (msg.type) {
    case 'auth':
      setTimeout(() => {
        ws.send(JSON.stringify({ type: 'auth_ok' }));
      }, delayMs);
      break;

    case 'subscribe_events':
      ws.send(JSON.stringify({ id: msg.id, type: 'result', success: true, result: null }));
      break;

    case 'get_states':
      setTimeout(() => {
        ws.send(JSON.stringify({
          id: msg.id,
          type: 'result',
          success: true,
          result: Object.values(deviceStates)
        }));
      }, delayMs);
      break;

    case 'call_service':
      if (offlineMode) {
        setTimeout(() => {
          ws.send(JSON.stringify({
            id: msg.id,
            type: 'result',
            success: false,
            error: { code: 'HA_UNAVAILABLE', message: 'Device offline' }
          }));
        }, delayMs);
        return;
      }

      const { domain, service, target, service_data } = msg;
      const entities = target?.entity_id || service_data?.entity_id;
      const entityList = Array.isArray(entities) ? entities : [entities];

      setTimeout(() => {
        entityList.forEach(eid => {
          if (deviceStates[eid]) {
            deviceStates[eid].state = service === 'turn_on' ? 'on' : 'off';
            broadcastStateChange(eid);
          }
        });
        ws.send(JSON.stringify({ id: msg.id, type: 'result', success: true, result: null }));
      }, delayMs);
      break;

    default:
      ws.send(JSON.stringify({ id: msg.id, type: 'result', success: true, result: null }));
  }
}

// === WebSocket Server ===
const wss = new WebSocket.Server({ port: WS_PORT });
wss.on('connection', (ws) => {
  clients.push(ws);
  ws.send(JSON.stringify({ type: 'auth_required' }));

  ws.on('message', (data) => {
    try {
      handleMessage(ws, data);
    } catch (e) {
      console.error('WS error:', e.message);
    }
  });

  ws.on('close', () => {
    clients = clients.filter(c => c !== ws);
  });
});

// === HTTP Server (serve frontend) ===
const mimeTypes = {
  '.html': 'text/html',
  '.js': 'application/javascript',
  '.css': 'text/css',
};

const server = http.createServer((req, res) => {
  const filePath = req.url === '/' ? '/frontend/index.html' : req.url;
  const fullPath = path.join(PROJECT_ROOT, filePath);
  const ext = path.extname(fullPath);

  fs.readFile(fullPath, (err, data) => {
    if (err) {
      res.writeHead(404);
      res.end('Not found');
      return;
    }
    res.writeHead(200, { 'Content-Type': mimeTypes[ext] || 'application/octet-stream' });
    res.end(data);
  });
});

server.listen(HTTP_PORT, () => {
  console.log(`Mock HA WS: ws://localhost:${WS_PORT}/api/websocket`);
  console.log(`Frontend HTTP: http://localhost:${HTTP_PORT}/`);
});

// === CLI Controls ===
console.log('\n--- Mock HA Controls ---');
console.log('Commands:');
console.log('  toggle <ch1|ch2|ch3|ch4>  - Simulate HA toggling a channel');
console.log('  offline                    - Enter offline mode');
console.log('  online                     - Exit offline mode');
console.log('  delay <ms>                 - Set response delay');
console.log('  status                     - Show device states');
console.log('  quit                       - Exit');
console.log('------------------------\n');

const readline = require('readline');
const rl = readline.createInterface({ input: process.stdin, output: process.stdout });

rl.on('line', (line) => {
  const [cmd, arg] = line.trim().split(' ');
  switch (cmd) {
    case 'toggle': {
      const map = { ch1: 'light.matter_panel_ch1', ch2: 'light.matter_panel_ch2', ch3: 'light.matter_panel_ch3', ch4: 'light.matter_panel_ch4' };
      const eid = map[arg];
      if (eid && deviceStates[eid]) {
        deviceStates[eid].state = deviceStates[eid].state === 'on' ? 'off' : 'on';
        broadcastStateChange(eid);
        console.log(`Toggled ${arg} -> ${deviceStates[eid].state}`);
      }
      break;
    }
    case 'offline':
      offlineMode = true;
      console.log('Offline mode ON');
      break;
    case 'online':
      offlineMode = false;
      console.log('Offline mode OFF');
      break;
    case 'delay':
      delayMs = parseInt(arg) || 100;
      console.log(`Delay set to ${delayMs}ms`);
      break;
    case 'status':
      console.log(deviceStates);
      break;
    case 'quit':
      process.exit(0);
  }
});
