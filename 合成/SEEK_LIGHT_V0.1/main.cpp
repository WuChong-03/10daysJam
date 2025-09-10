// ===============================================================
// main.cpp  —— 单文件外部结构（整合 Title/Slides/Play/Clear/GameOver）
// 使用 Novice 框架。把你的 play.cpp 主循环粘到文末 RunPlayDemo() 里。
// ===============================================================
#pragma region インクルード
#define NOMINMAX
#include <Novice.h>
#include <Windows.h>
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <cstring> // memcpy

// ★ 新增：play 模块用到的标准库
#include <fstream>
#include <sstream>
#include <vector>
#include <queue>
#include <random>
#include <ctime>
#include <string>
#pragma endregion

// ---- forward for play module (used by WinMain) ----
namespace play {
	enum PlayResult {
		PR_Title = 0,
		PR_Clear = 1,
		PR_GameOver = 2
	};
	int PlayMainLoop();            // forward declaration
	extern float g_lastRunTimeSec; // ★ 新增：Clear 计时显示用
}


#pragma region 基本設定
const char kWindowTitle[] = "UI Sandbox Minimal (1920x1080 Fullscreen Canvas)";
const int SCREEN_W = 1920;
const int SCREEN_H = 1080;

enum GameScene { SC_Title, SC_Play, SC_Clear, SC_GameOver };

// 颜色（RGBA：0xRRGGBBAA）
constexpr unsigned int COL_BLACK = 0x000000FFu;
constexpr unsigned int COL_DARK1 = 0x101010FFu;
constexpr unsigned int COL_DARK3 = 0x202020FFu;

// 扫描线参数（全局）
const int   SCAN_STEP = 4;
const int   SCAN_THICK = 2;
const float SCAN_SPEED = 40.0f;
const unsigned int SCAN_COLOR_NORMAL = 0x2E2E2E55u; // 常态
const unsigned int SCAN_COLOR_GREY = 0x80808040u; // 灰线阶段
constexpr unsigned int SCAN_COLOR_DANGER = 0x40404055u; // 红色抖动(AA=0x55)

// 提示图呼吸an
const float FADE_PERIOD_SEC = 1.6f;
const int   MASK_ALPHA_MIN = 10;
const int   MASK_ALPHA_MAX = 255;

// 过场参数
const float FADE_SPEED = 280.0f;   // 渐黑/渐亮速度：每秒 alpha（0~255）
const float BLACK_HOLD_SEC = 1.0f;     // 标题阶段的全黑停留
const float GREY_HOLD_SEC = 1.0f;     // 灰线停留

// 幻灯片 1（road）
const float SLIDE1_DURATION = 6.0f;     // 每段幻灯片时长（秒）
const float ROAD_SCROLL_SPD = 2400.0f;  // 路面向右滚动速度(px/s)
const int   ROAD_TILING_W = SCREEN_W; // 以屏幕宽做循环阈值

// 最终流程（carDoor → 黑屏 → 跑步 → 开门 → Play）
const float CAR_DOOR_DUR_SEC = 1.2f;
const float CAR_DOOR_VOL = 1.0f;
const float POST_BLACK_HOLD_SEC = 0.5f;
const float FINAL_RUN_SEC = 2.0f;
const float OPEN_DOOR_FULL_SEC = 2.0f;
const float RUN_VOL = 0.8f;
const float DOOR_VOL = 0.9f;

// 音量
const float TITLE_BGM_VOL = 0.8f;
const float BUTTON_VOL = 1.0f;
const float TV_VOL = 0.2f;
const float INCAR_VOL = 1.0f; // inCar
const float CAR2_VOL = 1.0f; // car2

// 结局/过渡音量
const float CLEAR_BGM_VOL = 1.0f;
const float GAMEOVER_BGM_VOL = 0.55f;   // 调小点
const float PASSING_VOL = 1.0f;    // 与 Clear BGM 同时播放

// CLEAR 画面：展示与推图
const float CLEAR_SHOW1_SEC = 2.0f;     // clear1 静止展示
const float CLEAR_SLIDE_DUR_SEC = 1.25f;    // 推图总时长（LERP + easeOut，先快后慢）
#pragma endregion

#pragma region 小工具/效果函数


static inline void DrawScanlineOverlay(float timeSec, unsigned int color) {
	int offset = (int)std::fmod(timeSec * SCAN_SPEED, (float)SCAN_STEP);
	if (offset < 0) offset += SCAN_STEP;
	for (int y = offset; y < SCREEN_H; y += SCAN_STEP) {
		Novice::DrawBox(0, y, SCREEN_W, SCAN_THICK, 0.0f, color, kFillModeSolid);
	}
}

static inline void DrawRoadSlide(int roadTex, float scrollX) {
	float off = std::fmod(scrollX, (float)ROAD_TILING_W);
	if (off < 0) off += ROAD_TILING_W;
	int x1 = (int)off - ROAD_TILING_W; // 左块
	int x2 = x1 + ROAD_TILING_W;       // 右块
	Novice::DrawSprite(x1, 0, roadTex, (float)SCREEN_W / ROAD_TILING_W, 1.0f, 0.0f, 0xFFFFFFFFu);
	Novice::DrawSprite(x2, 0, roadTex, (float)SCREEN_W / ROAD_TILING_W, 1.0f, 0.0f, 0xFFFFFFFFu);
}

// clamp/lerp/ease
static inline float Clamp01(float t) { return t < 0.f ? 0.f : (t > 1.f ? 1.f : t); }
static inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
// Ease-Out-Cubic：先快后慢
static inline float EaseOutCubic(float t) { t = Clamp01(t); float u = 1.0f - t; return 1.0f - u * u * u; }
#pragma endregion

#pragma region エントリポイント
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
	Novice::Initialize(kWindowTitle, SCREEN_W, SCREEN_H);
	char keys[256] = { 0 };
	char preKeys[256] = { 0 };
	// 贴图（标题）
	int titleBgTex = Novice::LoadTexture("./assets/title/img/bg.png");
	int titleLogoTex = Novice::LoadTexture("./assets/title/img/logo.png");
	int titleHintTex = Novice::LoadTexture("./assets/title/img/hint.png");
	int titleHintMaskTex = Novice::LoadTexture("./assets/title/img/hint_mask.png");


	// 幻灯片贴图
	int roadTex = Novice::LoadTexture("./assets/title/img/road.png");
	int carTex = Novice::LoadTexture("./assets/title/img/car.png");
	int newsTex = Novice::LoadTexture("./assets/title/img/news.png");
	int houseTex = Novice::LoadTexture("./assets/title/img/house.png");

	// 结局贴图
	// Clear
	int clear1Tex = Novice::LoadTexture("./assets/end/img/clear1.png");
	int clear2Tex = Novice::LoadTexture("./assets/end/img/clear2.png");
	int clearMojiTex = Novice::LoadTexture("./assets/end/img/clear_moji.png");
	int clearMojiMaskTex = Novice::LoadTexture("./assets/end/img/clear_moji_mask.png");
	// GameOver
	int gameOverBgTex = Novice::LoadTexture("./assets/end/img/gameOver.png");
	int gameOverMojiTex = Novice::LoadTexture("./assets/end/img/gameOver_moji.png");
	int gameOverMojiMaskTex = Novice::LoadTexture("./assets/end/img/gameOver_moji_mask.png");

	// 音频（Title & Slides）
	int bgmId = Novice::LoadAudio("./assets/title/bgm/bgm.mp3");
	int sfxBtnId = Novice::LoadAudio("./assets/common/sfx/button.mp3");
	int sfxTvId = Novice::LoadAudio("./assets/common/sfx/tv.mp3");
	int sfxInCarId = Novice::LoadAudio("./assets/title/sfx/inCar.mp3");
	int sfxCar2Id = Novice::LoadAudio("./assets/title/sfx/car2.mp3");
	int sfxCarDoorId = Novice::LoadAudio("./assets/title/sfx/carDoor.mp3");

	// Play 结尾音效
	int sfxRunId = Novice::LoadAudio("./assets/play/player/sfx/running.mp3");
	int sfxOpenDoorId = Novice::LoadAudio("./assets/play/player/sfx/openDoor.mp3");

	// 结局/过渡 BGM
	int clearBgmId = Novice::LoadAudio("./assets/end/bgm/clear.mp3");
	int gameOverBgmId = Novice::LoadAudio("./assets/end/bgm/gameOver.mp3");
	int passingBgmId = Novice::LoadAudio("./assets/end/bgm/passing.mp3");

	// voice 句柄
	int voiceBgm = -1, voiceBtn = -1, voiceTv = -1, voiceInCar = -1;
	int voiceCar2 = -1, voiceCarDoor = -1, voiceRun = -1, voiceOpenDoor = -1;
	int voiceClearBgm = -1, voiceGameOverBgm = -1, voicePassing = -1;

	auto StopVoice = [](int& v) { if (v >= 0) { Novice::StopAudio(v); v = -1; } };

	GameScene scene = SC_Title;
	float sceneTime = 0.0f;
	const float DT = 1.0f / 60.0f;

	// 标题演出状态
	bool  fadeActive = false;     float fadeAlpha = 0.0f;
	bool  blackHoldPhase = false; float blackHoldRemainSec = 0.0f;
	bool  scanGreyPhase = false;  float scanGreyRemainSec = 0.0f;

	// 幻灯片状态
	bool  slidePhase = false;
	int   slideIndex = 0;          // 0=无，1=road+car，2=news，3=house
	float slideRemainSec = 0.0f;
	float roadScrollX = 0.0f;

	// 幻灯片间灰闪
	bool  slideGreyPhase = false;
	float slideGreyRemainSec = 0.0f;
	int   slideNextIndex = 0;

	// house 段 car2 定时停止
	float car2RemainSec = 0.0f;

	// 最终阶段（Title→Play 的黑屏音效演出）
	bool  finalCarDoorPhase = false;   float finalCarDoorRemain = 0.0f;
	bool  finalFadePhase = false;      float finalFadeAlpha = 0.0f;
	bool  finalBlackHoldPhase = false; float finalBlackRemain = 0.0f;
	bool  finalAudioPhase = false;     float finalRunRemain = 0.0f, finalDoorRemain = 0.0f;
	bool  finalDoorStarted = false;

	// CLEAR 场景
	float clearShow1Remain = 0.0f;  // 先静止展示 clear1
	float clearSlideT = 0.0f;       // 进度 0→1，用 easeOutCubic
	bool  clearSlidePhase = false;  // 是否在推图
	bool  clearMojiPhase = false;   // 是否显示 moji

	// 通用场景过渡（淡出→切换→淡入）
	enum TransPhase { TR_None, TR_FadeOut, TR_Switch, TR_FadeIn };
	TransPhase trPhase = TR_None;
	float      trAlpha = 0.0f;     // 叠加的黑幕 alpha
	GameScene  trTarget = SC_Title;

	auto StopAllVoices = [&]() {
		StopVoice(voiceBgm); StopVoice(voiceBtn); StopVoice(voiceTv);
		StopVoice(voiceInCar); StopVoice(voiceCar2); StopVoice(voiceCarDoor);
		StopVoice(voiceRun);  StopVoice(voiceOpenDoor);
		// 不在此停 clear/gameover/passing —— 它们在切场景时分支处理
		};

	auto StopVoicesForSwitch = [&](GameScene target) {
		// 先停常规 voice
		StopAllVoices();
		// 进入 Clear：不要停 passing/clearBgm（让它们继续）
		if (target != SC_Clear) {
			StopVoice(voicePassing);
			StopVoice(voiceClearBgm);
		}
		// 进入 GameOver：若不是 GO 就停掉 GO BGM
		if (target != SC_GameOver) {
			StopVoice(voiceGameOverBgm);
		}
		};

	auto ResetForTitle = [&]() {
		// 标题/过场状态复位
		fadeActive = false; fadeAlpha = 0.0f;
		blackHoldPhase = false; blackHoldRemainSec = 0.0f;
		scanGreyPhase = false; scanGreyRemainSec = 0.0f;

		slidePhase = false; slideRemainSec = 0.0f; roadScrollX = 0.0f; slideIndex = 0;
		slideGreyPhase = false; slideGreyRemainSec = 0.0f; slideNextIndex = 0;

		car2RemainSec = 0.0f;

		finalCarDoorPhase = false; finalCarDoorRemain = 0.0f;
		finalFadePhase = false; finalFadeAlpha = 0.0f;
		finalBlackHoldPhase = false; finalBlackRemain = 0.0f;
		finalAudioPhase = false; finalRunRemain = 0.0f; finalDoorRemain = 0.0f; finalDoorStarted = false;

		// CLEAR 状态复位
		clearShow1Remain = 0.0f;
		clearSlideT = 0.0f;
		clearSlidePhase = false;
		clearMojiPhase = false;
		};

	auto InitForSceneEnter = [&](GameScene target) {
		scene = target;
		sceneTime = 0.0f;

		if (target == SC_Title) {
			ResetForTitle();
		}
		else if (target == SC_Play) {
			// Play：交由 TR_Switch 中的 RunPlayDemo() 直接执行（此处无需额外初始化）
		}
		else if (target == SC_Clear) {
			// CLEAR（展示→推图→moji）
			clearShow1Remain = CLEAR_SHOW1_SEC;
			clearSlideT = 0.0f;
			clearSlidePhase = false;
			clearMojiPhase = false;

			// 进入 Clear 时：passing 与 clearBgm 并行播放
			if (clearBgmId >= 0 && voiceClearBgm < 0) voiceClearBgm = Novice::PlayAudio(clearBgmId, true, CLEAR_BGM_VOL);
			if (passingBgmId >= 0 && voicePassing < 0) voicePassing = Novice::PlayAudio(passingBgmId, false, PASSING_VOL);
		}
		else if (target == SC_GameOver) {
			// 进入 GameOver：仅播放较小音量的 GameOver BGM
			if (gameOverBgmId >= 0 && voiceGameOverBgm < 0) voiceGameOverBgm = Novice::PlayAudio(gameOverBgmId, true, GAMEOVER_BGM_VOL);
		}
		};

	// 支持“从全黑直接切换再淡入”以避免露底（house 闪一下）
	auto StartTransitionTo = [&](GameScene target, bool fromBlack = false) {
		if (trPhase != TR_None) return;
		trTarget = target;
		if (fromBlack) {
			trPhase = TR_Switch;
			trAlpha = 255.0f; // 直接在黑幕中切场景
		}
		else {
			trPhase = TR_FadeOut;
			trAlpha = 0.0f;
		}
		};

	// ==== 主循环 ====
	while (Novice::ProcessMessage() == 0) {
		Novice::BeginFrame();
		std::memcpy(preKeys, keys, 256);
		Novice::GetHitKeyStateAll(keys);


		sceneTime += DT;

		// 若正在做通用过渡，屏蔽触发输入（避免连发）
		bool blockingInput = (trPhase != TR_None);

		// 输入：Enter
		if (!preKeys[DIK_RETURN] && keys[DIK_RETURN]) {
			if (!blockingInput) {
				if (scene == SC_Title) {
					// Title 的 Enter：启动内部演出（非场景切换）→ 播按钮音
					if (!fadeActive && !blackHoldPhase && !scanGreyPhase && !slidePhase) {
						if (sfxBtnId >= 0) { voiceBtn = Novice::PlayAudio(sfxBtnId, false, BUTTON_VOL); }
						fadeActive = true; fadeAlpha = 0.0f;
					}
					// 快进 Enter（跳过 slide）——不要播按钮音
					if (slidePhase && !slideGreyPhase && !finalCarDoorPhase && !finalFadePhase && !finalBlackHoldPhase && !finalAudioPhase) {
						slideRemainSec = 0.0f; // 跳过当前 slide
					}
				}
				else if (scene == SC_Clear || scene == SC_GameOver) {
					// Clear/GameOver → Title：按 Enter 切回 Title，要播按钮音
					if (sfxBtnId >= 0) { voiceBtn = Novice::PlayAudio(sfxBtnId, false, BUTTON_VOL); }
					StartTransitionTo(SC_Title); // 正常淡出→切换→淡入
				}
			}
		}

		// GameOver：按 R Retry 回 Play（需要按钮音）
		if (scene == SC_GameOver && !blockingInput) {
			if (!preKeys[DIK_R] && keys[DIK_R]) {
				if (sfxBtnId >= 0) { voiceBtn = Novice::PlayAudio(sfxBtnId, false, BUTTON_VOL); }
				StartTransitionTo(SC_Play);
			}
		}

		// 清屏
		Novice::DrawBox(0, 0, SCREEN_W, SCREEN_H, 0.0f, COL_BLACK, kFillModeSolid);

		// ===== 场景绘制 =====
		if (scene == SC_Title) {
			// 背景 BGM（仅在无额外演出/过渡时播放）
			if (!fadeActive && !blackHoldPhase && !scanGreyPhase && !slidePhase && trPhase == TR_None) {
				if (voiceBgm < 0 && bgmId >= 0) {
					voiceBgm = Novice::PlayAudio(bgmId, true, TITLE_BGM_VOL);
				}
			}

			// 标题静态画面
			if (!blackHoldPhase && !scanGreyPhase && !slidePhase) {
				if (titleBgTex >= 0)    Novice::DrawSprite(0, 0, titleBgTex, 1.0f, 1.0f, 0.0f, 0xFFFFFFFFu);
				if (titleLogoTex >= 0)  Novice::DrawSprite(0, 0, titleLogoTex, 1.0f, 1.0f, 0.0f, 0xFFFFFFFFu);
				if (titleHintTex >= 0)  Novice::DrawSprite(0, 0, titleHintTex, 1.0f, 1.0f, 0.0f, 0xFFFFFFFFu);
				if (titleHintMaskTex >= 0) {
					float phase = std::fmod(sceneTime / FADE_PERIOD_SEC, 1.0f);
					float tri = (phase < 0.5f) ? (phase * 2.0f) : (2.0f - phase * 2.0f);
					int   a = (int)(MASK_ALPHA_MIN + (MASK_ALPHA_MAX - MASK_ALPHA_MIN) * tri + 0.5f);
					unsigned int colMask = (0xFFFFFF00u | (unsigned)a);
					Novice::DrawSprite(0, 0, titleHintMaskTex, 1.0f, 1.0f, 0.0f, colMask);
				}
			}

			// 标题内部演出链
			if (fadeActive) {
				fadeAlpha += FADE_SPEED * DT;
				if (fadeAlpha >= 255.0f) {
					fadeAlpha = 255.0f;
					fadeActive = false;
					blackHoldPhase = true;
					blackHoldRemainSec = BLACK_HOLD_SEC;
				}
				unsigned int a = (unsigned)std::clamp(fadeAlpha, 0.0f, 255.0f);
				Novice::DrawBox(0, 0, SCREEN_W, SCREEN_H, 0.0f, (0x00000000u | a), kFillModeSolid);
			}
			else if (blackHoldPhase) {
				Novice::DrawBox(0, 0, SCREEN_W, SCREEN_H, 0.0f, 0x000000FFu, kFillModeSolid);
				blackHoldRemainSec -= DT;
				if (blackHoldRemainSec <= 0.0f) {
					blackHoldPhase = false;
					scanGreyPhase = true;
					scanGreyRemainSec = GREY_HOLD_SEC;
					StopVoice(voiceBgm);
					if (sfxTvId >= 0) voiceTv = Novice::PlayAudio(sfxTvId, false, TV_VOL);
				}
			}
			else if (scanGreyPhase) {
				scanGreyRemainSec -= DT;
				if (scanGreyRemainSec <= 0.0f) {
					scanGreyPhase = false;
					StopVoice(voiceTv);
					// 进入幻灯片 1
					slidePhase = true;
					slideIndex = 1;
					slideRemainSec = SLIDE1_DURATION;
					roadScrollX = 0.0f;
					if (sfxInCarId >= 0 && voiceInCar < 0) {
						voiceInCar = Novice::PlayAudio(sfxInCarId, /*loop=*/true, /*vol=*/INCAR_VOL);
					}
				}
			}
			else if (slidePhase) {
				if (slideGreyPhase) {
					Novice::DrawBox(0, 0, SCREEN_W, SCREEN_H, 0.0f, 0x000000FFu, kFillModeSolid);
					slideGreyRemainSec -= DT;
					if (slideGreyRemainSec <= 0.0f) {
						slideGreyPhase = false;
						StopVoice(voiceTv);
						slideIndex = slideNextIndex;
						slideRemainSec = SLIDE1_DURATION;
						if (slideIndex != 1) StopVoice(voiceInCar);
						if (slideIndex == 3) {
							car2RemainSec = 0.5f;
							if (sfxCar2Id >= 0) { StopVoice(voiceCar2); voiceCar2 = Novice::PlayAudio(sfxCar2Id, false, CAR2_VOL); }
						}
					}
				}
				else if (finalCarDoorPhase) {
					if (houseTex >= 0) Novice::DrawSprite(0, 0, houseTex, 1.0f, 1.0f, 0.0f, 0xFFFFFFFFu);
					finalCarDoorRemain -= DT;
					if (finalCarDoorRemain <= 0.0f) {
						finalCarDoorPhase = false;
						finalFadePhase = true;
						finalFadeAlpha = 0.0f;
					}
				}
				else if (finalFadePhase) {
					if (houseTex >= 0) Novice::DrawSprite(0, 0, houseTex, 1.0f, 1.0f, 0.0f, 0xFFFFFFFFu);
					finalFadeAlpha += FADE_SPEED * DT;
					if (finalFadeAlpha >= 255.0f) {
						finalFadeAlpha = 255.0f;
						finalFadePhase = false;
						finalBlackHoldPhase = true;
						finalBlackRemain = POST_BLACK_HOLD_SEC;
					}
					unsigned int a = (unsigned)std::clamp(finalFadeAlpha, 0.0f, 255.0f);
					Novice::DrawBox(0, 0, SCREEN_W, SCREEN_H, 0.0f, (0x00000000u | a), kFillModeSolid);
				}
				else if (finalBlackHoldPhase) {
					Novice::DrawBox(0, 0, SCREEN_W, SCREEN_H, 0.0f, 0x000000FFu, kFillModeSolid);
					finalBlackRemain -= DT;
					if (finalBlackRemain <= 0.0f) {
						finalBlackHoldPhase = false;
						finalAudioPhase = true;
						finalRunRemain = FINAL_RUN_SEC;
						finalDoorRemain = OPEN_DOOR_FULL_SEC;
						finalDoorStarted = false;
						if (sfxRunId >= 0) { StopVoice(voiceRun); voiceRun = Novice::PlayAudio(sfxRunId, true, RUN_VOL); }
					}
				}
				else if (finalAudioPhase) {
					// 始终全黑，避免露出 house
					Novice::DrawBox(0, 0, SCREEN_W, SCREEN_H, 0.0f, 0x000000FFu, kFillModeSolid);

					if (finalRunRemain > 0.0f) {
						finalRunRemain -= DT;
						if (finalRunRemain <= 0.0f) {
							StopVoice(voiceRun);
							if (!finalDoorStarted && sfxOpenDoorId >= 0) {
								finalDoorStarted = true;
								StopVoice(voiceOpenDoor);
								voiceOpenDoor = Novice::PlayAudio(sfxOpenDoorId, false, DOOR_VOL);
							}
						}
					}
					else {
						finalDoorRemain -= DT;
						if (finalDoorRemain <= 0.0f) {
							finalAudioPhase = false;
							StartTransitionTo(SC_Play, /*fromBlack=*/true);
						}
					}
				}
				else {
					if (slideIndex == 1) {
						if (roadTex >= 0) { roadScrollX += ROAD_SCROLL_SPD * DT; DrawRoadSlide(roadTex, roadScrollX); }
						if (carTex >= 0) { Novice::DrawSprite(0, 0, carTex, 1.0f, 1.0f, 0.0f, 0xFFFFFFFFu); }
						slideRemainSec -= DT;
						if (slideRemainSec <= 0.0f) {
							StopVoice(voiceInCar);
							slideGreyPhase = true;
							slideGreyRemainSec = GREY_HOLD_SEC;
							slideNextIndex = 2;
							if (sfxTvId >= 0) { StopVoice(voiceTv); voiceTv = Novice::PlayAudio(sfxTvId, false, TV_VOL); }
						}
					}
					else if (slideIndex == 2) {
						if (newsTex >= 0) { Novice::DrawSprite(0, 0, newsTex, 1.0f, 1.0f, 0.0f, 0xFFFFFFFFu); }
						slideRemainSec -= DT;
						if (slideRemainSec <= 0.0f) {
							slideGreyPhase = true;
							slideGreyRemainSec = GREY_HOLD_SEC;
							slideNextIndex = 3;
							if (sfxTvId >= 0) { StopVoice(voiceTv); voiceTv = Novice::PlayAudio(sfxTvId, false, TV_VOL); }
						}
					}
					else if (slideIndex == 3) {
						if (houseTex >= 0) { Novice::DrawSprite(0, 0, houseTex, 1.0f, 1.0f, 0.0f, 0xFFFFFFFFu); }

						if (car2RemainSec > 0.0f) {
							car2RemainSec -= DT;
							if (car2RemainSec <= 0.0f) {
								car2RemainSec = 0.0f;
								StopVoice(voiceCar2);
								if (!finalCarDoorPhase) {
									finalCarDoorPhase = true;
									finalCarDoorRemain = CAR_DOOR_DUR_SEC;
									if (sfxCarDoorId >= 0) {
										StopVoice(voiceCarDoor);
										voiceCarDoor = Novice::PlayAudio(sfxCarDoorId, false, CAR_DOOR_VOL);
									}
								}
							}
						}

						slideRemainSec -= DT;
						if (slideRemainSec <= 0.0f) {
							if (!finalCarDoorPhase) {
								StopVoice(voiceCar2);
								finalCarDoorPhase = true;
								finalCarDoorRemain = CAR_DOOR_DUR_SEC;
								if (sfxCarDoorId >= 0) {
									StopVoice(voiceCarDoor);
									voiceCarDoor = Novice::PlayAudio(sfxCarDoorId, false, CAR_DOOR_VOL);
								}
							}
						}
					}
				}
			} // end SC_Title
		}
		else if (scene == SC_Play) {
			// 合并后：SC_Play 的实际渲染与循环在 TR_Switch -> RunPlayDemo() 里执行
			// 这里不做渲染，避免重复。
		}
		else if (scene == SC_Clear) {
			// Clear：不要 StopAllVoices，以保留 passing + clearBgm 共鸣
			// 1) clear1 静止展示
			if (!clearSlidePhase && !clearMojiPhase) {
				if (clear1Tex >= 0) {
					Novice::DrawSprite(0, 0, clear1Tex, 1.0f, 1.0f, 0.0f, 0xFFFFFFFFu);
				}
				clearShow1Remain -= DT;
				if (clearShow1Remain <= 0.0f) {
					clearShow1Remain = 0.0f;
					clearSlidePhase = true;
					clearSlideT = 0.0f; // 开始 LERP
				}
			}
			// 2) 推图：easeOutCubic
			else if (clearSlidePhase && !clearMojiPhase) {
				clearSlideT += DT / CLEAR_SLIDE_DUR_SEC;
				float t = EaseOutCubic(Clamp01(clearSlideT));

				float clear1X = Lerp(0.0f, -static_cast<float>(SCREEN_W), t);
				float clear2X = Lerp(static_cast<float>(SCREEN_W), 0.0f, t);

				if (clear1Tex >= 0) {
					Novice::DrawSprite((int)clear1X, 0, clear1Tex, 1.0f, 1.0f, 0.0f, 0xFFFFFFFFu); 
				}
				
				if (clear2Tex >= 0) {
					Novice::DrawSprite((int)clear2X, 0, clear2Tex, 1.0f, 1.0f, 0.0f, 0xFFFFFFFFu);
				}
					

				if (clearSlideT >= 1.0f) {
					clearSlideT = 1.0f;
					clearSlidePhase = false;
					clearMojiPhase = true;
				}
			}
			// 3) moji 呼吸
			else {
				if (clear2Tex >= 0) {
					Novice::DrawSprite(0, 0, clear2Tex, 1.0f, 1.0f, 0.0f, 0xFFFFFFFFu);
				}
				if (clearMojiTex >= 0) {
					Novice::DrawSprite(0, 0, clearMojiTex, 1.0f, 1.0f, 0.0f, 0xFFFFFFFFu);
				}
				if (clearMojiMaskTex >= 0) {
					float phase = std::fmod(sceneTime / FADE_PERIOD_SEC, 1.0f);
					float tri = (phase < 0.5f) ? (phase * 2.0f) : (2.0f - phase * 2.0f);
					int   a = (int)(MASK_ALPHA_MIN + (MASK_ALPHA_MAX - MASK_ALPHA_MIN) * tri + 0.5f);
					unsigned int colMask = (0xFFFFFF00u | (unsigned)a);
					Novice::DrawSprite(0, 0, clearMojiMaskTex, 1.0f, 1.0f, 0.0f, colMask);
				}
			}
		}
		else if (scene == SC_GameOver) {
			if (gameOverBgTex >= 0) {
				Novice::DrawSprite(0, 0, gameOverBgTex, 1.0f, 1.0f, 0.0f, 0xFFFFFFFFu);
			}
			if (gameOverMojiTex >= 0) {
				Novice::DrawSprite(0, 0, gameOverMojiTex, 1.0f, 1.0f, 0.0f, 0xFFFFFFFFu);
			}
			if (gameOverMojiMaskTex >= 0) {
				float phase = std::fmod(sceneTime / FADE_PERIOD_SEC, 1.0f);
				float tri = (phase < 0.5f) ? (phase * 2.0f) : (2.0f - phase * 2.0f);
				int   a = (int)(MASK_ALPHA_MIN + (MASK_ALPHA_MAX - MASK_ALPHA_MIN) * tri + 0.5f);
				unsigned int colMask = (0xFFFFFF00u | (unsigned)a);
				Novice::DrawSprite(0, 0, gameOverMojiMaskTex, 1.0f, 1.0f, 0.0f, colMask);
			}
		}

		// === 通用场景过渡（最后叠加，盖住一切）===
		if (trPhase == TR_FadeOut) {
			trAlpha += FADE_SPEED * DT;
			if (trAlpha > 255.0f) trAlpha = 255.0f;
			unsigned int a = (unsigned)trAlpha;
			Novice::DrawBox(0, 0, SCREEN_W, SCREEN_H, 0.0f, (0x00000000u | a), kFillModeSolid);

			if (trAlpha >= 255.0f) {
				trPhase = TR_Switch;
			}
		}
		if (trPhase == TR_Switch) {
			// 切场景的一瞬间
			StopVoicesForSwitch(trTarget);

			// 进入 Play：在黑幕中直接跑子循环，跑完回来
			if (trTarget == SC_Play) {
				// 跑 Play 子循环，拿到结果码
				int result = play::PlayMainLoop();

				// 用结果决定切到哪个场景
				if (result == play::PR_Clear) {
					trTarget = SC_Clear;
				}
				else if (result == play::PR_GameOver) {
					trTarget = SC_GameOver;
				}
				else {
					// 其它情况（例如按 ESC 或返回标题）
					trTarget = SC_Title;
				}
			}


			// 切场景
			if (trTarget == SC_Title) {
				ResetForTitle();
			}
			InitForSceneEnter(trTarget);

			// 切完后开始淡入
			trPhase = TR_FadeIn;
		}
		if (trPhase == TR_FadeIn) {
			trAlpha -= FADE_SPEED * DT;
			if (trAlpha < 0.0f) trAlpha = 0.0f;
			unsigned int a = (unsigned)trAlpha;
			Novice::DrawBox(0, 0, SCREEN_W, SCREEN_H, 0.0f, (0x00000000u | a), kFillModeSolid);

			if (trAlpha <= 0.0f) {
				trPhase = TR_None; // 过渡结束
			}
		}

		// === 全局扫描线（最上层；灰闪阶段用灰色，其余用常态）===
		bool anyGrey = scanGreyPhase || slideGreyPhase;
		DrawScanlineOverlay(sceneTime, anyGrey ? SCAN_COLOR_GREY : SCAN_COLOR_NORMAL);

		// ESC 退出（仅在外层场景时响应；Play 在子循环内部自己处理）
		if (!preKeys[DIK_ESCAPE] && keys[DIK_ESCAPE] && scene != SC_Play && trPhase == TR_None) break;

		Novice::EndFrame();
	}

	Novice::Finalize();
	return 0;
}
#pragma endregion

// ===================================================================
// 嵌入的 Play 子循环 —— 把你的 play.cpp 主循环粘进【替换区域】。
// 规则：
// 1) 这里不要 Novice::Initialize/Finalize（外层已做）
// 2) 可以自己 BeginFrame/EndFrame（外层在黑幕中等待）
// 3) 退出条件你自定（例如按 ESC，或通关/死亡）；
//    如需把结果回传外层，可改成 int 返回值并在 TR_Switch 处理中使用。
// ===================================================================
// 子循环：调用真正的 Play 主循环
// ========================== PLAY MODULE BEGIN ==========================
namespace play {

#pragma region 基本設定と定数（封装在命名空间，避免与外层重名）

	const float CAM_LERP = 0.15f;
	const float CAM_SNAP = 0.5f;

	const int TILE = 64;
	const int MAP_COLS = 94;
	const int MAP_ROWS = 94;
	const int MAP_W_PX = MAP_COLS * TILE;
	const int MAP_H_PX = MAP_ROWS * TILE;

	const int   PLAYER_W = 64;
	const int   PLAYER_H = 80;
	const float HALF_W = PLAYER_W * 0.5f;
	const float HALF_H = PLAYER_H * 0.5f;
	// 仅用于碰撞
	const int   PLY_COLL_W = 33;
	const int   PLY_COLL_H = 52;
	const float PLY_COLL_HALF_W = PLY_COLL_W * 0.5f;
	const float PLY_COLL_HALF_H = PLY_COLL_H * 0.5f;

	// 敌人碰撞盒（和玩家同规格，命中更公平）
	const int   ENEMY_COLL_W = 33;
	const int   ENEMY_COLL_H = 52;

	constexpr float EPS = 0.001f;

	const char* LIGHT_TEX_PATH = "./assets/play/env/img/light_mask.png";
	const int   LIGHT_TEX_BASE = 1024;
	static int  gLightTex = -1;

	// 灯光/电量
	const float R_MIN = 120.0f;
	const float R_MAX = 600.0f;
	const float FUEL_MAX = 100.0f;
	const float DRAIN_PER_FRAME = FUEL_MAX / (35.0f * 60.0f);
	const float RADIUS_LERP = 0.20f;

	// 三个灯光档位对应半径
	const float LIGHT_LEVELS[3] = { 180.0f, 280.0f, 550.0f };
	const float DRAIN_MULT[3] = { 0.6f, 1.0f, 1.6f };

	// 颜色（注意与外层分离作用域）
	constexpr unsigned int COL_WHITE = 0xFFFFFFFFu;
	constexpr unsigned int COL_BLACK_P = 0x000000FFu; // 避免与外层 COL_BLACK 同名
	constexpr unsigned int COL_BATT = 0x00FF00FFu;
	constexpr unsigned int COL_ITEM = 0xFF00FFFFu;
	constexpr unsigned int COL_PLY = 0x55AAFFFFu;
	constexpr unsigned int COL_MM_BG = 0x000000A0u;
	constexpr unsigned int COL_MM_P = 0x00FFFFFFu;
	constexpr unsigned int COL_ENEMY = 0xFF0000FFu;

	const float BATT_RESPAWN_SEC = 90.0f;
	const float FRAME_SEC = 1.0f / 60.0f;
	const float PICK_RADIUS = 48.0f;
	const int   DOT_SZ = 20;

	const int MINI_W = 288;
	const int MINI_H = 288;
	const int MINI_MARGIN = 12;

	const int   ENEMY_W = 64;
	const int   ENEMY_H = 80;
	const float ENEMY_SPEED = 3.2f;
	const int ENEMIES_PER_QUADRANT = 3;   // ★ 敌人数量入口
	const float BACKTRACK_CHANCE_WHEN_ALT_EXISTS = 0.0f;
	// 固定视野半径保留但不用于“发现玩家”
	const float ENEMY_VIEW_RADIUS = 260.0f;
	const float CHASE_REPATH_SEC = 0.25f;
	const float CHASE_LOST_SECONDS = 10.0f;

	// 动画
	const float ENEMY_ANIM_FPS = 20.0f;
	const int ENEMY_FRAMES = 7;
	const float ENEMY_ANIM_DT = 1.0f / ENEMY_ANIM_FPS;

	const int   PLAYER_FRAMES = 9;
	const float PLAYER_ANIM_FPS = 20.0f;
	const float PLAYER_ANIM_DT = 1.0f / PLAYER_ANIM_FPS;

	// Idle 动画
	const int   PLAYER_IDLE_FRAMES = 4;
	const float PLAYER_IDLE_FPS = 8.0f;                  // 待机较慢
	const float PLAYER_IDLE_ANIM_DT = 1.0f / PLAYER_IDLE_FPS;

	// 渲染缩放
	const float RENDER_SCALE = 1.5f;
	inline float ViewW() { return ::SCREEN_W / RENDER_SCALE; }
	inline float ViewH() { return ::SCREEN_H / RENDER_SCALE; }

	// BGM参数
	const float DANGER_ENTER_DIST = 220.0f; // 保留
	const float DANGER_EXIT_DIST = 270.0f; // 保留
	const float DANGER_LINGER_SEC = 4.0f;
	const float ALERT_LINGER_SEC = 2.5f;

	const float MAIN_BGM_VOL = 0.50f;
	const float ALERT_BGM_VOL = 0.45f;   // 原 0.80
	const float DANGER_BGM_VOL = 0.55f;   // 原 0.85

	const float FADE_UP_PER_SEC = 1.50f;
	const float FADE_DOWN_PER_SEC = 1.30f;

	const float ALERT_MAX_DIST = 500.0f;
	const float DANGER_VOL_FAR_WHEN_CHASE = 650.0f;
	const float DANGER_VOL_MIN_WHEN_CHASE = 0.12f;

	const float DANGER_TAIL_IN_STAGE1 = 0.6f; // 保留

	const int  MAX_BATT_ACTIVE = 6;   // 同时最多在地图上存在的电池数
	const bool BATT_ENABLE_RESPAWN = true; // 关掉=不再刷新
#pragma endregion

#pragma region 小工具/数据结构（封装）
	inline bool AABBOverlap(float cx1, float cy1, int w1, int h1,
		float cx2, float cy2, int w2, int h2) {
		float hw1 = w1 * 0.5f, hh1 = h1 * 0.5f;
		float hw2 = w2 * 0.5f, hh2 = h2 * 0.5f;
		return (fabsf(cx1 - cx2) <= (hw1 + hw2)) && (fabsf(cy1 - cy2) <= (hh1 + hh2));
	}

	struct BatterySpawn { float x, y; bool alive; float timer; };
	struct Collectible { float x, y; bool alive; };

	inline float Dist2(float x1, float y1, float x2, float y2) {
		float dx = x1 - x2, dy = y1 - y2; return dx * dx + dy * dy;
	}
	inline void TileCenter(int r, int c, float& x, float& y) {
		x = c * (float)TILE + TILE * 0.5f;
		y = r * (float)TILE + TILE * 0.5f;
	}
	inline void WorldToRC(float x, float y, int& r, int& c) {
		c = (int)floorf(x / (float)TILE);
		r = (int)floorf(y / (float)TILE);
		if (r < 0) r = 0; if (r >= MAP_ROWS) r = MAP_ROWS - 1;
		if (c < 0) c = 0; if (c >= MAP_COLS) c = MAP_COLS - 1;
	}
#pragma endregion

#pragma region マップ/CSV
	enum TileType : int { T_WALL = 0, T_PATH = 1, T_SAFE = 2, T_BATT = 3, T_ITEM = 4 };
	static int tileGrid[MAP_ROWS][MAP_COLS];

	static bool LoadTileCSV(const char* path) {
		std::ifstream fin(path);
		if (!fin.is_open()) return false;
		std::string line; int r = 0;
		while (std::getline(fin, line) && r < MAP_ROWS) {
			std::stringstream ss(line);
			std::string cell; int c = 0;
			while (std::getline(ss, cell, ',') && c < MAP_COLS) {
				while (!cell.empty() && (cell.back() == '\r' || cell.back() == ' ')) cell.pop_back();
				while (!cell.empty() && cell.front() == ' ') cell.erase(cell.begin());
				int v = 1; if (!cell.empty()) { try { v = std::stoi(cell); } catch (...) { v = 1; } }
				if (v < 0 || v > 4) v = 1;
				tileGrid[r][c++] = v;
			}
			while (c < MAP_COLS) tileGrid[r][c++] = 1;
			++r;
		}
		while (r < MAP_ROWS) { for (int c = 0; c < MAP_COLS; ++c) tileGrid[r][c] = 1; ++r; }
		return true;
	}

	inline int CollAtRC(int r, int c) {
		if (r < 0 || r >= MAP_ROWS || c < 0 || c >= MAP_COLS) return 1;
		return (tileGrid[r][c] == T_WALL) ? 1 : 0;
	}
#pragma endregion

#pragma region 路线CSV（1~4 巡逻线）
	static int routeGrid[MAP_ROWS][MAP_COLS];

	static bool LoadRouteCSV(const char* path) {
		std::ifstream fin(path);
		if (!fin.is_open()) return false;
		std::string line; int r = 0;
		while (std::getline(fin, line) && r < MAP_ROWS) {
			std::stringstream ss(line);
			std::string cell; int c = 0;
			while (std::getline(ss, cell, ',') && c < MAP_COLS) {
				while (!cell.empty() && (cell.back() == '\r' || cell.back() == ' ')) cell.pop_back();
				while (!cell.empty() && cell.front() == ' ') cell.erase(cell.begin());
				int v = 0; if (!cell.empty()) { try { v = std::stoi(cell); } catch (...) { v = 0; } }
				if (v < 0) v = 0; if (v > 4) v = 4;
				routeGrid[r][c++] = v;
			}
			while (c < MAP_COLS) routeGrid[r][c++] = 0;
			++r;
		}
		while (r < MAP_ROWS) { for (int c = 0; c < MAP_COLS; ++c) routeGrid[r][c] = 0; ++r; }
		return true;
	}
#pragma endregion

#pragma region 敌人AI（巡逻/追击/返航 + 动画）
	inline bool EnemyWalkableTile(int r, int c) {
		if (r < 0 || r >= MAP_ROWS || c < 0 || c >= MAP_COLS) return false;
		int t = tileGrid[r][c];
		return (t != T_WALL && t != T_SAFE);
	}
	inline bool InQuadrant(int r, int c, int routeId) {
		int midR = MAP_ROWS / 2, midC = MAP_COLS / 2;
		switch (routeId) {
		case 1: return (r < midR && c < midC);
		case 2: return (r < midR && c >= midC);
		case 3: return (r >= midR && c < midC);
		case 4: return (r >= midR && c >= midC);
		default: return true;
		}
	}
	inline bool RoutePassableWithId(int r, int c, int routeId) {
		if (!EnemyWalkableTile(r, c)) return false;
		if (!InQuadrant(r, c, routeId)) return false;
		return (routeGrid[r][c] == routeId);
	}
	inline bool HasRouteNeighborWithId(int r, int c, int routeId) {
		const int dr[4] = { 1,-1,0,0 };
		const int dc[4] = { 0,0,1,-1 };
		for (int k = 0; k < 4; ++k) {
			int nr = r + dr[k], nc = c + dc[k];
			if (RoutePassableWithId(nr, nc, routeId)) return true;
		}
		return false;
	}

	static bool BFSPath(int sr, int sc, int gr, int gc, std::vector<std::pair<int, int>>& out) {
		out.clear();
		if (!EnemyWalkableTile(sr, sc) || !EnemyWalkableTile(gr, gc)) return false;

		static int vis[MAP_ROWS][MAP_COLS];
		static std::pair<int, int> pre[MAP_ROWS][MAP_COLS];
		for (int r = 0; r < MAP_ROWS; ++r)
			for (int c = 0; c < MAP_COLS; ++c) { vis[r][c] = 0; pre[r][c] = { -1,-1 }; }

		std::queue<std::pair<int, int>> q;
		q.push({ sr,sc }); vis[sr][sc] = 1;
		const int dr[4] = { 1,-1,0,0 }, dc[4] = { 0,0,1,-1 };
		bool found = false;

		while (!q.empty()) {
			auto u = q.front(); q.pop();
			if (u.first == gr && u.second == gc) { found = true; break; }
			for (int k = 0; k < 4; ++k) {
				int nr = u.first + dr[k], nc = u.second + dc[k];
				if (nr < 0 || nr >= MAP_ROWS || nc < 0 || nc >= MAP_COLS) continue;
				if (vis[nr][nc]) continue;
				if (!EnemyWalkableTile(nr, nc)) continue;
				vis[nr][nc] = 1; pre[nr][nc] = u; q.push({ nr,nc });
			}
		}
		if (!found) return false;

		std::vector<std::pair<int, int>> rev;
		int r = gr, c = gc;
		while (!(r == sr && c == sc)) { rev.push_back({ r,c }); auto p = pre[r][c]; r = p.first; c = p.second; }
		rev.push_back({ sr,sc });
		out.assign(rev.rbegin(), rev.rend());
		return true;
	}

	static bool BFSPath8(int sr, int sc, int gr, int gc, std::vector<std::pair<int, int>>& out) {
		out.clear();
		if (!EnemyWalkableTile(sr, sc) || !EnemyWalkableTile(gr, gc)) return false;

		static int vis[MAP_ROWS][MAP_COLS];
		static std::pair<int, int> pre[MAP_ROWS][MAP_COLS];
		for (int r = 0; r < MAP_ROWS; ++r)
			for (int c = 0; c < MAP_COLS; ++c) { vis[r][c] = 0; pre[r][c] = { -1,-1 }; }

		std::queue<std::pair<int, int>> q;
		q.push({ sr,sc }); vis[sr][sc] = 1;

		const int dr[8] = { 1,-1,0,0,  1,1,-1,-1 };
		const int dc[8] = { 0,0,1,-1, 1,-1, 1,-1 };

		auto canDiag = [&](int r0, int c0, int r1, int c1)->bool {
			int vr = r1 - r0, vc = c1 - c0;
			if (abs(vr) + abs(vc) != 2) return true;
			return EnemyWalkableTile(r0, c0 + vc) && EnemyWalkableTile(r0 + vr, c0);
			};

		bool found = false;
		while (!q.empty()) {
			auto u = q.front(); q.pop();
			if (u.first == gr && u.second == gc) { found = true; break; }
			for (int k = 0; k < 8; ++k) {
				int nr = u.first + dr[k], nc = u.second + dc[k];
				if (nr < 0 || nr >= MAP_ROWS || nc < 0 || nc >= MAP_COLS) continue;
				if (vis[nr][nc]) continue;
				if (!EnemyWalkableTile(nr, nc)) continue;
				if (!canDiag(u.first, u.second, nr, nc)) continue;
				vis[nr][nc] = 1; pre[nr][nc] = u; q.push({ nr,nc });
			}
		}
		if (!found) return false;

		std::vector<std::pair<int, int>> rev;
		int r = gr, c = gc;
		while (!(r == sr && c == sc)) { rev.push_back({ r,c }); auto p = pre[r][c]; r = p.first; c = p.second; }
		rev.push_back({ sr,sc });
		out.assign(rev.rbegin(), rev.rend());
		return true;
	}

	static bool FindNearestRouteTileById(int sr, int sc, int routeId, int& rr, int& rc) {
		static int vis[MAP_ROWS][MAP_COLS];
		for (int r = 0; r < MAP_ROWS; ++r) for (int c = 0; c < MAP_COLS; ++c) vis[r][c] = 0;

		std::queue<std::pair<int, int>> q;
		q.push({ sr,sc }); vis[sr][sc] = 1;

		const int dr[4] = { 1,-1,0,0 }, dc[4] = { 0,0,1,-1 };
		while (!q.empty()) {
			auto u = q.front(); q.pop();
			if (InQuadrant(u.first, u.second, routeId) &&
				routeGrid[u.first][u.second] == routeId &&
				EnemyWalkableTile(u.first, u.second)) {
				rr = u.first; rc = u.second; return true;
			}
			for (int k = 0; k < 4; ++k) {
				int nr = u.first + dr[k], nc = u.second + dc[k];
				if (nr < 0 || nr >= MAP_ROWS || nc < 0 || nc >= MAP_COLS) continue;
				if (vis[nr][nc]) continue;
				if (!EnemyWalkableTile(nr, nc)) continue;
				vis[nr][nc] = 1; q.push({ nr,nc });
			}
		}
		return false;
	}

	enum EnemyState { ES_Patrol, ES_Chase, ES_Return };
	enum EnemyDir { ED_Front = 0, ED_Back = 1, ED_Left = 2, ED_Right = 3 };

	struct Enemy {
		float x = 0.f, y = 0.f;
		int   curR = 0, curC = 0;
		int   tgtR = 0, tgtC = 0;
		int   prevR = -1, prevC = -1;
		int   routeId = 1;
		EnemyState state = ES_Patrol;

		std::vector<std::pair<int, int>> path;
		int   pathIdx = 0;
		float repathTimer = 0.0f;
		float lostTimer = 0.0f;

		int   dir = ED_Front;
		int   frame = 0;
		float animAccum = 0.0f;
		float lastVX = 0.0f, lastVY = 0.0f;
	};

	static void EnemyStepToTarget(Enemy& e) {
		float tx, ty; TileCenter(e.tgtR, e.tgtC, tx, ty);
		float dx = tx - e.x, dy = ty - e.y;

		e.lastVX = 0.0f; e.lastVY = 0.0f;
		if (fabsf(dx) >= fabsf(dy)) {
			float step = copysignf(std::min(fabsf(dx), ENEMY_SPEED), dx);
			e.x += step; e.lastVX = step;
		}
		else {
			float step = copysignf(std::min(fabsf(dy), ENEMY_SPEED), dy);
			e.y += step; e.lastVY = step;
		}

		if (e.x < HALF_W) e.x = HALF_W;
		if (e.y < HALF_H) e.y = HALF_H;
		if (e.x > MAP_W_PX - HALF_W) e.x = MAP_W_PX - HALF_W;
		if (e.y > MAP_H_PX - HALF_H) e.y = MAP_H_PX - HALF_H;
	}

	static void EnemyStepToTargetFree(Enemy& e) {
		float tx, ty; TileCenter(e.tgtR, e.tgtC, tx, ty);
		float dx = tx - e.x, dy = ty - e.y;
		float len = sqrtf(dx * dx + dy * dy);
		e.lastVX = 0.0f; e.lastVY = 0.0f;
		if (len > 0.0001f) {
			float vx = dx / len * ENEMY_SPEED;
			float vy = dy / len * ENEMY_SPEED;
			if (fabsf(vx) > fabsf(dx)) vx = dx;
			if (fabsf(vy) > fabsf(dy)) vy = dy;
			e.x += vx; e.y += vy;
			e.lastVX = vx; e.lastVY = vy;
		}
		if (e.x < HALF_W) e.x = HALF_W;
		if (e.y < HALF_H) e.y = HALF_H;
		if (e.x > MAP_W_PX - HALF_W) e.x = MAP_W_PX - HALF_W;
		if (e.y > MAP_H_PX - HALF_H) e.y = MAP_H_PX - HALF_H;
	}

	static void ChooseNextOnRoute(Enemy& e, std::mt19937& rng) {
		if (!RoutePassableWithId(e.curR, e.curC, e.routeId)) return;

		struct RC { int r, c; };
		RC cand[4]; int n = 0;
		const int dr[4] = { 1,-1,0,0 }, dc[4] = { 0,0,1,-1 };
		for (int k = 0; k < 4; ++k) {
			int nr = e.curR + dr[k], nc = e.curC + dc[k];
			if (RoutePassableWithId(nr, nc, e.routeId)) cand[n++] = { nr,nc };
		}
		if (n == 0) { e.tgtR = (e.prevR >= 0) ? e.prevR : e.curR; e.tgtC = (e.prevC >= 0) ? e.prevC : e.curC; return; }

		std::vector<RC> poolNoBack, poolAll;
		for (int i = 0; i < n; ++i) {
			poolAll.push_back(cand[i]);
			if (!(cand[i].r == e.prevR && cand[i].c == e.prevC)) poolNoBack.push_back(cand[i]);
		}
		std::uniform_int_distribution<int> pickAll(0, (int)poolAll.size() - 1);
		if (!poolNoBack.empty()) {
			std::uniform_real_distribution<float> uf(0.f, 1.f);
			if (uf(rng) < BACKTRACK_CHANCE_WHEN_ALT_EXISTS) {
				auto rc = poolAll[pickAll(rng)]; e.tgtR = rc.r; e.tgtC = rc.c;
			}
			else {
				std::uniform_int_distribution<int> pick(0, (int)poolNoBack.size() - 1);
				auto rc = poolNoBack[pick(rng)]; e.tgtR = rc.r; e.tgtC = rc.c;
			}
		}
		else {
			auto rc = poolAll[pickAll(rng)]; e.tgtR = rc.r; e.tgtC = rc.c;
		}
	}
#pragma endregion

#pragma region 遮罩绘制（光圈PNG）
	static inline void DrawDarkCircleHole(int cx, int cy, int radius, unsigned int blackARGB = COL_BLACK_P) {
		if (gLightTex < 0 || radius <= 0) {
			Novice::DrawBox(0, 0, ::SCREEN_W, ::SCREEN_H, 0.0f, blackARGB, kFillModeSolid);
			return;
		}
		float diameter = 2.0f * (float)radius;
		float s = diameter / (float)LIGHT_TEX_BASE;
		int   w = (int)(LIGHT_TEX_BASE * s);
		int   h = w;
		int   x = cx - w / 2;
		int   y = cy - h / 2;

		if (y > 0)                        Novice::DrawBox(0, 0, ::SCREEN_W, y, 0.0f, blackARGB, kFillModeSolid);
		if (y + h < ::SCREEN_H)           Novice::DrawBox(0, y + h, ::SCREEN_W, ::SCREEN_H - (y + h), 0.0f, blackARGB, kFillModeSolid);
		int midY = y; if (midY < 0) midY = 0;
		int midH = h; if (midY + midH > ::SCREEN_H) midH = ::SCREEN_H - midY; if (midH < 0) midH = 0;
		if (x > 0 && midH > 0)            Novice::DrawBox(0, midY, x, midH, 0.0f, blackARGB, kFillModeSolid);
		if (x + w < ::SCREEN_W && midH > 0) Novice::DrawBox(x + w, midY, ::SCREEN_W - (x + w), midH, 0.0f, blackARGB, kFillModeSolid);

		Novice::DrawSprite(x, y, gLightTex, s, s, 0.0f, COL_WHITE);
	}
#pragma endregion

	// ======================= 外层调用的入口函数 =======================
	// 注意：没有 Initialize/Finalize；外层已经做好。
	// 返回值目前未使用，你可以改为返回 1/2 表示 Clear/GameOver。
	int PlayMainLoop() {
		// 资源
		int mapTex = Novice::LoadTexture("./assets/play/env/img/map.png");
		gLightTex = Novice::LoadTexture(LIGHT_TEX_PATH);

		int pauseBgTex = Novice::LoadTexture("./assets/ui/img/pause_bg.png");
		int pauseRetTex = Novice::LoadTexture("./assets/ui/img/return.png");
		int pauseBackTex = Novice::LoadTexture("./assets/ui/img/backToTitle.png");

		// === UI: 剩余数量 moji & 0-9 数字（64x64） ===
		int uiRemainMojiTex = Novice::LoadTexture("./assets/ui/img/moji.png");
		int uiSpaceTex = Novice::LoadTexture("./assets/ui/img/space.png");
		int uiDigitRemain[10];
		for (int i = 0; i < 10; ++i) {
			char path[256];
			std::snprintf(path, sizeof(path), "./assets/ui/img/moji_%d.png", i);
			uiDigitRemain[i] = Novice::LoadTexture(path);
		}

		// ★ 电量 UI 贴图（0/10/40/70/100）
		int uiFuel0 = Novice::LoadTexture("./assets/ui/img/0.png");
		int uiFuel10 = Novice::LoadTexture("./assets/ui/img/10.png");
		int uiFuel40 = Novice::LoadTexture("./assets/ui/img/40.png");
		int uiFuel70 = Novice::LoadTexture("./assets/ui/img/70.png");
		int uiFuel100 = Novice::LoadTexture("./assets/ui/img/100.png");

		// ★ 灯光档位 UI（0=没电, 1~3=一/二/三档）
		int uiLight[4] = {
			Novice::LoadTexture("./assets/ui/img/light_0.png"),
			Novice::LoadTexture("./assets/ui/img/light_1.png"),
			Novice::LoadTexture("./assets/ui/img/light_2.png"),
			Novice::LoadTexture("./assets/ui/img/light_3.png"),
		};
		// ★ 空格操作音效（切档或充电）
		int sfxLight = Novice::LoadAudio("./assets/common/sfx/light.mp3");

		// ★ 尝试式加载（给 " collection.png" 这种意外前置空格兜底）
		auto LoadTextureTry = [](const char* p1, const char* p2)->int {
			int id = Novice::LoadTexture(p1);
			if (id >= 0) return id;
			return Novice::LoadTexture(p2);
			};

		// ★ 拾取物贴图：收藏品 & 电池
		int collectTex = LoadTextureTry("./assets/play/env/img/collection.png",
			"./assets/play/env/img/ collection.png"); // 前面带空格的兜底
		int batteryTex = Novice::LoadTexture("./assets/play/env/img/battery.png");

		// ★ 拾取音效
		int sfxGet = Novice::LoadAudio("./assets/play/env/sfx/get.mp3");

		// 以瓦片 64px 为基准，拾取图标画成 48px 更协调
		const int   PICKUP_BASE = 64;
		const int   PICKUP_DRAW = 48;
		const float PICKUP_SCALE = (float)PICKUP_DRAW / (float)PICKUP_BASE;

		// 电池库存UI：x1 ~ x7
		int battStackTex[8]; // 用 1..7，下标 0 忽略
		for (int i = 0; i < 8; ++i) battStackTex[i] = -1;
		for (int i = 1; i <= 7; ++i) {
			char path[256];
			std::snprintf(path, sizeof(path), "./assets/ui/img/x%d.png", i);
			battStackTex[i] = Novice::LoadTexture(path);
		}

		bool paused = false;
		int  pauseSel = 0; // 0 = Return, 1 = BackToTitle

		// 安全设置音量
		auto SetVoiceVol = [](int voice, float vol) {
			if (voice >= 0) {
				if (vol < 0.0f) vol = 0.0f;
				if (vol > 1.0f) vol = 1.0f;
				Novice::SetAudioVolume(voice, vol);
			}
			};

		// 线性靠近
		auto ApproachLinear = [](float cur, float tgt, float ratePerSec)->float {
			float step = ratePerSec * FRAME_SEC;
			if (cur < tgt) { cur += step; if (cur > tgt) cur = tgt; }
			else if (cur > tgt) { cur -= step; if (cur < tgt) cur = tgt; }
			return cur;
			};

		// Audio: 仅主 BGM + 跑步 + 暂停声（无追击音效）
		int sfxBgm = Novice::LoadAudio("./assets/play/bgm/play.mp3");
		int sfxRun = Novice::LoadAudio("./assets/play/player/sfx/running.mp3");
		int sfxPause = Novice::LoadAudio("./assets/ui/sfx/pause.mp3");

		int voiceBgm = -1;
		int voiceRun = -1;
		if (sfxBgm >= 0) {
			voiceBgm = Novice::PlayAudio(sfxBgm, /*loop=*/true, /*volume=*/MAIN_BGM_VOL);
		}

		float volMain = MAIN_BGM_VOL; // 只保留主 BGM 音量

		// 玩家贴图
		int playerTex[4][PLAYER_FRAMES];
		int playerIdleTex[4][PLAYER_IDLE_FRAMES];
		for (int d = 0; d < 4; ++d) {
			for (int f = 0; f < PLAYER_FRAMES; ++f)        playerTex[d][f] = -1;
			for (int f = 0; f < PLAYER_IDLE_FRAMES; ++f)   playerIdleTex[d][f] = -1;
		}

		auto loadDir4 = [&](const char* dirName, int dIdx) {
			for (int i = 0; i < PLAYER_FRAMES; ++i) {
				char path[256];
				std::snprintf(path, sizeof(path), "./assets/play/player/img/%s_%02d.png", dirName, i);
				playerTex[dIdx][i] = Novice::LoadTexture(path);
			}
			};
		auto loadIdleDir4 = [&](const char* dirName, int dIdx) {
			for (int i = 0; i < PLAYER_IDLE_FRAMES; ++i) {
				char path[256];
				std::snprintf(path, sizeof(path), "./assets/play/player/img/%s_%02d.png", dirName, i);
				playerIdleTex[dIdx][i] = Novice::LoadTexture(path);
			}
			};
		loadDir4("front", 0);
		loadDir4("back", 1);
		loadDir4("left", 2);
		loadDir4("right", 3);

		// 待机
		loadIdleDir4("idle_front", 0);
		loadIdleDir4("idle_back", 1);
		loadIdleDir4("idle_left", 2);
		loadIdleDir4("idle_right", 3);

		// 敌人贴图
		int enemyTex[4][ENEMY_FRAMES];
		for (int d = 0; d < 4; ++d)
			for (int f = 0; f < ENEMY_FRAMES; ++f)
				enemyTex[d][f] = -1;

		auto loadEnemyDirN = [&](const char* dirName, int dIdx) {
			for (int i = 0; i < ENEMY_FRAMES; ++i) {
				char path[256];
				std::snprintf(path, sizeof(path), "./assets/play/enemy/img/%s_%02d.png", dirName, i);
				enemyTex[dIdx][i] = Novice::LoadTexture(path);
			}
			};
		loadEnemyDirN("front", 0);
		loadEnemyDirN("back", 1);
		loadEnemyDirN("left", 2);
		loadEnemyDirN("right", 3);

		// 玩家动画状态
		enum PlayerDir { PD_Front = 0, PD_Back = 1, PD_Left = 2, PD_Right = 3 };
		int   playerDir = PD_Front;

		// 行走动画
		int   playerFrame = 0;
		float playerAnimAccum = 0.0f;

		// 待机动画
		int   playerIdleFrame = 0;
		float playerIdleAccum = 0.0f;

		(void)LoadTileCSV("./assets/play/env/collision_94x94.csv");
		(void)LoadRouteCSV("./assets/play/env/route_combined_94x94.csv");

		char keys[256] = { 0 }, preKeys[256] = { 0 };

		// 初始玩家/相机
		float playerX = 256.0f, playerY = 256.0f;
		float speed = 5.0f;
		float camX = std::clamp(playerX - ::SCREEN_W * 0.5f, 0.0f, (float)(MAP_W_PX - ::SCREEN_W));
		float camY = std::clamp(playerY - ::SCREEN_H * 0.5f, 0.0f, (float)(MAP_H_PX - ::SCREEN_H));

		// 灯光/电量
		float fuel = FUEL_MAX;
		int   batteryInv = 4, collectedCnt = 0; // 收集物计数

		// 灯光三档：默认中档
		int   lightLevel = 1; // 0/1/2 => 180/280/550
		float targetRadius = LIGHT_LEVELS[lightLevel];
		float radius = targetRadius;

		// 刷新点
		std::vector<BatterySpawn> batteries;
		std::vector<Collectible>  collectibles;

		auto RebuildPickups = [&]() {
			batteries.clear();
			collectibles.clear();

			std::vector<int> battIndices;

			for (int r = 0; r < MAP_ROWS; ++r) {
				for (int c = 0; c < MAP_COLS; ++c) {
					int t = tileGrid[r][c];
					float x, y; TileCenter(r, c, x, y);

					if (t == T_BATT) {
						batteries.push_back({ x, y, false, 0.0f });
						battIndices.push_back((int)batteries.size() - 1);
					}
					else if (t == T_ITEM) {
						collectibles.push_back({ x, y, true });
					}
				}
			}

			// 随机挑选一部分激活（不超过 MAX_BATT_ACTIVE）
			std::mt19937 shufRng((unsigned)time(nullptr));
			std::shuffle(battIndices.begin(), battIndices.end(), shufRng);

			int aliveN = std::min((int)battIndices.size(), MAX_BATT_ACTIVE);
			for (int i = 0; i < aliveN; ++i) {
				batteries[battIndices[i]].alive = true;
			}
			for (int i = aliveN; i < (int)battIndices.size(); ++i) {
				batteries[battIndices[i]].timer = (float)(i - aliveN) * 3.0f;
			}
			};
		RebuildPickups();

		// 小地图
		const float miniSX = (float)MINI_W / (float)MAP_W_PX;
		const float miniSY = (float)MINI_H / (float)MAP_H_PX;
		const int   miniX = ::SCREEN_W - MINI_W - MINI_MARGIN;
		const int   miniY = MINI_MARGIN;

		// RNG
		std::mt19937 rng(static_cast<unsigned>(time(nullptr)));

		// 敌人容器
		std::vector<Enemy> enemies;
		enemies.reserve(4);

		// 敌人颜色
		auto enemyColor = [](int id) -> unsigned int {
			switch (id) {
			case 1: return 0xFF66CCFFu; // 粉
			case 2: return 0xFFA500FFu; // 橙
			case 3: return 0xFF0000FFu; // 红
			case 4: return 0x1E90FFFFu; // 蓝
			default: return COL_ENEMY;
			}
			};

		// 生成指定巡逻线上的敌人
		auto spawnEnemyOnRoute = [&](int routeId) {
			std::vector<std::pair<int, int>> pool;
			for (int r = 0; r < MAP_ROWS; ++r) {
				for (int c = 0; c < MAP_COLS; ++c) {
					if (routeGrid[r][c] == routeId &&
						RoutePassableWithId(r, c, routeId) &&
						HasRouteNeighborWithId(r, c, routeId)) {
						pool.push_back({ r, c });
					}
				}
			}

			if (!pool.empty()) {
				std::uniform_int_distribution<int> pick(0, static_cast<int>(pool.size()) - 1);
				auto rc = pool[pick(rng)];

				float sx, sy;
				TileCenter(rc.first, rc.second, sx, sy);

				Enemy e{};
				e.x = sx;  e.y = sy;
				e.curR = rc.first;  e.curC = rc.second;
				e.tgtR = e.curR;    e.tgtC = e.curC;
				e.prevR = -1;       e.prevC = -1;
				e.routeId = routeId;
				e.state = ES_Patrol;

				e.dir = ED_Front;
				e.frame = 0;
				e.animAccum = 0.0f;
				e.lastVX = 0.0f;
				e.lastVY = 0.0f;

				ChooseNextOnRoute(e, rng);
				enemies.push_back(e);
			}
			};

		auto RebuildEnemies = [&]() {
			enemies.clear();
			for (int rid = 1; rid <= 4; ++rid) {
				for (int i = 0; i < ENEMIES_PER_QUADRANT; ++i) {
					spawnEnemyOnRoute(rid);
				}
			}
			};
		RebuildEnemies();

		// 重置游戏（胜利/失败均调用）
		auto ResetGame = [&]() {
			if (voiceRun >= 0) { Novice::StopAudio(voiceRun); voiceRun = -1; }

			// 玩家
			playerX = 256.0f; playerY = 256.0f;
			playerDir = PD_Front; playerFrame = 0; playerAnimAccum = 0.0f;
			speed = 5.0f;

			// 相机
			camX = std::clamp(playerX - ::SCREEN_W * 0.5f, 0.0f, (float)(MAP_W_PX - ::SCREEN_W));
			camY = std::clamp(playerY - ::SCREEN_H * 0.5f, 0.0f, (float)(MAP_H_PX - ::SCREEN_H));

			// 灯光/电量
			fuel = FUEL_MAX;
			batteryInv = 4;
			collectedCnt = 0;
			lightLevel = 1;
			targetRadius = LIGHT_LEVELS[lightLevel];
			radius = targetRadius;

			// 拾取和敌人
			RebuildPickups();
			RebuildEnemies();

			// BGM音量回到主
			volMain = MAIN_BGM_VOL;
			SetVoiceVol(voiceBgm, volMain);
			};

		float scanTime = 0.0f;  // ★ 扫描线用时间计数器


		// 剩余数字的轻微下落动画（切换时 0.10 秒）
		int   lastRemainShown = -1;
		float remainAnimT = 0.0f;           // 剩余的动画时间
		const float REMAIN_ANIM_DUR = 0.10f;

		// ===== 子循环 =====
		while (Novice::ProcessMessage() == 0) {
			Novice::BeginFrame();
			memcpy(preKeys, keys, 256);
			Novice::GetHitKeyStateAll(keys);

			// === Pause: P 进入；菜单用上下+Enter；Return 恢复 / BackToTitle 退出 ===
			if (!preKeys[DIK_P] && keys[DIK_P] && !paused) {
				paused = true;
				pauseSel = 0; // 默认 Return
				// 停声（进入真正静音）
				if (voiceRun >= 0) { Novice::StopAudio(voiceRun);  voiceRun = -1; }
				if (voiceBgm >= 0) { Novice::StopAudio(voiceBgm);  voiceBgm = -1; }
				// 暂停提示音
				if (sfxPause >= 0) { Novice::PlayAudio(sfxPause, false, 1.0f); }
			}

			if (paused) {
				// 下：从 Return -> BackToTitle（到头不再移动，也不播声）
				if (!preKeys[DIK_DOWN] && keys[DIK_DOWN]) {
					if (pauseSel < 1) {
						pauseSel++;
						if (sfxPause >= 0) Novice::PlayAudio(sfxPause, /*loop=*/false, /*vol=*/1.0f);
					}
				}
				// 上：从 BackToTitle -> Return（到头不再移动，也不播声）
				if (!preKeys[DIK_UP] && keys[DIK_UP]) {
					if (pauseSel > 0) {
						pauseSel--;
						if (sfxPause >= 0) Novice::PlayAudio(sfxPause, false, 1.0f);
					}
				}
				// Enter：确认当前选项
				if (!preKeys[DIK_RETURN] && keys[DIK_RETURN]) {
					if (sfxPause >= 0) Novice::PlayAudio(sfxPause, false, 1.0f);  // 确认音
					if (pauseSel == 0) {
						// Return：恢复游戏 & 继续BGM（只有主 BGM）
						paused = false;
						if (sfxBgm >= 0) voiceBgm = Novice::PlayAudio(sfxBgm, true, volMain);
					}
					else {
						// Back to Title：停声并退出
						if (voiceRun >= 0) { Novice::StopAudio(voiceRun);  voiceRun = -1; }
						if (voiceBgm >= 0) { Novice::StopAudio(voiceBgm);  voiceBgm = -1; }
						return 0;
					}
				}

				// === 渲染暂停画面（在黑色闪烁下层）===
				if (pauseBgTex >= 0) Novice::DrawSprite(0, 0, pauseBgTex, 1.0f, 1.0f, 0.0f, COL_WHITE);
				if (pauseSel == 0) {
					if (pauseRetTex >= 0) Novice::DrawSprite(0, 0, pauseRetTex, 1.0f, 1.0f, 0.0f, COL_WHITE);
				}
				else {
					if (pauseBackTex >= 0) Novice::DrawSprite(0, 0, pauseBackTex, 1.0f, 1.0f, 0.0f, COL_WHITE);
				}
				// 扫描线在暂停 UI 上方
				scanTime += FRAME_SEC;
				::DrawScanlineOverlay(scanTime, ::SCAN_COLOR_NORMAL);

				Novice::EndFrame();
				continue; // 真·暂停：本帧到此结束，不再更新游戏
			}
			// Space：没电充电，否则切换档位
			// Space：没电时尝试消耗电池充电；否则切换档位
			if (!preKeys[DIK_SPACE] && keys[DIK_SPACE]) {
				bool changed = false;
				if (fuel <= 0.0f) {
					if (batteryInv > 0) {
						batteryInv--;
						fuel = FUEL_MAX;
						changed = true;
					}
					// else: 没电且无电池，不做事也不播音（需要“失败提示音”再说）
				}
				else {
					// 0/1/2 => 一/二/三档（图标用 +1）
					lightLevel = (lightLevel + 1) % 3;
					changed = true;
				}
				if (changed && sfxLight >= 0) {
					Novice::PlayAudio(sfxLight, /*loop=*/false, /*volume=*/1.0f);
				}
			}


			// 移动输入（归一化）
			int ix = 0, iy = 0;
			if (keys[DIK_A] || keys[DIK_LEFT])  ix -= 1;
			if (keys[DIK_D] || keys[DIK_RIGHT]) ix += 1;
			if (keys[DIK_W] || keys[DIK_UP])    iy -= 1;
			if (keys[DIK_S] || keys[DIK_DOWN])  iy += 1;

			float dx = 0.0f, dy = 0.0f;
			if (ix != 0 || iy != 0) {
				float len = sqrtf((float)(ix * ix + iy * iy));
				float inv = 1.0f / len;
				dx = ix * speed * inv;
				dy = iy * speed * inv;
			}

			// 跑步声
			bool isMoving = (ix != 0 || iy != 0);
			if (isMoving) {
				if (voiceRun < 0 && sfxRun >= 0) {
					voiceRun = Novice::PlayAudio(sfxRun, /*loop=*/true, /*volume=*/0.8f);
				}
			}
			else {
				if (voiceRun >= 0) { Novice::StopAudio(voiceRun); voiceRun = -1; }
			}

			// 面向（根据输入）
			if (ix != 0 || iy != 0) {
				if (abs(ix) >= abs(iy))  playerDir = (ix > 0) ? PD_Right : PD_Left;
				else                     playerDir = (iy > 0) ? PD_Front : PD_Back;
			}

			// 动画推进
			if (isMoving) {
				// 行走动画
				playerAnimAccum += FRAME_SEC;
				while (playerAnimAccum >= PLAYER_ANIM_DT) {
					playerAnimAccum -= PLAYER_ANIM_DT;
					playerFrame = (playerFrame + 1) % PLAYER_FRAMES;
				}
				// 切到移动时，把 idle 累积清空（可选）
				playerIdleAccum = 0.0f;
			}
			else {
				// 待机动画
				playerIdleAccum += FRAME_SEC;
				while (playerIdleAccum >= PLAYER_IDLE_ANIM_DT) {
					playerIdleAccum -= PLAYER_IDLE_ANIM_DT;
					playerIdleFrame = (playerIdleFrame + 1) % PLAYER_IDLE_FRAMES;
				}
			}

			// 玩家碰撞（X）
			if (dx != 0.0f) {
				float newX = playerX + dx;
				float top = playerY - PLY_COLL_HALF_H, bottom = playerY + PLY_COLL_HALF_H;
				int r0 = (int)floorf(top / (float)TILE);
				int r1 = (int)floorf((bottom - EPS) / (float)TILE);
				r0 = std::clamp(r0, 0, MAP_ROWS - 1);
				r1 = std::clamp(r1, 0, MAP_ROWS - 1);
				if (dx > 0.0f) {
					float right = newX + PLY_COLL_HALF_W;
					int c = (int)floorf((right - EPS) / (float)TILE);
					c = std::min(c, MAP_COLS - 1);
					bool hit = false; for (int r = r0; r <= r1; ++r) if (CollAtRC(r, c)) { hit = true; break; }
					if (hit) newX = (float)(c * TILE) - PLY_COLL_HALF_W;
				}
				else {
					float left = newX - PLY_COLL_HALF_W;
					int c = (int)floorf(left / (float)TILE);
					c = std::max(c, 0);
					bool hit = false; for (int r = r0; r <= r1; ++r) if (CollAtRC(r, c)) { hit = true; break; }
					if (hit) newX = (float)((c + 1) * TILE) + PLY_COLL_HALF_W;
				}
				playerX = newX;
			}

			// 玩家碰撞（Y）
			if (dy != 0.0f) {
				float newY = playerY + dy;
				float left = playerX - PLY_COLL_HALF_W, right = playerX + PLY_COLL_HALF_W;
				int c0 = (int)floorf(left / (float)TILE);
				int c1 = (int)floorf((right - EPS) / (float)TILE);
				c0 = std::clamp(c0, 0, MAP_COLS - 1);
				c1 = std::clamp(c1, 0, MAP_COLS - 1);
				if (dy > 0.0f) {
					float bottom = newY + PLY_COLL_HALF_H;
					int r = (int)floorf((bottom - EPS) / (float)TILE);
					r = std::min(r, MAP_ROWS - 1);
					bool hit = false; for (int c = c0; c <= c1; ++c) if (CollAtRC(r, c)) { hit = true; break; }
					if (hit) newY = (float)(r * TILE) - PLY_COLL_HALF_H;
				}
				else {
					float top = newY - PLY_COLL_HALF_H;
					int r = (int)floorf(top / (float)TILE);
					r = std::max(r, 0);
					bool hit = false; for (int c = c0; c <= c1; ++c) if (CollAtRC(r, c)) { hit = true; break; }
					if (hit) newY = (float)((r + 1) * TILE) + PLY_COLL_HALF_H;
				}
				playerY = newY;
			}

			// 边界与相机
			playerX = std::clamp(playerX, HALF_W, (float)MAP_W_PX - HALF_W);
			playerY = std::clamp(playerY, HALF_H, (float)MAP_H_PX - HALF_H);

			float targetCamX = std::clamp(playerX - ViewW() * 0.5f, 0.0f, (float)(MAP_W_PX - ViewW()));
			float targetCamY = std::clamp(playerY - ViewH() * 0.5f, 0.0f, (float)(MAP_H_PX - ViewH()));

			camX += (targetCamX - camX) * CAM_LERP;
			camY += (targetCamY - camY) * CAM_LERP;
			if (fabsf(targetCamX - camX) < CAM_SNAP) camX = targetCamX;
			if (fabsf(targetCamY - camY) < CAM_SNAP) camY = targetCamY;

			// 电量/灯光
			fuel -= DRAIN_PER_FRAME * DRAIN_MULT[lightLevel];
			if (fuel < 0.0f) fuel = 0.0f;
			// ★ 新：没电直接全黑（半径为 0）
			targetRadius = (fuel <= 0.0f) ? 0.0f : LIGHT_LEVELS[lightLevel];
			radius += (targetRadius - radius) * RADIUS_LERP;

			// === 安全区判定 ===
			int pR, pC; WorldToRC(playerX, playerY, pR, pC);
			bool playerInSafe = (tileGrid[pR][pC] == T_SAFE);

			// 拾取
			const float pickR2 = PICK_RADIUS * PICK_RADIUS;
			int activeBatt = 0;
			for (auto& b : batteries) if (b.alive) ++activeBatt;

			for (auto& b : batteries) {
				if (!b.alive) {
					if (BATT_ENABLE_RESPAWN) {
						b.timer -= FRAME_SEC;
						if (b.timer <= 0.0f && activeBatt < MAX_BATT_ACTIVE) {
							b.alive = true;
							b.timer = 0.0f;
							++activeBatt;
						}
					}
				}
				else {
					if (Dist2(playerX, playerY, b.x, b.y) <= pickR2) {
						if (batteryInv < 7) {
							b.alive = false;
							if (BATT_ENABLE_RESPAWN) b.timer = BATT_RESPAWN_SEC; else b.timer = 0.0f;
							batteryInv++;
							if (sfxGet >= 0) Novice::PlayAudio(sfxGet, /*loop=*/false, /*vol=*/1.0f);  // 拾取声
						}
						else {
							// 满背包：不拾取，不清除地图上的电池（保持 b.alive = true）
							// （需要提示音的话可以在这里单独播一个“背包已满”的音效）
						}
					}

				}
			}

			for (auto& it : collectibles) {
				if (it.alive && Dist2(playerX, playerY, it.x, it.y) <= pickR2) {
					it.alive = false;
					collectedCnt++;
					if (sfxGet >= 0) Novice::PlayAudio(sfxGet, /*loop=*/false, /*vol=*/1.0f);  // ★新增：拾取声
				}

			}

			// 敌人AI & 动画
			const float lightR2 = radius * radius;

			float minEnemyDist2 = 1e30f; // 仅调试
			bool  anyChasing = false;    // 有敌人处于追击态

			for (auto& enemy : enemies) {
				float d2 = Dist2(enemy.x, enemy.y, playerX, playerY);
				if (d2 < minEnemyDist2) minEnemyDist2 = d2;

				// 进入光圈才算“看到玩家”，但玩家在安全区时，强制不可见
				bool litFound = (!playerInSafe) && (d2 <= lightR2);

				switch (enemy.state) {
				case ES_Patrol: {
					if (litFound) {
						int er, ec, pr, pc; WorldToRC(enemy.x, enemy.y, er, ec); WorldToRC(playerX, playerY, pr, pc);
						std::vector<std::pair<int, int>> path;
						if (BFSPath(er, ec, pr, pc, path)) {
							enemy.state = ES_Chase; enemy.path = std::move(path);
							enemy.pathIdx = (enemy.path.size() >= 2) ? 1 : 0;
							auto tgt = enemy.path[enemy.pathIdx]; enemy.tgtR = tgt.first; enemy.tgtC = tgt.second;
							enemy.repathTimer = CHASE_REPATH_SEC; enemy.lostTimer = 0.0f;
						}
					}
					else {
						float tx, ty; TileCenter(enemy.tgtR, enemy.tgtC, tx, ty);
						if (fabsf(enemy.x - tx) <= 2.0f && fabsf(enemy.y - ty) <= 2.0f) {
							enemy.x = tx; enemy.y = ty;
							enemy.prevR = enemy.curR; enemy.prevC = enemy.curC;
							enemy.curR = enemy.tgtR;  enemy.curC = enemy.tgtC;
							ChooseNextOnRoute(enemy, rng);
						}
						EnemyStepToTarget(enemy);
					}
				} break;

				case ES_Chase: {
					if (playerInSafe) {
						int er, ec; WorldToRC(enemy.x, enemy.y, er, ec);
						int rr, rc;
						if (FindNearestRouteTileById(er, ec, enemy.routeId, rr, rc)) {
							std::vector<std::pair<int, int>> back;
							if (BFSPath(er, ec, rr, rc, back)) {
								enemy.state = ES_Return;
								enemy.path = std::move(back);
								enemy.pathIdx = (enemy.path.size() >= 2) ? 1 : 0;
								auto tgt = enemy.path[enemy.pathIdx];
								enemy.tgtR = tgt.first; enemy.tgtC = tgt.second;
							}
							else {
								enemy.state = ES_Patrol;
								ChooseNextOnRoute(enemy, rng);
							}
						}
						else {
							enemy.state = ES_Patrol;
							ChooseNextOnRoute(enemy, rng);
						}
						enemy.lostTimer = 0.0f;
						EnemyStepToTarget(enemy);
						break;
					}

					if (litFound) enemy.lostTimer = 0.0f; else enemy.lostTimer += FRAME_SEC;

					enemy.repathTimer -= FRAME_SEC;
					if (enemy.repathTimer <= 0.0f) {
						int er, ec, pr, pc; WorldToRC(enemy.x, enemy.y, er, ec); WorldToRC(playerX, playerY, pr, pc);
						std::vector<std::pair<int, int>> newPath;
						if (BFSPath8(er, ec, pr, pc, newPath)) {
							enemy.path = std::move(newPath);
							enemy.pathIdx = (enemy.path.size() >= 2) ? 1 : 0;
							auto tgt = enemy.path[enemy.pathIdx]; enemy.tgtR = tgt.first; enemy.tgtC = tgt.second;
						}
						enemy.repathTimer = CHASE_REPATH_SEC;
					}

					float tx, ty; TileCenter(enemy.tgtR, enemy.tgtC, tx, ty);
					if (fabsf(enemy.x - tx) <= 2.5f && fabsf(enemy.y - ty) <= 2.5f) {
						enemy.x = tx; enemy.y = ty;
						int cr, cc; WorldToRC(enemy.x, enemy.y, cr, cc);
						enemy.curR = cr; enemy.curC = cc;
						if (enemy.pathIdx + 1 < (int)enemy.path.size()) {
							++enemy.pathIdx; auto nxt = enemy.path[enemy.pathIdx];
							enemy.tgtR = nxt.first; enemy.tgtC = nxt.second;
						}
					}
					EnemyStepToTargetFree(enemy);

					if (enemy.lostTimer >= CHASE_LOST_SECONDS) {
						int er, ec; WorldToRC(enemy.x, enemy.y, er, ec);
						int rr, rc;
						if (FindNearestRouteTileById(er, ec, enemy.routeId, rr, rc)) {
							std::vector<std::pair<int, int>> back;
							if (BFSPath(er, ec, rr, rc, back)) {
								enemy.state = ES_Return; enemy.path = std::move(back);
								enemy.pathIdx = (enemy.path.size() >= 2) ? 1 : 0;
								auto tgt = enemy.path[enemy.pathIdx]; enemy.tgtR = tgt.first; enemy.tgtC = tgt.second;
							}
							else { enemy.state = ES_Patrol; ChooseNextOnRoute(enemy, rng); }
						}
						else { enemy.state = ES_Patrol; ChooseNextOnRoute(enemy, rng); }
					}
				} break;

				case ES_Return: {
					float tx, ty; TileCenter(enemy.tgtR, enemy.tgtC, tx, ty);
					if (fabsf(enemy.x - tx) <= 2.0f && fabsf(enemy.y - ty) <= 2.0f) {
						enemy.x = tx; enemy.y = ty;
						int cr, cc; WorldToRC(enemy.x, enemy.y, cr, cc);
						enemy.curR = cr; enemy.curC = cc;
						if (enemy.pathIdx + 1 < (int)enemy.path.size()) {
							++enemy.pathIdx; auto nxt = enemy.path[enemy.pathIdx];
							enemy.tgtR = nxt.first; enemy.tgtC = nxt.second;
						}
						else {
							enemy.prevR = -1; enemy.prevC = -1;
							enemy.state = ES_Patrol; ChooseNextOnRoute(enemy, rng);
						}
					}
					EnemyStepToTarget(enemy);

					if (litFound) {
						int er, ec, pr, pc; WorldToRC(enemy.x, enemy.y, er, ec); WorldToRC(playerX, playerY, pr, pc);
						std::vector<std::pair<int, int>> path;
						if (BFSPath(er, ec, pr, pc, path)) {
							enemy.state = ES_Chase; enemy.path = std::move(path);
							enemy.pathIdx = (enemy.path.size() >= 2) ? 1 : 0;
							auto tgt = enemy.path[enemy.pathIdx]; enemy.tgtR = tgt.first; enemy.tgtC = tgt.second;
							enemy.repathTimer = CHASE_REPATH_SEC; enemy.lostTimer = 0.0f;
						}
					}
				} break;
				}

				// 敌人动画
				const float MV_EPS = 0.01f;
				bool enemyMoving = (fabsf(enemy.lastVX) > MV_EPS || fabsf(enemy.lastVY) > MV_EPS);
				if (enemyMoving) {
					if (fabsf(enemy.lastVX) > fabsf(enemy.lastVY)) enemy.dir = (enemy.lastVX > 0.0f) ? ED_Right : ED_Left;
					else enemy.dir = (enemy.lastVY > 0.0f) ? ED_Front : ED_Back;
					enemy.animAccum += FRAME_SEC;
					while (enemy.animAccum >= ENEMY_ANIM_DT) {
						enemy.animAccum -= ENEMY_ANIM_DT;
						enemy.frame = (enemy.frame + 1) % ENEMY_FRAMES;
					}
				}
				else { enemy.frame = 0; enemy.animAccum = 0.0f; }

				// 追击聚合（玩家在安全区不计）
				if (!playerInSafe && enemy.state == ES_Chase) {
					anyChasing = true;
				}
			}

			// === 胜负判定（抓到/收集）===
			// 小工具：离开 Play 前把这里的声音都停掉
			auto StopAllPlayVoices = [&]() {
				if (voiceRun >= 0) { Novice::StopAudio(voiceRun);  voiceRun = -1; }
				if (voiceBgm >= 0) { Novice::StopAudio(voiceBgm);  voiceBgm = -1; }
				};

			bool caught = false;
			if (!playerInSafe) {
				for (auto& enemy : enemies) {
					if (AABBOverlap(playerX, playerY, PLY_COLL_W, PLY_COLL_H,
						enemy.x, enemy.y, ENEMY_COLL_W, ENEMY_COLL_H)) {
						caught = true; break;
					}
				}
			}

			constexpr int WIN_COUNT = 15;
			bool win = (collectedCnt >= WIN_COUNT);

			if (caught) {
				StopAllPlayVoices();
				return PR_GameOver;
			}
			if (win) {
				StopAllPlayVoices();
				return PR_Clear;
			}


			// === BGM：仅主 BGM（不再有追击音效） ===
			float tgtMain = MAIN_BGM_VOL;  // 始终保持主 BGM
			volMain = ApproachLinear(volMain, tgtMain, (tgtMain > volMain) ? FADE_UP_PER_SEC : FADE_DOWN_PER_SEC);
			SetVoiceVol(voiceBgm, volMain);

			// ===== 绘制 =====
			// 地图
			Novice::DrawSprite(
				(int)(-camX * RENDER_SCALE),
				(int)(-camY * RENDER_SCALE),
				mapTex, RENDER_SCALE, RENDER_SCALE, 0.0f, COL_WHITE);

			int pScreenX = (int)((playerX - camX) * RENDER_SCALE);
			int pScreenY = (int)((playerY - camY) * RENDER_SCALE);

			// 玩家
			int tex = -1;
			if (isMoving) tex = playerTex[playerDir][playerFrame];
			else {
				tex = playerIdleTex[playerDir][playerIdleFrame];
				if (tex < 0) tex = playerTex[playerDir][0];
			}
			if (tex >= 0) {
				Novice::DrawSprite(
					pScreenX - (int)(HALF_W * RENDER_SCALE),
					pScreenY - (int)(HALF_H * RENDER_SCALE),
					tex, RENDER_SCALE, RENDER_SCALE, 0.0f, COL_WHITE);
			}
			else {
				Novice::DrawBox(
					pScreenX - (int)(HALF_W * RENDER_SCALE),
					pScreenY - (int)(HALF_H * RENDER_SCALE),
					(int)(PLAYER_W * RENDER_SCALE), (int)(PLAYER_H * RENDER_SCALE),
					0.0f, COL_PLY, kFillModeSolid);
			}

			// 拾取点
			auto drawDot = [&](float wx, float wy, unsigned int col) {
				int sx = (int)((wx - camX) * RENDER_SCALE);
				int sy = (int)((wy - camY) * RENDER_SCALE);
				Novice::DrawBox(
					sx - (int)(DOT_SZ * 0.5f * RENDER_SCALE),
					sy - (int)(DOT_SZ * 0.5f * RENDER_SCALE),
					(int)(DOT_SZ * RENDER_SCALE), (int)(DOT_SZ * RENDER_SCALE),
					0.0f, col, kFillModeSolid);
				};
			// ★ 用贴图画拾取物（先电池再收藏品；在玩家/敌人之下或之上按你原顺序即可）
			auto drawPickup = [&](float wx, float wy, int texId) {
				int sx = (int)((wx - camX) * RENDER_SCALE);
				int sy = (int)((wy - camY) * RENDER_SCALE);
				if (texId >= 0) {
					// 以 48px 视觉尺寸绘制；和世界缩放联动
					int halfPx = (int)(0.5f * PICKUP_DRAW * RENDER_SCALE);
					float s = PICKUP_SCALE * RENDER_SCALE;
					Novice::DrawSprite(sx - halfPx, sy - halfPx, texId, s, s, 0.0f, COL_WHITE);
				}
				else {
					// 兜底：还用彩色方块
					Novice::DrawBox(
						sx - (int)(DOT_SZ * 0.5f * RENDER_SCALE),
						sy - (int)(DOT_SZ * 0.5f * RENDER_SCALE),
						(int)(DOT_SZ * RENDER_SCALE), (int)(DOT_SZ * RENDER_SCALE),
						0.0f, COL_ITEM, kFillModeSolid);
				}
				};

			for (auto& b : batteries)    if (b.alive)  drawPickup(b.x, b.y, batteryTex);
			for (auto& it : collectibles) if (it.alive) drawPickup(it.x, it.y, collectTex);

			// 敌人
			for (auto& enemy : enemies) {
				int ex = (int)((enemy.x - camX) * RENDER_SCALE);
				int ey = (int)((enemy.y - camY) * RENDER_SCALE);
				int et = enemyTex[enemy.dir][enemy.frame];
				if (et >= 0) {
					Novice::DrawSprite(
						ex - (int)(HALF_W * RENDER_SCALE),
						ey - (int)(HALF_H * RENDER_SCALE),
						et, RENDER_SCALE, RENDER_SCALE, 0.0f, COL_WHITE);
				}
				else {
					Novice::DrawBox(
						ex - (int)(HALF_W * RENDER_SCALE),
						ey - (int)(HALF_H * RENDER_SCALE),
						(int)(ENEMY_W * RENDER_SCALE), (int)(ENEMY_H * RENDER_SCALE),
						0.0f, enemyColor(enemy.routeId), kFillModeSolid);
				}
			}

			// === 扫描线：在遮罩之前（追击时用偏黑的灰；其余常态） ===
			scanTime += FRAME_SEC;
			unsigned int scanCol = ::SCAN_COLOR_NORMAL;
			::DrawScanlineOverlay(scanTime, scanCol);

			// 光圈遮罩（画在最上层，盖住黑暗区域）
			DrawDarkCircleHole(
				pScreenX, pScreenY,
				(int)(radius * RENDER_SCALE + 0.5f),
				COL_BLACK_P);

			// === 上中：剩余收集物数量 UI ===
			if (uiRemainMojiTex >= 0) {
				// 文字底图（你已调整到上中，按要求放 0,0）
				Novice::DrawSprite(0, 0, uiRemainMojiTex, 1.0f, 1.0f, 0.0f, 0xFFFFFFFFu);
			}
			// === SPACE 提示：放在左下角，避开其他 UI，确保在遮罩之后绘制 ===
			if (uiSpaceTex >= 0) {
				Novice::DrawSprite(0,0, uiSpaceTex, 1.0f, 1.0f, 0.0f, 0xFFFFFFFFu);
			}

			// 计算剩余并触发轻动画
			int remain = WIN_COUNT - collectedCnt;
			if (remain < 0) remain = 0;
			if (remain != lastRemainShown) {
				lastRemainShown = remain;
				remainAnimT = REMAIN_ANIM_DUR; // 触发下落动画
			}

			// 数字绘制（默认两位；<10 时只画个位并居中）
			const int DIG_W = 64;
			const int CENTER_X = ::SCREEN_W / 2;   // 上中
			int baseY = 48;                        // 顶部 Y，后面你可以微调

			// 轻微下落动画：新数字从上往下落入几像素
			int yOfs = 0;
			if (remainAnimT > 0.0f) {
				float t = remainAnimT / REMAIN_ANIM_DUR; // 1 -> 0
				yOfs = (int)(-20.0f * t);                // 从 -20px 落到 0
				remainAnimT -= FRAME_SEC;
				if (remainAnimT < 0.0f) remainAnimT = 0.0f;
			}

			// 拆位并绘制
			int tens = remain / 10;
			int ones = remain % 10;
			if (remain >= 10) {
				int totalW = DIG_W * 2 + 8;          // 两位 + 间隙(8px)
				int startX = CENTER_X - totalW / 2;
				if (uiDigitRemain[tens] >= 0) Novice::DrawSprite(startX, baseY + yOfs, uiDigitRemain[tens], 1.0f, 1.0f, 0.0f, 0xFFFFFFFFu);
				if (uiDigitRemain[ones] >= 0) Novice::DrawSprite(startX + DIG_W + 8, baseY + yOfs, uiDigitRemain[ones], 1.0f, 1.0f, 0.0f, 0xFFFFFFFFu);
			}
			else {
				// 只有个位：居中
				int startX = CENTER_X - DIG_W / 2;
				if (uiDigitRemain[ones] >= 0) Novice::DrawSprite(startX, baseY + yOfs, uiDigitRemain[ones], 1.0f, 1.0f, 0.0f, 0xFFFFFFFFu);
			}

			// ★ 灯光 UI：固定画在(0,0)，放在遮罩之后
			{
				// fuel<=0 显示 light_0；否则显示当前档位(1~3)
				int iconIdx = (fuel <= 0.0f) ? 0 : (lightLevel + 1); // 0..3
				if (iconIdx < 0) iconIdx = 0;
				if (iconIdx > 3) iconIdx = 3;

				int iconTex = uiLight[iconIdx];
				if (iconTex >= 0) {
					Novice::DrawSprite(0, 0, iconTex, 1.0f, 1.0f, 0.0f, COL_WHITE);
				}
			}

			// ★ 电量 UI（画在遮罩之后，固定在 0,0）
			{
				int fuelPct = (int)((fuel / FUEL_MAX) * 100.0f + 0.5f);
				if (fuelPct < 0) fuelPct = 0;
				if (fuelPct > 100) fuelPct = 100;

				int uiTex = uiFuel0;
				if (fuelPct <= 0)       uiTex = uiFuel0;
				else if (fuelPct <= 10)  uiTex = uiFuel10;
				else if (fuelPct <= 40)  uiTex = uiFuel40;
				else if (fuelPct <= 70)  uiTex = uiFuel70;
				else                    uiTex = uiFuel100;

				if (uiTex >= 0) {
					Novice::DrawSprite(0, 0, uiTex, 1.0f, 1.0f, 0.0f, COL_WHITE);
				}
			}

			// 电池库存 UI（1~7），0 不画
			if (batteryInv > 0) {
				int idx = (batteryInv <= 7) ? batteryInv : 7;
				int battTex = battStackTex[idx];
				if (battTex >= 0) {
					Novice::DrawSprite(0, 0, battTex, 1.0f, 1.0f, 0.0f, 0xFFFFFFFFu);
				}
			}

			// Mini-map
			{
				Novice::DrawBox(miniX - 4, miniY - 4, MINI_W + 8, MINI_H + 8, 0.0f, COL_MM_BG, kFillModeSolid);
				Novice::DrawSprite(miniX, miniY, mapTex, miniSX, miniSY, 0.0f, 0xFFFFFFB0);
				int mpX = (int)(miniX + playerX * miniSX);
				int mpY = (int)(miniY + playerY * miniSY);
				Novice::DrawBox(mpX - 3, mpY - 3, 6, 6, 0.0f, COL_MM_P, kFillModeSolid);
			}

			Novice::EndFrame();

			// ESC 退出回 Title（外层接手）
			if (!paused && !preKeys[DIK_ESCAPE] && keys[DIK_ESCAPE]) {
				if (voiceRun >= 0) { Novice::StopAudio(voiceRun);  voiceRun = -1; }
				if (voiceBgm >= 0) { Novice::StopAudio(voiceBgm);  voiceBgm = -1; }
				break;
			}
		}

		// 不在这里 Finalize，交给外层
		return 0;
	}
}
