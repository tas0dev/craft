# Craft build system

CraftはC/C++プロジェクト向けのシンプルなビルドシステムです。
MakeやNinjaなどの外部ビルドシステムに依存せず、ソースファイルの探索、コンパイル、依存関係の追跡、リンクまでを行います。
通常のCアプリケーションだけでなく、静的ライブラリ、動的ライブラリ、freestanding環境などの低レイヤー開発でも利用できることを目標としています。

## 特徴

- Make / Ninjaに依存しないビルド
- TOML形式の読みやすいマニフェスト
- ソースディレクトリの再帰探索
- コンパイル・リンクコマンド変更時の自動再ビルド
- 複数ターゲット
- ターゲット間依存関係
- 依存ターゲットのinclude path伝播
- 実行ファイル / 静的ライブラリ / 動的ライブラリのビルド
- カスタムCflags / LDflags
- カスタムリンカスクリプトの対応
- コンパイラ・リンカの指定

## 使い方

> インストール方法は後日追記します

Craftプロジェクトには`craft.toml`が必要です。

```toml
[project]
name = "hello"
```

標準では以下のディレクトリが使用されます。

```
hello/
├── craft.toml
├── src/
│   └── main.c
└── include/
```

`source_dirs`が指定されていない場合は`src`が使用されます。
`include_dirs`が指定されていない場合は、`src`および`include`がincludeパスとして指定されます。

### ビルド

```sh
craft build
```

成果物は`target/`以下に生成されます。

中間生成物はターゲットごとに、

```
target/build/<target>/
```

へ配置されます。

例えば、

```
target/
├── build/
│   └── hello/
│       ├── main.o
│       ├── main.d
│       └── main.cmd
└── hello.exe
```

のようになります。

### 実行

```sh
craft run
```

実行可能ターゲットをビルドしたあと、その成果物を実行します。 複数の実行可能ターゲットがある場合は、ターゲット名を指定できます。

### クリーン

```sh
craft clean
```

`target/` 以下のビルド生成物を削除します。

### インストール

ターゲットを現在のユーザー向けにインストールできます。

```sh
craft install
```

Windowsでは、 `%LOCALAPPDATA%\Microsoft\WindowsApps\XXX.exe`
Linuxでは、`$HOME/.local/bin/XXX`にインストールされます。

### ターゲット一覧を表示

```sh
craft targets
```

プロジェクトに定義されているターゲットを表示します。

### マルチターゲット

Craftでは複数のビルドターゲットを定義できます。

```toml
[project]
name = "example"

[target.app]
type = "executable"
source_dirs = [
    "src/app"
]

[target.library]
type = "staticlib"
source_dirs = [
    "src/library"
]
```

特定のターゲットだけをビルドする場合は、

```sh
craft build app
```

のように指定します。

ターゲットを省略すると、すべてのターゲットがビルドされます。

### ビルドの種類

ビルドの種類は各`target`の中に

```toml
[target]
type = "..."
```

のように記述できます。

#### executable

```toml
type = "executable"
```

実行可能ファイルを生成します。

#### staticlib

```toml
type = "staticlib"
```

静的ライブラリを生成します。

### dynlib

```toml
type = "dynlib"
```

動的ライブラリを生成します。

### ターゲット間依存関係

`dependencies`を使用して、別のCraftターゲットへ依存できます。

```toml
[project]
name = "example"

[target.app]
type = "executable"
dependencies = [
    "testlib"
]

[target.testlib]
type = "staticlib"
source_dirs = [
    "testlib"
]

include_dirs = [
    "testlib"
]
```

この場合、

```sh
craft build app
```

を実行すると、Craftは最初に`testlib`をビルドし、その成果物を`app`のリンク時に使用します。 依存ターゲットの`include_dirs`
も依存元へ自動的に伝播します。

そのため`app`側で同じincludeパスを再定義する必要はありません。 循環依存が存在する場合はビルドを停止します。

### Toolchain

使用するコンパイラとリンカを指定できます。

```toml
[toolchain]
cc = "clang"
ld = "ld.lld"
```

省略時はccおよびldが使用されます。

### コンパイルオプション

```toml
[target.app]
cflags = [
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-O2"
]
```

指定したオプションは各ソースファイルのコンパイル時に渡されます。

### リンクオプション

```toml
[target.app]
ldflags = [
    "-nostdlib"
]
```

### リンカスクリプト

```toml
[target.kernel]
linker_script = "linker.ld"
```

リンク時には指定されたリンカスクリプトが使用されます。 例えばfreestandingなプログラムでは、

```toml
[project]
name = "kernel"

[toolchain]
cc = "clang"
ld = "ld.lld"

[target.kernel]
type = "executable"

source_dirs = [
    "src",
    "arch/x86_64"
]

include_dirs = [
    "src",
    "include",
    "arch/x86_64/include"
]

cflags = [
    "-ffreestanding",
    "-fno-stack-protector",
    "-mno-red-zone"
]

ldflags = [
    "-nostdlib"
]

linker_script = "linker.ld"
```

のような構成になるでしょう。

## ライセンス

[license](./license)ファイルを参照してください。

Copyright (c) 2026 tas0dev.