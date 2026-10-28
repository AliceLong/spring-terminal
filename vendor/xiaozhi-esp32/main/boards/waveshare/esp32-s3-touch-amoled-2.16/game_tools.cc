#include <esp_log.h>
#include <cJSON.h>
#include "application.h"
#include "day1_probe.h"
#include "game/spring_tool_json.h"
#include "mcp_server.h"
#include "spring_touch.h"
namespace desk_gap {
namespace {
ToolResult ReplyGameJson(const char* tool, std::string json) {
    const auto wrapped_bytes = spring::GameToolReplyBytes(json);
    ESP_LOGI("SpringTools", "TOOL_REPLY tool=%s text_bytes=%u wrapped_budget_bytes=%u", tool,
             static_cast<unsigned>(json.size()), static_cast<unsigned>(wrapped_bytes));
    if (wrapped_bytes > spring::kGameToolReplyBudget)
        return std::unexpected(
            std::string("Tool reply exceeds safe transport size; query a smaller detail page."));
    return ReturnValue(std::move(json));
}
}  // namespace
void RegisterGameTools() {
    auto& server = McpServer::GetInstance();
    server.AddTool(
        "get_world_state",
        "Read LIVE 终点站是春天. You are gardener robot 01-99 in an old sightseeing carriage. "
        "Use live costs, region, locked_yield, paused_no_energy and emergency_seconds. "
        "An unpaid idle slot may switch to Light on power loss or Moss on water shortage only "
        "after device approval. Exploration brings scrap, never parts; recycle separately. Bird "
        "costs food2 water2; its home needs tree stage2. Goals do not approve spending. "
        "A queued proposal is not completed work. "
        "ALWAYS query for plants, water/energy/fiber, tree, work, journey and language_stage. "
        "Explicit plant care/watering uses request_plant_care; mature harvest uses "
        "request_plant_harvest. "
        "Never promise either without the actual tool request. plant_action is real progress, not "
        "completion at queue time. "
        "For active_journey, say its name, remaining time, pause and expected result; describe "
        "discarded-device work as inspection, never repair. Speak naturally. Without "
        "accepted_communication, answer the player's question; never offer to open 剧情 "
        "unprompted. "
        "World has current life only: get_life_state has history, get_memories has decisions, "
        "get_story_state has narrative. Traveling is not arrival, and solitary life is not shared. "
        "Do not invent specific scenery. On 接听通讯 say only accepted_communication.opening, "
        "then wait. "
        "On 想说件事 YOU initiated contact; the player only accepted YOUR invitation. "
        "Query communication.social. If active=true, OPEN with its verified event, "
        "never ask why the player called or say they asked for a routine check. Add one restrained "
        "reaction or question, then listen. source_kind1 is life,2 is a read archive,3 is a "
        "completed milestone. player_requested proves an invitation, not shared viewing. "
        "communication.character has the shared interest; get_character_state has its evidence. "
        "No invented scenery, plant changes or shared presence. "
        "For safe-point waits, action order or interruptions query get_gardener_schedule. "
        "For personal agency and proposed life wishes query get_character_state. "
        "For narrative, names, archives or story planting wishes query get_story_state. Story "
        "contact "
        "id6 uses its current opening. "
        "language_stage 0: very short hesitant fragments; stage 1: short mechanical sentences, "
        "restrained; stage2: can reflect and ask personal questions; stage3: natural but reserved "
        "personal expression. Never assistant-like. "
        "Never speak tool names or JSON or invent animals, harvests, construction or a human "
        "past. Read-only.",
        PropertyList(), [](const PropertyList&) -> ToolResult {
            RecordMonitorQuery(0);
            const auto s = GetWorldSnapshot();
            ESP_LOGI("SpringTools", "LIVE_QUERY revision=%lu tree=%lu events=%lu",
                     (unsigned long)s.revision, (unsigned long)s.tree_stage,
                     (unsigned long)s.event_count);
            auto json =
                spring::ToolWorldJson(s, ActiveCommunication(), CommunicationContextJson(s));
            const auto bit = spring_ui::NextCommunication(s.notices);
            if (bit < 6) {
                auto* root = cJSON_Parse(json.c_str());
                if (root) {
                    auto* incoming = cJSON_AddObjectToObject(root, "incoming_communication");
                    cJSON_AddNumberToObject(incoming, "id", bit);
                    cJSON_AddStringToObject(incoming, "opening", spring::Opening(bit, s));
                    cJSON_AddBoolToObject(incoming, "unread", true);
                    auto* encoded = cJSON_PrintUnformatted(root);
                    if (encoded) {
                        json = encoded;
                        cJSON_free(encoded);
                    }
                    cJSON_Delete(root);
                }
            }
            return ReplyGameJson("get_world_state", std::move(json));
        });
    server.AddTool(
        "get_character_state",
        "Read device-authoritative current interest, completed player-requested inspection, "
        "read archive facts and verified milestones. ALWAYS query when asked what is on your mind, "
        "why you want to share, or what you remember about a requested inspection. "
        "An interest is not recovered memory or a new observation. A player request does not "
        "prove they watched. Only returned archives are known here; never reveal locked history. "
        "Read agency.level, implemented_max_level, wish token/status and permission result. "
        "On 我有想法 verify communication.agency.active using get_world_state, then read this "
        "tool. "
        "known_archive_ids are references; use get_story_archive for their text. "
        "Use get_world_state for current resources and action status. Read-only.",
        PropertyList(), [](const PropertyList&) -> ToolResult {
            RecordMonitorQuery(10);
            return ReplyGameJson("get_character_state", CharacterContextJson(GetWorldSnapshot()));
        });
    server.AddTool(
        "get_life_state",
        "Read LIVE gardener solitary life: intent, phase, reason, arrived, last six outcomes. "
        "For what you are doing, query get_world_state first; use this tool for solitary-life "
        "intent, why it stopped, and recent life outcomes. "
        "Traveling is not watching; interrupted is not completed. No specific scenery or leaf "
        "changes are confirmed. player_participated=false: never say we experienced it together. "
        "Use returned language_stage. Very short broken phrases at0, short mechanical sentences "
        "at1; more personal at2, natural but reserved at3. No resource effects, no contact "
        "requests. Read-only.",
        PropertyList(), [](const PropertyList&) -> ToolResult {
            RecordMonitorQuery(1);
            return ReplyGameJson("get_life_state", spring::LifeJson(GetWorldSnapshot()));
        });
    server.AddTool("get_recent_events",
                   "Read the latest 12 REAL game events, oldest first, from 终点站是春天. target "
                   "is zero-based plant slot for harvest/maturity/selection. Game seconds are "
                   "simulated time, not wall time. Also query get_world_state for current facts. "
                   "Read-only; cannot mint resources or complete construction.",
                   PropertyList(), [](const PropertyList&) -> ToolResult {
                       RecordMonitorQuery(2);
                       return ReplyGameJson("get_recent_events",
                                            spring::EventsJson(GetWorldSnapshot()));
                   });
    server.AddTool(
        "get_memories",
        "Read real device experiences and permanent milestones. ALWAYS use for past decisions, "
        "first cooperation, how a tree/facility was built, past rescues and shared memories. "
        "category0 all/1 life/2 decisions/3 world/4 player cooperation/5 legacy import. chain0 "
        "any; "
        "chain>0 retrieves that decision's stages. after_id0 no lower bound. "
        "Use next_before as before_id to page older retained events. Not a computer-archive query. "
        "limit1..24 is an upper bound; replies fit transport size. Page records with next_before; "
        "page long_term independently with next_fact_offset as fact_offset (0 means done). "
        "Long_term facts carry evidence IDs, cost and participation. Baseline means "
        "already present when tracking began: NEVER invent builder/date or call it first ever. "
        "first_recorded_since_tracking is NOT first in the whole game's history. Approved is "
        "not completed; proposed/rejected/superseded never count as executed. An actor or flag "
        "only proves that specific recorded participation, not that player watched every action. "
        "Device retains128 detailed events,32 pinned facts; overwritten history is incomplete. "
        "No record does not mean never happened. Read-only, no preference inference or story "
        "unlock. For narrative choices, names and archive facts also query get_story_state.",
        PropertyList({Property("category", kPropertyTypeInteger, 0, 0, 5),
                      Property("chain", kPropertyTypeInteger, 0, 0, 2147483647),
                      Property("after_id", kPropertyTypeInteger, 0, 0, 2147483647),
                      Property("before_id", kPropertyTypeInteger, 0, 0, 2147483647),
                      Property("limit", kPropertyTypeInteger, 6, 1, 24),
                      Property("fact_offset", kPropertyTypeInteger, 0, 0, 32),
                      Property("fact_limit", kPropertyTypeInteger, 4, 1, 8)}),
        [](const PropertyList& p) -> ToolResult {
            RecordMonitorQuery(4);
            return ReplyGameJson("get_memories",
                                 spring::ToolMemoryJson(
                                     GetWorldSnapshot(), p["category"].value<int>(),
                                     p["chain"].value<int>(), p["after_id"].value<int>(),
                                     p["limit"].value<int>(), p["before_id"].value<int>(),
                                     p["fact_offset"].value<int>(), p["fact_limit"].value<int>()));
        });
    server.AddTool(
        "invite_gardener_life",
        "Accept an EXPLICIT player invitation to watch the window, inspect a plant, or rest. "
        "ALWAYS call this before agreeing to actually go; words alone do not move your body. "
        "intent:1 window INSIDE the carriage (never outside train),2 inspect,3 rest; "
        "target:0-based "
        "plot0..3 for inspect;4 memorial flower ONLY when planted;5 animal home ONLY when actually "
        "occupied;6 head plant after performance history is known. For an explicit invitation "
        "to sit on the passenger seat, submit intent3 BEFORE deciding it is unavailable. "
        "bench_permission=false means no invitation has been accepted yet; it is NOT a refusal "
        "condition. A valid FIRST invitation grants seat permission, never completed rest. "
        "The device validates actual conditions. Never explain chapter numbers, unlocks or "
        "internal permission fields to the player; only explain a verified in-world blocker. "
        "No spending or construction. Refused when "
        "important work, shortage or ripe crops need attention. A short watering/harvest tool "
        "may finish safely first; read get_gardener_schedule for the real wait. "
        "A new explicit life destination replaces the old life plan. Queued is not arrival or "
        "completion. "
        "When queued:true, say one brief acknowledgement in your actual language stage, "
        "then continue the conversation while the local life FSM moves. If refused, explain "
        "briefly and keep talking. Do not claim specific scenery, observations, or player "
        "presence. Invitation is recorded; actual arrival/outcome are separate facts. "
        "A repeated identical pending invitation is not another action.",
        PropertyList({Property("intent", kPropertyTypeInteger, 1, 3),
                      Property("target", kPropertyTypeInteger, 0, 0, 6)}),
        [](const PropertyList& p) -> ToolResult {
            RecordMonitorQuery(5);
            const auto s = GetWorldSnapshot();
            const auto intent = p["intent"].value<int>(), target = p["target"].value<int>();
            spring::Core check;
            static_cast<spring::State&>(check.s) = s;
            check.storage_ok = s.storage_ok;
            check.work = s.work;
            if (!check.CanInviteLife(intent, target))
                return ReturnValue(
                    std::string("{\"queued\":false,\"reason\":\"conditions_not_met\","
                                "\"instruction\":\"Query world and get_gardener_schedule for "
                                "the actual blocker; do not invent chapter locks.\"}"));
            const bool ok = QueueLifeInvitation(intent, target, s.life.sequence);
            return ReturnValue(std::string(ok ? "{\"queued\":true,\"arrived\":false,\"spent\":"
                                                "false,\"conversation_stays_open\":true}"
                                              : "{\"queued\":false,\"reason\":\"queue busy\"}"));
        });
    server.AddTool(
        "request_gardener_work",
        "Use BEFORE agreeing to an explicit player request to go back to work, continue "
        "maintenance, "
        "or 'then go' referring to work just discussed. A preference question alone is not a "
        "command. "
        "Query get_world_state first. This returns control from voluntary life to EXISTING "
        "automatic "
        "maintenance while conversation stays open. Never builds, spends, plants, harvests or "
        "creates "
        "a new economic job. pending_work=false means there is no current task: say so briefly, "
        "do not promise to go do nonexistent work. queued=true is not completion; verify the "
        "actual "
        "robot.work using get_world_state. For seeing occupied animals instead, "
        "use invite_gardener_life intent2 target5; inspection uses intent2 target0..3. "
        "For specific real care use request_plant_care; for mature harvesting "
        "request_plant_harvest.",
        PropertyList(), [](const PropertyList&) -> ToolResult {
            RecordMonitorQuery(11);
            const auto s = GetWorldSnapshot();
            spring::Core check;
            static_cast<spring::State&>(check.s) = s;
            check.storage_ok = s.storage_ok;
            check.work = s.work;
            if (!check.CanResumeWork())
                return ReturnValue(
                    std::string("{\"queued\":false,\"reason\":\"storage, awakening or story action "
                                "unavailable\"}"));
            const bool ok = QueueResumeWork(s.life.sequence);
            return ReturnValue(
                std::string("{\"queued\":") + (ok ? "true" : "false") +
                ",\"pending_work\":" + (check.LifeWorkPending() ? "true" : "false") +
                ",\"spent\":false,\"completed\":false,\"conversation_stays_open\":true}");
        });
    // Each request returns immediately. Only the world worker owns the body and reward.
    for (unsigned kind = 1; kind <= 2; ++kind) {
        const char* name = kind == 1 ? "request_plant_care" : "request_plant_harvest";
        server.AddTool(
            name,
            kind == 1
                ? "Use BEFORE agreeing to explicitly care for or water a SPECIFIC planted slot. "
                  "Query get_world_state first. slot is ONE-BASED 1..4; cycle must match that "
                  "plant's current cycle. This creates a real move-and-care job while conversation "
                  "remains open. Care tidies leaves and waters non-moss using the ALREADY PAID "
                  "cycle; NO extra cost, growth boost, new planting or harvest. Unpaid cycles, no "
                  "energy and important work are refused. For inspection only use "
                  "invite_gardener_life. A preference question is not a command. QUEUED is not "
                  "completion: query get_plant_action_status before claiming success."
                : "Use BEFORE agreeing to explicitly HARVEST a SPECIFIC MATURE planted slot. Query "
                  "get_world_state first. slot is ONE-BASED 1..4; cycle must match. Real "
                  "move-and-harvest job, including moss collection into WATER storage, never "
                  "irrigation. No reward at request time: one original yield is credited only on "
                  "actual completion. No extra bonus or cost. Immature, stale, important work and "
                  "duplicate crop claims cannot mint resources. QUEUED is not completion: query "
                  "get_plant_action_status; conversation stays open. No implicit approval of "
                  "building or planting.",
            PropertyList({Property("slot", kPropertyTypeInteger, 1, 4),
                          Property("cycle", kPropertyTypeInteger, 1, INT32_MAX)}),
            [kind, name](const PropertyList& p) -> ToolResult {
                RecordMonitorQuery(kind == 1 ? 13 : 14);
                const auto s = GetWorldSnapshot();
                spring::Core check;
                static_cast<spring::State&>(check.s) = s;
                check.storage_ok = s.storage_ok;
                const unsigned slot = p["slot"].value<int>() - 1;
                const unsigned cycle = p["cycle"].value<int>();
                const auto reason = check.CheckPlantWork(kind, slot, cycle);
                if (reason != spring::PlantActionReason::None)
                    return ReplyGameJson(
                        name,
                        std::string("{\"request_id\":0,\"status\":\"REJECTED\",\"reason\":\"") +
                            spring::PlantActionReasonName(reason) + "\",\"completed\":false}");
                const auto id =
                    QueuePlantWork(kind, slot, cycle, s.demo, s.memory ? s.memory->world_hi : 0,
                                   s.memory ? s.memory->world_lo : 0);
                return ReplyGameJson(name,
                                     id ? spring::PlantActionJson(GetPlantWorkStatus(id))
                                        : "{\"request_id\":0,\"status\":\"REJECTED\",\"reason\":"
                                          "\"plant_action_or_queue_busy\",\"completed\":false}");
            });
    }
    server.AddTool("get_plant_action_status",
                   "Read the actual outcome of request_plant_care or request_plant_harvest. "
                   "QUEUED, TRAVELING and RUNNING are not completed. UNKNOWN means restart or "
                   "receipt expired (only 4 receipts retained), never success. completed=true "
                   "confirms execution and persisted result. Read get_world_state for current "
                   "crop/resources. Do not poll in a loop or interrupt the player's conversation.",
                   PropertyList({Property("request_id", kPropertyTypeInteger, 1, INT32_MAX)}),
                   [](const PropertyList& p) -> ToolResult {
                       RecordMonitorQuery(15);
                       return ReplyGameJson("get_plant_action_status",
                                            spring::PlantActionJson(
                                                GetPlantWorkStatus(p["request_id"].value<int>())));
                   });
    server.AddTool(
        "get_gardener_schedule",
        "Read the actual stage0/1 body owner, safe-point wait reason, queued plant task, approved "
        "wish pause/resume and latest life interruption. ALWAYS query when asked why you have not "
        "moved, what comes next or what interrupted your plan; query after a queued or busy reply "
        "before promising a next action. wait_text is factual guidance in current language stage, "
        "not a new action. Only an already-approved stage1 wish may resume after temporary "
        "care/harvest; a new life destination or return-to-work cancels the old plan. Paused is "
        "not completed; arrived_now differs from has_reached_before. Important work is refused, "
        "never silently queued. Read-only, no story/permission upgrade.",
        PropertyList(), [](const PropertyList&) -> ToolResult {
            RecordMonitorQuery(16);
            return ReplyGameJson("get_gardener_schedule", spring::ScheduleJson(GetWorldSnapshot()));
        });
    server.AddTool(
        "cancel_gardener_activity",
        "Only on an EXPLICIT player request to stop a current requested plant task or life "
        "arrangement. Query get_gardener_schedule first. Provide exactly ONE current "
        "plant_request_id or life_sequence; the other must be0. Cancels that precise active task, "
        "never construction/exploration, paid production or already committed harvest. Cancelling "
        "a life plan also removes its resume permission; cancelling an inserted plant task can "
        "allow a previously approved paused wish to resume. Transport queued is not cancellation: "
        "query actual schedule/status. A preference, silence or 'not now' before wish approval "
        "uses respond_life_wish instead.",
        PropertyList({Property("plant_request_id", kPropertyTypeInteger, 0, 0, INT32_MAX),
                      Property("life_sequence", kPropertyTypeInteger, 0, 0, INT32_MAX)}),
        [](const PropertyList& p) -> ToolResult {
            const unsigned plant = p["plant_request_id"].value<int>(),
                           life = p["life_sequence"].value<int>();
            RecordMonitorQuery(17);
            const auto expected = GetWorldSnapshot();
            const bool queued =
                bool(plant) != bool(life) && QueueActivityCancellation(plant, life, expected);
            return ReturnValue(
                std::string(queued ? "{\"queued\":true,\"cancelled\":false,\"query_schedule\":true}"
                                   : "{\"queued\":false,\"cancelled\":false}"));
        });
    server.AddTool(
        "respond_life_wish",
        "Reply to the robot's CURRENT SELF-PROPOSED wish only. Query get_character_state first. "
        "token must exactly match agency.wish.token. choice1 approve,2 defer,3 decline. "
        "Only a NEW explicit player reply AFTER opening the wish may approve. Clicking the "
        "bubble, asking its preference, silence, or its own opening are NEVER permission. "
        "Do not call invite_gardener_life as a workaround for a self-proposed wish. "
        "queued=true is not approval or completion: query agency.wish status again. "
        "No resource spending; important work rejects, short watering/harvest may finish first. "
        "Query get_gardener_schedule for actual waiting, pause and resume. Conversation remains "
        "open.",
        PropertyList({Property("token", kPropertyTypeInteger, 1, INT32_MAX),
                      Property("choice", kPropertyTypeInteger, 1, 3)}),
        [](const PropertyList& p) -> ToolResult {
            RecordMonitorQuery(12);
            const bool ok = QueueWishResponse(p["token"].value<int>(), p["choice"].value<int>());
            return ReturnValue(std::string(ok ? "{\"queued\":true,\"approved\":false,\"completed\":"
                                                "false,\"query_status\":true}"
                                              : "{\"queued\":false,\"reason\":\"queue busy\"}"));
        });
    server.AddTool(
        "request_robot_wave",
        "Wave once while the voice conversation remains open. Returns a request ID in QUEUED "
        "state, not completion. Use get_robot_wave_status to verify the actual result. "
        "This is a visual action only and never changes resources or story.",
        PropertyList(std::vector<Property>{}), [](const PropertyList&) -> ToolResult {
            const uint32_t id = QueueVoiceWave();
            return ReturnValue(
                std::string(id ? "{\"request_id\":" + std::to_string(id) + ",\"status\":\"QUEUED\"}"
                               : "{\"request_id\":0,\"status\":\"REJECTED\"}"));
        });
    server.AddTool(
        "get_robot_wave_status",
        "Read the actual device result for a request_robot_wave request ID.",
        PropertyList({Property("request_id", kPropertyTypeInteger, 1, INT32_MAX)}),
        [](const PropertyList& p) -> ToolResult {
            const uint32_t id = p["request_id"].value<int>();
            return ReturnValue(std::string("{\"request_id\":" + std::to_string(id) +
                                           ",\"status\":\"" + VoiceWaveStatus(id) + "\"}"));
        });
    server.AddTool(
        "propose_garden_action",
        "Create a REVIEWABLE plan proposal only, no resource spending. Query get_world_state "
        "first. "
        "kind: 1 build facility(target0..7), 2 next-cycle plant(target slot0..3,value plant0..3), "
        "3 priority(target0 survival/1 tree/2 construction/3 animals), 4 care(target slot0..3 "
        "or255 off), "
        "5 journey(target exact live journey id,value1 execute/0 skip), 6 recycle, 7 grow tree. "
        "Plant slots are ZERO-BASED: target=world plants.slot-1. Plant "
        "value0=tracklight,1=dewmoss,2=patchvine,3=travelbean. Care only supports moss/vine/bean. "
        "Do not call this for questions or ambiguous intent. Explain costs/upkeep and ask player "
        "to "
        "confirm after costs are stated. A later explicit user turn may use approve_garden_action "
        "with the exact proposal token. Never approve in the proposal creation turn. Queued is NOT "
        "completed.",
        PropertyList({Property("kind", kPropertyTypeInteger, 1, 7),
                      Property("target", kPropertyTypeInteger, 0, 2147483647),
                      Property("value", kPropertyTypeInteger, 0, 0, 3)}),
        [](const PropertyList& p) -> ToolResult {
            RecordMonitorQuery(3);
            const auto kind = p["kind"].value<int>(), target = p["target"].value<int>(),
                       value = p["value"].value<int>();
            const auto s = GetWorldSnapshot();
            spring::Core check;
            static_cast<spring::State&>(check.s) = s;
            check.storage_ok = s.storage_ok;
            if (!check.Propose(kind, target, value))
                return ReturnValue(std::string(
                    "{\"queued\":false,\"reason\":\"invalid or stale request; re-query world\"}"));
            const bool ok = QueueEconomyProposal(kind, target, value);
            return ReturnValue(std::string(
                ok ? "{\"queued\":true,\"requires_device_confirmation\":true,\"spent\":false}"
                   : "{\"queued\":false,\"reason\":\"queue busy\"}"));
        });
    server.AddTool(
        "get_story_state",
        "Read LIVE V4.6 narrative, names, wishes, private observations, unlocked history and "
        "current story opening. ALWAYS use when player asks narrative or responds to story contact "
        "id6. Say only the current opening first, then wait. Query node255 for current, or a "
        "specific AVAILABLE node. Missing/locked nodes have no text: never guess future "
        "revelations. Player is a remote maintenance-terminal collaborator, NOT train "
        "consciousness. Names may be early/late/absent. No flower or animal required to finish. "
        "For 打开档案 use archive_selected then get_story_archive. Open with one concrete "
        "discovered fact and a restrained reaction or question appropriate to language_stage; "
        "do not merely announce that an archive opened. Keep the conversation open for follow-up. "
        "Interpret only this unlocked evidence, distinguish guesses, no plot advance or G17 leak. "
        "Records are NEWEST FIRST; an empty record_cursor starts with "
        "recent interactions. For older records copy next_record_cursor exactly into "
        "record_cursor; "
        "an empty next cursor means done. Never invent numeric offsets. If restart_required=true, "
        "discard the partial history and query again with an empty cursor. Use current fields "
        "for current facts, not history. record_limit is an upper bound. Read-only.",
        PropertyList(
            {Property("node", kPropertyTypeInteger, 255, 0, 255),
             Property("record_offset", kPropertyTypeInteger, 0, 0, 16),
             Property("record_limit", kPropertyTypeInteger, 6, 1, 16),
             Property("record_cursor", kPropertyTypeString, std::string("")).SetMaxLength(128)}),
        [](const PropertyList& p) -> ToolResult {
            RecordMonitorQuery(6);
            return ReplyGameJson("get_story_state",
                                 spring::ToolStoryJson(GetWorldSnapshot(), p["node"].value<int>(),
                                                       p["record_offset"].value<int>(),
                                                       p["record_limit"].value<int>(),
                                                       p["record_cursor"].value<std::string>()));
        });
    server.AddTool(
        "get_story_archive",
        "Read discovered archive evidence only. id0 service report,1 performance,2 access log,3 "
        "G17 care,4 clearing list,5 final growth,6 recovered return summary. unavailable means "
        "unknown, do not invent. Reading with this tool does NOT prove player read it. Open screen "
        "ONLY on player request via story_action verb3; otherwise keep carriage visible and speak "
        "a short summary.",
        PropertyList({Property("id", kPropertyTypeInteger, 0, 6)}),
        [](const PropertyList& p) -> ToolResult {
            RecordMonitorQuery(7);
            return ReplyGameJson(
                "get_story_archive",
                spring::StoryArchiveJson(GetWorldSnapshot(), p["id"].value<int>()));
        });
    server.AddTool(
        "story_action",
        "Queue an EXPLICIT player narrative action; query get_story_state first. verb0 accept "
        "contact(node),1 acknowledge/discuss current node(node,choice),2 pause(node),3 OPEN "
        "requested discovered archive(node=id),4 propose memorial flower(water2,zero "
        "yield,separate pot),5 confirm flower(token),6 propose name(node1 player/2 robot,text),7 "
        "confirm candidate name(token),8 attend CURRENT actually-arrived life,10 show available "
        "story summary(node),11 cancel name(token). choice for seed node10:1 keep,2 postpone,3 "
        "decline. PlantWish node11 choice2 postpone/3 decline; planting MUST use verb4 then a "
        "later explicit user utterance verb5 exact token. Naming MUST repeat candidate then a "
        "later utterance verb7. Same-turn confirmations are rejected by device. Never call "
        "acknowledgement for mere questions, silence or merely reading a tool. Do not advance "
        "whole chapters automatically. Archive open keeps this voice session running; "
        "after its card appears, explain a real key fact and leave room for questions. "
        "Private life is not shared just "
        "because displayed. queued is NOT committed; requery to verify. verb9 reserved for device "
        "notification; do not call.",
        PropertyList({Property("verb", kPropertyTypeInteger, 0, 11),
                      Property("node", kPropertyTypeInteger, 255, 0, 255),
                      Property("choice", kPropertyTypeInteger, 0, 0, 3),
                      Property("token", kPropertyTypeInteger, 0, 0, 2147483647),
                      Property("text", kPropertyTypeString, std::string(""))}),
        [](const PropertyList& p) -> ToolResult {
            RecordMonitorQuery(8);
            const unsigned verb = p["verb"].value<int>(), node = p["node"].value<int>();
            if (verb == 9)
                return ReturnValue(
                    std::string("{\"queued\":false,\"reason\":\"device-only notification\"}"));
            const auto text = p["text"].value<std::string>();
            const bool ok = QueueStoryAction(verb, node, p["choice"].value<int>(),
                                             p["token"].value<int>(), text.c_str());
            return ReturnValue(
                std::string(ok ? "{\"queued\":true,\"committed\":false}" : "{\"queued\":false}"));
        });
    server.AddTool(
        "end_gardener_communication",
        "End the current conversation AFTER your brief spoken closing and playback finish. "
        "Use when the player explicitly asks to end, or the current discussion is naturally done. "
        "Never end merely because an archive opening or first sharing sentence finished. "
        "This does not cancel work, clear invitations or advance the story.",
        PropertyList(), [](const PropertyList&) -> ToolResult {
            const bool ok = Application::GetInstance().EndConversationAfterReply();
            ESP_LOGI("SpringTools", "COMMUNICATION_CLOSE accepted=%d", ok);
            return ReturnValue(
                std::string(ok ? "{\"close_after_reply\":true}" : "{\"close_after_reply\":false}"));
        });
    server.AddTool(
        "approve_garden_action",
        "Approve ONLY a specific already-presented economic proposal after a NEW explicit player "
        "confirmation, with its live token. Query live costs, upkeep and token first. Never "
        "approve on initial desire, ambiguous replies or same utterance as proposing; the device "
        "enforces later-user-turn. Calls queue the same transaction as the screen card; queued is "
        "not construction completion. Never mint resources.",
        PropertyList({Property("token", kPropertyTypeInteger, 1, 2147483647)}),
        [](const PropertyList& p) -> ToolResult {
            RecordMonitorQuery(9);
            const bool ok = QueueVoiceApproval(p["token"].value<int>());
            return ReturnValue(
                std::string(ok ? "{\"queued\":true,\"completed\":false}" : "{\"queued\":false}"));
        });
}
}  // namespace desk_gap
