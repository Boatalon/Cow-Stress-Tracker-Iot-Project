# 🌐 Cow-Stress-Tracker Web Dashboard (Vercel + Supabase + OTA)

เว็บแอปพลิเคชันสำหรับตรวจสอบสถานะการเชื่อมต่ออินเทอร์เน็ตของบอร์ด **Seeed Studio XIAO ESP32-C3**, ตรวจสอบการส่งข้อมูลด้วยรหัสลับ **OTP (API Key เดิมจาก `OTP_Secrets.h`)**, และอัปเดตเฟิร์มแวร์ประหยัดพลังงานผ่านเน็ต (**HTTP OTA Update**)

---

## 🚀 วิธีรันในเครื่อง (Local Development)

```bash
cd web-dashboard
npm install
npm run dev
```
แล้วเปิดเบราว์เซอร์ที่ `http://localhost:3000` (ระบบอ่านรหัสลับ `OTP_Secrets.h` ให้อัตโนมัติ และทำงานได้ทันทีแม้ยังไม่ได้เชื่อม Supabase)

---

## ☁️ วิธีนำขึ้น Vercel + เชื่อมต่อ Supabase (ใช้ชื่อ URL ของเราเอง)

### ขั้นตอนที่ 1: สร้างฐานข้อมูล Supabase (ฟรี)
1. ไปที่ [https://supabase.com](https://supabase.com) สร้างโปรเจกต์ใหม่ (แนะนำเลือก Region: **Southeast Asia (Singapore)** เพื่อความเร็วสูงสุด)
2. ไปที่เมนู **SQL Editor** → คัดลอกโค้ดทั้งหมดจากไฟล์ [`../supabase/schema.sql`](../supabase/schema.sql) ไปวางแล้วกด **Run**
3. ไปที่เมนู **Project Settings → API** แล้วคัดลอกค่า:
   - `Project URL` (`NEXT_PUBLIC_SUPABASE_URL`)
   - `anon public` key (`NEXT_PUBLIC_SUPABASE_ANON_KEY`)
   - `service_role` secret key (`SUPABASE_SERVICE_ROLE_KEY`)

### ขั้นตอนที่ 2: Deploy ขึ้น Vercel และตั้งชื่อ URL ของตัวเอง
1. ไปที่ [https://vercel.com](https://vercel.com) → กด **Add New Project** → เลือก Repository นี้
2. ตั้งค่า **Project Name** เป็นชื่อที่คุณต้องการ (เช่น `cow-tracker-yourname` จะได้ URL เป็น `https://cow-tracker-yourname.vercel.app`)
3. ตั้งค่า **Root Directory** เป็น `web-dashboard`
4. ในหัวข้อ **Environment Variables** ให้เพิ่มตัวแปรดังนี้:
   - `NEXT_PUBLIC_SUPABASE_URL` = URL จาก Supabase
   - `NEXT_PUBLIC_SUPABASE_ANON_KEY` = Anon Key จาก Supabase
   - `SUPABASE_SERVICE_ROLE_KEY` = Service Role Key จาก Supabase
   - `OTP_DEVICE_TABLE` = คัดลอกค่าจากไฟล์ `web-dashboard/.env.local` (ตารางรหัสลับจริงของ Device 1–20)
5. กด **Deploy**

---

## 🐄 วิธีตั้งค่า URL ในบอร์ด ESP32-C3

ใช้รหัสลับ `OTP_Activity.h` และ `OTP_Secrets.h` เดิมทั้งหมด โดยเปลี่ยนแค่ชื่อโดเมนใน [`Cow-Stress-Tracker.ino`](../Cow-Stress-Tracker/Cow-Stress-Tracker.ino) บรรทัดที่ 52–53:

```cpp
#define DEFAULT_SERVER "ชื่อที่คุณตั้ง.vercel.app"
#define DEFAULT_PATH   "/api/iot/input"
```
*(หมายเหตุ: ระบบทำ URL Rewrite รองรับ Path เดิม `/Dairy-cattle-system-test/IoTDevice/Activity/input.php` ไว้ให้ด้วย ดังนั้นหากเปลี่ยนแค่ `DEFAULT_SERVER` อย่างเดียวก็สามารถรับข้อมูลได้ทันที หรือจะเปลี่ยนผ่านหน้าเว็บ WiFiManager `Cow-Stress-Tracker-Config` ตอนกดปุ่ม BOOT ก็ได้เช่นกัน)*

