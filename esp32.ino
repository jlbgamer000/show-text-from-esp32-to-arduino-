#include <WiFi.h>
#include <WebServer.h>

#define TX2_PIN 17
#define RX2_PIN 16
#define CTS_PIN 18

#define BTN_UP 32
#define BTN_DOWN 35

const char* ssid = "vivo X200";
const char* password = "vivox2001234";

WebServer server(80);

String rawText = "System initialized.\nLine limit expanded to 1000.\nReady for large payloads.";
uint8_t currentSize = 1;
uint16_t currentFg = 0xFFFF;
uint16_t currentBg = 0x0000;
bool showDebug = true;
int currentRotation = 0; 
int currentAlign = 0;    
int currentFont = 0;     
int forceClear = 1;      

// INCREASED CAPACITY TO 1000 LINES
#define MAX_LINES 1000
String wrappedLines[MAX_LINES];
int totalLines = 0;
int scrollOffset = 0;
unsigned long lastBtnCheck = 0;

void wrapText() {
  totalLines = 0;
  int screenWidth = (currentRotation == 1 || currentRotation == 3) ? 320 : 240;
  int screenHeight = (currentRotation == 1 || currentRotation == 3) ? 240 : 320;
  
  int charWidth = (currentFont == 1) ? 11 : 6;
  int charHeight = (currentFont == 1) ? 16 : 8;
  
  int charsPerLine = screenWidth / (charWidth * currentSize);
  if (charsPerLine < 1) charsPerLine = 1;

  int startIdx = 0;
  int len = rawText.length();

  if (len == 0) {
    totalLines = 0;
    scrollOffset = 0;
    return;
  }

  while (startIdx < len && totalLines < MAX_LINES) {
    int nextNewline = rawText.indexOf('\n', startIdx);
    int chunkEnd = (nextNewline != -1 && nextNewline - startIdx <= charsPerLine) 
                   ? nextNewline 
                   : min(startIdx + charsPerLine, len);

    if (chunkEnd < len && nextNewline != chunkEnd && rawText.charAt(chunkEnd) != ' ' && rawText.charAt(chunkEnd) != '\n') {
      int lastSpace = rawText.lastIndexOf(' ', chunkEnd);
      if (lastSpace > startIdx) {
        chunkEnd = lastSpace;
      }
    }

    String line = rawText.substring(startIdx, chunkEnd);
    line.trim();
    
    if (currentAlign == 1) {
      int spacesNeeded = (charsPerLine - line.length()) / 2;
      for (int s = 0; s < spacesNeeded; s++) line = " " + line;
    }
    
    while (line.length() < charsPerLine) {
      line += " ";
    }
    
    wrappedLines[totalLines++] = line;

    startIdx = chunkEnd;
    while (startIdx < len && (rawText.charAt(startIdx) == ' ' || rawText.charAt(startIdx) == '\n')) {
      startIdx++;
    }
  }

  int maxVisibleLines = (screenHeight - (showDebug ? 24 : 0)) / (charHeight * currentSize);
  if (scrollOffset > max(0, totalLines - maxVisibleLines)) {
    scrollOffset = max(0, totalLines - maxVisibleLines);
  }
}

void transmitViewportToUno() {
  int screenWidth = (currentRotation == 1 || currentRotation == 3) ? 320 : 240;
  int screenHeight = (currentRotation == 1 || currentRotation == 3) ? 240 : 320;
  int charHeight = (currentFont == 1) ? 16 : 8;
  
  int maxVisibleLines = (screenHeight - (showDebug ? 24 : 0)) / (charHeight * currentSize);
  int linesToSend = min(maxVisibleLines, totalLines - scrollOffset);
  if (linesToSend < 0) linesToSend = 0;

  while (digitalRead(CTS_PIN) == HIGH) { delayMicroseconds(50); }
  
  Serial2.print("X,");
  Serial2.print(currentSize); Serial2.print(',');
  Serial2.print(currentFg); Serial2.print(',');
  Serial2.print(currentBg); Serial2.print(',');
  Serial2.print(showDebug ? 1 : 0); Serial2.print(',');
  Serial2.print(linesToSend); Serial2.print(',');
  Serial2.print(totalLines); Serial2.print(',');
  Serial2.print(scrollOffset); Serial2.print(',');
  Serial2.print(currentRotation); Serial2.print(',');
  Serial2.print(currentFont); Serial2.print(',');
  Serial2.print(forceClear); Serial2.print('\n');

  delay(15); 
  while (digitalRead(CTS_PIN) == HIGH) { delay(1); } 

  for (int i = 0; i < linesToSend; i++) {
    Serial2.print(wrappedLines[scrollOffset + i]);
    Serial2.print('\n');
    delay(5); 
    while (digitalRead(CTS_PIN) == HIGH) { delay(1); }
  }
  
  forceClear = 0; 
}

const char* html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=0">
<style>
  * { box-sizing: border-box; font-family: -apple-system, sans-serif; margin: 0; padding: 0; }
  body { height: 100vh; width: 100vw; overflow: hidden; background: #f4f4f4; color: #111; display: flex; flex-direction: column; padding: 10px; }
  .section { border: 1px solid #ddd; border-radius: 6px; padding: 8px; margin-bottom: 6px; background: #fff; }
  .row { display: flex; gap: 8px; align-items: center; margin-bottom: 6px; }
  .row:last-child { margin-bottom: 0; }
  label { font-size: 11px; font-weight: bold; width: 55px; color: #555; }
  input[type="range"] { flex: 1; }
  input[type="color"] { width: 30px; height: 24px; border: 1px solid #ccc; border-radius: 4px; padding: 0; cursor: pointer; }
  select { flex: 1; padding: 4px; font-size: 12px; border: 1px solid #ccc; border-radius: 4px; }
  textarea { width: 100%; height: 90px; border: 1px solid #ccc; border-radius: 4px; padding: 6px; font-size: 13px; resize: none; }
  .btn-group { display: flex; gap: 8px; margin-bottom: 6px; }
  button { padding: 9px; border: none; font-weight: bold; border-radius: 5px; width: 100%; font-size: 13px; color: #fff; }
  #updateBtn { background: #007aff; }
  #updateBtn:active { background: #0056b3; }
  #clearBtn { background: #ff3b30; }
  #clearBtn:active { background: #c62828; }
  .log-box { flex: 1; border: 1px solid #222; border-radius: 4px; background: #1e1e1e; color: #0f0; font-family: monospace; font-size: 10px; padding: 6px; overflow-y: auto; }
</style>
</head>
<body>
  <div class="section">
    <div class="row">
      <label>Size: <span id="sizeVal">1</span></label>
      <input type="range" id="sizeInput" min="1" max="4" value="1">
      <label style="margin-left:10px; width:20px;">HUD</label>
      <input type="checkbox" id="hudCheck" checked>
    </div>
    <div class="row">
      <label>Layout</label>
      <select id="layoutSelect">
        <option value="0">Portrait</option>
        <option value="1">Landscape</option>
      </select>
      <label style="margin-left:5px; width:70px; color:#ff3b30;">Invert (180&deg;)</label>
      <input type="checkbox" id="invertCheck">
    </div>
    <div class="row">
      <label>Font</label>
      <select id="fontInput">
        <option value="0">Default</option>
        <option value="1">Free Mono</option>
      </select>
      <label style="margin-left:5px; width:30px;">Align</label>
      <select id="alignInput">
        <option value="0">Left</option>
        <option value="1">Center</option>
      </select>
    </div>
    <div class="row">
      <label>Colors</label>
      <input type="color" id="fgInput" value="#FFFFFF"> (Text)
      <input type="color" id="bgInput" value="#000000" style="margin-left:10px;"> (BG)
    </div>
  </div>

  <div class="section">
    <textarea id="textContent" placeholder="Enter text...">System initialized.
Line limit expanded to 1000.
Ready for large payloads.</textarea>
  </div>

  <div class="btn-group">
    <button id="clearBtn">Clear Screen</button>
    <button id="updateBtn">Push Text</button>
  </div>

  <div class="log-box" id="sysLog">System Initialized. Ready for data.</div>

<script>
  let lastState = {};

  function addLog(msg) {
    const logBox = document.getElementById('sysLog');
    logBox.innerHTML += `<div>[>>] ${msg}</div>`;
    logBox.scrollTop = logBox.scrollHeight;
  }

  function hexToRgb565(hex) {
    let r = parseInt(hex.substr(1,2), 16) >> 3;
    let g = parseInt(hex.substr(3,2), 16) >> 2;
    let b = parseInt(hex.substr(5,2), 16) >> 3;
    return ((r << 11) | (g << 5) | b);
  }

  function sendData(isClear) {
    let s = document.getElementById('sizeInput').value;
    let f = document.getElementById('fontInput').value;
    let a = document.getElementById('alignInput').value;
    
    let isLandscape = document.getElementById('layoutSelect').value === "1";
    let isInverted = document.getElementById('invertCheck').checked;
    let r = 0;
    if (!isLandscape && !isInverted) r = 0;
    else if (isLandscape && !isInverted) r = 1;
    else if (!isLandscape && isInverted) r = 2;
    else if (isLandscape && isInverted) r = 3;
    
    let needsWipe = isClear || lastState.s !== s || lastState.f !== f || lastState.r !== r || lastState.a !== a;
    lastState = {s, f, r, a};
    
    document.getElementById('sizeVal').innerText = s;

    let formData = new FormData();
    formData.append("size", s);
    formData.append("font", f);
    formData.append("rot", r);
    formData.append("align", a);
    formData.append("fg", hexToRgb565(document.getElementById('fgInput').value));
    formData.append("bg", hexToRgb565(document.getElementById('bgInput').value));
    formData.append("debug", document.getElementById('hudCheck').checked ? 1 : 0);
    formData.append("force", needsWipe ? 1 : 0);
    formData.append("text", isClear ? "" : document.getElementById('textContent').value);

    addLog("Transmitting viewport data...");

    fetch('/update_text', { method: 'POST', body: formData })
      .then(() => addLog("Transmission Complete."))
      .catch(() => addLog("ERROR: Connection failed."));
  }

  document.getElementById('updateBtn').onclick = () => sendData(false);
  document.getElementById('clearBtn').onclick = () => {
    document.getElementById('textContent').value = "";
    sendData(true);
  };
</script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RX2_PIN, TX2_PIN);
  pinMode(CTS_PIN, INPUT);
  pinMode(BTN_UP, INPUT_PULLDOWN);
  pinMode(BTN_DOWN, INPUT);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(250); }

  server.on("/", []() { server.send(200, "text/html", html); });

  server.on("/update_text", HTTP_POST, []() {
    if (server.hasArg("size")) currentSize = server.arg("size").toInt();
    if (server.hasArg("font")) currentFont = server.arg("font").toInt();
    if (server.hasArg("rot")) currentRotation = server.arg("rot").toInt();
    if (server.hasArg("align")) currentAlign = server.arg("align").toInt();
    if (server.hasArg("fg")) currentFg = server.arg("fg").toInt();
    if (server.hasArg("bg")) currentBg = server.arg("bg").toInt();
    if (server.hasArg("debug")) showDebug = (server.arg("debug").toInt() == 1);
    if (server.hasArg("force")) forceClear = server.arg("force").toInt();
    if (server.hasArg("text")) rawText = server.arg("text");

    scrollOffset = 0;
    wrapText();
    transmitViewportToUno();
    server.send(200, "text/plain", "OK");
  });

  server.begin();
  wrapText();
  delay(1000);
  transmitViewportToUno();
}

void loop() {
  server.handleClient();

  if (millis() - lastBtnCheck > 40) {
    lastBtnCheck = millis();
    bool upPressed = (digitalRead(BTN_UP) == HIGH);
    bool downPressed = (digitalRead(BTN_DOWN) == HIGH);

    if (upPressed && scrollOffset > 0) {
      scrollOffset--;
      forceClear = 0;
      transmitViewportToUno();
      delay(150);
    } else if (downPressed) {
      int screenHeight = (currentRotation == 1 || currentRotation == 3) ? 240 : 320;
      int charHeight = (currentFont == 1) ? 16 : 8;
      int maxVisibleLines = (screenHeight - (showDebug ? 24 : 0)) / (charHeight * currentSize);
      
      if (scrollOffset + maxVisibleLines < totalLines) {
        scrollOffset++;
        forceClear = 0; 
        transmitViewportToUno();
        delay(150);
      }
    }
  }
}