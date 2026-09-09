<h1 align="center">
  <img src="../images/logo.svg" alt="AutoZWJ" width="180"><br>
  <strong>AutoZWJ</strong>
</h1>

<p align="center">
  AviUtl2のプラグイン<br>
  音MAD/YTPMV向けのトラック同期支援プラグイン。RPP/MIDIなど複数のプロジェクト形式に対応しています。
</p>

<p align="center">
  <a href="https://github.com/TNOTawa/AutoZWJ/releases/latest">
    <img src="https://img.shields.io/github/v/release/TNOTawa/AutoZWJ?display_name=tag" alt="最新バージョン">
  </a>
  <a href="https://github.com/TNOTawa/AutoZWJ/blob/main/LICENSE">
    <img src="https://img.shields.io/github/license/TNOTawa/AutoZWJ" alt="ライセンス">
  </a>
  <a href="https://github.com/TNOTawa/AutoZWJ/releases/latest">
    <img src="https://img.shields.io/github/downloads/TNOTawa/AutoZWJ/total" alt="ダウンロード数">
  </a>
  <a href="https://github.com/TNOTawa/AutoZWJ">
    <img src="https://img.shields.io/github/stars/TNOTawa/AutoZWJ" alt="Stars">
  </a>
  <img src="https://img.shields.io/github/last-commit/TNOTawa/AutoZWJ" alt="最終コミット">
</p>

<p align="center">
  <a href="../../README.md">English</a> |
  <a href="../zh/README.md">简体中文</a> |
  日本語
</p>

REAPERプロジェクトファイル（`.rpp`）、標準MIDIファイル（`.mid`）、UTAU/OpenUTAUプロジェクト（`.ust` / `.ustx`）、またはLRC歌詞ファイル（`.lrc`）内の素材を、AviUtl2のタイムライン上に既存のオブジェクトをテンプレートとして一括生成するプラグインです。

## 機能概要

- REAPER `.rpp` / 標準MIDI `.mid` / UTAU `.ust` / OpenUTAU `.ustx` / LRC `.lrc` ファイルを解析
- タイムライン上の既存オブジェクトをスタイルテンプレートとして使用し、エフェクトチェーンとパラメータ設定を継承
- マルチソースマッピング：複数のテンプレートオブジェクトを選択し、順次ローテーション / ランダム抽選 / コードマッピング / アニメーションシーケンス [beta] で割り当て
- エフェクトチェーンエディタ：テンプレートのエフェクトチェーンを読み取り専用で表示し、固定値 / 変数マッピング / 式評価によるパラメータbakeを設定
- スクリプト変数システム：`$note.velocity$ / 127 * 200` などの式でbake値を生成
- BPMグリッド同期：MIDI/RPPのテンポマップをAviUtl2のBPMグリッドに適用
- 多言語対応：簡体字中国語 / English / 日本語に対応し、ホストUIの言語を自動検出
- テンプレート下層に自動レイヤー割り当て、コンパクトに整列
- 交互反転、オブジェクト長制御、マルチノート戦略、偶数行替え
- オブジェクトとノートの同期：次の音符までの伸長、固定フレーム、ギャップ生成、最終フレーム固定（次の音符までの伸長を選択可能）
- 現シーンのFPS・解像度を自動取得、セッション間で設定を永続化

## インストール

### AviUtl2 Catalogからインストール

[AviUtl2 Catalog](https://github.com/Neosku/aviutl2-catalog)からAutoZWJをダウンロードしてインストールする方法を推奨します。プラグインの管理と更新が容易になります。

### ドラッグ＆ドロップでインストール

`AutoZWJ.aux2` ファイルをAviUtl2のプレビューウィンドウへ直接ドラッグ＆ドロップすると、自動的にインストールされます。

### 手動インストール

`AutoZWJ.aux2` を AviUtl2 の `Plugin` フォルダに配置し、AviUtl2 を起動してください。

## クイックスタート

1. **開始**<br>
   タイムライン上の**任意のオブジェクト**を選択し、右クリック → **インポート設定...**。タイムラインにオブジェクトがない場合は、まず任意の素材を1つ配置してテンプレートにするか、手順2に進んでプロジェクトファイルをドロップしてください

2. **プロジェクト読込**<br>
   プロジェクト未読込の場合、プラグインウィンドウは自動的にインポートページを開きます：`.rpp` / `.mid` / `.ust` / `.ustx` ファイルを AviUtl2 ウィンドウにドロップするか、インポートページで最近のプロジェクト / **参照...** を選択します。続いて生成するトラックをチェックし、**インポート実行** をクリック

3. **設定・生成**<br>
   設定ページでパラメータを調整 → **OK** または **適用** をクリック

詳細なチュートリアル・機能説明は[ドキュメント](../ja/index.md)をご参照ください。

## ビルド

MinGW-w64 (g++ 15.2+)、CMake 3.20+ が必要です。AviUtl2 SDK は git サブモジュールで管理されており、Dear ImGui は `src/thirdparty/imgui` に固定バージョンで同梱されています。初回クローン後にサブモジュールを初期化してください:

```powershell
git submodule update --init
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -G "MinGW Makefiles"
cmake --build build
```

生成物: `build/AutoZWJ.aux2`

正式リリースは GitHub Actions が `v*` タグのプッシュ時に Release ビルドを実行し、GitHub Release として公開します。詳細は [ビルドガイド](../zh/build.md) を参照してください。

## 参考

- [AviUtl2 / AviUtl ExEdit2 Plugin SDK](https://spring-fragrance.mints.ne.jp/aviutl/)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [RPPtoEXO ver2.0](https://github.com/Garech-mas/RPPtoEXO-ver2.0)
- [OtomadHelper](https://github.com/otomad/OtomadHelper)
- [om_midi](https://github.com/otomad/om_midi)
- [import_midi_tempos.aux2](https://github.com/sevenc-nanashi/import_midi_tempos.aux2)
- [UltraPaste](https://github.com/zzzzzz9125/UltraPaste)

## コントリビューション

バグ修正、機能改善、ドキュメント更新、翻訳など、AutoZWJへのコントリビューションを歓迎します。以下の手順を推奨します：

1. 本リポジトリをForkし、作業用のブランチを作成する
2. 変更を行い、コミット前に必要なビルドとテストを実施する
3. 変更の目的、主な変更点、検証結果を記載してPull Requestを作成する

大規模な機能追加やアーキテクチャ変更については、実装を始める前にIssueで方針をご相談ください。

<p align="center">
  <a href="https://github.com/TNOTawa/AutoZWJ/graphs/contributors">
    <img src="https://contrib.rocks/image?repo=tnotawa/autozwj" alt="コントリビューター">
  </a>
</p>

## Issueの報告

問題や機能提案がある場合は、[Issues](https://github.com/TNOTawa/AutoZWJ/issues)から報告してください。可能な範囲で以下の情報を添えてください：

- AutoZWJ、AviUtl2、Windowsの各バージョン
- 使用したプロジェクト形式（RPP、MIDI、UST/USTX、LRC）と再現手順
- 期待される結果と実際の結果
- 関連するログ、スクリーンショット、必要に応じて機密情報を除いた最小構成のサンプルファイル

重複を避けるため、投稿前に既存のIssueをご確認ください。

## 寄付

AutoZWJがお役に立ちましたら、以下のプラットフォームから継続的な開発をご支援いただけます。

<p align="center">
  <a href="https://ifdian.net/a/tnotawa">
    <img src="https://img.shields.io/badge/Afdian-Sponsor-946CE6?style=flat-square" alt="Afdianで支援">
  </a>
  <a href="https://tnot.fanbox.cc/">
    <img src="https://img.shields.io/badge/pixivFANBOX-Sponsor-0096FA?style=flat-square" alt="pixivFANBOXで支援">
  </a>
</p>

## ライセンス

MIT License
