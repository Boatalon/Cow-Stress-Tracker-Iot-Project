'use client';

import React, { useEffect, useState } from 'react';

export interface DeviceStatus {
  did: number;
  name: string;
  last_seen: string | null;
  last_act: number;
  last_ang_x: number;
  last_ang_y: number;
  last_ang_z: number;
  last_gx: number;
  last_gy: number;
  last_gz: number;
  last_tilt: number;
  last_stand: number;
  last_sleep: number;
  last_temp: number;
  last_rssi: number;
  fw_version: string;
  last_otp_valid: boolean;
  total_packets: number;
  send_sec: number;
  target_ota_ver: string;
  ref_gx: number;
  ref_gy: number;
  ref_gz: number;
  step_trig: number;
  step_rst: number;
  lie_ang: number;
  stand_ang: number;
  cal_seq: number;
  continuous_mode: boolean;
}

export interface TelemetryLog {
  id: number;
  did: number;
  act: number;
  ang_x: number;
  ang_y: number;
  ang_z: number;
  stand: number;
  sleep: number;
  temp: number;
  device_time: number;
  server_time: number;
  time_drift_sec: number;
  otp_received: number;
  otp_expected: number;
  otp_valid: boolean;
  fw_version: string;
  rssi: number;
  ip_address: string;
  ku_forwarded: boolean;
  ku_status: number;
  raw_query: string;
  created_at: string;
}

export interface FirmwareRelease {
  id: number;
  version: string;
  filename: string;
  file_url: string;
  md5_hash: string;
  file_size: number;
  notes: string;
  is_active: boolean;
  target_devices: string;
  created_at: string;
}

export function ManualCalibrationSection({
  devices,
  onRefresh,
  showToast,
}: {
  devices: DeviceStatus[];
  onRefresh: () => Promise<void>;
  showToast: (msg: string) => void;
}) {
  const [selectedDid, setSelectedDid] = useState<number>(6);
  const currentDev = devices.find((d) => d.did === selectedDid) || devices[5] || devices[0];

  const [stepTrig, setStepTrig] = useState<number>(0.25);
  const [stepRst, setStepRst] = useState<number>(0.12);
  const [lieAng, setLieAng] = useState<number>(45.0);
  const [standAng, setStandAng] = useState<number>(30.0);
  const [busy, setBusy] = useState<boolean>(false);

  useEffect(() => {
    if (currentDev) {
      setStepTrig(currentDev.step_trig ?? 0.25);
      setStepRst(currentDev.step_rst ?? 0.12);
      setLieAng(currentDev.lie_ang ?? 45.0);
      setStandAng(currentDev.stand_ang ?? 30.0);
    }
  }, [
    selectedDid,
    currentDev?.step_trig,
    currentDev?.step_rst,
    currentDev?.lie_ang,
    currentDev?.stand_ang,
  ]);

  if (!currentDev) return null;

  // คำนวณมุม Tilt สดบนหน้าเว็บเทียบกับ ref_gx, ref_gy, ref_gz
  const calcLiveTilt = (): number => {
    const gx = currentDev.last_gx ?? 0;
    const gy = currentDev.last_gy ?? 0;
    const gz = currentDev.last_gz ?? 1;
    const rx = currentDev.ref_gx ?? 0;
    const ry = currentDev.ref_gy ?? 0;
    const rz = currentDev.ref_gz ?? 1;
    const n1 = Math.sqrt(gx * gx + gy * gy + gz * gz);
    const n2 = Math.sqrt(rx * rx + ry * ry + rz * rz);
    if (n1 < 0.01 || n2 < 0.01) return currentDev.last_tilt || 0;
    const dot = Math.max(-1, Math.min(1, (gx * rx + gy * ry + gz * rz) / (n1 * n2)));
    return Number(((Math.acos(dot) * 180) / Math.PI).toFixed(1));
  };

  const liveTilt = calcLiveTilt();

  const handleCalibrateAction = async (mode: 'use_current' | 'board_sample' | 'params_only', toggleContinuous?: boolean) => {
    setBusy(true);
    try {
      const res = await fetch('/api/dashboard', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          action: 'calibrate_device',
          did: selectedDid,
          mode,
          step_trig: stepTrig,
          step_rst: stepRst,
          lie_ang: lieAng,
          stand_ang: standAng,
          continuous_mode: toggleContinuous !== undefined ? toggleContinuous : currentDev.continuous_mode,
        }),
      });
      const data = await res.json();
      if (data.ok) {
        if (mode === 'use_current') {
          showToast(`🎯 ตั้งท่าปัจจุบันของ DID #${selectedDid} เป็นท่ายืนอ้างอิง (0.0°) เรียบร้อย!`);
        } else if (mode === 'board_sample') {
          showToast(`📡 ส่งคำสั่งให้บอร์ด DID #${selectedDid} วัดท่ายืนใหม่ 20 ตัวอย่างในรอบถัดไปแล้ว!`);
        } else {
          showToast(`💾 บันทึกค่าพารามิเตอร์และโหมดของ DID #${selectedDid} เรียบร้อย!`);
        }
        await onRefresh();
      }
    } finally {
      setBusy(false);
    }
  };

  return (
    <div className="space-y-6">
      {/* แถบเลือกเครื่องและสลับโหมด ส่งตลอดเวลา vs ประหยัดพลังงาน */}
      <div className="bg-slate-900 border border-slate-800 rounded-2xl p-5 flex flex-col lg:flex-row lg:items-center justify-between gap-4">
        <div className="space-y-1">
          <div className="flex items-center gap-3 flex-wrap">
            <h2 className="text-base sm:text-lg font-bold text-white">
              🎯 โหมด Manual Calibrate & จูนค่าเซ็นเซอร์ผ่านหน้าเว็บ
            </h2>
            <select
              value={selectedDid}
              onChange={(e) => setSelectedDid(Number(e.target.value))}
              className="bg-slate-950 border border-emerald-500/50 rounded-xl px-3 py-1.5 text-sm font-bold text-emerald-300"
            >
              {devices.map((d) => (
                <option key={d.did} value={d.did}>
                  DID #{d.did} — {d.name} {d.total_packets > 0 ? `(ส่งแล้ว ${d.total_packets} ครั้ง)` : ''}
                </option>
              ))}
            </select>
          </div>
          <p className="text-xs text-slate-400">
            ไม่ต้องใช้ Auto-Calibrate ตอนเปิดเครื่อง — คุณสามารถกดตั้งค่าท่ายืน (0°) ด้วยตัวเองและสลับโหมดส่งข้อมูลสดได้จากหน้านี้ทันที
          </p>
        </div>

        <div className="flex flex-wrap items-center gap-3">
          <div
            className={`px-3.5 py-2 rounded-xl border text-xs font-semibold flex items-center gap-2 ${
              currentDev.continuous_mode
                ? 'bg-amber-500/20 border-amber-400/50 text-amber-200'
                : 'bg-emerald-500/15 border-emerald-500/40 text-emerald-300'
            }`}
          >
            <span>{currentDev.continuous_mode ? '⚡ โหมดส่งตลอดเวลา (ทุก 5 วินาที - ไม่หลับ)' : '🔋 โหมดประหยัดพลังงาน (Deep Sleep)'}</span>
          </div>
          <button
            disabled={busy}
            onClick={() => handleCalibrateAction('params_only', !currentDev.continuous_mode)}
            className={`px-4 py-2 rounded-xl text-xs font-bold transition cursor-pointer disabled:opacity-50 ${
              currentDev.continuous_mode
                ? 'bg-emerald-600 hover:bg-emerald-500 text-white'
                : 'bg-amber-500 hover:bg-amber-400 text-slate-950'
            }`}
          >
            {currentDev.continuous_mode
              ? '🔋 สลับกลับเป็นโหมดประหยัดไฟ (Deep Sleep)'
              : '⚡ เปิดโหมดส่งตลอดเวลา (เพื่อจูนค่าสด)'}
          </button>
        </div>
      </div>

      {/* การ์ดแสดงค่ามุมปัจจุบัน + ปุ่มกด Manual Calibrate */}
      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        <div className="bg-slate-900 border border-slate-800 rounded-2xl p-5 space-y-5">
          <div>
            <h3 className="text-base font-bold text-white">1️⃣ กดตั้งค่าท่ายืนอ้างอิง (Manual Standing Calibration)</h3>
            <p className="text-xs text-slate-400 mt-1">
              ติดบอร์ดที่ขาวัวในท่ายืน (หรือวางบอร์ดในแนวท่ายืนที่ต้องการ) รอให้บอร์ดส่งค่ามุมล่าสุดเข้ามา แล้วกดปุ่มด้านล่างเพื่อล็อกท่านั้นเป็น <strong>0.0° (ท่ายืน)</strong> ถาวร
            </p>
          </div>

          <div className="grid grid-cols-2 sm:grid-cols-3 gap-3 text-center">
            <div className="bg-slate-950 border border-slate-800 rounded-xl p-3">
              <div className="text-[11px] text-slate-400">มุมเอียงขาปัจจุบัน (Tilt)</div>
              <div
                className={`text-2xl font-bold mt-1 ${
                  liveTilt >= lieAng ? 'text-purple-400' : liveTilt <= standAng ? 'text-emerald-400' : 'text-amber-300'
                }`}
              >
                {liveTilt.toFixed(1)}°
              </div>
              <div className="text-[11px] text-slate-500 mt-0.5">
                {liveTilt >= lieAng ? '🟣 เกณฑ์ท่านอน' : '🟢 เกณฑ์ท่ายืน'}
              </div>
            </div>

            <div className="bg-slate-950 border border-slate-800 rounded-xl p-3">
              <div className="text-[11px] text-slate-400">มุมบอร์ดล่าสุด (ANG X/Y/Z)</div>
              <div className="text-xs font-mono font-bold text-slate-200 mt-2">
                X: {currentDev.last_ang_x.toFixed(1)}°
                <br />
                Y: {currentDev.last_ang_y.toFixed(1)}°
                <br />
                Z: {currentDev.last_ang_z.toFixed(1)}°
              </div>
            </div>

            <div className="bg-slate-950 border border-slate-800 rounded-xl p-3 col-span-2 sm:col-span-1">
              <div className="text-[11px] text-slate-400">แรงโน้มถ่วงล่าสุด vs ค่าอ้างอิง</div>
              <div className="text-[11px] font-mono text-cyan-300 mt-1.5">
                ปัจจุบัน: ({currentDev.last_gx.toFixed(2)}, {currentDev.last_gy.toFixed(2)}, {currentDev.last_gz.toFixed(2)})
              </div>
              <div className="text-[11px] font-mono text-emerald-300 mt-1">
                อ้างอิงยืน: ({currentDev.ref_gx.toFixed(2)}, {currentDev.ref_gy.toFixed(2)}, {currentDev.ref_gz.toFixed(2)})
              </div>
            </div>
          </div>

          <div className="space-y-2.5 pt-1">
            <button
              disabled={busy}
              onClick={() => handleCalibrateAction('use_current')}
              className="w-full py-3 px-4 rounded-xl bg-emerald-600 hover:bg-emerald-500 text-white font-bold text-sm transition cursor-pointer disabled:opacity-50 shadow-lg shadow-emerald-950/50"
            >
              🎯 ตั้งท่าปัจจุบันของบอร์ด ({currentDev.last_ang_x.toFixed(1)}°, {currentDev.last_ang_y.toFixed(1)}°) เป็น &quot;ท่ายืน 0.0°&quot; ทันที
            </button>

            <button
              disabled={busy}
              onClick={() => handleCalibrateAction('board_sample')}
              className="w-full py-2.5 px-4 rounded-xl bg-slate-800 hover:bg-slate-700 text-slate-200 border border-slate-700 font-semibold text-xs transition cursor-pointer disabled:opacity-50"
            >
              📡 สั่งให้บอร์ดวัดค่าเฉลี่ย 20 ตัวอย่างสดๆ ตอนตื่นรอบหน้าแล้วบันทึกเป็นท่ายืน (Calibrate Seq #{currentDev.cal_seq + 1})
            </button>
          </div>
        </div>

        {/* การ์ดปรับจูนความไวการนับก้าวและมุมยืน/นอน */}
        <div className="bg-slate-900 border border-slate-800 rounded-2xl p-5 space-y-4">
          <div>
            <h3 className="text-base font-bold text-white">2️⃣ ปรับจูนความไวก้าวเดิน & มุมตัดสิน ยืน/นอน</h3>
            <p className="text-xs text-slate-400 mt-1">
              ปรับค่าแล้วกดบันทึก บอร์ดจะรับค่าไปเซฟลง Flash ถาวรทันทีที่เชื่อมต่อรอบถัดไป
            </p>
          </div>

          <div className="grid grid-cols-1 sm:grid-cols-2 gap-4 text-xs">
            <div className="bg-slate-950 border border-slate-800 rounded-xl p-3.5 space-y-1.5">
              <div className="flex justify-between">
                <label className="text-slate-300 font-medium">แรงกระแทกเริ่มนับก้าว (Trigger)</label>
                <span className="font-mono font-bold text-emerald-400">{stepTrig.toFixed(2)} g</span>
              </div>
              <input
                type="range"
                min={0.1}
                max={1.0}
                step={0.02}
                value={stepTrig}
                onChange={(e) => setStepTrig(Number(e.target.value))}
                className="w-full accent-emerald-500"
              />
              <div className="text-[11px] text-slate-500">ลดลง = นับก้าวง่ายขึ้น (แนะนำ 0.22 – 0.30 g)</div>
            </div>

            <div className="bg-slate-950 border border-slate-800 rounded-xl p-3.5 space-y-1.5">
              <div className="flex justify-between">
                <label className="text-slate-300 font-medium">จุดรีเซ็ตพร้อมนับก้าวต่อไป (Reset)</label>
                <span className="font-mono font-bold text-cyan-400">{stepRst.toFixed(2)} g</span>
              </div>
              <input
                type="range"
                min={0.05}
                max={0.6}
                step={0.02}
                value={stepRst}
                onChange={(e) => setStepRst(Number(e.target.value))}
                className="w-full accent-cyan-500"
              />
              <div className="text-[11px] text-slate-500">ควรต่ำกว่าค่า Trigger (แนะนำ 0.10 – 0.15 g)</div>
            </div>

            <div className="bg-slate-950 border border-slate-800 rounded-xl p-3.5 space-y-1.5">
              <div className="flex justify-between">
                <label className="text-slate-300 font-medium">มุมขาที่ถือว่า &quot;นอน&quot; (Lie Angle)</label>
                <span className="font-mono font-bold text-purple-400">{lieAng.toFixed(0)}°</span>
              </div>
              <input
                type="range"
                min={25}
                max={80}
                step={1}
                value={lieAng}
                onChange={(e) => setLieAng(Number(e.target.value))}
                className="w-full accent-purple-500"
              />
              <div className="text-[11px] text-slate-500">ถ้า Tilt ≥ ค่านี้ จะหยุดนับก้าวและเข้าโหมดนอน (แนะนำ 45°)</div>
            </div>

            <div className="bg-slate-950 border border-slate-800 rounded-xl p-3.5 space-y-1.5">
              <div className="flex justify-between">
                <label className="text-slate-300 font-medium">มุมขาที่ถือว่า &quot;ลุกยืน&quot; (Stand Angle)</label>
                <span className="font-mono font-bold text-emerald-400">{standAng.toFixed(0)}°</span>
              </div>
              <input
                type="range"
                min={10}
                max={60}
                step={1}
                value={standAng}
                onChange={(e) => setStandAng(Number(e.target.value))}
                className="w-full accent-emerald-500"
              />
              <div className="text-[11px] text-slate-500">ถ้า Tilt ≤ ค่านี้ จะกลับมาเปิดรับค่าเซ็นเซอร์และนับก้าว (แนะนำ 30°)</div>
            </div>
          </div>

          <button
            disabled={busy}
            onClick={() => handleCalibrateAction('params_only')}
            className="w-full py-2.5 px-4 rounded-xl bg-cyan-600 hover:bg-cyan-500 text-white font-bold text-sm transition cursor-pointer disabled:opacity-50"
          >
            💾 บันทึกค่าความไวก้าวเดินและมุม ยืน/นอน ลงบอร์ด DID #{selectedDid}
          </button>
        </div>
      </div>
    </div>
  );
}

export function OtaSection({
  firmwares,
  otaVersion,
  setOtaVersion,
  otaTarget,
  setOtaTarget,
  otaNotes,
  setOtaNotes,
  setOtaFile,
  otaUploading,
  onUpload,
  onDelete,
}: {
  firmwares: FirmwareRelease[];
  otaVersion: string;
  setOtaVersion: (v: string) => void;
  otaTarget: string;
  setOtaTarget: (v: string) => void;
  otaNotes: string;
  setOtaNotes: (v: string) => void;
  setOtaFile: (f: File | null) => void;
  otaUploading: boolean;
  onUpload: (e: React.FormEvent) => void;
  onDelete: (version: string) => void;
}) {
  return (
    <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
      <form onSubmit={onUpload} className="bg-slate-900 border border-slate-800 rounded-2xl p-5 space-y-4">
        <div>
          <h2 className="text-base font-bold text-white">📤 อัปโหลดเฟิร์มแวร์ประหยัดไฟ (.bin)</h2>
          <p className="text-xs text-slate-400 mt-1">
            ใน Arduino IDE กดเมนู <strong>Sketch → Export Compiled Binary</strong> แล้วนำไฟล์ .bin มาอัปโหลด
          </p>
        </div>
        <div>
          <label className="text-xs text-slate-300 block mb-1">เลขเวอร์ชันใหม่ (เช่น 2.2.0)</label>
          <input
            type="text"
            required
            value={otaVersion}
            onChange={(e) => setOtaVersion(e.target.value)}
            className="w-full bg-slate-950 border border-slate-700 rounded-xl px-3 py-2 text-sm text-white"
          />
        </div>
        <div>
          <label className="text-xs text-slate-300 block mb-1">เครื่องเป้าหมาย (all หรือเลขเครื่อง เช่น 6,13)</label>
          <input
            type="text"
            required
            value={otaTarget}
            onChange={(e) => setOtaTarget(e.target.value)}
            className="w-full bg-slate-950 border border-slate-700 rounded-xl px-3 py-2 text-sm text-white"
          />
        </div>
        <div>
          <label className="text-xs text-slate-300 block mb-1">ไฟล์เฟิร์มแวร์ (.bin)</label>
          <input
            type="file"
            accept=".bin,application/octet-stream"
            onChange={(e) => setOtaFile(e.target.files?.[0] || null)}
            className="w-full text-xs text-slate-300 file:mr-3 file:py-2 file:px-3 file:rounded-lg file:border-0 file:text-xs file:font-semibold file:bg-emerald-600 file:text-white"
          />
        </div>
        <div>
          <label className="text-xs text-slate-300 block mb-1">รายละเอียด</label>
          <textarea
            rows={2}
            value={otaNotes}
            onChange={(e) => setOtaNotes(e.target.value)}
            className="w-full bg-slate-950 border border-slate-700 rounded-xl px-3 py-2 text-xs text-white"
          />
        </div>
        <button
          type="submit"
          disabled={otaUploading}
          className="w-full py-2.5 rounded-xl bg-emerald-600 hover:bg-emerald-500 text-white font-bold text-sm transition cursor-pointer disabled:opacity-50"
        >
          {otaUploading ? 'กำลังอัปโหลด...' : '🚀 อัปโหลดและสั่งอัปเดต OTA'}
        </button>
      </form>

      <div className="lg:col-span-2 bg-slate-900 border border-slate-800 rounded-2xl p-5 space-y-4">
        <h2 className="text-base font-bold text-white">📦 รายการเฟิร์มแวร์ OTA ในระบบ</h2>
        {firmwares.length === 0 ? (
          <div className="border border-dashed border-slate-800 rounded-xl p-8 text-center text-sm text-slate-500">
            ยังไม่มีไฟล์เฟิร์มแวร์ในระบบ อัปโหลดไฟล์ .bin ด้านซ้ายเพื่อเริ่มใช้งาน OTA
          </div>
        ) : (
          <div className="space-y-3">
            {firmwares.map((fw) => (
              <div
                key={fw.version}
                className="p-4 rounded-xl bg-slate-950/80 border border-slate-800 flex flex-col sm:flex-row sm:items-center justify-between gap-3"
              >
                <div className="space-y-1">
                  <div className="flex items-center gap-2">
                    <span className="px-2.5 py-0.5 rounded-lg bg-emerald-500/20 text-emerald-300 font-bold text-sm">
                      v{fw.version}
                    </span>
                    <span className="text-xs text-slate-300 font-mono">{fw.filename}</span>
                    <span className="text-xs text-slate-500">({(fw.file_size / 1024).toFixed(1)} KB)</span>
                  </div>
                  <div className="text-xs text-slate-400">{fw.notes || '-'}</div>
                  <div className="text-[11px] font-mono text-slate-500">
                    MD5: <span className="text-slate-300">{fw.md5_hash}</span> | เป้าหมาย:{' '}
                    <span className="text-cyan-300">{fw.target_devices}</span>
                  </div>
                </div>
                <div className="flex items-center gap-2 shrink-0">
                  <a
                    href={fw.file_url}
                    target="_blank"
                    rel="noreferrer"
                    className="px-3 py-1.5 rounded-lg bg-slate-800 hover:bg-slate-700 text-xs text-slate-200 border border-slate-700"
                  >
                    ดาวน์โหลด .bin
                  </a>
                  <button
                    onClick={() => onDelete(fw.version)}
                    className="px-3 py-1.5 rounded-lg bg-rose-950/70 hover:bg-rose-900 text-xs text-rose-300 border border-rose-800/50 cursor-pointer"
                  >
                    ลบ
                  </button>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
}
