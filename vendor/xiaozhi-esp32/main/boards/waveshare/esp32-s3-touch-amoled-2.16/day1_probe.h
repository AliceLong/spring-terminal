#pragma once
#include <driver/i2c_master.h>
#include <lvgl.h>
#include <functional>
#include <memory>
class LvglFont;
#include "game/spring_core.h"
namespace desk_gap {
bool SetGameTextFont(std::shared_ptr<LvglFont> font);
std::shared_ptr<LvglFont> GetGameTextFont();
void StartDay1Probe(lv_obj_t* parent);
void RecordTouchPoll(int64_t started_us, bool pressed);
spring::Snapshot GetWorldSnapshot();
void GetWorldSnapshot(spring::Snapshot& out);
void RegisterGameTools();
void RecordMonitorQuery(unsigned kind);
bool QueueLifeInvitation(unsigned intent, unsigned target, uint32_t sequence);
bool QueueResumeWork(uint32_t sequence);
bool QueueActivityCancellation(uint32_t plant_id, uint32_t life_sequence,
                               const spring::Snapshot& expected);
uint32_t QueuePlantWork(unsigned kind, unsigned slot, uint32_t cycle, bool demo, uint32_t world_hi,
                        uint32_t world_lo);
spring::PlantAction GetPlantWorkStatus(uint32_t id);
bool QueueWishResponse(uint32_t token, unsigned choice, bool voice_request = true);
uint32_t QueueVoiceWave();
const char* VoiceWaveStatus(uint32_t request_id);
bool QueueEconomyProposal(unsigned kind, unsigned target, unsigned value = 0);
bool QueueStoryAction(unsigned verb, unsigned target = 255, unsigned value = 0, unsigned token = 0,
                      const char* text = "", bool voice_request = true);
bool QueueVoiceApproval(unsigned token);
unsigned ActiveCommunication();
std::string CharacterContextJson(const spring::Snapshot& s);
std::string CommunicationContextJson(const spring::Snapshot& s);
void VoiceInteractionCompleted();
void GardenerTranscript(const char* role, const char* text);
void StartMotionInput(i2c_master_bus_handle_t bus);
void MotionShakeDetected();
void MotionSample(float angle, bool stable);
void SetMotionAvailable(bool available);
void SetPickupWakeCallback(std::function<void()> callback);
void PickupWakeFeedback();
}  // namespace desk_gap
