// IMU 보정값 — NU40-DK / LSM6DS3TR-C (0x6A)
// 측정: 2026-10-06, 보드 평평·정지. 재보정은 oled_imu_test 스케치에서 시리얼 'c'.
// 정지 |a| = 1.033g, 노이즈 0.0031g (안정적으로 보정됨)
#pragma once

// 가속도 바이어스(g) — 평평할 때 (0,0,1g) 기준. (센서 자체 바이어스, 작음)
#define IMU_ACC_OFF_X   -0.01670f
#define IMU_ACC_OFF_Y   -0.01478f
#define IMU_ACC_OFF_Z    0.03308f

// 자이로 제로레이트 바이어스(dps) — 방향 무관, 정지 시 이 값만큼 빼면 0.
#define IMU_GYR_OFF_X    1.3661f
#define IMU_GYR_OFF_Y   -3.7556f
#define IMU_GYR_OFF_Z    0.1996f

// |a| 스케일 보정(선택): 정지 |a|가 1.033g라 1/1.033.  mag *= IMU_ACC_SCALE → 정지 시 1.00g
#define IMU_ACC_SCALE    0.968f

// 흔들기 판정: |a|-1g 가 이보다 크면 '피크'. 노이즈 대비 충분히 큼(권장 시작값, 단계6에서 튜닝)
#define SHAKE_THRESHOLD  0.35f

// 참고: 키링은 사용 중 방향이 제각각이므로 흔들기 감지는 축별 오프셋이 아니라
//       방향 무관한 |가속도| 크기(= sqrt(ax^2+ay^2+az^2))의 1g 이탈로 판정한다.
//       위 가속도 오프셋은 센서 건강 확인/정밀용이며 흔들기엔 거의 영향 없음.
