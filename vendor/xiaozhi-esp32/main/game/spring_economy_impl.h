// Included inside Core. All actions are deterministic and saveable.
bool HasFacility(unsigned id) const { return id < 8 && (s.economy.built & (1u << id)); }
bool BusyEconomy() const {
    return s.economy.construction != 255 || s.economy.recycle_ms || s.economy.journey_work ||
           s.economy.emergency_work;
}
uint32_t WaterCapacity() const { return s.economy.enabled ? (HasFacility(0) ? 100 : 60) : 99; }
uint32_t EnergyCapacity() const { return s.economy.enabled ? (HasFacility(3) ? 100 : 60) : 99; }
void Gain(uint32_t& pool, uint32_t amount, uint32_t cap) {
    if (pool < cap)
        pool += std::min<uint32_t>(amount, cap - pool);
}
void EnableEconomy(bool fresh = false) {
    if (s.economy.enabled)
        return;
    s.economy.enabled = true;
    if (fresh) {
        s.water = 16;
        s.energy = 12;
        s.economy.paid_mask = 0;
    } else {
        s.economy.paid_mask = 7;
        for (auto& p : s.plots)
            p.growth_ms = p.ready ? 600000u
                                  : static_cast<uint32_t>(std::min<uint64_t>(
                                        600000, uint64_t(p.growth_ms) * 600000 /
                                                    kGrowMs[static_cast<unsigned>(p.plant)]));
        for (unsigned i = 0; i < s.event_count; ++i) {
            const auto& r = s.events[i];
            if (r.type != Event::Harvest && r.type != Event::Matured &&
                r.type != Event::Irrigated && s.economy.decision_count < 12)
                s.economy.decisions[s.economy.decision_count++] = {r.id, r.second, r.type, r.target,
                                                                   r.amount};
        }
    }  // Existing growth was already paid under the old rules.
    ++s.revision;
}
bool EconomyHarvest(unsigned slot, bool irrigate, MemoryActor actor = MemoryActor::Robot) {
    if (!storage_ok || slot >= PlotCount() || !PlotAt(slot).ready || s.economy.emergency_work)
        return false;
    if (s.energy)
        s.economy.round_worked = true;
    auto& p = PlotAt(slot);
    const auto kind = p.plant;
    uint32_t amount = s.economy.mature_yield[slot]
                          ? s.economy.mature_yield[slot]
                          : kRegionYields[s.economy.region][static_cast<unsigned>(kind)];
    if (s.economy.boost_paid & (1u << slot))
        ++amount;
    if (irrigate && kind == Plant::Moss) {
        for (unsigned i = 0; i < PlotCount(); ++i)
            if (!PlotAt(i).ready && (s.economy.paid_mask & (1u << i)))
                PlotAt(i).growth_ms = std::min<uint32_t>(600000u, PlotAt(i).growth_ms + 30000);
        Add(Event::Irrigated, slot, 30);
    } else {
        auto& pool = kind == Plant::Light  ? s.energy
                     : kind == Plant::Moss ? s.water
                     : kind == Plant::Food ? s.economy.food
                                           : s.fiber;
        const auto before = pool;
        Gain(pool, amount,
             kind == Plant::Light  ? EnergyCapacity()
             : kind == Plant::Moss ? WaterCapacity()
             : kind == Plant::Food ? 30
                                   : 60);
        Add(Event::Harvest, slot, pool - before, actor);
    }
    s.economy.mature_yield[slot] = 0;
    p.plant = p.next;
    MemoryPlantApplied(slot);
    p.ready = false;
    p.growth_ms = p.mature_ms = 0;
    ++p.cycle;
    s.economy.paid_mask &= ~(1u << slot);
    s.economy.boost_paid &= ~(1u << slot);
    s.economy.discovered_plants |= 1u << static_cast<unsigned>(p.plant);
    if (dew_slot == static_cast<int>(slot)) {
        dew_slot = -1;
        dew_path = 0;
        tilt_armed = false;
    }
    work = Work::Harvest;
    target = slot;
    s.work_ms = 2500;
    return true;
}
bool Propose(uint32_t kind, uint32_t item, uint32_t value = 0,
             MemoryActor actor = MemoryActor::Robot) {
    if (!storage_ok || !s.economy.enabled || !s.story_stage || !kind || kind > 7)
        return false;
    if ((kind == 1 && item >= 8) || (kind == 2 && (item >= PlotCount() || value >= 4)) ||
        (kind == 3 && item >= 4) || (kind == 4 && item != 255 && item >= PlotCount()) ||
        (kind == 5 && (!s.economy.journey || item != s.economy.journey_id || value > 1)))
        return false;
    if (s.economy.proposal_seq == UINT32_MAX)
        return false;
    s.economy.proposal = {kind, item, value, ++s.economy.proposal_seq};
    MemoryProposed(s.economy.proposal, actor);
    s.notices &= ~32u;
    s.announced &= ~32u;
    Add(Event::ProposalCreated, kind, item);
    return true;
}
bool DismissProposal(uint32_t token) {
    if (!storage_ok || !s.economy.proposal.kind || s.economy.proposal.token != token)
        return false;
    MemoryRejected(token);
    s.economy.proposal = {};
    s.notices &= ~32u;
    s.announced &= ~32u;
    Add(Event::PlanChanged, 255, token);
    return true;
}
bool CanRecoveryPlant(unsigned slot, unsigned value) const {
    return slot < PlotCount() && !(s.economy.paid_mask & (1u << slot)) && !PlotAt(slot).ready &&
           ((!s.energy && value == 0) || (s.water < 2 && s.energy && value == 1));
}
bool CanConfirm(uint32_t token) const {
    const auto& e = s.economy;
    const auto& p = e.proposal;
    if (!storage_ok || !p.kind || p.token != token)
        return false;
    switch (static_cast<ProposalKind>(p.kind)) {
        case ProposalKind::Build: {
            if (p.target >= 8 || HasFacility(p.target) || BusyEconomy() || s.tree_work_ms)
                return false;
            const auto& b = kFacilities[p.target];
            return (p.target != 7 || s.tree_stage >= 2) && s.water >= b.water &&
                   s.energy >= b.energy && s.fiber >= b.fiber && e.parts >= b.parts;
        }
        case ProposalKind::Plant:
            return p.target < PlotCount() && p.value < 4;
        case ProposalKind::Goal:
            return p.target < 4;
        case ProposalKind::Boost:
            return HasFacility(6) && (p.target == 255 || (p.target < PlotCount() &&
                                                          PlotAt(p.target).next != Plant::Light));
        case ProposalKind::Tree:
            return s.tree_stage < 2 && !s.tree_work_ms && !BusyEconomy() &&
                   s.water >= TreeWater(s.tree_stage) && s.fiber >= TreeFiber(s.tree_stage);
        case ProposalKind::Salvage:
            return e.scrap && s.energy >= (HasFacility(5) ? 2u : 1u) && !BusyEconomy() &&
                   !s.tree_work_ms;
        case ProposalKind::Journey:
            if (!e.journey || p.target != e.journey_id)
                return false;
            if (!p.value)
                return true;
            if (BusyEconomy() || s.tree_work_ms)
                return false;
            if (e.journey == 1)
                return s.energy >= 2;
            if (e.journey == 2 || e.journey == 6)
                return true;
            if (e.journey == 3)
                return HasFacility(1);
            if (!HasFacility(7))
                return false;
            if (e.journey == 4)
                return !(e.animals & 1) && e.food >= 2 && s.water >= 2;
            return (e.animals & 1) && !(e.animals & 2) && e.food >= 4 && s.water >= 2;
        default:
            return false;
    }
}
bool ConfirmProposal(uint32_t token) {
    if (!CanConfirm(token))
        return false;
    auto& e = s.economy;
    const auto p = e.proposal;
    const bool immediate = p.kind == 2 && CanRecoveryPlant(p.target, p.value);
    const auto memory_job = MemoryApproved(p);
    // Consume token before any work starts. Repeated confirmations cannot spend twice.
    e.proposal = {};
    s.notices &= ~32u;
    s.announced &= ~32u;
    switch (static_cast<ProposalKind>(p.kind)) {
        case ProposalKind::Build: {
            const auto& b = kFacilities[p.target];
            s.water -= b.water;
            s.energy -= b.energy;
            s.fiber -= b.fiber;
            e.parts -= b.parts;
            e.construction = p.target;
            e.construction_ms = 1;
            Add(Event::FacilityStarted, p.target);
            break;
        }
        case ProposalKind::Plant:
            if (CanRecoveryPlant(p.target, p.value)) {
                auto& plot = PlotAt(p.target);
                plot.plant = plot.next = static_cast<Plant>(p.value);
                plot.growth_ms = plot.mature_ms = 0;
                e.mature_yield[p.target] = 0;
                e.discovered_plants |= 1u << p.value;
                Add(Event::Planted, p.target, p.value);
            } else
                Select(p.target, static_cast<Plant>(p.value));
            break;
        case ProposalKind::Goal: {
            auto it = std::find(e.priorities.begin(), e.priorities.end(), p.target);
            std::rotate(e.priorities.begin(), it, it + 1);
            e.recommendation_stamp = 0;
            Add(Event::PlanChanged, p.target);
            break;
        }
        case ProposalKind::Boost:
            e.boost = p.target;
            Add(Event::PlanChanged, 6, p.target);
            break;
        case ProposalKind::Tree:
            if (!BuildTree())
                return false;
            break;
        case ProposalKind::Salvage:
            if (HasFacility(5))
                s.energy -= 2;
            --e.scrap;
            e.recycle_yield = HasFacility(5) ? 2 : 1;
            e.recycle_ms = HasFacility(5) ? 60000 : 180000;
            Add(Event::PlanChanged, 5);
            break;
        case ProposalKind::Journey: {
            const auto journey = e.journey;
            if (p.value) {
                e.journey_task = journey;
                e.journey_work = journey == 1 ? 120000 : 60000;
                if (journey == 1)
                    s.energy -= 2;
                if (journey == 4 || journey == 5) {
                    e.food -= journey == 4 ? 2 : 4;
                    s.water -= 2;
                }
            }
            Add(Event::JourneyResolved, journey, p.value);
            e.journey = 0;
            e.journey_read = 0;
            s.notices &= ~16u;
            s.announced &= ~16u;
            break;
        }
        default:
            return false;
    }
    MemoryDecisionResult(p, memory_job, immediate);
    return true;
}
bool IncomingEnergy() const {
    if (HasFacility(2))
        return true;
    for (unsigned i = 0; i < PlotCount(); ++i)
        if (PlotAt(i).plant == Plant::Light &&
            (PlotAt(i).ready || (s.economy.paid_mask & (1u << i))))
            return true;
    return false;
}
unsigned MissingParts() const {
    unsigned need = 0;
    for (unsigned i = 0; i < 8; ++i)
        if (!HasFacility(i) && s.economy.construction != i)
            need += kFacilities[i].parts;
    const unsigned available = s.economy.parts + s.economy.scrap * (HasFacility(5) ? 2 : 1);
    return need > available ? need - available : 0;
}
unsigned ChooseJourney() {
    auto& e = s.economy;
    const unsigned seq = e.journey_seq++;
    if (HasFacility(7) && !(e.animals & 1))
        return 4;
    if (HasFacility(7) && (e.animals & 1) && !(e.animals & 2) && seq % 2)
        return 5;
    if (MissingParts() && seq % 2 == 0)
        return 1;
    unsigned candidate = (seq / 2) % 6 + 1;
    if (candidate == 4 && (!e.bird_visit || (e.animals & 1)))
        candidate = 2;
    if (candidate == 5 && (!(e.animals & 1) || (e.animals & 2)))
        candidate = 6;
    return candidate;
}
void AdvanceEconomy(uint32_t ms) {
    ms = std::min<uint32_t>(ms, 43200000u);
    auto& e = s.economy;
    while (ms) {
        // Split exactly at deterministic boundaries for online/offline equivalence.
        uint32_t dt = std::min<uint32_t>(ms, 1000u);
        dt = std::min(dt, 600000u - e.round_ms);
        dt = std::min(dt, 1800000u - e.region_ms);
        ms -= dt;
        s.game_ms += dt;
        e.round_ms += dt;
        e.region_ms += dt;
        if (e.region_ms == 1800000) {
            e.region_ms = 0;
            e.region = (e.region + 1) % 5;
            if (e.region == 2 && HasFacility(1))
                Gain(s.water, 6, WaterCapacity());
            Add(Event::RegionChanged, e.region);
        }
        if (e.round_ms == 600000) {
            e.round_ms = 0;
            if (HasFacility(1))
                Gain(s.water, 1, WaterCapacity());
            if (HasFacility(2))
                Gain(s.energy, 1, EnergyCapacity());
            if (e.round_worked && s.energy)
                --s.energy;
            e.round_worked = false;
        }
        if (s.work_ms) {
            s.work_ms -= std::min(s.work_ms, dt);
            if (!s.work_ms)
                work = Work::Rest;
        }
        e.emergency_cooldown -= std::min(e.emergency_cooldown, dt);
        if (!e.emergency_work && !e.emergency_cooldown && !s.energy && s.water < 2 &&
            !IncomingEnergy()) {
            e.emergency_work = 300000;
            Add(Event::EmergencyStarted);
        }
        const bool emergency = e.emergency_work != 0;
        if (emergency) {
            work = Work::Build;
            e.emergency_work -= std::min(e.emergency_work, dt);
            if (!e.emergency_work) {
                Gain(s.water, 2, WaterCapacity());
                e.emergency_cooldown = 1800000;
                Add(Event::EmergencyDone, 0, 2);
                work = Work::Rest;
            }
        }
        for (unsigned i = 0; i < 2; ++i)
            if (e.animals & (1u << i)) {
                e.animal_elapsed[i] += dt;
                if (e.animal_elapsed[i] >= 1800000) {
                    e.animal_elapsed[i] -= 1800000;
                    e.animal_help &= ~(1u << i);
                    const unsigned food = i ? 3 : 2;
                    if (e.food >= food && s.water) {
                        e.food -= food;
                        --s.water;
                        e.animal_help |= 1u << i;
                        Add(Event::AnimalFed, i);
                        if (!i && HasFacility(1))
                            Gain(s.water, e.region == 2 ? 5 : 3, WaterCapacity());
                    }
                }
            }
        std::array<unsigned, 4> order{0, 1, 2, 3};
        auto rank = [&](unsigned i) {
            const auto p = PlotAt(i).plant;
            if (s.water < 10 && p == Plant::Moss)
                return 0;
            if (s.energy < 6 && p == Plant::Light)
                return 1;
            if ((e.priorities[0] == 1 || e.priorities[0] == 2) && p == Plant::Vine)
                return 2;
            if (e.priorities[0] == 3 && p == Plant::Food)
                return 2;
            return 3;
        };
        std::sort(order.begin(), order.begin() + PlotCount(), [&](unsigned a, unsigned b) {
            return rank(a) == rank(b) ? a < b : rank(a) < rank(b);
        });
        for (unsigned n = 0; n < PlotCount(); ++n) {
            const unsigned i = order[n];
            auto& p = PlotAt(i);
            if (p.ready) {
                p.mature_ms = std::min<uint32_t>(45000, p.mature_ms + dt);
                if (!emergency && !PlantWorkActive() &&
                    (!(LifeActive() && s.life.reason == unsigned(LifeReason::PlayerInvitation) &&
                       s.schedule.pause == ScheduleBlocker::None) ||
                     (p.plant == Plant::Light && !s.energy) ||
                     (p.plant == Plant::Moss && s.water < 2)) &&
                    p.mature_ms >= 45000 && !s.work_ms) {
                    const bool ordinary = s.energy != 0;
                    if (EconomyHarvest(i, false) && ordinary)
                        e.round_worked = true;
                }
                continue;
            }
            if (!(e.paid_mask & (1u << i))) {
                const unsigned water = p.plant == Plant::Moss ? 0 : 2,
                               energy = p.plant == Plant::Light ? 0 : 1;
                if (emergency || (!s.energy && p.plant != Plant::Light) || s.water < water ||
                    s.energy < energy)
                    continue;
                const bool ordinary = s.energy != 0;
                s.water -= water;
                s.energy -= energy;
                e.paid_mask |= 1u << i;
                if (ordinary)
                    e.round_worked = true;
                if (HasFacility(6) && e.boost == i && p.plant != Plant::Light && s.energy) {
                    --s.energy;
                    e.boost_paid |= 1u << i;
                }
            }
            p.growth_ms = std::min<uint32_t>(600000, p.growth_ms + dt);
            if (p.growth_ms == 600000) {
                p.ready = true;
                p.mature_ms = 0;
                e.mature_yield[i] = kRegionYields[e.region][static_cast<unsigned>(p.plant)];
                Add(Event::Matured, i, static_cast<unsigned>(p.plant));
            }
        }
        // Essential harvest takes a short safe break in a long task, preserving all timers.
        const bool harvesting = work == Work::Harvest && s.work_ms;
        if (!emergency && !harvesting && s.energy && !PlantWorkExecuting()) {
            if (e.construction != 255) {
                work = Work::Build;
                e.round_worked = true;
                e.construction_ms += dt;
                if (e.construction_ms >= kFacilities[e.construction].ms) {
                    const auto id = e.construction;
                    e.built |= 1u << id;
                    e.construction = 255;
                    e.construction_ms = 0;
                    Add(Event::FacilityBuilt, id);
                    work = Work::Rest;
                }
            } else if (e.recycle_ms) {
                work = Work::Build;
                e.round_worked = true;
                e.recycle_ms -= std::min(e.recycle_ms, dt);
                if (!e.recycle_ms) {
                    Gain(e.parts, e.recycle_yield, 20);
                    Add(Event::Recycled, 0, e.recycle_yield);
                    e.recycle_yield = 0;
                    work = Work::Rest;
                }
            } else if (e.journey_work) {
                work = Work::Build;
                e.round_worked = true;
                e.journey_work -= std::min(e.journey_work, dt);
                if (!e.journey_work) {
                    const auto journey = e.journey_task;
                    e.journey_task = 0;
                    if (journey == 1) {
                        unsigned amount = 1;
                        if (e.animal_help & 2) {
                            if (++e.mouse_help_explorations >= 2) {
                                ++amount;
                                e.mouse_help_explorations = 0;
                            }
                        }
                        ++e.scavenges;
                        Gain(e.scrap, amount, 20);
                    }
                    if (journey == 2)
                        Gain(s.water, 4, WaterCapacity());
                    if (journey == 6)
                        Gain(e.food, 2, 30);
                    if (journey == 3)
                        Gain(s.water, 6, WaterCapacity());
                    if (journey == 4 || journey == 5) {
                        const unsigned animal = journey - 4;
                        e.animals |= 1u << animal;
                        e.discovered_animals |= 1u << animal;
                        e.animal_elapsed[animal] = 0;
                        e.animal_help &= ~(1u << animal);
                    }
                    Add(Event::JourneyCompleted, journey);
                    work = Work::Rest;
                }
            } else if (s.tree_work_ms) {
                work = Work::Build;
                e.round_worked = true;
                s.tree_work_ms += dt;
                if (s.tree_work_ms >= TreeDuration(s.tree_stage)) {
                    s.tree_work_ms = 0;
                    ++s.tree_stage;
                    s.story_stage = 2;
                    Notice(4);
                    Add(Event::TreeGrown, 0, s.tree_stage);
                    work = Work::Rest;
                }
            } else if (!s.work_ms && !LifeActive() && !PlantWorkActive() &&
                       s.game_ms % 20000 < dt) {
                work = Work::Water;
                target = order[(s.game_ms / 20000) % PlotCount()];
                s.work_ms = kWaterWorkMs;
                e.round_worked = true;
            }
        } else if (!emergency && !harvesting && !PlantWorkExecuting()) {
            work = Work::Rest;
            s.work_ms = 0;
        }
        if (s.tree_stage >= 1 && !e.bird_visit) {
            e.bird_visit = 1;
            e.discovered_animals |= 1;
            Add(Event::BirdVisited);  // Visit is not ownership or income.
        }
        e.journey_clock = std::min<uint32_t>(1800000, e.journey_clock + dt);
        if (e.journey_clock >= 1800000 && !e.pending_journey) {
            e.pending_journey = ChooseJourney();
            e.journey_clock = 0;
        }
        if (!e.journey && !e.journey_work && e.pending_journey) {
            e.journey = e.pending_journey;
            e.pending_journey = 0;
            ++e.journey_id;
            e.journey_read = 0;
            if (e.journey == 4 || e.journey == 5)
                e.discovered_animals |= 1u << (e.journey - 4);
            s.notices |= 16;
            s.announced &= ~16u;
            Add(Event::JourneyFound, e.journey, e.journey_id);
        }
        if (!e.proposal.kind && !BusyEconomy() && !s.tree_work_ms && s.water >= 6 &&
            s.energy >= 4) {
            unsigned kind = 0, item = 0, stamp = 0;
            if (e.priorities[0] == 1 && s.tree_stage < 2 &&
                s.water >= TreeWater(s.tree_stage) + 6 && s.fiber >= TreeFiber(s.tree_stage)) {
                kind = 7;
                stamp = 100 + s.tree_stage;
            }
            if (e.priorities[0] == 2) {
                for (unsigned id : {0u, 2u, 4u, 1u, 3u, 5u, 6u, 7u})
                    if (!HasFacility(id) && (id != 7 || s.tree_stage >= 2)) {
                        const auto& b = kFacilities[id];
                        if (s.water >= b.water + 6 && s.energy >= b.energy + 4 &&
                            s.fiber >= b.fiber && e.parts >= b.parts) {
                            kind = 1;
                            item = id;
                            stamp = 200 + id;
                        }
                        break;
                    }
            }
            if (e.priorities[0] == 3 && s.tree_stage >= 2 && !HasFacility(7)) {
                const auto& b = kFacilities[7];
                if (s.water >= b.water + 6 && s.fiber >= b.fiber && e.parts >= b.parts) {
                    kind = 1;
                    item = 7;
                    stamp = 307;
                }
            }
            if (kind && stamp != e.recommendation_stamp) {
                Propose(kind, item);
                e.recommendation_stamp = stamp;
                s.notices |= 32;
                s.announced &= ~32u;
            }
        }
        const unsigned epoch = s.game_ms / 1800000;
        if (e.warning_epoch != epoch) {
            e.warning_epoch = epoch;
            e.warning_mask = 0;
        }
        unsigned reason = e.emergency_work                                 ? 3
                          : !s.energy && (BusyEconomy() || s.tree_work_ms) ? 2
                                                                           : 0;
        if (!reason && e.region_ms >= 1740000) {
            const unsigned next = (e.region + 1) % 5;
            int water_net = HasFacility(1) ? 1 : 0;
            for (unsigned i = 0; i < PlotCount(); ++i) {
                const auto plant = PlotAt(i).next;
                if (plant == Plant::Moss)
                    water_net += kRegionYields[next][1];
                else
                    water_net -= 2;
            }
            if (water_net < 0 && s.water < unsigned(-water_net * 3 + 4))
                reason = 1;
        }
        if (reason && !(s.notices & 8) && !(e.warning_mask & (1u << reason))) {
            e.warning_reason = reason;
            e.warning_mask |= 1u << reason;
            s.accepted &= ~8u;
            s.notices |= 8;
            s.announced &= ~8u;
            Add(Event::Shortage, reason);
        }
        MemoryTaskPauses();
        ++s.revision;
    }
}
FullSave PackFull() const {
    FullSave out{};
    auto legacy = Pack();
    std::copy(legacy.begin(), legacy.end(), out.begin());
    size_t n = 128;
    auto put = [&](uint32_t v) { out[n++] = v; };
    put(0x45434f32);
    put(4);
    const auto& e = s.economy;
    put(e.enabled);
    put(e.journey_task);
    put(e.recommendation_stamp);
    put(e.food);
    put(e.parts);
    put(e.built);
    put(e.construction);
    put(e.construction_ms);
    put(e.boost);
    put(e.paid_mask);
    put(e.boost_paid);
    const auto& p = e.fourth;
    put(static_cast<unsigned>(p.plant));
    put(static_cast<unsigned>(p.next));
    put(p.growth_ms);
    put(p.mature_ms);
    put(p.cycle);
    put(p.ready);
    for (auto v :
         {e.animals, e.animal_help, e.scrap, e.recycle_ms, e.journey_work, e.journey_clock,
          e.round_ms, e.emergency_ms, e.journey, e.journey_id, e.journey_seq, e.journey_read,
          e.scavenges, e.proposal_seq, e.discovered_plants, e.discovered_animals})
        put(v);
    for (auto v : e.priorities) {
        put(v);
    }
    put(e.proposal.kind);
    put(e.proposal.target);
    put(e.proposal.value);
    put(e.proposal.token);
    for (auto v : e.animal_elapsed)
        put(v);
    put(e.decision_count);
    for (const auto& d : e.decisions) {
        put(d.id);
        put(d.second);
        put(static_cast<unsigned>(d.type));
        put(d.target);
        put(d.amount);
    }
    for (auto v : {e.region, e.region_ms, e.emergency_work, e.emergency_cooldown, e.pending_journey,
                   e.recycle_yield, e.bird_visit, e.mouse_help_explorations})
        put(v);
    put(e.round_worked);
    for (auto v : e.mature_yield)
        put(v);
    put(e.warning_epoch);
    put(e.warning_mask);
    put(e.warning_reason);
    put(static_cast<unsigned>(PlantWorkActive() ? Work::Rest : work));
    put(static_cast<unsigned>(target));
    put(0x4c494645);
    const auto& l = s.life;
    for (auto v : {l.intent, l.phase, l.reason, l.target, l.plant, l.under_tree, l.reached,
                   l.elapsed, l.travel_ms, l.pause_ms, l.cooldown, l.sequence})
        put(v);
    for (auto v : l.repeat_cooldown)
        put(v);
    put(l.count);
    for (const auto& d : l.records)
        for (auto v : {d.id, d.intent, d.target, d.plant, d.reason, d.result, d.second,
                       d.under_tree, d.reached})
            put(v);
    // All earlier fields occupy words 0..325. This bounded extension preserves save size.
    n = 326;
    put(0x43483132);
    const auto& c = s.chapters;
    for (auto v :
         {c.version, c.flags, c.start_second, c.end_second, c.start_water, c.end_water,
          c.start_energy, c.end_energy, c.manual_harvests, c.requested_cares, c.requested_harvests,
          c.automatic_harvests, c.last_requested_slot, c.drought_chain, c.observation_sequence,
          c.bench_sequence, c.observation_evidence, c.bench_evidence, c.invitation_evidence})
        put(v);
    out.back() = FullChecksum(out);
    return out;
}
template <size_t N>
static uint32_t FullChecksum(const std::array<uint32_t, N>& words) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i + 1 < words.size(); ++i)
        for (unsigned shift = 0; shift < 32; shift += 8) {
            h ^= (words[i] >> shift) & 255;
            h *= 16777619;
        }
    return h;
}
bool RestoreFull(const FullSave& words) {
    if (words[128] != 0x45434f32 || (words[129] != 2 && words[129] != 3 && words[129] != 4) ||
        words.back() != FullChecksum(words))
        return false;
    Core next;
    Save legacy{};
    std::copy_n(words.begin(), 128, legacy.begin());
    if (!next.Restore(legacy, true))
        return false;
    size_t n = 130;
    auto get = [&]() { return words[n++]; };
    auto& e = next.s.economy;
    auto enabled = get();
    e.enabled = enabled;
    e.journey_task = get();
    e.recommendation_stamp = get();
    e.food = get();
    e.parts = get();
    e.built = get();
    e.construction = get();
    e.construction_ms = get();
    e.boost = get();
    e.paid_mask = get();
    e.boost_paid = get();
    auto& p = e.fourth;
    p.plant = static_cast<Plant>(get());
    p.next = static_cast<Plant>(get());
    p.growth_ms = get();
    p.mature_ms = get();
    p.cycle = get();
    auto ready = get();
    p.ready = ready;
    uint32_t* fields[] = {&e.animals,           &e.animal_help,  &e.scrap,
                          &e.recycle_ms,        &e.journey_work, &e.journey_clock,
                          &e.round_ms,          &e.emergency_ms, &e.journey,
                          &e.journey_id,        &e.journey_seq,  &e.journey_read,
                          &e.scavenges,         &e.proposal_seq, &e.discovered_plants,
                          &e.discovered_animals};
    for (auto* f : fields)
        *f = get();
    for (auto& v : e.priorities) {
        v = get();
    }
    e.proposal = {get(), get(), get(), get()};
    if (enabled != 1 || ready > 1 || e.food > 30 || e.parts > 20 || e.built > 255 ||
        e.animals > 3 || e.animal_help > 3 || e.scrap > 20 || e.journey > 6 || e.journey_task > 6 ||
        (bool(e.journey_work) != bool(e.journey_task)) || e.paid_mask > 15 || e.boost_paid > 15 ||
        (e.construction != 255 && e.construction >= 8) ||
        (e.boost != 255 && e.boost >= next.PlotCount()) || e.round_ms >= 600000 ||
        e.emergency_ms >= 1800000 || e.journey_clock > 1800000 || e.recycle_ms > 180000 ||
        e.journey_work > 120000 || static_cast<unsigned>(p.plant) > 3 ||
        static_cast<unsigned>(p.next) > 3 || p.growth_ms > 600000 || p.mature_ms > 45000 ||
        !p.cycle || e.proposal.kind > 7 || e.proposal.token > e.proposal_seq ||
        e.discovered_plants > 15 || e.discovered_animals > 3)
        return false;
    if ((e.construction == 255 && e.construction_ms) ||
        (e.construction != 255 &&
         (e.construction_ms >= kFacilities[e.construction].ms || next.HasFacility(e.construction))))
        return false;
    for (auto& v : e.animal_elapsed) {
        v = get();
        if (v >= 1800000)
            return false;
    }
    e.decision_count = get();
    if (e.decision_count > 12)
        return false;
    uint32_t previous_id = 0;
    for (unsigned i = 0; i < e.decisions.size(); ++i) {
        auto& d = e.decisions[i];
        d.id = get();
        d.second = get();
        d.type = static_cast<Event>(get());
        d.target = get();
        d.amount = get();
        if (i < e.decision_count) {
            if (!d.id || d.id <= previous_id || d.id >= next.s.next_event ||
                d.second > next.s.game_ms / 1000 || static_cast<unsigned>(d.type) >= 23)
                return false;
            previous_id = d.id;
        }
    }
    const auto& pending = e.proposal;
    if (pending.kind > 7)
        return false;
    if (pending.kind &&
        (!pending.token || (pending.kind == 1 && pending.target >= 8) ||
         (pending.kind == 2 && (pending.target >= next.PlotCount() || pending.value >= 4)) ||
         (pending.kind == 3 && pending.target >= 4) ||
         (pending.kind == 4 && pending.target != 255 && pending.target >= next.PlotCount()) ||
         (pending.kind == 5 &&
          (!e.journey || pending.target != e.journey_id || pending.value > 1))))
        return false;
    if ((e.construction != 255 ? 1 : 0) + (e.recycle_ms ? 1 : 0) + (e.journey_work ? 1 : 0) +
            (next.s.tree_work_ms ? 1 : 0) >
        1)
        return false;
    unsigned seen = 0;
    for (auto v : e.priorities) {
        if (v >= 4 || (seen & (1u << v)))
            return false;
        seen |= 1u << v;
    }
    if (words[129] >= 3) {
        uint32_t* extension[] = {
            &e.region,          &e.region_ms,     &e.emergency_work, &e.emergency_cooldown,
            &e.pending_journey, &e.recycle_yield, &e.bird_visit,     &e.mouse_help_explorations};
        for (auto* v : extension)
            *v = get();
        const auto worked = get();
        e.round_worked = worked;
        for (auto& v : e.mature_yield) {
            v = get();
            if (v > 8)
                return false;
        }
        e.warning_epoch = get();
        e.warning_mask = get();
        e.warning_reason = get();
        const auto saved_work = get(), saved_target = get();
        if (saved_work > 3 || saved_target >= next.PlotCount())
            return false;
        next.work = static_cast<Work>(saved_work);
        next.target = saved_target;
        if (e.warning_epoch > next.s.game_ms / 1800000 || e.warning_mask > 14 ||
            e.warning_reason > 3)
            return false;
        if (e.region >= 5 || e.region_ms >= 1800000 || e.emergency_work > 300000 ||
            e.emergency_cooldown > 1800000 || e.pending_journey > 6 || e.recycle_yield > 2 ||
            bool(e.recycle_ms) != bool(e.recycle_yield) || e.bird_visit > 1 ||
            e.mouse_help_explorations > 1 || worked > 1)
            return false;
    } else {
        if (next.s.tree_work_ms && next.s.tree_stage == 1)
            next.s.tree_work_ms *= 2;
        if (e.construction == 1 || e.construction == 7)
            e.construction_ms = std::max<uint32_t>(1u, e.construction_ms * 2 / 3);
        if (e.construction == 4)
            e.construction_ms = std::max<uint32_t>(1u, e.construction_ms / 2);
        e.recycle_yield = e.recycle_ms ? 2 : 0;
        e.bird_visit = next.s.tree_stage ? 1 : 0;
        for (unsigned i = 0; i < next.PlotCount(); ++i)
            if (next.PlotAt(i).ready)
                e.mature_yield[i] =
                    next.PlotAt(i).plant == Plant::Light || next.PlotAt(i).plant == Plant::Moss ? 6
                                                                                                : 3;
    }
    if (words[129] == 4) {
        if (get() != 0x4c494645)
            return false;
        auto& l = next.s.life;
        uint32_t* life_fields[] = {&l.intent,    &l.phase,      &l.reason,   &l.target,
                                   &l.plant,     &l.under_tree, &l.reached,  &l.elapsed,
                                   &l.travel_ms, &l.pause_ms,   &l.cooldown, &l.sequence};
        for (auto* v : life_fields)
            *v = get();
        if (l.intent > 3 || l.phase > 4 || bool(l.intent) != bool(l.phase) || l.reason > 5 ||
            (l.intent && l.reason != l.intent && l.reason != 4 && l.reason != 5) ||
            (l.target >= next.PlotCount() && !(l.intent == 2 && l.target >= 4 && l.target <= 6)) ||
            l.plant > 3 || l.under_tree > 2 || l.reached > 1 ||
            (l.under_tree && (l.intent != 3 || (l.under_tree == 1 && !next.s.tree_stage))) ||
            l.elapsed >= 20000 || l.travel_ms > 30000 || l.pause_ms > 60000 ||
            l.cooldown > 120000 || (l.intent && !l.sequence) || (l.phase == 3 && !l.reached))
            return false;
        for (auto& v : l.repeat_cooldown) {
            v = get();
            if (v > 300000)
                return false;
        }
        l.count = get();
        if (l.count > l.records.size())
            return false;
        uint32_t last_id = 0;
        for (unsigned i = 0; i < l.records.size(); ++i) {
            auto& d = l.records[i];
            d = {get(), get(), get(), get(), get(), get(), get(), get(), get()};
            if (i < l.count &&
                (!d.id || d.id <= last_id || d.id > l.sequence || !d.intent || d.intent > 3 ||
                 (d.reason != d.intent && d.reason != 4 && d.reason != 5) ||
                 (d.target >= next.PlotCount() &&
                  !(d.intent == 2 && d.target >= 4 && d.target <= 6)) ||
                 d.plant > 3 || d.result > 4 || d.second > next.s.game_ms / 1000 ||
                 d.under_tree > 2 || d.reached > 1 || (!d.result && !d.reached)))
                return false;
            last_id = d.id;
        }
        next.life_recovered = l.intent != 0;
    }
    if (words[326] == 0x43483132) {
        n = 327;
        auto& c = next.s.chapters;
        uint32_t* fields[] = {&c.version,
                              &c.flags,
                              &c.start_second,
                              &c.end_second,
                              &c.start_water,
                              &c.end_water,
                              &c.start_energy,
                              &c.end_energy,
                              &c.manual_harvests,
                              &c.requested_cares,
                              &c.requested_harvests,
                              &c.automatic_harvests,
                              &c.last_requested_slot,
                              &c.drought_chain,
                              &c.observation_sequence,
                              &c.bench_sequence,
                              &c.observation_evidence,
                              &c.bench_evidence,
                              &c.invitation_evidence};
        for (auto* field : fields)
            *field = get();
        if (c.version > 1 || (c.flags & ~127u) ||
            (c.last_requested_slot != 255 && c.last_requested_slot > 3) ||
            c.start_second > next.s.game_ms / 1000 || c.end_second > next.s.game_ms / 1000 ||
            (ChapterHas(c, DroughtEnded) &&
             (!ChapterHas(c, DroughtStarted) || c.end_second < c.start_second)))
            return false;
    } else if (words[326] != 0) {
        return false;
    }
    next.storage_ok = storage_ok;
    next.demo = demo;
    *this = next;
    return true;
}

bool RestoreFull(const LegacyFullSave& legacy) {
    if (legacy[129] != 2 || legacy.back() != FullChecksum(legacy))
        return false;
    FullSave expanded{};
    std::copy(legacy.begin(), legacy.end(), expanded.begin());
    expanded.back() = FullChecksum(expanded);
    return RestoreFull(expanded);
}
