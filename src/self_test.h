// --self-test の一部（音の出口: 一覧の読み取り・キー・出口ごとの設定のかけ方・設定ファイル）。
// OpenVR も外部コマンドも使わず、Frame で取った pw-dump・pw-metadata の出力で試す。
#pragma once

#include <functional>

/** 1 つの確かめの結果を出す関数（何を確かめたか、合っていれば true）。 */
using Expect = std::function<void(const char*, bool)>;

/**
 * 音の出口の確かめをすべて行う。
 * @param expect 結果を出す関数
 */
void outputSelfTests(const Expect& expect);
