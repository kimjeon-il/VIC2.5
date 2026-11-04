#pragma once
// VIC2.5 daily time v2.0 transitional execution layer.
//
// A day is the world-visible commit boundary. Supported real day actions are
// posted into the *existing* Phase C Financial/Inventory/Jurisdiction ledgers,
// while the legacy B/C/D/E/G/H monthly simulation runs once at calendar month
// end, never 28-31 times per month. I/J remain specification-only.
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
inline bool is_month_end(Date d) {
    return d.day==days_in_month(d.year,d.month);
}
inline Date following_day(Date d) {
    const auto index=to_day_index(d);
    require(index<to_day_index({9999,12,31}),"TIME_ERR_CALENDAR_OVERFLOW");
    return from_day_index(index+1);
}

enum class Kind :std::uint8_t { payment=1, trade=2, military_control=3 };
struct Command {
    std::string id;
    Kind kind{};
    std::string from,to,region,good;
    std::int64_t amount{},unit_price{};
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
        }
    });
}

class World;
namespace detail { template<class A,class W> void metadata(A&,W&); }
class World {
    friend struct SnapshotCodec;
    template<class A,class W> friend void detail::metadata(A&,W&);
    phasegh::World engine_;
    DayIndex day_{};
    std::uint64_t day_commit_{};
    std::uint64_t world_seed_{};
    std::string migration_origin_{"fresh"};
    std::uint64_t migration_checksum_{};
    std::map<DayIndex,std::vector<Command>> scheduled_;
    std::set<std::string> known_command_ids_;
    std::vector<Entry> journal_;

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

    // No world clone on an ordinary day with no due actions and no month-close.
    // On event or month-close days the *whole* C/D/E/GH state is staged and
    // published only after all domain/event validation succeeds.
    DayReport advance_day(const phasegh::World::Plan& month_close_plan={}) {
        const auto current=date();
        const bool close=is_month_end(current);
        if(!close)for(const auto& stage:month_close_plan)
            require(stage.empty(),"TIME_ERR_MONTHLY_PLAN_OUTSIDE_CLOSE");
        const auto next=following_day(current);
        const auto it=scheduled_.find(day_);
        const std::vector<Command> commands=
            it==scheduled_.end()?std::vector<Command>{}:it->second;
        if(day_commit_==std::numeric_limits<std::uint64_t>::max())
            throw std::overflow_error("TIME_ERR_COMMIT_COUNTER");
        if(commands.empty()&&!close) {
            ++day_;
            ++day_commit_;
            return {current,next,day_commit_,engine_.commit_id(),0,false};
        }

        // Prepare all metadata before touching a committed world.
        auto next_scheduled=scheduled_;
        auto next_journal=journal_;
        next_scheduled.erase(day_);
        auto ordered=commands;
        std::sort(ordered.begin(),ordered.end(),[](const Command& a,const Command& b) {
            const auto stage_a=stage_of(a.kind),stage_b=stage_of(b.kind);
            if(stage_a!=stage_b)return stage_a<stage_b;
            return a.id<b.id;
        });
        auto candidate=engine_;
        if(!ordered.empty()) {
            phasec::IntegratedWorld::Plan real_actions;
            for(const auto& command:ordered) {
                real_actions[static_cast<std::size_t>(stage_of(command.kind))]
                    .push_back([command](phasec::IntegratedWorld& c) {
                        execute(command,c);
                    });
            }
            candidate.advance_daily(real_actions);
        }
        if(close)candidate.advance(month_close_plan);
        require(candidate.month()==legacy_month(next),
                "TIME_ERR_MONTH_CLOSE_NOT_ALIGNED");
        candidate.validate();
        for(const auto& command:ordered)
            next_journal.push_back({day_,command.id,command.kind,
                                     candidate.commit_id()});

        static_assert(std::is_nothrow_move_assignable_v<phasegh::World>);
        static_assert(std::is_nothrow_move_assignable_v<
                      std::map<DayIndex,std::vector<Command>>>);
        engine_=std::move(candidate);
        scheduled_=std::move(next_scheduled);
        journal_=std::move(next_journal);
        ++day_;
        ++day_commit_;
        return {current,next,day_commit_,engine_.commit_id(),ordered.size(),close};
    }
};
} // namespace vic25::daily
