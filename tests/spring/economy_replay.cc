#include "game/spring_core.h"
#include "game/spring_json.h"
#include <cassert>
#include <iostream>
using namespace spring;
static bool first = true;
void ReplayFrame(const Core &c, const char *title, const char *note) {
  if (!first)
    std::cout << ',';
  first = false;
  std::cout << "{\"title\":\"" << title << "\",\"note\":\"" << note
            << "\",\"state\":" << WorldJson(c.Get()) << '}';
}
void Approve(Core &c, unsigned kind, unsigned target = 0, unsigned value = 0) {
  assert(c.Propose(kind, target, value));
  assert(c.ConfirmProposal(c.s.economy.proposal.token));
}
void Finish(Core &c) {
  for (unsigned i=0;i<3600 && (c.BusyEconomy() || c.s.tree_work_ms);++i) c.Advance(1000);
  assert(!c.BusyEconomy() && !c.s.tree_work_ms);
}
int main() {
  Core c;
  c.storage_ok = true;
  c.EnableEconomy(true);
  c.Wake();
  std::cout << '[';
  ReplayFrame(c, "从真实开局开始",
              "水16、能源12、零件2；序章保留一滴已经成熟的露珠。");
  c.Advance(1000);
  ReplayFrame(c, "开始种植轮次",
              "投入在一轮开始时扣除，等待与重启不会重复收费。");
  c.Harvest(1);
  ReplayFrame(c, "收下序章露珠", "成熟雨窝苔提供水；后续每轮需要能源投入。");
  c.Advance(600000);
  for (unsigned i = 0; i < 3; ++i)
    c.Harvest(i);
  ReplayFrame(c, "第一轮收成", "三个位置形成供水、供能和建设材料循环。");
  for (unsigned i = 0; i < 6; ++i)
    c.Advance(645000);
  ReplayFrame(c, "积累建设材料",
              "园丁自动照料与收获；当前来讯保留，后续机会最多排队一个。");
  assert(c.Propose(1, 0));
  ReplayFrame(c, "机器人提出储水罐方案",
              "提出方案没有扣费，显示一次投入和建成后的收益。");
  assert(c.ConfirmProposal(c.s.economy.proposal.token));
  ReplayFrame(c, "玩家批准建设", "材料只扣一次，机器人开始施工。");
  Finish(c);
  assert(c.HasFacility(0));
  ReplayFrame(c, "储水罐建成", "水容量提高到100；其他生产照常推进。");
  assert(c.s.economy.journey == 1);
  assert(c.Announce(4));
  assert(!c.Announce(4));
  ReplayFrame(c, "旅途主动呼叫",
              "发现废弃设备；同一事件不会反复播报或发奖励。");
  assert(c.Accept(4));
  assert(c.Propose(5, c.s.economy.journey_id, 1));
  ReplayFrame(c, "接听后讨论探索方案",
              "接听和提出方案都不产生奖励，仍需要批准。");
  assert(c.ConfirmProposal(c.s.economy.proposal.token));
  ReplayFrame(c, "批准探索", "能源2已投入；回收物要等工作完成，零件需另行拆解。");
  Finish(c);
  assert(c.s.economy.scrap == 1);
  ReplayFrame(c, "探索完成",
              "获得回收物1件；工作台能进一步提高回收收益。");
  while (c.s.fiber < 9)
    c.Advance(645000);
  Approve(c, 1, 5);
  Finish(c);
  Approve(c, 6);
  Finish(c);
  ReplayFrame(c, "回收提高建设能力",
              "工作台使用能源2，处理一件回收物，得到零件2。");
  Approve(c, 3, 1);
  ReplayFrame(c, "玩家表达先养树", "优先级已记录；方向不会直接扣除建设材料。");
  for (unsigned i = 0; i < 6 && !c.s.economy.proposal.kind; ++i)
    c.Advance(645000);
  assert(c.s.economy.proposal.kind == 7);
  ReplayFrame(c, "计划形成具体提案",
              "资源满足后，园丁按养树目标主动提出培育方案。");
  assert(c.ConfirmProposal(c.s.economy.proposal.token));
  Finish(c);
  ReplayFrame(c, "目标落实到世界", "树长大；计划与完成结果保留在决策日志。");
  while (c.s.fiber < 15)
    c.Advance(645000);
  Approve(c, 1, 4);
  Finish(c);
  ReplayFrame(c, "扩建第四个位置",
              "新增位置种植旅粮豆，开始为动物准备持续食物。");
  c.Advance(645000);
  c.Harvest(3);
  while (c.s.fiber < 12)
    c.Advance(645000);
  Approve(c, 7);
  Finish(c);
  assert(c.s.tree_stage == 2);
  Approve(c, 1, 7);
  Finish(c);
  for (unsigned i = 0; i < 6 && c.s.economy.journey != 4; ++i) {
    if (c.s.economy.journey)
      Approve(c, 5, c.s.economy.journey_id, 0);
    c.Advance(1800000);
  }
  assert(c.s.economy.journey == 4);
  ReplayFrame(c, "动物求助也是主动通讯",
              "先检查小窝与救助投入，再决定是否承担长期照料。");
  Approve(c, 5, c.s.economy.journey_id, 1);
  Finish(c);
  assert(c.s.economy.animals & 1);
  ReplayFrame(c, "寻雨雀入住",
              "食物和水开始承担照料需求；没有集雨器时不会凭空集水。");
  c.Advance(1800000);
  ReplayFrame(c, "持续经营而非一次奖励",
              "每30分钟检查动物照料；缺粮只暂停帮助，动物不会死亡。");
  Core restored;
  restored.storage_ok = true;
  assert(restored.RestoreFull(c.PackFull()));
  ReplayFrame(restored, "退出后仍可恢复",
              "资源、设施、计划、待决定事件和重要日志一起保存。");
  std::cout << ']';
}
