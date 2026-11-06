import fs from 'fs';
import path from 'path';

export interface DeviceKey {
  code1: number;
  code2: number;
  prime: number;
}

export const MAX_DEVICE_ID = 20;

const DUMMY_KEYS: DeviceKey[] = Array.from({ length: MAX_DEVICE_ID }, (_, idx) => ({
  code1: 1001 + idx,
  code2: 2001 + idx,
  prime: 7001,
}));

let cachedKeys: DeviceKey[] | null = null;

/**
 * โหลดตารางรหัสลับ OTP (Device 1 - 20)
 * ลำดับการโหลด:
 * 1. Environment Variable `OTP_DEVICE_TABLE` (สำหรับตอน Deploy บน Vercel หรือจาก .env.local)
 * 2. อ่านจากไฟล์ `../Cow-Stress-Tracker/OTP_Secrets.h` อัตโนมัติ (กรณีรันในเครื่องเดียวกัน)
 * 3. ค่าจำลอง (Dummy Keys) กรณีไม่มีทั้ง 1 และ 2
 */
export function getDeviceTable(): { keys: DeviceKey[]; source: 'env' | 'secrets_header' | 'dummy' } {
  if (process.env.OTP_DEVICE_TABLE) {
    try {
      const parsed = JSON.parse(process.env.OTP_DEVICE_TABLE);
      if (Array.isArray(parsed) && parsed.length >= MAX_DEVICE_ID) {
        const keys = parsed.slice(0, MAX_DEVICE_ID).map((item: number[] | DeviceKey) => {
          if (Array.isArray(item)) {
            return { code1: Number(item[0]), code2: Number(item[1]), prime: Number(item[2]) };
          }
          return { code1: Number(item.code1), code2: Number(item.code2), prime: Number(item.prime) };
        });
        return { keys, source: 'env' };
      }
    } catch {
      // fallback to file check
    }
  }

  if (cachedKeys) {
    return { keys: cachedKeys, source: 'secrets_header' };
  }

  try {
    const headerPath = path.resolve(process.cwd(), '../Cow-Stress-Tracker/OTP_Secrets.h');
    if (fs.existsSync(headerPath)) {
      const content = fs.readFileSync(headerPath, 'utf8');
      const regex = /\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}/g;
      const found: DeviceKey[] = [];
      let match: RegExpExecArray | null;
      while ((match = regex.exec(content)) !== null) {
        found.push({
          code1: parseInt(match[1], 10),
          code2: parseInt(match[2], 10),
          prime: parseInt(match[3], 10),
        });
      }
      if (found.length >= MAX_DEVICE_ID) {
        cachedKeys = found.slice(0, MAX_DEVICE_ID);
        return { keys: cachedKeys, source: 'secrets_header' };
      }
    }
  } catch {
    // ignore fs error in serverless environment
  }

  return { keys: DUMMY_KEYS, source: 'dummy' };
}

export function isValidDeviceId(id: number): boolean {
  return Number.isInteger(id) && id >= 1 && id <= MAX_DEVICE_ID;
}

export function getDeviceKey(id: number): DeviceKey {
  const { keys } = getDeviceTable();
  const validId = isValidDeviceId(id) ? id : 1;
  return keys[validId - 1];
}

// ฟังก์ชันการหารและ mod แบบ C 32-bit signed int ทุกประการ
function cRem(a: number, b: number): number {
  return (a | 0) % (b | 0) | 0;
}

function cDiv(a: number, b: number): number {
  return Math.trunc((a | 0) / (b | 0)) | 0;
}

export function calCode(y: number, a: number, b: number, p: number): number {
  y = y | 0;
  a = a | 0;
  b = b | 0;
  p = p | 0;

  let a1 = Math.imul(cRem(y, p), cRem(a, p));
  if (a1 < 0) {
    a1 = cRem(p - cRem(Math.imul(-1, a1), p), p);
  } else {
    a1 = cRem(a1, p);
  }

  if (b < 0) {
    b = cRem(p - cRem(Math.imul(-1, b), p), p);
  } else {
    b = cRem(b, p);
  }

  if (b === 0) return 0;

  if (cRem(a1, b) === 0) {
    return cRem(cDiv(a1, b), p);
  } else {
    for (let i = 0; i < p; i++) {
      if (cRem(Math.imul(i, b), p) === a1) {
        return i;
      }
    }
    return 0;
  }
}

function myAtoI(c: string | undefined): number {
  if (!c || c.length === 0) return 0;
  const code = c.charCodeAt(0);
  if (code >= 48 && code <= 57) {
    return code - 48;
  }
  return 0;
}

/**
 * พอร์ตฟังก์ชัน genCode() จาก OTP_Activity.h แบบ 100% Bit-Exact
 */
export function genCode(ttStr: string, id: number, code1: number, code2: number, p: number): number {
  const l = 10;
  let y1 = 0;
  let y2 = 0;
  let x5 = 29;

  for (let i = 0; i < l; i++) {
    const k = myAtoI(ttStr[i]);
    if (i < 5) {
      y1 = (Math.imul(y1, 11) + k) | 0;
    } else {
      y2 = (Math.imul(y2, 13) + k) | 0;
    }
    x5 = (x5 + k) | 0;
  }

  const a = [0, 1, 1, 1, 1];
  const b = [0, 1, 1, 1, 1];

  for (let i = 1; i <= 4; i++) {
    a[i] = 1;
    b[i] = 1;
    for (let j = 1; j <= 4; j++) {
      if (i !== j) {
        a[i] = Math.imul(a[i], (x5 - j) | 0);
        b[i] = Math.imul(b[i], (i - j) | 0);
      }
    }
  }

  const term1 = calCode(y1, cRem(a[1], p), b[1], p);
  const term2 = calCode(y2, cRem(a[2], p), b[2], p);
  const term3 = calCode(code1, cRem(a[3], p), b[3], p);
  const term4 = calCode(code2, cRem(a[4], p), b[4], p);

  const otp = cRem((term1 + term2 + term3 + term4) | 0, p);
  return otp;
}

export function generateOtpForDevice(timestampStr: string, deviceId: number): {
  otp: number;
  deviceId: number;
  keySource: 'env' | 'secrets_header' | 'dummy';
} {
  const { keys, source } = getDeviceTable();
  const validId = isValidDeviceId(deviceId) ? deviceId : 1;
  const key = keys[validId - 1];
  const otp = genCode(String(timestampStr), validId, key.code1, key.code2, key.prime);
  return { otp, deviceId: validId, keySource: source };
}

