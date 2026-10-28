// Read-only host replay of the ACTUAL Core; all worlds below are test fixtures.
#include "game/spring_json.h"
#include <cassert>
#include <iostream>
using namespace spring;
Core Fresh(unsigned water=16) {
    Core c;c.storage_ok=true;c.EnableEconomy(true);c.Wake();
    c.s.water=water;c.s.energy=12;
    for(auto& p:c.s.plots){p.ready=false;p.growth_ms=p.mature_ms=0;}
    c.s.economy.paid_mask=0;c.s.economy.mature_yield.fill(0);
    return c;
}
void Sample(const Core& c) {
    std::cout<<"{\"second\":"<<c.s.game_ms/1000<<",\"water\":"<<c.s.water<<",\"energy\":"<<c.s.energy<<",\"fiber\":"<<c.s.fiber<<",\"food\":"<<c.s.economy.food<<",\"parts\":"<<c.s.economy.parts<<'}';
}
void Run(const char* name,unsigned seconds,int fixed_region,bool shake,bool animals,bool comma) {
    auto c=Fresh(fixed_region==1?8:16);
    if(animals){c.s.tree_stage=2;c.s.economy.built=(1u<<1)|(1u<<4)|(1u<<7);c.s.economy.animals=3;c.s.economy.food=8;}
    if(fixed_region>=0)c.s.economy.region=fixed_region;
    unsigned harvest=0,water_low=0,stopped=0,food_pauses=0,first_harvest=0,manual=0;uint32_t last=c.s.event_count?c.s.events[c.s.event_count-1].id:0;
    std::array<uint64_t,4> gains{},wasted{};
    if(comma)std::cout<<',';
    std::cout<<"{\"name\":"<<StoryQuote(name)<<",\"fixed_region\":"<<fixed_region<<",\"legal_shake\":"<<(shake?"true":"false")<<",\"samples\":[";Sample(c);
    for(unsigned sec=1;sec<=seconds;++sec){
        if(fixed_region>=0){c.s.economy.region=fixed_region;c.s.economy.region_ms=0;}
        std::array<unsigned,4> kind{},expected{};
        for(unsigned i=0;i<c.PlotCount();++i){kind[i]=unsigned(c.PlotAt(i).plant);expected[i]=c.s.economy.mature_yield[i]?c.s.economy.mature_yield[i]:kRegionYields[c.s.economy.region][kind[i]];}
        c.Step(1000);
        if(shake&&c.work==Work::Rest&&!c.LifeWorkPending())manual+=c.Shake();
        for(unsigned i=0;i<c.s.event_count;++i){const auto& e=c.s.events[i];if(e.id<=last)continue;
            if(e.type==Event::Harvest&&e.target<c.PlotCount()){if(!first_harvest)first_harvest=sec;auto k=kind[e.target];gains[k]+=e.amount;wasted[k]+=expected[e.target]>e.amount?expected[e.target]-e.amount:0;++harvest;}
            if(e.type==Event::AnimalFed)++food_pauses; // Successful care cycles, not inferred from ownership.
        }
        if(c.s.event_count)last=c.s.events[c.s.event_count-1].id;
        water_low+=c.s.water<=4;
        bool blocked=false;for(unsigned i=0;i<c.PlotCount();++i)blocked|=!c.PlotAt(i).ready&&!(c.s.economy.paid_mask&(1u<<i));
        stopped+=blocked;
        assert(c.s.water<=100&&c.s.energy<=100&&c.s.fiber<=60&&c.s.economy.food<=30);
        if(sec%60==0){std::cout<<',';Sample(c);}
    }
    std::cout<<"],\"harvests\":"<<harvest<<",\"first_harvest_seconds\":"<<first_harvest<<",\"manual_harvests\":"<<manual<<",\"low_water_seconds\":"<<water_low<<",\"unpaid_slot_seconds\":"<<stopped<<",\"successful_animal_care\":"<<food_pauses<<",\"harvest_gain\":[";
    for(unsigned i=0;i<4;++i){if(i)std::cout<<',';std::cout<<gains[i];}std::cout<<"],\"harvest_overflow\":[";
    for(unsigned i=0;i<4;++i){if(i)std::cout<<',';std::cout<<wasted[i];}std::cout<<"]}";
}
int main(){
    std::cout<<"{\"source\":\"actual Core, isolated fixtures, no opening mature gifts, no production config changes\",\"resource_order\":[\"energy\",\"water\",\"fiber\",\"food\"],\"scenarios\":[";
    Run("normal_auto_2h",7200,0,false,false,false);Run("normal_shake_2h",7200,0,true,false,true);
    Run("drought_auto_30m",1800,1,false,false,true);Run("drought_shake_30m",1800,1,true,false,true);
    Run("four_slots_two_animals_2h",7200,0,false,true,true);Run("rotating_regions_4h",14400,-1,false,false,true);
    auto off=Fresh();off.s.wall_epoch=1800000000;off.Sync(1800086400);
    assert(off.offline_seconds==43200);
    std::cout<<"],\"offline24h\":{\"elapsed_seconds\":86400,\"actually_simulated_seconds\":"<<off.offline_seconds<<",\"final\":";Sample(off);std::cout<<"}}\n";
}
