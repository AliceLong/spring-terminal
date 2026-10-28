#include "game/spring_agency.h"
#include "game/spring_monitor_json.h"
#include "game/spring_social.h"
#include "game/spring_story_authority.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace spring;
Core Fresh(unsigned stage) {
    Core c; c.storage_ok = true; c.EnableEconomy(true); c.EnableMemory(11,22); c.Wake();
    c.s.story.enabled=1; c.s.story.active=kNoStory; c.s.story.language=stage;
    c.s.water=c.s.energy=40;
    for(auto& p:c.s.plots) {p.ready=false;p.growth_ms=p.mature_ms=0;}
    c.s.life.cooldown=0;
    return c;
}
void Fixture(std::ostream& o,const char* name,Core c,AgencyState a,bool comma) {
    MonitorRuntime r; r.uptime_ms=5000;r.voice="idle";r.fsm="idle";r.power="游玩";
    r.mind_boot=r.mind_offer=1;r.mind_reply_deadline=25000;
    for(unsigned i=1;i<=3;++i)for(unsigned t=0;t<(i==2?c.PlotCount():1u);++t)
        if(AgencyMode(c.s.story,i,t)!=0&&c.CanConsiderLocalMindLife(i,t))r.mind_mask|=1u<<((i-1)*4+t);
    r.mind_deadline=r.mind_mask?25000:0;r.mind_sequence=c.s.life.sequence;
    auto character=EmptyCharacter(c.Get());RefreshCharacter(c.Get(),character);auto obs=ObservationJson(c.Get(),character);obs.pop_back();
    r.observation_json=obs+",\"agency\":"+AgencyJson(c.Get(),a)+"}";
    r.communication_json="{\"social\":"+SocialJson(c.Get(),EmptySocial(c.Get()),false)+",\"agency\":"+AgencyJson(c.Get(),a)+"}";
    if(comma)o<<',';o<<"{\"name\":"<<StoryQuote(name)<<",\"simulated\":true,\"snapshot\":"<<MonitorJson(c.Get(),r,99)<<'}';
}
int main() {
    auto zero=Fresh(0);auto a0=EmptyAgency(zero.Get());
    assert(AgencyLevel(zero.s.story)==0&&!OfferWish(zero.Get(),a0,3,0));
    assert(zero.CanConsiderLocalMindLife(1)&&!zero.RequestLocalMindLife(1));
    auto routine=zero;routine.StepLife(100,false,false,true);assert(!PersonalLife(routine.s.life.intent,routine.s.life.target));
    assert(zero.InviteLife(1)); // Explicit invitation remains possible and runs in-call.
    zero.StepLife(100,true,false,false);assert(zero.s.life.phase==2);
    auto one=Fresh(1);auto a=EmptyAgency(one.Get());auto checkpoint=one.PackFull();
    assert(OfferWish(one.Get(),a,3,0));unsigned token=a.current.token;
    assert(one.PackFull()==checkpoint&&!one.LifeActive());
    assert(!StageWishApproval(one,a,token)); // Click before presentation cannot approve.
    assert(PresentWish(a,token));assert(!one.LifeActive());
    const WishReplyContext later_voice{true,true,13,12};
    assert(DecideWishReply(one.Get(),a,token,1,later_voice)==WishReplyDecision::Approve);
    assert(DecideWishReply(one.Get(),a,token,1,{true,true,12,12})==WishReplyDecision::Rejected);
    assert(DecideWishReply(one.Get(),a,token,1,{true,false,13,12})==WishReplyDecision::Rejected);
    assert(DecideWishReply(one.Get(),a,token,1,{false,false,0,UINT32_MAX})==WishReplyDecision::Approve);
    assert(DecideWishReply(one.Get(),a,token,0,later_voice)==WishReplyDecision::Rejected);
    assert(AgencyObservationJson(one.Get(),EmptyCharacter(one.Get()),a,true).find("\"active\":true")!=std::string::npos);
    assert(!CanApproveWish(one.Get(),a,token+1));
    auto busy=one;busy.work=Work::Water;auto next=a;assert(!StageWishApproval(busy,next,token));
    assert(next.current.status==unsigned(WishStatus::Presented));
    assert(StageWishApproval(one,a,token));auto seq=one.s.life.sequence;
    assert(a.current.sequence==seq&&a.current.chain&&one.LifeActive());
    assert(DecideWishReply(one.Get(),a,token,1,later_voice)==WishReplyDecision::AlreadyExecuting);
    assert(DecideWishReply(one.Get(),a,token,1,{true,false,13,12})==WishReplyDecision::Rejected);
    auto wrong_world=one;wrong_world.WriteMemory().world_hi=99;
    assert(DecideWishReply(wrong_world.Get(),a,token,1,later_voice)==WishReplyDecision::Rejected);
    assert(!StageWishApproval(one,a,token)); // Repeat cannot move twice.
    one.StepLife(100,true,false,false);one.StepLife(100,true,true,false);
    for(int i=0;i<65;++i)one.StepLife(1000,true,true,false);
    assert(!one.LifeActive());assert(RefreshAgency(one.Get(),a));
    assert(a.current.status==unsigned(WishStatus::Completed)&&a.current.reached&&a.count==1);
    auto interrupted=Fresh(1);auto ai=EmptyAgency(interrupted.Get());
    assert(OfferWish(interrupted.Get(),ai,1,0)&&PresentWish(ai,ai.current.token));
    assert(StageWishApproval(interrupted,ai,ai.current.token));
    interrupted.work=Work::Water;interrupted.StepLife(100,false,false,false);
    assert(RefreshAgency(interrupted.Get(),ai)&&ai.current.status==unsigned(WishStatus::Interrupted)&&!ai.current.reached);
    auto restart=Fresh(1);auto ar=EmptyAgency(restart.Get());assert(OfferWish(restart.Get(),ar,3,0));
    assert(RecoverPendingWish(ar)&&ar.current.status==unsigned(WishStatus::RestartCancelled)&&!restart.LifeActive());
    auto pending=Fresh(1);auto ap=EmptyAgency(pending.Get());assert(OfferWish(pending.Get(),ap,1,0));
    assert(!OfferWish(pending.Get(),ap,3,0));assert(PresentWish(ap,ap.current.token));
    FinishWish(ap,WishStatus::Declined);assert(!StageWishApproval(pending,ap,ap.current.token));
    auto other=pending;other.WriteMemory().world_hi=99;
    assert(!AgencyMatches(other.Get(),ap));
    for(unsigned stage=2;stage<=3;++stage){auto c=Fresh(stage);assert(AgencyLevel(c.s.story)==1&&!c.RequestLocalMindLife(3));}
    auto corrupt=ap;corrupt.current.status=99;assert(!AgencyMatches(pending.Get(),corrupt));
    assert(!FreshVoiceConfirmation(12,12)&&FreshVoiceConfirmation(13,12));
    std::ofstream out("tests/.build/v56-agency-fixtures.json");out<<'[';
    auto f0=Fresh(0),f1=Fresh(1);Fixture(out,"stage0",f0,EmptyAgency(f0.Get()),false);
    Fixture(out,"stage1",f1,EmptyAgency(f1.Get()),true);
    auto af=EmptyAgency(f1.Get());assert(OfferWish(f1.Get(),af,3,0));Fixture(out,"stage1_pending",f1,af,true);
    auto after=Fresh(1);assert(after.InviteLife(2,0));after.StepLife(100,false,false,false);after.StepLife(100,false,true,false);
    for(int i=0;i<21;++i)after.StepLife(1000,false,true,false);after.s.life.cooldown=0;
    Fixture(out,"stage1_after_inspection",after,EmptyAgency(after.Get()),true);out<<']';
    std::cout<<"Agency: stage gates, click != approval, idempotence, work interruption, completion evidence, reboot and world isolation pass\n";
}
