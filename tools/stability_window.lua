local flux = require "flux"

assert(flux.create {
    title = "flux.stability",
    width = 480,
    height = 320,
    titlebar_height = 30,
    backdrop = "none",
})
flux.show()

local frames = 0
while true do
    if not flux.pump() then
        break
    end
    if not flux.begin_frame(true) then
        break
    end
    frames = frames + 1

    flux.clear(0xFF202020)
    flux.rect_filled(0, 0, 480, 320, 0xFF202020)
    flux.rect_filled(20, 20, 440, 70, 0xFF3070C0, 10)
    flux.text(40, 40, "stability probe", 0xFFFFFFFF, 20)
    flux.rect_filled(20, 110, 440, 160, 0xFF2A2A2A, 10)
    flux.text(40, 130, "static content must not shimmer", 0xFFFFFFFF, 14)
    for index = 1, 6 do
        flux.text(40, 160 + index * 18, "row " .. index, 0xFFB4B4B4, 13)
    end

    -- Static modal dim must be drawn into the content surface, not a
    -- full-window translucent overlay layer.
    flux.rect_filled(0, 0, 480, 320, 0x40000000)

    flux.layer.use("overlay")
    flux.rect_filled(300, 50, 150, 50, 0xFF222222, 8)
    flux.text(316, 66, "overlay", 0xFFFFFFFF, 13)
    flux.layer.reset()

    flux.popup_open("稳定性菜单", { id = "menu" })
    if flux.begin_popup("稳定性菜单", 120, 180, 220, 110, { id = "menu" }) then
        flux.menu_item("静态菜单项", { id = "a", shortcut = "", checked = false })
        flux.menu_item("另一个菜单项", { id = "b", shortcut = "", checked = true })
        flux.menu_separator()
        flux.menu_item("关闭", { id = "c", shortcut = "Esc", checked = false })
        flux.end_popup()
    end

    if not flux.end_frame() then
        break
    end
end
flux.shutdown()
