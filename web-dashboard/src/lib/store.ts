import { createClient, SupabaseClient } from '@supabase/supabase-js';
import fs from 'fs';
import path from 'path';
import { MAX_DEVICE_ID } from './otp';

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

export interface SystemSettings {
  forward_to_ku: boolean;
  ku_server_url: string;
  default_send_sec: number;
  strict_otp_check: boolean;
}

interface LocalStoreData {
  devices: DeviceStatus[];
  logs: TelemetryLog[];
  firmwares: FirmwareRelease[];
  settings: SystemSettings;
  nextLogId: number;
  nextFwId: number;
}

const LOCAL_STORE_PATH = path.join('/tmp', 'cow-stress-tracker-store.json');
export const LOCAL_FIRMWARE_DIR = path.join('/tmp', 'cow-stress-tracker-firmware');

export function isSupabaseConfigured(): boolean {
  const url = process.env.NEXT_PUBLIC_SUPABASE_URL || '';
  const key = process.env.SUPABASE_SERVICE_ROLE_KEY || process.env.NEXT_PUBLIC_SUPABASE_ANON_KEY || '';
  return (
    url.startsWith('https://') &&
    !url.includes('your-project-id') &&
    key.length > 20 &&
    !key.includes('your-')
  );
}

export function getSupabaseAdmin(): SupabaseClient | null {
  if (!isSupabaseConfigured()) return null;
  const url = process.env.NEXT_PUBLIC_SUPABASE_URL!;
  const key = process.env.SUPABASE_SERVICE_ROLE_KEY || process.env.NEXT_PUBLIC_SUPABASE_ANON_KEY!;
  return createClient(url, key, {
    auth: { persistSession: false },
  });
}

function normalizeDevice(raw: Partial<DeviceStatus>, fallbackDid: number): DeviceStatus {
  const did = raw.did || fallbackDid;
  return {
    did,
    name: raw.name || `Cow Tracker #${did}`,
    last_seen: raw.last_seen ?? null,
    last_act: Number(raw.last_act) || 0,
    last_ang_x: Number(raw.last_ang_x) || 0,
    last_ang_y: Number(raw.last_ang_y) || 0,
    last_ang_z: Number(raw.last_ang_z) || 0,
    last_gx: raw.last_gx !== undefined ? Number(raw.last_gx) : 0,
    last_gy: raw.last_gy !== undefined ? Number(raw.last_gy) : 0,
    last_gz: raw.last_gz !== undefined ? Number(raw.last_gz) : 1,
    last_tilt: Number(raw.last_tilt) || 0,
    last_stand: raw.last_stand !== undefined ? Number(raw.last_stand) : 1,
    last_sleep: raw.last_sleep !== undefined ? Number(raw.last_sleep) : 0,
    last_temp: Number(raw.last_temp) || 0,
    last_rssi: Number(raw.last_rssi) || 0,
    fw_version: raw.fw_version || '-',
    last_otp_valid: raw.last_otp_valid !== undefined ? Boolean(raw.last_otp_valid) : true,
    total_packets: Number(raw.total_packets) || 0,
    send_sec: Number(raw.send_sec) || 60,
    target_ota_ver: raw.target_ota_ver || '',
    ref_gx: raw.ref_gx !== undefined ? Number(raw.ref_gx) : 0,
    ref_gy: raw.ref_gy !== undefined ? Number(raw.ref_gy) : 0,
    ref_gz: raw.ref_gz !== undefined ? Number(raw.ref_gz) : 1,
    step_trig: Number(raw.step_trig) || 0.25,
    step_rst: Number(raw.step_rst) || 0.12,
    lie_ang: Number(raw.lie_ang) || 45.0,
    stand_ang: Number(raw.stand_ang) || 30.0,
    cal_seq: Number(raw.cal_seq) || 0,
    continuous_mode: Boolean(raw.continuous_mode),
  };
}

function createInitialLocalStore(): LocalStoreData {
  const devices: DeviceStatus[] = Array.from({ length: MAX_DEVICE_ID }, (_, idx) =>
    normalizeDevice({ did: idx + 1 }, idx + 1)
  );

  return {
    devices,
    logs: [],
    firmwares: [],
    settings: {
      forward_to_ku: false,
      ku_server_url: 'https://sac.eng.kps.ku.ac.th/Dairy-cattle-system-test/IoTDevice/Activity/input.php',
      default_send_sec: 60,
      strict_otp_check: false,
    },
    nextLogId: 1,
    nextFwId: 1,
  };
}

function readLocalStore(): LocalStoreData {
  try {
    if (fs.existsSync(LOCAL_STORE_PATH)) {
      const raw = fs.readFileSync(LOCAL_STORE_PATH, 'utf8');
      const data = JSON.parse(raw) as LocalStoreData;
      if (Array.isArray(data.devices) && data.devices.length === MAX_DEVICE_ID) {
        data.devices = data.devices.map((d, i) => normalizeDevice(d, i + 1));
        return data;
      }
    }
  } catch {
    // ignore and recreate
  }
  const initial = createInitialLocalStore();
  writeLocalStore(initial);
  return initial;
}

function writeLocalStore(data: LocalStoreData): void {
  try {
    fs.writeFileSync(LOCAL_STORE_PATH, JSON.stringify(data, null, 2), 'utf8');
  } catch {
    // ignore write error
  }
}

export async function getSystemSettings(): Promise<SystemSettings> {
  const sb = getSupabaseAdmin();
  if (sb) {
    const { data, error } = await sb.from('system_settings').select('*').eq('id', 1).single();
    if (!error && data) {
      return {
        forward_to_ku: Boolean(data.forward_to_ku),
        ku_server_url: data.ku_server_url || 'https://sac.eng.kps.ku.ac.th/Dairy-cattle-system-test/IoTDevice/Activity/input.php',
        default_send_sec: Number(data.default_send_sec) || 60,
        strict_otp_check: Boolean(data.strict_otp_check),
      };
    }
  }
  return readLocalStore().settings;
}

export async function updateSystemSettings(partial: Partial<SystemSettings>): Promise<SystemSettings> {
  const current = await getSystemSettings();
  const updated: SystemSettings = {
    ...current,
    ...partial,
  };

  const sb = getSupabaseAdmin();
  if (sb) {
    await sb.from('system_settings').upsert({
      id: 1,
      ...updated,
      updated_at: new Date().toISOString(),
    });
  }

  const local = readLocalStore();
  local.settings = updated;
  writeLocalStore(local);
  return updated;
}

export async function getDevices(): Promise<DeviceStatus[]> {
  const sb = getSupabaseAdmin();
  if (sb) {
    const { data, error } = await sb.from('devices').select('*').order('did', { ascending: true });
    if (!error && data && data.length > 0) {
      return data.map((d, i) => normalizeDevice(d as Partial<DeviceStatus>, i + 1));
    }
  }
  return readLocalStore().devices;
}

export interface DeviceConfigUpdates {
  name?: string;
  send_sec?: number;
  target_ota_ver?: string;
  ref_gx?: number;
  ref_gy?: number;
  ref_gz?: number;
  step_trig?: number;
  step_rst?: number;
  lie_ang?: number;
  stand_ang?: number;
  cal_seq?: number;
  continuous_mode?: boolean;
}

export async function updateDeviceConfig(did: number, updates: DeviceConfigUpdates): Promise<void> {
  const sb = getSupabaseAdmin();
  if (sb) {
    if (did === 0) {
      await sb.from('devices').update({ ...updates, updated_at: new Date().toISOString() }).gte('did', 1);
    } else {
      await sb.from('devices').update({ ...updates, updated_at: new Date().toISOString() }).eq('did', did);
    }
  }

  const local = readLocalStore();
  for (const dev of local.devices) {
    if (did === 0 || dev.did === did) {
      Object.assign(dev, updates);
    }
  }
  writeLocalStore(local);
}

export async function getRecentLogs(limit = 60, didFilter?: number): Promise<TelemetryLog[]> {
  const sb = getSupabaseAdmin();
  if (sb) {
    let query = sb.from('telemetry_logs').select('*').order('created_at', { ascending: false }).limit(limit);
    if (didFilter && didFilter >= 1 && didFilter <= MAX_DEVICE_ID) {
      query = query.eq('did', didFilter);
    }
    const { data, error } = await query;
    if (!error && data) {
      return data as TelemetryLog[];
    }
  }

  const local = readLocalStore();
  let logs = local.logs;
  if (didFilter && didFilter >= 1 && didFilter <= MAX_DEVICE_ID) {
    logs = logs.filter((l) => l.did === didFilter);
  }
  return logs.slice(0, limit);
}

export async function recordTelemetry(
  entry: Omit<TelemetryLog, 'id' | 'created_at'> & {
    gx?: number;
    gy?: number;
    gz?: number;
    tilt?: number;
  }
): Promise<{
  log: TelemetryLog;
  device: DeviceStatus;
  otaRelease: FirmwareRelease | null;
}> {
  const nowIso = new Date().toISOString();
  const local = readLocalStore();

  const { gx, gy, gz, tilt, ...logFields } = entry;

  const newLog: TelemetryLog = {
    ...logFields,
    id: local.nextLogId++,
    created_at: nowIso,
  };
  local.logs.unshift(newLog);
  if (local.logs.length > 300) {
    local.logs = local.logs.slice(0, 300);
  }

  let targetDevice = local.devices.find((d) => d.did === entry.did) || local.devices[0];
  if (entry.did >= 1 && entry.did <= MAX_DEVICE_ID) {
    targetDevice = local.devices[entry.did - 1];
    targetDevice.last_seen = nowIso;
    targetDevice.last_act = entry.act;
    targetDevice.last_ang_x = entry.ang_x;
    targetDevice.last_ang_y = entry.ang_y;
    targetDevice.last_ang_z = entry.ang_z;
    if (gx !== undefined) targetDevice.last_gx = gx;
    if (gy !== undefined) targetDevice.last_gy = gy;
    if (gz !== undefined) targetDevice.last_gz = gz;
    if (tilt !== undefined) targetDevice.last_tilt = tilt;
    targetDevice.last_stand = entry.stand;
    targetDevice.last_sleep = entry.sleep;
    targetDevice.last_temp = entry.temp;
    targetDevice.last_rssi = entry.rssi;
    if (entry.fw_version) targetDevice.fw_version = entry.fw_version;
    targetDevice.last_otp_valid = entry.otp_valid;
    targetDevice.total_packets = (targetDevice.total_packets || 0) + 1;
  }
  writeLocalStore(local);

  const sb = getSupabaseAdmin();
  if (sb) {
    await sb.from('telemetry_logs').insert({
      ...logFields,
      created_at: nowIso,
    });

    if (entry.did >= 1 && entry.did <= MAX_DEVICE_ID) {
      const { data: existingRaw } = await sb.from('devices').select('*').eq('did', entry.did).single();
      const existingDev = normalizeDevice((existingRaw || {}) as Partial<DeviceStatus>, entry.did);
      const totalPackets = (existingDev.total_packets || 0) + 1;

      // ถ้ายังไม่ส่ง GX/GY/GZ มาโดยตรง ให้แปลงค่าประมาณจากมุม ANGX, ANGY
      const radX = (entry.ang_x * Math.PI) / 180;
      const radY = (entry.ang_y * Math.PI) / 180;
      const estGx = gx !== undefined ? gx : Number((-Math.sin(radY)).toFixed(4));
      const estGy = gy !== undefined ? gy : Number((Math.sin(radX) * Math.cos(radY)).toFixed(4));
      const estGz = gz !== undefined ? gz : Number((Math.cos(radX) * Math.cos(radY)).toFixed(4));

      // คำนวณมุม Tilt เทียบกับ ref_gx, ref_gy, ref_gz บนฝั่งเว็บให้ด้วย
      let calcTilt = tilt !== undefined ? tilt : existingDev.last_tilt;
      if (tilt === undefined) {
        const n1 = Math.sqrt(estGx * estGx + estGy * estGy + estGz * estGz);
        const n2 = Math.sqrt(
          existingDev.ref_gx * existingDev.ref_gx +
            existingDev.ref_gy * existingDev.ref_gy +
            existingDev.ref_gz * existingDev.ref_gz
        );
        if (n1 > 0.01 && n2 > 0.01) {
          const dot = Math.max(
            -1,
            Math.min(
              1,
              (estGx * existingDev.ref_gx + estGy * existingDev.ref_gy + estGz * existingDev.ref_gz) /
                (n1 * n2)
            )
          );
          calcTilt = Number(((Math.acos(dot) * 180) / Math.PI).toFixed(1));
        }
      }

      const updatedDev: DeviceStatus = {
        ...existingDev,
        did: entry.did,
        last_seen: nowIso,
        last_act: entry.act,
        last_ang_x: entry.ang_x,
        last_ang_y: entry.ang_y,
        last_ang_z: entry.ang_z,
        last_gx: estGx,
        last_gy: estGy,
        last_gz: estGz,
        last_tilt: calcTilt,
        last_stand: entry.stand,
        last_sleep: entry.sleep,
        last_temp: entry.temp,
        last_rssi: entry.rssi,
        fw_version: entry.fw_version || existingDev.fw_version || '1.0.0',
        last_otp_valid: entry.otp_valid,
        total_packets: totalPackets,
      };

      await sb.from('devices').upsert({
        ...updatedDev,
        updated_at: nowIso,
      });
      targetDevice = updatedDev;
    }
  }

  const firmwares = await getFirmwareReleases();
  let matchedOta: FirmwareRelease | null = null;

  if (targetDevice.target_ota_ver) {
    matchedOta = firmwares.find((f) => f.version === targetDevice.target_ota_ver && f.is_active) || null;
  } else {
    matchedOta =
      firmwares.find((f) => {
        if (!f.is_active) return false;
        if (f.target_devices === 'all') return true;
        const ids = f.target_devices
          .split(',')
          .map((s) => parseInt(s.trim(), 10))
          .filter((n) => !isNaN(n));
        return ids.includes(entry.did);
      }) || null;
  }

  if (matchedOta && entry.fw_version && entry.fw_version === matchedOta.version) {
    matchedOta = null;
  }

  return { log: newLog, device: targetDevice, otaRelease: matchedOta };
}

export async function getFirmwareReleases(): Promise<FirmwareRelease[]> {
  const sb = getSupabaseAdmin();
  if (sb) {
    const { data, error } = await sb
      .from('firmware_releases')
      .select('*')
      .order('created_at', { ascending: false });
    if (!error && data) {
      return data as FirmwareRelease[];
    }
  }
  return readLocalStore().firmwares;
}

export async function addFirmwareRelease(
  release: Omit<FirmwareRelease, 'id' | 'created_at'>
): Promise<FirmwareRelease> {
  const nowIso = new Date().toISOString();
  const local = readLocalStore();

  local.firmwares = local.firmwares.filter((f) => f.version !== release.version);
  const newFw: FirmwareRelease = {
    ...release,
    id: local.nextFwId++,
    created_at: nowIso,
  };
  local.firmwares.unshift(newFw);
  writeLocalStore(local);

  const sb = getSupabaseAdmin();
  if (sb) {
    const { data, error } = await sb
      .from('firmware_releases')
      .upsert(
        {
          ...release,
          created_at: nowIso,
        },
        { onConflict: 'version' }
      )
      .select()
      .single();
    if (!error && data) {
      return data as FirmwareRelease;
    }
  }

  return newFw;
}

export async function deleteFirmwareRelease(version: string): Promise<void> {
  const local = readLocalStore();
  local.firmwares = local.firmwares.filter((f) => f.version !== version);
  writeLocalStore(local);

  const sb = getSupabaseAdmin();
  if (sb) {
    await sb.from('firmware_releases').delete().eq('version', version);
  }
}
