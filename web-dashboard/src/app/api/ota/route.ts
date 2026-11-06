import { NextRequest, NextResponse } from 'next/server';
import crypto from 'crypto';
import fs from 'fs';
import path from 'path';
import {
  addFirmwareRelease,
  deleteFirmwareRelease,
  getSupabaseAdmin,
  LOCAL_FIRMWARE_DIR,
  updateDeviceConfig,
} from '@/lib/store';

export const dynamic = 'force-dynamic';

export async function POST(req: NextRequest) {
  try {
    const formData = await req.formData();
    const adminPass = process.env.ADMIN_PASSCODE || '';
    const passcode = String(formData.get('passcode') || '');
    if (adminPass && passcode !== adminPass) {
      return NextResponse.json(
        { ok: false, error: 'รหัสผ่านผู้ดูแล (ADMIN_PASSCODE) ไม่ถูกต้อง' },
        { status: 403 }
      );
    }

    const file = formData.get('file') as File | null;
    const version = String(formData.get('version') || '').trim();
    const notes = String(formData.get('notes') || '').trim();
    const targetDevices = String(formData.get('target_devices') || 'all').trim();

    if (!file || !version) {
      return NextResponse.json(
        { ok: false, error: 'กรุณาเลือกไฟล์เฟิร์มแวร์ (.bin) และระบุเลขเวอร์ชัน' },
        { status: 400 }
      );
    }

    const arrayBuffer = await file.arrayBuffer();
    const buffer = Buffer.from(arrayBuffer);
    const md5Hash = crypto.createHash('md5').update(buffer).digest('hex');
    const safeFilename = `cow-tracker-v${version.replace(/[^a-zA-Z0-9._-]/g, '_')}.bin`;

    // 1. บันทึกลงโฟลเดอร์ชั่วคราวในเครื่อง/Serverless เผื่อดาวน์โหลดผ่าน /api/ota/download/
    if (!fs.existsSync(LOCAL_FIRMWARE_DIR)) {
      fs.mkdirSync(LOCAL_FIRMWARE_DIR, { recursive: true });
    }
    fs.writeFileSync(path.join(LOCAL_FIRMWARE_DIR, safeFilename), buffer);

    const origin = new URL(req.url).origin;
    let fileUrl = `${origin}/api/ota/download/${encodeURIComponent(safeFilename)}`;

    // 2. ถ้าเชื่อมต่อ Supabase แล้ว ให้อัปโหลดขึ้น Supabase Storage Bucket 'firmware'
    const sb = getSupabaseAdmin();
    if (sb) {
      const { error: uploadErr } = await sb.storage
        .from('firmware')
        .upload(safeFilename, buffer, {
          contentType: 'application/octet-stream',
          upsert: true,
        });

      if (!uploadErr) {
        const { data: pubData } = sb.storage.from('firmware').getPublicUrl(safeFilename);
        if (pubData?.publicUrl) {
          fileUrl = pubData.publicUrl;
        }
      }
    }

    const release = await addFirmwareRelease({
      version,
      filename: safeFilename,
      file_url: fileUrl,
      md5_hash: md5Hash,
      file_size: buffer.length,
      notes,
      is_active: true,
      target_devices: targetDevices || 'all',
    });

    // อัปเดต target_ota_ver ให้เครื่องเป้าหมายอัตโนมัติ
    if (targetDevices === 'all') {
      await updateDeviceConfig(0, { target_ota_ver: version });
    } else {
      const ids = targetDevices
        .split(',')
        .map((s) => parseInt(s.trim(), 10))
        .filter((n) => !isNaN(n) && n >= 1 && n <= 20);
      for (const id of ids) {
        await updateDeviceConfig(id, { target_ota_ver: version });
      }
    }

    return NextResponse.json({ ok: true, release });
  } catch (err) {
    return NextResponse.json(
      { ok: false, error: err instanceof Error ? err.message : 'Upload failed' },
      { status: 500 }
    );
  }
}

export async function DELETE(req: NextRequest) {
  const url = new URL(req.url);
  const version = url.searchParams.get('version') || '';
  if (!version) {
    return NextResponse.json({ ok: false, error: 'Missing version' }, { status: 400 });
  }
  await deleteFirmwareRelease(version);
  return NextResponse.json({ ok: true });
}

