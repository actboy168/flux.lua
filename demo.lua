local WIDTH = 800
local HEIGHT = 620
local TITLEBAR = 34
local CONTENT_X = 24
local CONTENT_W = WIDTH - CONTENT_X * 2
local CONTENT_Y = 92
local CONTENT_H = 450
local FOOTER_Y = HEIGHT - 70
local HINT_Y = HEIGHT - 42

local function arg_or(index, fallback)
    local value = arg and arg[index]
    if value == nil or value == "" then
        return fallback
    end
    return value
end

local max_frames = tonumber(arg_or(1, ""))
local requested_backdrop = arg_or(2, "mica")
local requested_dpi = tonumber(arg_or(3, ""))
local requested_page = arg_or(4, "basics")

if requested_dpi then
    package.cpath = "build/test/bin/?.dll;" .. package.cpath
end
local flux = require "flux"

local ok, err = flux.create {
    title = "flux.lua demo",
    width = WIDTH,
    height = HEIGHT,
    titlebar_height = TITLEBAR,
    backdrop = requested_backdrop,
}
assert(ok, err)
flux.show()
if requested_dpi then
    flux.testing.debug_set_dpi_scale(requested_dpi)
end

local opaque = requested_backdrop == "none"

local themes = {
    dark = {
        bg = 0xFF202020,
        tint = 0x33000000,
        titlebar = 0x00000000,
        separator = 0x14FFFFFF,
        text = 0xFFFFFFFF,
        dim = 0xFFC8C8C8,
        card = 0xCC2B2B2B,
        card_border = 0x1AFFFFFF,
        panel = 0x14FFFFFF,
        panel_hover = 0x24FFFFFF,
        panel_active = 0x33FFFFFF,
        border = 0x1FFFFFFF,
        white = 0xFFFFFFFF,
        disabled = 0xFF7A7A7A,
        track = 0x1FFFFFFF,
        popup_bg = 0xF22B2B2B,
        popup_border = 0x24FFFFFF,
        popup_shadow = 0x66000000,
        toast_shadow = 0x40000000,
        tooltip_bg = 0xF22B2B2B,
        tooltip_border = 0x24FFFFFF,
        tooltip_shadow = 0x66000000,
    },
    light = {
        bg = 0xFFF3F3F3,
        tint = 0xCCF3F3F3,
        titlebar = 0x00000000,
        separator = 0x14000000,
        text = 0xFF1B1B1B,
        dim = 0xFF5C5C5C,
        card = 0xFFFFFFFF,
        card_border = 0xFFE5E5E5,
        panel = 0xFFF9F9F9,
        panel_hover = 0xFFF0F0F0,
        panel_active = 0xFFE8E8E8,
        border = 0xFFE0E0E0,
        white = 0xFFFFFFFF,
        disabled = 0xFF9A9A9A,
        track = 0xFFE5E5E5,
        popup_bg = 0xFFFFFFFF,
        popup_border = 0xFFE0E0E0,
        popup_shadow = 0x33000000,
        toast_shadow = 0x26000000,
        tooltip_bg = 0xFFFFFFFF,
        tooltip_border = 0xFFE0E0E0,
        tooltip_shadow = 0x33000000,
    },
}

local state = {
    page = requested_page,
    theme = "dark",
    accent = 1,
    name = "flux.lua",
    password = "flux-demo",
    count = 4,
    ratio = 0.65,
    enabled = true,
    choice = 2,
    selected_row = 1,
    action = "就绪",
    blur = false,
    layer_shadow = false,
    menu_open = false,
    menu_request = false,
    menu_kind = "",
    menu_row = 0,
    menu_x = 0,
    menu_y = 0,
    toast = { visible = false, age = 0, text = "" },
}

local accent_palette = {
    { name = "蓝", color = 0xFFD47800, hover = 0xFFD9861A, active = 0xFFB16300 },
    { name = "青", color = 0xFF6C7B0F, hover = 0xFF7B8C16, active = 0xFF58650B },
    { name = "紫", color = 0xFFB0578E, hover = 0xFFBD6B9C, active = 0xFF944774 },
    { name = "品红", color = 0xFFB339C2, hover = 0xFFC250CF, active = 0xFF952EA2 },
    { name = "橙", color = 0xFF1050CA, hover = 0xFF2662D9, active = 0xFF0D42A8 },
}

local pages = {
    { key = "basics", label = "基础" },
    { key = "input", label = "输入" },
    { key = "layout", label = "布局" },
    { key = "list", label = "列表" },
    { key = "table", label = "表格" },
    { key = "visual", label = "视觉" },
}

local page_exists = false
for _, page in ipairs(pages) do
    if page.key == state.page then
        page_exists = true
    end
end
if not page_exists then
    state.page = "basics"
end

local function apply_accent(index)
    local accent = accent_palette[index] or accent_palette[1]
    state.accent = index
    flux.set_style_color("accent", accent.color)
    flux.set_style_color("accent_hover", accent.hover)
    flux.set_style_color("accent_active", accent.active)
    flux.set_style_color("tab_focus", accent.color)
end

local function apply_theme(name)
    local theme = themes[name]
    if not theme then
        return
    end
    state.theme = name
    flux.set_style_color("text", theme.text)
    flux.set_style_color("text_dim", theme.dim)
    flux.set_style_color("panel", theme.panel)
    flux.set_style_color("panel_hover", theme.panel_hover)
    flux.set_style_color("panel_active", theme.panel_active)
    flux.set_style_color("border", theme.border)
    flux.set_style_color("white", theme.white)
    flux.set_style_color("disabled", theme.disabled)
    flux.set_style_color("track", theme.track)
    flux.set_style_color("tab_bg", theme.panel)
    flux.set_style_color("tab_bg_hover", theme.panel_hover)
    flux.set_style_color("tab_bg_selected", theme.panel_active)
    flux.set_style_color("tab_text", theme.text)
    flux.set_style_color("tab_text_selected", theme.text)
    flux.set_style_color("tab_baseline", theme.border)
    flux.set_style_color("popup_bg", theme.popup_bg)
    flux.set_style_color("popup_border", theme.popup_border)
    flux.set_style_color("popup_shadow", theme.popup_shadow)
    flux.set_style_color("menu_text", theme.text)
    flux.set_style_color("menu_shortcut", theme.dim)
    flux.set_style_color("menu_item_hover", theme.panel_hover)
    flux.set_style_color("menu_separator", theme.border)
    flux.set_style_color("tooltip_bg", theme.tooltip_bg)
    flux.set_style_color("tooltip_border", theme.tooltip_border)
    flux.set_style_color("tooltip_shadow", theme.tooltip_shadow)
    flux.set_style_color("tab_bar_bg", 0x00000000)
    flux.set_style_color("tab_bar_border", 0x00000000)
    flux.set_style_color("tab_bg", 0x00000000)
    flux.set_style_color("tab_bg_selected", theme.card)
    flux.set_style_var("rounding", 4)
    flux.set_style_var("border_size", 1)
    flux.set_style_var("item_spacing_x", 8)
    flux.set_style_var("item_spacing_y", 8)
    flux.set_style_var("font_size", 14)
    flux.set_style_var("line_height", 20)
    flux.set_style_var("scrollbar_size", 4)
    flux.set_style_var("scrollbar_idle_alpha", 0)
    flux.set_style_var("tab_height", 32)
    flux.set_style_var("tab_rounding", 4)
    apply_accent(state.accent)
end

apply_theme(state.theme)

local function notify(text)
    state.action = text
    state.toast.visible = true
    state.toast.age = 0
    state.toast.text = text
end

local function toggle_theme()
    apply_theme(state.theme == "dark" and "light" or "dark")
    notify("主题：" .. state.theme)
end

local function set_blur(value)
    state.blur = value
    local radius = value and not flux.gfx_limited() and 6 or 0
    flux.layer.blur("content", radius)
    state.action = value and "背景模糊已开启" or "背景模糊已关闭"
end

local function update_toast()
    if not state.toast.visible then
        return
    end
    state.toast.age = state.toast.age + flux.input.delta_time()
    if state.toast.age >= 2.2 then
        state.toast.visible = false
    end
end

local function draw_titlebar()
    local theme = themes[state.theme]
    flux.rect_filled(0, 0, WIDTH, TITLEBAR, theme.titlebar)
    flux.rect_filled(0, TITLEBAR, WIDTH, 1, theme.separator)
    flux.text(24, 9, "flux.lua", theme.text, 12, { weight = "semibold" })

    local close_x, close_y, close_w, close_h = flux.close_rect()
    local hovered = flux.input.mouse_in_rect(close_x, close_y, close_w, close_h)
    local active = hovered and flux.input.mouse_down("left")
    if hovered then
        flux.rect_filled(close_x, close_y, close_w, close_h,
            active and 0xFF1E2AB0 or 0xFF1C2BC4, 0)
    end
    local text_width, text_height = flux.measure_text("×", 16)
    flux.text(close_x + (close_w - text_width) / 2,
        close_y + (close_h - text_height) / 2,
        "×", hovered and 0xFFFFFFFF or theme.dim, 16)
end

local function draw_tabs()
    flux.set_cursor_pos(CONTENT_X, 48)
    local labels = {}
    local selected = 1
    for index, page in ipairs(pages) do
        labels[index] = page.label
        if page.key == state.page then
            selected = index
        end
    end
    local next_index = flux.tab_bar("demo_tabs", labels, {
        id = "tabs",
        selected = selected,
        width = 636,
        height = 30,
    })
    if next_index ~= selected and pages[next_index] then
        state.page = pages[next_index].key
        state.action = "页面：" .. pages[next_index].label
    end
end

local function draw_basics()
    local width = CONTENT_W - 32
    flux.label("基础控件", 0, 20)
    flux.spacing(1)
    flux.label("按钮、选择、输入、下拉和进度条都使用同一套样式与焦点系统。",
        0, 12, width, true, 2)
    flux.spacing(3)

    flux.begin_row()
    if flux.button("主操作", 132, 34, { id = "primary", primary = true }) then
        notify("点击了主操作")
    end
    do
        local bx, by, bw, bh = flux.item_rect()
        flux.rect_tooltip("主操作：演示 rect_tooltip", bx, by, bw, bh, { delay = 0.35 })
    end
    if flux.button("次要操作", 132, 34, { id = "secondary" }) then
        notify("点击了次要操作")
    end
    local changed
    changed, state.enabled = flux.checkbox("启用选项", state.enabled, { id = "enabled" })
    if changed then
        notify("启用选项：" .. tostring(state.enabled))
    end
    flux.end_row()

    flux.spacing(2)
    flux.begin_row()
    if flux.selectable("选项 A", state.choice == 1, { id = "choice_a", w = 160, h = 32 }) then
        state.choice = 1
        notify("选择：A")
    end
    if flux.selectable("选项 B", state.choice == 2, { id = "choice_b", w = 160, h = 32 }) then
        state.choice = 2
        notify("选择：B")
    end
    if flux.button("切换主题", 132, 32, { id = "theme" }) then
        toggle_theme()
    end
    flux.end_row()

    flux.spacing(3)
    flux.label("单行输入", 0, 14)
    flux.begin_row()
    local name_changed, name_value = flux.input_text("名称", state.name, { id = "name", w = 260, h = 32 })
    if name_changed then
        state.name = name_value
    end
    local count_changed, count_value = flux.input_int("数量", state.count, { id = "count", w = 92, h = 32 })
    if count_changed then
        state.count = math.max(0, count_value)
    end
    local ratio_changed, ratio_value = flux.input_float("比例", state.ratio, { id = "ratio", w = 100, h = 32 })
    if ratio_changed then
        state.ratio = math.max(0, math.min(1, ratio_value))
    end
    flux.end_row()

    flux.spacing(2)
    flux.begin_row()
    flux.label("预设", 0, 14)
    local preset_changed, preset =
        flux.combo("预设", state.choice, { "Alpha", "Beta", "Gamma" }, { id = "preset", w = 160, h = 32 })
    if preset_changed then
        state.choice = preset
        notify("预设：" .. ({ "Alpha", "Beta", "Gamma" })[preset])
    end
    flux.end_row()

    flux.spacing(3)
    flux.label(("进度 %.0f%%"):format(state.ratio * 100), 0, 14)
    flux.progress_bar("demo_ratio", state.ratio, width, { h = 8 })
    flux.spacing(2)
    flux.progress_bar("demo_indeterminate", -1, width, { h = 6, indeterminate = true })

    flux.spacing(3)
    flux.separator(width)
    flux.spacing(2)
    if flux.button("强调色：" .. accent_palette[state.accent].name, 180, 34, { id = "styled", primary = true }) then
        local next_index = state.accent % #accent_palette + 1
        apply_accent(next_index)
        notify("强调色：" .. accent_palette[next_index].name)
    end

    flux.spacing(3)
    flux.label(
        "这是一段用于演示自动换行和省略号的文本。flux 使用 DirectWrite 排版，支持逻辑像素、DPI 缩放和系统字体回退。",
        0, 13, width, true, 3)
end

local function draw_input()
    local width = CONTENT_W - 32
    flux.label("输入", 0, 20)
    flux.spacing(1)
    flux.label("文本、整数、浮点和密码输入共享同一套编辑、选区与 IME 行为。",
        0, 12, width, true, 2)
    flux.spacing(3)

    flux.label("普通文本", 0, 14)
    local changed, value, submitted, deactivated, focused = flux.input_text("演示名称", state.name, { id = "input_name", w = 320, h = 32 })
    if changed then
        state.name = value
    end
    if submitted then
        notify("提交名称：" .. state.name)
    end

    flux.spacing(2)
    flux.label("密码", 0, 14)
    local password_changed, password_value = flux.input_text("演示密码", state.password, { id = "input_password", w = 320, h = 32, password = true })
    if password_changed then
        state.password = password_value
    end

    flux.spacing(3)
    flux.label("数值", 0, 14)
    flux.begin_row()
    local count_changed, count_value = flux.input_int("演示整数", state.count, { id = "input_count", w = 112, h = 32 })
    if count_changed then
        state.count = count_value
    end
    local ratio_changed, ratio_value = flux.input_float("演示浮点", state.ratio, { id = "input_ratio", w = 128, h = 32 })
    if ratio_changed then
        state.ratio = math.max(0, math.min(1, ratio_value))
    end
    flux.end_row()

    flux.spacing(3)
    flux.label(("焦点 %s · 提交 %s · 失焦 %s"):format(
        tostring(focused), tostring(submitted), tostring(deactivated)), 0, 12)
    flux.label("点击输入框后可以输入文本；中文输入法会显示 composition 预览。",
        0, 12, width, true, 2)
end

local function draw_layout()
    local width = CONTENT_W - 32
    flux.label("布局", 0, 20)
    flux.spacing(1)
    flux.label("表单行、行/列、缩进、嵌套 child 和滚动。", 0, 12, width, true, 2)
    flux.spacing(3)

    flux.begin_form_row(120)
    flux.form_label("名称")
    local name_changed, name_value = flux.input_text("表单名称", state.name, { id = "form_name", w = 240, h = 30 })
    if name_changed then
        state.name = name_value
    end
    flux.end_form_row()

    flux.begin_form_row(120)
    flux.form_label("数量")
    local count_changed, count_value = flux.input_int("表单数量", state.count, { id = "form_count", w = 90, h = 30 })
    if count_changed then
        state.count = count_value
    end
    flux.same_line()
    flux.checkbox("启用", state.enabled, { id = "form_enabled" })
    flux.end_form_row()

    flux.begin_form_row(120)
    flux.form_label("范围")
    flux.progress_bar("form_progress", state.ratio, 240, { h = 8 })
    flux.end_form_row()

    flux.spacing(4)
    flux.label("行与列", 0, 14)
    flux.begin_row()
    flux.begin_column()
    flux.label("左列", 0, 12)
    if flux.button("左按钮", 140, 30, { id = "left_button" }) then
        notify("左列按钮")
    end
    flux.end_column()
    flux.begin_column()
    flux.label("右列", 0, 12)
    if flux.button("右按钮", 140, 30, { id = "right_button" }) then
        notify("右列按钮")
    end
    flux.end_column()
    flux.end_row()

    flux.spacing(4)
    flux.label("嵌套 child", 0, 14)
    flux.begin_child("layout_inner", 320, 112)
    for index = 1, 20 do
        flux.label(("可滚动内容 %02d"):format(index), 0, 12)
        flux.spacing(1)
    end
    flux.end_child()
    flux.label(("内部滚动位置 %.1f"):format(flux.child_scroll("layout_inner")), 0, 12)
end

local LIST_TOTAL = 5000

local function draw_list()
    local width = CONTENT_W - 32
    flux.label("虚拟列表", 0, 20)
    flux.spacing(1)
    flux.label("只绘制可见行；滚轮、滚动条、方向键和 Ctrl/Shift 多选都由框架处理。",
        0, 12, width, true, 2)
    flux.spacing(2)
    flux.begin_row()
    if flux.button("全选", 80, 28, { id = "list_all" }) then
        flux.list_select_all("items")
        notify("已全选")
    end
    if flux.button("清空", 80, 28, { id = "list_clear" }) then
        flux.list_clear_selection("items")
        notify("已清空选择")
    end
    if flux.button("跳到 2500", 110, 28, { id = "list_jump" }) then
        flux.list_scroll_to("items", 2500, "center")
        notify("滚动到第 2500 行")
    end
    flux.end_row()
    flux.spacing(2)

    local _, first, last = flux.begin_list("items", LIST_TOTAL, 28, width, 300, { mode = "extended" })
    for index = first, last do
        local clicked, double_clicked, right_clicked, selected = flux.list_item(index)
        if clicked then
            state.selected_row = index
            state.action = "选择第 " .. index .. " 行"
        end
        if double_clicked then
            notify("双击第 " .. index .. " 行")
        end
        if right_clicked then
            state.selected_row = index
            state.menu_kind = "row"
            state.menu_row = index
            state.menu_open = true
            state.menu_request = true
            state.menu_x, state.menu_y = flux.input.mouse_pos()
        end

        local x, y, w, h = flux.item_rect()
        local color = selected and flux.style_color("white") or flux.style_color("text")
        flux.text(x + 14, y + 5, ("%04d"):format(index), color, 13)
        flux.text(x + 74, y + 5, ("Item %04d"):format(index), color, 13)
        flux.text(x + w - 150, y + 5, ("%d KB"):format(16 + index % 128), color, 13)
    end
    flux.end_list()

    flux.spacing(2)
    flux.label(("共 %d 行 · 已选 %d · 当前行 %d"):format(
        LIST_TOTAL, flux.list_selected_count("items"), state.selected_row), 0, 12)
end

local TABLE_ROWS = {
    { "Widget", "Control", "12 KB" },
    { "TextLayout", "Text", "48 KB" },
    { "Layer", "Composition", "8 KB" },
    { "DrawList", "Render", "96 KB" },
    { "Popup", "Overlay", "24 KB" },
    { "VirtualList", "Data", "18 KB" },
    { "Theme", "Style", "4 KB" },
    { "Animation", "Motion", "32 KB" },
}

local function draw_table()
    local width = CONTENT_W - 32
    flux.label("表格", 0, 20)
    flux.spacing(1)
    flux.label("固定表头、可滚动 body、行选中和双击/右键事件。",
        0, 12, width, true, 2)
    flux.spacing(2)

    local selected = state.selected_row
    if flux.begin_table("demo_table", { "名称", "类型", "大小" }, { 280, 200, 140 }, width, 300, { id = "table" }) then
        for index, row in ipairs(TABLE_ROWS) do
            local clicked = flux.table_next_row(index, selected == index)
            if clicked then
                state.selected_row = index
                state.action = "选择：" .. row[1]
            end
            flux.table_cell_text(row[1])
            flux.table_cell_text(row[2])
            flux.table_cell_text(row[3])
        end
        flux.end_table()
    end
end

local function draw_visual()
    local width = CONTENT_W - 32
    flux.label("视觉与动画", 0, 20)
    flux.spacing(1)
    flux.label("绘制原语、裁剪、alpha/offset、控件补间、图层动画和弹出层。",
        0, 12, width, true, 2)
    flux.spacing(3)

    local x, y = flux.cursor_pos()
    flux.rect_gradient(x, y, width, 52, 0xFF3C8AE0, 0xFF8E44AD, 10, true)
    flux.text(x + 16, y + 16, "rect_gradient", 0xFFFFFFFF, 14, { weight = "semibold" })
    flux.set_cursor_pos(x, y + 60)

    x, y = flux.cursor_pos()
    flux.rect_radial(x, y, width, 52, 0x8840A0FF, 0x00000000, 10)
    flux.text(x + 16, y + 16, "rect_radial", flux.style_color("text"), 14,
        { weight = "semibold" })
    flux.set_cursor_pos(x, y + 60)

    x, y = flux.cursor_pos()
    flux.shadow(x, y, width, 52, 10, 0x66000000, 10, 5)
    flux.rect_filled(x, y, width, 52, flux.style_color("panel"), 10)
    flux.text(x + 16, y + 16, "shadow + rect_filled", flux.style_color("text"), 14,
        { weight = "semibold" })
    flux.set_cursor_pos(x, y + 64)

    x, y = flux.cursor_pos()
    flux.push_clip(x, y, width, 30)
    for index = 0, 9 do
        flux.rect_filled(x + index * 76, y, 68, 30,
            index % 2 == 0 and 0xFF3C8AE0 or 0xFF8E44AD, 0)
    end
    flux.pop_clip()
    flux.set_cursor_pos(x, y + 38)
    flux.label("push_clip / pop_clip", 0, 12)
    flux.spacing(2)

    x, y = flux.cursor_pos()
    flux.push_alpha(0.45)
    flux.text(x, y, "push_alpha 45%", flux.style_color("text"), 13)
    flux.pop_alpha()
    flux.push_offset(10, 0)
    flux.text(x + 160, y, "push_offset", flux.style_color("text"), 13)
    flux.pop_offset()
    flux.spacing(2)
    flux.dummy(0, 26)

    flux.begin_row()
    if flux.button("播放图层转场", 150, 32, { id = "layer_motion" }) then
        flux.layer.set("content", { opacity = 0, offset = { 0, 18 } })
        flux.layer.animate("content", {
            opacity = { from = 0, to = 1, duration = 0.28, easing = "out-cubic" },
            offset = { from = { 0, 18 }, to = { 0, 0 }, duration = 0.28, easing = "out-cubic" },
        })
        notify("图层转场")
    end
    local shadow_changed, shadow_value =
        flux.checkbox("图层阴影", state.layer_shadow, { id = "layer_shadow" })
    if shadow_changed then
        state.layer_shadow = shadow_value
        flux.layer.shadow("content", shadow_value and {
            blur = 18, offset = { 0, 6 }, opacity = 0.45,
        } or nil)
        notify(shadow_value and "图层阴影已开启" or "图层阴影已关闭")
    end
    local blur_changed, blur_value =
        flux.checkbox("模糊背景", state.blur, { id = "blur" })
    if blur_changed then
        set_blur(blur_value)
    end
    flux.end_row()

    local info = flux.layer.get("content")
    flux.label(("content 层 opacity %.2f · offset_y %.1f · reduced_motion=%s · time=%.1f"):format(
        info.opacity, info.offset.y,
        tostring(flux.anim.reduced_motion()), flux.time()), 0, 12)
    flux.spacing(3)

    flux.label("自绘控件动画", 0, 14)
    local hovered, active, clicked =
        flux.invisible_button("动画卡片", width, 76, { id = "anim_card", focusable = false, interactive = true })
    local cx, cy, card_w, card_h = flux.item_rect()
    local hover, press = flux.control_anim("anim_card", { hovered = hovered, active = active })
    local panel = flux.style_color("panel")
    local accent = flux.style_color("accent")
    local fill = flux.mix_color(panel, accent, hover * 0.45)
    local inset = press * 2
    flux.rect_filled(cx + inset, cy + inset, card_w - inset * 2, card_h - inset * 2, fill, 10)
    flux.text(cx + 18 + inset, cy + 14 + inset,
        "control_anim + mix_color", flux.style_color("text"), 14, { weight = "semibold" })
    flux.text(cx + 18 + inset, cy + 40 + inset,
        "hover/press 只影响绘制，不改变布局和命中区域。",
        flux.style_color("text_dim"), 12)
    if clicked then
        notify("点击了自绘卡片")
    end

    flux.spacing(3)
    flux.begin_row()
    if flux.button("显示通知", 132, 32, { id = "toast" }) then
        notify("这是一个通知")
    end
    if flux.button("打开菜单", 132, 32, { id = "menu" }) then
        local mx, my, _, mh = flux.item_rect()
        state.menu_kind = "actions"
        state.menu_open = true
        state.menu_request = true
        state.menu_x = mx
        state.menu_y = my + mh
    end
    flux.label("tooltip / popup 使用独立绘制缓冲", 0, 12)
    flux.end_row()
end

local function draw_toast()
    if not state.toast.visible then
        return
    end
    local age = state.toast.age
    local p = math.min(1, age / 0.16, (2.2 - age) / 0.2)
    p = math.max(0, p)
    local function alpha(color)
        local value = math.floor((((color >> 24) & 0xFF) * p) + 0.5)
        return (color & 0x00FFFFFF) | (value << 24)
    end
    local theme = themes[state.theme]
    local x, y, w, h = WIDTH - 274, 100 + (1 - p) * 10, 248, 56
    flux.shadow(x, y + 2, w, h, 8, alpha(theme.toast_shadow), 5, 2)
    flux.rect_filled(x, y, w, h, alpha(theme.card), 8)
    flux.rect(x, y, w, h, alpha(theme.card_border), 8, 1)
    flux.text(x + 14, y + 10, "通知", alpha(theme.text), 13, { weight = "semibold" })
    flux.text(x + 14, y + 30, state.toast.text, alpha(theme.dim), 12)
end

local function draw_menu()
    if not state.menu_open then
        if flux.popup_is_open("demo_menu") then
            flux.popup_close("demo_menu")
        end
        return
    end
    if state.menu_request then
        state.menu_request = false
        flux.popup_open("demo_menu")
    end
    if not flux.popup_is_open("demo_menu") then
        state.menu_open = false
        return
    end
    local is_row = state.menu_kind == "row"
    local title = is_row and ("第 " .. state.menu_row .. " 行") or "操作"
    local menu_height = flux.menu_measure {
        items = 3,
        separators = 1,
        text_rows = 0,
        text_row_height = 18,
    }
    local px, py, pw, ph = flux.popup_place(
        { x = state.menu_x, y = state.menu_y, w = 1, h = 1 },
        190, menu_height, { gap = 4, margin = 8 })
    if flux.begin_popup(title, px, py, pw, ph, { id = "demo_menu" }) then
        if is_row then
            if flux.menu_item("选择此行", { id = "menu_select", shortcut = "", checked = false }) then
                state.selected_row = state.menu_row
                notify("选择第 " .. state.menu_row .. " 行")
                state.menu_open = false
            end
            if flux.menu_item("查看详情", { id = "menu_details", shortcut = "", checked = false }) then
                notify("详情：" .. state.menu_row)
                state.menu_open = false
            end
        else
            if flux.menu_item("显示通知", { id = "menu_toast", shortcut = "", checked = false }) then
                notify("来自菜单的通知")
                state.menu_open = false
            end
            if flux.menu_item("切换主题", { id = "menu_theme", shortcut = "", checked = false }) then
                toggle_theme()
                state.menu_open = false
            end
        end
        flux.menu_separator()
        if flux.menu_item("关闭", { id = "menu_close", shortcut = "Esc", checked = false }) then
            state.menu_open = false
        end
        flux.end_popup()
    else
        state.menu_open = false
    end
end

local function draw()
    local theme = themes[state.theme]
    for _, _ in ipairs(flux.anim.poll()) do
        notify("图层动画完成")
    end
    update_toast()
    flux.clear(opaque and theme.bg or 0x00000000)
    if not opaque then
        flux.rect_filled(0, 0, WIDTH, HEIGHT, theme.tint)
    end

    draw_titlebar()
    draw_tabs()

    flux.rect_filled(CONTENT_X, CONTENT_Y, CONTENT_W, CONTENT_H, theme.card, 8)
    flux.rect(CONTENT_X, CONTENT_Y, CONTENT_W, CONTENT_H, theme.card_border, 8, 1)
    flux.set_cursor_pos(CONTENT_X, CONTENT_Y)
    flux.begin_child("page", CONTENT_W, CONTENT_H, { id = "content" })
    flux.indent(16)
    flux.spacing(4)
    if state.page == "basics" then
        draw_basics()
    elseif state.page == "input" then
        draw_input()
    elseif state.page == "layout" then
        draw_layout()
    elseif state.page == "list" then
        draw_list()
    elseif state.page == "table" then
        draw_table()
    else
        draw_visual()
    end
    flux.end_child()

    flux.set_cursor_pos(CONTENT_X, FOOTER_Y)
    flux.label(("主题 %s · %s"):format(state.theme, state.action), 0, 12, CONTENT_W, false, 0)
    flux.text(CONTENT_X, HINT_Y,
        "Tab 切换焦点 · Enter/Space 激活 · 滚轮/滚动条滚动",
        theme.dim, 11)

    draw_toast()
    draw_menu()
    flux.assert_style_balanced()
end

local frames = 0
local stop_reason = "unknown"

while true do
    if not flux.pump() then
        stop_reason = "pump"
        break
    end
    if not flux.begin_frame(max_frames ~= nil) then
        if max_frames then
            stop_reason = "begin_frame"
            break
        end
        flux.sleep()
    else
        draw()
        if not flux.end_frame() then
            stop_reason = "end_frame: " .. flux.last_error()
            break
        end
        frames = frames + 1
        if max_frames and frames >= max_frames then
            stop_reason = "frame limit"
            break
        end
    end
end

print(("frames=%d stop=%s"):format(frames, stop_reason))
flux.shutdown()
