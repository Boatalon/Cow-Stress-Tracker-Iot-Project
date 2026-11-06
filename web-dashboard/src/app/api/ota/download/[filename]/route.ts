import { NextRequest, NextResponse } from 'next/server';
import fs from 'fs';
import path from 'path';
import { LOCAL_FIRMWARE_DIR } from '@/lib/store';

export const dynamic = 'force-dynamic';

export async function GET(
  _req: NextRequest,
  context: { params: Promise<{ filename: string }> }
) {
  const { filename } = await context.params;
  const safeName = path.basename(filename);
  const filePath = path.join(LOCAL_FIRMWARE_DIR, safeName);

  if (!fs.existsSync(filePath)) {
    return NextResponse.json({ error: 'Firmware binary not found' }, { status: 404 });
  }

  const buffer = fs.readFileSync(filePath);
  return new NextResponse(buffer, {
    status: 200,
    headers: {
      'Content-Type': 'application/octet-stream',
      'Content-Length': String(buffer.length),
      'Content-Disposition': `attachment; filename="${safeName}"`,
    },
  });
}

