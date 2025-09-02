# 🎮 10daysJam チーム開発ルール

このプロジェクトは GitHub リポジトリで進行状況を管理します。  
メンバーは zip で提出、私が統合＆アップロードを行います。  
作業フローは以下の 6 ステップです。

---

## 1. プロジェクト構造と約束

```
assets/
  title/      # タイトル画面
    img/ sfx/ bgm/
  play/       # プレイ画面
    player/ enemy/ env/ ui/
      img/ sfx/
  ui/         # 全体UI
    img/ sfx/
  end/        # 結果画面
    img/ sfx/ bgm/
  common/     # 共通リソース
    fnt/ icon/ img/ sfx/ bgm/
```

- ファイル名：英小文字＋アンダースコア（例：`ui_btn_ok.png`）  
- フレーム番号：`00, 01, 02…` とゼロ埋め（例：`run_00.png`）  
- **命名に使う英語名は必ず事前に相談**（統一のため。私から全員に周知します）  
- 例：  
  - `assets/end/bgm/bgm_result.mp3`  
  - `assets/play/player/img/p_run_00.png`  
  - `assets/play/enemy/slime/img/wk_01.png`

---

## 2. 提出ルールとコメント

- 提出物：`main.cpp` + 必要な `assets/` + `handoff.md`  
- zip 名称：`handoff_名前_YYYYMMDD_v1.zip`（例：`handoff_yamada_20250903_v1.zip`）

### コード内コメント規約（単一ファイル運用）
- `#pragma region` … 自分の担当領域  
- `// [ANCHOR:XXXX]` … 重要箇所の目印  
- `// [CHANGE YYYY-MM-DD 名前] 一言` … 実際に変更した行の直上に記録  

**main.cpp の例（抜粋）**
```cpp
#pragma region PLAYER_ZONE   // 担当：山田
// [ANCHOR:PLAYER_STATE]
int playerX = 640, playerY = 360, playerR = 20;
int playerSpeed = 5;

// [ANCHOR:PLAYER_UPDATE]
// [CHANGE 2025-09-03 山田] 矢印キー移動と画面端クランプを追加
if (keys[DIK_LEFT])  playerX -= playerSpeed;
if (keys[DIK_RIGHT]) playerX += playerSpeed;
if (keys[DIK_UP])    playerY -= playerSpeed;
if (keys[DIK_DOWN])  playerY += playerSpeed;
playerX = std::clamp(playerX, playerR, 1280 - playerR);
playerY = std::clamp(playerY, playerR,  720 - playerR);
#pragma endregion
```

### handoff.md（シンプル版テンプレ）
```markdown
# 交付説明（シンプル）
- 作者：
- 日付：YYYY-MM-DD

- 変更ファイル（ファイル名 + 行範囲）:
  - main.cpp：30–95 行
  - （必要なら）assets/img_enemy_lv1.png：新規追加

- 変更内容（1〜3行で要点）:
  1) ＿＿＿＿＿＿＿
  2) ＿＿＿＿＿＿＿
  3) ＿＿＿＿＿＿＿

- 自己テスト：ビルド OK / 起動 OK / 変更点の動作 OK
- リスク・未確認：＿＿＿＿＿＿＿
```

---

## 3. 毎日の提出方法
1. 各自 → zip 作成  
2. Discord に提出  
3. 私が受領 → 展開 → 統合 → GitHub に push  
4. Discord で統合レポートを共有  

---

## 4. GitHub 運用
- **Push / Merge は私が担当**（メンバーは DL のみ）  
- 最新進捗は GitHub の履歴で確認可能  

---

## 5. 自己テストと確認（提出前チェック）
- ビルド成功 ✅  
- 自分の機能が動作 ✅  
- 他人の領域を壊していない ✅  
- FPS ≈ 55+（またはカクつきなし）  

---

## 6. 進捗の見える化と連絡
- 進捗・更新履歴は GitHub で確認可能  
- **新素材や命名は事前に私へ相談 → 私が全員に通知**  
- すべてのテンプレート／サンプルは GitHub の `template` フォルダに置いてあります。各自参照してください。

---

## まとめ
**構造を統一 → zip 提出 → handoff.md 報告 → 私が統合 → GitHub で履歴管理**  
この流れで作業すれば、衝突・混乱を防ぎ、安全に協力できます。
