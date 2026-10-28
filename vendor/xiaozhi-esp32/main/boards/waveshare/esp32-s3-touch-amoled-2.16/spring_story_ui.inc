#include "spring_feature_gates.h"
// Reuse the stable modal/shield/tap guard. Story never auto-opens an archive.
unsigned story_seen_doc = 255;
unsigned story_doc = 0, story_doc_page = 0, story_selected = 255, story_record_page = 0;
bool StorySend(spring::StoryVerb verb, unsigned target = 255, unsigned value = 0,
               unsigned token = 0, const char* text = "") {
    return QueueStoryAction(static_cast<unsigned>(verb), target, value, token, text, false);
}
void StoryModal(unsigned kind) {
    Modal(kind);
    for (auto* b : choices)
        lv_obj_remove_flag(b, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(primary, LV_OBJ_FLAG_HIDDEN);
    story_doc_page = 0;
}
unsigned PickStoryNode(const spring::Snapshot& s) { return spring::StoryCurrentNode(s.story); }
void StorySummary() {
    // Keep the immutable snapshot alive across nested modal calls, off the LVGL stack.
    const auto snapshot =
        std::unique_ptr<spring::Snapshot>(new spring::Snapshot(GetWorldSnapshot()));
    const auto& s = *snapshot;
    story_selected = PickStoryNode(s);
    if (story_selected < spring::kStoryCount)
        StorySend(spring::StoryVerb::Inspect, story_selected);
    StoryModal(24);
}
void StoryChoice(unsigned action) {
    // Keep the immutable snapshot alive across nested modal calls, off the LVGL stack.
    const auto snapshot =
        std::unique_ptr<spring::Snapshot>(new spring::Snapshot(GetWorldSnapshot()));
    const auto& s = *snapshot;
    const auto& n = s.story;
    if (modal_kind == 28) {
        if (action == 0 && s.demo &&
            Application::GetInstance().GetDeviceState() == kDeviceStateIdle)
            Send(Action::Mode);
        else if (action == 2)
            StoryModal(17);
        return;
    }
    if (modal_kind == 27 && communication_source == CommunicationSource::Life &&
        active_call.load() == 98) {
        uint32_t token = 0;
        {
            std::lock_guard<std::mutex> guard(social_mutex);
            if (spring::WishPending(agency_public))
                token = agency_public.current.token;
        }
        if (token && action < 3)
            QueueWishResponse(token, action + 1, false);
        return;
    }
    if (modal_kind == 17) {
        if (action < 2)
            EconomyModal(action == 0 ? 10 : 7);
        else
            StoryModal(25);
        return;
    }
    if (modal_kind == 25) {
        if (action == 0)
            EconomyModal(11);
        else
            StoryModal(action == 1 ? 18 : 26);
        return;
    }
    if (modal_kind == 26) {
        if (action < 2)
            story_record_page = (story_record_page + 1) % 2;
        else
            StoryModal(25);
        return;
    }
    if (modal_kind == 18) {
        if (action == 2) {
            if (n.archives & (1u << story_doc)) {
                StorySend(spring::StoryVerb::Archive, story_doc, 1);
            }
            return;
        }
        for (unsigned i = 0; i < 7; ++i) {
            story_doc = (story_doc + (action == 0 ? 6 : 1)) % 7;
            if (n.archives & (1u << story_doc))
                break;
        }
        return;
    }
    if (modal_kind == 19) {
        if (action < 2) {
            lv_obj_scroll_by(archive_scroll, 0, action ? -90 : 90, LV_ANIM_OFF);
            return;
        }
        communication_source = CommunicationSource::Story;
        communication_archive.store(story_doc);
        active_call.store(99);
        Application::GetInstance().Schedule([] {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateIdle)
                app.InvokeDeviceEvent("打开档案");
        });
        return;
    }
    if (modal_kind == 20) {
        if (n.count)
            story_record_page = (story_record_page + (action == 0 ? n.count - 1 : 1)) % n.count;
        return;
    }
    if (modal_kind == 21) {
        if (action == 0)
            StoryModal(22);
        else if (action == 1) {
            StorySend(spring::StoryVerb::OfferFlower);
            StoryModal(23);
        } else
            StorySummary();
        return;
    }
    if (modal_kind == 22) {
        if (action == 0 && n.candidate_who)
            StorySend(spring::StoryVerb::CancelName, 255, 0, n.candidate_token);
        else if (action == 2)
            StorySummary();
        else if (action == 1)
            HideModal(nullptr);
        return;
    }
    if (modal_kind == 23) {
        if (n.available & (1u << 11)) {
            StorySend(spring::StoryVerb::Complete, 11, action == 0 ? 2 : 3);
        }
        HideModal(nullptr);
        return;
    }
    if (modal_kind == 24 && story_selected < spring::kStoryCount) {
        if (action == 0) {
            StorySend(spring::StoryVerb::Accept, story_selected);
            HideModal(nullptr);
        } else if (action == 1) {
            if (story_selected == 10 || story_selected == 11)
                StorySend(spring::StoryVerb::Complete, story_selected, 2);
            else
                StorySend(spring::StoryVerb::Pause, story_selected);
            HideModal(nullptr);
        } else if (story_selected == 10 || story_selected == 11) {
            StorySend(spring::StoryVerb::Complete, story_selected, 3);
            HideModal(nullptr);
        } else if (spring::kStorySpecs[story_selected].archive < 7) {
            story_doc = spring::kStorySpecs[story_selected].archive;
            StorySend(spring::StoryVerb::Archive, story_doc, 1);
        }
    }
}
void StoryConfirm() {
    // Keep the immutable snapshot alive across nested modal calls, off the LVGL stack.
    const auto snapshot =
        std::unique_ptr<spring::Snapshot>(new spring::Snapshot(GetWorldSnapshot()));
    const auto& s = *snapshot;
    if (modal_kind == 17 && spring_ui::kCollectionPracticeEnabled) {
        StoryModal(28);
        return;
    }
    if (modal_kind == 28) {
        if (!spring_ui::kCollectionPracticeEnabled)
            return;
        if (s.demo)
            StartCollectionPractice(s);
        else if (Application::GetInstance().GetDeviceState() == kDeviceStateIdle)
            Send(Action::Mode);
        return;
    }
    if (modal_kind == 25) {
        StoryModal(17);
        return;
    }
    if (modal_kind == 26) {
        StoryModal(25);
        return;
    }
    if (modal_kind == 18) {
        StoryChoice(2);
        return;
    }
    if (modal_kind == 19) {
        StoryModal(18);
        return;
    }
    if (modal_kind == 20 || modal_kind == 21) {
        EconomyModal(7);
        return;
    }
    if (modal_kind == 22) {
        if (s.story.candidate_who)
            StorySend(spring::StoryVerb::ConfirmName, 255, 0, s.story.candidate_token);
        return;
    }
    if (modal_kind == 23) {
        StorySend(spring::StoryVerb::ApproveFlower, 255, 0, s.story.offer_token);
        return;
    }
    if (modal_kind == 24 && story_selected < spring::kStoryCount) {
        if (story_selected == 6 && spring::ChapterHas(s.chapters, spring::BeyondWorkObserved) &&
            !spring::ChapterHas(s.chapters, spring::BenchRestCompleted)) {
            StorySend(spring::StoryVerb::InviteSeat);
            HideModal(nullptr);
        } else if (story_selected == 11) {
            StorySend(spring::StoryVerb::OfferFlower);
            StoryModal(23);
        } else {
            StorySend(spring::StoryVerb::Complete, story_selected, story_selected == 10 ? 1 : 0);
            HideModal(nullptr);
        }
    }
}
void RenderStoryModal(const spring::Snapshot& s) {
    const auto& n = s.story;
    const char* labels[3] = {"旅途档案", "经历记录", "园丁小事"};
    const char* main = "当前安排";
    lv_obj_remove_state(primary, LV_STATE_DISABLED);
    for (auto* button : choices)
        lv_obj_remove_flag(button, LV_OBJ_FLAG_HIDDEN);
    spring_ui::TextChanged(modal_hint, "");
    if (modal_kind == 17) {
        spring_ui::TextChanged(modal_title, "车厢记录仪");
        spring_ui::TextChanged(modal_body,
                               "建设：设施蓝图与大树升级\n当前计划：工作、方案与待办事项\n收藏：图"
                               "鉴、旅途档案与纪念物\n已建设施可直接点击场景操作");
        labels[0] = "建设";
        labels[1] = "当前计划";
        labels[2] = "收藏";
        if (spring_ui::kCollectionPracticeEnabled) {
            main = "收获练习";
            lv_obj_remove_flag(primary, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(primary, LV_OBJ_FLAG_HIDDEN);
        }
    } else if (modal_kind == 28) {
        spring_ui::TextChanged(modal_title, "收获练习");
        spring_ui::TextChanged(modal_body,
                               s.demo ? "轻晃摇落，滑动接取。\n资源与叶片两种练习交替出现。\n本次接"
                                        "取不额外入库。\n演示存档独立保存，自动养护照常。"
                                      : "将在独立演示存档中练习。\n正常进度保留，十倍推进经营。\n先"
                                        "进入演示，再开始接取。\n本次接取不额外入库。");
        spring_ui::TextChanged(modal_hint,
                               s.demo ? "练习结束后可在这里切回正常" : "正常与演示分别保存");
        labels[0] = "切回正常";
        labels[1] = "";
        labels[2] = "记录仪";
        main = s.demo ? "开始练习" : "进入演示";
        lv_obj_add_flag(choices[1], LV_OBJ_FLAG_HIDDEN);
        if (!s.demo)
            lv_obj_add_flag(choices[0], LV_OBJ_FLAG_HIDDEN);
    } else if (modal_kind == 27) {
        spring_ui::TextChanged(modal_title, "与01-99通讯");
        bool wish_pending = false;
        if (communication_source == CommunicationSource::Life && active_call.load() == 98) {
            std::lock_guard<std::mutex> guard(social_mutex);
            wish_pending = spring::WishPending(agency_public);
            spring_ui::TextFormat(
                modal_body, "01-99想%s\n%s",
                spring::WishName(agency_public.current.intent, agency_public.current.target),
                wish_pending ? "尚未行动，等你同意。"
                             : spring::WishStatusText(agency_public.current.status));
        } else if (communication_source == CommunicationSource::Life && active_call.load() == 99) {
            std::lock_guard<std::mutex> guard(social_mutex);
            spring_ui::TextFormat(modal_body, "01-99想和你说说\n%s\n可以继续追问。",
                                  spring::SocialTopic(social_public, true));
        } else if (active_call.load() < 6)
            spring_ui::TextChanged(modal_body, spring::Opening(active_call.load(), s));
        else if (active_call.load() == 6 && n.active < spring::kStoryCount)
            spring_ui::TextChanged(modal_body, spring::StoryNodeOpening(s, n.active));
        else
            spring_ui::TextChanged(modal_body, "花园车厢 · 01-99\n通讯已打开，可以继续交谈。");
        const auto audio = Application::GetInstance().GetDeviceState();
        spring_ui::TextChanged(modal_hint, communication_closing            ? "播报结束后关闭"
                                           : audio == kDeviceStateSpeaking  ? "01-99 正在说话"
                                           : audio == kDeviceStateListening ? "正在聆听"
                                           : audio == kDeviceStateIdle      ? "通讯已结束"
                                                                            : "正在连接");
        for (unsigned i = 0; i < 3; ++i) {
            if (wish_pending) {
                lv_obj_remove_flag(choices[i], LV_OBJ_FLAG_HIDDEN);
                static const char* replies[] = {"同意", "稍后", "不去"};
                spring_ui::TextChanged(choice_labels[i], replies[i]);
            } else
                lv_obj_add_flag(choices[i], LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_add_flag(primary, LV_OBJ_FLAG_HIDDEN);
        return;
    } else if (modal_kind == 25) {
        spring_ui::TextChanged(modal_title, "收藏");
        spring_ui::TextChanged(
            modal_body,
            "图鉴：已发现的植物与动物\n旅途档案：旅途中找到的原文\n纪念物：实际获得的种子与纪念花");
        labels[0] = "图鉴";
        labels[1] = "旅途档案";
        labels[2] = "纪念物";
        main = "记录仪首页";
    } else if (modal_kind == 26) {
        spring_ui::TextChanged(modal_title, "收藏 · 纪念物");
        if (n.seed && (!n.flower || story_record_page % 2 == 0))
            spring_ui::TextChanged(modal_body,
                                   n.seed ? "窗边种子\n已在旅途中获得\n可以在当前计划中查看种植安排"
                                          : "窗边种子\n尚未获得");
        else if (n.flower)
            spring_ui::TextChanged(modal_body, n.flower == 4
                                                   ? "窗边纪念花\n已开花，留在车厢里陪伴旅途"
                                               : n.flower ? "窗边纪念花\n已种下，正在真实生长"
                                                          : "窗边纪念花\n尚未种下");
        else
            spring_ui::TextChanged(modal_body, "还没有获得纪念物，继续这段旅途吧");
        labels[0] = "上一件";
        labels[1] = "下一件";
        labels[2] = "收藏";
        main = "返回收藏";
        if (!(n.seed && n.flower)) {
            lv_obj_add_flag(choices[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(choices[1], LV_OBJ_FLAG_HIDDEN);
        }
    } else if (modal_kind == 18) {
        if (!(n.archives & (1u << story_doc)))
            for (unsigned i = 0; i < 7; ++i)
                if (n.archives & (1u << i)) {
                    story_doc = i;
                    break;
                }
        spring_ui::TextChanged(modal_title, "旅途档案");
        spring_ui::TextFormat(
            modal_body, "%s\n%s\n只展示已经发现的记录",
            n.archives ? spring::kStoryArchiveTitles[story_doc] : "还没有发现档案",
            (n.archives_read & (1u << story_doc)) ? "已查看" : "未查看");
        labels[0] = "上一份";
        labels[1] = "下一份";
        labels[2] = "查看";
        main = "打开档案";
        if (!n.archives)
            lv_obj_add_state(primary, LV_STATE_DISABLED);
    } else if (modal_kind == 19) {
        if (story_seen_doc != story_doc || !(n.archives_read & (1u << story_doc))) {
            StorySend(spring::StoryVerb::ArchiveSeen, story_doc);
            story_seen_doc = story_doc;
        }
        spring_ui::TextChanged(modal_title, spring::kStoryArchiveTitles[story_doc]);
        spring_ui::TextChanged(archive_text, spring::kStoryArchiveTexts[story_doc]);
        lv_obj_update_layout(archive_text);
        const auto scale = lv_obj_get_style_transform_scale_y(archive_text, LV_PART_MAIN);
        lv_obj_set_height(archive_content, lv_obj_get_height(archive_text) * scale / 256 + 6);
        const auto audio = Application::GetInstance().GetDeviceState();
        spring_ui::TextFormat(modal_hint, "原文 · 上下滑动 · %s",
                              communication_closing            ? "播报结束后关闭"
                              : audio == kDeviceStateSpeaking  ? "01-99 正在说话"
                              : audio == kDeviceStateListening ? "正在聆听"
                              : communication_source == CommunicationSource::Story ? "可以追问"
                                                                                   : "可选择朗读");
        labels[0] = "向上";
        labels[1] = "向下";
        labels[2] = "读给我听";
        main = "档案列表";
    } else if (modal_kind == 20) {
        spring_ui::TextChanged(modal_title, "经历与决定");
        if (n.count) {
            const auto& e = n.records[n.count - 1 - story_record_page % n.count];
            const char* action = e.action == 6    ? "玩家确认此段内容"
                                 : e.action == 10 ? "已投入水2种花"
                                 : e.action == 12 ? "称呼已确认"
                                 : e.action == 4  ? "独自生活已展示"
                                 : e.action == 2  ? "玩家完成真实协作"
                                 : e.action == 8  ? "请求查看档案"
                                                  : "真实节点记录";
            spring_ui::TextFormat(modal_body, "%s\n%s\n游戏%lu秒 · 值%lu",
                                  (e.action == 8 || e.action == 14) && e.node < 7
                                      ? spring::kStoryArchiveTitles[e.node]
                                      : spring::kStorySpecs[e.node].title,
                                  action, (unsigned long)e.second, (unsigned long)e.value);
        } else
            spring_ui::TextChanged(modal_body, "还没有记录");
        labels[0] = "上一条";
        labels[1] = "下一条";
        labels[2] = "下一条";
        main = "记录首页";
        lv_obj_add_flag(choices[2], LV_OBJ_FLAG_HIDDEN);
    } else if (modal_kind == 21) {
        spring_ui::TextChanged(modal_title, "当前计划 · 旅途安排");
        const auto current = PickStoryNode(s);
        spring_ui::TextFormat(
            modal_body, "待续：%s\n称呼：%s\n种花：%s\n暂缓：%s",
            current < spring::kStoryCount ? spring::kStorySpecs[current].title : "暂无片段",
            n.candidate_who ? "有候选，等待确认" : "暂无待确认候选",
            n.flower ? "已种下，继续生长"
            : n.seed ? "可查看或继续安排"
                     : "尚未获得种子",
            n.paused < spring::kStoryCount ? spring::kStorySpecs[n.paused].title
            : n.seed_choice == 2           ? "种子暂存，稍后可继续"
                                           : "暂无");
        labels[0] = "确认称呼";
        labels[1] = "种花方案";
        labels[2] = "继续旅途";
        main = "返回计划";
    } else if (modal_kind == 22) {
        spring_ui::TextChanged(modal_title, "称呼确认");
        spring_ui::TextFormat(modal_body, "对象：%s\n候选：%s\n确认前保留原称呼\n也可以保留01-99",
                              n.candidate_who == 1 ? "玩家" : "园丁",
                              n.candidate_who ? n.candidate.data()
                              : n.robot_named ? n.robot_name.data()
                                              : "请先通过语音提出");
        labels[0] = "取消候选";
        labels[1] = "稍后再说";
        labels[2] = "继续旅途";
        main = "确认称呼";
        if (!n.candidate_who)
            lv_obj_add_state(primary, LV_STATE_DISABLED);
        else
            lv_obj_remove_state(primary, LV_STATE_DISABLED);
    } else if (modal_kind == 23) {
        spring_ui::TextChanged(modal_title, "窗边纪念花");
        spring_ui::TextFormat(modal_body, "一次投入：水2\n产出：0 · 独立花盆\n当前水：%lu\n%s",
                              (unsigned long)s.water,
                              n.flower    ? "已种植，不会再次扣费"
                              : !n.seed   ? "还没有发现种子"
                              : n.offered ? "等待明确批准"
                                          : "正在准备方案");
        labels[0] = "暂缓";
        labels[1] = "不种";
        labels[2] = "不种";
        main = "确认投入";
        lv_obj_add_flag(choices[2], LV_OBJ_FLAG_HIDDEN);
        if (!n.offered || n.flower) {
            lv_obj_add_flag(choices[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(choices[1], LV_OBJ_FLAG_HIDDEN);
        }
        if (!n.offered || n.flower || s.water < 2)
            lv_obj_add_state(primary, LV_STATE_DISABLED);
        else
            lv_obj_remove_state(primary, LV_STATE_DISABLED);
    } else if (modal_kind == 24) {
        if (story_selected < spring::kStoryCount) {
            if (!(n.presented & (1u << story_selected)))
                StorySend(spring::StoryVerb::Presented, story_selected);
            spring_ui::TextChanged(modal_title, spring::kStorySpecs[story_selected].title);
            spring_ui::TextChanged(modal_body, spring::StoryNodeDetail(s, story_selected));
        } else {
            spring_ui::TextChanged(modal_title, "剧情摘要");
            spring_ui::TextChanged(modal_body, "暂时没有待处理片段。\n经营与生活继续运行。");
        }
        labels[0] = "语音交流";
        labels[1] = "稍后";
        labels[2] = (story_selected == 10 || story_selected == 11) ? "不种" : "查看档案";
        if (story_selected >= spring::kStoryCount) {
            lv_obj_add_state(primary, LV_STATE_DISABLED);
            for (auto* button : choices)
                lv_obj_add_flag(button, LV_OBJ_FLAG_HIDDEN);
        } else if (story_selected != 10 && story_selected != 11 &&
                   spring::kStorySpecs[story_selected].archive >= 7) {
            lv_obj_add_flag(choices[2], LV_OBJ_FLAG_HIDDEN);
        }
        main = story_selected == 6 && spring::ChapterHas(s.chapters, spring::BeyondWorkObserved) &&
                       !spring::ChapterHas(s.chapters, spring::BenchRestCompleted)
                   ? "邀请坐下"
               : story_selected == 10 ? "保留种子"
               : story_selected == 11 ? "种植方案"
                                      : "我看过了";
    }
    for (unsigned i = 0; i < 3; ++i)
        spring_ui::TextChanged(choice_labels[i], labels[i]);
    spring_ui::TextChanged(lv_obj_get_child(primary, 0), main);
}
