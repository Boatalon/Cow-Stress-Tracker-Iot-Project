import type { Metadata } from 'next';
import './globals.css';

export const metadata: Metadata = {
  title: 'Cow Stress Tracker — IoT Connectivity & OTA Dashboard',
  description:
    'ระบบตรวจสอบการเชื่อมต่ออินเทอร์เน็ต การส่งข้อมูลด้วยรหัสลับ OTP และอัปเดตเฟิร์มแวร์ประหยัดพลังงาน (OTA) สำหรับ Cow-Stress-Tracker',
};

export default function RootLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <html lang="th">
      <body className="min-h-screen antialiased selection:bg-emerald-500 selection:text-white">
        {children}
      </body>
    </html>
  );
}

