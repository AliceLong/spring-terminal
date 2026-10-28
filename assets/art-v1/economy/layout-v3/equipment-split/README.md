# 独立设备素材

内置 image_gen 以上一版设备架为参考生成，母版 master.png；四个接入素材为 rack-empty.png、solar.png、battery.png、bench.png，均保留透明通道。裁切只用于图集接入。

空架是固定场景摆放结构，不新增合并建筑。集能板绑定设施2、蓄能箱绑定设施3、工作台绑定设施5；每项独立图层与点击区域。移除旧版“全部建好后换整架大图”逻辑，完整布局同样使用独立设备组合。其余设施、数值、存档和设备固件不变。

验证：三种设备全部8种解锁组合，确保只绘制对应设备、始终保留空架、不使用旧rack.png；21步经营回放和三阶段绘制通过。静态截图由同一网页绘制函数渲染，非设备截图。

最终提示词：

Use case: precise-object-edit. Convert the reference equipment shelf into a PRODUCTION GAME SPRITE ATLAS of FOUR SEPARATE pieces in a strict 2x2 grid. Transparent background with large empty gutters and no text or labels. Match the same honey wood, olive green metal, moss ivy and small white flowers, warm hand-painted game style. Top LEFT cell: EMPTY tall freestanding wooden shelf structure ONLY, three horizontal empty shelves and two vertical supports, small moss vines on side posts. Remove every solar panel, battery box, lamp, tool, gear and crate. Empty transparent spaces between shelves. Top RIGHT cell: isolated tilted BLUE SOLAR PANEL with wooden frame and short mounting bracket, same design as reference, no shelf, no barrel. Bottom LEFT cell: isolated GREEN METAL BATTERY BOX with yellow lightning symbol, same design, no shelf or lamp. Bottom RIGHT cell: isolated WORKBENCH TOOL KIT with red-handled pliers, two gears and a short low wooden worktop, no tall shelving or battery. Each object fully contained in its own quadrant with clear transparent space around it; objects do not touch or overlap cell boundaries. Do NOT include the rain barrel. Actual alpha transparency, no gradient or glow backdrop. All four pieces should be separately usable sprites so no piece contains any other device.
