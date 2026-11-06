-- ============================================================================
-- Supabase SQL Schema สำหรับระบบ Cow-Stress-Tracker IoT Dashboard & OTA Manager
-- วิธีใช้: คัดลอกโค้ดทั้งหมดนี้ไปวางใน Supabase Dashboard -> SQL Editor แล้วกด Run
-- ============================================================================

-- 1. ตารางเก็บสถานะและการตั้งค่าของแต่ละอุปกรณ์ (Device ID 1 - 20)
CREATE TABLE IF NOT EXISTS public.devices (
  did INTEGER PRIMARY KEY CHECK (did >= 1 AND did <= 20),
  name TEXT NOT NULL DEFAULT '',
  last_seen TIMESTAMPTZ,
  last_act INTEGER DEFAULT 0,
  last_ang_x DOUBLE PRECISION DEFAULT 0,
  last_ang_y DOUBLE PRECISION DEFAULT 0,
  last_ang_z DOUBLE PRECISION DEFAULT 0,
  last_stand INTEGER DEFAULT 1,
  last_sleep INTEGER DEFAULT 0,
  last_temp DOUBLE PRECISION DEFAULT 0,
  last_rssi INTEGER DEFAULT 0,
  fw_version TEXT DEFAULT '1.0.0',
  last_otp_valid BOOLEAN DEFAULT true,
  total_packets INTEGER DEFAULT 0,
  send_sec INTEGER DEFAULT 60 CHECK (send_sec >= 15 AND send_sec <= 3600),
  target_ota_ver TEXT DEFAULT '',
  updated_at TIMESTAMPTZ DEFAULT NOW()
);

-- เติมข้อมูลเริ่มต้นให้ครบ Device 1 - 20
INSERT INTO public.devices (did, name, send_sec)
SELECT i, 'Cow Tracker #' || i, 60
FROM generate_series(1, 20) AS s(i)
ON CONFLICT (did) DO NOTHING;

-- 2. ตารางเก็บประวัติการส่งข้อมูลทั้งหมด (รวมถึงกรณีที่รหัส OTP ไม่ผ่าน เพื่อใช้เช็กว่าเน็ตเชื่อมต่อได้ไหม)
CREATE TABLE IF NOT EXISTS public.telemetry_logs (
  id BIGSERIAL PRIMARY KEY,
  did INTEGER NOT NULL,
  act INTEGER NOT NULL DEFAULT 0,
  ang_x DOUBLE PRECISION NOT NULL DEFAULT 0,
  ang_y DOUBLE PRECISION NOT NULL DEFAULT 0,
  ang_z DOUBLE PRECISION NOT NULL DEFAULT 0,
  stand INTEGER NOT NULL DEFAULT 1,
  sleep INTEGER NOT NULL DEFAULT 0,
  temp DOUBLE PRECISION NOT NULL DEFAULT 0,
  device_time BIGINT NOT NULL DEFAULT 0,
  server_time BIGINT NOT NULL DEFAULT 0,
  time_drift_sec BIGINT NOT NULL DEFAULT 0,
  otp_received INTEGER NOT NULL DEFAULT 0,
  otp_expected INTEGER NOT NULL DEFAULT 0,
  otp_valid BOOLEAN NOT NULL DEFAULT false,
  fw_version TEXT DEFAULT '',
  rssi INTEGER DEFAULT 0,
  ip_address TEXT DEFAULT '',
  ku_forwarded BOOLEAN DEFAULT false,
  ku_status INTEGER DEFAULT 0,
  raw_query TEXT DEFAULT '',
  created_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_telemetry_logs_did_created ON public.telemetry_logs (did, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_telemetry_logs_created_at ON public.telemetry_logs (created_at DESC);

-- 3. ตารางเก็บไฟล์เฟิร์มแวร์สำหรับอัปเดตผ่านเน็ต (OTA Releases)
CREATE TABLE IF NOT EXISTS public.firmware_releases (
  id BIGSERIAL PRIMARY KEY,
  version TEXT NOT NULL UNIQUE,
  filename TEXT NOT NULL,
  file_url TEXT NOT NULL,
  md5_hash TEXT NOT NULL,
  file_size INTEGER NOT NULL DEFAULT 0,
  notes TEXT DEFAULT '',
  is_active BOOLEAN DEFAULT true,
  target_devices TEXT DEFAULT 'all', -- 'all' หรือระบุเลขเครื่อง เช่น '1,13'
  created_at TIMESTAMPTZ DEFAULT NOW()
);

-- 4. ตารางเก็บการตั้งค่าระบบส่วนกลาง (เช่น การส่งต่อไปเซิร์ฟเวอร์ มก. และรอบส่งเริ่มต้น)
CREATE TABLE IF NOT EXISTS public.system_settings (
  id INTEGER PRIMARY KEY DEFAULT 1 CHECK (id = 1),
  forward_to_ku BOOLEAN DEFAULT false,
  ku_server_url TEXT DEFAULT 'https://sac.eng.kps.ku.ac.th/Dairy-cattle-system-test/IoTDevice/Activity/input.php',
  default_send_sec INTEGER DEFAULT 60,
  strict_otp_check BOOLEAN DEFAULT false, -- false = บันทึกข้อมูลเสมอพร้อมติดป้ายบอกสถานะ OTP, true = ปฏิเสธถ้ารหัสผิด
  updated_at TIMESTAMPTZ DEFAULT NOW()
);

INSERT INTO public.system_settings (id, forward_to_ku, default_send_sec, strict_otp_check)
VALUES (1, false, 60, false)
ON CONFLICT (id) DO NOTHING;

-- 5. สร้าง Storage Bucket สำหรับเก็บไฟล์เฟิร์มแวร์ (.bin)
INSERT INTO storage.buckets (id, name, public)
VALUES ('firmware', 'firmware', true)
ON CONFLICT (id) DO NOTHING;

