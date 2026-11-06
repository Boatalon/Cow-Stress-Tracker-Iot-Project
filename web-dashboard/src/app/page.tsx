'use client';

import React, { useCallback, useEffect, useMemo, useState } from 'react';
import {
  DeviceStatus,
  FirmwareRelease,
  ManualCalibrationSection,
  OtaSection,
  TelemetryLog,
} from '@/components/InspectorAndOta';

interface SystemSettings {
  forward_to_ku: boolean;
  ku_server_url: string;
  default_send_sec: number;
  strict_otp_check: boolean;
}

function formatRelativeTime(iso: string | null): { text: string; isOnline: boolean } {
  if (!iso) return { text: 'ยังไม่เคยส่งข้อมูล', isOnline: false };
  const diffSec = Math.max(0, Math.floor((Date.now() - new Date(iso).getTime()) / 1000));
  const isOnline = diffSec <= 180;
  if (diffSec < 5) return { text: 'เมื่อสักครู่นี้', isOnline: true };
  if (diffSec < 60) return { text: `${diffSec} วินาทีที่แล้ว`, isOnline };
  if (diffSec < 3600) return { text: `${Math.floor(diffSec / 60)} นาทีที่แล้ว`, isOnline };
  if (diffSec < 86400) return { text: `${Math.floor(diffSec / 3600)} ชั่วโมงที่แล้ว`, isOnline: false };
  return { text: `${Math.floor(diffSec / 86400)} วันที่แล้ว`, isOnline: false };
}

export default function DashboardPage() {
  const [activeTab, setActiveTab] = useState<'devices' | 'calibrate' | 'inspector' | 'ota' | 'settings'>('devices');
  const [devices, setDevices] = useState<DeviceStatus[]>([]);
  const [logs, setLogs] = useState<TelemetryLog[]>([]);
  const [firmwares, setFirmwares] = useState<FirmwareRelease[]>([]);
  const [settings, setSettings] = useState<SystemSettings>({
    forward_to_ku: false,
    ku_server_url: 'https://sac.eng.kps.ku.ac.th/Dairy-cattle-system-test/IoTDevice/Activity/input.php',
    default_send_sec: 60,
    strict_otp_check: false,
  });
  const [supabaseConnected, setSupabaseConnected] = useState(false);
  const [keySource, setKeySource] = useState<string>('env');
  const [showAllDevices, setShowAllDevices] = useState(false);
  const [selectedDidFilter, setSelectedDidFilter] = useState<number>(0);
  const [originUrl, setOriginUrl] = useState<string>('https://cow-stress-tracker.vercel.app');
  const [toastMsg, setToastMsg] = useState<string>('');

  const [simDid, setSimDid] = useState<number>(6);
  const [simAct, setSimAct] = useState<number>(12);
  const [simStand, setSimStand] = useState<number>(1);
  const [simTemp, setSimTemp] = useState<number>(38.4);
  const [simAngX, setSimAngX] = useState<number>(4.2);
  const [simAngY, setSimAngY] = useState<number>(-1.8);
  const [simAngZ, setSimAngZ] = useState<number>(15.0);
  const [simFw, setSimFw] = useState<string>('2.2.0');
  const [simRssi, setSimRssi] = useState<number>(-45);
  const [simSending, setSimSending] = useState<boolean>(false);
  const [simLastResult, setSimLastResult] = useState<string>('');

  const [otaVersion, setOtaVersion] = useState<string>('2.2.1');
  const [otaTarget, setOtaTarget] = useState<string>('all');
  const [otaNotes, setOtaNotes] = useState<string>('เฟิร์มแวร์ v2.2 รองรับ Manual Calibrate & โหมดส่งต่อเนื่อง/ประหยัดไฟจากหน้าเว็บ');
  const [otaFile, setOtaFile] = useState<File | null>(null);
  const [otaUploading, setOtaUploading] = useState<boolean>(false);

  const showToast = (msg: string) => {
    setToastMsg(msg);
    setTimeout(() => setToastMsg(''), 4000);
  };

  const fetchDashboard = useCallback(async () => {
    try {
      const q = selectedDidFilter > 0 ? `?did=${selectedDidFilter}` : '';
      const res = await fetch(`/api/dashboard${q}`, { cache: 'no-store' });
      const data = await res.json();
      if (data.ok) {
        setDevices(data.devices || []);
        setLogs(data.logs || []);
        setFirmwares(data.firmwares || []);
        if (data.settings) setSettings(data.settings);
        setSupabaseConnected(Boolean(data.supabaseConnected));
        setKeySource(data.keySource || 'env');
      }
    } catch {
      // ignore
    }
  }, [selectedDidFilter]);

  useEffect(() => {
    if (typeof window !== 'undefined') {
      setOriginUrl(window.location.origin);
    }
    fetchDashboard();
    const timer = setInterval(fetchDashboard, 3000);
    return () => clearInterval(timer);
  }, [fetchDashboard]);

  const stats = useMemo(() => {
    const activeDevs = devices.filter((d) => d.total_packets > 0);
    const onlineDevs = devices.filter((d) => formatRelativeTime(d.last_seen).isOnline);
    const totalPackets = devices.reduce((acc, d) => acc + (d.total_packets || 0), 0);
    const validOtpLogs = logs.filter((l) => l.otp_valid).length;
    return {
      activeCount: activeDevs.length,
      onlineCount: onlineDevs.length,
      totalPackets,
      validOtpRate: logs.length > 0 ? Math.round((validOtpLogs / logs.length) * 100) : 100,
    };
  }, [devices, logs]);

  const displayedDevices = useMemo(() => {
    if (showAllDevices) return devices;
    const active = devices.filter((d) => d.total_packets > 0 || d.did === 6 || d.did === 13);
    return active.sort((a, b) => b.total_packets - a.total_packets || a.did - b.did);
  }, [devices, showAllDevices]);

  const handleSendTestPacket = async (forceWrongOtp = false) => {
    setSimSending(true);
    setSimLastResult('');
    try {
      const nowEpoch = Math.floor(Date.now() / 1000);
      const otpRes = await fetch('/api/dashboard', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          action: 'generate_otp',
          did: simDid,
          time: String(nowEpoch),
        }),
      });
      const otpData = await otpRes.json();
      const codeToSend = forceWrongOtp ? (otpData.otp + 999) % 7000 : otpData.otp;

      const params = new URLSearchParams({
        DID: String(simDid),
        ACT: String(simStand === 1 ? simAct : 0),
        ANGX: String(simAngX),
        ANGY: String(simAngY),
        ANGZ: String(simAngZ),
        Stand: String(simStand),
        Sleep: String(simStand === 1 ? 0 : 1),
        Temp: String(simTemp),
        Time: String(nowEpoch),
        Code: String(codeToSend),
        FW: simFw,
        RSSI: String(simRssi),
      });

      const res = await fetch(`/api/iot/input?${params.toString()}`);
      const json = await res.json();
      setSimLastResult(
        JSON.stringify(
          {
            request_url: `/api/iot/input?${params.toString()}`,
            http_status: res.status,
            calculated_otp: otpData.otp,
            sent_otp: codeToSend,
            server_response: json,
          },
          null,
          2
        )
      );
      showToast(
        forceWrongOtp
          ? '🧪 ส่ง Packet ทดสอบ (รหัส OTP ผิดจำลอง) เรียบร้อย!'
          : `✅ ส่งข้อมูล Device #${simDid} สำเร็จ! (OTP: ${codeToSend})`
      );
      await fetchDashboard();
    } catch (err) {
      setSimLastResult(err instanceof Error ? err.message : 'เกิดข้อผิดพลาด');
    } finally {
      setSimSending(false);
    }
  };

  const handleUploadOta = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!otaFile) {
      showToast('⚠️ กรุณาเลือกไฟล์ .bin ก่อนอัปโหลด');
      return;
    }
    setOtaUploading(true);
    try {
      const fd = new FormData();
      fd.append('file', otaFile);
      fd.append('version', otaVersion);
      fd.append('target_devices', otaTarget);
      fd.append('notes', otaNotes);

      const res = await fetch('/api/ota', { method: 'POST', body: fd });
      const data = await res.json();
      if (data.ok) {
        showToast(`🚀 อัปโหลดเฟิร์มแวร์ v${otaVersion} สำเร็จ!`);
        setOtaFile(null);
        await fetchDashboard();
      } else {
        showToast(`❌ อัปโหลดไม่สำเร็จ: ${data.error}`);
      }
    } catch {
      showToast('❌ เกิดข้อผิดพลาดในการอัปโหลดไฟล์');
    } finally {
      setOtaUploading(false);
    }
  };

  const handleDeleteOta = async (version: string) => {
    await fetch(`/api/ota?version=${encodeURIComponent(version)}`, { method: 'DELETE' });
    showToast(`🗑️ ลบเฟิร์มแวร์เวอร์ชัน ${version} แล้ว`);
    await fetchDashboard();
  };

  const handleSaveSettings = async (newSettings: Partial<SystemSettings>) => {
    const merged = { ...settings, ...newSettings };
    setSettings(merged);
    await fetch('/api/dashboard', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ action: 'update_settings', ...merged }),
    });
    showToast('💾 บันทึกการตั้งค่าระบบเรียบร้อยแล้ว');
    await fetchDashboard();
  };

  const handleUpdateDeviceInterval = async (did: number, sendSec: number) => {
    await fetch('/api/dashboard', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ action: 'update_device', did, send_sec: sendSec }),
    });
    showToast(`⏱️ ตั้งเวลารอบส่งข้อมูลเป็น ${sendSec} วินาทีแล้ว`);
    await fetchDashboard();
  };

  return (
    <div className="min-h-screen bg-slate-950 text-slate-100 pb-16">
      {toastMsg && (
        <div className="fixed bottom-5 right-5 z-50 bg-emerald-600 text-white px-4 py-3 rounded-xl shadow-2xl border border-emerald-400/40 text-sm font-medium">
          {toastMsg}
        </div>
      )}

      <header className="border-b border-slate-800 bg-slate-900/80 backdrop-blur sticky top-0 z-30">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 py-4 flex flex-col lg:flex-row lg:items-center lg:justify-between gap-4">
          <div className="flex items-center gap-3">
            <div className="w-11 h-11 rounded-2xl bg-emerald-500/15 border border-emerald-500/40 flex items-center justify-center text-2xl">
              🐄
            </div>
            <div>
              <div className="flex items-center gap-2 flex-wrap">
                <h1 className="text-lg sm:text-xl font-bold text-white tracking-tight">
                  Cow-Stress-Tracker IoT Cloud
                </h1>
                <span className="px-2.5 py-0.5 text-xs font-semibold rounded-full bg-emerald-500/20 text-emerald-300 border border-emerald-500/30">
                  v2.2 Manual Calibrate & OTA
                </span>
              </div>
              <p className="text-xs sm:text-sm text-slate-400">
                ระบบเช็กสถานะการต่อเน็ต กด Manual Calibrate ท่ายืนจากหน้าเว็บ และอัปเดตเฟิร์มแวร์ผ่าน Wi-Fi
              </p>
            </div>
          </div>

          <div className="flex flex-wrap items-center gap-2 text-xs">
            <span className="px-3 py-1.5 rounded-lg bg-slate-800 border border-slate-700 text-slate-300 flex items-center gap-1.5">
              <span className="w-2 h-2 rounded-full bg-emerald-400 animate-pulse" />
              API Key (OTP):{' '}
              <strong className="text-emerald-300">
                {keySource === 'env' || keySource === 'secrets_header' ? 'รหัสจริง (Device 1–20)' : 'Dummy Keys'}
              </strong>
            </span>
            <span
              className={`px-3 py-1.5 rounded-lg border flex items-center gap-1.5 ${
                supabaseConnected
                  ? 'bg-emerald-950/60 border-emerald-700/60 text-emerald-300'
                  : 'bg-amber-950/60 border-amber-700/60 text-amber-300'
              }`}
            >
              🗄️ DB: <strong>{supabaseConnected ? 'Supabase Connected' : 'Local Mode'}</strong>
            </span>
            <button
              onClick={() => setActiveTab('calibrate')}
              className="px-3.5 py-1.5 rounded-lg bg-amber-500 hover:bg-amber-400 text-slate-950 font-bold transition cursor-pointer"
            >
              🎯 ไปหน้า Calibrate ท่ายืน
            </button>
          </div>
        </div>

        <div className="max-w-7xl mx-auto px-4 sm:px-6 flex gap-1 overflow-x-auto pt-1">
          {[
            { id: 'devices', label: '📡 สถานะบอร์ด & ข้อมูลสด' },
            { id: 'calibrate', label: '🎯 Manual Calibrate & โหมดส่งต่อเนื่อง' },
            { id: 'inspector', label: '🔍 เช็ก Log การส่งข้อมูล & ทดสอบ OTP' },
            { id: 'ota', label: '🚀 อัปเดตเฟิร์มแวร์ประหยัดไฟ (OTA)' },
            { id: 'settings', label: '⚙️ ตั้งค่าระบบ & รอบเวลาส่ง' },
          ].map((tab) => (
            <button
              key={tab.id}
              onClick={() => setActiveTab(tab.id as typeof activeTab)}
              className={`px-4 py-2.5 text-sm font-medium border-b-2 transition whitespace-nowrap cursor-pointer ${
                activeTab === tab.id
                  ? 'border-emerald-400 text-emerald-300 bg-emerald-500/10 rounded-t-lg'
                  : 'border-transparent text-slate-400 hover:text-slate-200'
              }`}
            >
              {tab.label}
            </button>
          ))}
        </div>
      </header>

      <main className="max-w-7xl mx-auto px-4 sm:px-6 pt-6 space-y-6">
        <div className="grid grid-cols-2 lg:grid-cols-4 gap-4">
          <div className="bg-slate-900 border border-slate-800 rounded-2xl p-4">
            <div className="text-xs text-slate-400">บอร์ดที่ออนไลน์ตอนนี้</div>
            <div className="mt-1 flex items-baseline gap-2">
              <span className="text-2xl sm:text-3xl font-bold text-emerald-400">{stats.onlineCount}</span>
              <span className="text-xs text-slate-400">/ {stats.activeCount} เครื่องที่เคยเชื่อมต่อ</span>
            </div>
          </div>
          <div className="bg-slate-900 border border-slate-800 rounded-2xl p-4">
            <div className="text-xs text-slate-400">จำนวนข้อมูลที่ส่งเข้าสำเร็จ</div>
            <div className="mt-1 flex items-baseline gap-2">
              <span className="text-2xl sm:text-3xl font-bold text-white">{stats.totalPackets}</span>
              <span className="text-xs text-slate-400">Packets</span>
            </div>
          </div>
          <div className="bg-slate-900 border border-slate-800 rounded-2xl p-4">
            <div className="text-xs text-slate-400">ความถูกต้องของรหัส OTP</div>
            <div className="mt-1 flex items-baseline gap-2">
              <span className="text-2xl sm:text-3xl font-bold text-cyan-400">{stats.validOtpRate}%</span>
              <span className="text-xs text-slate-400">OTP_Activity.h</span>
            </div>
          </div>
          <div className="bg-slate-900 border border-slate-800 rounded-2xl p-4">
            <div className="text-xs text-slate-400">ส่งต่อไปเซิร์ฟเวอร์ มก. (KU Proxy)</div>
            <div className="mt-1 flex items-center justify-between">
              <span className={`text-sm font-bold ${settings.forward_to_ku ? 'text-emerald-400' : 'text-slate-400'}`}>
                {settings.forward_to_ku ? '🟢 เปิดส่งต่ออัตโนมัติ' : '⚪ ปิดอยู่'}
              </span>
              <button
                onClick={() => handleSaveSettings({ forward_to_ku: !settings.forward_to_ku })}
                className="text-xs px-2.5 py-1 rounded-lg bg-slate-800 hover:bg-slate-700 text-slate-200 border border-slate-700 cursor-pointer"
              >
                สลับ
              </button>
            </div>
          </div>
        </div>

        {activeTab === 'devices' && (
          <div className="space-y-6">
            <div className="bg-slate-900/90 border border-emerald-500/30 rounded-2xl p-4 flex flex-col md:flex-row md:items-center justify-between gap-4">
              <div className="space-y-1">
                <div className="text-xs font-semibold uppercase tracking-wider text-emerald-400">
                  🌐 URL สำหรับตั้งค่าในบอร์ด ESP32-C3 (ใช้รหัส OTP เดิมได้เลย)
                </div>
                <div className="text-sm text-slate-300 font-mono break-all">
                  server = &quot;<span className="text-emerald-300 font-bold">{originUrl.replace(/^https?:\/\//, '')}</span>&quot;;
                  <span className="text-slate-500 mx-2">|</span>
                  pathphp = &quot;<span className="text-cyan-300">/api/iot/input</span>&quot;;
                </div>
              </div>
              <div className="flex items-center gap-2 shrink-0">
                <button
                  onClick={() => {
                    navigator.clipboard.writeText(originUrl.replace(/^https?:\/\//, ''));
                    showToast('📋 คัดลอกชื่อ Server Domain แล้ว!');
                  }}
                  className="px-3 py-2 rounded-xl bg-slate-800 hover:bg-slate-700 text-xs font-medium border border-slate-700 cursor-pointer"
                >
                  📋 คัดลอกชื่อโดเมน
                </button>
                <button
                  onClick={() => setShowAllDevices(!showAllDevices)}
                  className="px-3 py-2 rounded-xl bg-slate-800 hover:bg-slate-700 text-xs font-medium border border-slate-700 cursor-pointer"
                >
                  {showAllDevices ? 'แสดงเฉพาะเครื่องที่ใช้งาน' : 'แสดงครบทั้ง 20 เครื่อง'}
                </button>
              </div>
            </div>

            <div className="grid grid-cols-1 md:grid-cols-2 xl:grid-cols-3 gap-4">
              {displayedDevices.map((dev) => {
                const rel = formatRelativeTime(dev.last_seen);
                const isStanding = dev.last_stand === 1;
                return (
                  <div
                    key={dev.did}
                    className={`rounded-2xl border p-5 transition ${
                      rel.isOnline
                        ? 'bg-slate-900 border-emerald-500/50 shadow-lg shadow-emerald-950/30'
                        : dev.total_packets > 0
                        ? 'bg-slate-900/90 border-slate-700'
                        : 'bg-slate-900/40 border-slate-800/80 opacity-80'
                    }`}
                  >
                    <div className="flex items-start justify-between gap-2">
                      <div>
                        <div className="flex items-center gap-2">
                          <span className="px-2.5 py-0.5 rounded-lg bg-slate-800 text-white font-bold text-sm border border-slate-700">
                            DID #{dev.did}
                          </span>
                          <h3 className="font-semibold text-white">{dev.name}</h3>
                        </div>
                        <p className="text-xs text-slate-400 mt-1">
                          ติดต่อล่าสุด: <span className="text-slate-200 font-medium">{rel.text}</span>
                        </p>
                      </div>
                      <div className="flex flex-col items-end gap-1">
                        <span
                          className={`px-2.5 py-1 rounded-full text-xs font-semibold flex items-center gap-1.5 ${
                            rel.isOnline
                              ? 'bg-emerald-500/20 text-emerald-300 border border-emerald-500/40'
                              : dev.total_packets > 0
                              ? 'bg-amber-500/15 text-amber-300 border border-amber-500/30'
                              : 'bg-slate-800 text-slate-400 border border-slate-700'
                          }`}
                        >
                          {rel.isOnline ? '🟢 Online' : dev.total_packets > 0 ? 'รอข้อมูลรอบถัดไป' : 'ยังไม่เชื่อมต่อ'}
                        </span>
                        {dev.continuous_mode && (
                          <span className="text-[11px] px-2 py-0.5 rounded-md font-semibold bg-amber-500/20 text-amber-300">
                            ⚡ โหมดส่งต่อเนื่อง (5s)
                          </span>
                        )}
                      </div>
                    </div>

                    <div
                      className={`mt-4 p-3 rounded-xl border flex items-center justify-between ${
                        isStanding
                          ? 'bg-emerald-950/30 border-emerald-700/40'
                          : 'bg-purple-950/30 border-purple-700/40'
                      }`}
                    >
                      <div className="flex items-center gap-2.5">
                        <span className="text-xl">{isStanding ? '🟢' : '🟣'}</span>
                        <div className="text-xs font-bold text-white">
                          {isStanding
                            ? `ยืน (STANDING) — Tilt: ${(dev.last_tilt || 0).toFixed(1)}°`
                            : `นอน (LYING) — Tilt: ${(dev.last_tilt || 0).toFixed(1)}°`}
                        </div>
                      </div>
                      <button
                        onClick={() => setActiveTab('calibrate')}
                        className="px-2.5 py-1 rounded-lg bg-slate-800 hover:bg-slate-700 text-[11px] text-amber-300 font-semibold border border-slate-700 cursor-pointer"
                      >
                        🎯 Calibrate
                      </button>
                    </div>

                    <div className="mt-4 grid grid-cols-3 gap-2.5 text-center">
                      <div className="bg-slate-950/70 border border-slate-800 rounded-xl p-2.5">
                        <div className="text-[11px] text-slate-400">ก้าวเดิน (ACT)</div>
                        <div className="text-lg font-bold text-emerald-400 mt-0.5">{dev.last_act}</div>
                      </div>
                      <div className="bg-slate-950/70 border border-slate-800 rounded-xl p-2.5">
                        <div className="text-[11px] text-slate-400">อุณหภูมิ</div>
                        <div className="text-lg font-bold text-amber-300 mt-0.5">
                          {dev.last_temp ? `${dev.last_temp.toFixed(1)}°C` : '-'}
                        </div>
                      </div>
                      <div className="bg-slate-950/70 border border-slate-800 rounded-xl p-2.5">
                        <div className="text-[11px] text-slate-400">Wi-Fi RSSI</div>
                        <div className="text-sm font-bold text-cyan-300 mt-1">
                          {dev.last_rssi ? `${dev.last_rssi} dBm` : '-'}
                        </div>
                      </div>
                    </div>

                    <div className="mt-3 pt-3 border-t border-slate-800/80 flex items-center justify-between text-xs text-slate-400">
                      <span className="font-mono">
                        X:{dev.last_ang_x.toFixed(1)}° Y:{dev.last_ang_y.toFixed(1)}° Z:{dev.last_ang_z.toFixed(1)}°
                      </span>
                      <span>
                        FW: {dev.fw_version || '-'} | ส่งทุก {dev.continuous_mode ? 5 : dev.send_sec}s
                      </span>
                    </div>
                  </div>
                );
              })}
            </div>
          </div>
        )}

        {activeTab === 'calibrate' && (
          <ManualCalibrationSection
            devices={devices}
            onRefresh={fetchDashboard}
            showToast={showToast}
          />
        )}

        {activeTab === 'inspector' && (
          <div className="space-y-6">
            <div className="bg-slate-900 border border-slate-800 rounded-2xl p-5 space-y-4">
              <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-2">
                <div>
                  <h2 className="text-base font-bold text-white">🧪 ทดสอบการส่งข้อมูล & คำนวณรหัสลับ OTP</h2>
                  <p className="text-xs text-slate-400">จำลองการส่งข้อมูลจากบอร์ด ESP32-C3 พร้อมรหัส OTP</p>
                </div>
                <div className="flex items-center gap-2">
                  <button
                    onClick={() => handleSendTestPacket(false)}
                    disabled={simSending}
                    className="px-4 py-2 rounded-xl bg-emerald-600 hover:bg-emerald-500 text-white text-xs font-bold cursor-pointer"
                  >
                    🚀 ยิงข้อมูลทดสอบ (OTP ถูกต้อง)
                  </button>
                  <button
                    onClick={() => handleSendTestPacket(true)}
                    disabled={simSending}
                    className="px-3 py-2 rounded-xl bg-rose-950/80 hover:bg-rose-900 text-rose-200 border border-rose-700/50 text-xs font-medium cursor-pointer"
                  >
                    ลองยิง OTP ผิด
                  </button>
                </div>
              </div>

              <div className="grid grid-cols-2 sm:grid-cols-5 gap-3 text-xs">
                <div>
                  <label className="text-slate-400 block mb-1">Device ID (1-20)</label>
                  <input
                    type="number"
                    min={1}
                    max={20}
                    value={simDid}
                    onChange={(e) => setSimDid(Number(e.target.value))}
                    className="w-full bg-slate-950 border border-slate-700 rounded-lg px-2.5 py-1.5 text-white"
                  />
                </div>
                <div>
                  <label className="text-slate-400 block mb-1">ท่าทาง (Stand)</label>
                  <select
                    value={simStand}
                    onChange={(e) => setSimStand(Number(e.target.value))}
                    className="w-full bg-slate-950 border border-slate-700 rounded-lg px-2.5 py-1.5 text-white"
                  >
                    <option value={1}>1 = ยืน (นับก้าว)</option>
                    <option value={0}>0 = นอน (ไม่นับก้าว)</option>
                  </select>
                </div>
                <div>
                  <label className="text-slate-400 block mb-1">จำนวนก้าว (ACT)</label>
                  <input
                    type="number"
                    value={simStand === 1 ? simAct : 0}
                    disabled={simStand === 0}
                    onChange={(e) => setSimAct(Number(e.target.value))}
                    className="w-full bg-slate-950 border border-slate-700 rounded-lg px-2.5 py-1.5 text-white"
                  />
                </div>
                <div>
                  <label className="text-slate-400 block mb-1">อุณหภูมิ (°C)</label>
                  <input
                    type="number"
                    step="0.1"
                    value={simTemp}
                    onChange={(e) => setSimTemp(Number(e.target.value))}
                    className="w-full bg-slate-950 border border-slate-700 rounded-lg px-2.5 py-1.5 text-white"
                  />
                </div>
                <div>
                  <label className="text-slate-400 block mb-1">เวอร์ชัน FW</label>
                  <input
                    type="text"
                    value={simFw}
                    onChange={(e) => setSimFw(e.target.value)}
                    className="w-full bg-slate-950 border border-slate-700 rounded-lg px-2.5 py-1.5 text-white"
                  />
                </div>
              </div>

              {simLastResult && (
                <pre className="bg-slate-950 border border-slate-800 rounded-xl p-3 text-xs font-mono text-emerald-300 overflow-x-auto">
                  {simLastResult}
                </pre>
              )}
            </div>

            <div className="bg-slate-900 border border-slate-800 rounded-2xl overflow-hidden">
              <div className="p-4 border-b border-slate-800 flex items-center justify-between">
                <h3 className="text-base font-bold text-white">📋 ประวัติการรับข้อมูลสด (Live Incoming Requests)</h3>
                <select
                  value={selectedDidFilter}
                  onChange={(e) => setSelectedDidFilter(Number(e.target.value))}
                  className="bg-slate-950 border border-slate-700 rounded-lg px-3 py-1.5 text-xs text-white"
                >
                  <option value={0}>ทุกเครื่อง (All Devices)</option>
                  {Array.from({ length: 20 }, (_, i) => i + 1).map((id) => (
                    <option key={id} value={id}>
                      Device #{id}
                    </option>
                  ))}
                </select>
              </div>
              <div className="overflow-x-auto">
                <table className="w-full text-left border-collapse text-xs">
                  <thead>
                    <tr className="bg-slate-950/60 text-slate-400 border-b border-slate-800">
                      <th className="py-3 px-4">เวลา</th>
                      <th className="py-3 px-3">DID</th>
                      <th className="py-3 px-3">ท่าทาง</th>
                      <th className="py-3 px-3">ก้าว</th>
                      <th className="py-3 px-3">ANG (X / Y / Z)</th>
                      <th className="py-3 px-3">Temp</th>
                      <th className="py-3 px-3">ผลการตรวจ OTP</th>
                      <th className="py-3 px-3">FW / RSSI</th>
                    </tr>
                  </thead>
                  <tbody className="divide-y divide-slate-800/70">
                    {logs.map((log) => (
                      <tr key={log.id} className="hover:bg-slate-800/40">
                        <td className="py-2.5 px-4 font-mono text-slate-300">
                          {new Date(log.created_at).toLocaleTimeString('th-TH')}
                        </td>
                        <td className="py-2.5 px-3 font-bold text-white">#{log.did}</td>
                        <td className="py-2.5 px-3">
                          {log.stand === 1 ? (
                            <span className="text-emerald-300 font-medium">ยืน (1)</span>
                          ) : (
                            <span className="text-purple-300 font-medium">นอน (0)</span>
                          )}
                        </td>
                        <td className="py-2.5 px-3 font-bold text-emerald-400">{log.act}</td>
                        <td className="py-2.5 px-3 font-mono text-slate-300">
                          {log.ang_x.toFixed(1)}° / {log.ang_y.toFixed(1)}° / {log.ang_z.toFixed(1)}°
                        </td>
                        <td className="py-2.5 px-3 text-amber-300">{log.temp.toFixed(1)}°C</td>
                        <td className="py-2.5 px-3 font-mono">
                          {log.otp_valid ? (
                            <span className="text-cyan-300">✅ {log.otp_received}</span>
                          ) : (
                            <span className="text-rose-300">
                              ❌ รับ {log.otp_received} (ควรเป็น {log.otp_expected})
                            </span>
                          )}
                        </td>
                        <td className="py-2.5 px-3 text-slate-400 font-mono">
                          v{log.fw_version || '-'} ({log.rssi || 0} dBm)
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            </div>
          </div>
        )}

        {activeTab === 'ota' && (
          <OtaSection
            firmwares={firmwares}
            otaVersion={otaVersion}
            setOtaVersion={setOtaVersion}
            otaTarget={otaTarget}
            setOtaTarget={setOtaTarget}
            otaNotes={otaNotes}
            setOtaNotes={setOtaNotes}
            setOtaFile={setOtaFile}
            otaUploading={otaUploading}
            onUpload={handleUploadOta}
            onDelete={handleDeleteOta}
          />
        )}

        {activeTab === 'settings' && (
          <div className="bg-slate-900 border border-slate-800 rounded-2xl p-5 space-y-5">
            <h2 className="text-base font-bold text-white">🔋 ตั้งค่ารอบเวลาส่งข้อมูลประหยัดพลังงาน</h2>
            <div className="flex flex-wrap gap-2">
              {[
                { label: '⚡ 15 วินาที (โหมดส่งไว)', sec: 15 },
                { label: '⚡ 30 วินาที (โหมดทดสอบ)', sec: 30 },
                { label: '⏱️ 60 วินาที (1 นาที - แนะนำ)', sec: 60 },
                { label: '🔋 180 วินาที (3 นาที - ประหยัดไฟ)', sec: 180 },
                { label: '🌙 300 วินาที (5 นาที - ประหยัดไฟสูงสุด)', sec: 300 },
              ].map((preset) => (
                <button
                  key={preset.sec}
                  onClick={() => {
                    handleSaveSettings({ default_send_sec: preset.sec });
                    handleUpdateDeviceInterval(0, preset.sec);
                  }}
                  className={`px-3 py-2 rounded-xl text-xs font-medium border transition cursor-pointer ${
                    settings.default_send_sec === preset.sec
                      ? 'bg-emerald-600 text-white border-emerald-400'
                      : 'bg-slate-950 text-slate-300 border-slate-800'
                  }`}
                >
                  {preset.label}
                </button>
              ))}
            </div>
          </div>
        )}
      </main>
    </div>
  );
}
