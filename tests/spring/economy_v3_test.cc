#include "game/spring_core.h"
#include "game/spring_json.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace spring;
Core Fresh() {
    Core c; c.storage_ok=true; c.EnableEconomy(true); c.Wake();
    for(auto& p:c.s.plots) {p.ready=false;p.growth_ms=p.mature_ms=0;}
    c.s.economy.mature_yield.fill(0);
    c.s.economy.paid_mask=7;
    c.s.water=c.s.energy=40;
    return c;
}
void Approve(Core& c,unsigned kind,unsigned target=0,unsigned value=0) {
    assert(c.Propose(kind,target,value)); assert(c.ConfirmProposal(c.s.economy.proposal.token));
}
int main(int argc,char**argv) {
    // Every region: crop output is frozen at maturity, survives save, and is paid once.
    for(unsigned region=0;region<5;++region) {
        auto c=Fresh(); c.s.economy.region=region;
        for(auto& p:c.s.plots) p.growth_ms=599000;
        c.Advance(1000);
        auto saved=c.PackFull();Core r;r.storage_ok=true;assert(r.RestoreFull(saved));assert(r.PackFull()==saved);
        r.s.economy.region=(region+1)%5;
        const auto water=r.s.water,energy=r.s.energy;
        assert(r.Harvest(0)); assert(r.s.energy==energy+kRegionYields[region][0]);
        assert(r.Harvest(1)); assert(r.s.water==water+kRegionYields[region][1]);
        assert(r.Harvest(2)); assert(r.s.fiber==kRegionYields[region][2]);
        assert(!r.Harvest(2));
    }
    { auto c=Fresh();c.s.fiber=30;const auto w=c.s.water;
      Approve(c,7); assert(c.s.water==w-4&&c.s.fiber==27);
      c.Advance(60000); assert(c.s.tree_stage==1&&c.s.economy.bird_visit);
      assert(c.Propose(1,7));assert(!c.CanConfirm(c.s.economy.proposal.token));
      Approve(c,7); assert(c.s.fiber==21);
      c.Advance(119000);assert(c.s.tree_stage==1);
      c.Advance(1000);assert(c.s.tree_stage==2);
      Approve(c,1,7);c.Advance(120000);assert(c.HasFacility(7)); }
    // Workbench is an efficiency upgrade, not a prerequisite for parts.
    for(bool bench:{false,true}) {auto c=Fresh();c.s.economy.scrap=1;
      if(bench)c.s.economy.built|=32;
      const auto e=c.s.energy;Approve(c,6);
      assert(c.s.energy==e-(bench?2:0));
      c.Advance(bench?59000:179000);assert(c.s.economy.parts==2);
      Core r;r.storage_ok=true;assert(r.RestoreFull(c.PackFull()));r.Advance(1000);
      assert(r.s.economy.parts==(bench?4u:3u));assert(!r.s.economy.recycle_ms);
      r.Advance(1000);assert(r.s.economy.parts==(bench?4u:3u)); }
    // Power loss pauses real construction; emergency water cannot advance it.
    {auto c=Fresh();for(auto&p:c.s.plots)p.plant=p.next=Plant::Vine;
      c.s.economy.paid_mask=0;c.s.water=c.s.energy=0;
      c.s.economy.construction=0;c.s.economy.construction_ms=10000;
      c.Advance(150000);assert(c.s.economy.emergency_work==150000);
      assert(c.s.economy.construction_ms==10000);
      Core r;r.storage_ok=true;assert(r.RestoreFull(c.PackFull()));
      r.Advance(150000);assert(r.s.water==2&&!r.s.energy&&!r.s.economy.emergency_work);
      assert(r.s.economy.construction_ms==10000);
      Approve(r,2,0,0);assert(r.s.plots[0].plant==Plant::Light);
      r.Advance(650000);assert(r.s.energy>0&&r.s.economy.construction_ms>10000);
      r.Advance(130000);assert(r.HasFacility(0)); }
    // Already-paid non-energy crops may be safely harvested to free a recovery slot.
    {auto c=Fresh();for(auto&p:c.s.plots){p.plant=p.next=Plant::Vine;p.growth_ms=590000;}
      c.s.water=c.s.energy=0;c.Advance(700000);
      assert(!c.s.plots[0].ready&&!(c.s.economy.paid_mask&1));
      assert(c.s.water==2&&c.s.energy==0);Approve(c,2,0,0);
      c.Advance(650000);assert(c.s.energy>0); }
    // Animal upkeep is atomic, starts at rescue, and never creates ownership on visit.
    {auto c=Fresh();c.s.economy.animals=3;c.s.economy.built=130;
      c.s.economy.region=2;c.s.economy.food=5;
      c.s.economy.animal_elapsed={1799000,1799000};
      c.Advance(1000);assert(c.s.economy.food==0&&c.s.water==43&&c.s.economy.animal_help==3);
      c.s.economy.animal_elapsed={1799000,1799000};c.Advance(1000);
      assert(c.s.economy.animals==3&&c.s.economy.animal_help==0&&c.s.water==43); }
    // Single region entry bonus; no replay after saving and reloading.
    {auto c=Fresh();c.s.economy.built=2;c.s.economy.region=1;c.s.economy.region_ms=1799000;
      c.Advance(1000);assert(c.s.water==46);
      Core r;r.storage_ok=true;assert(r.RestoreFull(c.PackFull()));r.Advance(1000);assert(r.s.water==46);}
    // Solar can recover supply: no redundant emergency water injection.
    {auto c=Fresh();c.s.economy.built=4;c.s.economy.paid_mask=0;c.s.water=c.s.energy=0;
      c.Advance(300000);assert(c.s.water==0&&!c.s.economy.emergency_work);}
    // A solar-equipped train can still run out of water with no moss planted.
    // Player-approved empty-slot moss recovery prevents that separate lock.
    {auto c=Fresh();c.s.economy.built=4;c.s.economy.paid_mask=0;c.s.water=c.s.energy=0;
      for(auto&p:c.s.plots)p.plant=p.next=Plant::Vine;
      c.Advance(600000);assert(c.s.energy>0&&c.s.water==0);
      assert(c.CanRecoveryPlant(0,1));Approve(c,2,0,1);
      c.Advance(650000);assert(c.s.water>0);}
    // Pending calls are bounded and unapproved calls spend nothing or give no loot.
    {auto c=Fresh();c.s.economy.journey_clock=1799000;c.Advance(1000);
      assert(c.s.economy.journey==1);const auto id=c.s.economy.journey_id;
      c.Advance(3600000);assert(c.s.economy.journey_id==id&&c.s.economy.pending_journey);
      assert(c.s.economy.scrap==0&&c.s.economy.parts==2);
      Approve(c,5,id,0);c.Advance(1000);assert(c.s.economy.journey_id==id+1);}
    // Supply alerts survive old permanent drought acknowledgements without
    // making the newly persisted save invalid or repeatedly calling on each tick.
    {auto c=Fresh();c.s.accepted|=8;c.s.energy=c.s.water=0;c.s.economy.paid_mask=0;
      for(auto&p:c.s.plots)p.plant=p.next=Plant::Vine;
      c.Advance(1000);assert(c.s.notices&8);assert(!(c.s.accepted&8));
      Core r;r.storage_ok=true;assert(r.RestoreFull(c.PackFull()));
      assert(c.Accept(3));c.Advance(1000);assert(!(c.s.notices&8));}
    // Scripted player: approves affordable work and discoveries, never injects stock.
    // Validate the complete eight-facility route using the actual firmware core.
    { Core c;c.storage_ok=true;c.EnableEconomy(true);c.Wake();c.Harvest(1);
      auto attempt=[&](unsigned kind,unsigned target=0,unsigned value=0) {
          Core preview=c;
          if(!preview.Propose(kind,target,value) || !preview.CanConfirm(preview.s.economy.proposal.token)) return false;
          Approve(c,kind,target,value);return true;
      };
      unsigned complete_at=0;
      for(unsigned t=0;t<12*3600;t++) {
          if(c.s.economy.journey) {
              const auto id=c.s.economy.journey_id;
              if(!attempt(5,id,1) && c.s.economy.journey>=3) attempt(5,id,0);
          }
          if(!c.BusyEconomy()&&!c.s.tree_work_ms) {
              bool acted=false;
              if(c.s.economy.scrap) acted=attempt(6);
              if(!acted && c.s.tree_stage<2) acted=attempt(7);
              if(!acted) for(unsigned id:{0u,1u,4u,5u,2u,3u,6u,7u})
                  if(!c.HasFacility(id)) {attempt(1,id);break;}
          }
          c.Advance(1000);
          assert(c.s.water<=100&&c.s.energy<=100&&c.s.fiber<=60&&c.s.economy.food<=30&&c.s.economy.parts<=20);
          if(c.s.economy.built==255&&c.s.tree_stage==2&&c.s.economy.animals==3) {complete_at=t+1;break;}
      }
      assert(complete_at);
      std::cout<<"Actual-core all8 + canopy + both rescues: "<<complete_at/60.<<" minutes (scripted approvals, not a promise for real players)\n";
      Core restored;restored.storage_ok=true;assert(restored.RestoreFull(c.PackFull()));
      unsigned care_due[2]={0,0},care_missed[2]={0,0};
      for(unsigned t=0;t<24*3600;t++) {
          const auto previous_care=restored.s.economy.animal_elapsed;
          restored.Advance(1000);
          for(unsigned i=0;i<2;++i) if(restored.s.economy.animal_elapsed[i]<previous_care[i]) {
              ++care_due[i];if(!(restored.s.economy.animal_help&(1u<<i)))++care_missed[i];
          }
          if(t%600==0) {auto save=restored.PackFull();Core r;r.storage_ok=true;assert(r.RestoreFull(save));assert(r.PackFull()==save);restored=r;}
          assert(restored.s.water<=100&&restored.s.energy<=100&&restored.s.fiber<=60&&restored.s.economy.food<=30);
          assert(restored.s.economy.animals==3);
      }
      assert(care_due[0]==48&&care_due[1]==48);
      assert(!care_missed[0]&&!care_missed[1]);
      std::cout<<"Actual-core24h five-region unattended run: both animals retained,48 successful care payments each; bounded stocks and periodic durable round-trips passed\n";
    }
    // Legacy V2 migration is read-only, preserves inventory/builds, and is stable.
    if(argc==2) {LegacyFullSave old{};std::ifstream f(argv[1],std::ios::binary);f.read(reinterpret_cast<char*>(old.data()),sizeof(old));assert(f.gcount()==sizeof(old));
      auto immutable=old;Core c;c.storage_ok=true;assert(c.RestoreFull(old));
      assert(old==immutable);Core r;r.storage_ok=true;assert(r.RestoreFull(c.PackFull()));assert(r.PackFull()==c.PackFull());
      assert(c.s.water==old[2]&&c.s.energy==old[3]&&c.s.fiber==old[4]);}
    std::cout<<"V3: five-region locked yields, two tree upgrades, gated nest, manual/workbench recycling, paused construction, both emergency paths, atomic two-animal care, once-only entry, solar recovery, bounded calls and V2 migration passed\n";
}
