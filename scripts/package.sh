#!/usr/bin/env bash
# リリースの tar.gz を作る。Steam Frame の上で実行する（本体の cairo・FreeType・PipeWire・SteamVR の OpenVR にリンクするため）。
#   scripts/package.sh   → dist/frame-mic-tuner-<バージョン>.tar.gz, dist/SHA256SUMS
# 中身: 実行ファイル、install.sh（tar.gz からならビルドせずそのまま入れられる）、vendor/frame-updater/frame-update.sh
# （install.sh が ~/.local/share/frame-mic-tuner/ に置く更新スクリプト）、contrib（WirePlumber・.desktop・.service・
# アイコン・設定の例）、README（7 言語）・CHANGELOG・LICENSE など。CMakeLists.txt や src/ は入れない
# （install.sh はそれが無ければ「tar.gz から」と見なし、ビルドをせずに入れる）。
set -euo pipefail
cd "$(dirname "$0")/.."

name="frame-mic-tuner"
build_dir="build-release"

if [[ "$(uname -m)" != "aarch64" ]]; then
    echo "Run this on the Steam Frame (aarch64). This machine is $(uname -m)." >&2
    exit 1
fi

# 同梱する vendor/frame-updater/ が、手で書き換えられていない・frame-updater 本体とずれていないかを確かめる
sh vendor/frame-updater/verify.sh
# 共通の UI 部品（vendor/frame-ui/）と、ほかのアプリの一覧（vendor/frame-apps/）も同じく確かめる
# （どちらも実行ファイルに焼き込むので tar.gz には入れない）
sh vendor/frame-ui/verify.sh
sh vendor/frame-apps/verify.sh

# 開発用の build/ とは別のフォルダで、Release でビルドし直す
cmake -G Ninja -S . -B "$build_dir" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build "$build_dir" --clean-first

# 安定版 SteamOS（glibc 2.39）でも起動するか: 要求する glibc の版が 2.38 を超えていたら止める
# （ベータの Frame でビルドすると、関数によっては新しい版につながる。0.3.6 の前に log10f が GLIBC_2.43 になった）
max_glibc="2.38"
need_glibc="$(objdump -T "$build_dir/$name" | grep -oE 'GLIBC_[0-9]+\.[0-9]+' | sed 's/GLIBC_//' | sort -Vu | tail -n 1)"
if [[ -z "$need_glibc" ]]; then
    echo "Could not read the glibc versions from $build_dir/$name (objdump -T)" >&2
    exit 1
fi
if [[ "$(printf '%s\n%s\n' "$need_glibc" "$max_glibc" | sort -V | tail -n 1)" != "$max_glibc" ]]; then
    echo "The binary needs GLIBC_$need_glibc (stable SteamOS has 2.39; allowed up to $max_glibc):" >&2
    objdump -T "$build_dir/$name" | grep -E "GLIBC_$need_glibc" >&2
    exit 1
fi
echo "glibc: needs up to GLIBC_$need_glibc (allowed up to $max_glibc)"

version="$("$build_dir/$name" --version | awk '{print $2}')"
if [[ -z "$version" || "$version" == "unknown" ]]; then
    echo "Could not read the version from $build_dir/$name --version" >&2
    exit 1
fi

stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT
root="$stage/$name"
mkdir -p "$root/contrib/icons" "$root/contrib/wireplumber" "$root/vendor/frame-updater" "$root/third_party/openvr"
install -m755 "$build_dir/$name" "$root/$name"
strip "$root/$name"
install -m755 install.sh "$root/"
install -m644 LICENSE THIRD_PARTY_LICENSES.md README.md README.ja.md README.sc.md README.de.md README.fr.md README.es.md README.it.md CHANGELOG.md "$root/"
install -m644 "contrib/$name.service" "contrib/$name.desktop" contrib/config.example.json "$root/contrib/"
install -m644 contrib/icons/*.png "$root/contrib/icons/"
install -m644 contrib/wireplumber/* "$root/contrib/wireplumber/"
install -Dm644 third_party/openvr/LICENSE "$root/third_party/openvr/LICENSE"
# install.sh が ~/.local/share/frame-mic-tuner/ に入れる更新スクリプト（cpp/*・python/* は不要。パネルにはもう埋め込み済み）
install -m755 vendor/frame-updater/frame-update.sh "$root/vendor/frame-updater/"

mkdir -p dist
out="dist/$name-$version.tar.gz"
# 持ち主の名前は入れない（uid 0 にそろえる）
tar -C "$stage" --owner=0 --group=0 --numeric-owner --sort=name -czf "$out" "$name"
(cd dist && sha256sum "$(basename "$out")" > SHA256SUMS)
echo
tar -tzvf "$out"
echo
cat dist/SHA256SUMS
echo
echo "リリースにする（タグ v$version は gh release create が作る。リリースノートはリポジトリの外のファイルに書いて渡す）:"
echo "  gh release create v$version $out dist/SHA256SUMS --title v$version --notes-file <リリースノートのファイル>"
echo "下書き（--draft）・プレリリース（--prerelease）にはしない: パネルの更新は /releases/latest を見るので、見つけられなくなる。"
echo "SHA256SUMS も必ず添付する（無いと更新ボタンから入れられず、手で更新してもらうことになる）。"
