#ifndef OTP_ACTIVITY_H
#define OTP_ACTIVITY_H

#include <Arduino.h>

//==================== OTP LIBRARY สำหรับ Activity Sensor ====================
// เวอร์ชัน WiFiManager: เลือก Device ID ตอนรันไทม์ (จากหน้าเว็บตั้งค่า)
// รหัสลับของแต่ละอุปกรณ์ถูกแยกไว้ในไฟล์ OTP_Secrets.h (ซึ่งถูกซ่อนโดย .gitignore)

const int MAX_DEVICE_ID = 20;

struct DeviceKey {
  int code1;
  int code2;
  int prime;
};

#if __has_include("OTP_Secrets.h")
  #include "OTP_Secrets.h"
#else
  #warning "ไม่พบไฟล์ OTP_Secrets.h -> ใช้ค่าจำลอง (Dummy Keys) สำหรับทดสอบคอมไพล์ กรุณาคัดลอก OTP_Secrets.example.h เป็น OTP_Secrets.h"
  const DeviceKey DEVICE_TABLE[MAX_DEVICE_ID] = {
      {1001, 2001, 7001}, {1002, 2002, 7001}, {1003, 2003, 7001}, {1004, 2004, 7001},
      {1005, 2005, 7001}, {1006, 2006, 7001}, {1007, 2007, 7001}, {1008, 2008, 7001},
      {1009, 2009, 7001}, {1010, 2010, 7001}, {1011, 2011, 7001}, {1012, 2012, 7001},
      {1013, 2013, 7001}, {1014, 2014, 7001}, {1015, 2015, 7001}, {1016, 2016, 7001},
      {1017, 2017, 7001}, {1018, 2018, 7001}, {1019, 2019, 7001}, {1020, 2020, 7001},
  };
#endif

inline bool isValidDeviceId(int id) {
  return id >= 1 && id <= MAX_DEVICE_ID;
}

// คืนค่า Key ของ Device ที่เลือก (ถ้า ID ไม่ถูกต้อง ใช้ Device 1)
inline DeviceKey getDeviceKey(int id) {
  if (!isValidDeviceId(id)) id = 1;
  return DEVICE_TABLE[id - 1];
}

int calCode(int y, int a, int b, int p) {
  int a1 = (y % p) * (a % p);
  if (a1 < 0)
    a1 = (p - ((-1 * a1) % p)) % p;
  else
    a1 = a1 % p;
  if (b < 0)
    b = (p - ((-1 * b) % p)) % p;
  else
    b = b % p;

  if (a1 % b == 0) {
    return (a1 / b) % p; // positive & mod=0
  } else {
    int i;
    for (i = 0; i < p; i++)
      if ((i * b) % p == a1)
        return i;
    return 0;
  }
  return 0;
}

int myAtoI(char c) {
  switch (c) {
  case '0':
    return 0;
  case '1':
    return 1;
  case '2':
    return 2;
  case '3':
    return 3;
  case '4':
    return 4;
  case '5':
    return 5;
  case '6':
    return 6;
  case '7':
    return 7;
  case '8':
    return 8;
  case '9':
    return 9;
  }
  return 0;
}

int genCode(String tt_str, int id, int code1, int code2, int p) {
  char tt[15];
  tt_str.toCharArray(tt, 15);

  int y1, y2, a[5], b[5], x5;
  int i, j, k, l;

  l = 10;
  y1 = 0;
  y2 = 0;
  x5 = 29;
  for (i = 0; i < l; i++) {
    k = myAtoI(tt[i]);
    if (i < 5)
      y1 = y1 * 11 + k;
    else
      y2 = y2 * 13 + k;
    x5 += k;
  }

  for (i = 1; i <= 4; i++) {
    a[i] = 1;
    b[i] = 1;
    for (j = 1; j <= 4; j++) {
      if (i != j) {
        a[i] *= x5 - j;
        b[i] *= i - j;
      }
    }
  }

  int otp =
      (calCode(y1, a[1] % p, b[1], p) + calCode(y2, a[2] % p, b[2], p) +
       calCode(code1, a[3] % p, b[3], p) + calCode(code2, a[4] % p, b[4], p)) %
      p;
  return otp;
}

#endif
