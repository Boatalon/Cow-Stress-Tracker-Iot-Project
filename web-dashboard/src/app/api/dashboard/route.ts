import { NextRequest, NextResponse } from 'next/server';
import { generateOtpForDevice, getDeviceTable } from '@/lib/otp';
import {
  DeviceConfigUpdates,
  getDevices,
  getFirmwareReleases,
  getRecentLogs,
  getSystemSettings,
  isSupabaseConfigured,
  updateDeviceConfig,
  updateSystemSettings,
} from '@/lib/store';

export const dynamic = 'force-dynamic';

export async function GET(req: NextRequest) {
  const url = new URL(req.url);
  const didFilter = parseInt(url.searchParams.get('did') || '0', 10) || undefined;

  const [devices, logs, firmwares, settings] = await Promise.all([
    getDevices(),
    getRecentLogs(80, didFilter),
    getFirmwareReleases(),
    getSystemSettings(),
  ]);

  const { source: keySource } = getDeviceTable();

  return NextResponse.json({
    ok: true,
    supabaseConnected: isSupabaseConfigured(),
    keySource,
    serverTime: Math.floor(Date.now() / 1000),
    devices,
    logs,
    firmwares,
    settings,
  });
}

export async function POST(req: NextRequest) {
  try {
    const body = await req.json();
    const action = body.action as string;

    if (action === 'generate_otp') {
      const did = Number(body.did) || 6;
      const timeStr = String(body.time || Math.floor(Date.now() / 1000));
      const { otp, deviceId, keySource } = generateOtpForDevice(timeStr, did);
      return NextResponse.json({
        ok: true,
        did: deviceId,
        time: timeStr,
        otp,
        keySource,
      });
    }

    const adminPass = process.env.ADMIN_PASSCODE || '';
    if (adminPass && body.passcode !== adminPass) {
      return NextResponse.json(
        { ok: false, error: 'รหัสผ่านผู้ดูแล (ADMIN_PASSCODE) ไม่ถูกต้อง' },
        { status: 403 }
      );
    }

    if (action === 'update_settings') {
      const updated = await updateSystemSettings({
        forward_to_ku: Boolean(body.forward_to_ku),
        ku_server_url: body.ku_server_url,
        default_send_sec: Math.min(3600, Math.max(5, Number(body.default_send_sec) || 60)),
        strict_otp_check: Boolean(body.strict_otp_check),
      });
      return NextResponse.json({ ok: true, settings: updated });
    }

    if (action === 'update_device') {
      const did = Number(body.did) || 0;
      const updates: DeviceConfigUpdates = {};
      if (body.name !== undefined) updates.name = String(body.name);
      if (body.send_sec !== undefined) {
        updates.send_sec = Math.min(3600, Math.max(5, Number(body.send_sec) || 60));
      }
      if (body.target_ota_ver !== undefined) {
        updates.target_ota_ver = String(body.target_ota_ver);
      }
      if (body.continuous_mode !== undefined) {
        updates.continuous_mode = Boolean(body.continuous_mode);
      }
      await updateDeviceConfig(did, updates);
      return NextResponse.json({ ok: true });
    }

    if (action === 'calibrate_device') {
      const did = Number(body.did) || 6;
      const devices = await getDevices();
      const targetDev = devices.find((d) => d.did === did) || devices[0];
      const nextSeq = (targetDev.cal_seq || 0) + 1;
      const mode = String(body.mode || 'use_current'); // 'use_current' | 'board_sample' | 'params_only'

      const updates: DeviceConfigUpdates = {
        cal_seq: nextSeq,
      };

      if (mode === 'use_current') {
        let gx = Number(targetDev.last_gx);
        let gy = Number(targetDev.last_gy);
        let gz = Number(targetDev.last_gz);
        const norm = Math.sqrt(gx * gx + gy * gy + gz * gz);
        if (norm > 0.01) {
          gx = Number((gx / norm).toFixed(4));
          gy = Number((gy / norm).toFixed(4));
          gz = Number((gz / norm).toFixed(4));
        } else {
          gx = 0;
          gy = 0;
          gz = 1;
        }
        updates.ref_gx = gx;
        updates.ref_gy = gy;
        updates.ref_gz = gz;
      } else if (mode === 'board_sample') {
        // ส่ง (0,0,0) เป็นสัญญาณให้บอร์ดวัดค่าเฉลี่ย 20 ตัวอย่างสดๆ จากเซ็นเซอร์ตัวเองแล้วบันทึกเป็นท่ายืน
        updates.ref_gx = 0;
        updates.ref_gy = 0;
        updates.ref_gz = 0;
      } else if (body.ref_gx !== undefined && body.ref_gy !== undefined && body.ref_gz !== undefined) {
        updates.ref_gx = Number(body.ref_gx);
        updates.ref_gy = Number(body.ref_gy);
        updates.ref_gz = Number(body.ref_gz);
      }

      if (body.step_trig !== undefined) updates.step_trig = Math.max(0.05, Math.min(2.0, Number(body.step_trig)));
      if (body.step_rst !== undefined) updates.step_rst = Math.max(0.02, Math.min(1.5, Number(body.step_rst)));
      if (body.lie_ang !== undefined) updates.lie_ang = Math.max(15, Math.min(85, Number(body.lie_ang)));
      if (body.stand_ang !== undefined) updates.stand_ang = Math.max(5, Math.min(75, Number(body.stand_ang)));
      if (body.continuous_mode !== undefined) updates.continuous_mode = Boolean(body.continuous_mode);

      await updateDeviceConfig(did, updates);
      return NextResponse.json({ ok: true, cal_seq: nextSeq, updates });
    }

    return NextResponse.json({ ok: false, error: 'Unknown action' }, { status: 400 });
  } catch (err) {
    return NextResponse.json(
      { ok: false, error: err instanceof Error ? err.message : 'Internal error' },
      { status: 500 }
    );
  }
}
