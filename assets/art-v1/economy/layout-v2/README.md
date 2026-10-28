# 树、水池与双窝 · 本地布局第二版

2026-10-21，按用户提供的整体布局和储水池细节图修改。仅网页预览，不写固件、设备导出数据和玩家存档。

入口：../../economy-preview.html。默认完整美术布局展示设施和动物；关闭开关后按原21步核心回放的真实解锁状态显示，资源和历史始终来自原记录。大树可选三个阶段。幼树阶段如需展示已解锁双窝，使用支撑杆；成熟后依附树干。仍为寻雨雀与拾荒鼠，不新增兔子经济规则。

树位于右侧，末期树冠超出顶部和右边；鸟窝高枝、小动物窝贴树干；储水池改为石砌苔藓水池、竹管出水口、木桩与睡莲。左侧能源工具设施，中部园丁，前排种植箱。

生成方式：内置 image_gen，透明背景。母版tree-water-homes-master.png，四张裁切精灵tree.png、pool.png、bird-home.png、animal-home.png。参考输入为用户本轮两张图。图像分割与页面缩放仅用于素材接入。

最终生成提示词：

Use case: stylized-concept. Produce ONE transparent game sprite atlas, a strict 2x2 grid with four isolated assets, generous transparent gutters, no labels, no text, no background, no ground plane outside each asset. Match the supplied warm painterly moss-covered train-garden references, hand painted ochre wood, yellow-green leaves, readable silhouettes for 480px display. Upper left quadrant: a majestic mature tree, thick curved trunk on RIGHT, large leafy canopy extending LEFT and across the top; full tree from canopy to roots, no nests, no animals, no water. Upper right quadrant: low elliptical stone water reservoir exactly inspired by reference 2, blue water and two lily pads, pale worn stone rim with moss, short wooden corner posts, small bamboo spout over the rear-right rim pouring a short stream. No tree, no animals, no giant surrounding grass patch. Lower left quadrant: empty woven twig bird nest cradled on a short leafy branch, no bird. Lower right quadrant: empty cozy arched wooden tree-hollow house with dark oval opening, little wooden sill and moss, no rabbit/animal. Tree takes its quadrant fully; other assets have centered silhouettes. Parallel front three-quarter 2.5D game scene view, same camera for all four. Keep separate objects within quadrants, never touch quadrant boundaries. Actual transparent alpha. These are production sprites, not a scene mockup.

检查：economy-preview-test.cjs通过全部21步及完整布局三阶段，确认不改回放数据。通过本地Canvas渲染器导出review-stage-0/1/2.png逐张检查；这些是网页绘制函数的静态渲染，不是设备截图或自动浏览器验收。
