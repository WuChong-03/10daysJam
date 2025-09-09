#include <Novice.h>

const char kWindowTitle[] = "GC1C_02_ゴ_チュウ";
const int  kWindowWidth  = 1280;
const int  kWindowHeight = 720;

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

  // ライブラリの初期化
  Novice::Initialize(kWindowTitle, kWindowWidth, kWindowHeight);

  // キー入力結果を受け取る箱
  char keys[256] = {0};
  char preKeys[256] = {0};

  // ===== 単一ファイル協作ルール（簡）========================
  // 1) 自分の #pragma region だけ編集
  // 2) 重要箇所に // [ANCHOR:XXXX]
  // 3) 変更箇所に // [CHANGE YYYY-MM-DD 氏名] + 一言
  // ========================================================

  #pragma region PLAYER_ZONE   // 担当：〇〇（自分の名前）
  // [ANCHOR:PLAYER_STATE] プレイヤーの状態
  int playerX = kWindowWidth  / 2;
  int playerY = kWindowHeight / 2;
  int playerR = 20;
  int playerSpeed = 5;
  #pragma endregion

  // ウィンドウの×ボタンが押されるまでループ
  while (Novice::ProcessMessage() == 0) {
    // フレームの開始
    Novice::BeginFrame();

    // キー入力を受け取る
    memcpy(preKeys, keys, 256);
    Novice::GetHitKeyStateAll(keys);

    ///
    /// ↓更新処理ここから
    ///

    #pragma region PLAYER_ZONE_UPDATE
    // [ANCHOR:PLAYER_UPDATE] プレイヤー移動
    // [CHANGE 2025-09-03 山田] 矢印キーで移動できるようにした
    if (keys[DIK_LEFT])  { playerX -= playerSpeed; }
    if (keys[DIK_RIGHT]) { playerX += playerSpeed; }
    if (keys[DIK_UP])    { playerY -= playerSpeed; }
    if (keys[DIK_DOWN])  { playerY += playerSpeed; }

    // [CHANGE 2025-09-03 山田] 画面外に出ないようにクランプを追加
    if (playerX < playerR)                 playerX = playerR;
    if (playerX > kWindowWidth - playerR)  playerX = kWindowWidth - playerR;
    if (playerY < playerR)                 playerY = playerR;
    if (playerY > kWindowHeight - playerR) playerY = kWindowHeight - playerR;
    #pragma endregion

    ///
    /// ↑更新処理ここまで
    ///

    ///
    /// ↓描画処理ここから
    ///

    // [ANCHOR:PLAYER_DRAW] プレイヤー描画
    Novice::DrawEllipse(playerX, playerY, playerR, playerR, 0.0f, WHITE, kFillModeSolid);
    Novice::ScreenPrintf(10, 10, "Move: Arrow Keys");
    Novice::ScreenPrintf(10, 30, "ESC: Exit");

    ///
    /// ↑描画処理ここまで
    ///

    // フレームの終了
    Novice::EndFrame();

    // ESCキーが押されたらループを抜ける
    if (preKeys[DIK_ESCAPE] == 0 && keys[DIK_ESCAPE] != 0) {
      break;
    }
  }

  // ライブラリの終了
  Novice::Finalize();
  return 0;
}
