#ifndef WEB_DASHBOARD_H
#define WEB_DASHBOARD_H

#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="th">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Cow-Stress-Calibrator (192.168.4.1)</title>
  <style>
    :root {
      --bg: oklch(97.2% 0.007 80);
      --surface: oklch(94.2% 0.01 80);
      --fg: oklch(20% 0.012 70);
      --muted: oklch(48% 0.014 75);
      --hairline: oklch(88% 0.012 80);
      --track: oklch(84% 0.014 80);
      --stand-fg: oklch(48% 0.13 152);
      --stand-bg: oklch(92% 0.04 152);
      --stand-pill-fg: oklch(34% 0.10 152);
      --lie-fg: oklch(52% 0.16 38);
      --lie-bg: oklch(92% 0.05 42);
      --lie-pill-fg: oklch(38% 0.13 38);
    }
    @media (prefers-color-scheme: dark) {
      :root {
        --bg: oklch(16.5% 0.008 75);
        --surface: oklch(20.5% 0.009 75);
        --fg: oklch(93% 0.008 80);
        --muted: oklch(66% 0.012 75);
        --hairline: oklch(26% 0.01 75);
        --track: oklch(30% 0.012 75);
        --stand-fg: oklch(72% 0.14 152);
        --stand-bg: oklch(26% 0.05 152);
        --stand-pill-fg: oklch(82% 0.11 152);
        --lie-fg: oklch(74% 0.15 42);
        --lie-bg: oklch(27% 0.06 40);
        --lie-pill-fg: oklch(83% 0.12 42);
      }
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      padding: 20px;
      font-family: "Plus Jakarta Sans", "IBM Plex Sans Thai", -apple-system, BlinkMacSystemFont, sans-serif;
      background: var(--bg);
      color: var(--fg);
      line-height: 1.45;
      -webkit-font-smoothing: antialiased;
    }
    @media (min-width: 768px) { body { padding: 36px 40px; } }
    .wrap { max-width: 1120px; margin: 0 auto; }
    .tnum { font-variant-numeric: tabular-nums; }
    .subtle { color: var(--muted); }
    .hero-metric {
      font-size: clamp(2.25rem, 4.5vw, 3.25rem);
      line-height: 1;
      letter-spacing: -0.035em;
      font-weight: 700;
    }
    .sub-metric {
      font-size: clamp(1.5rem, 2.8vw, 2rem);
      line-height: 1.1;
      letter-spacing: -0.025em;
      font-weight: 650;
    }

    /* Header */
    .masthead {
      display: flex;
      flex-wrap: wrap;
      align-items: baseline;
      justify-content: space-between;
      gap: 12px;
      padding-bottom: 22px;
      border-bottom: 1px solid var(--hairline);
    }
    .title {
      font-size: clamp(1.25rem, 2vw, 1.5rem);
      font-weight: 700;
      letter-spacing: -0.025em;
      margin: 0;
    }

    /* Editorial Telemetry Strip */
    .strip {
      padding: 30px 0;
      border-bottom: 1px solid var(--hairline);
      display: grid;
      grid-template-columns: repeat(12, 1fr);
      gap: 24px;
      align-items: end;
    }
    .col-focal { grid-column: span 4; }
    .col-stat { grid-column: span 2; }
    @media (max-width: 900px) {
      .col-focal { grid-column: span 12; }
      .col-stat { grid-column: span 6; }
    }
    .pill {
      display: inline-block;
      padding: 2px 10px;
      border-radius: 999px;
      font-size: 12px;
      font-weight: 600;
    }
    .pill-stand { background: var(--stand-bg); color: var(--stand-pill-fg); }
    .pill-lie { background: var(--lie-bg); color: var(--lie-pill-fg); }

    /* Buttons (44px min touch target) */
    .btn {
      min-height: 44px;
      padding: 0 16px;
      border-radius: 8px;
      border: none;
      background: var(--surface);
      color: var(--fg);
      font-size: 13px;
      font-weight: 600;
      cursor: pointer;
      text-decoration: none;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      transition: transform 90ms ease, opacity 120ms ease;
    }
    .btn:active { transform: scale(0.98); }
    .btn-ink {
      background: var(--fg);
      color: var(--bg);
    }

    /* Workbench 2-Column Layout */
    .workbench {
      padding: 32px 0;
      display: grid;
      grid-template-columns: repeat(12, 1fr);
      gap: 40px;
    }
    .col-charts { grid-column: span 7; }
    .col-controls {
      grid-column: span 5;
      padding-left: 32px;
      border-left: 1px solid var(--hairline);
    }
    @media (max-width: 960px) {
      .col-charts { grid-column: span 12; }
      .col-controls {
        grid-column: span 12;
        padding-left: 0;
        border-left: none;
        padding-top: 28px;
        border-top: 1px solid var(--hairline);
      }
    }
    .chart-block { margin-bottom: 32px; }
    .chart-head {
      display: flex;
      flex-wrap: wrap;
      align-items: baseline;
      justify-content: space-between;
      gap: 8px;
      margin-bottom: 10px;
    }
    .legend {
      display: flex;
      flex-wrap: wrap;
      gap: 14px;
      font-size: 12px;
      color: var(--muted);
    }
    canvas {
      width: 100%;
      height: 200px;
      background: var(--surface);
      border-radius: 12px;
      display: block;
    }

    /* Sliders & Inline Click-to-Edit Number Inputs */
    .slider-group { margin-bottom: 28px; }
    .slider-item { margin-top: 14px; }
    .slider-label {
      display: flex;
      align-items: center;
      justify-content: space-between;
      font-size: 14px;
      margin-bottom: 2px;
    }
    .num-wrap {
      display: inline-flex;
      align-items: center;
      gap: 4px;
    }
    .num-edit {
      -moz-appearance: textfield;
      appearance: textfield;
      width: 68px;
      padding: 3px 6px;
      border-radius: 6px;
      border: 1px solid transparent;
      background: var(--surface);
      color: var(--fg);
      font-family: inherit;
      font-weight: 600;
      font-size: 14px;
      text-align: right;
      font-variant-numeric: tabular-nums;
      transition: border-color 120ms ease, background-color 120ms ease;
    }
    .num-edit:hover {
      border-color: var(--track);
    }
    .num-edit:focus {
      outline: none;
      background: var(--bg);
      border-color: var(--stand-fg);
    }
    .num-edit::-webkit-outer-spin-button,
    .num-edit::-webkit-inner-spin-button {
      -webkit-appearance: none;
      margin: 0;
    }
    input[type="range"] {
      -webkit-appearance: none;
      appearance: none;
      width: 100%;
      height: 32px;
      background: transparent;
      cursor: pointer;
      margin: 0;
    }
    input[type="range"]::-webkit-slider-runnable-track {
      height: 4px;
      border-radius: 999px;
      background: var(--track);
    }
    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 20px;
      height: 20px;
      margin-top: -8px;
      border-radius: 50%;
      background: var(--fg);
      border: 2px solid var(--bg);
      box-shadow: 0 1px 4px rgba(0,0,0,0.25);
    }

    /* Export Section */
    .export-sec {
      padding-top: 28px;
      border-top: 1px solid var(--hairline);
    }
    pre {
      background: var(--surface);
      border-radius: 12px;
      padding: 16px;
      font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
      font-size: 12px;
      line-height: 1.6;
      overflow-x: auto;
      margin: 12px 0 0 0;
    }
  </style>
</head>
<body>
  <div class="wrap">

    <!-- Top Masthead -->
    <header class="masthead">
      <div style="display:flex;align-items:baseline;gap:16px;flex-wrap:wrap;">
        <h1 class="title">Cow-Stress-Calibrator</h1>
        <span class="subtle tnum" style="font-size:14px;">
          ต้นขาหลัง · อุณหภูมิ <b id="val-temp" style="color:var(--fg);">--</b>°C
        </span>
      </div>
      <div class="subtle tnum" style="font-size:13px;">
        สถานะ: <b id="conn-text" style="color:var(--fg);">กำลังเชื่อมต่อ...</b>
      </div>
    </header>

    <!-- Editorial Telemetry Strip -->
    <section class="strip">

      <!-- Primary Focal Metric: Posture & Leg Angle -->
      <div class="col-focal">
        <div style="display:flex;align-items:center;gap:10px;margin-bottom:8px;">
          <span class="subtle" style="font-size:14px;font-weight:500;">สถานะท่าทาง</span>
          <span id="lock-pill" class="pill pill-stand">เปิดนับก้าว</span>
          <span id="posture-timer-badge" class="tnum" style="display:none;font-size:12px;font-weight:600;color:var(--lie-fg);"></span>
        </div>
        <div class="tnum" style="display:flex;align-items:baseline;gap:20px;">
          <span id="posture-text" class="hero-metric" style="color:var(--stand-fg);">ยืน</span>
          <span class="sub-metric subtle"><span id="val-tilt">0.0</span>°</span>
        </div>
      </div>

      <!-- Step Count -->
      <div class="col-stat">
        <div class="subtle" style="font-size:14px;font-weight:500;margin-bottom:8px;">ก้าวที่นับได้ (ACT)</div>
        <div class="tnum" style="display:flex;align-items:baseline;gap:10px;">
          <span class="hero-metric" id="val-steps">0</span>
          <span class="subtle" style="font-size:12px;">เดิม <b id="val-legacy-steps">0</b></span>
        </div>
      </div>

      <!-- Standing Count & Time -->
      <div class="col-stat">
        <div class="subtle" style="font-size:14px;font-weight:500;margin-bottom:8px;">ยืนสะสม (Stand)</div>
        <div class="tnum" style="display:flex;align-items:baseline;gap:8px;">
          <span class="sub-metric" id="val-stand-count">1</span>
          <span class="subtle" style="font-size:12px;">ครั้ง · <b id="val-stand-time">0s</b></span>
        </div>
      </div>

      <!-- Lying Count & Time -->
      <div class="col-stat">
        <div class="subtle" style="font-size:14px;font-weight:500;margin-bottom:8px;">นอนสะสม (Sleep)</div>
        <div class="tnum" style="display:flex;align-items:baseline;gap:8px;">
          <span class="sub-metric" id="val-lie-count">0</span>
          <span class="subtle" style="font-size:12px;">ครั้ง · <b id="val-lie-time">0s</b></span>
        </div>
      </div>

      <!-- Manual Ground Truth Action -->
      <div class="col-stat">
        <div class="subtle tnum" style="display:flex;justify-content:space-between;font-size:14px;font-weight:500;margin-bottom:8px;">
          <span>นับด้วยตา: <b id="val-manual" style="color:var(--fg);">0</b></span>
          <span id="val-error" style="font-size:12px;">±0%</span>
        </div>
        <button class="btn btn-ink" style="width:100%;" onclick="addManualStep()">+1 กดเมื่อเห็นวัวก้าว</button>
      </div>

    </section>

    <!-- Asymmetric 2-Column Workbench -->
    <div class="workbench">

      <!-- Left Column: Dual Waveforms -->
      <div class="col-charts">

        <!-- Waveform 1 -->
        <div class="chart-block">
          <div class="chart-head">
            <b style="font-size:16px;">แรงกระแทกก้าวขา (<span class="tnum" id="val-dyn">0.00</span> g)</b>
            <div class="legend">
              <span>━ แรงขยับขา</span>
              <span style="color:#ea580c;">--- เริ่มนับก้าว</span>
              <span>--- รีเซ็ต</span>
              <span style="color:#15803d;">● นับก้าว</span>
            </div>
          </div>
          <canvas id="stepChart" width="560" height="200"></canvas>
        </div>

        <!-- Waveform 2 -->
        <div class="chart-block">
          <div class="chart-head">
            <b style="font-size:16px;">มุมเอียงต้นขาหลัง (0° – 90°)</b>
            <div class="legend">
              <span>━ มุมขาจริง</span>
              <span style="color:#ea580c;">--- เกณฑ์นอน</span>
              <span style="color:#15803d;">--- เกณฑ์ยืน</span>
            </div>
          </div>
          <canvas id="tiltChart" width="560" height="200"></canvas>
        </div>

        <!-- Utility Actions -->
        <div style="display:flex;flex-wrap:wrap;align-items:center;justify-content:space-between;gap:12px;">
          <div style="display:flex;flex-wrap:wrap;gap:10px;">
            <button class="btn" onclick="calibrateStand()">ตั้งท่ายืนปัจจุบันเป็น 0° (Calibrate)</button>
            <button class="btn" onclick="resetCounters()">รีเซ็ตตัวนับทั้งหมด</button>
            <a class="btn" href="/api/csv" download="cow_sensor_log.csv">ดาวน์โหลด CSV</a>
          </div>
          <span id="calib-status" class="subtle tnum" style="font-size:12px;">REF (0.00, 0.00, 1.00)</span>
        </div>

      </div>

      <!-- Right Column: Parameter Tuning Controls -->
      <div class="col-controls">

        <div class="slider-group">
          <div style="display:flex;align-items:baseline;justify-content:space-between;">
            <b style="font-size:16px;">การนับก้าว</b>
            <span class="subtle" style="font-size:12px;">คลิกตัวเลขเพื่อพิมพ์ค่าได้</span>
          </div>

          <div class="slider-item">
            <div class="slider-label tnum">
              <span>แรงเริ่มนับก้าว <span class="subtle" style="font-size:12px;">(STEP_TRIGGER_G)</span></span>
              <label class="num-wrap">
                <input type="number" id="num-trigger" class="num-edit" min="0.05" max="2.00" step="0.01" value="0.35"
                  onfocus="this.select()" oninput="onNumInput('trigger')" onchange="onNumCommit()">
                <b class="subtle">g</b>
              </label>
            </div>
            <input type="range" id="inp-trigger" min="0.10" max="1.00" step="0.01" value="0.35" oninput="onSliderInput()" onchange="pushParams()">
          </div>

          <div class="slider-item">
            <div class="slider-label tnum">
              <span>แรงรีเซ็ตก่อนก้าวถัดไป <span class="subtle" style="font-size:12px;">(STEP_RESET_G)</span></span>
              <label class="num-wrap">
                <input type="number" id="num-reset" class="num-edit" min="0.01" max="1.50" step="0.01" value="0.15"
                  onfocus="this.select()" oninput="onNumInput('reset')" onchange="onNumCommit()">
                <b class="subtle">g</b>
              </label>
            </div>
            <input type="range" id="inp-reset" min="0.05" max="0.50" step="0.01" value="0.15" oninput="onSliderInput()" onchange="pushParams()">
          </div>

          <div class="slider-item">
            <div class="slider-label tnum">
              <span>เวลาห่างขั้นต่ำระหว่างก้าว <span class="subtle" style="font-size:12px;">(STEP_DELAY_MS)</span></span>
              <label class="num-wrap">
                <input type="number" id="num-delay" class="num-edit" min="100" max="3000" step="10" value="500"
                  onfocus="this.select()" oninput="onNumInput('delay')" onchange="onNumCommit()">
                <b class="subtle">ms</b>
              </label>
            </div>
            <input type="range" id="inp-delay" min="200" max="1500" step="10" value="500" oninput="onSliderInput()" onchange="pushParams()">
          </div>

          <div class="slider-item">
            <div class="slider-label tnum">
              <span>ความเนียนตัวกรอง <span class="subtle" style="font-size:12px;">(ACCEL_SMOOTH_ALPHA)</span></span>
              <label class="num-wrap">
                <input type="number" id="num-alpha" class="num-edit" min="0.05" max="1.00" step="0.01" value="0.35"
                  onfocus="this.select()" oninput="onNumInput('alpha')" onchange="onNumCommit()">
              </label>
            </div>
            <input type="range" id="inp-alpha" min="0.05" max="1.00" step="0.01" value="0.35" oninput="onSliderInput()" onchange="pushParams()">
          </div>
        </div>

        <div class="slider-group" style="padding-top:24px;border-top:1px solid var(--hairline);margin-bottom:0;">
          <b style="font-size:16px;">การตรวจท่านอนและเวลาหน่วง</b>

          <div class="slider-item">
            <div class="slider-label tnum">
              <span>มุมขาที่ถือว่านอน <span class="subtle" style="font-size:12px;">(LIE_ANGLE_DEG)</span></span>
              <label class="num-wrap">
                <input type="number" id="num-lie-ang" class="num-edit" min="15" max="85" step="1" value="45"
                  onfocus="this.select()" oninput="onNumInput('lie-ang')" onchange="onNumCommit()">
                <b class="subtle">°</b>
              </label>
            </div>
            <input type="range" id="inp-lie-ang" min="20" max="80" step="1" value="45" oninput="onSliderInput()" onchange="pushParams()">
          </div>

          <div class="slider-item">
            <div class="slider-label tnum">
              <span>มุมขาที่ถือว่ายืน <span class="subtle" style="font-size:12px;">(STAND_ANGLE_DEG)</span></span>
              <label class="num-wrap">
                <input type="number" id="num-stand-ang" class="num-edit" min="5" max="75" step="1" value="30"
                  onfocus="this.select()" oninput="onNumInput('stand-ang')" onchange="onNumCommit()">
                <b class="subtle">°</b>
              </label>
            </div>
            <input type="range" id="inp-stand-ang" min="10" max="65" step="1" value="30" oninput="onSliderInput()" onchange="pushParams()">
          </div>

          <div class="slider-item">
            <div class="slider-label tnum">
              <span>หน่วงเวลา ยืน → นอน <span class="subtle" style="font-size:12px;">(STAND_TO_LIE_DELAY_SEC)</span></span>
              <label class="num-wrap">
                <input type="number" id="num-to-lie" class="num-edit" min="1" max="120" step="1" value="10"
                  onfocus="this.select()" oninput="onNumInput('to-lie')" onchange="onNumCommit()">
                <b class="subtle">วินาที</b>
              </label>
            </div>
            <input type="range" id="inp-to-lie" min="1" max="60" step="1" value="10" oninput="onSliderInput()" onchange="pushParams()">
          </div>

          <div class="slider-item">
            <div class="slider-label tnum">
              <span>หน่วงเวลา นอน → ยืน <span class="subtle" style="font-size:12px;">(LIE_TO_STAND_DELAY_SEC)</span></span>
              <label class="num-wrap">
                <input type="number" id="num-to-stand" class="num-edit" min="1" max="120" step="1" value="5"
                  onfocus="this.select()" oninput="onNumInput('to-stand')" onchange="onNumCommit()">
                <b class="subtle">วินาที</b>
              </label>
            </div>
            <input type="range" id="inp-to-stand" min="1" max="60" step="1" value="5" oninput="onSliderInput()" onchange="pushParams()">
          </div>
        </div>

      </div>

    </div>

    <!-- Firmware Export Footer -->
    <section class="export-sec">
      <div style="display:flex;flex-wrap:wrap;align-items:center;justify-content:space-between;gap:12px;">
        <div>
          <b style="font-size:16px;">ค่าคงที่สำหรับใส่ใน Cow-Stress-Tracker.ino</b>
          <div class="subtle" style="font-size:12px;">คัดลอกไปวางทับส่วนตั้งค่าด้านบนสุดของไฟล์ Cow-Stress-Tracker.ino</div>
        </div>
        <button class="btn btn-ink" id="btn-copy" onclick="copyDefines()">คัดลอกโค้ด #define</button>
      </div>
      <pre id="code-output" class="tnum"></pre>
    </section>

  </div>

  <script>
    const MAX_POINTS = 250;
    const stepHistory = [];
    const tiltHistory = [];
    let lastSampleId = 0;
    let slidersLoaded = false;

    let params = {
      stepTriggerG: 0.35,
      stepResetG: 0.15,
      stepDelayMs: 500,
      smoothAlpha: 0.35,
      lieAngleDeg: 45,
      standAngleDeg: 30,
      standToLieSec: 10,
      lieToStandSec: 5,
      refGrav: [0.00, 0.00, 1.00]
    };

    let isStandingState = true;

    function formatDuration(ms) {
      const s = Math.floor(ms / 1000);
      if (s < 60) return s + 's';
      const m = Math.floor(s / 60);
      const rem = s % 60;
      return m + 'm ' + rem + 's';
    }

    function clamp(v, min, max) {
      return Math.min(max, Math.max(min, v));
    }

    function enforceHysteresis() {
      if (params.stepResetG >= params.stepTriggerG) {
        params.stepResetG = Math.max(0.01, +(params.stepTriggerG - 0.02).toFixed(2));
      }
      if (params.standAngleDeg >= params.lieAngleDeg) {
        params.standAngleDeg = Math.max(5, params.lieAngleDeg - 2);
      }
    }

    function onSliderInput() {
      params.stepTriggerG = parseFloat(document.getElementById('inp-trigger').value);
      params.stepResetG = parseFloat(document.getElementById('inp-reset').value);
      params.stepDelayMs = parseInt(document.getElementById('inp-delay').value, 10);
      params.smoothAlpha = parseFloat(document.getElementById('inp-alpha').value);
      params.lieAngleDeg = parseInt(document.getElementById('inp-lie-ang').value, 10);
      params.standAngleDeg = parseInt(document.getElementById('inp-stand-ang').value, 10);
      params.standToLieSec = parseInt(document.getElementById('inp-to-lie').value, 10);
      params.lieToStandSec = parseInt(document.getElementById('inp-to-stand').value, 10);
      enforceHysteresis();
      syncLabelsAndCode(false);
    }

    function onNumInput(key) {
      const raw = parseFloat(document.getElementById('num-' + key).value);
      if (isNaN(raw)) return;
      if (key === 'trigger') params.stepTriggerG = clamp(raw, 0.05, 2.00);
      else if (key === 'reset') params.stepResetG = clamp(raw, 0.01, 1.50);
      else if (key === 'delay') params.stepDelayMs = Math.round(clamp(raw, 100, 3000));
      else if (key === 'alpha') params.smoothAlpha = clamp(raw, 0.05, 1.00);
      else if (key === 'lie-ang') params.lieAngleDeg = Math.round(clamp(raw, 15, 85));
      else if (key === 'stand-ang') params.standAngleDeg = Math.round(clamp(raw, 5, 75));
      else if (key === 'to-lie') params.standToLieSec = Math.round(clamp(raw, 1, 120));
      else if (key === 'to-stand') params.lieToStandSec = Math.round(clamp(raw, 1, 120));
      enforceHysteresis();
      syncLabelsAndCode(true);
    }

    function onNumCommit() {
      enforceHysteresis();
      syncLabelsAndCode(false);
      pushParams();
    }

    function syncLabelsAndCode(skipActiveNum) {
      document.getElementById('inp-trigger').value = params.stepTriggerG;
      document.getElementById('inp-reset').value = params.stepResetG;
      document.getElementById('inp-delay').value = params.stepDelayMs;
      document.getElementById('inp-alpha').value = params.smoothAlpha;
      document.getElementById('inp-lie-ang').value = params.lieAngleDeg;
      document.getElementById('inp-stand-ang').value = params.standAngleDeg;
      document.getElementById('inp-to-lie').value = params.standToLieSec;
      document.getElementById('inp-to-stand').value = params.lieToStandSec;

      const active = document.activeElement;
      const setNum = (id, val) => {
        const el = document.getElementById(id);
        if (!skipActiveNum || el !== active) el.value = val;
      };
      setNum('num-trigger', params.stepTriggerG.toFixed(2));
      setNum('num-reset', params.stepResetG.toFixed(2));
      setNum('num-delay', params.stepDelayMs);
      setNum('num-alpha', params.smoothAlpha.toFixed(2));
      setNum('num-lie-ang', params.lieAngleDeg);
      setNum('num-stand-ang', params.standAngleDeg);
      setNum('num-to-lie', params.standToLieSec);
      setNum('num-to-stand', params.lieToStandSec);

      const code = [
        '// ================= ตั้งค่าการนับก้าวและการนอน (ได้จาก Cow-Stress-Calibrator) =================',
        '#define STEP_TRIGGER_G          ' + params.stepTriggerG.toFixed(2) + 'f   // แรงกระแทกเริ่มนับก้าว (g)',
        '#define STEP_RESET_G            ' + params.stepResetG.toFixed(2) + 'f   // แรงรีเซ็ตก่อนนับก้าวถัดไป (g)',
        '#define STEP_DELAY_MS           ' + params.stepDelayMs + 'UL    // ระยะเวลาขั้นต่ำระหว่างก้าว (ms)',
        '#define ACCEL_SMOOTH_ALPHA      ' + params.smoothAlpha.toFixed(2) + 'f   // ตัวกรองสัญญาณความเร่ง (0.1-1.0)',
        '#define LIE_ANGLE_DEG           ' + params.lieAngleDeg.toFixed(1) + 'f   // มุมเอียงขาที่ถือว่านอน (องศา)',
        '#define STAND_ANGLE_DEG         ' + params.standAngleDeg.toFixed(1) + 'f   // มุมเอียงขาที่ถือว่ายืน (องศา)',
        '#define STAND_TO_LIE_DELAY_SEC  ' + params.standToLieSec + 'UL     // หน่วงเวลาเปลี่ยนท่า ยืน -> นอน (วินาที)',
        '#define LIE_TO_STAND_DELAY_SEC  ' + params.lieToStandSec + 'UL     // หน่วงเวลาเปลี่ยนท่า นอน -> ยืน (วินาที)',
        '#define REF_GRAV_X              ' + params.refGrav[0].toFixed(3) + 'f',
        '#define REF_GRAV_Y              ' + params.refGrav[1].toFixed(3) + 'f',
        '#define REF_GRAV_Z              ' + params.refGrav[2].toFixed(3) + 'f'
      ].join('\n');
      document.getElementById('code-output').textContent = code;
    }

    function pushParams() {
      const qs = new URLSearchParams({
        trig: params.stepTriggerG,
        rst: params.stepResetG,
        dly: params.stepDelayMs,
        alp: params.smoothAlpha,
        lie: params.lieAngleDeg,
        std: params.standAngleDeg,
        t2l: params.standToLieSec,
        t2s: params.lieToStandSec
      });
      fetch('/api/params?' + qs.toString(), { method: 'POST' });
    }

    function calibrateStand() {
      fetch('/api/calibrate', { method: 'POST' })
        .then(r => r.json())
        .then(d => {
          params.refGrav = [d.rx, d.ry, d.rz];
          document.getElementById('calib-status').textContent =
            'REF (' + d.rx.toFixed(2) + ', ' + d.ry.toFixed(2) + ', ' + d.rz.toFixed(2) + ')';
          syncLabelsAndCode();
        });
    }

    function resetCounters() {
      fetch('/api/reset', { method: 'POST' });
    }

    function addManualStep() {
      fetch('/api/manual_step', { method: 'POST' });
    }

    function copyDefines() {
      const text = document.getElementById('code-output').textContent;
      const ta = document.createElement('textarea');
      ta.value = text;
      document.body.appendChild(ta);
      ta.select();
      document.execCommand('copy');
      document.body.removeChild(ta);
      const btn = document.getElementById('btn-copy');
      btn.textContent = 'คัดลอกเรียบร้อยแล้ว';
      setTimeout(() => { btn.textContent = 'คัดลอกโค้ด #define'; }, 1800);
    }

    function pollServer() {
      fetch('/api/poll?since=' + lastSampleId)
        .then(r => r.json())
        .then(d => {
          document.getElementById('conn-text').textContent = 'ออนไลน์ (50 Hz)';

          if (!slidersLoaded && d.p) {
            slidersLoaded = true;
            params.stepTriggerG = d.p[0];
            params.stepResetG = d.p[1];
            params.stepDelayMs = d.p[2];
            params.smoothAlpha = d.p[3];
            params.lieAngleDeg = d.p[4];
            params.standAngleDeg = d.p[5];
            params.standToLieSec = d.p[6];
            params.lieToStandSec = d.p[7];
            params.refGrav = [d.p[8], d.p[9], d.p[10]];

            document.getElementById('inp-trigger').value = params.stepTriggerG;
            document.getElementById('inp-reset').value = params.stepResetG;
            document.getElementById('inp-delay').value = params.stepDelayMs;
            document.getElementById('inp-alpha').value = params.smoothAlpha;
            document.getElementById('inp-lie-ang').value = params.lieAngleDeg;
            document.getElementById('inp-stand-ang').value = params.standAngleDeg;
            document.getElementById('inp-to-lie').value = params.standToLieSec;
            document.getElementById('inp-to-stand').value = params.lieToStandSec;
            document.getElementById('calib-status').textContent =
              'REF (' + params.refGrav[0].toFixed(2) + ', ' + params.refGrav[1].toFixed(2) + ', ' + params.refGrav[2].toFixed(2) + ')';
            syncLabelsAndCode();
          }

          isStandingState = (d.stand === 1);
          document.getElementById('val-tilt').textContent = d.tilt.toFixed(1);
          document.getElementById('val-dyn').textContent = d.dyn.toFixed(2);
          document.getElementById('val-temp').textContent = d.temp > -50 ? d.temp.toFixed(1) : '--';
          document.getElementById('val-steps').textContent = d.steps;
          document.getElementById('val-legacy-steps').textContent = d.lsteps;
          document.getElementById('val-manual').textContent = d.msteps;
          document.getElementById('val-stand-count').textContent = d.scnt;
          document.getElementById('val-lie-count').textContent = d.lcnt;
          document.getElementById('val-stand-time').textContent = formatDuration(d.sms);
          document.getElementById('val-lie-time').textContent = formatDuration(d.lms);

          if (d.msteps > 0) {
            const err = Math.round((Math.abs(d.steps - d.msteps) / d.msteps) * 100);
            document.getElementById('val-error').textContent = '±' + err + '%';
          } else {
            document.getElementById('val-error').textContent = '±0%';
          }

          const postureEl = document.getElementById('posture-text');
          const lockPill = document.getElementById('lock-pill');

          if (isStandingState) {
            postureEl.textContent = 'ยืน';
            postureEl.style.color = 'var(--stand-fg)';
            lockPill.textContent = 'เปิดนับก้าว';
            lockPill.className = 'pill pill-stand';
          } else {
            postureEl.textContent = 'นอน';
            postureEl.style.color = 'var(--lie-fg)';
            lockPill.textContent = 'ล็อกการนับก้าว';
            lockPill.className = 'pill pill-lie';
          }

          const badge = document.getElementById('posture-timer-badge');
          if (d.rem > 0.05) {
            badge.style.display = 'inline-block';
            badge.textContent = isStandingState
              ? ('นอนใน ' + d.rem.toFixed(1) + 's')
              : ('ยืนใน ' + d.rem.toFixed(1) + 's');
          } else {
            badge.style.display = 'none';
          }

          if (d.s && d.s.length > 0) {
            d.s.forEach(pt => {
              lastSampleId = pt[0];
              stepHistory.push({ v: pt[1], step: pt[3] === 1 });
              if (stepHistory.length > MAX_POINTS) stepHistory.shift();
              tiltHistory.push({ v: pt[2] });
              if (tiltHistory.length > MAX_POINTS) tiltHistory.shift();
            });
          }

          drawStepChart();
          drawTiltChart();
        })
        .catch(() => {
          document.getElementById('conn-text').textContent = 'ขาดการเชื่อมต่อ...';
        });
    }

    function isDark() {
      return window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches;
    }

    function drawCleanGrid(ctx, w, h, yTicks, yFormatter) {
      const padL = 38, padR = 12, padT = 12, padB = 20;
      const plotW = w - padL - padR;
      const plotH = h - padT - padB;

      ctx.strokeStyle = isDark() ? 'rgba(255,255,255,0.07)' : 'rgba(24,24,22,0.08)';
      ctx.lineWidth = 1;
      ctx.font = '11px sans-serif';
      yTicks.forEach(t => {
        const y = padT + (1 - t.norm) * plotH;
        ctx.beginPath();
        ctx.moveTo(padL, y);
        ctx.lineTo(w - padR, y);
        ctx.stroke();
        ctx.fillStyle = isDark() ? 'rgba(255,255,255,0.45)' : 'rgba(24,24,22,0.45)';
        ctx.fillText(yFormatter(t.val), 6, y + 3);
      });

      const xSecTicks = [-10, -8, -6, -4, -2, 0];
      xSecTicks.forEach(sec => {
        const normX = (sec + 10) / 10;
        const x = padL + normX * plotW;
        ctx.fillStyle = isDark() ? 'rgba(255,255,255,0.45)' : 'rgba(24,24,22,0.45)';
        ctx.fillText(sec + 's', x - 9, h - 4);
      });

      return { padL, padR, padT, padB, plotW, plotH };
    }

    function drawStepChart() {
      const canvas = document.getElementById('stepChart');
      const ctx = canvas.getContext('2d');
      const w = canvas.width, h = canvas.height;
      const maxG = 1.0;

      ctx.clearRect(0, 0, w, h);
      if (!isStandingState) {
        ctx.fillStyle = isDark() ? 'rgba(234, 88, 12, 0.09)' : 'rgba(234, 88, 12, 0.06)';
        ctx.fillRect(0, 0, w, h);
      }

      const yTicks = [0, 0.25, 0.5, 0.75, 1.0].map(v => ({ val: v, norm: v / maxG }));
      const box = drawCleanGrid(ctx, w, h, yTicks, v => v.toFixed(2));
      const yOf = (g) => box.padT + (1 - Math.min(maxG, Math.max(0, g)) / maxG) * box.plotH;
      const xOf = (idx) => box.padL + (idx / Math.max(1, MAX_POINTS - 1)) * box.plotW;

      const yTrig = yOf(params.stepTriggerG);
      ctx.strokeStyle = '#ea580c';
      ctx.setLineDash([5, 4]);
      ctx.lineWidth = 1.25;
      ctx.beginPath(); ctx.moveTo(box.padL, yTrig); ctx.lineTo(w - box.padR, yTrig); ctx.stroke();

      const yRst = yOf(params.stepResetG);
      ctx.strokeStyle = isDark() ? 'rgba(255,255,255,0.35)' : 'rgba(24,24,22,0.35)';
      ctx.beginPath(); ctx.moveTo(box.padL, yRst); ctx.lineTo(w - box.padR, yRst); ctx.stroke();
      ctx.setLineDash([]);

      if (stepHistory.length > 1) {
        ctx.strokeStyle = isDark() ? '#f4f3ef' : '#1c1b18';
        ctx.lineWidth = 1.8;
        ctx.beginPath();
        stepHistory.forEach((pt, idx) => {
          const x = xOf(idx);
          const y = yOf(pt.v);
          if (idx === 0) ctx.moveTo(x, y);
          else ctx.lineTo(x, y);
        });
        ctx.stroke();

        stepHistory.forEach((pt, idx) => {
          if (pt.step) {
            const x = xOf(idx);
            const y = yOf(pt.v);
            ctx.fillStyle = '#15803d';
            ctx.beginPath();
            ctx.arc(x, y, 4.5, 0, Math.PI * 2);
            ctx.fill();
          }
        });
      }
    }

    function drawTiltChart() {
      const canvas = document.getElementById('tiltChart');
      const ctx = canvas.getContext('2d');
      const w = canvas.width, h = canvas.height;
      const maxDeg = 90;

      ctx.clearRect(0, 0, w, h);
      const yTicks = [0, 30, 60, 90].map(v => ({ val: v, norm: v / maxDeg }));
      const box = drawCleanGrid(ctx, w, h, yTicks, v => v + '°');
      const yOf = (deg) => box.padT + (1 - Math.min(maxDeg, Math.max(0, deg)) / maxDeg) * box.plotH;
      const xOf = (idx) => box.padL + (idx / Math.max(1, MAX_POINTS - 1)) * box.plotW;

      const yLie = yOf(params.lieAngleDeg);
      ctx.strokeStyle = '#ea580c';
      ctx.setLineDash([5, 4]);
      ctx.lineWidth = 1.25;
      ctx.beginPath(); ctx.moveTo(box.padL, yLie); ctx.lineTo(w - box.padR, yLie); ctx.stroke();

      const yStand = yOf(params.standAngleDeg);
      ctx.strokeStyle = '#15803d';
      ctx.beginPath(); ctx.moveTo(box.padL, yStand); ctx.lineTo(w - box.padR, yStand); ctx.stroke();
      ctx.setLineDash([]);

      if (tiltHistory.length > 1) {
        ctx.strokeStyle = isDark() ? '#f4f3ef' : '#1c1b18';
        ctx.lineWidth = 1.8;
        ctx.beginPath();
        tiltHistory.forEach((pt, idx) => {
          const x = xOf(idx);
          const y = yOf(pt.v);
          if (idx === 0) ctx.moveTo(x, y);
          else ctx.lineTo(x, y);
        });
        ctx.stroke();
      }
    }

    syncLabelsAndCode();
    setInterval(pollServer, 200);
  </script>
</body>
</html>
)rawliteral";

#endif
