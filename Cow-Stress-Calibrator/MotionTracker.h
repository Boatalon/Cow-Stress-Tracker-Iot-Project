#ifndef MOTION_TRACKER_H
#define MOTION_TRACKER_H

#include <Arduino.h>
#include <math.h>

// ============================================================================
// โครงสร้างพารามิเตอร์สำหรับปรับจูนการนับก้าวและการนอน
// ชื่อตรงกับค่า #define ด้านบนสุดของไฟล์ Cow-Stress-Tracker.ino แบบ 1 ต่อ 1
// ============================================================================
struct MotionParams {
  float         stepTriggerG;        // STEP_TRIGGER_G: แรงกระแทกเริ่มนับก้าว (g)
  float         stepResetG;          // STEP_RESET_G: แรงรีเซ็ตก่อนนับก้าวถัดไป (g)
  unsigned long stepDelayMs;         // STEP_DELAY_MS: ระยะเวลาขั้นต่ำระหว่างก้าว (ms)
  float         smoothAlpha;         // ACCEL_SMOOTH_ALPHA: ตัวกรองสัญญาณความเร่ง (0.10 - 1.00)
  float         lieAngleDeg;         // LIE_ANGLE_DEG: มุมเอียงขาที่ถือว่านอน (องศา)
  float         standAngleDeg;       // STAND_ANGLE_DEG: มุมเอียงขาที่ถือว่ายืน (องศา)
  unsigned long standToLieDelaySec;  // STAND_TO_LIE_DELAY_SEC: หน่วงเวลา ยืน -> นอน (วินาที)
  unsigned long lieToStandDelaySec;  // LIE_TO_STAND_DELAY_SEC: หน่วงเวลา นอน -> ยืน (วินาที)
  float         refGravX;            // REF_GRAV_X: เวกเตอร์แรงโน้มถ่วงท่ายืนอ้างอิง แกน X
  float         refGravY;            // REF_GRAV_Y: เวกเตอร์แรงโน้มถ่วงท่ายืนอ้างอิง แกน Y
  float         refGravZ;            // REF_GRAV_Z: เวกเตอร์แรงโน้มถ่วงท่ายืนอ้างอิง แกน Z
};

struct MotionState {
  // เวกเตอร์แรงโน้มถ่วงปัจจุบัน (แยกด้วย Low-pass filter)
  float gravX;
  float gravY;
  float gravZ;

  // ค่าความเร่งของการขยับขา (หลังหักแรงโน้มถ่วงออกแล้ว)
  float rawDynMag;
  float dynMag;
  float prevTotalMag;

  // มุมเอียงของต้นขาเทียบกับท่ายืนอ้างอิง (0 - 180 องศา)
  float tiltDeg;

  // สถานะท่าทางและการนับการยืน/การนอน
  bool          isStanding;          // true = ยืน (Stand=1, Sleep=0), false = นอน (Stand=0, Sleep=1)
  int           standCount;          // จำนวนครั้งที่ยืน
  int           lieCount;            // จำนวนครั้งที่นอน
  unsigned long standDurationMs;     // เวลายืนสะสม (ms)
  unsigned long lieDurationMs;       // เวลานอนสะสม (ms)

  // ตัวจับเวลาหน่วงการเปลี่ยนท่า (Debounce / Bout Filter)
  bool          pendingTransition;
  bool          pendingTargetStand;
  unsigned long transitionStartMs;
  float         transitionRemainSec;

  // ตัวนับก้าว
  int           stepCount;           // ก้าวที่นับได้ (ล็อกไม่ให้นับตอนนอน)
  int           legacyStepCount;     // ก้าวจากโค้ดวิธีเดิม (ใช้เปรียบเทียบ)
  int           manualStepCount;     // ก้าวที่ผู้ใช้กดนับด้วยตาบนหน้าเว็บ
  bool          stepArmed;
  unsigned long lastStepMs;
  unsigned long lastLegacyStepMs;
  unsigned long lastUpdateMs;
};

inline void defaultMotionParams(MotionParams &p) {
  p.stepTriggerG       = 0.35f;
  p.stepResetG         = 0.15f;
  p.stepDelayMs        = 500UL;
  p.smoothAlpha        = 0.35f;
  p.lieAngleDeg        = 45.0f;
  p.standAngleDeg      = 30.0f;
  p.standToLieDelaySec = 10UL;
  p.lieToStandDelaySec = 5UL;
  p.refGravX           = 0.0f;
  p.refGravY           = 0.0f;
  p.refGravZ           = 1.0f;
}

// คำนวณมุมเอียง (องศา) ระหว่างเวกเตอร์แรงโน้มถ่วงปัจจุบัน กับ ท่ายืนอ้างอิง
inline float computeTiltDegrees(float gx, float gy, float gz,
                                float rx, float ry, float rz) {
  float normG = sqrtf(gx * gx + gy * gy + gz * gz);
  float normR = sqrtf(rx * rx + ry * ry + rz * rz);
  if (normG < 0.001f || normR < 0.001f) return 0.0f;

  float dot = (gx * rx + gy * ry + gz * rz) / (normG * normR);
  if (dot > 1.0f) dot = 1.0f;
  if (dot < -1.0f) dot = -1.0f;
  return acosf(dot) * (180.0f / PI);
}

inline void motionInit(MotionState &st, const MotionParams &p,
                       float ax, float ay, float az, unsigned long nowMs) {
  st.gravX               = ax;
  st.gravY               = ay;
  st.gravZ               = az;
  st.rawDynMag           = 0.0f;
  st.dynMag              = 0.0f;
  st.prevTotalMag        = sqrtf(ax * ax + ay * ay + az * az);
  st.tiltDeg             = computeTiltDegrees(ax, ay, az, p.refGravX, p.refGravY, p.refGravZ);
  st.isStanding          = (st.tiltDeg < p.lieAngleDeg);
  st.standCount          = st.isStanding ? 1 : 0;
  st.lieCount            = st.isStanding ? 0 : 1;
  st.standDurationMs     = 0;
  st.lieDurationMs       = 0;
  st.pendingTransition   = false;
  st.pendingTargetStand  = st.isStanding;
  st.transitionStartMs   = nowMs;
  st.transitionRemainSec = 0.0f;
  st.stepCount           = 0;
  st.legacyStepCount     = 0;
  st.manualStepCount     = 0;
  st.stepArmed           = true;
  st.lastStepMs          = nowMs;
  st.lastLegacyStepMs    = nowMs;
  st.lastUpdateMs        = nowMs;
}

// บันทึกเวกเตอร์แรงโน้มถ่วงปัจจุบันเป็นท่ายืนอ้างอิง (0 องศา)
inline void motionCaptureStandingRef(MotionState &st, MotionParams &p) {
  float norm = sqrtf(st.gravX * st.gravX + st.gravY * st.gravY + st.gravZ * st.gravZ);
  if (norm > 0.01f) {
    p.refGravX = st.gravX / norm;
    p.refGravY = st.gravY / norm;
    p.refGravZ = st.gravZ / norm;
  } else {
    p.refGravX = 0.0f;
    p.refGravY = 0.0f;
    p.refGravZ = 1.0f;
  }
  st.tiltDeg             = 0.0f;
  st.isStanding          = true;
  st.pendingTransition   = false;
  st.transitionRemainSec = 0.0f;
}

inline void motionResetCounters(MotionState &st, unsigned long nowMs) {
  st.stepCount           = 0;
  st.legacyStepCount     = 0;
  st.manualStepCount     = 0;
  st.standCount          = st.isStanding ? 1 : 0;
  st.lieCount            = st.isStanding ? 0 : 1;
  st.standDurationMs     = 0;
  st.lieDurationMs       = 0;
  st.pendingTransition   = false;
  st.transitionRemainSec = 0.0f;
  st.lastUpdateMs        = nowMs;
}

// อัปเดตค่าทุกๆ รอบการอ่านเซ็นเซอร์ (เช่น 50 Hz = ทุก 20 ms)
// คืนค่า true หากตรวจพบการก้าวขาในรอบนี้
inline bool motionUpdate(MotionState &st, const MotionParams &p,
                         float ax, float ay, float az, unsigned long nowMs) {
  unsigned long dtMs = (nowMs >= st.lastUpdateMs) ? (nowMs - st.lastUpdateMs) : 0;
  st.lastUpdateMs = nowMs;

  // สะสมเวลาการยืน / การนอน
  if (dtMs > 0 && dtMs < 5000) {
    if (st.isStanding) st.standDurationMs += dtMs;
    else               st.lieDurationMs   += dtMs;
  }

  // 1. แยกแรงโน้มถ่วงโลกด้วย Low-pass filter (alpha = 0.04 ที่ 50Hz ~ 0.5 วินาที)
  const float gravAlpha = 0.04f;
  st.gravX = (1.0f - gravAlpha) * st.gravX + gravAlpha * ax;
  st.gravY = (1.0f - gravAlpha) * st.gravY + gravAlpha * ay;
  st.gravZ = (1.0f - gravAlpha) * st.gravZ + gravAlpha * az;

  // 2. ลบแรงโน้มถ่วงออก เหลือเฉพาะความเร่งจากการขยับขา (Dynamic Acceleration)
  float dynX = ax - st.gravX;
  float dynY = ay - st.gravY;
  float dynZ = az - st.gravZ;
  st.rawDynMag = sqrtf(dynX * dynX + dynY * dynY + dynZ * dynZ);

  // 3. กรองความเรียบของสัญญาณกระแทกด้วย ACCEL_SMOOTH_ALPHA
  float alpha = constrain(p.smoothAlpha, 0.05f, 1.0f);
  st.dynMag = (1.0f - alpha) * st.dynMag + alpha * st.rawDynMag;

  // 4. คำนวณมุมเอียงของต้นขาเทียบกับท่ายืนอ้างอิง
  st.tiltDeg = computeTiltDegrees(st.gravX, st.gravY, st.gravZ,
                                  p.refGravX, p.refGravY, p.refGravZ);

  // 5. ตรวจสอบการเปลี่ยนท่า (ยืน <-> นอน) พร้อมหน่วงเวลาแยกขาเข้าและขาออก
  if (st.isStanding) {
    // ปัจจุบัน "ยืน" -> เช็กว่าขาเอียงเกิน LIE_ANGLE_DEG ติดต่อกันครบ STAND_TO_LIE_DELAY_SEC หรือยัง
    if (st.tiltDeg >= p.lieAngleDeg) {
      if (!st.pendingTransition || st.pendingTargetStand != false) {
        st.pendingTransition  = true;
        st.pendingTargetStand = false;
        st.transitionStartMs  = nowMs;
      }
      float elapsedSec = (nowMs - st.transitionStartMs) / 1000.0f;
      float delaySec   = (float)p.standToLieDelaySec;
      st.transitionRemainSec = (elapsedSec < delaySec) ? (delaySec - elapsedSec) : 0.0f;

      if (elapsedSec >= delaySec) {
        st.isStanding          = false;
        st.lieCount++;
        st.pendingTransition   = false;
        st.transitionRemainSec = 0.0f;
      }
    } else {
      st.pendingTransition   = false;
      st.transitionRemainSec = 0.0f;
    }
  } else {
    // ปัจจุบัน "นอน" -> เช็กว่าขากลับมาตั้งตรง <= STAND_ANGLE_DEG ครบ LIE_TO_STAND_DELAY_SEC หรือยัง
    if (st.tiltDeg <= p.standAngleDeg) {
      if (!st.pendingTransition || st.pendingTargetStand != true) {
        st.pendingTransition  = true;
        st.pendingTargetStand = true;
        st.transitionStartMs  = nowMs;
      }
      float elapsedSec = (nowMs - st.transitionStartMs) / 1000.0f;
      float delaySec   = (float)p.lieToStandDelaySec;
      st.transitionRemainSec = (elapsedSec < delaySec) ? (delaySec - elapsedSec) : 0.0f;

      if (elapsedSec >= delaySec) {
        st.isStanding          = true;
        st.standCount++;
        st.pendingTransition   = false;
        st.transitionRemainSec = 0.0f;
      }
    } else {
      st.pendingTransition   = false;
      st.transitionRemainSec = 0.0f;
    }
  }

  // 6. การนับก้าวแบบใหม่:
  //    - ต้องลดลงต่ำกว่า STEP_RESET_G ก่อนจึงพร้อมนับก้าวใหม่ (Hysteresis)
  //    - ถ้านอนอยู่ (!st.isStanding) จะล็อกไม่ให้นับก้าวเด็ดขาด
  if (st.dynMag <= p.stepResetG) {
    st.stepArmed = true;
  }

  bool stepDetectedNow = false;
  if (st.isStanding &&
      st.stepArmed &&
      st.dynMag >= p.stepTriggerG &&
      (nowMs - st.lastStepMs >= p.stepDelayMs)) {
    st.stepCount++;
    st.stepArmed    = false;
    st.lastStepMs   = nowMs;
    stepDetectedNow = true;
  }

  // 7. การนับก้าวแบบโค้ดเดิม (ใช้เปรียบเทียบบนหน้าเว็บ)
  float totalMag = sqrtf(ax * ax + ay * ay + az * az);
  float magDelta = fabsf(totalMag - st.prevTotalMag);
  st.prevTotalMag = totalMag;
  if (magDelta > 0.5f && (nowMs - st.lastLegacyStepMs > 500UL)) {
    st.legacyStepCount++;
    st.lastLegacyStepMs = nowMs;
  }

  return stepDetectedNow;
}

#endif

