#include "Test.h"
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

Test::Test() {}
Test::~Test() {}
RullPoseMsg rullPose = {};

// steady_clock alias
using SteadyClock = std::chrono::steady_clock;
const double DT = 1.0 / 120.0; // 120 Hz

template <typename T>
T clamp(T val, T minVal, T maxVal) {
	if (val < minVal) return minVal;
	else if (val > maxVal) return maxVal;
	else return val;
}

std::atomic<int> g_run{ 0 };   // 시작 상태: 정지
std::atomic<int> g_quit{ 0 };

int  getRullRun()      { return g_run.load(std::memory_order_acquire); }
void setRullRun(int v) { g_run.store(v ? 1 : 0, std::memory_order_release); }
int  getRullQuit()     { return g_quit.load(std::memory_order_acquire); }
void setRullQuit(int v){ g_quit.store(v ? 1 : 0, std::memory_order_release); }
RullPoseMsg getRullPose() { return rullPose; }

int rullBase(){
	JSBSim::FGFDMExec fdm;

	// JSBSim include
	SGPath rootDir("C:/Users/hoon4/Desktop/sjh/02.AI_Model/jsbsim-1.0.0/jsbsim-1.0.0");
	fdm.SetRootDir(rootDir);
	std::cout << "RootDir: " << rootDir.str() << std::endl;
	if (!fdm.LoadModel("f16", true)) {
		std::cerr << "Failed to load model" << std::endl;
		return 1;
	}
	
	// Tacview Log
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

	// 초기 위치/상태
	fdm.SetPropertyValue("ic/long-gc-deg", 127.0);	// 경도
	fdm.SetPropertyValue("ic/lat-geod-deg", 37.0);	// 위도
	fdm.SetPropertyValue("ic/h-sl-ft", 5000.0);
	fdm.SetPropertyValue("ic/psi-true-deg", 90.0);
	fdm.SetPropertyValue("ic/theta-deg", 0.0);
	fdm.SetPropertyValue("ic/phi-deg", 0.0);
	fdm.SetPropertyValue("ic/vt-kts", 350);

	// 초기화
	fdm.RunIC();
	// --- dt를 1/120초로 고정 ---
	fdm.Setdt(DT);


	// ---- 유턴 트리거 파라미터 ----
	const double TRIGGER_LON_DEG = 127.5;  // 경도 트리거
	const double CLIMB_DELTA_FT = 2000.0; // 트리거 시 추가로 올라갈 고도
	const double MAX_BANK_DEG = 25.0;  // 선회 최대 뱅크각
	const double Kp_hdg2bank = 0.8;   // 헤딩오차 -> 목표 뱅크각 변환 게인 (deg/deg)

	// 상태 관리
	enum class Phase { CRUISE, CLIMB_TURN, STABILIZE };
	Phase phase = Phase::CRUISE;
	bool  turn_initiated = false;
	double new_heading = 90.0; // 초기 목표와 동일하게 시작
	double last_lon = fdm.GetPropertyValue("position/long-gc-deg");

	// ★ (선택권장) 간단 트림: 종방향 트림 실행 → 수평비행 기준점 확보
	//    JSBSim은 property로 trim을 트리거할 수 있음 (완료 여부는 simulation/trim-completed로 확인 가능)
	fdm.SetPropertyValue("simulation/do_simple_trim", 0);  // 0 = Longitudinal trim
	// trim 완료될 때까지 몇 스텝 진행
	for (int i = 0; i<400; ++i) {
		fdm.Run();
		// 완료 신호가 있으면 중단
		if (fdm.GetPropertyValue("simulation/trim-completed") != 0.0) break;
	} // 참고: do_simple_trim 사용 가능. (필요시 ground trim=2 등) :contentReference[oaicite:0]{index=0}

	// 엔진 시동 / 기어 업
	fdm.SetPropertyValue("propulsion/set-running", -1);
	fdm.SetPropertyValue("gear/gear-cmd-norm", 0.0);

	// 목표
	double target_altitude = 5000.0; // ft
	double target_heading = 90.0;    // deg
	double target_speed = 350.0;   // kts

	// ★ 제어 파라미터 (초기값 제안: 상황에 맞춰 소폭 조정)
	//    고도루프: PD + (옵션) I.  단위 주의: alt(ft), vs(ft/s), q(rad/s)
	const double Kp_alt = 2.0e-4;   // alt 오차 1500ft ≈ elevator 0.3에 도달
	const double Kd_vs = 3.0e-3;   // 승강률 감쇄 (ft/s → cmd)
	const double Kq_damp = 0.5;      // 피치율 감쇄 (rad/s → cmd)
	const double Ki_alt = 0.0;      // 필요 시 1.0e-6 ~ 5.0e-6 정도부터 테스트

	// 스로틀(속도루프)
	double base_throttle = 0.45;     // 트림 후 수평비행 유지에 맞춰 상향 (예시)
	const double Kp_thr = 0.05;     // 속도 비례 이득 (너무 크면 요동)
	const double Ki_thr = 0.001;    // 속도 적분 (천천히)

	// 롤/요 루프
	const double Kp_roll = 0.4;
	const double Kp_rud = 0.02;

	// ★ 신호 부호: 많은 JSBSim 기종에서 elevator < 0 → nose up
	const double ELEVATOR_NOSE_UP = -1.0; // 필요 시 +1.0로 바꿔 테스트

	// 적분항/필터 상태
	double alt_i = 0.0;
	double spd_i = 0.0;
	double last_time = fdm.GetSimTime();



	//통신 초기화
	if (!SendClient_Init("192.253.0.37", 20001)) return 1;

	// 실시간 페이싱 준비    
	timeBeginPeriod(1);
	auto real_prev = SteadyClock::now();
	double acc = 0.0;

	double log_last_sim = 0.0;    // 시뮬시간 기준 1Hz 로깅
	double log_last_real = 0.0;   // 실시간 기준 1Hz 로깅
	double sim_time0 = fdm.GetSimTime();
	auto   real_time0 = SteadyClock::now();


	while (!getRullQuit()) {

		// STOP 상태면 대기 (acc 폭주 방지 위해 기준시각 갱신)
		if (!getRullRun()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
			real_prev = SteadyClock::now();
			continue;
		}

		// ---- 실제 경과시간 누적 ----
		auto real_now = SteadyClock::now();
		double real_dt = std::chrono::duration<double>(real_now - real_prev).count();
		real_prev = real_now;
		acc += real_dt;

		// ---- 누산된 시간만큼 여러 step 실행(필수!) ----
		int steps = 0;

		while (acc >= DT) {
			// 중간에 STOP/QUIT 들어오면 즉시 빠져나오기
			if (getRullQuit() || !getRullRun()) { acc = 0.0; break; }

			// 현재 상태
			double current_alt = fdm.GetPropertyValue("position/h-sl-ft");
			double current_heading = fdm.GetPropertyValue("attitude/psi-deg");
			double current_speed = fdm.GetPropertyValue("velocities/vtrue-kts");
			double current_roll = fdm.GetPropertyValue("attitude/phi-deg");

			// ★ 추가 상태(감쇄용)
			double vs_fps = fdm.GetPropertyValue("velocities/h-dot-fps");   // ft/s (고도 변화율) :contentReference[oaicite:1]{index=1}
			double q_radps = fdm.GetPropertyValue("velocities/q-rad_sec");   // 피치율(rad/s)      :contentReference[oaicite:2]{index=2}

			// (2) 제어계산 (기존 그대로)
			//  - alt_i 적분엔 dt=DT 사용(시뮬 스텝과 동일)
			double alt_error = (target_altitude - current_alt);
			alt_i += alt_error * DT;
			alt_i = clamp(alt_i, -20000.0, 20000.0);

			// ★ 승강률/피치율 감쇄 포함한 엘리베이터 명령
			double elevator_cmd = (Kp_alt * alt_error)
				- (Kd_vs  * vs_fps)
				- (Kq_damp * q_radps)
				+ (Ki_alt * alt_i);

			elevator_cmd = ELEVATOR_NOSE_UP * elevator_cmd;
			elevator_cmd = clamp(elevator_cmd, -0.3, 0.3);               // 너무 크면 헌팅 유발

			// 목표 헤딩은 상태에 따라 달라짐(아래 3) 참고)
			double heading_ref = new_heading;

			// 헤딩 오차(-180~+180)
			double heading_error = current_heading - heading_ref;
			if (heading_error < 0) heading_error += 360;   // 항상 0~360 범위 (왼쪽으로 회전)
			//if (heading_error > 180) heading_error -= 360;
			//if (heading_error < -180) heading_error += 360;

			// 헤딩 오차 -> 목표 뱅크각(포화)
			double phi_cmd = clamp(-Kp_hdg2bank * heading_error, -MAX_BANK_DEG, +MAX_BANK_DEG);

			// 롤 루프: (목표 뱅크각 - 현재 롤)
			double roll_error = (phi_cmd - current_roll);
			double aileron_cmd = clamp(Kp_roll * roll_error, -1.0, 1.0);

			// 요(러더)는 약하게만 보조(필요 없으면 더 줄여도 됨)
			double rudder_cmd = clamp(Kp_rud * heading_error, -0.3, 0.3);


			// --- 속도 루프(스로틀) ---
			double spd_error = (target_speed - current_speed);
			spd_i += spd_error * DT;
			spd_i = clamp(spd_i, -50.0, 50.0);
			double throttle_cmd = base_throttle + (Kp_thr * spd_error) + (Ki_thr * spd_i);
			throttle_cmd = clamp(throttle_cmd, 0.0, 1.0);

			// ----- 유턴 상태머신 -----
			double lon_now = fdm.GetPropertyValue("position/long-gc-deg");

			// (1) 트리거: 경도 127.5를 서쪽->동쪽으로 "넘어설 때" 1회 발동
			if (!turn_initiated) {
				bool crossed_east = (last_lon < TRIGGER_LON_DEG) && (lon_now >= TRIGGER_LON_DEG) || (lon_now <= 126.9);
				if (crossed_east) {
					turn_initiated = true;
					phase = Phase::CLIMB_TURN;

					// 반대 방향으로 목표 헤딩 설정(현재 헤딩 기준 180도)
					new_heading = current_heading + 180.0;
					if (new_heading >= 360.0) new_heading -= 360.0;

					// 피치 업을 유도하기 위해 목표 고도를 잠시 올림
					// (PD 고도 루프가 엘리베이터를 끌어올리게 됨)
					// 아래 target_altitude 변수는 기존 선언을 유지하되 수정 가능하도록 만들어두세요.
					// 여기서는 그냥 변수 값을 바꿉니다.
					// ex) target_altitude는 const가 아니라 double로 선언하면 좋습니다.
					// 위에서 const로 되어 있다면 'double target_altitude = 20000.0;' 로 바꾸세요.
					// (이 파일 맨 위 정의 부분 참고)
				}
			}
			last_lon = lon_now;

			// (2) 상태별 목표 업데이트
			static double cruise_altitude = 5000.0; // 평시 순항고도
			switch (phase) {
			case Phase::CRUISE:
				// 평상시: 원래 목표 유지
				new_heading = 90.0;                 // 초기 목표와 동일
				target_altitude = cruise_altitude;  // 순항
				break;

			case Phase::CLIMB_TURN:
				// 유턴 중 + 약간 상승
				//target_altitude = cruise_altitude + CLIMB_DELTA_FT;

				// 헤딩이 충분히 반대로 돌아갔으면 안정화 단계로
			{
									  double hdg_err = current_heading - new_heading;
									  if (hdg_err > 180) hdg_err -= 360;
									  if (hdg_err < -180) hdg_err += 360;
									  if (std::fabs(hdg_err) < 5.0) { // 오차 5° 이내면 만족
										  phase = Phase::STABILIZE;
									  }
			}
				break;

			case Phase::STABILIZE:
				// 목표 고도 원복, 헤딩 유지
				target_altitude = cruise_altitude;
				turn_initiated = false;

				// 속도/고도/헤딩이 안정되면 다시 CRUISE로 내려가도 되지만
				// 여기서는 계속 유지
				break;
			}

			// 명령 적용
			fdm.SetPropertyValue("fcs/elevator-cmd-norm", elevator_cmd);	// 고도
			fdm.SetPropertyValue("fcs/rudder-cmd-norm", rudder_cmd);		// Yaw
			fdm.SetPropertyValue("fcs/aileron-cmd-norm", aileron_cmd);		// 뱅크각 (회전율)
			fdm.SetPropertyValue("fcs/throttle-cmd-norm", throttle_cmd);	// 속도

			// 1 step
			fdm.Run();
			UpdatePoseFromJSBSim(fdm);
			sendMessage();

			// --- 실시간 120Hz 페이싱 ---
			acc -= DT;
			if (++steps > 10) {        // 과도 추격 제한(상황에 맞게 조정 가능)
				acc = 0.0;
				break;
			}

		}
		double sim_now = fdm.GetSimTime();
		double real_sec = std::chrono::duration<double>(SteadyClock::now() - real_time0).count();

		if (sim_now - log_last_sim >= 1.0) {
			log_last_sim = sim_now;
			double current_time = fdm.GetSimTime();
			double lon = fdm.GetPropertyValue("position/long-gc-deg");
			double lat = fdm.GetPropertyValue("position/lat-geod-deg");
			double alt_m = fdm.GetPropertyValue("position/h-sl-meters");    // 고도[m]  (※ ft 쓰면 ×0.3048)
			double alt_ft = fdm.GetPropertyValue("position/h-sl-ft");
			double heading = fdm.GetPropertyValue("attitude/psi-deg");
			double spd_kts = fdm.GetPropertyValue("velocities/vtrue-kts");
			double spd_fps = fdm.GetPropertyValue("velocities/vtrue-fps");
			double roll = fdm.GetPropertyValue("attitude/phi-deg");        // 롤[deg] (+: 우익 하강)
			double pitch = fdm.GetPropertyValue("attitude/theta-deg");      // 피치[deg] (+: 기수 올림)
			double yaw = fdm.GetPropertyValue("attitude/psi-deg");        // 요/헤딩[deg] (진북 기준 시계방향)

			double tas_mps = fdm.GetPropertyValue("velocities/vtrue-kts") * 0.514444;  // m/s

			tacview << "#" << std::fixed << std::setprecision(3) << current_time << "\n";
			tacview.flush();
			tacview << std::hex << std::uppercase << obj_Id
			<< ",T=" << std::fixed << std::setprecision(6) << lon << "|" << lat
			<< "|" << std::setprecision(2) << alt_m
			<< "|" << std::setprecision(3) << roll
			<< "|" << std::setprecision(3) << pitch
			<< "|" << std::setprecision(3) << yaw
			//<< ",HDG=" << std::setprecision(3) << heading
			<< ",TAS=" << std::setprecision(3) << tas_mps << "\n\n";
			tacview.flush();
			
			std::cout << "Time: " << current_time
			<< ", Lon: " << lon
			<< ", Lat: " << lat
			<< ", Alt(ft): " << alt_ft
			<< ", Speed(kts): " << spd_kts
			<< ", Speed(fps): " << spd_fps
			<< ", Heading(deg): " << heading
			<< std::endl;
			
		}

		// ---- CPU 양보 (0.5~1ms) ----
		std::this_thread::sleep_for(std::chrono::microseconds(1000));
	}

	timeEndPeriod(1);
	SendClient_Close();
	tacview.close();
	return 0;
}