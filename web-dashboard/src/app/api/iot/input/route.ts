import { NextRequest, NextResponse } from 'next/server';
import { generateOtpForDevice, isValidDeviceId } from '@/lib/otp';
import { getSystemSettings, recordTelemetry } from '@/lib/store';

export const dynamic = 'force-dynamic';

async function handleTelemetryRequest(req: NextRequest) {
  const url = new URL(req.url);
  const sp = url.searchParams;

  const getParam = (key: string): string => {
    return sp.get(key) ?? sp.get(key.toLowerCase()) ?? sp.get(key.toUpperCase()) ?? '';
  };

  const didRaw = parseInt(getParam('DID') || '0', 10);
  const did = isValidDeviceId(didRaw) ? didRaw : 1;
  const act = parseInt(getParam('ACT') || '0', 10) || 0;
  const angX = parseFloat(getParam('ANGX') || '0') || 0;
  const angY = parseFloat(getParam('ANGY') || '0') || 0;
  const angZ = parseFloat(getParam('ANGZ') || '0') || 0;
  const gxRaw = getParam('GX');
  const gyRaw = getParam('GY');
  const gzRaw = getParam('GZ');
  const tiltRaw = getParam('Tilt');
  const gx = gxRaw !== '' ? parseFloat(gxRaw) : undefined;
  const gy = gyRaw !== '' ? parseFloat(gyRaw) : undefined;
  const gz = gzRaw !== '' ? parseFloat(gzRaw) : undefined;
  const tilt = tiltRaw !== '' ? parseFloat(tiltRaw) : undefined;

  const stand = parseInt(getParam('Stand') || '1', 10);
  const sleep = parseInt(getParam('Sleep') || (stand === 1 ? '0' : '1'), 10);
  const temp = parseFloat(getParam('Temp') || '0') || 0;
  const timeStr = (getParam('Time') || '0').trim();
  const codeReceived = parseInt(getParam('Code') || '-1', 10);
  const fwVersion = (getParam('FW') || '').trim();
  const rssi = parseInt(getParam('RSSI') || '0', 10) || 0;

  const serverEpoch = Math.floor(Date.now() / 1000);
  const deviceEpoch = parseInt(timeStr, 10) || 0;
  const timeDriftSec = deviceEpoch > 0 ? serverEpoch - deviceEpoch : 0;

  const { otp: otpExpected } = generateOtpForDevice(timeStr, did);
  const otpValid = isValidDeviceId(didRaw) && timeStr.length >= 10 && codeReceived === otpExpected;

  const settings = await getSystemSettings();

  let kuForwarded = false;
  let kuStatus = 0;
  if (settings.forward_to_ku && settings.ku_server_url) {
    try {
      const forwardParams = new URLSearchParams({
        DID: String(did),
        ACT: String(act),
        ANGX: String(angX),
        ANGY: String(angY),
        ANGZ: String(angZ),
        Stand: String(stand),
        Sleep: String(sleep),
        Temp: String(temp),
        Time: timeStr,
        Code: String(codeReceived),
      });
      const targetKuUrl = `${settings.ku_server_url}?${forwardParams.toString()}`;
      const kuRes = await fetch(targetKuUrl, {
        method: 'GET',
        signal: AbortSignal.timeout(5000),
      });
      kuForwarded = kuRes.ok;
      kuStatus = kuRes.status;
    } catch {
      kuForwarded = false;
      kuStatus = 504;
    }
  }

  const ipAddress =
    req.headers.get('x-forwarded-for')?.split(',')[0]?.trim() ||
    req.headers.get('x-real-ip') ||
    'unknown';

  const { device, otaRelease } = await recordTelemetry({
    did,
    act,
    ang_x: angX,
    ang_y: angY,
    ang_z: angZ,
    gx: gx !== undefined && !isNaN(gx) ? gx : undefined,
    gy: gy !== undefined && !isNaN(gy) ? gy : undefined,
    gz: gz !== undefined && !isNaN(gz) ? gz : undefined,
    tilt: tilt !== undefined && !isNaN(tilt) ? tilt : undefined,
    stand: isNaN(stand) ? 1 : stand,
    sleep: isNaN(sleep) ? 0 : sleep,
    temp,
    device_time: deviceEpoch,
    server_time: serverEpoch,
    time_drift_sec: timeDriftSec,
    otp_received: isNaN(codeReceived) ? -1 : codeReceived,
    otp_expected: otpExpected,
    otp_valid: otpValid,
    fw_version: fwVersion || '1.0.0',
    rssi,
    ip_address: ipAddress,
    ku_forwarded: kuForwarded,
    ku_status: kuStatus,
    raw_query: url.search,
  });

  const effectiveSendSec = device.continuous_mode
    ? 5
    : device.send_sec || settings.default_send_sec || 60;

  if (settings.strict_otp_check && !otpValid) {
    return NextResponse.json(
      {
        ok: false,
        error: 'INVALID_OTP',
        otp_valid: false,
        did,
        server_time: serverEpoch,
        send_sec: effectiveSendSec,
      },
      { status: 401 }
    );
  }

  return NextResponse.json(
    {
      ok: true,
      otp_valid: otpValid,
      did,
      server_time: serverEpoch,
      send_sec: effectiveSendSec,
      cont_mode: device.continuous_mode ? 1 : 0,
      cal_seq: device.cal_seq || 0,
      ref_gx: Number((device.ref_gx ?? 0).toFixed(4)),
      ref_gy: Number((device.ref_gy ?? 0).toFixed(4)),
      ref_gz: Number((device.ref_gz ?? 1).toFixed(4)),
      trig: Number((device.step_trig ?? 0.25).toFixed(2)),
      rst: Number((device.step_rst ?? 0.12).toFixed(2)),
      lie: Number((device.lie_ang ?? 45.0).toFixed(1)),
      std: Number((device.stand_ang ?? 30.0).toFixed(1)),
      ota_ver: otaRelease ? otaRelease.version : '',
      ota_url: otaRelease ? otaRelease.file_url : '',
      ota_md5: otaRelease ? otaRelease.md5_hash : '',
    },
    { status: 200 }
  );
}

export async function GET(req: NextRequest) {
  return handleTelemetryRequest(req);
}

export async function POST(req: NextRequest) {
  return handleTelemetryRequest(req);
}
