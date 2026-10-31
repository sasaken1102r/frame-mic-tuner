-- SPDX-License-Identifier: MIT
-- Steam Frame のマイクのフィルターを、設定に合わせてオン・オフする WirePlumber スクリプト。
-- Valve の /etc/wireplumber/scripts/microphone-tracker.lua の代わりに動く（元のファイルは書き換えない）。
--
-- 元のスクリプトと同じく、マイクを使うアプリがいる間だけフィルターを有効にする。
-- 元と違うところ:
-- - エコー除去とノイズ除去を、それぞれ WirePlumber の設定で切れる（ノイズ除去は出力によらず設定どおり。
--   Valve は Frame のスピーカーのときだけ入れる）
--     wpctl settings --save frame-mic.echo-cancel false       # エコー除去を切る（すぐ反映・再起動後も残る）
--     wpctl settings --save frame-mic.noise-suppression true  # ノイズ除去を入れる
--   設定を変えると、使用中のマイクにもその場で反映する（PipeWire の再起動は要らない）
-- - 既定のマイクが Frame の内蔵マイクでないときは、何も入れない（Valve はそのマイクにエコー除去の予備を入れる）
-- - エコー除去は、出力によらず本体（echo_cancel_*）だけを使い、予備（echo_cancel_fallback_*、SteamOS 20261015 から）は
--   使わない。Valve は Frame のスピーカー以外の出力で予備を使い、その消す音の側を今の出力の手前に入れるが、
--   USB-C のドングルではそこから出力へのつながりが動き出せず、音が全部出なくなった（0.3.4）。予備のマイクの側だけを
--   使うと、消す音の側が動かないのでマイクが無音になる。本体の消す音の側は Frame のスピーカーの手前に入る
--   （20261015 より前と同じ）。本体と予備を両方入れると、自動音量調整が 2 回かかってマイクの声が大きくなりすぎる
-- エコー除去の予備が無い SteamOS（20261015 より前）では、0.3.3 までと同じく、名前の頭で見分けて全部の段を入れる
-- （そのころの Valve の tracker も、マイクを使うアプリがいれば全部入れていた）

local BUILTIN_SOURCE = "alsa_input.platform-sound.HiFi__Mic__source"
local BUILTIN_SOURCE_LOOPBACK = "alsa_loopback_device." .. BUILTIN_SOURCE

local state = {
  stream_count = 0,
  source = "",  -- 既定のマイク（default.audio.source の名前）
}

-- フィルターのノード（Valve の設定で steamos.mic_filter = true が付いているもの）
local filters_om = ObjectManager {
  Interest {
    type = "node",
    -- ノードの情報側のプロパティなので type = "pw" が要る（既定の pw-global だと一致しない）
    Constraint { "steamos.mic_filter", "=", "true", type = "pw" },
  }
}

-- フィルターの有効・無効を書き込む先（filters）と、既定のマイク（default）
local metadata_om = ObjectManager {
  Interest {
    type = "metadata",
    Constraint { "metadata.name", "c", "filters", "default" },
  }
}

-- 既定のマイクが Frame の内蔵マイク（Valve のループバックのラッパー）か
local function builtin_source ()
  return state.source == BUILTIN_SOURCE_LOOPBACK
end

-- エコー除去の予備があるか（SteamOS 20261015 から）
local function has_fallback ()
  return filters_om:lookup { Constraint { "node.name", "=", "echo_cancel_fallback_source" } } ~= nil
end

-- 予備があるときの段ごとの決まり。use = 今の既定のマイクでこの段を入れるか、key = 切れる設定。
-- ここに無いフィルター（エコー除去の予備）は入れない
local RULES = {
  eq_source = { use = builtin_source },
  echo_cancel_source = { use = builtin_source, key = "frame-mic.echo-cancel" },
  echo_cancel_sink = { use = builtin_source, key = "frame-mic.echo-cancel" },
  ns_source = { use = builtin_source, key = "frame-mic.noise-suppression" },
}

-- 予備が無いときの決まり（0.3.3 まで）: ノード名の頭から、どの設定でオン・オフするかを決める
-- 戻り値: 設定のキー。nil なら常にマイクの使用状況だけで決める（EQ など）
local function legacy_setting_for (name)
  if name:find ("^echo_cancel") then
    return "frame-mic.echo-cancel"
  end
  if name:find ("^ns_") or name:find ("^dsp_") then
    return "frame-mic.noise-suppression"
  end
  return nil
end

-- 既定の名前（{ "name": "..." }）。無ければ ""
local function default_name (defaults, key)
  local value = defaults:find (0, key)
  if value == nil then
    return ""
  end
  local parsed = Json.Raw (value):parse ()
  return type (parsed) == "table" and parsed.name or ""
end

-- 既定のマイクを読み直す
local function read_defaults ()
  local defaults = metadata_om:lookup { Constraint { "metadata.name", "=", "default" } }
  if defaults == nil then
    return
  end
  local source = default_name (defaults, "default.audio.source")
  -- 内蔵マイクのループバックがまだできていない間は、前の値のまま（Valve と同じ）
  if source == BUILTIN_SOURCE then
    return
  end
  state.source = source
end

-- 1 つのフィルターを今の状態に合わせる
local function apply_node (metadata, node)
  local name = node.properties ["node.name"] or ""
  local node_id = node.properties ["object.id"]

  local enabled
  if has_fallback () then
    local rule = RULES [name]
    enabled = state.stream_count > 0 and rule ~= nil and rule.use ()
        and (rule.key == nil or Settings.get_boolean (rule.key))
  else
    local key = legacy_setting_for (name)
    enabled = state.stream_count > 0 and (key == nil or Settings.get_boolean (key))
  end
  metadata:set (node_id, "filter.smart.disabled", "Spa:String:JSON",
      enabled and "false" or "true")
end

-- すべてのフィルターを今の状態に合わせる
local function apply_all ()
  local metadata = metadata_om:lookup { Constraint { "metadata.name", "=", "filters" } }
  if metadata == nil then
    return
  end
  read_defaults ()
  for node in filters_om:iterate () do
    apply_node (metadata, node)
  end
end

filters_om:connect ("object-added", function ()
  apply_all ()
end)
metadata_om:connect ("object-added", function ()
  apply_all ()
end)

filters_om:activate ()
metadata_om:activate ()

Settings.subscribe ("frame-mic.*", function ()
  apply_all ()
end)

SimpleEventHook {
  name = "frame-mic-defaults-changed",
  interests = {
    EventInterest {
      Constraint { "event.type", "=", "metadata-changed" },
      Constraint { "metadata.name", "=", "default" },
      Constraint { "event.subject.key", "=", "default.audio.source" },
    }
  },
  execute = function (event)
    apply_all ()
  end
}:register ()

SimpleEventHook {
  name = "frame-mic-stream-added",
  interests = {
    EventInterest {
      Constraint { "event.type", "=", "node-added" },
      Constraint { "media.class", "=", "Stream/Input/Audio" },
      Constraint { "node.virtual", "!", "true" },
    }
  },
  execute = function (event)
    state.stream_count = state.stream_count + 1
    if state.stream_count == 1 then
      apply_all ()
    end
  end
}:register ()

SimpleEventHook {
  name = "frame-mic-stream-removed",
  interests = {
    EventInterest {
      Constraint { "event.type", "=", "node-removed" },
      Constraint { "media.class", "=", "Stream/Input/Audio" },
      Constraint { "node.virtual", "!", "true" },
    }
  },
  execute = function (event)
    state.stream_count = math.max (0, state.stream_count - 1)
    if state.stream_count == 0 then
      apply_all ()
    end
  end
}:register ()
