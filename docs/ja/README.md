[中文](../../README.md) | [English](../en/README.md) | [日本語](.)

# AutoZWJ

> RPP / MIDI → AviUtl2 オブジェクト一括インポートプラグイン

REAPERプロジェクトファイル（`.rpp`）、標準MIDIファイル（`.mid`）、またはLRC歌詞ファイル（`.lrc`）内の素材を、AviUtl2のタイムライン上に既存のオブジェクトをテンプレートとして一括生成するプラグインです。

## 機能概要

- REAPER `.rpp` / 標準MIDI `.mid` / LRC `.lrc` ファイルを解析
- タイムライン上の既存オブジェクトを**スタイルテンプレート**として継承（エフェクトチェーン、パラメータ設定）
- **多ソースマッピング** — 複数テンプレートを戦略に応じて割り当て（順次ローテーション / ランダム抽選 / コードマッピング）
- **エフェクトチェーンエディタ** — テンプレートのエフェクトチェーンを確認・パラメータbake（固定値 / 変数マッピング / 式評価）
- **スクリプト変数システム** — `$note.velocity$ / 127 * 200` のような式でbake値を動的生成
- **BPMグリッド同期** — MIDI/RPPのテンポマップをAviUtl2のBPMグリッドに適用
- **国際化対応** — UIは中国語・英語・日本語に対応、ホストUI言語を自動検出
- テンプレート下層に自動レイヤー割り当て、コンパクトに整列
- 交互反転、オブジェクト長制御、マルチノート戦略、偶数行替え
- 現シーンのFPS・解像度を自動取得、セッション間で設定を永続化

## インストール

`AutoZWJ.aux2` を AviUtl2 の `Plugin` フォルダに配置し、AviUtl2 を起動してください。

## クイックスタート

1. **プロジェクト読込**  
   タイムラインの空き領域を右クリック → **オーディオプロジェクト選択...** → `.rpp` または `.mid` ファイルを選択

2. **テンプレート選択**  
   タイムライン上で1つ以上のオブジェクトを選択（エフェクトチェーンやフィルターが継承されます）

3. **設定・生成**  
   選択オブジェクトを右クリック → **インポート設定...** → ポップアップウィンドウでパラメータ調整 → **OK** または **適用**

詳細なチュートリアル・機能説明は[ドキュメント](../zh/)をご参照ください。

## ビルド

MinGW-w64 (g++ 15.2+)、CMake 3.20+、Dear ImGui docking branch が必要です。

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -G "MinGW Makefiles"
cmake --build build
```

生成物: `build/AutoZWJ.aux2`

詳細は [ビルドガイド](../zh/build.md) を参照してください。

## 参考

- [RPPtoEXO ver2.0](https://github.com/Garech-mas/RPPtoEXO-ver2.0)
- [OtomadHelper](https://github.com/otomad/OtomadHelper)
- [om_midi](https://github.com/otomad/om_midi)
- [AviUtl2 / AviUtl ExEdit2 Plugin SDK](https://spring-fragrance.mints.ne.jp/aviutl/)
- [Dear ImGui](https://github.com/ocornut/imgui)

## ライセンス

MIT License
