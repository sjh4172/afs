#include "CommJSBSIM.h"

#include <cmath>
#include <algorithm>

// 전역 rullPose는 다른 cpp에서 정의되어 있어야 합니다.
extern RullPoseMsg rullPose;

// ---------- 상수/유틸 ----------
static inline double clampd(double v, double lo, double hi){
	return std::max(lo, std::min(hi, v));
}

// lat: [-90, +90] → [-1,+1] 정규화 후 Q30 (× 2^30)
static inline int32_t pack_lat_q30(double lat_deg) {
	double x = clampd(lat_deg / 90.0, -1.0, 1.0);
	return static_cast<int32_t>(std::llround(x * 1073741824.0)); // 2^30
}

// lon: [-180, +180] → [-1,+1] 정규화 후 Q31 (× (2^31-1))
static inline int32_t pack_lon_q31(double lon_deg) {
	double x = clampd(lon_deg / 180.0, -1.0, 1.0);
	return static_cast<int32_t>(std::llround(x * 2147483647.0)); // 2^31-1
}

// Q15: [-1, +1] → int16_t (sc 항목)
static inline int16_t pack_q15(double x){
	x = clampd(x, -1.0, 1.0);
	return static_cast<int16_t>(std::lround(x * 32767.0));
}


static inline int16_t pack_q4(double x){
	return static_cast<int16_t>(std::lround(x * 16));
}

// 선형 스케일러: 실수/LSB → int16_t
static inline int16_t pack_s16_from_lsb(double value, double lsb,
	double lo_physical, double hi_physical)
{
	// 물리량 클램프
	value = clampd(value, lo_physical, hi_physical);
	// raw = value / LSB
	const double raw = value / lsb;
	// int16_t 범위 클램프
	const double raw_clamped = clampd(raw, -32768.0, 32767.0);
	return static_cast<int16_t>(std::lround(raw_clamped));
}

// Alt/AGL: 단위 ft, LSB = 4 ft (범위 예: -1060 ~ 80377)
static inline int16_t pack_alt_4ft(double ft){
	return pack_s16_from_lsb(ft, 4.0, -1060.0, 80377.0);
}

// FPM: 단위 ft/min, LSB = 0.5
static inline int16_t pack_fpm(double fpm){
	// 범위는 필요에 따라 조정(여기선 int16 한계에 맡김)
	return pack_s16_from_lsb(fpm, 0.5, -32768.0 * 0.5, 32767.0 * 0.5);
}

// 속도/가속도: LSB = 0.03125 (2^-5)
//  - Velocity: ft/s
//  - Acceleration: ft/s^2
static inline int16_t pack_vel_fts(double fts){
	return pack_s16_from_lsb(fts, 0.03125, -3000.0, 3000.0); // ICD 범위 기준
}
static inline int16_t pack_acc_fts2(double fts2){
	return pack_s16_from_lsb(fts2, 0.03125, -512.0, 512.0);  // ICD 범위 기준
}

// 각도계열(sc): Q15 [-1, +1]
// - Azimuth/TrueHeading: heading(deg) ∈ [0,360) → (-180,180]로 감은 뒤 /180
// - Roll: roll(deg) ∈ [-180,180] → /180
// - Pitch: pitch(deg) ∈ [-90,90] → /90
static inline double wrap180(double deg){
	double x = std::fmod(deg, 360.0);
	if (x <= -180.0) x += 360.0;
	if (x >   180.0) x -= 360.0;
	return x;
}

// 단위 변환
static inline double m_to_ft(double m){ return m * 3.280839895013123; }
static inline double mps_to_fts(double mps){ return mps * 3.280839895013123; }
static inline double mps_to_fpm(double mps){ return mps * 196.850394; }
static inline double mps2_to_fts2(double mps2){ return mps2 * 3.280839895013123; }

void UpdatePoseFromJSBSim(JSBSim::FGFDMExec& fdm)
{
	// ---------- JSBSim에서 double로 읽기 ----------
	const double lat_deg = fdm.GetPropertyValue("position/lat-geod-deg");   // [-90,90]
	const double lon_deg = fdm.GetPropertyValue("position/long-gc-deg");    // [-180,180]

	// ft 단위가 바로 있으면 그걸 쓰는 게 가장 안전
	const double h_msl_ft = fdm.GetPropertyValue("position/h-sl-ft");
	const double h_agl_ft = fdm.GetPropertyValue("position/h-agl-ft");

	const double roll_deg = fdm.GetPropertyValue("attitude/phi-deg");        // [-180,180]
	const double pitch_deg = fdm.GetPropertyValue("attitude/theta-deg");      // [-90,90]
	const double hdg_deg = fdm.GetPropertyValue("attitude/psi-deg");        // [0,360)

	// 속도/가속도 (JSBSim 기본은 m/s, m/s^2)
	const double u_mps = fdm.GetPropertyValue("velocities/u-mps");
	const double v_mps = fdm.GetPropertyValue("velocities/v-mps");
	const double w_mps = fdm.GetPropertyValue("velocities/w-mps");

	const double ax_mps2 = fdm.GetPropertyValue("accelerations/udot-mps2");
	const double ay_mps2 = fdm.GetPropertyValue("accelerations/vdot-mps2");
	const double az_mps2 = fdm.GetPropertyValue("accelerations/wdot-mps2");

	// 등반/수평 속도
	const double climb_mps = fdm.GetPropertyValue("velocities/h-dot-mps"); // +상승
	const double vtrue_mps = fdm.GetPropertyValue("velocities/vtrue-mps");
	const double gamma_deg = fdm.GetPropertyValue("flight-path/gamma-deg");
	const double v_horz_mps = std::isfinite(gamma_deg)
		? vtrue_mps * std::cos(gamma_deg * M_PI / 180.0)
		: vtrue_mps;

	// 속도
	const double speed = fdm.GetPropertyValue("velocities/vtrue-fps");

	// ---------- 스케일 & 패킹 ----------
	rullPose.header = static_cast<int16_t>(0xA5A6); // 필요 시 프로토콜 정의값 사용

	rullPose.Latitude = pack_lat_q30(lat_deg);  // Q30
	rullPose.Longitude = pack_lon_q31(lon_deg);  // Q31

	// 고도/AGL: LSB=4 ft, signed 범위(-1060..80377)
	rullPose.Altitude = pack_alt_4ft(h_msl_ft);
	rullPose.AGL = pack_alt_4ft(h_agl_ft);

	// 속도(ft/s): LSB=0.03125
	rullPose.VelocityX = pack_vel_fts(mps_to_fts(u_mps));
	rullPose.VelocityY = pack_vel_fts(mps_to_fts(v_mps));
	rullPose.VelocityZ = pack_vel_fts(mps_to_fts(w_mps));

	// 방위/자세(sc=Q15)
	const double hdg180 = wrap180(hdg_deg); // (-180,180]
	rullPose.Azimuth = pack_q15(hdg180 / 180.0);
	rullPose.Roll = pack_q15(clampd(roll_deg / 180.0, -1.0, 1.0));
	rullPose.Pitch = pack_q15(clampd(pitch_deg / 90.0, -1.0, 1.0));
	rullPose.PresentTrueHeading = pack_q15(hdg180 / 180.0);

	// 가속도(ft/s²): LSB=0.03125
	rullPose.AccelerationX = pack_acc_fts2(mps2_to_fts2(ax_mps2));
	rullPose.AccelerationY = pack_acc_fts2(mps2_to_fts2(ay_mps2));
	rullPose.AccelerationZ = pack_acc_fts2(mps2_to_fts2(az_mps2));

	// 속도
	rullPose.Speed = pack_q4(speed);

	// G: LSB=0.001 (0~16 g) — 여기선 크기 기준 예시, 필요시 특정 축/센서값 사용
	const double g_now = std::sqrt(ax_mps2*ax_mps2 + ay_mps2*ay_mps2 + az_mps2*az_mps2) / 9.80665;
	{
		double g_raw = std::round(g_now / 0.001); // 1 LSB = 0.001 g
		g_raw = clampd(g_raw, 0.0, 32767.0);      // 구조체가 int16_t 이므로 양수 클램프
		rullPose.CurrentG = static_cast<int16_t>(g_raw);
	}

	// FPM: ft/min, LSB=0.5
	rullPose.FPMHorizontal = pack_fpm(mps_to_fpm(v_horz_mps));
	rullPose.FPMVertical = pack_fpm(mps_to_fpm(climb_mps));

	// 적기 정보: 필요 시 동일 스케일로 채우세요(여긴 0 초기화 예시)
	rullPose.EnemyLatitude = 0;
	rullPose.EnemyLongitude = 0;
	rullPose.EnemyAltitude = 0;
	rullPose.EnemyAzimuth = 0;
	rullPose.EnemyRoll = 0;
	rullPose.EnemyPitch = 0;
	rullPose.EnemySpeed = 0;

	// 예약 필드
	rullPose.RESERVED1 = rullPose.RESERVED2 = 0;
}
