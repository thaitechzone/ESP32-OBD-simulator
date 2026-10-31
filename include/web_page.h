#pragma once

// =====================================================================
//  web_page.h -- หน้าเว็บควบคุมตัวจำลอง (HTML/CSS/JS ล้วน เก็บใน flash)
//  เปิดที่ http://obd-sim.local หรือ IP ที่ Serial พิมพ์ตอนบูต
//  ใช้ API ใน src/main.cpp: /api/state, /api/log, /api/scenario, /api/manual, /api/faults, /api/adapter, /api/drop
//  เปิดไฟล์นี้ตรง ๆ ในเบราว์เซอร์ไม่ได้ แต่ถ้าเรียก API ไม่ได้ หน้าเว็บจะเข้าโหมด DEMO (ค่าจำลองในเบราว์เซอร์) ให้ดูหน้าตาได้
// =====================================================================

#include <Arduino.h>

const char SIM_WEB_PAGE[] PROGMEM = R"SIMHTML(<!doctype html>
<html lang="th">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0B0C0E">
<title>OBD-SIM · ELM327 Simulator</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Chakra+Petch:wght@400;500;600;700&display=swap" media="print" onload="this.media='all'">
<style>
:root{--bg:#0B0C0E;--surf:#14171B;--surf2:#1A1E23;--line:#2A2F36;--track:#22262C;--muted:#8A9099;--text:#F2F2EE;
--accent:#FFB000;--accentSoft:rgba(255,176,0,.12);--ok:#3DDC97;--okSoft:rgba(61,220,151,.12);--bad:#FF4D3D;--badSoft:rgba(255,77,61,.12);--info:#5AA9FF;
--r:10px;--mono:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace}
*{box-sizing:border-box}
html{-webkit-text-size-adjust:100%}
body{margin:0;background:var(--bg);color:var(--text);font-family:'Chakra Petch',system-ui,-apple-system,'Segoe UI',sans-serif;font-variant-numeric:tabular-nums;-webkit-font-smoothing:antialiased}
button,input{font:inherit;color:inherit}
.wrap{max-width:1180px;margin:0 auto;padding:20px 16px 48px;display:flex;flex-direction:column;gap:16px}
.muted{color:var(--muted)}
.hidden{display:none!important}

/* header */
header{display:flex;flex-wrap:wrap;gap:12px 20px;align-items:center;justify-content:space-between;padding-bottom:14px;border-bottom:1px solid var(--line)}
.brand{display:flex;align-items:center;gap:12px}
.logo{width:40px;height:40px;border-radius:10px;background:var(--accent);display:grid;place-items:center;color:#000;flex:none}
.brand h1{margin:0;font-size:22px;letter-spacing:.5px;line-height:1.1}
.brand p{margin:2px 0 0;font-size:13px;color:var(--muted)}
.demo{font-size:11px;font-weight:700;background:var(--accent);color:#000;border-radius:3px;padding:2px 7px;margin-left:6px;vertical-align:middle;letter-spacing:.5px}
.chips{display:flex;flex-wrap:wrap;gap:8px}
.chip{display:flex;align-items:center;gap:8px;border:1px solid var(--line);border-radius:999px;padding:6px 12px;font-size:13px;background:var(--surf)}
.chip b{font-weight:600}
.dot{width:8px;height:8px;border-radius:50%;background:var(--muted);flex:none}
.dot.ok{background:var(--ok);box-shadow:0 0 0 3px var(--okSoft)}
.dot.bad{background:var(--bad)}
.dot.wait{background:var(--accent);animation:blink 1s steps(1) infinite}
@keyframes blink{50%{opacity:.25}}

/* layout */
.grid{display:grid;gap:16px}
.g2{grid-template-columns:1fr}
.g3{grid-template-columns:1fr}
@media(min-width:900px){.g2{grid-template-columns:1.05fr 1fr}.g3{grid-template-columns:1.2fr 1fr 1fr}}
.card{background:var(--surf);border:1px solid var(--line);border-radius:var(--r);padding:16px;min-width:0}
.card h2{margin:0 0 12px;font-size:12px;letter-spacing:2px;color:var(--muted);font-weight:600;display:flex;flex-wrap:wrap;align-items:center;justify-content:space-between;gap:8px}
.card h2 .side{letter-spacing:0;font-size:12px}

/* live */
.live{display:grid;grid-template-columns:auto 1fr;gap:16px;align-items:center}
@media(max-width:520px){.live{grid-template-columns:1fr}}
.dial{position:relative;width:200px;height:186px;margin:0 auto}
.dial svg{display:block}
.dial .speed{position:absolute;left:0;right:0;top:62px;text-align:center}
.dial .speed b{font-size:52px;font-weight:600;line-height:1;letter-spacing:-1px}
.dial .speed span{display:block;font-size:12px;color:var(--muted);margin-top:2px}
.dial .rpm{position:absolute;left:0;right:0;bottom:4px;text-align:center;font-size:12px;color:var(--muted)}
.dial .rpm b{font-size:20px;color:var(--accent);font-weight:700;margin-right:4px}
.dial .gear{position:absolute;right:6px;top:4px;font-size:11px;color:var(--muted);text-align:center;border:1px solid var(--line);border-radius:6px;padding:3px 7px}
.dial .gear b{display:block;font-size:18px;color:var(--text);line-height:1.1}
.tiles{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
.tile{background:var(--surf2);border-radius:8px;padding:10px 10px 9px}
.tile .l{font-size:11px;color:var(--muted);font-weight:600;letter-spacing:.5px}
.tile .v{font-size:24px;font-weight:600;margin:3px 0 6px;line-height:1}
.tile .v small{font-size:12px;color:var(--muted);font-weight:500;margin-left:2px}
.bar{height:4px;border-radius:2px;background:var(--track);overflow:hidden}
.bar i{display:block;height:100%;width:0;background:var(--text);border-radius:2px;transition:width .3s}
.ecuoff{margin-top:12px;border:1px dashed var(--bad);color:var(--bad);border-radius:8px;padding:8px 12px;font-size:13px;display:flex;gap:8px;align-items:center}

/* scenarios */
.scen{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:8px}
.sc{position:relative;text-align:left;background:var(--surf2);border:1px solid var(--line);border-radius:8px;padding:10px 10px 10px 42px;cursor:pointer;min-height:64px;transition:border-color .15s,background .15s}
.sc:hover{border-color:#3A4048}
.sc svg{position:absolute;left:11px;top:12px;color:var(--muted)}
.sc b{display:block;font-size:14px;font-weight:600}
.sc span{display:block;font-size:12px;color:var(--muted);margin-top:2px;line-height:1.35}
.sc.on{border-color:var(--accent);background:var(--accentSoft)}
.sc.on svg{color:var(--accent)}
.sc.on::after{content:'';position:absolute;right:9px;top:9px;width:8px;height:8px;border-radius:50%;background:var(--accent);box-shadow:0 0 0 3px var(--accentSoft)}

/* manual sliders */
.note{font-size:13px;color:var(--muted);background:var(--surf2);border-radius:8px;padding:8px 12px;margin-bottom:12px;line-height:1.45}
.note b{color:var(--accent);font-weight:600}
.sliders{display:grid;gap:12px}
.sl{display:grid;grid-template-columns:86px 1fr 74px;gap:10px;align-items:center}
.sl label{font-size:13px;color:var(--muted)}
.sl output{text-align:right;font-weight:600}
.sl output small{color:var(--muted);font-weight:500;font-size:11px;margin-left:2px}
input[type=range]{-webkit-appearance:none;appearance:none;width:100%;height:28px;background:transparent;cursor:pointer}
input[type=range]::-webkit-slider-runnable-track{height:4px;border-radius:2px;background:linear-gradient(90deg,var(--accent) var(--p,0%),var(--track) var(--p,0%))}
input[type=range]::-moz-range-track{height:4px;border-radius:2px;background:var(--track)}
input[type=range]::-moz-range-progress{height:4px;border-radius:2px;background:var(--accent)}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:18px;height:18px;border-radius:50%;background:var(--text);border:2px solid var(--bg);margin-top:-7px;box-shadow:0 0 0 1px var(--line)}
input[type=range]::-moz-range-thumb{width:16px;height:16px;border-radius:50%;background:var(--text);border:2px solid var(--bg)}
.presets{display:flex;flex-wrap:wrap;gap:6px;margin-top:14px}

/* buttons */
.btn{border:1px solid var(--line);background:var(--surf2);border-radius:6px;padding:7px 12px;font-size:13px;font-weight:600;cursor:pointer;transition:background .15s,border-color .15s}
.btn:hover{border-color:#3A4048}
.btn.primary{background:var(--accent);border-color:var(--accent);color:#000}
.btn.danger{border-color:var(--bad);color:var(--bad);background:transparent}
.btn.danger:hover{background:var(--badSoft)}
.btn:disabled{opacity:.4;cursor:not-allowed}

/* faults */
.mil{display:flex;align-items:center;gap:10px;margin-bottom:12px}
.milicon{width:34px;height:34px;border-radius:8px;display:grid;place-items:center;background:var(--surf2);color:var(--muted)}
.mil.on .milicon{background:var(--accent);color:#000}
.mil b{display:block;font-size:14px}
.mil span{font-size:12px;color:var(--muted)}
.codes{display:flex;flex-wrap:wrap;gap:6px;min-height:34px;margin-bottom:12px}
.code{display:inline-flex;align-items:center;gap:6px;background:var(--badSoft);border:1px solid var(--bad);color:var(--text);border-radius:6px;padding:4px 4px 4px 10px;font-weight:600;font-size:13px}
.code button{border:0;background:transparent;color:var(--muted);cursor:pointer;width:22px;height:22px;border-radius:4px;font-size:16px;line-height:1}
.code button:hover{background:rgba(255,255,255,.08);color:var(--text)}
.empty{font-size:13px;color:var(--muted);align-self:center}
.sub{font-size:11px;letter-spacing:1.5px;color:var(--muted);margin:4px 0 8px;font-weight:600}
.pchips{display:flex;flex-wrap:wrap;gap:6px}
.pc{border:1px solid var(--line);background:var(--surf2);border-radius:6px;padding:5px 9px;cursor:pointer;font-size:12px;text-align:left}
.pc b{display:block;font-size:13px}
.pc span{color:var(--muted)}
.pc.on{border-color:var(--bad);background:var(--badSoft)}
.addrow{display:flex;gap:6px;margin-top:12px}
.addrow input{flex:1;min-width:0;background:var(--bg);border:1px solid var(--line);border-radius:6px;padding:7px 10px;text-transform:uppercase;letter-spacing:1px}
.addrow input:focus{outline:none;border-color:var(--accent)}

/* adapter */
.rows{display:flex;flex-direction:column;gap:2px}
.row{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:10px 0;border-bottom:1px solid var(--line)}
.row:last-child{border-bottom:0}
.row b{display:block;font-size:14px;font-weight:600}
.row span{display:block;font-size:12px;color:var(--muted);margin-top:2px;line-height:1.35}
.sw{position:relative;width:46px;height:26px;flex:none;cursor:pointer}
.sw input{position:absolute;opacity:0;width:100%;height:100%;margin:0;cursor:pointer}
.sw i{position:absolute;inset:0;border-radius:13px;background:var(--track);transition:background .15s;pointer-events:none}
.sw i::after{content:'';position:absolute;left:3px;top:3px;width:20px;height:20px;border-radius:50%;background:var(--muted);transition:transform .15s,background .15s}
.sw input:checked+i{background:var(--okSoft);box-shadow:inset 0 0 0 1px var(--ok)}
.sw input:checked+i::after{transform:translateX(20px);background:var(--ok)}
.lat{display:grid;grid-template-columns:40px 1fr 60px;gap:8px;align-items:center;font-size:12px;color:var(--muted)}
.lat output{color:var(--text);text-align:right;font-weight:600;font-size:13px}

/* log */
.logbar{display:flex;flex-wrap:wrap;gap:8px 14px;align-items:center}
.chk{display:flex;align-items:center;gap:6px;font-size:13px;color:var(--muted);cursor:pointer;letter-spacing:0}
.chk input{accent-color:var(--accent)}
.log{background:#08090A;border:1px solid var(--line);border-radius:8px;height:300px;overflow:auto;padding:8px 0;font-family:var(--mono);font-size:12px;line-height:1.65}
.log div{padding:0 12px;white-space:pre-wrap;word-break:break-all}
.log div:hover{background:rgba(255,255,255,.03)}
.log .t{color:#5C626A;margin-right:10px}
.log .rx .k{color:var(--accent)}
.log .sim{color:var(--ok)}
.log .bt{color:var(--info)}
.log .net{color:#C792EA}
.log .err{color:var(--bad)}

footer{display:flex;flex-wrap:wrap;justify-content:space-between;gap:8px;font-size:12px;color:var(--muted)}
.toast{position:fixed;left:50%;bottom:20px;transform:translateX(-50%) translateY(120px);background:var(--surf);border:1px solid var(--bad);color:var(--text);border-radius:8px;padding:10px 16px;font-size:13px;transition:transform .25s;z-index:10;max-width:calc(100% - 32px)}
.toast.show{transform:translateX(-50%) translateY(0)}
.toast.ok{border-color:var(--ok)}
</style>
</head>
<body>
<div class="wrap">
  <header>
    <div class="brand">
      <div class="logo"><svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round"><path d="M4 16a8 8 0 1 1 16 0"/><path d="M12 16l4-5"/><circle cx="12" cy="16" r="1.4" fill="currentColor"/></svg></div>
      <div><h1>OBD-SIM<span class="demo hidden" id="demo">DEMO</span></h1><p>ตัวจำลองรถ + อะแดปเตอร์ ELM327 สำหรับทดสอบเกจ CYD</p></div>
    </div>
    <div class="chips">
      <div class="chip"><span class="dot" id="btDot"></span><span class="muted">Bluetooth</span><b id="btText">--</b></div>
      <div class="chip"><span class="dot ok" id="netDot"></span><span class="muted" id="netMode">WiFi</span><b id="netIp">--</b></div>
      <div class="chip"><span class="muted">คำสั่ง</span><b id="cmdRate">--</b></div>
    </div>
  </header>

  <div class="grid g2">
    <section class="card">
      <h2>ค่าที่ส่งให้ CYD <span class="side muted" id="liveScen">--</span></h2>
      <div class="live">
        <div class="dial">
          <svg width="200" height="186" viewBox="0 0 200 186">
            <path id="arcTrack" fill="none" stroke="#22262C" stroke-width="12" stroke-linecap="round"/>
            <path id="arcRed" fill="none" stroke="#FF4D3D" stroke-width="3"/>
            <path id="arcVal" fill="none" stroke="#FFB000" stroke-width="12" stroke-linecap="round"/>
            <g id="ticks"></g>
          </svg>
          <div class="gear">GEAR<b id="gear">N</b></div>
          <div class="speed"><b id="speed">--</b><span>km/h</span></div>
          <div class="rpm"><b id="rpm">--</b>RPM</div>
        </div>
        <div class="tiles" id="tiles"></div>
      </div>
      <div class="ecuoff hidden" id="ecuOffNote"><svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="9"/><path d="M5.6 5.6l12.8 12.8"/></svg>ECU ปิดอยู่: คำสั่ง OBD ทั้งหมดตอบ NO DATA (CYD จะขึ้น WAITING FOR ECU)</div>
    </section>

    <section class="card">
      <h2>สถานการณ์ <span class="side muted">แตะเพื่อเปลี่ยน</span></h2>
      <div class="scen" id="scen"></div>
    </section>
  </div>

  <div class="grid g3">
    <section class="card">
      <h2>ปรับค่าเอง (MANUAL) <button class="btn" id="toManual" type="button">ใช้ค่าปัจจุบัน</button></h2>
      <div class="note" id="manualNote">ตอนนี้กำลังเล่นสถานการณ์ <b id="manualScen">--</b> ขยับแถบใดก็ได้เพื่อสลับเป็นโหมดปรับค่าเอง</div>
      <div class="sliders" id="sliders"></div>
      <div class="presets" id="presets"></div>
    </section>

    <section class="card">
      <h2>รหัสข้อผิดพลาด (DTC) <span class="side muted" id="dtcCount">0 / 8</span></h2>
      <div class="mil" id="mil"><div class="milicon"><svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linejoin="round"><path d="M4 15h3l2-4h7l3 3v3H4z"/><path d="M8 11V8h6v3"/><path d="M2 13v4M22 12v4"/></svg></div><div><b id="milText">MIL OFF</b><span>ไฟเช็คเครื่องยนต์</span></div></div>
      <div class="codes" id="codes"></div>
      <div class="sub">รหัสที่ใช้บ่อย</div>
      <div class="pchips" id="pchips"></div>
      <div class="addrow"><input id="custom" maxlength="5" placeholder="เช่น P0442" autocomplete="off" spellcheck="false"><button class="btn primary" id="addCode" type="button">เพิ่ม</button><button class="btn" id="clearCodes" type="button">ล้าง</button></div>
    </section>

    <section class="card">
      <h2>อะแดปเตอร์ / ECU</h2>
      <div class="rows">
        <div class="row"><div><b>ECU ตอบ (ติดเครื่อง)</b><span>ปิด = ทุกคำสั่ง OBD ตอบ NO DATA</span></div><label class="sw"><input type="checkbox" id="ecu"><i></i></label></div>
        <div class="row"><div><b>รองรับ PID ระดับน้ำมัน</b><span>ปิด = 012F ตอบ NO DATA, CYD แสดง N/A (ตัดลิงก์ให้ CYD ถามใหม่)</span></div><label class="sw"><input type="checkbox" id="fuelPid"><i></i></label></div>
        <div class="row" style="display:block"><div style="margin-bottom:8px"><b>หน่วงเวลาตอบ</b><span>อะแดปเตอร์จริงตอบใน 20–80 ms, CYD หมดเวลาที่ 1,500 ms</span></div>
          <div class="lat"><span>ต่ำสุด</span><input type="range" id="latMin" min="0" max="1000" step="10"><output id="latMinV">--</output></div>
          <div class="lat"><span>สูงสุด</span><input type="range" id="latMax" min="0" max="1000" step="10"><output id="latMaxV">--</output></div>
        </div>
        <div class="row"><div><b>ตัดการเชื่อมต่อ Bluetooth</b><span>CYD จะเชื่อมต่อใหม่เองใน ~4 วินาที</span></div><button class="btn danger" id="drop" type="button">ตัดลิงก์</button></div>
      </div>
    </section>
  </div>

  <section class="card">
    <h2>คำสั่งจาก CYD และคำตอบ
      <span class="logbar">
        <label class="chk"><input type="checkbox" id="hidePoll" checked>ซ่อนการวนถามค่า (01xx / ATRV)</label>
        <label class="chk"><input type="checkbox" id="pause">หยุดเลื่อน</label>
        <button class="btn" id="clearLog" type="button">ล้างหน้าจอ</button>
      </span>
    </h2>
    <div class="log" id="log"></div>
  </section>

  <footer><span>ตัวจำลองตอบเหมือน ELM327 v1.5 · CAN 11 bit (A6) · ชื่อ Bluetooth <b id="btName">OBDII</b> PIN 1234</span><span id="uptime"></span></footer>
</div>
<div class="toast" id="toast"></div>

<script>
'use strict';
const $ = id => document.getElementById(id);
const SCEN = [
  ['idle','จอดติดเครื่อง','รอบเดินเบา 780, 0 km/h','<circle cx="12" cy="12" r="8"/><path d="M10 9v6M14 9v6"/>'],
  ['city','ขับในเมือง','จอด–เร่ง–วิ่ง–เบรก 30–60 km/h','<path d="M3 21V9l5-3v15M8 21V4l7 3v14M15 21v-9l6 2v7M2 21h20"/>'],
  ['launch','ออกตัว 0–100','คันเร่งสุด ถึง 100 ใน ~6.7 วินาที','<path d="M5 21V4M5 4h12l-2 4 2 4H5"/>'],
  ['redline','รอบสูง','รอบ 3,000–7,200 ทดสอบ shift light','<path d="M4 17a8 8 0 1 1 16 0"/><path d="M12 17l5-6"/>'],
  ['overheat','น้ำร้อนเกิน','น้ำขึ้นถึง 118°C (เตือนที่ 105)','<path d="M10 14V5a2 2 0 1 1 4 0v9a4 4 0 1 1-4 0z"/><path d="M12 9v7"/>'],
  ['lowbatt','แบตต่ำ','แบตเตอรี่ 11.9 V','<rect x="3" y="7" width="16" height="10" rx="2"/><path d="M21 11v2M7 11v2"/>'],
  ['lowfuel','น้ำมันใกล้หมด','ระดับน้ำมัน 7 %','<path d="M4 21V5a2 2 0 0 1 2-2h6a2 2 0 0 1 2 2v16M3 21h12M4 10h10M14 8l3 3v6a2 2 0 0 0 4 0V9l-3-3"/>'],
  ['nofuel','ไม่รองรับ PID น้ำมัน','012F ตอบ NO DATA → N/A','<circle cx="12" cy="12" r="8"/><path d="M6.5 6.5l11 11"/>'],
  ['manual','ปรับค่าเอง','ตั้งทุกค่าด้วยแถบเลื่อน','<path d="M4 6h10M18 6h2M4 12h4M12 12h8M4 18h12M20 18h0"/><circle cx="16" cy="6" r="2"/><circle cx="10" cy="12" r="2"/><circle cx="18" cy="18" r="2"/>'],
];
// key, ป้าย, หน่วย, min, max, step, ทศนิยม, ระดับสี
const FIELDS = [
  ['rpm','รอบเครื่อง','rpm',0,8000,50,0],
  ['speed','ความเร็ว','km/h',0,240,1,0],
  ['cool','น้ำหล่อเย็น','°C',40,125,1,0],
  ['thr','คันเร่ง','%',0,100,1,0],
  ['load','โหลด','%',0,100,1,0],
  ['iat','อากาศเข้า','°C',-10,80,1,0],
  ['fuel','น้ำมัน','%',0,100,1,0],
  ['volt','แบตเตอรี่','V',10,15.5,0.1,1],
];
const TILES = [['cool','COOL','°C',60,120],['volt','BATT','V',11,15],['fuel','FUEL','%',0,100],['thr','THR','%',0,100],['load','LOAD','%',0,100],['iat','IAT','°C',0,80]];
const PRESETS = [
  ['เดินเบา',{rpm:780,speed:0,thr:12,load:21,cool:90}],
  ['ทางด่วน 110',{rpm:2700,speed:110,thr:24,load:38,cool:92}],
  ['เร่งสุด 6,800',{rpm:6800,speed:95,thr:100,load:96}],
  ['น้ำร้อน 110°C',{cool:110}],
  ['แบต 11.8 V',{volt:11.8}],
  ['น้ำมัน 5 %',{fuel:5}],
];
const DTC = [['P0133','O2 ตอบสนองช้า'],['P0420','แคตาไลติก'],['P0171','ส่วนผสมบาง'],['P0300','จุดระเบิดหลายสูบ'],['P0301','สูบ 1 จุดระเบิด'],['P0118','เซ็นเซอร์น้ำสูง'],['P0562','แรงดันต่ำ'],['U0100','ขาดสื่อสาร ECM'],['P1234','เฉพาะผู้ผลิต']];

let S = null, demo = false, logNext = 0, lastCmds = null, lastCmdAt = 0, dragging = null;

// ---------------------------------------------------------------- API
async function api(path, body) {
  if (demo) return Demo.post(path, body);
  const opt = body ? {method:'POST', headers:{'X-SIM':'1','Content-Type':'application/x-www-form-urlencoded'}, body:new URLSearchParams(body)} : {};
  const r = await fetch(path, opt);
  const j = await r.json().catch(() => ({}));
  if (!r.ok) throw new Error(j.error || ('HTTP ' + r.status));
  return j;
}
function toast(msg, ok) {
  const t = $('toast'); t.textContent = msg; t.className = 'toast show' + (ok ? ' ok' : '');
  clearTimeout(toast.h); toast.h = setTimeout(() => t.className = 'toast', 2600);
}
async function act(path, body, okMsg) {
  try { await api(path, body); if (okMsg) toast(okMsg, true); poll(); }
  catch (e) { toast('ไม่สำเร็จ: ' + e.message); }
}

// ---------------------------------------------------------------- dial
const CX = 100, CY = 100, R = 82;
const pol = (a, r) => [CX + r * Math.cos(a), CY + r * Math.sin(a)];
function arc(a0, a1, r) {
  const [x0, y0] = pol(a0, r), [x1, y1] = pol(a1, r);
  return `M${x0.toFixed(1)} ${y0.toFixed(1)}A${r} ${r} 0 ${a1 - a0 > Math.PI ? 1 : 0} 1 ${x1.toFixed(1)} ${y1.toFixed(1)}`;
}
const A0 = Math.PI * 0.75, SWEEP = Math.PI * 1.5, ang = rpm => A0 + SWEEP * Math.max(0, Math.min(1, rpm / 8000));
function initDial() {
  $('arcTrack').setAttribute('d', arc(A0, A0 + SWEEP, R));
  $('arcRed').setAttribute('d', arc(ang(6500), A0 + SWEEP, R + 11));
  let t = '';
  for (let k = 0; k <= 8; k++) {
    const a = ang(k * 1000), [x0, y0] = pol(a, R - 18), [x1, y1] = pol(a, R - 11), [lx, ly] = pol(a, R - 28);
    const c = k >= 7 ? '#FF4D3D' : '#5C626A';
    t += `<line x1="${x0}" y1="${y0}" x2="${x1}" y2="${y1}" stroke="${c}" stroke-width="2"/>`;
    if (k % 2 === 0) t += `<text x="${lx}" y="${ly + 4}" fill="${c}" font-size="11" text-anchor="middle" font-family="inherit">${k}</text>`;
  }
  $('ticks').innerHTML = t;
}

// ---------------------------------------------------------------- build UI
function build() {
  $('scen').innerHTML = SCEN.map(([id, n, d, icon]) =>
    `<button class="sc" type="button" data-id="${id}"><svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">${icon}</svg><b>${n}</b><span>${d}</span></button>`).join('');
  $('scen').addEventListener('click', e => { const b = e.target.closest('.sc'); if (b) act('/api/scenario', {id:b.dataset.id}); });

  $('tiles').innerHTML = TILES.map(([k, l, u]) => `<div class="tile"><div class="l">${l}</div><div class="v"><span id="t_${k}">--</span><small>${u}</small></div><div class="bar"><i id="b_${k}"></i></div></div>`).join('');

  $('sliders').innerHTML = FIELDS.map(([k, l, u, mn, mx, st]) =>
    `<div class="sl"><label for="m_${k}">${l}</label><input type="range" id="m_${k}" data-k="${k}" min="${mn}" max="${mx}" step="${st}"><output id="o_${k}">--<small>${u}</small></output></div>`).join('');
  FIELDS.forEach(([k, , u, , , , dec]) => {
    const el = $('m_' + k);
    el.addEventListener('pointerdown', () => dragging = k);
    el.addEventListener('input', () => { showSlider(k, +el.value, dec, u); sendManual({[k]: el.value}); });
    el.addEventListener('change', () => { dragging = null; sendManual({[k]: el.value}, true); });
  });
  $('presets').innerHTML = PRESETS.map(([n], i) => `<button class="btn" type="button" data-i="${i}">${n}</button>`).join('');
  $('presets').addEventListener('click', e => { const b = e.target.closest('[data-i]'); if (b) act('/api/manual', PRESETS[b.dataset.i][1], 'ตั้งค่า: ' + PRESETS[b.dataset.i][0]); });
  $('toManual').addEventListener('click', () => act('/api/scenario', {id:'manual'}));

  $('pchips').innerHTML = DTC.map(([c, d]) => `<button class="pc" type="button" data-c="${c}"><b>${c}</b><span>${d}</span></button>`).join('');
  $('pchips').addEventListener('click', e => { const b = e.target.closest('[data-c]'); if (b) toggleCode(b.dataset.c); });
  $('codes').addEventListener('click', e => { const b = e.target.closest('[data-del]'); if (b) setCodes(S.faults.filter(c => c !== b.dataset.del)); });
  $('addCode').addEventListener('click', addCustom);
  $('custom').addEventListener('keydown', e => { if (e.key === 'Enter') addCustom(); });
  $('clearCodes').addEventListener('click', () => setCodes([]));

  $('ecu').addEventListener('change', e => act('/api/adapter', {ecu: e.target.checked ? 1 : 0}, e.target.checked ? 'ECU เปิด' : 'ECU ปิด: ตอบ NO DATA'));
  $('fuelPid').addEventListener('change', e => act('/api/adapter', {fuel: e.target.checked ? 1 : 0}));
  ['latMin', 'latMax'].forEach(id => {
    const el = $(id);
    el.addEventListener('pointerdown', () => dragging = id);
    el.addEventListener('input', () => { fill(el); $(id + 'V').textContent = el.value + ' ms'; });
    el.addEventListener('change', () => { dragging = null; act('/api/adapter', {latmin: $('latMin').value, latmax: $('latMax').value}); });
  });
  $('drop').addEventListener('click', () => act('/api/drop', {}, 'ตัดลิงก์แล้ว CYD จะเชื่อมต่อใหม่'));
  $('clearLog').addEventListener('click', () => $('log').innerHTML = '');
  $('hidePoll').addEventListener('change', () => document.querySelectorAll('#log .poll').forEach(d => d.classList.toggle('hidden', $('hidePoll').checked)));
}

function fill(el) { el.style.setProperty('--p', ((el.value - el.min) / (el.max - el.min) * 100) + '%'); }
function fmt(v, dec) { return v == null ? '--' : (+v).toLocaleString('en-US', {minimumFractionDigits: dec, maximumFractionDigits: dec}); }
function showSlider(k, v, dec, u) { $('o_' + k).innerHTML = `${fmt(v, dec)}<small>${u}</small>`; fill($('m_' + k)); }

let manualTimer = null, manualPending = {};
function sendManual(part, now) {
  Object.assign(manualPending, part);
  clearTimeout(manualTimer);
  manualTimer = setTimeout(() => { const b = manualPending; manualPending = {}; act('/api/manual', b); }, now ? 0 : 120);
}

function toggleCode(c) { setCodes(S.faults.includes(c) ? S.faults.filter(x => x !== c) : [...S.faults, c]); }
function setCodes(list) {
  if (list.length > 8) { toast('ใส่ได้สูงสุด 8 รหัส'); return; }
  act('/api/faults', {codes: list.join(',')});
}
function addCustom() {
  const v = $('custom').value.trim().toUpperCase();
  if (!/^[PCBU][0-3][0-9A-F]{3}$/.test(v)) { toast('รูปแบบรหัส เช่น P0442, C0035, B1000, U0100'); return; }
  if (!S.faults.includes(v)) setCodes([...S.faults, v]);
  $('custom').value = '';
}

// ---------------------------------------------------------------- render
function level(k, v) {
  if (k === 'cool') return v >= 105 ? 'bad' : v >= 97 ? 'accent' : 'ok';
  if (k === 'volt') return v < 12.4 ? 'bad' : v < 12.8 ? 'accent' : 'ok';
  if (k === 'fuel') return v < 10 ? 'bad' : v < 20 ? 'accent' : 'ok';
  return null;
}
function render() {
  const s = S, c = s.car;
  // header
  $('btDot').className = 'dot ' + (!s.bt.ready ? 'bad' : s.bt.connected ? 'ok' : 'wait');
  $('btText').textContent = !s.bt.ready ? 'เปิดไม่ได้' : s.bt.connected ? 'CYD เชื่อมต่อแล้ว' + (s.bt.peer ? ' · ' + s.bt.peer : '') : 'รอ CYD เชื่อมต่อ';
  $('netMode').textContent = s.net.ap ? 'AP ' + s.net.ssid : 'WiFi';
  $('netIp').textContent = s.net.ip;
  $('btName').textContent = s.bt.name + (s.bt.mac ? ' (' + s.bt.mac + ')' : '');
  const now = Date.now();
  if (lastCmds != null && now > lastCmdAt + 500) $('cmdRate').textContent = Math.round((s.cmds - lastCmds) * 1000 / (now - lastCmdAt)) + ' /s · ' + s.cmds.toLocaleString('en-US');
  lastCmds = s.cmds; lastCmdAt = now;
  const up = s.uptime; $('uptime').textContent = 'เปิดมา ' + Math.floor(up / 3600) + ' ชม. ' + Math.floor(up / 60) % 60 + ' นาที';

  // live
  const red = c.rpm >= 6500;
  $('arcVal').setAttribute('d', c.rpm > 30 ? arc(A0, ang(c.rpm), R) : '');
  $('arcVal').setAttribute('stroke', red ? '#FF4D3D' : '#FFB000');
  $('rpm').textContent = fmt(Math.round(c.rpm / 10) * 10, 0);
  $('rpm').style.color = red ? 'var(--bad)' : '';
  $('speed').textContent = Math.round(c.speed);
  $('gear').textContent = c.gear ? c.gear : 'N';
  TILES.forEach(([k, , , lo, hi]) => {
    const v = c[k], lv = level(k, v);
    const na = k === 'fuel' && !s.fuelPid;
    $('t_' + k).textContent = na ? 'N/A' : fmt(v, k === 'volt' ? 1 : 0);
    $('t_' + k).style.color = !na && lv && lv !== 'ok' ? `var(--${lv})` : '';
    const b = $('b_' + k);
    b.style.width = na ? '0' : Math.max(3, Math.min(100, (v - lo) / (hi - lo) * 100)) + '%';
    b.style.background = lv ? `var(--${lv})` : 'var(--text)';
  });
  $('ecuOffNote').classList.toggle('hidden', s.ecu);
  const sc = SCEN.find(x => x[0] === s.scenario);
  $('liveScen').textContent = sc ? sc[1] : s.scenario;

  // scenarios
  document.querySelectorAll('.sc').forEach(b => b.classList.toggle('on', b.dataset.id === s.scenario));

  // manual
  const manual = s.scenario === 'manual';
  $('manualNote').classList.toggle('hidden', manual);
  $('manualScen').textContent = sc ? sc[1] : '';
  $('toManual').classList.toggle('hidden', manual);
  FIELDS.forEach(([k, , u, , , , dec]) => {
    if (dragging === k) return;
    const v = manual ? s.manual[k] : c[k === 'cool' ? 'cool' : k];
    $('m_' + k).value = v;
    showSlider(k, v, dec, u);
  });

  // faults
  const f = s.faults;
  $('dtcCount').textContent = f.length + ' / 8';
  $('mil').classList.toggle('on', f.length > 0);
  $('milText').textContent = f.length ? 'MIL ON · ' + f.length + ' รหัส' : 'MIL OFF';
  $('codes').innerHTML = f.length ? f.map(x => `<span class="code">${x}<button type="button" data-del="${x}" aria-label="ลบ ${x}">×</button></span>`).join('') : '<span class="empty">ไม่มีรหัสค้าง: CYD กด READ จะขึ้น No fault codes</span>';
  document.querySelectorAll('.pc').forEach(b => b.classList.toggle('on', f.includes(b.dataset.c)));

  // adapter
  $('ecu').checked = s.ecu;
  $('fuelPid').checked = s.fuelPid;
  if (dragging !== 'latMin') { $('latMin').value = s.latMin; $('latMinV').textContent = s.latMin + ' ms'; fill($('latMin')); }
  if (dragging !== 'latMax') { $('latMax').value = s.latMax; $('latMaxV').textContent = s.latMax + ' ms'; fill($('latMax')); }
  $('drop').disabled = !s.bt.connected;
}

// ---------------------------------------------------------------- log
function addLog(lines) {
  const box = $('log'), stick = !$('pause').checked;
  const hide = $('hidePoll').checked;
  const frag = document.createDocumentFragment();
  for (const raw of lines) {
    const sp = raw.indexOf(' '), t = +raw.slice(0, sp) / 10, msg = raw.slice(sp + 1);
    const d = document.createElement('div');
    const tag = msg.slice(0, 5);
    d.className = tag === '[RX] ' ? 'rx' : tag === '[SIM]' ? 'sim' : tag === '[BT] ' ? 'bt' : tag === '[NET]' ? 'net' : '';
    if (/FAIL|error|too long/i.test(msg)) d.className += ' err';
    const isPoll = /^\[RX\] (01[0-9A-F]{2}|ATRV)\b/.test(msg) && !/^\[RX\] 01(00|20)\b/.test(msg);
    if (isPoll) { d.classList.add('poll'); if (hide) d.classList.add('hidden'); }
    const ts = `${String(Math.floor(t / 60) % 60).padStart(2, '0')}:${(t % 60).toFixed(1).padStart(4, '0')}`;
    if (d.classList.contains('rx')) {
      const m = msg.match(/^\[RX\] (\S+)\s+(\+\d+ms)\s+(.*)$/);
      d.innerHTML = m ? `<span class="t">${ts}</span><span class="k">${esc(m[1])}</span>  <span class="muted">${m[2]}</span>  ${esc(m[3])}` : `<span class="t">${ts}</span>${esc(msg)}`;
    } else {
      d.innerHTML = `<span class="t">${ts}</span>${esc(msg)}`;
    }
    frag.appendChild(d);
  }
  box.appendChild(frag);
  while (box.childElementCount > 600) box.firstElementChild.remove();
  if (stick) box.scrollTop = box.scrollHeight;
}
const esc = s => s.replace(/[&<>]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;'}[c]));

// ---------------------------------------------------------------- poll loop
// WiFi ใช้วิทยุร่วมกับ Bluetooth ของตัวจำลอง: ถามไม่ถี่ และหยุดถามเมื่อซ่อนแท็บ/ปิดจอ
async function poll() {
  if (document.hidden && S) return;
  try { S = demo ? Demo.state() : await api('/api/state'); render(); }
  catch (e) { if (!demo && !S) startDemo(); }
}
async function pollLog() {
  if (document.hidden) return;
  try {
    const j = demo ? Demo.log(logNext) : await api('/api/log?since=' + logNext + ($('hidePoll').checked ? '&quiet=1' : ''));
    if (j.next < logNext) { logNext = 0; return; }
    logNext = j.next;
    if (j.lines.length) addLog(j.lines);
  } catch (e) {}
}

// ---------------------------------------------------------------- demo mode (เปิดหน้าเว็บโดยไม่มีตัวจำลอง)
function startDemo() {
  demo = true;
  $('demo').classList.remove('hidden');
  toast('ไม่พบตัวจำลอง: แสดงโหมด DEMO', true);
}
const Demo = (() => {
  const st = {scenario:'city', bt:{ready:true, connected:true, peer:'A4:F0:0F:5B:B9:02', name:'OBDII', mac:'C0:CD:D6:CA:1E:6E'}, net:{ap:false, ip:'192.168.1.24', ssid:'home'}, uptime:0, cmds:0,
    car:{rpm:780, speed:0, cool:88, thr:12, load:21, iat:34, fuel:62, volt:14.1, gear:0}, manual:{rpm:780, speed:0, cool:90, thr:12, load:21, iat:34, fuel:62, volt:14.1},
    faults:['P0133','P0420'], ecu:true, fuelPid:true, latMin:20, latMax:50};
  const lines = []; let seq = 0, t0 = Date.now(), tick = 0;
  const log = m => { lines.push(Math.round((Date.now() - t0) / 100) + ' ' + m); seq++; if (lines.length > 80) lines.shift(); };
  const POLL = ['010C','010D','010C','0111','010C','0104','010C','0105','010C','010F','010C','012F','010C','ATRV'];
  log('[BT] slave "OBDII" PIN 1234, MAC C0:CD:D6:CA:1E:6E'); log('[NET] WiFi connected -> http://192.168.1.24  (http://obd-sim.local)'); log('[BT] client connected (LED on)');
  setInterval(() => {
    tick++; st.uptime = Math.round((Date.now() - t0) / 1000);
    const c = st.car, s = st.scenario, t = tick / 5;
    if (s === 'manual') Object.assign(c, st.manual, {gear: 0});
    else {
      let sp = 0, thr = 12;
      if (s === 'city') { sp = Math.max(0, 45 + 25 * Math.sin(t / 4)); thr = 15 + 20 * Math.max(0, Math.cos(t / 4)); }
      if (s === 'launch') { const k = t % 14; sp = k < 4 ? 0 : Math.min(105, (k - 4) * 15); thr = k < 4 ? 12 : 100; }
      if (s === 'redline') { const x = Math.abs((t % 8) - 4) / 4; c.rpm = 7200 - x * 4200; sp = c.rpm / 50; thr = 100; }
      c.speed = sp; c.thr = thr; c.load = 20 + thr * 0.7;
      if (s !== 'redline') { c.gear = sp < 1 ? 0 : Math.min(6, 1 + Math.floor(sp / 22)); c.rpm = sp < 1 ? 780 + Math.random() * 20 : 1200 + (sp % 22) * 180; }
      c.cool += ((s === 'overheat' ? 118 : 90) - c.cool) * 0.03;
      c.volt = s === 'lowbatt' ? 11.9 : 14.1; c.fuel = s === 'lowfuel' ? 7 : 62; c.iat = 34;
    }
    if (st.bt.connected && st.ecu) {
      const n = 4 + Math.floor(Math.random() * 3);
      for (let i = 0; i < n; i++) {
        const cmd = POLL[(st.cmds + i) % POLL.length];
        const rep = cmd === 'ATRV' ? c.volt.toFixed(1) + 'V' : cmd === '012F' && !st.fuelPid ? 'NO DATA' : '41' + cmd.slice(2) + '2E';
        log(`[RX] ${cmd.padEnd(8)} +${20 + Math.floor(Math.random() * 30)}ms  ${rep}`);
      }
      st.cmds += n;
    }
  }, 200);
  const NAMES = {idle:'IDLE', city:'CITY', launch:'LAUNCH', redline:'REDLINE', overheat:'OVERHEAT', lowbatt:'LOW BATT', lowfuel:'LOW FUEL', nofuel:'NO FUEL PID', manual:'MANUAL'};
  return {
    state: () => JSON.parse(JSON.stringify(st)),
    log: since => ({next: seq, lines: lines.slice(Math.max(0, lines.length - (seq - since)))}),
    post: async (path, b) => {
      if (path === '/api/scenario') { if (b.id === 'manual' && st.scenario !== 'manual') Object.assign(st.manual, st.car); st.scenario = b.id; st.fuelPid = b.id !== 'nofuel'; log('[SIM] scenario -> ' + NAMES[b.id]); }
      if (path === '/api/manual') { if (st.scenario !== 'manual') { Object.assign(st.manual, st.car); st.scenario = 'manual'; } for (const k in b) st.manual[k] = +b[k]; }
      if (path === '/api/faults') { st.faults = b.codes ? b.codes.split(',') : []; log('[SIM] fault codes: ' + (st.faults.join(' ') || 'none')); }
      if (path === '/api/adapter') { if ('ecu' in b) st.ecu = b.ecu == 1; if ('fuel' in b) st.fuelPid = b.fuel == 1; if ('latmin' in b) { st.latMin = +b.latmin; st.latMax = Math.max(+b.latmin, +b.latmax); } }
      if (path === '/api/drop') { st.bt.connected = false; log('[BT] client disconnected'); setTimeout(() => { st.bt.connected = true; log('[BT] client connected (LED on)'); }, 4000); }
      return {ok: true};
    },
  };
})();

initDial();
build();
poll();
setInterval(poll, 1000);
setInterval(pollLog, 1500);
document.addEventListener('visibilitychange', () => { if (!document.hidden) { poll(); pollLog(); } });
</script>
</body>
</html>
)SIMHTML";
