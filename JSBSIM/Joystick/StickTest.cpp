#include "StickTest.h"
#include "FGFDMExec.h"
#include "CommJoystcik.h"
#include "ClientJoystick.h"
#include "ServerJoystick.h"
#include "simgear/misc/sg_path.hxx"
#include <iostream>
#include <windows.h>
#include <fstream>
#include <iomanip>
#include <string>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <thread>
#ifdef _WIN32
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif
#include "../STICK/STICK.h"
#pragma comment(lib, "../Debug/STICK.lib")

StickTest::StickTest(){}
StickTest::~StickTest() {}
StickPoseMsg stickPose = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10000, 10000 };
// steady_clock alias
using SteadyClock = std::chrono::steady_clock;
const double DT = 1.0 / 120.0; // 120 Hz

std::atomic<int> g_run{ 0 };   // 시작 상태: 정지
std::atomic<int> g_quit{ 0 };

int  getStickRun()      { return g_run.load(std::memory_order_acquire); }
void setStickRun(int v) { g_run.store(v ? 1 : 0, std::memory_order_release); }
int  getStickQuit()     { return g_quit.load(std::memory_order_acquire); }
void setStickQuit(int v){ g_quit.store(v ? 1 : 0, std::memory_order_release); }

StickPoseMsg getStickPose() { return stickPose; }

template <typename T>
T clamp(T val, T minVal, T maxVal) {
	if (val < minVal) return minVal;
	else if (val > maxVal) return maxVal;
	else return val;
}

int stickBase(){
	using clock = std::chrono::steady_clock;
	//통신 초기화
	if (!SendClient_Init("192.253.0.37", 20000)) return 1;
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
	int obj_Id = 0x3E7;
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
	fdm.SetPropertyValue("ic/long-gc-deg", 127.01);
	fdm.SetPropertyValue("ic/lat-geod-deg", 37.0);
	fdm.SetPropertyValue("ic/h-sl-ft", 5000.0);
	fdm.SetPropertyValue("ic/psi-true-deg", 90.0);
	fdm.SetPropertyValue("ic/theta-deg", 0.0);
	fdm.SetPropertyValue("ic/phi-deg", 0.0);
	fdm.SetPropertyValue("ic/vt-kts", 500);

	// 초기화
	fdm.RunIC();
	// --- dt를 1/120초로 고정 ---
	fdm.Setdt(DT);

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
	// === 스틱 초기화 ===
	stickInput.mode = 0;     // 0: 실제 조이스틱 모드
	InitStick();

	double target_speed = 350.0;   // kts
	// 스로틀(속도루프)
	double base_throttle = 0.45;     // 트림 후 수평비행 유지에 맞춰 상향 (예시)
	const double Kp_thr = 0.05;     // 속도 비례 이득 (너무 크면 요동)
	const double Ki_thr = 0.001;    // 속도 적분 (천천히)
	double spd_i = 0.0;


	// 실시간 페이싱 준비    
	timeBeginPeriod(1);
	auto real_prev = SteadyClock::now();
	double acc = 0.0;

	double log_last_sim = 0.0;    // 시뮬시간 기준 1Hz 로깅
	double log_last_real = 0.0;   // 실시간 기준 1Hz 로깅
	double sim_time0 = fdm.GetSimTime();
	auto   real_time0 = SteadyClock::now();

	while (!getStickQuit()) {

		// STOP 상태면 대기 (acc 폭주 방지 위해 기준시각 갱신)
		if (!getStickRun()) {
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
			if (getStickQuit() || !getStickRun()) { acc = 0.0; break; }

			// 최신 스틱 값 읽기 → stickCmd 갱신(-1~+1 / 0~1 범위)
			//StepStick();
			fdm.SetPropertyValue("fcs/aileron-cmd-norm", static_cast<double>(stickMsg.rollCmd) / 32767.0);
			fdm.SetPropertyValue("fcs/elevator-cmd-norm", static_cast<double>(stickMsg.pitchCmd) / 32767.0);
			fdm.SetPropertyValue("fcs/rudder-cmd-norm", static_cast<double>(stickMsg.rudderCmd) / 32767.0);
			fdm.SetPropertyValue("fcs/throttle-cmd-norm", static_cast<double>(static_cast<uint16_t>(stickMsg.throttleCmd)) / 65535.0); // 0~1

			/*
			double current_speed = fdm.GetPropertyValue("velocities/vtrue-kts");
			// --- 속도 루프(스로틀) ---
			double spd_error = (target_speed - current_speed);
			spd_i += spd_error * DT;
			spd_i = clamp(spd_i, -50.0, 50.0);
			double throttle_cmd = base_throttle + (Kp_thr * spd_error) + (Ki_thr * spd_i);
			throttle_cmd = clamp(throttle_cmd, 0.0, 1.0);

			// 딱 꽂기: JSBSim FCS에 직접 반영
			// (대부분의 JSBSim 항공기에서 이 프로퍼티 경로가 표준입니다)
			fdm.SetPropertyValue("fcs/aileron-cmd-norm", stickCmd.rollCmd);     // 좌우
			fdm.SetPropertyValue("fcs/elevator-cmd-norm", -stickCmd.pitchCmd);    // 상하
			fdm.SetPropertyValue("fcs/rudder-cmd-norm", stickCmd.rudderCmd);   // 방향타
			fdm.SetPropertyValue("fcs/throttle-cmd-norm", throttle_cmd); // 0~1
			*/


			// 멀티엔진이면 예: fcs/throttle-cmd-norm[0], [1] 각각에 동일 값 세팅
			// fdm.SetPropertyValue("fcs/throttle-cmd-norm[0]", stickCmd.throttleCmd);
			// fdm.SetPropertyValue("fcs/throttle-cmd-norm[1]", stickCmd.throttleCmd);

			//std::cout << "roll: " << stickCmd.rollCmd << ", pitch: " << stickCmd.pitchCmd << ", rudder: " << stickCmd.rudderCmd << ", throttle: " << stickCmd.throttleCmd << std::endl;
			// 시뮬 한 스텝
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
			
			/*
			std::cout << "! STICK TEST ! " << "Time: " << current_time
				<< ", Lon: " << lon
				<< ", Lat: " << lat
				<< ", Alt(ft): " << alt_ft
				<< ", Speed(kts): " << spd_kts
				<< ", Speed(fps): " << spd_fps
				<< ", Heading(deg): " << heading
				<< std::endl;
				*/
		}
		//std::cout << "rullHP: " << stickPose.rullHp << std::endl;
		//std::cout << "stickHP: " << stickPose.stickHp << std::endl;

		// ---- CPU 양보 (0.5~1ms) ----
		std::this_thread::sleep_for(std::chrono::microseconds(1000));
	}

	timeEndPeriod(1);
	SendClient_Close();
	tacview.close();
	return 0;
}