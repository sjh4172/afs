/*
File: Waypoint.cpp (Dogfight Mode — Full Version, Reacquire Fix)
- 저고도 탈출(LAE) 및 Altitude Guard 이후 "재획득(PER: Post-Escape Reacquire)" 상태 추가
- Altitude Guard 히스테리시스(진입/해제)로 불필요한 재진입 방지
- 과대 뱅크 지속 시 릴리즈/정렬 로직 추가
- PN/PP 블렌딩 유지, min/max 미사용(clamp 계열만 사용)
*/

#include "Waypoint.h"
#include "FGFDMExec.h"
#include "simgear/misc/sg_path.hxx"
#include <iostream>
#include <windows.h>
#include <fstream>
#include <iomanip>
#include <string>
#include <cmath>
#include <algorithm>
#include "ClientJSBSIM.h"
#include <chrono>
#include <thread>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#include "../Joystick/StickTest.h"
#pragma comment(lib, "../Debug/Joystick.lib")

Waypoint::Waypoint() {}
Waypoint::~Waypoint() {}

using SteadyClock = std::chrono::steady_clock;
const double DT = 1.0 / 120.0; // 120 Hz

template <typename T>
T clamp(T val, T minVal, T maxVal) {
    if (val < minVal) return minVal;
    else if (val > maxVal) return maxVal;
    else return val;
}

static inline double deg2rad(double d){ return d * (3.14159265358979323846 / 180.0); }
static inline double rad2deg(double r){ return r * (180.0 / 3.14159265358979323846); }
static inline double wrap180(double a){
    while (a > 180.0) a -= 360.0;
    while (a < -180.0) a += 360.0;
    return a;
}
static inline double wrap360(double a){
    while (a >= 360.0) a -= 360.0;
    while (a < 0.0)    a += 360.0;
    return a;
}

extern std::atomic<int> g_run;
extern std::atomic<int> g_quit;

int  getRullRun()      { return g_run.load(std::memory_order_acquire); }
void setRullRun(int v) { g_run.store(v ? 1 : 0, std::memory_order_release); }
int  getRullQuit()     { return g_quit.load(std::memory_order_acquire); }
void setRullQuit(int v){ g_quit.store(v ? 1 : 0, std::memory_order_release); }
RullPoseMsg getRullPose() { return rullPose; }

// 단위/유틸
static inline double clamp_deg(double v, double lo, double hi){
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static const double FT_PER_NM = 6076.12;
static const double G_FTPS2 = 32.174;
static const double DEG2RAD = 3.14159265358979323846 / 180.0;
static const double RAD2DEG = 180.0 / 3.14159265358979323846;

// 위경도 차이를 N/E(ft)로 근사 변환
static inline void ll_to_ne_ft(double lat_deg, double lon_deg, double lat0_deg, double lon0_deg, double& n_ft, double& e_ft){
    const double dLat_deg = lat_deg - lat0_deg;
    const double dLon_deg = lon_deg - lon0_deg;
    const double meanLat = (lat_deg + lat0_deg) * 0.5;
    const double dN_nm = dLat_deg * 60.0;
    const double dE_nm = dLon_deg * 60.0 * std::cos(deg2rad(meanLat));
    n_ft = dN_nm * FT_PER_NM;
    e_ft = dE_nm * FT_PER_NM;
}

int waypointBase(){
    JSBSim::FGFDMExec fdm;

    SGPath rootDir("C:/Users/hoon4/Desktop/sjh/02.AI_Model/jsbsim-1.0.0/jsbsim-1.0.0");
    fdm.SetRootDir(rootDir);
    std::cout << "RootDir: " << rootDir.str() << std::endl;
    if (!fdm.LoadModel("f16", true)) {
        std::cerr << "Failed to load model" << std::endl;
        return 1;
    }

    int obj_Id = 0x3E8;
    std::ofstream tacview("C:/Users/hoon4/Desktop/sjh/02.AI_Model/flight.acmi");
    tacview << "FileType=text/acmi/tacview\n";
    tacview << "FileVersion=2.2\n\n";
    tacview << "0,ReferenceTime=2025-08-12T00:00:00Z\n";
    tacview << "0,Title=Test Flight from Log\n";
    tacview << "0,Author=ChatGPT\n\n";
    tacview << std::hex << std::uppercase << obj_Id;
    tacview << ",Name=TestPlane,Type=Air+FixedWing,Color=Blue,Callsign=Eagle1, \n\n";
    tacview.flush();

    // 초기 상태
    fdm.SetPropertyValue("ic/long-gc-deg", 127.0);
    fdm.SetPropertyValue("ic/lat-geod-deg", 37.0);
    fdm.SetPropertyValue("ic/h-sl-ft", 5000.0);
    fdm.SetPropertyValue("ic/psi-true-deg", 90.0);
    fdm.SetPropertyValue("ic/theta-deg", 0.0);
    fdm.SetPropertyValue("ic/phi-deg", 0.0);
    fdm.SetPropertyValue("ic/vt-kts", 500.0);

    fdm.RunIC();
    fdm.Setdt(DT);

    // --- 옵션/특수 상태 ---
    bool   otb_active = false;
    int    otb_mode = 0; // 0:none, 1:UP, 2:SIDE
    double otb_t = 0.0;
    double otb_turn_sign = 0.0;

    const double OTB_ENTER_RANGE_FT = 900.0, OTB_TIME_S = 0.9;
    const double OTB_BANK_SIDE_DEG = 110.0, OTB_BANK_UP_DEG = 65.0;
    const double OTB_PITCH_UP_DEG = 22.0, OTB_PITCH_SIDE_DEG = 10.0;
    const double OTB_SPEED_MIN_KTS = 440.0, OTB_THR_MIN = 0.55;

    // 간단 트림
    fdm.SetPropertyValue("simulation/do_simple_trim", 0);
    for (int i = 0; i < 400; ++i) {
        fdm.Run();
        if (fdm.GetPropertyValue("simulation/trim-completed") != 0.0) break;
    }

    // 엔진/기어
    fdm.SetPropertyValue("propulsion/set-running", -1);
    fdm.SetPropertyValue("gear/gear-cmd-norm", 0.0);

    // 속도 루프(도그파이트)
    double target_speed_base = 520.0;
    double base_throttle = 0.58;
    const double Kp_thr = 0.10;
    const double Ki_thr = 0.0030;

    // 거리 기반 스로틀 보정
    const double RNG_REF_FT = 7000.0;
    const double RNG_SCALE_FT = 20000.0;
    const double VC_SCALE = 400.0;
    const double Kp_thr_rng = 0.35;
    const double Kv_thr_vc = 0.25;
    const double CLOSE_BRAKE_FT = 2500.0;
    const double CLOSE_BRAKE_VC = 60.0;

    // 롤 제어
    const double MAX_BANK_DEG = 179.0;
    const double Kp_roll_ang = 0.52;
    const double tau_phi_s = 0.040;
    const double Kp_p = 1.85;
    const double Kd_pdot = 0.12;
    const double PHI_CMD_SLEW_DPS = 1000.0;

    // 피치 제어
    const double KR_YAW = 0.35;
    const double KR_DAMP = 0.15;
    const double ELEVATOR_NOSE_UP = -1.0;
    const double PITCH_MAX_CMD_DEG = 90.0;
    const double PITCH_MIN_CMD_DEG = -30.0;
    const double Kp_pitch = 0.80;
    const double Kd_q = 0.85;
    const double Ki_pitch = 0.06;
    const double I_LIM = 12.0;

    // LOS 기반 피치 리드
    const double LOS_GAIN = 1.60;
    const double LOS_LEAD_SEC = 0.80;
    const double PITCH_CMD_SLEW_DPS = 420.0;

    // PN 계수 (조밀 추적을 위해 PN 가중치 증가)
    const double N_h_base = 7.2;   // increased for tighter horizontal PN
    const double N_v_base = 4.6;   // slightly stronger vertical PN
    const double N_k_lam = 1.5;    // more sensitivity to LOS rate
    const double N_k_rng = 2800.0;

    // 피치-턴 보조
    const double PITCH_TURN_GAIN = 1.55;
    const double PITCH_TURN_MAX = 30.0;

    // PPR(추월 회복)
    const double PPR_ENTER_RANGE_FT = 6500.0;
    const double PPR_MIN_VC_SWITCH = 40.0;
    const double PPR_BANK_DEG = 95.0;
    const double PPR_MAX_PITCH_DEG = 40.0;
    const double PPR_PITCH_BOOST = 6.0;
    const double PPR_MAX_TIME_S = 1.2;
    const double PPR_EXIT_HDG_DEG = 28.0;
    const double PPR_RELAX_RATE = 0.85;
    const double PPR_BLEND_TAU = 0.6;

    // PPSG(소프트가드)
    const double PPSG_TIME_S = 0.9;
    const double PPSG_BANK_CAP = 95.0;
    double ppsg_t = 0.0;
    bool   ppsg_active = false;
    double ppsg_turn_sign = 0.0;

    // Altitude Guard (히스테리시스 추가)
    const double ALT_GUARD_FT = 3000.0;  // 진입 기준
    const double ALT_GUARD_EXIT_FT = 3300.0;  // 해제 기준
    const double ALT_GUARD_BANK_CAP = 85.0;
    const double ALT_GUARD_MIN_THR = 0.75; // 1500~3000
    const double ALT_GUARD_MIN_THR_LOW = 0.85; // <1500
    bool   alt_guard_active = false;

    auto altitude_guard_pitch_min = [&](double alt_ft)->double{
        if (alt_ft >= ALT_GUARD_FT) return 0.0;
        double deficit = ALT_GUARD_FT - alt_ft;
        double ratio = deficit / ALT_GUARD_FT;
        double cmd = 18.0 + 17.0 * ratio; // 18~35°
        return cmd;
    };
    auto altitude_guard_pitch_max = [&](double alt_ft)->double{
        if (alt_ft >= ALT_GUARD_FT) return 60.0;
        double ratio = (ALT_GUARD_FT - alt_ft) / ALT_GUARD_FT; // 0~1
        double cap = 60.0 - 5.0 * ratio; // 60→55
        if (cap < 55.0) cap = 55.0;
        return cap;
    };

    // LAE: Low-Altitude Escape
    const double LAE_TRIG_ALT_FT = 2500.0;
    const double LAE_EXIT_ALT_FT = 3500.0;
    const double LAE_SINK_TRIG_FPS = -10.0;
    const double LAE_TIME_S = 2.5;
    const double LAE_BANK_CAP = 45.0;
    const double LAE_PITCH_TARGET = 55.0;
    const double LAE_THR = 1.0;
    bool   lae_active = false;
    double lae_t = 0.0;

    // PER: Post-Escape Reacquire (재획득 상태)
    const double PER_TIME_S = 2.8;   // 재정렬 유지 시간
    const double PER_BANK_CAP = 70.0;  // 재정렬 중 최대 뱅크
    const double PER_PITCH_TARGET = 10.0;  // 재정렬 중 기본 피치 목표(LOS 엘리베이션에 더해짐)
    const double PER_EXIT_HDG_DEG = 12.0;  // 헤딩 오차가 이 값보다 작아지고…
    const double PER_EXIT_VC_FTPS = 30.0;  // 폐쇄속도가 양(접근)으로 회복되면 종료
    const double PER_VC_BAD_FTPS = -80.0; // 지나치게 열리는 경우에는 피치 다소 올림
    const double PER_SLEW_GAIN = 0.55;  // 헤딩 정렬 이득
    bool   per_active = false;
    double per_t = 0.0;

    // PASS-THROUGH 복귀 (타깃을 지나쳐갈 때 강제 복귀)
    const double PASS_THROUGH_FT = 1200.0;   // 거리 임계값
    const double PASS_RECOVERY_S  = 1.4;     // 복귀 유지 시간
    const double PASS_BANK_CAP    = 95.0;    // 복귀 시 허용 뱅크
    const double PASS_TURN_GAIN   = 1.0;     // 헤딩오차에 곱해지는 복귀 이득
    bool   pass_recovery_active = false;
    double pass_recovery_t = 0.0;

    // 상태
    double pitch_i = 0.0;
    double spd_i = 0.0;
    double snap_t = 0.0;
    bool   snap_active = false;

    // 이전 샘플
    double prev_los_az_deg = 0.0, prev_los_el_deg = 0.0, prev_range_ft = 0.0;
    double prev_Vc = 0.0;
    double prev_pitch_cmd = 0.0, prev_phi_cmd = 0.0;
    double prev_p_radps = 0.0;
    bool   los_init = false;

    // PPR 상태
    bool   ppr_active = false;
    double ppr_t = 0.0;
    double ppr_turn_sign = 0.0;

    // 타깃 유효성/재사용
    static bool   tgt_valid = false;
    static double lastTgtLat = 0.0, lastTgtLon = 0.0, lastTgtAlt = 0.0;

    // 필터된 LOS 속도/폐쇄속도(전역 유지용)
    static double flt_los_az_rate = 0.0, flt_los_el_rate = 0.0, flt_Vc = 0.0;

    if (!SendClient_Init("192.253.0.37", 20001)) return 1;

    timeBeginPeriod(1);
    auto real_prev = SteadyClock::now();
    double acc = 0.0;

    double log_last_sim = 0.0;

    // 디버그 출력 변수
    double los_az_rate_dps = 0.0;
    double los_el_rate_dps = 0.0;
    double Vc_ftps = 0.0;
    double phi_cmd_deg = 0.0;
    double pitch_cmd_deg = 0.0;

    std::cout << "[DOGFIGHT MODE + LAE + PER] 활성화" << std::endl;

    while (!getRullQuit()) {
        if (!getRullRun()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            real_prev = SteadyClock::now();
            continue;
        }

        auto real_now = SteadyClock::now();
        double real_dt = std::chrono::duration<double>(real_now - real_prev).count();
        real_prev = real_now;
        acc += real_dt;

        int steps = 0;
        while (acc >= DT) {
            if (getRullQuit() || !getRullRun()) { acc = 0.0; break; }

            // 현재 상태
            const double curLatDeg = fdm.GetPropertyValue("position/lat-geod-deg");
            const double curLonDeg = fdm.GetPropertyValue("position/long-gc-deg");
            const double curAltFt = fdm.GetPropertyValue("position/h-sl-ft");
            const double curPitch = fdm.GetPropertyValue("attitude/theta-deg");
            const double curRoll = fdm.GetPropertyValue("attitude/phi-deg");
            const double curHead = fdm.GetPropertyValue("attitude/psi-deg");
            const double vtrue_kts = fdm.GetPropertyValue("velocities/vtrue-kts");
            const double vtrue_fps = fdm.GetPropertyValue("velocities/vtrue-fps");
            const double hdot_fps = fdm.GetPropertyValue("velocities/h-dot-fps"); // 상승(+)/하강(-)

            const double p_radps = fdm.GetPropertyValue("velocities/p-rad_sec");
            const double q_radps = fdm.GetPropertyValue("velocities/q-rad_sec");
            const double r_radps = fdm.GetPropertyValue("velocities/r-rad_sec");
            const double pdot_radps2 = (p_radps - prev_p_radps) / DT;
            prev_p_radps = p_radps;

            // 타깃 수신
            StickPoseMsg tp = getStickPose();
            double tgtLatDeg = (static_cast<double>(tp.Latitude) / 1073741824.0) * 90.0;
            double tgtLonDeg = (static_cast<double>(tp.Longitude) / 2147483647.0)  * 180.0;
            double tgtAltFt = static_cast<double>(tp.Altitude) * 4.0;

            // 잘못된 타깃 샘플 무시/재사용
            bool bad_target = (std::fabs(tgtLatDeg) < 1e-6 && std::fabs(tgtLonDeg) < 1e-6) || (tp.Altitude == 0);
            if (bad_target && tgt_valid) {
                tgtLatDeg = lastTgtLat; tgtLonDeg = lastTgtLon; tgtAltFt = lastTgtAlt;
            }
            else if (!bad_target) {
                tgt_valid = true;
                lastTgtLat = tgtLatDeg; lastTgtLon = tgtLonDeg; lastTgtAlt = tgtAltFt;
            }
            if (!tgt_valid) {
                // 유효 타깃 없으면 홀드
                fdm.SetPropertyValue("fcs/aileron-cmd-norm", 0.0);
                fdm.SetPropertyValue("fcs/elevator-cmd-norm", 0.0);
                fdm.SetPropertyValue("fcs/throttle-cmd-norm", 0.6);
                fdm.Run();
                UpdatePoseFromJSBSim(fdm);
                sendMessage();
                acc -= DT;
                if (++steps > 10) { acc = 0.0; break; }
                continue;
            }

            // 상대 위치 (N/E/H)
            double dN_ft = 0.0, dE_ft = 0.0;
            ll_to_ne_ft(tgtLatDeg, tgtLonDeg, curLatDeg, curLonDeg, dN_ft, dE_ft);
            const double dH_ft = tgtAltFt - curAltFt;
            const double ground_ft = std::sqrt(dN_ft*dN_ft + dE_ft*dE_ft);
            const double range_ft = std::sqrt(ground_ft*ground_ft + dH_ft*dH_ft);

            // LOS 각/각속도
            const double los_az_deg = wrap360(rad2deg(std::atan2(dE_ft, dN_ft)));
            const double los_el_deg = rad2deg(std::atan2(dH_ft, (ground_ft < 1.0 ? 1.0 : ground_ft)));

            if (!los_init) {
                prev_los_az_deg = los_az_deg; prev_los_el_deg = los_el_deg;
                prev_range_ft = range_ft;
                prev_pitch_cmd = curPitch;   prev_phi_cmd = curRoll;
                los_init = true;
            }

            los_az_rate_dps = wrap180(los_az_deg - prev_los_az_deg) / DT;
            los_el_rate_dps = (los_el_deg - prev_los_el_deg) / DT;
            const double range_rate_ftps = (range_ft - prev_range_ft) / DT;
            Vc_ftps = -range_rate_ftps; // +면 접근

            // PPR / PPSG 트리거
            bool vc_sign_switch = (std::fabs(prev_Vc) > PPR_MIN_VC_SWITCH) &&
                ((prev_Vc > 0.0 && Vc_ftps < 0.0) || (prev_Vc < 0.0 && Vc_ftps > 0.0));
            bool tgt_behind = (std::fabs(wrap180((wrap360(rad2deg(std::atan2(dE_ft, dN_ft))) - curHead))) > 120.0);
            bool near_for_ppr = (range_ft < PPR_ENTER_RANGE_FT);
            if (!ppr_active && near_for_ppr && (vc_sign_switch || tgt_behind)) {
                ppr_active = true;
                ppr_t = 0.0;
                double hdg_err_enter = wrap180(los_az_deg - curHead);
                ppr_turn_sign = (hdg_err_enter >= 0.0) ? 1.0 : -1.0;
            }
            if (!ppsg_active && vc_sign_switch && near_for_ppr) {
                ppsg_active = true;
                ppsg_t = 0.0;
                double hdg_err_enter2 = wrap180(los_az_deg - curHead);
                ppsg_turn_sign = (hdg_err_enter2 >= 0.0) ? 1.0 : -1.0;
            }
            prev_Vc = Vc_ftps;

            prev_los_az_deg = los_az_deg; prev_los_el_deg = los_el_deg; prev_range_ft = range_ft;

            // ===== 필터 (섀도잉 방지: *_f 로 계산 후 전역 상태로 복사) =====
            const double a = 0.55;
            double flt_los_az_rate_f = a * los_az_rate_dps + (1.0 - a) * flt_los_az_rate;
            double flt_los_el_rate_f = a * los_el_rate_dps + (1.0 - a) * flt_los_el_rate;
            double flt_Vc_f = a * Vc_ftps + (1.0 - a) * flt_Vc;
            flt_los_az_rate = flt_los_az_rate_f;
            flt_los_el_rate = flt_los_el_rate_f;
            flt_Vc = flt_Vc_f;

            // 리드 타임 제한
            double t_go = range_ft / (std::abs(flt_Vc_f) < 1.0 ? 1.0 : std::abs(flt_Vc_f));
            if (t_go < 0.25) t_go = 0.25;
            if (t_go > 4.0)  t_go = 4.0;

            // 가변 PN
            double N_h = N_h_base + N_k_lam * std::abs(flt_los_az_rate_f*DEG2RAD) + (N_k_rng / (N_k_rng + range_ft));
            double N_v = N_v_base + 0.7 * std::abs(flt_los_el_rate_f*DEG2RAD) + (0.8*N_k_rng / (N_k_rng + range_ft));
            N_h = clamp(N_h, 3.5, 6.8);
            N_v = clamp(N_v, 2.5, 5.0);

            // 수평(뱅크) 기본: PN + PP
            const double MAX_BANK_OPENING = 120.0;
            const double MAX_BANK_NEAR = 150.0;
            const double MAX_BANK_CLOSING = 179.0;

            const double lambda_az_radps = flt_los_az_rate_f * DEG2RAD;
            double phi_cmd_pn_deg = rad2deg(std::atan2(N_h * flt_Vc_f * lambda_az_radps, G_FTPS2));
            const bool poor_pn = (flt_Vc_f <= 0.0) || (std::abs(flt_los_az_rate_f) < 0.7);
            double hdg_err = wrap180(los_az_deg - curHead);

            double hdg_abs = std::abs(hdg_err);
            double hdg_scale = hdg_abs / 90.0; if (hdg_scale > 1.0) hdg_scale = 1.0;
            double phi_cmd_pp_deg = clamp_deg((1.00 + 0.85 * hdg_scale) * hdg_err, -MAX_BANK_DEG, +MAX_BANK_DEG);

            // PN 우선도를 높여 더 타이트한 추적 (PN 비중 상승)
            double phi_cmd_normal_deg = poor_pn ? phi_cmd_pp_deg : (0.95 * phi_cmd_pn_deg + 0.05 * phi_cmd_pp_deg);
            {
                double dynamic_bank_cap = MAX_BANK_CLOSING;
                if (flt_Vc_f < 0.0) {
                    dynamic_bank_cap = MAX_BANK_OPENING;
                }
                else if (std::abs(flt_Vc_f) < 80.0) {
                    dynamic_bank_cap = MAX_BANK_NEAR;
                }
                phi_cmd_normal_deg = clamp_deg(phi_cmd_normal_deg, -dynamic_bank_cap, +dynamic_bank_cap);
            }

            // anti-flip
            if (phi_cmd_normal_deg * hdg_err < 0.0) {
                phi_cmd_normal_deg = 0.9 * phi_cmd_pp_deg;
            }

            // 부스트/스냅/안티-핀휠
            {
                double bank_boost = 0.0;
                if (std::abs(flt_los_az_rate_f) > 3.0 || std::abs(hdg_err) > 40.0) {
                    double rate_norm = std::abs(flt_los_az_rate_f) / 8.0; if (rate_norm > 1.0) rate_norm = 1.0;
                    double hdg_norm2 = std::abs(hdg_err) / 90.0; if (hdg_norm2 > 1.0) hdg_norm2 = 1.0;
                    double situ_gain2 = 1.0; if (flt_Vc_f > 120.0) situ_gain2 += 0.40;
                    bank_boost = (48.0 * (0.6 * rate_norm + 0.4 * hdg_norm2) * situ_gain2) * (hdg_err >= 0.0 ? 1.0 : -1.0);
                    phi_cmd_normal_deg += bank_boost;
                }
                bool snap_trig = (std::abs(flt_los_az_rate_f) > 6.0 && std::abs(hdg_err) > 35.0);
                bool snap_rel = (std::abs(flt_los_az_rate_f) < 3.0 && std::abs(hdg_err) < 25.0);
                if (!snap_active && snap_trig && flt_Vc_f > 60.0) { snap_active = true; snap_t = 0.0; }
                if (snap_active) {
                    snap_t += DT;
                    double dynamic_bank_cap = (flt_Vc_f < 0.0 ? MAX_BANK_OPENING : MAX_BANK_CLOSING);
                    phi_cmd_normal_deg = (hdg_err >= 0.0 ? +dynamic_bank_cap : -dynamic_bank_cap);
                    if (snap_t > 0.8 || snap_rel || flt_Vc_f <= 0.0) { snap_active = false; }
                }
                // 과대 뱅크 릴리즈(LOS 속도/헤딩오차가 작아졌는데 φ 명령이 큰 상태가 지속되면 풀어줌)
                static double over_bank_t = 0.0;
                if (std::abs(phi_cmd_normal_deg) > 110.0) {
                    if (std::abs(flt_los_az_rate_f) < 4.0 && std::abs(hdg_err) < 25.0) over_bank_t += DT; else over_bank_t = 0.0;
                    if (over_bank_t > 1.2) {
                        double sign = (phi_cmd_normal_deg >= 0.0) ? 1.0 : -1.0;
                        double relax = 20.0 + 1.2 * std::abs(hdg_err); if (relax > 90.0) relax = 90.0;
                        phi_cmd_normal_deg = sign * relax;
                        over_bank_t = 0.0;
                    }
                }
                else {
                    over_bank_t = 0.0;
                }
            }

            // ===== PASS-THROUGH 감지 및 복귀 로직 =====
            bool just_passed_target = (prev_Vc > 0.0 && Vc_ftps < 0.0 && range_ft < PASS_THROUGH_FT);
            if (just_passed_target) {
                pass_recovery_active = true;
                pass_recovery_t = 0.0;
                // snap/over-bank 해제시켜 복귀가 가능하도록 함
                snap_active = false;
                // integrator 리셋으로 급격한 기동 억제
                pitch_i = 0.0;
                spd_i = 0.0;
            }

            if (pass_recovery_active) {
                pass_recovery_t += DT;
                // 강제 헤딩 정렬: LOS 쪽으로 즉시 돌아오도록 bank 지정
                double phi_recovery = clamp_deg(PASS_TURN_GAIN * hdg_err, -PASS_BANK_CAP, +PASS_BANK_CAP);
                // 부드럽게 적용 (overwrite normal command but keep magnitude limits)
                phi_cmd_normal_deg = phi_recovery;
                // 복귀 중에는 피치를 너무 올리지 않음
                if (pitch_cmd_deg > 15.0) pitch_cmd_deg = 15.0;
                // 일정 시간 경과하면 복귀 완료로 간주
                if (pass_recovery_t > PASS_RECOVERY_S || std::abs(hdg_err) < 8.0 || flt_Vc_f > 20.0) {
                    pass_recovery_active = false;
                }
            }

            // 기본 슬루/필터
            {
                double candA = phi_cmd_normal_deg;
                double candB = (phi_cmd_normal_deg > 0.0) ? (phi_cmd_normal_deg - 360.0) : (phi_cmd_normal_deg + 360.0);
                if (std::abs(candB - prev_phi_cmd) < std::abs(candA - prev_phi_cmd)) phi_cmd_normal_deg = candB;

                const double alpha = 0.60;
                phi_cmd_normal_deg = alpha * phi_cmd_normal_deg + (1.0 - alpha) * prev_phi_cmd;

                const double max_step = PHI_CMD_SLEW_DPS * DT;
                double dphi = phi_cmd_normal_deg - prev_phi_cmd;
                if (dphi >  max_step) phi_cmd_normal_deg = prev_phi_cmd + max_step;
                if (dphi < -max_step) phi_cmd_normal_deg = prev_phi_cmd - max_step;

                phi_cmd_normal_deg = clamp_deg(
                    phi_cmd_normal_deg,
                    -(flt_Vc_f < 0.0 ? MAX_BANK_OPENING : MAX_BANK_CLOSING),
                    +(flt_Vc_f < 0.0 ? MAX_BANK_OPENING : MAX_BANK_CLOSING));
            }

            // ===== PER(재획득) 상태 진입/유지/종료 =====
            // Altitude Guard 상태 판단(히스테리시스)
            if (!alt_guard_active && curAltFt < ALT_GUARD_FT) alt_guard_active = true;
            if (alt_guard_active && curAltFt > ALT_GUARD_EXIT_FT) alt_guard_active = false;

            // LAE 트리거
            if (!lae_active) {
                bool low_and_sinking = (curAltFt < LAE_TRIG_ALT_FT) && (hdot_fps < LAE_SINK_TRIG_FPS);
                bool low_and_fast_opening = (curAltFt < LAE_TRIG_ALT_FT) && (Vc_ftps < -300.0);
                if (low_and_sinking || low_and_fast_opening) {
                    lae_active = true;
                    lae_t = 0.0;
                    per_active = false; // LAE가 더 우선
                    per_t = 0.0;
                }
            }

            // 기본 φ 명령을 우선 계산해 두고, PER/LAE가 있으면 덮어씀
            double phi_cmd_after_modes_deg = phi_cmd_normal_deg;

            // ----- LAE (저고도 탈출) -----
            if (lae_active) {
                lae_t += DT;
                double signb = (phi_cmd_after_modes_deg >= 0.0) ? 1.0 : -1.0;
                if (std::abs(phi_cmd_after_modes_deg) > LAE_BANK_CAP) phi_cmd_after_modes_deg = signb * LAE_BANK_CAP;

                pitch_cmd_deg = (LOS_GAIN * los_el_deg) + (LOS_LEAD_SEC * flt_los_el_rate_f);
                if (pitch_cmd_deg < LAE_PITCH_TARGET) pitch_cmd_deg = LAE_PITCH_TARGET;

                // 종료 조건(고도 회복 or 타임아웃)
                bool alt_recovered = (curAltFt > LAE_EXIT_ALT_FT) && (hdot_fps > 0.0);
                if (alt_recovered || lae_t > LAE_TIME_S) {
                    lae_active = false;
                    // 재획득 상태로 전환
                    per_active = true;
                    per_t = 0.0;
                }
            }
            // ----- PER (재획득) -----
            else if (per_active || alt_guard_active) {
                if (!per_active) { per_active = true; per_t = 0.0; } // 고도 가드 진입 중에도 PER 형태로 정렬

                per_t += DT;

                // 헤딩 정렬 전용: 헤딩 오차 기반 PP, 제한된 뱅크
                double hdg_err_per = wrap180(los_az_deg - curHead);
                double phi_per = clamp_deg(PER_SLEW_GAIN * hdg_err_per, -PER_BANK_CAP, +PER_BANK_CAP);

                // 피치는 안정적으로: 기본 10도 + LOS elevation 일부만 반영
                double pitch_per = PER_PITCH_TARGET + 0.6 * los_el_deg;
                // 너무 열릴 때 폐쇄속도 보정
                if (flt_Vc_f < PER_VC_BAD_FTPS) pitch_per += 4.0;

                phi_cmd_after_modes_deg = phi_per;
                pitch_cmd_deg = clamp_deg(pitch_per, PITCH_MIN_CMD_DEG, 45.0);

                // 종료 조건: 헤딩 오차 작고, Vc가 양(접근)이며, 시간 충분 또는 범위 감소
                bool hdg_small = (std::abs(hdg_err_per) < PER_EXIT_HDG_DEG);
                bool good_vc = (flt_Vc_f > PER_EXIT_VC_FTPS);
                bool time_ok = (per_t > 0.8);
                static double range_prev_for_per = 0.0;
                bool range_closing = (range_prev_for_per > 1.0 && range_ft < range_prev_for_per);
                range_prev_for_per = range_ft;

                if ((hdg_small && good_vc && time_ok) || per_t > PER_TIME_S || (ppsg_active == false && ppr_active == false && std::abs(flt_los_az_rate_f) > 7.0)) {
                    per_active = false; // 정상 모드 복귀
                }
            }
            // ----- 정상 모드 (PER/LAE 아님) -----
            else {
                // 수직(피치) 계산
                pitch_cmd_deg = (LOS_GAIN * los_el_deg) + (LOS_LEAD_SEC * flt_los_el_rate_f);

                if (ppr_active) {
                    pitch_cmd_deg += PPR_PITCH_BOOST;
                    if (pitch_cmd_deg > PPR_MAX_PITCH_DEG) pitch_cmd_deg = PPR_MAX_PITCH_DEG;
                }
                else if (flt_Vc_f > 120.0 && std::fabs(flt_los_el_rate_f) > 2.5) {
                    const double lambda_el_radps = flt_los_el_rate_f * DEG2RAD;
                    const double a_z_cmd = N_v * flt_Vc_f * lambda_el_radps;
                    pitch_cmd_deg += rad2deg(std::atan2(a_z_cmd, G_FTPS2));
                }
                // 방향 전환 시 피치 보조
                double bank_at_cap = (std::abs(phi_cmd_normal_deg) > (MAX_BANK_DEG - 1.0)) ? 1.0 : 0.0;
                {
                    double bank_norm = std::abs(phi_cmd_normal_deg) / 60.0; if (bank_norm > 1.0) bank_norm = 1.0;
                    double bank_curve = 0.5 * bank_norm * bank_norm + 0.5 * bank_norm;
                    double hdg_norm = std::abs(hdg_err) / 45.0; if (hdg_norm > 2.0) hdg_norm = 2.0;
                    double situ_gain = 1.0;
                    if (flt_Vc_f > 120.0) situ_gain += 0.25;
                    if (std::abs(flt_los_az_rate_f) > 2.0) situ_gain += 0.2;
                    double pitch_turn_boost = PITCH_TURN_GAIN * bank_curve * (1.0 + 0.65 * hdg_norm) * PITCH_TURN_MAX * situ_gain;
                    if (bank_at_cap > 0.5 || std::abs(hdg_err) > 60.0) pitch_turn_boost *= 1.25;
                    if (std::abs(phi_cmd_normal_deg) > 150.0) pitch_turn_boost *= 0.6;
                    if (los_el_deg < -2.0) pitch_turn_boost *= 0.7;
                    pitch_cmd_deg += pitch_turn_boost;
                }

                if (std::abs(phi_cmd_normal_deg) < 20.0 && std::abs(hdg_err) < 25.0 && std::fabs(pitch_cmd_deg - curPitch) < 0.8)
                    pitch_cmd_deg = curPitch;

                phi_cmd_after_modes_deg = phi_cmd_normal_deg;
            }

            // ===== Altitude Guard 적용(활성 시) =====
            if (alt_guard_active) {
                double min_up = altitude_guard_pitch_min(curAltFt);
                if (pitch_cmd_deg < min_up) pitch_cmd_deg = min_up;

                double sign_bank = (phi_cmd_after_modes_deg >= 0.0) ? 1.0 : -1.0;
                double cap = ALT_GUARD_BANK_CAP;
                if (std::abs(phi_cmd_after_modes_deg) > cap) phi_cmd_after_modes_deg = sign_bank * cap;

                double pcap = altitude_guard_pitch_max(curAltFt);
                if (pitch_cmd_deg > pcap) pitch_cmd_deg = pcap;
            }

            // 제한 & 슬루 & 저고도 추가 보호
            if (std::abs(phi_cmd_after_modes_deg) > 100.0) {
                pitch_cmd_deg = clamp_deg(pitch_cmd_deg, PITCH_MIN_CMD_DEG, 45.0);
            }
            else if (std::abs(phi_cmd_after_modes_deg) > 150.0 && PITCH_MAX_CMD_DEG > 75.0) {
                pitch_cmd_deg = clamp_deg(pitch_cmd_deg, PITCH_MIN_CMD_DEG, 75.0);
            }
            else {
                pitch_cmd_deg = clamp_deg(pitch_cmd_deg, PITCH_MIN_CMD_DEG, PITCH_MAX_CMD_DEG);
            }
            if (curAltFt < 2000.0 && pitch_cmd_deg > 35.0) {
                if (!lae_active) pitch_cmd_deg = 35.0;
            }
            else {
                pitch_cmd_deg = clamp_deg(pitch_cmd_deg, PITCH_MIN_CMD_DEG, PITCH_MAX_CMD_DEG);
            }

            // 피치 슬루
            {
                const double max_step = PITCH_CMD_SLEW_DPS * DT;
                double dcmd = pitch_cmd_deg - prev_pitch_cmd;
                if (dcmd >  max_step) pitch_cmd_deg = prev_pitch_cmd + max_step;
                if (dcmd < -max_step) pitch_cmd_deg = prev_pitch_cmd - max_step;
                prev_pitch_cmd = pitch_cmd_deg;
            }

            // 최종 φ 명령 확정
            phi_cmd_deg = phi_cmd_after_modes_deg;
            prev_phi_cmd = phi_cmd_deg;

            // 피드백 제어들
            const double pitch_err = pitch_cmd_deg - curPitch;
            pitch_i += pitch_err * DT;
            if (pitch_i > I_LIM)  pitch_i = I_LIM;
            if (pitch_i < -I_LIM) pitch_i = -I_LIM;
            double elevator_cmd = (Kp_pitch * pitch_err) - (Kd_q * q_radps) + (Ki_pitch * pitch_i);
            elevator_cmd = ELEVATOR_NOSE_UP * elevator_cmd;
            elevator_cmd = clamp(elevator_cmd, -0.90, 0.90);

            double p_cmd = (phi_cmd_deg - curRoll) / tau_phi_s + (Kp_roll_ang * (phi_cmd_deg - curRoll));
            const double p_cmd_max = (950.0 * DEG2RAD);
            if (p_cmd >  p_cmd_max) p_cmd = p_cmd_max;
            if (p_cmd < -p_cmd_max) p_cmd = -p_cmd_max;

            double aileron_cmd = (Kp_p * (p_cmd - p_radps)) - (Kd_pdot * pdot_radps2);
            aileron_cmd = clamp(aileron_cmd, -1.0, 1.0);
            double r_cmd = KR_YAW * (flt_los_az_rate_f * DEG2RAD);
            double rudder_cmd = r_cmd - KR_DAMP * r_radps;
            rudder_cmd = clamp(rudder_cmd, -0.8, 0.8);

            // 속도/스로틀
            double target_speed_cmd = target_speed_base;
            if (ppr_active) target_speed_cmd = 440.0;
            if (std::abs(wrap180(los_az_deg - curHead)) > 60.0 || std::abs(phi_cmd_deg) > 60.0 || std::abs(flt_los_az_rate_f) > 6.0) {
                target_speed_cmd = 460.0;
            }
            if (std::abs(phi_cmd_deg) > (MAX_BANK_DEG - 1.0)) {
                target_speed_cmd = 440.0;
            }
            // PER 중에는 너무 빠르지 않게
            if (per_active) {
                if (target_speed_cmd > 470.0) target_speed_cmd = 470.0;
            }

            double spd_error = (target_speed_cmd - vtrue_kts);
            spd_i += spd_error * DT;
            spd_i = clamp(spd_i, -80.0, 80.0);
            double throttle_cmd = base_throttle + (Kp_thr * spd_error) + (Ki_thr * spd_i);
            throttle_cmd += 0.15 * (std::abs(curRoll) / 90.0);

            // 거리·폐쇄속도 기반
            {
                double rng_err = range_ft - RNG_REF_FT;
                double rng_term = Kp_thr_rng * (rng_err / (RNG_SCALE_FT <= 1.0 ? 1.0 : RNG_SCALE_FT));
                double vc_term = -Kv_thr_vc * (flt_Vc_f / (VC_SCALE <= 1.0 ? 1.0 : VC_SCALE));
                throttle_cmd += rng_term + vc_term;
                if (range_ft < CLOSE_BRAKE_FT && flt_Vc_f > CLOSE_BRAKE_VC) {
                    throttle_cmd -= 0.25;
                }
            }

            // LAE 시 스로틀 최대
            if (lae_active && throttle_cmd < LAE_THR) throttle_cmd = LAE_THR;

            // Altitude Guard 스로틀 하한
            if (curAltFt < 1500.0) {
                if (throttle_cmd < ALT_GUARD_MIN_THR_LOW) throttle_cmd = ALT_GUARD_MIN_THR_LOW;
            }
            else if (curAltFt < ALT_GUARD_FT) {
                if (throttle_cmd < ALT_GUARD_MIN_THR) throttle_cmd = ALT_GUARD_MIN_THR;
            }

            // 에너지 보호
            if (std::abs(phi_cmd_deg) > 80.0 || pitch_cmd_deg > 30.0) {
                if (throttle_cmd < 0.35) throttle_cmd = 0.35;
            }
            if (curAltFt < 2000.0 && throttle_cmd < 0.60) throttle_cmd = 0.60;

            throttle_cmd = clamp(throttle_cmd, 0.0, 1.0);

            // 적용
            fdm.SetPropertyValue("fcs/elevator-cmd-norm", elevator_cmd);
            fdm.SetPropertyValue("fcs/rudder-cmd-norm", rudder_cmd);
            fdm.SetPropertyValue("fcs/aileron-cmd-norm", aileron_cmd);
            fdm.SetPropertyValue("fcs/throttle-cmd-norm", throttle_cmd);

            // step
            fdm.Run();
            UpdatePoseFromJSBSim(fdm);
            sendMessage();

            acc -= DT;
            if (++steps > 10) { acc = 0.0; break; }
        }

        // 1Hz 로그
        double sim_now = fdm.GetSimTime();
        if (sim_now - log_last_sim >= 1.0) {
            StickPoseMsg t = getStickPose();
            const double logTgtLat = (static_cast<double>(t.Latitude) / 1073741824.0) * 90.0;
            const double logTgtLon = (static_cast<double>(t.Longitude) / 2147483647.0)  * 180.0;
            const double logTgtAlt = static_cast<double>(t.Altitude) * 4.0;

            std::cout << std::fixed << std::setprecision(6);
            std::cout << "tgtLatDeg: " << logTgtLat
                << ", tgtLonDeg: " << logTgtLon
                << ", tgtAltFt: " << std::setprecision(2) << logTgtAlt << std::endl;

            std::cout << "currentRoll: " << std::setprecision(3)
                << fdm.GetPropertyValue("attitude/phi-deg") << std::endl;

            log_last_sim = sim_now;

            double current_time = fdm.GetSimTime();
            double lon = fdm.GetPropertyValue("position/long-gc-deg");
            double lat = fdm.GetPropertyValue("position/lat-geod-deg");
            double alt_m = fdm.GetPropertyValue("position/h-sl-meters");
            double alt_ft = fdm.GetPropertyValue("position/h-sl-ft");
            double heading = fdm.GetPropertyValue("attitude/psi-deg");
            double spd_kts = fdm.GetPropertyValue("velocities/vtrue-kts");
            double spd_fps = fdm.GetPropertyValue("velocities/vtrue-fps");
            double roll = fdm.GetPropertyValue("attitude/phi-deg");
            double pitch = fdm.GetPropertyValue("attitude/theta-deg");
            double yaw = fdm.GetPropertyValue("attitude/psi-deg");
            double tas_mps = fdm.GetPropertyValue("velocities/vtrue-kts") * 0.514444;

            tacview << "#" << std::fixed << std::setprecision(3) << current_time << "\n";
            tacview.flush();
            tacview << std::hex << std::uppercase << obj_Id
                << ",T=" << std::fixed << std::setprecision(6) << lon << "|" << lat
                << "|" << std::setprecision(2) << alt_m
                << "|" << std::setprecision(3) << roll
                << "|" << std::setprecision(3) << pitch
                << "|" << std::setprecision(3) << yaw
                << ",TAS=" << std::setprecision(3) << tas_mps << "\n\n";
            tacview.flush();

            double print_range_ft = prev_range_ft;
            double last_throttle = fdm.GetPropertyValue("fcs/throttle-cmd-norm");

            std::cout << std::fixed << std::setprecision(6);
            std::cout << "Time: " << current_time
                << ", Lon: " << lon
                << ", Lat: " << lat
                << ", Alt(ft): " << std::setprecision(2) << alt_ft
                << ", Speed(kts): " << std::setprecision(3) << spd_kts
                << ", Speed(fps): " << std::setprecision(3) << spd_fps
                << ", Heading(deg): " << std::setprecision(3) << heading
                << std::endl;

            // 전역 스무딩 상태 출력
            std::cout << "los_az_rate_dps:" << flt_los_az_rate
                << " los_el_rate_dps:" << flt_los_el_rate
                << " Vc_ftps:" << flt_Vc
                << " phi_cmd_deg:" << phi_cmd_deg
                << " pitch_cmd_deg:" << pitch_cmd_deg
                << " range_ft:" << print_range_ft
                << " thr_cmd:" << last_throttle
                << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::microseconds(1000));
    }

    timeEndPeriod(1);
    SendClient_Close();
    tacview.close();
    return 0;
}
