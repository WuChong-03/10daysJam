#include <Novice.h>
const char kWindowTitle[] = "teamWork";
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
	Novice::Initialize(kWindowTitle, 1280, 720);
	char keys[256] = {0};
	char preKeys[256] = {0};
	while (Novice::ProcessMessage() == 0) {
		Novice::BeginFrame();
		memcpy(preKeys, keys, 256);
		Novice::GetHitKeyStateAll(keys);


		int playerX = 640;
		int playerY = 360;
		int playerRadius = 20;
		///
		/// ↓更新処理ここから
		///
		


		///
		/// ↑更新処理ここまで
		///












		///
		/// ↓描画処理ここから
		///
		Novice::DrawLine(100, 100, 400, 400, 0xffffff);
		Novice::DrawEllipse(playerX, playerY, playerRadius, playerRadius, 0.0f, WHITE, kFillModeSolid);
		///
		/// ↑描画処理ここまで
		///

		Novice::EndFrame();
		if (preKeys[DIK_ESCAPE] == 0 && keys[DIK_ESCAPE] != 0) {
			break;
		}
	}
	Novice::Finalize();
	return 0;
}
