# 素材生成记录

使用内置 image_gen，参考用户本轮构图及原设备背景。背景为不透明；设备架/集水桶图集、种植箱为透明素材。旧版树、水池和双窝沿用 layout-v2 的独立素材。生成后裁切到本目录，未运行设备导出器。

## 最终车厢背景提示词

Use case: precise-object-edit. Generate an EMPTY layered game background plate with unmistakable OLD RAILWAY CARRIAGE INTERIOR, NOT a wooden cottage, conservatory or building. Use reference1 only for warm painterly moss-garden palette and spacious arrangement; reference2 for railroad interior architecture. Square frontal side cutaway camera inside a train carriage, no vanishing-point corridor. Olive green riveted METAL arched carriage ceiling, curved structural ribs and exposed cables, 3 triangular train hand straps and 3 pendant lamps. A row of 3 large broad rectangular TRAIN WINDOWS with rounded corners, separated by slim riveted metal pillars; continuous metal lower window sill, visible railway carriage wall and small edge of train door at far right. Outside all windows lush green forest, distant stone railway aqueduct and blue sky. Window glazing spans y19% to56%. Bottom40% is flat worn pale ochre metal carriage floor with visible rectangular iron panel seams and occasional screw heads, soft creeping moss and tiny flowers around edges, open unobstructed central floor. NO tall opaque wall across lower half. NO wood cabin ceiling, no brick, no stone walls. REMOVE all movable foreground objects: no tree, no nests, no animals, no robot, no boxes, no plants in planters, no shelf, no solar panel, no barrels, no pool. No UI, no text, no icons, no border. Full bleed square. Match warm hand-painted illustrated game art and sunlight in reference1 while unmistakably train architecture.

## 木种植箱提示词

Use case: stylized-concept. Transparent isolated production game sprite: ONE large rustic rectangular WOODEN PLANTING BOX with dark rich soil, no plants. Match attached warm painterly garden train reference exactly: honey ochre weathered planks, thick timber rim, dark metal corner brackets, a little green moss and tiny white flowers at base. Frontal slightly elevated three-quarter view, see soil surface and deep wooden front face. Wide box approximately 2.3 times as wide as tall, whole object visible, fills canvas width with generous transparent padding. Strong readable silhouette, handpainted shaded wood, no black outlines, no text, no UI, no background, no surrounding floor, no duplicate boxes. Actual transparent alpha.

## 设备架图集透明处理提示词

Use case: background-extraction. Edit this exact two-object sprite atlas. Preserve every piece of the wooden equipment shelf and separate funnel barrel, their geometry, colors, painted details and spacing. Remove ALL the dark/green/gold gradient backdrop and ALL atmospheric glow/halo, including in gaps between shelf legs and between the two objects. Deliver genuine fully transparent alpha outside the physical object silhouettes. No cast floor, no added object, no style change. Opaque object interiors, crisp antialiased transparent edges.

设备架初始生成要求：单张双物件透明图集，左侧为参考图同款大尺寸木设备架（倾斜蓝色太阳能板、绿色蓄能箱、工具层、底部箱子、常春藤白花），右侧为独立大木桶与黄铜集水漏斗和侧管；统一暖色手绘风格，无角色、地面、文字和UI。
