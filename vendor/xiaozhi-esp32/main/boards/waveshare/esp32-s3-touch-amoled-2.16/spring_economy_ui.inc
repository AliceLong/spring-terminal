// Board-owned UI for economy decisions. No cost is spent by browsing a card.
void EconomyModal(unsigned kind) {
    Modal(kind);
    economy_page = 0;
    // Keep the immutable snapshot alive across nested modal calls, off the LVGL stack.
    const auto snapshot =
        std::unique_ptr<spring::Snapshot>(new spring::Snapshot(GetWorldSnapshot()));
    const auto& s = *snapshot;
    if (kind == 8)
        proposal_token = s.economy.proposal.token;
    for (auto* b : choices)
        lv_obj_remove_flag(b, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(primary, LV_OBJ_FLAG_HIDDEN);
}
void Offer(unsigned kind, unsigned item = 0, unsigned value = 0) {
    Send(Action::Propose, kind, item, value);
}
void EconomyChoice(unsigned action) {
    // Keep the immutable snapshot alive across nested modal calls, off the LVGL stack.
    const auto snapshot =
        std::unique_ptr<spring::Snapshot>(new spring::Snapshot(GetWorldSnapshot()));
    const auto& s = *snapshot;
    if (modal_kind == 7) {
        if (action == 0)
            EconomyModal(12);
        else if (action == 1)
            StoryModal(21);
        else
            EconomyModal(9);
        return;
    }
    if (modal_kind == 15) {
        if (action == 0)
            EconomyModal(7);
        return;
    }
    if (modal_kind == 16) {
        EconomyModal(action == 0 ? 11 : action == 1 ? 13 : 9);
        return;
    }
    if (modal_kind == 8) {
        if (action == 0) {
            Send(Action::DismissProposal, proposal_token);
            HideModal(nullptr);
        } else if (action == 1)
            EconomyModal(7);
        else
            Send(Action::Mute);
        return;
    }
    if (modal_kind == 9) {
        if (action < 2) {
            if (s.economy.journey)
                Offer(5, s.economy.journey_id, action == 0 ? 1 : 0);
        } else
            EconomyModal(7);
        return;
    }
    if (action == 2) {
        if (modal_kind == 11) {
            StoryModal(25);
            return;
        }
        if (modal_kind == 10 && economy_page == 5) {
            Offer(6);
            return;
        }
        if (modal_kind == 10 && economy_page == 6) {
            EconomyModal(14);
            return;
        }
        EconomyModal(7);
        return;
    }
    const unsigned count = modal_kind == 10   ? 9
                           : modal_kind == 11 ? 6
                           : modal_kind == 12 ? 4
                           : modal_kind == 13 ? 2
                                              : 5;
    economy_page = (economy_page + (action ? 1 : count - 1)) % count;
}
void EconomyConfirm() {
    // Keep the immutable snapshot alive across nested modal calls, off the LVGL stack.
    const auto snapshot =
        std::unique_ptr<spring::Snapshot>(new spring::Snapshot(GetWorldSnapshot()));
    const auto& s = *snapshot;
    switch (modal_kind) {
        case 7:
            if (s.economy.proposal.kind)
                EconomyModal(8);
            break;
        case 8:
            if (Send(Action::ConfirmProposal, proposal_token))
                HideModal(nullptr);
            break;
        case 9:
            if (s.economy.journey) {
                selected_call = 4;
                if (Send(Action::Accept, 4))
                    HideModal(nullptr);
            } else if (s.demo) {
                if (Send(Action::DemoJourney))
                    HideModal(nullptr);
            } else
                EconomyModal(7);
            break;
        case 10:
            if (economy_page == 8) {
                Modal(2);
                lv_obj_remove_flag(primary, LV_OBJ_FLAG_HIDDEN);
            } else
                Offer(1, economy_page);
            break;
        case 15:
            if (economy_page == 5)
                Offer(6);
            else if (economy_page == 6)
                EconomyModal(14);
            else if (economy_page == 7)
                EconomyModal(13);
            break;
        case 11:
            StoryModal(25);
            break;
        case 12:
            Offer(3, economy_page);
            break;
        case 13:
            if (s.economy.journey >= 4)
                EconomyModal(9);
            else
                EconomyModal(7);
            break;
        case 14:
            Offer(4, economy_page == 4 ? 255 : economy_page);
            break;
        case 16:
            EconomyModal(7);
            break;
    }
}
void EconomyButtons(const char* a, const char* b, const char* c, const char* main) {
    const char* labels[] = {a, b, c};
    for (unsigned i = 0; i < 3; ++i)
        spring_ui::TextChanged(choice_labels[i], labels[i]);
    spring_ui::TextChanged(lv_obj_get_child(primary, 0), main);
}
void EconomyPortrait(spring_art::Id id) {
    const unsigned value = static_cast<unsigned>(id);
    const unsigned first = static_cast<unsigned>(spring_art::Id::Facility_tank);
    if (value >= first && value < first + 8)
        id = static_cast<spring_art::Id>(static_cast<unsigned>(spring_art::Id::Thumb_tank) + value -
                                         first);
    const auto* source = spring_art::Get(id).image;
    if (lv_image_get_src(economy_icon) != source)
        lv_image_set_src(economy_icon, source);
    if (lv_obj_has_flag(economy_icon, LV_OBJ_FLAG_HIDDEN))
        lv_obj_remove_flag(economy_icon, LV_OBJ_FLAG_HIDDEN);
}
void RenderEconomyModal(const spring::Snapshot& s) {
    const auto& e = s.economy;
    spring::Core c;
    static_cast<spring::State&>(c.s) = s;
    c.storage_ok = s.storage_ok;
    // Re-evaluate eligibility per page; disabled state must not leak from a built item.
    lv_obj_remove_state(primary, LV_STATE_DISABLED);
    spring_ui::TextChanged(modal_hint, "语音提出意向；确认卡批准后才执行");
    switch (modal_kind) {
        case 7:
            spring_ui::TextChanged(modal_title, "当前计划");
            spring_ui::TextFormat(
                modal_body, "目标：%s\n待续：%s\n正在：%s\n地区：%s\n待批方案：%s",
                spring::kGoals[e.priorities[0]],
                spring::StoryCurrentNode(s.story) < spring::kStoryCount
                    ? spring::kStorySpecs[spring::StoryCurrentNode(s.story)].title
                    : "暂无",
                e.emergency_work     ? "应急取水"
                : e.construction < 8 ? spring::kFacilities[e.construction].name
                : s.tree_work_ms     ? "培育大树"
                : e.recycle_ms       ? "拆解回收物"
                : e.journey_work && e.journey_task < spring::kJourneyTaskNames.size()
                    ? spring::kJourneyTaskNames[e.journey_task]
                    : "日常照料",
                spring::kRegions[e.region], e.proposal.kind ? "有，点击查看" : "暂无");
            if (e.journey_work && e.journey_task > 0 &&
                e.journey_task < spring::kJourneyTaskNames.size())
                spring_ui::TextFormat(
                    modal_hint, "%s；约%u秒\n完成后：%s",
                    !s.energy || e.emergency_work ? "任务暂缓，进度保留" : "正在处理",
                    (e.journey_work + 999) / 1000, spring::kJourneyTaskResults[e.journey_task]);
            else
                spring_ui::TextChanged(modal_hint, e.emergency_work ? "取水中；原工作进度保留"
                                                   : !s.energy ? "断电暂停；可批准停产槽改种轨光花"
                                                   : c.MissingParts() ? "缺零件：探索后点工作台拆解"
                                                                      : "方向不会自动花费建设材料");
            EconomyButtons("调整目标", "旅途安排", "待办事件", "查看方案");
            if (!e.proposal.kind)
                lv_obj_add_flag(primary, LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_remove_flag(primary, LV_OBJ_FLAG_HIDDEN);
            break;
        case 8: {
            const auto& p = e.proposal;
            spring_ui::TextChanged(modal_title, "请确认这项安排");
            if (!p.kind || p.token != proposal_token) {
                spring_ui::TextChanged(modal_body,
                                       "该方案已更新或处理。\n请返回计划查看最新方案。");
                lv_obj_add_state(primary, LV_STATE_DISABLED);
            } else {
                switch (static_cast<spring::ProposalKind>(p.kind)) {
                    case spring::ProposalKind::Build: {
                        const auto& b = spring::kFacilities[p.target];
                        spring_ui::TextFormat(
                            modal_body,
                            "建造：%s\n投入：水%u 能源%u 藤%u "
                            "零件%u\n施工：%u秒，园丁自行安排\n效果：%s\n运行：%s",
                            b.name, b.water, b.energy, b.fiber, b.parts, b.ms / 1000, b.benefit,
                            b.upkeep);
                        EconomyPortrait(static_cast<spring_art::Id>(
                            static_cast<unsigned>(spring_art::Id::Facility_tank) + p.target));
                        break;
                    }
                    case spring::ProposalKind::Plant:
                        if (c.CanRecoveryPlant(p.target, p.value)) {
                            spring_ui::TextFormat(
                                modal_body,
                                "%u号位停产恢复\n批准后立即改种%s\n该槽尚未支付本轮投入\n%"
                                "s\n10分钟成熟，原工程进度保留",
                                p.target + 1, spring::kPlantNames[p.value],
                                p.value == 0 ? "启动需要水2；可低功耗照料"
                                             : "启动需要能源1，恢复供水");
                            break;
                        }
                        spring_ui::TextFormat(
                            modal_body,
                            "%u号位下轮改为：%"
                            "s\n当前植物继续生长，不重置\n每轮10分钟；开始时扣投入\n苔：能源1 → "
                            "水6\n花：水2 → 能源5\n藤3／豆2，产量随地区变化",
                            p.target + 1, spring::kPlantNames[p.value]);
                        break;
                    case spring::ProposalKind::Goal:
                        spring_ui::TextFormat(modal_body,
                                              "首要目标：%"
                                              "s\n其余目标顺延保留\n不自动改种或扣建设材料\n优先安"
                                              "排相关照料工作\n仍保留供水、供能和动物照料",
                                              spring::kGoals[p.target]);
                        break;
                    case spring::ProposalKind::Boost:
                        spring_ui::TextFormat(modal_body,
                                              "照料装置：%s\n目标种植位：%u\n每轮额外能源1、产物+"
                                              "1\n只在新一轮开始时生效\n关闭不额外扣费",
                                              p.target == 255 ? "关闭" : "启用",
                                              p.target == 255 ? 0 : p.target + 1);
                        break;
                    case spring::ProposalKind::Salvage:
                        spring_ui::TextFormat(modal_body,
                                              "拆解待回收物1件\n方式：%s\n额外能源%u，工作%"
                                              "u秒\n完成零件+%u\n现有待回收物：%u",
                                              c.HasFacility(5) ? "工作台加工" : "手动拆解",
                                              c.HasFacility(5) ? 2 : 0, c.HasFacility(5) ? 60 : 180,
                                              c.HasFacility(5) ? 2 : 1, e.scrap);
                        break;
                    case spring::ProposalKind::Tree:
                        spring_ui::TextFormat(
                            modal_body,
                            "继续培育大树\n投入：水%u、藤纤维%u\n工作%"
                            "u秒；断电会暂停\n成树：休息与鸟来访\n繁茂树冠：解锁住所",
                            spring::TreeWater(s.tree_stage), spring::TreeFiber(s.tree_stage),
                            spring::TreeDuration(s.tree_stage) / 1000);
                        break;
                    case spring::ProposalKind::Journey:
                        spring_ui::TextFormat(
                            modal_body,
                            "旅途：%s\n决定：%s\n%s\n完成后写入旅途日志\n园丁会把结果告诉你",
                            spring::kJourneys[e.journey],
                            p.value ? "执行探索/救助" : "略过，不扣资源",
                            !p.value         ? "此事件结束，不获得奖励"
                            : e.journey == 1 ? "能源2；探索120秒得回收物1，需另行拆解"
                            : e.journey == 2 ? "探索60秒，水4"
                            : e.journey == 3 ? "需集雨器；60秒收集水6"
                            : e.journey == 4 ? "需小窝、食物2水2；救助60秒"
                            : e.journey == 5 ? "需小窝与已救助小鸟；食物4水2；60秒"
                                             : "探索60秒，食物2");
                        break;
                    default:
                        break;
                }
                if (c.CanConfirm(proposal_token))
                    lv_obj_remove_state(primary, LV_STATE_DISABLED);
                else
                    lv_obj_add_state(primary, LV_STATE_DISABLED);
            }
            spring_ui::TextFormat(modal_hint, "现有 水%u 能源%u 藤%u 食物%u 零件%u\n%s", s.water,
                                  s.energy, s.fiber, e.food, e.parts,
                                  c.CanConfirm(proposal_token) ? "可以确认；投入只扣一次"
                                                               : "资源不足、任务忙或条件未满足");
            EconomyButtons("暂不执行", "计划", "提示音",
                           e.proposal.kind == 1 ? "确认建造" : "确认执行");
            // Keep the decision card focused: approve, decline or the existing back button.
            lv_obj_add_flag(choices[1], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(choices[2], LV_OBJ_FLAG_HIDDEN);
            if (e.proposal.kind == 1 && e.proposal.token == proposal_token) {
                const auto& b = spring::kFacilities[e.proposal.target];
                const char* reason =
                    !s.storage_ok                                ? "存档异常，暂不能建造"
                    : c.HasFacility(e.proposal.target)           ? "已经建好，无需重复建造"
                    : e.proposal.target == 7 && s.tree_stage < 2 ? "先培育繁茂树冠，才能建动物住所"
                    : c.BusyEconomy() || s.tree_work_ms          ? "园丁正在工作，完成后可建造"
                    : s.water < b.water                          ? "水不足，请先收集露珠"
                    : s.energy < b.energy                        ? "能源不足，请先收获轨光花"
                    : s.fiber < b.fiber                          ? "藤纤维不足，请先收获缝甲藤"
                    : e.parts < b.parts                          ? "零件不足，请先探索或拆解"
                                                                 : "点击“确认建造”才扣材料并开工";
                spring_ui::TextFormat(modal_hint, "现有 水%u 能源%u 藤%u 零件%u\n%s", s.water,
                                      s.energy, s.fiber, e.parts, reason);
            }
            break;
        }
        case 9:
            spring_ui::TextChanged(modal_title, "园丁 · 旅途发现");
            spring_ui::TextFormat(
                modal_body, "%s\n%s\n%s", spring::kJourneys[e.journey],
                spring::kJourneyOpenings[e.journey],
                e.journey ? "接听可以语音讨论；方案另行确认" : "下一次旅途发现时再联系");
            if ((e.journey == 4 || e.journey == 5))
                EconomyPortrait(e.journey == 4 ? spring_art::Id::Animal_bird
                                               : spring_art::Id::Animal_mouse);
            EconomyButtons("查看方案", "略过方案", "计划",
                           e.journey ? "接听聊聊"
                           : s.demo  ? "演示来讯"
                                     : "返回计划");
            if (!e.journey && !s.demo)
                lv_obj_add_state(primary, LV_STATE_DISABLED);
            break;
        case 10: {
            if (economy_page == 8) {
                lv_obj_add_flag(economy_icon, LV_OBJ_FLAG_HIDDEN);
                spring_ui::TextChanged(modal_title, "建设 9/9 · 大树升级");
                spring_ui::TextFormat(modal_body,
                                      "阶段：%s\n下一步：%s\n需要 水%u 藤%u\n持有 水%lu 藤%lu",
                                      s.tree_stage >= 2 ? "繁茂树冠"
                                      : s.tree_stage    ? "成树"
                                                        : "幼树",
                                      s.tree_work_ms      ? "培育中，进度已保存"
                                      : s.tree_stage >= 2 ? "已完成全部升级"
                                      : s.tree_stage      ? "繁茂树冠，解锁动物住所"
                                                          : "成树，提供休息与鸟来访",
                                      s.tree_stage >= 2 ? 0 : spring::TreeWater(s.tree_stage),
                                      s.tree_stage >= 2 ? 0 : spring::TreeFiber(s.tree_stage),
                                      (unsigned long)s.water, (unsigned long)s.fiber);
                EconomyButtons("上一项", "下一项", "当前计划", "查看大树");
                break;
            }
            const auto& b = spring::kFacilities[economy_page];
            spring_ui::TextFormat(modal_title, "建设 %u/9 · %s", economy_page + 1, b.name);
            char status[64];
            if (c.HasFacility(economy_page))
                snprintf(status, sizeof(status), "已建成");
            else if (economy_page == 7 && s.tree_stage < 2)
                snprintf(status, sizeof(status), "需先培育繁茂树冠");
            else if (e.construction == economy_page)
                snprintf(
                    status, sizeof(status),
                    !s.energy || e.emergency_work ? "已暂停；剩余约%u秒" : "施工中，还需约%u秒",
                    static_cast<unsigned>((b.ms - e.construction_ms + 999) / 1000));
            else if (s.water < b.water || s.energy < b.energy || s.fiber < b.fiber ||
                     e.parts < b.parts)
                snprintf(status, sizeof(status), "缺材料");
            else
                snprintf(status, sizeof(status), "可建造，等待确认");
            spring_ui::TextFormat(
                modal_body,
                "状态：%s\n投入 水%u 能源%u 藤%u 零件%u\n施工 %u 秒\n收益：%s\n维护：%s", status,
                b.water, b.energy, b.fiber, b.parts, b.ms / 1000, b.benefit, b.upkeep);
            spring_ui::TextFormat(modal_hint, "持有 水%lu 能源%lu 藤%lu 零件%u",
                                  (unsigned long)s.water, (unsigned long)s.energy,
                                  (unsigned long)s.fiber, e.parts);
            EconomyPortrait(static_cast<spring_art::Id>(
                static_cast<unsigned>(spring_art::Id::Facility_tank) + economy_page));
            EconomyButtons("上一项", "下一项",
                           economy_page == 5   ? "拆解"
                           : economy_page == 6 ? "照料设置"
                                               : "计划",
                           "查看建设");
            if (c.HasFacility(economy_page) || e.construction == economy_page)
                lv_obj_add_state(primary, LV_STATE_DISABLED);
            break;
        }
        case 11: {
            spring_ui::TextChanged(modal_title, "收藏 · 植物与动物图鉴");
            if (economy_page < 4) {
                unsigned i = economy_page;
                const bool known = e.discovered_plants & (1u << i);
                spring_ui::TextFormat(
                    modal_body,
                    "植物：%s\n观察：%s\n每轮10分钟\n%s\n操作：园丁自动照料，成熟后可收获",
                    spring::kPlantNames[i], known ? "已发现" : "尚未种植",
                    i == 0   ? "水2 → 能源5"
                    : i == 1 ? "能源1 → 水6"
                    : i == 2 ? "水2能源1 → 藤纤维3"
                             : "水2能源1 → 食物2");
                if (known)
                    EconomyPortrait(spring_art::PlantId(static_cast<spring::Plant>(i), 0));
                else
                    lv_obj_add_flag(economy_icon, LV_OBJ_FLAG_HIDDEN);
            } else {
                unsigned i = economy_page - 4;
                spring_ui::TextFormat(
                    modal_body, "动物：%s\n观察：%s\n%s\n%s", i ? "拾荒鼠" : "寻雨雀",
                    e.discovered_animals & (1u << i) ? "已遇见" : "尚未遇见",
                    i ? "每30分钟食物3、水1" : "每30分钟食物2、水1",
                    i ? "吃饱后，每2次探索多发现回收物" : "吃饱且有集雨器时，额外水3");
                if (e.discovered_animals & (1u << i))
                    EconomyPortrait(i ? spring_art::Id::Animal_mouse : spring_art::Id::Animal_bird);
                else
                    lv_obj_add_flag(economy_icon, LV_OBJ_FLAG_HIDDEN);
            }
            EconomyButtons("上一项", "下一项", "收藏", "返回收藏");
            break;
        }
        case 12:
            spring_ui::TextChanged(modal_title, "计划 · 目标优先级");
            spring_ui::TextFormat(modal_body, "当前顺序：\n1 %s\n2 %s\n3 %s\n4 %s\n拟设首要：%s",
                                  spring::kGoals[e.priorities[0]], spring::kGoals[e.priorities[1]],
                                  spring::kGoals[e.priorities[2]], spring::kGoals[e.priorities[3]],
                                  spring::kGoals[economy_page]);
            EconomyButtons("上个目标", "下个目标", "计划", "提出调整");
            break;
        case 13: {
            const unsigned i = economy_page;
            spring_ui::TextChanged(modal_title, i ? "动物 · 拾荒鼠" : "动物 · 寻雨雀");
            spring_ui::TextFormat(modal_body,
                                  "状态：%s\n小窝：%s\n%s\n帮助：%s\n缺粮暂停帮助，不死亡",
                                  e.animals & (1u << i) ? "已入住" : "尚未入住",
                                  c.HasFacility(7) ? "已建成" : "需先建设",
                                  i ? "每30分钟食物3、水1" : "每30分钟食物2、水1",
                                  e.animal_help & (1u << i) ? "正在提供" : "等待照料周期");
            EconomyPortrait(i ? spring_art::Id::Animal_mouse : spring_art::Id::Animal_bird);
            EconomyButtons("上一只", "下一只", "计划",
                           (e.journey == 4 || e.journey == 5) ? "查看求助" : "返回计划");
            break;
        }
        case 14:
            spring_ui::TextChanged(modal_title, "照料装置 · 指定目标");
            spring_ui::TextFormat(modal_body,
                                  "拟选择：%s\n种植位：%u\n每轮额外能源1，主产物+"
                                  "1\n只支持苔、藤、豆\n关闭后不再额外扣费",
                                  economy_page == 4 ? "关闭装置" : "加强照料",
                                  economy_page == 4 ? 0 : economy_page + 1);
            EconomyButtons("上一项", "下一项", "计划", "提出设置");
            break;
        case 15: {
            const unsigned i = economy_page;
            const auto& b = spring::kFacilities[i];
            spring_ui::TextChanged(modal_title, b.name);
            spring_ui::TextFormat(modal_body, "状态：已建成\n收益：%s\n维护：%s", b.benefit,
                                  b.upkeep);
            if (i == 0)
                spring_ui::TextFormat(modal_hint, "当前水量：%lu", (unsigned long)s.water);
            else if (i == 2 || i == 3)
                spring_ui::TextFormat(modal_hint, "当前能源：%lu", (unsigned long)s.energy);
            else
                spring_ui::TextChanged(modal_hint, "设施随车厢生活自动运行");
            EconomyPortrait(static_cast<spring_art::Id>(
                static_cast<unsigned>(spring_art::Id::Facility_tank) + i));
            EconomyButtons("当前计划", "", "",
                           i == 5   ? "拆解方案"
                           : i == 6 ? "照料设置"
                                    : "动物状态");
            lv_obj_add_flag(choices[1], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(choices[2], LV_OBJ_FLAG_HIDDEN);
            if (i == 5) {
                spring_ui::TextFormat(
                    modal_body, "待回收物：%u件\n每件消耗能源2，工作60秒\n完成获得零件2\n状态：%s",
                    e.scrap,
                    e.recycle_ms                        ? "正在拆解"
                    : !e.scrap                          ? "暂无待回收物"
                    : c.BusyEconomy() || s.tree_work_ms ? "园丁忙碌，稍后可继续"
                                                        : "可提出拆解方案");
                spring_ui::TextFormat(modal_hint, "持有能源：%lu；确认方案后才投入",
                                      (unsigned long)s.energy);
                if (!e.scrap || e.recycle_ms || c.BusyEconomy() || s.tree_work_ms || s.energy < 2)
                    lv_obj_add_state(primary, LV_STATE_DISABLED);
            } else if (i != 6 && i != 7)
                lv_obj_add_flag(primary, LV_OBJ_FLAG_HIDDEN);
            break;
        }
        case 16:
            spring_ui::TextChanged(modal_title, "车厢记录仪 · 更多");
            spring_ui::TextChanged(
                modal_body,
                "图鉴：认识植物、设施和动物\n动物：入住、食物和帮助状态\n旅途：查看当前待决定的事件"
                "\n暂时不决定，也可以回来查看\n未批准的方案不会开工");
            EconomyButtons("图鉴", "动物", "旅途", "返回计划");
            break;
    }
}
void RenderDecisionLog(const spring::Snapshot& s, unsigned page) {
    const auto& e = s.economy;
    const auto& d = e.decisions[e.decision_count - 1 - page % e.decision_count];
    static const char* titles[] = {"园丁苏醒",     "确认下轮种植", "植物成熟",     "收获完成",
                                   "完成灌溉",     "开始培育大树", "大树成长",     "旅途提醒",
                                   "接听通讯",     "开始建设",     "建设完成",     "发现旅途事件",
                                   "决定旅途方案", "调整计划",     "提出待批方案", "完成动物照料",
                                   "拆解完成",     "资源不足",     "旅途任务完成", "进入新地区",
                                   "小鸟来访",     "开始应急收集", "应急取水完成"};
    const char* object = "车厢生活";
    if ((d.type == spring::Event::FacilityStarted || d.type == spring::Event::FacilityBuilt) &&
        d.target < 8)
        object = spring::kFacilities[d.target].name;
    if ((d.type == spring::Event::JourneyFound || d.type == spring::Event::JourneyResolved ||
         d.type == spring::Event::JourneyCompleted) &&
        d.target < 7)
        object = spring::kJourneys[d.target];
    if (d.type == spring::Event::Planted && d.amount < 4)
        object = spring::kPlantNames[d.amount];
    if (d.type == spring::Event::PlanChanged && d.target < 4)
        object = spring::kGoals[d.target];
    if (d.type == spring::Event::PlanChanged && d.target == 255)
        object = "取消未执行方案";
    if (d.type == spring::Event::ProposalCreated)
        object = "等待你批准，尚未扣费";
    const char* result = d.type == spring::Event::JourneyResolved
                             ? (d.amount ? "已批准，园丁执行中" : "已略过，没有扣费")
                         : d.type == spring::Event::FacilityStarted ? "材料已扣，施工进度保存"
                         : d.type == spring::Event::Planted         ? "当前轮不变，下轮生效"
                                                                    : "记录已保存";
    spring_ui::TextChanged(modal_title, "记录仪 · 决策与旅途日志");
    spring_ui::TextFormat(modal_body, "第 %u / %u 条\n%s\n%s\n%s\n游戏时间：%u分钟",
                          page % e.decision_count + 1, e.decision_count,
                          titles[static_cast<unsigned>(d.type)], object, result, d.second / 60);
}
