#pragma once
#include "vic25/daily_time.hpp"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace vic25::p0 {

// P0 is an opt-in policy layer over the existing day coordinator. It creates no
// alternate money, inventory, population, diplomatic influence or battle ledger.
// All consequences are ordinary Phase C/D/E/G/H staged events.
using Money=phasec::Money;
using Quantity=vic25::Quantity;
using People=phaseb::People;
using Id=phaseb::Id;

inline Quantity minimum(Quantity a,Quantity b) { return std::min(a,b); }
inline Quantity proportional(Quantity n,std::int32_t day,std::int32_t days) {
    if(n<=0||day<=0||days<28||days>31||day>days)
        return 0;
    // No multiplying n by day: integer quotient/remainder avoids overflow.
    return (n/days)*day + ((n%days)*day)/days;
}
inline Quantity daily_increment(Quantity n,int day,int days) {
    return proportional(n,day,days)-proportional(n,day-1,days);
}
inline Money times(Money a,Money b) {return phaseb::mul_checked(a,b);}

struct Staffing {
    std::string employer,region;
    People wanted{};
    Money monthly_wage{};
};
struct Manufacturer {
    std::string recipe;
    Quantity target_inventory{};
    // Optional actual payroll; zero means a legacy employer already pays.
    std::string employee_wallet,treasury_wallet;
    Money wage_per_worker_per_day{};
};
struct Buyer {
    Id population_slice{};
    std::string household_actor,seller,region,good;
    Quantity units_per_person_per_day{1};
};
struct Investment {
    std::string construction_project,output_good;
    Money reserve_cash{},minimum_output_price{};
    Quantity maximum_daily_work{1};
};
struct PoliticalReaction {
    Id population_slice{};
    std::string region,interest_group;
    People max_new_members_per_day{1};
    People max_protesters_per_day{1};
};
// Existing C election implementation stores the last election globally. Until
// its nationalized election registry lands, P0 accepts at most one election
// schedule per world, rather than silently replacing another nation's result.
struct Election {
    std::string country,incumbent_party,reform_party;
    std::int32_t year{},month{1},day{1};
    People turnout_ppm{900'000};
};
struct CountryAI {
    std::string country,prime_minister,domestic_region,good,domestic_seller;
    Money protect_below_price{};
    Quantity surplus_stock_threshold{};
    std::int64_t protective_tariff_ppm{100'000};
};
struct Engagement {
    std::string attacker,defender,region;
    Id attacker_slice{},defender_slice{};
    // Scenario terrain is a bounded, explicit input, NOT fabricated GIS data.
    std::int64_t defender_terrain_ppm{1'000'000};
};

struct Policies {
    std::vector<Staffing> staffing;
    std::vector<Manufacturer> manufacturers;
    std::vector<Buyer> buyers;
    std::vector<Investment> investments;
    std::vector<PoliticalReaction> political_reactions;
    std::vector<Election> elections;
    std::vector<CountryAI> countries;
    std::vector<Engagement> engagements;

    void validate() const {
        std::set<std::string> seen_firms,seen_projects,seen_countries;
        std::set<std::string> engaged;
        for(const auto& f:manufacturers)
            if(f.recipe.empty() || f.target_inventory<0 ||
               f.wage_per_worker_per_day<0 || !seen_firms.insert(f.recipe).second ||
               (f.wage_per_worker_per_day>0 &&
                (f.employee_wallet.empty()||f.treasury_wallet.empty())))
                throw std::invalid_argument("P0_INVALID_MANUFACTURER");
        for(const auto& s:staffing)
            if(s.employer.empty()||s.region.empty()||s.wanted<0||s.monthly_wage<0)
                throw std::invalid_argument("P0_INVALID_STAFFING");
        for(const auto& b:buyers)
            if(!b.population_slice||b.household_actor.empty()||b.seller.empty()||
               b.region.empty()||b.good.empty()||b.units_per_person_per_day<=0)
                throw std::invalid_argument("P0_INVALID_BUYER");
        for(const auto& i:investments)
            if(i.construction_project.empty()||i.output_good.empty()||
               i.reserve_cash<0||i.minimum_output_price<=0||
               i.maximum_daily_work<=0||!seen_projects.insert(i.construction_project).second)
                throw std::invalid_argument("P0_INVALID_INVESTMENT");
        for(const auto& p:political_reactions)
            if(!p.population_slice||p.region.empty()||p.interest_group.empty()||
               p.max_new_members_per_day<0||p.max_protesters_per_day<0)
                throw std::invalid_argument("P0_INVALID_POLITICS");
        if(elections.size()>1)throw std::invalid_argument("P0_SINGLE_ELECTION_REGISTRY");
        for(const auto& e:elections)
            if(e.country.empty()||e.incumbent_party.empty()||e.reform_party.empty()||
               e.incumbent_party==e.reform_party||e.year<1||e.year>9999||
               e.month<1||e.month>12||e.day<1||
               e.day>daily::days_in_month(e.year,e.month)||
               e.turnout_ppm<0||e.turnout_ppm>1'000'000)
                throw std::invalid_argument("P0_INVALID_ELECTION");
        for(const auto& c:countries)
            if(c.country.empty()||c.prime_minister.empty()||
               c.domestic_region.empty()||c.good.empty()||
               c.domestic_seller.empty()||c.protect_below_price<=0||
               c.protective_tariff_ppm<0||c.protective_tariff_ppm>1'000'000||
               !seen_countries.insert(c.country+":"+c.good).second)
                throw std::invalid_argument("P0_INVALID_COUNTRY_AI");
        for(const auto& b:engagements) {
            if(b.attacker.empty()||b.defender.empty()||
               b.attacker==b.defender||b.region.empty()||
               !b.attacker_slice||!b.defender_slice||
               b.attacker_slice==b.defender_slice||
               b.defender_terrain_ppm<500'000||
               b.defender_terrain_ppm>2'000'000||
               !engaged.insert(b.attacker).second||
               !engaged.insert(b.defender).second)
                throw std::invalid_argument("P0_INVALID_ENGAGEMENT");
        }
    }
};

inline void hire_available(phasec::IntegratedWorld& c,const Staffing& p) {
    People need=p.wanted-c.phase_d_workforce(p.employer);
    if(need<=0)return;
    // This is not population creation. Only an eligible, truly uncontracted
    // regional slice can be hired; no cross-firm job overwrite.
    for(const auto& [id,s]:c.people().slices()) {
        if(need<=0)break;
        const auto& demo=c.people().demographic(s.key.demographic);
        if(demo.region!=p.region||c.people().job(id)!=nullptr)continue;
        const auto& econ=c.people().economic(s.key.economic);
        if(econ.profession!="worker"&&econ.profession!="unskilled_worker")continue;
        if(!c.people().eligible_for(id,phaseb::JobRequirements{}))continue;
        const People n=std::min(s.count,need);
        if(n>0) {
            c.hire(id,n,p.employer,p.monthly_wage);
            need-=n;
        }
    }
}

inline Quantity available_production(const phased::Stage& d,
                                      const phasec::IntegratedWorld& c,
                                      const Manufacturer& policy,
                                      int day,int days) {
    const auto& state=d.state();
    const auto& r=state.recipes.at(policy.recipe);
    const auto& f=c.facility(r.site);
    if(f.capacity<=f.damaged||r.outputs.empty()||
       r.needs_retrofit||r.requires_same_stage_nonstorable_energy)
        return 0;
    if(!r.tech_id.empty()) {
        auto used=state.active_site_tech.find(r.site);
        if(used==state.active_site_tech.end()||used->second!=r.tech_id)return 0;
    }
    const auto& primary=r.outputs.front();
    if(c.stock(f.owner,f.region,primary.good)>=policy.target_inventory)return 0;
    const auto& quote=c.market(f.region,primary.good);
    Money cost_per_batch=0;
    for(const auto& input:r.inputs) {
        const auto& iq=c.market(f.region,input.good);
        cost_per_batch=phaseb::add_checked(cost_per_batch,times(input.per_unit,iq.price));
    }
    const Money revenue=times(primary.per_unit,quote.price);
    if(revenue<=cost_per_batch)return 0;
    const Quantity stock_shortfall=policy.target_inventory-c.stock(f.owner,f.region,primary.good);
    Quantity batches=stock_shortfall/primary.per_unit+
        (stock_shortfall%primary.per_unit!=0?1:0);
    Quantity used=0;
    for(const auto& run:state.actual_production_runs) {
        if(run.month!=d.month())continue;
        const auto& past=state.recipes.at(run.recipe);
        if(past.site==r.site)
            used=phaseb::add_checked(used,times(run.batches,past.capacity_per_batch));
    }
    const Quantity allowed=proportional(f.capacity-f.damaged,day,days);
    if(allowed<=used)return 0;
    batches=std::min(batches,(allowed-used)/r.capacity_per_batch);
    People labor=c.phase_d_workforce(f.owner)-c.phase_d_shared_labor_used(f.owner);
    if(labor<=0)return 0;
    batches=std::min(batches,labor/r.workers_per_batch);
    for(const auto& input:r.inputs)
        batches=std::min(batches,c.phase_d_material_available(f.owner,f.region,input.good)/input.per_unit);
    if(policy.wage_per_worker_per_day>0)
        batches=std::min(batches,c.phase_d_cash_available(f.owner)/
            times(r.workers_per_batch,policy.wage_per_worker_per_day));
    return std::max<Quantity>(0,batches);
}

inline phasec::CombatLoss tactical_losses(People victim,
                          Quantity enemy_ammo,Quantity enemy_rifles,
                          std::int64_t terrain_ppm,bool victim_attacking) {
    const Quantity force=phaseb::add_checked(enemy_ammo,
                             std::min(enemy_ammo,enemy_rifles));
    // Deliberately bounded tactical proxy, not an invented full warfare AI.
    Quantity impact=force/5;
    if(impact==0&&force>0)impact=1;
    const auto terrain=static_cast<Quantity>(terrain_ppm);
    impact=victim_attacking ?
           (impact/1'000'000)*terrain+
              (impact%1'000'000)*terrain/1'000'000:
           (impact/terrain)*1'000'000+
              (impact%terrain)*1'000'000/terrain;
    impact=std::min<Quantity>(victim,impact);
    phasec::CombatLoss loss;
    loss.killed=impact/4;
    loss.acute=impact-loss.killed;
    return loss;
}

inline void fight(phasec::IntegratedWorld& c,const Engagement& e) {
    if(!c.people().has_slice(e.attacker_slice)||!c.people().has_slice(e.defender_slice))return;
    const People a=c.active_strength(e.attacker);
    const People b=c.active_strength(e.defender);
    if(a<=0||b<=0)return;
    const People a_slice=c.people().slice(e.attacker_slice).count;
    const People b_slice=c.people().slice(e.defender_slice).count;
    if(a!=a_slice||b!=b_slice)return; // bounded one-exclusively-assigned-slice test
    const Quantity ammo_a=std::min<Quantity>(a,c.stock(e.attacker,e.region,"ammunition"));
    const Quantity ammo_b=std::min<Quantity>(b,c.stock(e.defender,e.region,"ammunition"));
    if(ammo_a<=0||ammo_b<=0)return;
    const auto rifle_a=c.assigned_equipment(e.attacker,e.region,"rifle");
    const auto rifle_b=c.assigned_equipment(e.defender,e.region,"rifle");
    auto al=tactical_losses(a,ammo_b,rifle_b,e.defender_terrain_ppm,true);
    auto bl=tactical_losses(b,ammo_a,rifle_a,e.defender_terrain_ppm,false);
    al.ammunition=ammo_a;
    bl.ammunition=ammo_b;
    // Both loss counts are derived from the same pre-battle committed state.
    c.battle(e.attacker,e.attacker_slice,e.region,al);
    c.battle(e.defender,e.defender_slice,e.region,bl);
}

class DailyGame {
    daily::World world_;
    Policies policies_;
public:
    DailyGame(daily::World world,Policies policies)
      :world_(std::move(world)),policies_(std::move(policies)) {policies_.validate();}
    [[nodiscard]] const daily::World& world()const {return world_;}
    [[nodiscard]] const Policies& policies()const {return policies_;}
    // Campaign/save data owns the policy definitions; base day snapshots remain
    // fully compatible and replay exactly when the same policy set is restored.
    daily::DayReport advance_day(const phasegh::World::Plan& month_close_plan={}) {
        const auto date=world_.date();
        const int dim=daily::days_in_month(date.year,date.month);
        phasegh::World::Plan plan;
        for(const auto& ai:policies_.countries) {
            plan[0].push_back([ai,date](phasegh::Stage& gh) {
                const auto& c=gh.phase_c();
                if(c.sovereignty(ai.domestic_region)!=ai.country)return;
                const auto& quote=c.market(ai.domestic_region,ai.good);
                const auto available=c.stock(ai.domestic_seller,ai.domestic_region,ai.good);
                const auto duty=(quote.price<ai.protect_below_price &&
                                 available>=ai.surplus_stock_threshold)?
                                 ai.protective_tariff_ppm:std::int64_t{0};
                const auto key=std::make_pair(ai.country,ai.good);
                auto prior=gh.diplomacy().tariffs.find(key);
                if(prior!=gh.diplomacy().tariffs.end()&&prior->second.ppm==duty)return;
                gh.adopt_tariff_daily("P0:daily-tariff:"+ai.country+":"+ai.good+":"+
                    std::to_string(date.year)+"-"+std::to_string(date.month)+"-"+
                    std::to_string(date.day),ai.country,ai.good,duty,ai.prime_minister);
            });
        }
        for(const auto& investment:policies_.investments) {
            auto work=std::make_shared<Quantity>(0);
            plan[1].push_back([investment,work](phasegh::Stage& gh){
                const auto& domain=gh.phase_d().state();
                const auto& p=domain.constructions.at(investment.construction_project);
                auto& c=gh.phase_c();
                if(p.ready||p.commissioned||p.accepted_work>=p.work_required ||
                   c.market(p.region,investment.output_good).price<investment.minimum_output_price)
                    return;
                const Money free=c.phase_d_cash_available(p.client);
                if(free<=investment.reserve_cash)return;
                const Quantity materials=c.phase_d_material_available(
                              p.contractor,p.region,p.material_good)/p.material_per_work;
                const People staff=c.phase_d_workforce(p.contractor);
                const Quantity affordable=(free-investment.reserve_cash)/p.cash_price_per_work;
                const Quantity n=std::min({investment.maximum_daily_work,
                     p.work_required-p.accepted_work,materials,
                     static_cast<Quantity>(staff/p.workers_per_work),affordable});
                if(n<=0)return;
                gh.phase_d().plan_work(investment.construction_project,n);
                *work=n;
            });
            plan[4].push_back([investment,work](phasegh::Stage& gh){
                if(*work>0)gh.phase_d().perform_work(investment.construction_project,*work);
            });
            plan[5].push_back([investment,work](phasegh::Stage& gh){
                if(*work>0)gh.phase_d().pay_work(investment.construction_project,*work);
            });
            plan[8].push_back([investment,work](phasegh::Stage& gh){
                if(*work>0)gh.phase_d().capitalize_work(investment.construction_project,*work);
            });
        }
        for(const auto& staffing:policies_.staffing)
            plan[2].push_back([staffing](phasegh::Stage& gh){
                hire_available(gh.phase_c(),staffing);
            });
        for(const auto& firm:policies_.manufacturers) {
            auto workers=std::make_shared<People>(0);
            plan[4].push_back([firm,date,dim,workers](phasegh::Stage& gh) {
                auto& c=gh.phase_c();
                const Quantity batches=available_production(gh.phase_d(),c,firm,date.day,dim);
                if(batches<=0)return;
                const auto& r=gh.phase_d().state().recipes.at(firm.recipe);
                gh.phase_d().manufacture_daily(firm.recipe,batches,date.day,dim);
                *workers=times(batches,r.workers_per_batch);
            });
            if(firm.wage_per_worker_per_day>0)
                plan[5].push_back([firm,workers](phasegh::Stage& gh){
                    if(*workers<=0)return;
                    const auto& r=gh.phase_d().state().recipes.at(firm.recipe);
                    const auto& f=gh.phase_c().facility(r.site);
                    const Money gross=times(*workers,firm.wage_per_worker_per_day);
                    gh.phase_c().wages_under_law(f.owner,firm.employee_wallet,
                                                  firm.treasury_wallet,gross);
                });
        }
        for(const auto& b:policies_.buyers) {
            plan[7].push_back([b](phasegh::Stage& gh){
                auto& c=gh.phase_c();
                if(!c.people().has_slice(b.population_slice))return;
                const auto& row=c.people().slice(b.population_slice);
                const auto& d=c.people().demographic(row.key.demographic);
                if(d.region!=b.region)return;
                const Quantity demand=times(row.count,b.units_per_person_per_day);
                if(demand>0) {
                    const Quantity purchased=c.clear_consumer_market(
                        b.seller,b.household_actor,b.region,b.good,demand);
                    if(purchased>0)c.consume(b.household_actor,b.region,b.good,purchased);
                }
            });
        }
        for(const auto& p:policies_.political_reactions) {
            plan[9].push_back([p](phasegh::Stage& gh){
                auto& c=gh.phase_c();
                if(!c.people().has_slice(p.population_slice) ||
                   c.people().demographic(
                       c.people().slice(p.population_slice).key.demographic).region!=p.region)
                    return;
                const People shortage=c.food_grievance(p.region);
                if(shortage<=0)return;
                const auto total=c.people().slice(p.population_slice).count;
                const People existing=c.group_members(p.population_slice,p.interest_group);
                const People new_members=std::min({p.max_new_members_per_day,
                                    shortage,total-existing});
                if(new_members>0)c.organize_from_food_grievance(
                                      p.population_slice,p.interest_group,new_members);
                const People remaining=total-c.protest_participants(p.population_slice);
                const People protest=std::min({remaining,
                     c.group_members(p.population_slice,p.interest_group),
                     p.max_protesters_per_day});
                if(protest>0)c.protest(p.population_slice,p.interest_group,protest);
            });
        }
        for(const auto& election:policies_.elections) {
            if(date.year!=election.year||date.month!=election.month||date.day!=election.day)continue;
            plan[9].push_back([election](phasegh::Stage& gh){
                auto& c=gh.phase_c();
                const auto& pop=c.people();
                const auto& g=c.government(election.country);
                People incumbent=0,reform=0;
                for(const auto& [id,s]:pop.slices()){
                    const auto& d=pop.demographic(s.key.demographic);
                    if(c.sovereignty(d.region)!=election.country||d.citizenship!="citizen"||
                       d.service==phaseb::Service::active||d.service==phaseb::Service::pow||
                       d.service==phaseb::Service::missing||
                       pop.age_months(s.key.demographic)<static_cast<std::int64_t>(c.voting_age())*12)
                        continue;
                    const People voters=times(s.count,election.turnout_ppm)/1'000'000;
                    const People unemployed=s.count-pop.employed(id);
                    // Actual unemployment, observed food deprivation and tax burden
                    // drive reform support. No arbitrary political point ledger.
                    const People joblessness=times(unemployed,500'000)/1'000'000;
                    const People scarcity=c.food_grievance(d.region)>0?voters/5:0;
                    const People taxes=times(voters,c.income_tax_ppm())/4'000'000;
                    const People preference=std::min(voters,
                       phaseb::add_checked(joblessness,phaseb::add_checked(scarcity,taxes)));
                    reform=phaseb::add_checked(reform,preference);
                    incumbent=phaseb::add_checked(incumbent,voters-preference);
                }
                const People registered=phaseb::add_checked(incumbent,reform);
                if(registered>0)
                    c.hold_election(registered,0,g.chamber_seats,
                         {{election.incumbent_party,incumbent},
                          {election.reform_party,reform}});
            });
        }
        for(const auto& combat:policies_.engagements)
            plan[10].push_back([combat](phasegh::Stage& gh){
                fight(gh.phase_c(),combat);
            });
        plan[11].push_back([day=date.day](phasegh::Stage& gh){
            gh.phase_c().phase_time_close_daily_markets(day);
        });
        return world_.advance_day(month_close_plan,plan);
    }
};
} // namespace vic25::p0
