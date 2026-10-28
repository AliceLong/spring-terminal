# 旧乘客观光座椅

素材为车厢内固定双人座椅：褪色橄榄绿软包、旧米白金属扶手及固定安装底座。替代已否决的公园木条长椅方向；本素材不覆盖或删除早先原画。

masters/passenger-seat.png为内置imagegen生成的透明原画。设备通过tools/export-passenger-seat.py编码为I4，Prop_passenger_seat使用(236,374)底部中心锚点，位于窗下、种植箱后。

座椅属于原有家具，不是第九项设施、不计建造成本、没有点击或解锁按钮。当前WorldJson.furniture.passenger_seat.robot_use_unlocked固定为false，明确保留第二章剧情锁；没有根据树成长、语言阶段、邀请休息或聊天自动解锁。01当前继续地面休息。第二章统一开发时，需要新增真实剧情许可及持久化状态，再接入坐姿和到达反馈，本轮没有冒充这些已实现。

scene-preview.png为实际LVGL原生渲染，显示旧乘客座椅与仍在地面休息的01。不是设备照片。

本地构建与原生渲染通过，尚未刷入：设备USB无响应，刷前备份连接失败，本轮没有写入设备。最终应用及状态见dist/passenger-seat-20261022/manifest.json。为满足当前应用分区，导出排除了未被程序引用、已由地区场景替代的旧Drought及旧Prop_bench；源文件保留，未更改正在使用的地区景色。
