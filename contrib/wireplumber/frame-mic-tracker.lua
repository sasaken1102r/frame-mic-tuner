-- SPDX-License-Identifier: MIT
-- Steam Frame のマイクのフィルターを、設定に合わせてオン・オフする WirePlumber スクリプト。
-- Valve の /etc/wireplumber/scripts/microphone-tracker.lua の代わりに動く（元のファイルは書き換えない）。
--
-- 元のスクリプトと同じく、マイクを使うアプリがいる間だけフィルターを有効にする。エコー除去は、既定の出力が
-- Frame のスピーカーなら本体（echo_cancel_*）、ほかの出力なら予備（echo_cancel_fallback_*）のどちらか 1 つだけ。
-- 元と違うところ:
-- - エコー除去とノイズ除去を、それぞれ WirePlumber の設定で切れる（ノイズ除去は出力によらず設定どおり。
--   Valve は Frame のスピーカーのときだけ入れる）
--     wpctl settings --save frame-mic.echo-cancel false       # エコー除去を切る（すぐ反映・再起動後も残る）
--     wpctl settings --save frame-mic.noise-suppression true  # ノイズ除去を入れる
--   設定を変えると、使用中のマイクにもその場で反映する（PipeWire の再起動は要らない）
-- - 既定のマイクが Frame の内蔵マイクでないときは、何も入れない（Valve はそのマイクにエコー除去の予備を入れる）

local BUILTIN_SOURCE = "alsa_input.platform-sound.HiFi__Mic__source"
local BUILTIN_SINK = "alsa_output.platform-sound.HiFi__Speaker__sink"
local BUILTIN_SOURCE_LOOPBACK = "alsa_loopback_device." .. BUILTIN_SOURCE
local BUILTIN_SINK_LOOPBACK = "alsa_loopback_device.stereo." .. BUILTIN_SINK

local state = {
  stream_count = 0,
  source = "",  -- 既定のマイク（default.audio.source の名前）
  sink = "",    -- 既定の出力（default.audio.sink の名前）
}

-- フィルターのノード（Valve の設定で steamos.mic_filter = true が付いているもの）
local filters_om = ObjectManager {
  Interest {
    type = "node",
    -- ノードの情報側のプロパティなので type = "pw" が要る（既定の pw-global だと一致しない）
    Constraint { "steamos.mic_filter", "=", "true", type = "pw" },
  }
}

-- フィルターの有効・無効と行き先を書き込む先（filters）と、既定のマイク・出力（default）
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

-- エコー除去の予備があるか（SteamOS 20261015 から。それより前は本体だけで、出力によらず本体を入れていた）
local function has_fallback ()
  return filters_om:lookup { Constraint { "node.name", "=", "echo_cancel_fallback_source" } } ~= nil
end

-- エコー除去の本体を使う: 内蔵マイクで、出力が Frame のスピーカー（予備が無いなら出力によらず）
local function use_main_echo_cancel ()
  return builtin_source () and (state.sink == BUILTIN_SINK_LOOPBACK or not has_fallback ())
end

-- エコー除去の予備を使う: 内蔵マイクで、出力がほかの機器（イヤホン・Bluetooth など）
local function use_fallback_echo_cancel ()
  return builtin_source () and state.sink ~= BUILTIN_SINK_LOOPBACK
end

-- 段ごとの決まり。use = 今の既定のマイク・出力でこの段を入れるか、key = 切れる設定。
-- ここに無いフィルターは Valve と同じく入れない
-- （エコー除去は本体と予備の 2 段があり、Valve はどちらか 1 つしか入れない。両方入れると自動音量調整が 2 回かかる）
local RULES = {
  eq_source = { use = builtin_source },
  echo_cancel_source = { use = use_main_echo_cancel, key = "frame-mic.echo-cancel" },
  echo_cancel_sink = { use = use_main_echo_cancel, key = "frame-mic.echo-cancel" },
  echo_cancel_fallback_source = { use = use_fallback_echo_cancel, key = "frame-mic.echo-cancel" },
  echo_cancel_fallback_sink = { use = use_fallback_echo_cancel, key = "frame-mic.echo-cancel" },
  ns_source = { use = builtin_source, key = "frame-mic.noise-suppression" },
}

-- 既定の名前（{ "name": "..." }）。無ければ ""
local function default_name (defaults, key)
  local value = defaults:find (0, key)
  if value == nil then
    return ""
  end
  local parsed = Json.Raw (value):parse ()
  return type (parsed) == "table" and parsed.name or ""
end

-- 既定のマイク・出力を読み直す
local function read_defaults ()
  local defaults = metadata_om:lookup { Constraint { "metadata.name", "=", "default" } }
  if defaults == nil then
    return
  end
  local source = default_name (defaults, "default.audio.source")
  local sink = default_name (defaults, "default.audio.sink")
  -- 内蔵のマイク・スピーカーのループバックがまだできていない間は、前の値のまま（Valve と同じ）
  if source == BUILTIN_SOURCE or sink == BUILTIN_SINK then
    return
  end
  state.source = source
  state.sink = sink
end

-- 1 つのフィルターを今の状態に合わせる
local function apply_node (metadata, node)
  local name = node.properties ["node.name"] or ""
  local node_id = node.properties ["object.id"]

  -- エコー除去の予備の消す音（スピーカーへの音）は、今の出力から取る（Valve と同じ）。
  -- Frame のスピーカーのときは設定の既定のまま（入れないので、どちらでもよい）
  if name == "echo_cancel_fallback_sink" then
    local value = nil
    if state.sink ~= "" and state.sink ~= BUILTIN_SINK_LOOPBACK then
      value = "{ node.name = \"" .. state.sink .. "\" }"
    end
    if metadata:find (node_id, "filter.smart.target") ~= value then
      metadata:set (node_id, "filter.smart.target", "Spa:String:JSON", value)
    end
  end

  local rule = RULES [name]
  local enabled = state.stream_count > 0 and rule ~= nil and rule.use ()
      and (rule.key == nil or Settings.get_boolean (rule.key))
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
      Constraint { "event.subject.key", "c", "default.audio.source", "default.audio.sink" },
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
