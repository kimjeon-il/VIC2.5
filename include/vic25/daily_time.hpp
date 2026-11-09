#pragma once
// VIC2.5 daily time v2.0 transitional execution layer.
//
// A day is the world-visible commit boundary. Supported real day actions are
// posted into the *existing* Phase C Financial/Inventory/Jurisdiction ledgers,
// and Gregorian calendar rollovers are committed inside the SAME native day
// transaction. No separate month-close callback exists. I/J are design-only.
//
// The immutable-world no-op daily fast path intentionally avoids cloning a
// 30k-Province / 1m-POP simulation on days without a due action.
#include "vic25/phase_gh.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace vic25::daily {
using DayIndex=std::int64_t;
using People=phaseb::People;
inline void require(bool ok,const char* message) {
    if(!ok)throw std::invalid_argument(message);
}
inline constexpr const char* time_contract="vic25-time/2.0";
inline constexpr const char* calendar_contract="proleptic-gregorian/1";

struct Date {
    std::int32_t year{},month{},day{};
    auto operator<=>(const Date&)const=default;
};
inline bool leap(std::int32_t year) {
    return (year%4==0 && year%100!=0) || year%400==0;
}
inline std::int32_t days_in_month(std::int32_t year,std::int32_t month) {
    require(year>=1&&year<=9999&&month>=1&&month<=12,
            "TIME_ERR_INVALID_CALENDAR_DATE");
    constexpr std::array<std::int32_t,12> days{
        31,28,31,30,31,30,31,31,30,31,30,31
    };
    return days[static_cast<std::size_t>(month-1)]+
           ((month==2&&leap(year))?1:0);
}
inline DayIndex to_day_index(Date d) {
    require(d.year>=1&&d.year<=9999&&d.month>=1&&d.month<=12,
            "TIME_ERR_INVALID_CALENDAR_DATE");
    require(d.day>=1&&d.day<=days_in_month(d.year,d.month),
            "TIME_ERR_INVALID_CALENDAR_DATE");
    const auto y=static_cast<DayIndex>(d.year)-1;
    DayIndex index=365*y+y/4-y/100+y/400;
    constexpr std::array<int,12> before{
        0,31,59,90,120,151,181,212,243,273,304,334
    };
    index+=before[static_cast<std::size_t>(d.month-1)];
    if(d.month>2&&leap(d.year))++index;
    return index+d.day-1;
}
inline Date from_day_index(DayIndex index) {
    require(index>=0&&index<=to_day_index({9999,12,31}),
            "TIME_ERR_INVALID_CALENDAR_DATE");
    std::int32_t low=1,high=10000;
    while(low+1<high) {
        const auto mid=static_cast<std::int32_t>((low+high)/2);
        if(to_day_index({mid,1,1})<=index)low=mid;
        else high=mid;
    }
    for(std::int32_t month=1;month<=12;++month) {
        const auto start=to_day_index({low,month,1});
        const auto days=days_in_month(low,month);
        if(index>=start&&index<start+days)
            return {low,month,static_cast<std::int32_t>(index-start+1)};
    }
    throw std::logic_error("Gregorian day decoder failed");
}
inline std::int64_t legacy_month(Date date) {
    (void)to_day_index(date);
    return static_cast<std::int64_t>(date.year)*12 + date.month-1;
}
inline Date from_legacy_month(std::int64_t month) {
    require(month>=12&&month<=9999LL*12+11,
            "TIME_ERR_UNSUPPORTED_LEGACY_MONTH");
    const auto y=static_cast<std::int32_t>(month/12);
    const auto m=static_cast<std::int32_t>(month%12+1);
    return {y,m,1};
}
// The effective law decides the term length. Calendar arithmetic stays
// in the shared date engine, not in political domain-specific shortcuts.
inline Date add_calendar_months(Date date,std::int32_t months) {
    (void)to_day_index(date);
    require(months>=1&&months<=240,"TIME_ERR_INVALID_ELECTION_TERM");
    const std::int64_t zero_based=
        (static_cast<std::int64_t>(date.year)-1)*12+date.month-1+months;
    require(zero_based<(9999LL*12),
            "TIME_ERR_ELECTION_TERM_CALENDAR_OVERFLOW");
    const auto year=static_cast<std::int32_t>(zero_based/12+1);
    const auto month=static_cast<std::int32_t>(zero_based%12+1);
    return {year,month,std::min(date.day,days_in_month(year,month))};
}
inline bool is_month_end(Date d) {
    return d.day==days_in_month(d.year,d.month);
}
inline Date following_day(Date d) {
    const auto index=to_day_index(d);
    require(index<to_day_index({9999,12,31}),"TIME_ERR_CALENDAR_OVERFLOW");
    return from_day_index(index+1);
}

enum class Kind :std::uint8_t {
    payment=1, trade=2, military_control=3,
    pop_birth=4, pop_death=5, production=6,
    construction=7, manufacture=8,
    cargo_dispatch=9, treaty_offer=10, treaty_ratify=11,
    // Append-only: existing daily v2.0 Kind tags are unchanged.
    epidemic=12, consumption=13, tariff=14
};
struct Command {
    std::string id;
    Kind kind{};
    std::string from,to,region,good;
    std::int64_t amount{},unit_price{};
    // v2 fields: a precise target region or diplomatic government,
    // an additional quantity/day-offset, and an actual G transport edge ID.
    std::string destination;
    std::int64_t auxiliary{};
    std::string route_id;
};
struct DailyCargo {
    std::string id,sender,recipient,origin,destination,good,edge_id;
    vic25::Quantity quantity{},loss{};
    DayIndex dispatched{},arrival{};
    bool arrived{};
};
struct Entry {
    DayIndex effective_day{};
    std::string command_id;
    Kind kind{};
    std::uint64_t c_commit_id{};
};
struct DayReport {
    Date executed{};
    Date next{};
    std::uint64_t day_commit{};
    std::uint64_t authoritative_commit{};
    std::size_t actions{};
    bool monthly_close{};
};
inline std::int32_t stage_of(Kind kind) {
    switch(kind) {
        case Kind::payment:return 5;
        case Kind::trade:return 7;
        case Kind::military_control:return 10;
        case Kind::pop_birth:
        case Kind::pop_death:
        case Kind::epidemic:return 10;
        case Kind::consumption:return 7;
        case Kind::tariff:return 0;
        case Kind::production:
        case Kind::manufacture:return 4;
        case Kind::construction:return 1;
        case Kind::cargo_dispatch:return 7;
        case Kind::treaty_offer:
        case Kind::treaty_ratify:return 9;
    }
    throw std::invalid_argument("TIME_ERR_UNSUPPORTED_DAILY_KIND");
}
inline void execute(const Command& command,phasec::IntegratedWorld& c) {
    const auto command_id=std::string("DAY:")+command.id;
    c.command_once(command_id,[&](phasec::IntegratedWorld& w){
        switch(command.kind) {
            case Kind::payment:
                require(!command.from.empty()&&!command.to.empty()&&
                        command.amount>0,"TIME_ERR_INVALID_DAILY_ACTION");
                w.pay(command.from,command.to,command.amount);
                break;
            case Kind::trade:
                require(!command.from.empty()&&!command.to.empty()&&
                        !command.region.empty()&&!command.good.empty()&&
                        command.amount>0&&command.unit_price>0,
                        "TIME_ERR_INVALID_DAILY_ACTION");
                w.trade(command.from,command.to,command.region,
                        command.good,command.amount,command.unit_price);
                break;
            case Kind::military_control:
                require(!command.region.empty()&&!command.to.empty(),
                        "TIME_ERR_INVALID_DAILY_ACTION");
                w.occupy(command.region,command.to);
                break;
            case Kind::pop_birth:
                w.phase_time_birth(static_cast<phasec::Id>(std::stoull(command.from)),
                                   command.amount);
                break;
            case Kind::pop_death:
                w.phase_time_death(static_cast<phasec::Id>(std::stoull(command.from)),
                                   command.amount,command.auxiliary);
                break;
            case Kind::epidemic:
                require(!command.from.empty()&&command.amount>=0&&
                        command.unit_price>=0&&
                        (command.amount>0||command.unit_price>0),
                        "TIME_ERR_INVALID_DAILY_EPIDEMIC");
                w.epidemic(static_cast<phasec::Id>(std::stoull(command.from)),
                           command.amount,command.unit_price);
                break;
            case Kind::consumption:
                require(!command.from.empty()&&!command.region.empty()&&
                        !command.good.empty()&&command.amount>0,
                        "TIME_ERR_INVALID_DAILY_CONSUMPTION");
                w.consume(command.from,command.region,command.good,command.amount);
                break;
            case Kind::tariff:
                throw std::invalid_argument("TIME_ERR_EXPECTED_DIPLOMATIC_STAGE");
            case Kind::production:
                w.produce(command.from,command.region,command.good,command.to,
                          command.amount,command.unit_price);
                break;
            default:
                throw std::invalid_argument("TIME_ERR_EXPECTED_DOMAIN_STAGE");
        }
    });
}

class World;
namespace detail { template<class A,class W> void metadata(A&,W&,int format=2); }
class World {
    friend struct SnapshotCodec;
    template<class A,class W> friend void detail::metadata(A&,W&,int);
    phasegh::World engine_;
    DayIndex day_{};
    std::uint64_t day_commit_{};
    std::uint64_t world_seed_{};
    std::string migration_origin_{"fresh"};
    std::uint64_t migration_checksum_{};
    std::map<DayIndex,std::vector<Command>> scheduled_;
    std::set<std::string> known_command_ids_;
    std::vector<Entry> journal_;
    std::map<std::string,DailyCargo> cargo_;

    explicit World(phasegh::World engine,Date date,std::uint64_t seed,
                   std::string origin,std::uint64_t legacy_checksum)
        :engine_(std::move(engine)),day_(to_day_index(date)),
         world_seed_(seed),migration_origin_(std::move(origin)),
         migration_checksum_(legacy_checksum) {
        require(date.day==1,"TIME_ERR_PARTIAL_MONTH_MIGRATION");
        validate();
    }
public:
    static World adopt(phasegh::World legacy,std::uint64_t seed=0,
                       std::string origin="legacy-gh",
                       std::uint64_t legacy_checksum=0) {
        const auto start=from_legacy_month(legacy.month());
        return World(std::move(legacy),start,seed,std::move(origin),legacy_checksum);
    }
    [[nodiscard]] Date date()const{return from_day_index(day_);}
    [[nodiscard]] DayIndex day_index()const{return day_;}
    [[nodiscard]] std::uint64_t day_commit_id()const{return day_commit_;}
    [[nodiscard]] const phasegh::World& engine()const{return engine_;}
    [[nodiscard]] std::uint64_t world_seed()const{return world_seed_;}
    [[nodiscard]] const std::string& migration_origin()const{return migration_origin_;}
    [[nodiscard]] std::uint64_t migration_checksum()const{return migration_checksum_;}
    [[nodiscard]] const std::vector<Entry>& journal()const{return journal_;}
    [[nodiscard]] const std::map<std::string,DailyCargo>& cargo()const{return cargo_;}
    [[nodiscard]] std::size_t pending_count()const {
        std::size_t n=0;
        for(const auto& [day,commands]:scheduled_) {
            (void)day;
            n+=commands.size();
        }
        return n;
    }
    void validate()const {
        engine_.validate();
        const Date current=date();
        require(legacy_month(current)==engine_.month(),"TIME_ERR_MONTH_ALIGNMENT");
        require(!migration_origin_.empty(),"TIME_ERR_MISSING_MIGRATION_SOURCE");
        std::set<std::string> scheduled_ids;
        for(const auto& [due,commands]:scheduled_) {
            require(due>=day_&&!commands.empty(),"TIME_ERR_PAST_DUE_EVENT");
            for(const auto& command:commands) {
                require(!command.id.empty()&&known_command_ids_.contains(command.id) &&
                        scheduled_ids.insert(command.id).second,
                        "TIME_ERR_DUPLICATE_EVENT");
                (void)stage_of(command.kind);
            }
        }
        DayIndex previous=-1;
        for(const auto& entry:journal_) {
            require(entry.effective_day<day_&&entry.effective_day>=previous &&
                    known_command_ids_.contains(entry.command_id),
                    "TIME_ERR_INVALID_EVENT_JOURNAL");
            previous=entry.effective_day;
        }
        for(const auto& [id,c]:cargo_) {
            require(id==c.id&&!id.empty()&&known_command_ids_.contains(id)&&
                    !c.sender.empty()&&!c.recipient.empty()&&
                    !c.origin.empty()&&!c.destination.empty()&&
                    !c.good.empty()&&c.quantity>0&&c.loss>=0&&
                    c.loss<=c.quantity&&c.arrival>c.dispatched&&
                    c.dispatched<day_ &&
                    (c.arrived?c.arrival<day_:true),
                    "TIME_ERR_CARGO_JOURNAL_INTEGRITY");
            const auto in_transit=engine_.base().core().core().stock(
                    "TRANSIT:DAY:"+id,"in_transit",c.good);
            require(in_transit==(c.arrived?0:c.quantity),
                    "TIME_ERR_CARGO_LEDGER_MISMATCH");
        }
        require(day_commit_<=static_cast<std::uint64_t>(day_),
                "TIME_ERR_COMMIT_COUNTER"); // any fresh 1801+ clock has larger ordinal
    }
    void enqueue(Date effective_date,Command command) {
        const auto due=to_day_index(effective_date);
        require(due>=day_&&!command.id.empty()&&
                !known_command_ids_.contains(command.id),
                "TIME_ERR_DUPLICATE_OR_PAST_EVENT");
        (void)stage_of(command.kind);
        // Only immutable command data is staged until the due day.
        scheduled_[due].push_back(command);
        known_command_ids_.insert(std::move(command.id));
    }

    // Each stage callback executes on today's date. On the last calendar
    // day, POP age/eligibility updates share this exact transaction.
    DayReport advance_day(const phasegh::World::Plan& native_day_plan={}) {
        const auto current=date();
        const bool close=is_month_end(current);
        const bool first_calendar_day=current.day==1;
        const auto next=following_day(current);
        const auto it=scheduled_.find(day_);
        const std::vector<Command> commands=
            it==scheduled_.end()?std::vector<Command>{}:it->second;
        const bool cargo_due=std::any_of(cargo_.begin(),cargo_.end(),
            [&](const auto& p){return !p.second.arrived&&p.second.arrival==day_;});
        const bool treaty_due=engine_.has_due_daily_treaty(day_);
        const bool construction_due=engine_.has_ready_daily_commission();
        if(day_commit_==std::numeric_limits<std::uint64_t>::max() ||
           engine_.commit_id()==std::numeric_limits<std::uint64_t>::max())
            throw std::overflow_error("TIME_ERR_COMMIT_COUNTER");
        require(engine_.month()==legacy_month(current),
                "TIME_ERR_GAME_DATE_OUT_OF_SYNC");
        const bool native_actions=std::any_of(native_day_plan.begin(),native_day_plan.end(),
            [](const auto& row){return !row.empty();});
        if(commands.empty()&&!close&&!first_calendar_day&&
           !native_actions&&!cargo_due&&!treaty_due&&!construction_due &&
           !engine_.base().core().core().daily_auto_demography_enabled()&&
           engine_.base().core().core().annual_bank_interest_ppm()==0&&
           !engine_.base().core().core().has_active_sovereign_bonds()&&
           !engine_.has_autonomous_daily_trade()&&
           !engine_.base().core().core().has_due_p1_government(day_)&&
           !engine_.base().core().core().p4_has_due_amendment(day_)&&
           !engine_.base().core().core().p2_has_due(day_)&&
           !engine_.base().core().core().p2_has_review_due(day_)&&
           !engine_.base().core().core().p4_has_campaign_due(day_)&&
           !engine_.base().core().core().p1_has_character_work()&&
           !engine_.base().core().core().p2_has_negotiable_bills()&&
           !engine_.base().core().core().p3_needs_daily_tick()&&
           !engine_.base().core().core().p5_needs_daily_tick()&&
           !engine_.base().core().core().p5_has_due_daily_strike(day_)&&
           !engine_.base().core().core().p5_guerrilla_has_calendar_work(day_)&&
           engine_.base().core().core().p4_due_contests(day_).empty()) {
            ++day_;++day_commit_;
            return {current,next,day_commit_,engine_.commit_id(),0,false};
        }
        auto next_scheduled=scheduled_;
        auto next_journal=journal_;
        auto next_cargo=cargo_;
        next_scheduled.erase(day_);
        auto ordered=commands;
        std::sort(ordered.begin(),ordered.end(),
            [](const Command& a,const Command& b) {
                if(stage_of(a.kind)!=stage_of(b.kind))
                    return stage_of(a.kind)<stage_of(b.kind);
                return a.id<b.id;
            });
        // Each owner already stages its own ledger transaction. Avoid a
        // redundant complete world copy at the daily calendar boundary.
        phasegh::World::Plan real_actions;
        for(auto& [id,c]:next_cargo) {
            if(c.arrived||c.arrival!=day_)continue;
            real_actions[0].push_back([c](phasegh::Stage& stage) {
                stage.phase_c().phase_time_arrive(
                    c.id,c.recipient,c.destination,c.good,c.quantity,c.loss);
                stage.acknowledge_daily_cargo_arrival(
                    c.id,c.quantity,c.loss);
            });
            c.arrived=true;
        }
        for(const auto& command:ordered) {
            const auto stage=static_cast<std::size_t>(stage_of(command.kind));
            if(command.kind==Kind::construction) {
                // Same real project, C cash reservation, material inventory,
                // POP labor and WIP accounting; no fictional progress points.
                real_actions[1].push_back([command](phasegh::Stage& gh) {
                    gh.phase_d().plan_work(command.from,command.amount);
                });
                real_actions[4].push_back([command](phasegh::Stage& gh) {
                    gh.phase_d().perform_work(command.from,command.amount);
                });
                real_actions[5].push_back([command](phasegh::Stage& gh) {
                    gh.phase_d().pay_work(command.from,command.amount);
                });
                real_actions[8].push_back([command](phasegh::Stage& gh) {
                    gh.phase_d().capitalize_work(command.from,command.amount);
                });
            } else if(command.kind==Kind::manufacture) {
                real_actions[4].push_back([command,current](phasegh::Stage& gh) {
                    gh.phase_d().manufacture_daily(command.from,command.amount,
                          current.day,days_in_month(current.year,current.month));
                });
            } else if(command.kind==Kind::cargo_dispatch) {
                require(command.amount>0&&command.unit_price>0&&
                        command.auxiliary>=0&&command.auxiliary<=command.amount&&
                        !command.destination.empty()&&!command.route_id.empty(),
                        "TIME_ERR_INVALID_CARGO_DISPATCH");
                const auto arrival=day_+command.unit_price;
                require(arrival>day_&&arrival<=to_day_index({9999,12,31}),
                        "TIME_ERR_INVALID_CARGO_ETA");
                require(!next_cargo.contains(command.id),
                        "TIME_ERR_DUPLICATE_CARGO");
                real_actions[1].push_back(
                    [command,current](phasegh::Stage& gh) {
                        gh.reserve_daily_cargo(command.id,command.route_id,
                            command.from,command.region,command.good,
                            command.amount,current.day,
                            days_in_month(current.year,current.month));
                    });
                real_actions[7].push_back(
                    [command,current](phasegh::Stage& gh) {
                        gh.dispatch_daily_cargo(command.id,command.route_id,
                            command.from,command.region,command.destination,
                            command.good,command.amount,current.day,
                            days_in_month(current.year,current.month));
                    });
                next_cargo.emplace(command.id,DailyCargo{
                    command.id,command.from,command.to,command.region,
                    command.destination,command.good,command.route_id,
                    command.amount,command.auxiliary,day_,arrival,false});
            } else if(command.kind==Kind::tariff) {
                // A previously scheduled policy is effective at the start
                // of its designated day, without editing earlier trades.
                require(!command.from.empty()&&!command.to.empty()&&
                        !command.good.empty()&&command.amount>=0&&
                        command.amount<=1'000'000,
                        "TIME_ERR_INVALID_DAILY_TARIFF");
                real_actions[0].push_back([command](phasegh::Stage& gh) {
                    gh.adopt_tariff_daily(command.id,command.from,command.good,
                                          command.amount,command.to);
                });
            } else if(command.kind==Kind::treaty_offer) {
                require(command.auxiliary>0&&
                        day_+command.auxiliary<=to_day_index({9999,12,31}),
                        "TIME_ERR_INVALID_TREATY_EFFECTIVE_DAY");
                real_actions[9].push_back([command,today=day_](phasegh::Stage& gh) {
                    gh.propose_daily_treaty(command.region,command.from,command.to,
                        command.good,command.destination,
                        today,today+command.auxiliary);
                });
            } else if(command.kind==Kind::treaty_ratify) {
                real_actions[9].push_back([command](phasegh::Stage& gh) {
                    gh.ratify_daily_treaty(command.region,command.from,
                                           command.destination);
                });
            } else {
                real_actions[stage].push_back([command](phasegh::Stage& gh) {
                    execute(command,gh.phase_c());
                });
            }
        }
        // Exact birth day and age eligibility must be known at Stage0,
        // before any same-day labor, election or explicit birth command.
        real_actions[0].insert(real_actions[0].begin(),
            [day=current.day,today=day_](phasegh::Stage& gh) {
                gh.phase_c().phase_time_set_calendar_day(day);
                // A lawful change enters on its scheduled calendar day even
                // if no P0 controller, battle or economic command is active.
                gh.phase_c().phase_time_apply_p1_government(today);
                gh.phase_c().phase_time_apply_p5_settlement(today);
                gh.phase_c().phase_time_apply_p4_amendments(today);
                gh.phase_c().phase_time_apply_p2(today);
                gh.phase_c().phase_time_apply_p5_strikes(today);
                gh.phase_c().phase_time_apply_p5_guerrilla_access(today);
                gh.enforce_p2_tariff_ceiling();
            });
        // Stage 9 is ordered deliberately: first observe today's actual POP
        // conditions, then execute P0/player political commands and elections,
        // then settle bargains and political crises. The same staged C/D/E/G/H
        // transaction owns every mutation; no second political clock exists.
        real_actions[9].push_back([today=day_](phasegh::Stage& gh) {
            if(gh.phase_c().p3_needs_daily_tick())
                gh.phase_c().p3_daily_react(today);
        });
        // A political role points to an actual living PersonAnchor. Retire
        // invalid persons and transfer IG leadership before election review.
        real_actions[9].push_back([today=day_](phasegh::Stage& gh) {
            if(gh.phase_c().p1_has_character_work())
                gh.phase_c().p1_advance_characters(today);
        });
        // Registered national IGs and real contest dates determine party
        // formation and campaign momentum before today's ballots are counted.
        real_actions[9].push_back([today=day_](phasegh::Stage& gh) {
            if(gh.phase_c().p4_has_campaign_due(today))
                gh.phase_c().p4_daily_party_campaigns(today);
        });
        // P0 domain decisions run on the real calendar day and may consume
        // the freshly computed POP political response.
        for(std::size_t n=0;n<native_day_plan.size();++n)
            for(const auto& action:native_day_plan[n])
                real_actions[n].push_back(action);
        // Native election fallback. P0 plans its contested election earlier
        // in Stage9 (after P3 political reactions); the due index is then
        // cleared, so the fallback does not count the same ballot twice.
        // A base daily::World without P0 still holds all lawful due elections.
        real_actions[9].push_back([today=day_,current](phasegh::Stage& gh) {
            auto& c=gh.phase_c();
            for(const auto& contest_id:c.p4_due_contests(today)) {
                const auto months=c.p4_contest(contest_id).term_months;
                const auto next=to_day_index(add_calendar_months(current,months));
                c.p4_hold_contest(contest_id,today,next,
                                  c.p4_autonomous_ballot(contest_id));
            }
        });
        // Stage9 interest-group bargaining is conditional on an actually
        // submitted bill and historically/simulation-provided policy demands.
        // It never creates POPs, votes or fictional proposal resources.
        real_actions[9].push_back([today=day_](phasegh::Stage& gh) {
            gh.phase_c().p2_auto_negotiate_pending(today);
        });
        // Only due enactment checkpoints are checked, after P0 negotiation.
        real_actions[9].push_back([today=day_](phasegh::Stage& gh) {
            if(gh.phase_c().p2_has_review_due(today))
                gh.phase_c().p2_review_due(today);
        });
        // Ongoing movements generate warnings and observable responses;
        // actual armed actions still require verified army and equipment.
        real_actions[9].push_back([today=day_](phasegh::Stage& gh) {
            if(gh.phase_c().p5_needs_daily_tick())
                gh.phase_c().p5_daily_advance(today);
        });
        // Revolution readiness uses only actual unit manpower, arms and IG
        // support. A full clock without rebel assets cannot spawn an army.
        real_actions[9].push_back([today=day_](phasegh::Stage& gh) {
            if(gh.phase_c().p5_needs_daily_tick())
                gh.phase_c().p5_auto_prepare_revolutions(today);
        });
        // Only journal due dates run guerrilla review. No daily all-world
        // militia battles or copied guerrilla population and supply ledgers.
        real_actions[9].push_back([today=day_](phasegh::Stage& gh) {
            if(gh.phase_c().p5_guerrilla_has_calendar_work(today))
                gh.phase_c().p5_guerrilla_daily_review(today);
        });
        // Sovereign security obligations accrue and settle on their actual
        // calendar day even when no P0 AI/controller was explicitly attached.
        real_actions[5].push_back([today=day_](phasegh::Stage& gh) {
            gh.phase_c().phase_time_service_sovereign_bonds(today);
        });
        // The autonomous population hazard is calendar-day resolved, even
        // without user actions. The existing monthly rate is NOT replayed
        // at MonthClose.
        real_actions[10].push_back([current](phasegh::Stage& gh) {
            gh.phase_c().phase_time_auto_demography(
                current.day,days_in_month(current.year,current.month));
            // Phase C's Stage10 finalization reconciles P3/P5 POP refs
            // exactly once after all demographic and combat actions.
        });
        // One atomic C/D/E/G/H commit for native events AND calendar
        // rollover. No second mutable month-close pass or event replay.
        const auto next_c_commit=engine_.commit_id()+1;
        // Prepare journal allocations before the domain transaction commits.
        for(const auto& command:ordered)
            next_journal.push_back({day_,command.id,command.kind,
                                    next_c_commit});
        static_assert(std::is_nothrow_move_assignable_v<
                      std::map<DayIndex,std::vector<Command>>>);
        static_assert(std::is_nothrow_move_assignable_v<
                      std::map<std::string,DailyCargo>>);
        static_assert(std::is_nothrow_move_assignable_v<std::vector<Entry>>);
        engine_.advance_daily(real_actions,day_,first_calendar_day,
                              current.day,days_in_month(current.year,current.month),
                              close);
        scheduled_=std::move(next_scheduled);
        journal_=std::move(next_journal);
        cargo_=std::move(next_cargo);
        ++day_;++day_commit_;
        return {current,next,day_commit_,engine_.commit_id(),
                ordered.size()+(cargo_due?1:0),close};
    }
};
} // namespace vic25::daily
