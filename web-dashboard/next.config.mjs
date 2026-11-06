/** @type {import('next').NextConfig} */
const nextConfig = {
  async rewrites() {
    return [
      // รองรับ Path เดิมของเซิร์ฟเวอร์ มก. เพื่อให้เปลี่ยนแค่ชื่อโดเมน server = "ชื่อเรา.vercel.app" แล้วใช้งานได้ทันที
      {
        source: '/Dairy-cattle-system-test/IoTDevice/Activity/input.php',
        destination: '/api/iot/input',
      },
      {
        source: '/input.php',
        destination: '/api/iot/input',
      },
    ];
  },
};

export default nextConfig;

