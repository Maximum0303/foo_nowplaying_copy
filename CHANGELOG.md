# Changelog

Major changes in NowPlaying Copy & Artwork.

[English](#english) | [日本語](#日本語)

---

# English

## v1.2.3

- Polished and stabilized Japanese/English UI support
- Translated remaining Japanese messages shown in English mode
- Translated post-history counts, selection details, and backup results
- Translated artwork-optimization results and completion messages
- Added English text to history ZIP and settings INI file dialogs
- Translated the explanatory text at the bottom of Preferences
- Improved the template-deletion confirmation message in English
- New installations now create default template names in the selected UI language
- Existing custom template names and contents remain unchanged
- Replaced `wcsncpy` with `wcsncpy_s` to reduce build warnings
- Inherits the English-layout improvements and crash fixes from v1.2.2

## v1.2.2

- Improved layout of the English Preferences screen
- Adjusted button widths and spacing for longer English labels
- Improved display of controls such as Duplicate, Quality, Save limit, and Display limit
- Improved overall readability of the English UI
- Inherits the language-switch crash fix from v1.2.1

## v1.2.1

- Fixed a crash that could occur when opening Preferences after switching the UI language to English
- Fixed recursive language-text lookup introduced during v1.2.0 localization
- Improved stability of Japanese/English switching
- Preserved existing settings and templates

## v1.2.0

- Added Japanese/English UI support
- Added language selection: Automatic / 日本語 / English
- Automatic mode uses Japanese on a Japanese Windows UI and English otherwise
- Added instant UI-language switching
- Added persistent language preference
- Localized menus, Preferences, Preview, History, Help, and major messages
- Preserved existing user settings, templates, history, and backup compatibility

## v1.1.4

- Officially adopted the MIT License
- Added copyright notice
  - `Copyright (c) 2026 Maximum`
- Added `LICENSE` for the GitHub repository
- Added `license.txt` for distribution packages
- Added copyright and license information to component metadata
- Added a License section to the built-in Help

## v1.1.3

- Fixed Help text being displayed as a single line
- Normalized Help line breaks to Windows CRLF
- Removed horizontal scrolling
- Enabled wrapped text display
- Used the same line-break format when copying Help contents

## v1.1.2

- Added built-in Help
- Added a Help button at the bottom of Preferences
- Added File > NowPlaying Help
- Made the Help window resizable
- Added dark mode support
- Added Copy Help Contents
- Added explanations for basic usage, templates, artwork, X posting, history, backup, and pinning

## v1.1.1

- Adjusted the template-operation button layout at the top of Preferences
- Improved clipping of right-side buttons at some display scales and window widths
- Optimized the width of the template-name field and operation buttons

## v1.1.0

- Expanded the template count from a fixed 5 to 1–20
- Added template creation
- Added template duplication
- Added template deletion
- Added template move up/down
- Added reset to default templates
- Automatically migrated existing five-template settings
- Updated settings INI export/import for variable template counts
- Changed the Preview template list to dynamic display

## v1.0.9

- Added pinning to post history
- Added bulk pin/unpin for multiple selected history entries
- Added a pinned-only filter
- Excluded pinned entries from automatic history trimming
- Added pin information to history ZIP backup
- Restored pin information when importing history
- Added pin state to the history list

## v1.0.8

- Made the post-history save limit configurable
- Added a save limit range of 10–1000 entries
- Added a separate display limit for the History window
- Added display options: All, 50, 100, 200, and 500
- Search now checks all saved history regardless of display limit
- Added automatic trimming of older unpinned history entries

## v1.0.7

- Added multiple selection to post history
- Added Ctrl+click individual selection
- Added Shift+click range selection
- Added combined copying of multiple post texts
- Added bulk deletion
- Restricted image copy, X repost, and Open Folder to single selection

## v1.0.6

- Added post-history search
- Search by date/time, title, artist, template name, and post text
- Added ZIP backup for post history
- Added export of history and history artwork into a single ZIP
- Added additive history restore
- Added replace-mode history restore
- Added duplicate prevention during restore
- Improved portability of history artwork between PCs

## v1.0.5

- Added post history
- Automatically saves history when the X compose page is opened
- Saves date/time, title, artist, template, post text, and artwork
- Added a Post History window
- Added copy post text from history
- Added copy artwork from history
- Added reopen X compose from history
- Added Open History Artwork Folder
- Added history deletion
- Added opening history entries in Preview
- Added dark mode support

## v1.0.4

- Added posting-artwork optimization
- Creates `artwork-post.jpg` or `artwork-post.png` separately from the source image
- Added maximum image size from 256 to 4096 px
- Added JPEG/PNG format selection
- Added JPEG quality from 40 to 100
- Added aspect-ratio-preserving resize
- Added centered square crop
- Enabled/disabled related controls according to selected settings

## v1.0.3

- Fixed line breaks being lost in multiline templates
- Changed Title Formatting processing to work line by line
- Restored CRLF line breaks after processing
- Improved stability of multiline Japanese post text

## v1.0.2

- Improved clipboard copying of posting artwork
- Added both `CF_DIB` and `CF_BITMAP` clipboard formats
- Enabled image pasting into X with Ctrl+V
- Added Copy Image and Open X
- Added opening the X compose page with post text

## v1.0.1

- Added Post Preview
- Added post-text editing
- Added character count and approximate X-weight count
- Added artwork preview
- Added artwork-source display
- Added post-text copy
- Added artwork copy
- Added Open Output Folder
- Added Copy and Close option
- Added dark mode support

## v1.0.0

- Initial release
- Creates post text from the currently playing track
- Copies post text to the clipboard
- Saves post text to `nowplaying.txt`
- Includes five posting templates
- Supports foobar2000 Title Formatting
- Reads embedded front cover artwork
- Searches `cover`, `folder`, and `front` images in the track folder
- Searches `cover`, `folder`, and `front` images inside ZIP files
- Added a Preferences page
- Added configurable output folder
- Added settings INI export/import
- Supports Windows foobar2000 64-bit

---

# 日本語

NowPlaying Copy & Artworkの主な変更履歴です。

## v1.2.3

- 日本語／英語対応の仕上げと安定化
- 英語表示に残っていた日本語メッセージを追加翻訳
- 投稿履歴の件数表示、選択情報、バックアップ結果を英語化
- 画像最適化結果と処理完了メッセージを英語化
- 履歴ZIP／設定INIのファイル選択画面を英語化
- 設定画面下部の説明文を英語化
- テンプレート削除確認文を自然な英語表現へ修正
- 新規インストール時の初期テンプレート名を表示言語に対応
- 既存ユーザーの保存済みテンプレート名・内容はそのまま維持
- `wcsncpy` を `wcsncpy_s` へ変更し、ビルド警告を削減
- v1.2.2の英語レイアウト調整とクラッシュ修正を継承

## v1.2.2

- 英語表示時の設定画面レイアウトを改善
- 英語ラベルの長さに合わせてボタン幅と間隔を調整
- Duplicate、Quality、Save limit、Display limitなどの表示を改善
- 英語UI全体の視認性を改善
- v1.2.1の言語切り替え時クラッシュ修正を継承

## v1.2.1

- Englishへ切り替えた後に設定画面を開くとクラッシュする場合がある問題を修正
- v1.2.0の多言語化で発生した言語文字列取得処理の再帰呼び出しを修正
- 日本語／英語切り替えの安定性を改善
- 既存の設定・テンプレートをそのまま維持

## v1.2.0

- 日本語／英語UIに対応
- 表示言語として「自動／日本語／English」を追加
- 「自動」では日本語UIのWindowsで日本語、それ以外で英語を使用
- 表示言語の即時切り替えに対応
- 選択した表示言語を保存
- メニュー、設定画面、投稿プレビュー、履歴、ヘルプ、主要メッセージを多言語化
- 既存設定、テンプレート、履歴、バックアップ互換性を維持

## v1.1.4

- MIT Licenseを正式に採用
- 著作権表示を追加
  - `Copyright (c) 2026 Maximum`
- GitHubリポジトリ用の`LICENSE`を追加
- 配布パッケージ用の`license.txt`を追加
- コンポーネント情報に著作権とライセンスを表示
- 内蔵ヘルプにライセンス項目を追加

## v1.1.3

- ヘルプ画面の本文が改行されず1行表示になる問題を修正
- ヘルプ本文の改行をWindows用CRLFへ正規化
- 横スクロールを廃止
- 折り返し表示へ変更
- ヘルプ内容のコピーでも同じ改行形式を使用

## v1.1.2

- 内蔵ヘルプ画面を追加
- 設定画面下部に［ヘルプ］ボタンを追加
- ［ファイル］→［NowPlayingヘルプ］を追加
- ヘルプ画面をサイズ変更可能に変更
- ダークモードに対応
- ヘルプ全文のクリップボードコピーに対応
- 基本操作、テンプレート、アートワーク、X投稿、履歴、バックアップ、ピン留めを解説

## v1.1.1

- 設定画面上部のテンプレート操作ボタン配置を調整
- 表示倍率やウィンドウ幅によって右端のボタンが切れる問題を改善
- テンプレート名入力欄と操作ボタンの幅を最適化

## v1.1.0

- テンプレート数を固定5個から1～20個へ拡張
- テンプレートの追加に対応
- テンプレートの複製に対応
- テンプレートの削除に対応
- テンプレートの上下移動に対応
- 標準テンプレートへの初期化に対応
- 既存の5テンプレート設定を自動移行
- 設定INIの書き出し・読み込みを可変テンプレート数に対応
- 投稿プレビューのテンプレート一覧を動的表示に変更

## v1.0.9

- 投稿履歴のピン留め機能を追加
- 複数選択した履歴の一括ピン留め・解除に対応
- ピン留め履歴のみ表示するフィルターを追加
- ピン留め履歴を自動整理の対象外に変更
- 履歴ZIPバックアップへピン留め情報を保存
- 履歴復元時にピン留め情報を復元
- 投稿履歴一覧にピン留め状態を表示

## v1.0.8

- 投稿履歴の保存上限を設定可能に変更
- 保存上限を10～1000件で指定可能
- 投稿履歴画面の表示上限を独立して設定可能
- 表示上限として全件、50、100、200、500件を追加
- 検索中は表示上限を解除し、保存済みの全履歴を検索
- 古い通常履歴を自動整理する処理を追加

## v1.0.7

- 投稿履歴の複数選択に対応
- Ctrl＋クリックによる個別選択に対応
- Shift＋クリックによる範囲選択に対応
- 複数履歴の投稿文をまとめてコピー可能に変更
- 複数履歴の一括削除に対応
- 画像コピー、X再投稿、フォルダーを開く操作を1件選択時のみに制限

## v1.0.6

- 投稿履歴の検索機能を追加
- 日時、曲名、アーティスト、テンプレート名、投稿文を検索可能
- 履歴のZIPバックアップに対応
- 履歴と履歴画像を1つのZIPへ書き出し可能
- 履歴ZIPの追加復元に対応
- 履歴ZIPの置き換え復元に対応
- 復元時の重複登録防止を追加
- 履歴画像を別PCへ移行できるよう改善

## v1.0.5

- 投稿履歴機能を追加
- Xの投稿画面を開いた際に履歴を自動保存
- 投稿日時、曲名、アーティスト、テンプレート、投稿文、画像を保存
- 投稿履歴画面を追加
- 履歴から投稿文をコピー可能
- 履歴画像をコピー可能
- 履歴からXの投稿画面を再度開く機能を追加
- 履歴画像フォルダーを開く機能を追加
- 履歴の削除に対応
- 履歴から投稿プレビューを開く機能を追加
- ダークモードに対応

## v1.0.4

- 投稿用アートワークの最適化機能を追加
- 元画像とは別に`artwork-post.jpg`または`artwork-post.png`を作成
- 最大画像サイズを256～4096pxで指定可能
- JPEG／PNG形式を選択可能
- JPEG画質を40～100で指定可能
- 縦横比を維持したリサイズに対応
- 中央を正方形に切り抜く処理に対応
- 設定内容に応じて関連コントロールを有効・無効化

## v1.0.3

- 複数行テンプレートの改行が失われる問題を修正
- テンプレートを1行ずつTitle Formatting処理する方式へ変更
- 処理後にCRLF改行を復元
- 日本語を含む複数行投稿文の安定性を改善

## v1.0.2

- 投稿用画像のクリップボードコピーを改善
- `CF_DIB`と`CF_BITMAP`の両形式に対応
- Xの投稿画面でCtrl＋Vによる画像貼り付けが可能に
- ［画像をコピーしてXを開く］機能を追加
- 投稿文を入力したXの投稿画面を開く機能を追加

## v1.0.1

- 投稿プレビュー画面を追加
- 投稿文の編集に対応
- 文字数とX換算文字数の目安を表示
- アートワークのプレビューを追加
- アートワークの取得元を表示
- 投稿文のコピーに対応
- アートワークのコピーに対応
- 出力フォルダーを開く機能を追加
- コピー後に画面を閉じる設定を追加
- ダークモードに対応

## v1.0.0

- 初回公開
- 再生中の曲情報から投稿文を作成
- 投稿文をクリップボードへコピー
- 投稿文を`nowplaying.txt`へ保存
- 5種類の投稿テンプレートを搭載
- foobar2000のTitle Formattingに対応
- 埋め込みフロントカバーを取得
- 同一フォルダーの`cover`、`folder`、`front`画像を検索
- ZIP内の`cover`、`folder`、`front`画像を検索
- 設定ページを追加
- 出力フォルダーを指定可能
- 設定INIの書き出し・読み込みに対応
- Windows版foobar2000 64-bitに対応
