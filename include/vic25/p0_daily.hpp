#pragma once
#include "vic25/phase_f.hpp"
#include "vic25/day_budget.hpp"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
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
    if(n<=0||day<=0||days<28||days>31||day>days)return 0;
    return daily_budget::cumulative(n,day,days);
}
inline Quantity daily_increment(Quantity n,int day,int days) {
    if(n<=0||day<=0||days<28||days>31||day>days)return 0;
    return daily_budget::increment(n,day,days);
}
inline Money times(Money a,Money b) {return phaseb::mul_checked(a,b);}

// A site has one physical capacity across manufacturing, mining and research.
// All three owner policies must see the same already-used monthly allowance.
inline Quantity used_site_capacity(const phased::DomainState& domain,
                                   const std::string& site,std::int64_t month) {
    Quantity used=0;
    for(const auto& run:domain.actual_production_runs) {
        if(run.month!=month)continue;
        const auto& recipe=domain.recipes.at(run.recipe);
        if(recipe.site==site)
            used=phaseb::add_checked(used,
                times(run.batches,recipe.capacity_per_batch));
    }
    for(const auto& [id,source]:domain.resource_sites) {
        (void)id;
        if(source.site==site)
            used=phaseb::add_checked(used,
                times(source.produced_this_month,source.site_service_per_unit));
    }
    for(const auto& evidence:domain.research_evidence) {
        if(evidence.month!=month)continue;
        if(domain.researches.at(evidence.project).lab_site==site)
            used=phaseb::add_checked(used,evidence.units);
    }
    return used;
}

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
// Autonomous owner supply target: real resource-site output, not a fictional
// world resource stock. Wages, when configured, use the original C ledger.
struct Extractor {
    std::string resource_site;
    Quantity target_inventory{};
    Money wage_per_unit{};
    std::string employee_wallet,treasury_wallet;
};
struct ResearchPolicy {
    std::string project;
    Quantity maximum_daily_work{1};
};
struct Buyer {
    Id population_slice{};
    std::string household_actor,seller,region,good;
    Quantity units_per_person_per_day{1};
};
// Real owner repair decision, with a strict daily labor/material budget.
// Autonomous credit principal repayment uses the canonical C bank ledger;
 // no separate debt balance or generated repayment funds.
struct DebtService {
    std::string borrower;
    Money minimum_cash_reserve{},max_daily_principal{};
};
struct FacilityMaintenance {
    std::string facility,building_good;
    Quantity max_daily_restoration{1},material_per_unit{1};
    People workers_per_unit{1};
    Money wage_per_worker_per_day{};
    std::string employee_wallet,treasury_wallet;
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
// National ballot targets use the country-specific Phase C/E election
// registry. Elections of multiple countries can occur on the same date
// without sharing a seat or voter-count ledger.
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
// Government strategy rules are scenario inputs, not independent diplomacy,
 // funds or population state. Actual negotiations/dues use existing H/C ledgers.
struct CrisisMandate {
    std::string country,prime_minister;
    Money maximum_cash_settlement{};
    bool accept_claim_withdrawal{true};
    bool mediate_neutral_crises{false};
};
struct OrganizationDuesMandate {
    std::string country,prime_minister,organization;
};
// H autonomous ratification is conditional on real fiscal resources and
// observed treaty obligations; an AI must not invent or sign unknown clauses.
struct TreatyDecisionMandate {
    std::string country,prime_minister;
    Money maximum_new_fiscal_obligation{};
    bool allow_trade_constraints{};
    bool allow_military_commitments{};
    bool allow_territorial_cession{};
};
struct CouncilMemberRule {
    std::string country,prime_minister;
    Money minimum_free_treasury_for_yes{};
};
struct CouncilDecisionMandate {
    std::string organization,decision_id,subject,proposer,prime_minister;
    std::vector<CouncilMemberRule> member_rules;
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
    std::vector<Extractor> extractors;
    std::vector<ResearchPolicy> researches;
    std::vector<FacilityMaintenance> maintenance;
    std::vector<DebtService> debt_service;
    std::vector<Buyer> buyers;
    std::vector<Investment> investments;
    std::vector<PoliticalReaction> political_reactions;
    std::vector<Election> elections;
    std::vector<CountryAI> countries;
    std::vector<CrisisMandate> crisis_mandates;
    std::vector<OrganizationDuesMandate> organization_dues;
    std::vector<TreatyDecisionMandate> treaty_ratifications;
    std::vector<CouncilDecisionMandate> council_decisions;
    std::vector<phasef::FreightLane> freight_lanes;
    std::vector<Engagement> engagements;

    void validate() const {
        std::set<std::string> seen_firms,seen_projects,seen_countries;
        std::set<std::pair<Id,std::string>> seen_pop_goods;
        std::set<std::string> engaged;
        for(const auto& f:manufacturers)
            if(f.recipe.empty() || f.target_inventory<0 ||
               f.wage_per_worker_per_day<0 || !seen_firms.insert(f.recipe).second ||
               (f.wage_per_worker_per_day>0 &&
                (f.employee_wallet.empty()||f.treasury_wallet.empty())))
                throw std::invalid_argument("P0_INVALID_MANUFACTURER");
        std::set<std::string> extraction_ids;
        for(const auto& e:extractors)
            if(e.resource_site.empty()||e.target_inventory<=0||
               e.wage_per_unit<0||
               (e.wage_per_unit>0&&
                (e.employee_wallet.empty()||e.treasury_wallet.empty()))||
               !extraction_ids.insert(e.resource_site).second)
                throw std::invalid_argument("P0_INVALID_EXTRACTOR");
        std::set<std::string> research_ids;
        for(const auto& r:researches)
            if(r.project.empty()||r.maximum_daily_work<=0||
               !research_ids.insert(r.project).second)
                throw std::invalid_argument("P0_INVALID_RESEARCH_POLICY");
        for(const auto& s:staffing)
            if(s.employer.empty()||s.region.empty()||s.wanted<0||s.monthly_wage<0)
                throw std::invalid_argument("P0_INVALID_STAFFING");
        std::set<std::string> maintained_sites;
        for(const auto& m:maintenance)
            if(m.facility.empty()||m.building_good.empty()||
               m.max_daily_restoration<=0||m.material_per_unit<=0||
               m.workers_per_unit<=0||m.wage_per_worker_per_day<0||
               (m.wage_per_worker_per_day>0&&
                (m.employee_wallet.empty()||m.treasury_wallet.empty()))||
               !maintained_sites.insert(m.facility).second)
                throw std::invalid_argument("P0_INVALID_MAINTENANCE_POLICY");
        std::set<std::string> debt_borrowers;
        for(const auto& credit:debt_service)
            if(credit.borrower.empty()||credit.minimum_cash_reserve<0||
               credit.max_daily_principal<=0||
               !debt_borrowers.insert(credit.borrower).second)
                throw std::invalid_argument("P0_INVALID_DEBT_SERVICE");
        for(const auto& b:buyers)
            if(!b.population_slice||b.household_actor.empty()||b.seller.empty()||
               b.region.empty()||b.good.empty()||b.units_per_person_per_day<=0||
               !seen_pop_goods.insert({b.population_slice,b.good}).second)
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
        std::set<std::tuple<std::string,int,int>> election_months;
        for(const auto& e:elections)
            if(e.country.empty()||e.incumbent_party.empty()||e.reform_party.empty()||
               e.incumbent_party==e.reform_party||e.year<1||e.year>9999||
               e.month<1||e.month>12||e.day<1||
               e.day>daily::days_in_month(e.year,e.month)||
               e.turnout_ppm<0||e.turnout_ppm>1'000'000||
               !election_months.insert({e.country,e.year,e.month}).second)
                throw std::invalid_argument("P0_INVALID_COUNTRY_ELECTION");
        for(const auto& c:countries)
            if(c.country.empty()||c.prime_minister.empty()||
               c.domestic_region.empty()||c.good.empty()||
               c.domestic_seller.empty()||c.protect_below_price<=0||
               c.protective_tariff_ppm<0||c.protective_tariff_ppm>1'000'000||
               !seen_countries.insert(c.country+":"+c.good).second)
                throw std::invalid_argument("P0_INVALID_COUNTRY_AI");
        std::set<std::string> crisis_countries;
        for(const auto& mandate:crisis_mandates)
            if(mandate.country.empty()||mandate.prime_minister.empty()||
               mandate.maximum_cash_settlement<0||
               !crisis_countries.insert(mandate.country).second)
                throw std::invalid_argument("P0_INVALID_CRISIS_MANDATE");
        std::set<std::pair<std::string,std::string>> dues_members;
        for(const auto& mandate:organization_dues)
            if(mandate.country.empty()||mandate.prime_minister.empty()||
               mandate.organization.empty()||
               !dues_members.insert({mandate.organization,mandate.country}).second)
                throw std::invalid_argument("P0_INVALID_DUES_MANDATE");
        std::set<std::string> treaty_countries;
        for(const auto& m:treaty_ratifications)
            if(m.country.empty()||m.prime_minister.empty()||
               m.maximum_new_fiscal_obligation<0||
               !treaty_countries.insert(m.country).second)
                throw std::invalid_argument("P0_INVALID_TREATY_DECISION");
        std::set<std::pair<std::string,std::string>> council_ids;
        for(const auto& m:council_decisions) {
            if(m.organization.empty()||m.decision_id.empty()||
               m.subject.empty()||m.proposer.empty()||
               m.prime_minister.empty()||m.member_rules.empty()||
               !council_ids.insert({m.organization,m.decision_id}).second)
                throw std::invalid_argument("P0_INVALID_COUNCIL_DECISION");
            std::set<std::string> members;
            for(const auto& member:m.member_rules)
                if(member.country.empty()||member.prime_minister.empty()||
                   member.minimum_free_treasury_for_yes<0||
                   !members.insert(member.country).second)
                    throw std::invalid_argument("P0_INVALID_COUNCIL_MEMBER");
            if(!members.contains(m.proposer))
                throw std::invalid_argument("P0_MISSING_COUNCIL_PROPOSER");
        }
        std::set<std::string> freight_ids;
        for(const auto& f:freight_lanes) {
            phasef::validate(f);
            if(!freight_ids.insert(f.id).second)
                throw std::invalid_argument("P0_DUPLICATE_AUTONOMOUS_FREIGHT");
        }
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

// Contract wage is an economic cost even when payroll is scheduled outside
// the manufacture command. Conservative highest rate prevents firms from
// treating unpaid or omitted payroll as zero-cost labor.
inline Money actual_daily_worker_wage(const phasec::IntegratedWorld& c,
                                      const std::string& employer,
                                      int day,int days) {
    Money highest=0;
    for(const auto& [slice,job]:c.people().jobs()) {
        (void)slice;
        if(job.employer!=employer||job.count<=0||
           job.wage_per_month<=0)continue;
        highest=std::max(highest,
                         daily_increment(job.wage_per_month,day,days));
    }
    return highest;
}

inline void hire_available(phasec::IntegratedWorld& c,const Staffing& p) {
    // Count actual employment contracts in this region, NOT effective workers:
    // a strike is lost production for a day, not a vacant permanent job.
    People contracted=0;
    for(const auto& [slice,job]:c.people().jobs()) {
        if(job.employer!=p.employer)continue;
        const auto& row=c.people().slice(slice);
        if(c.people().demographic(row.key.demographic).region==p.region)
            contracted=phaseb::add_checked(contracted,job.count);
    }
    People need=p.wanted-contracted;
    if(need<=0)return;
    // This is not population creation. Only an eligible, truly uncontracted
    // regional slice can be hired; no cross-firm job overwrite.
    for(const auto& [id,s]:c.people().slices()) {
        if(need<=0)break;
        const auto& demo=c.people().demographic(s.key.demographic);
        if(demo.region!=p.region)continue;
        const auto* current_job=c.people().job(id);
        if(current_job && (current_job->employer!=p.employer ||
           current_job->occupation!="worker" ||
           current_job->wage_per_month!=p.monthly_wage))continue;
        const auto& econ=c.people().economic(s.key.economic);
        if(econ.profession!="worker"&&econ.profession!="unskilled_worker")continue;
        if(!c.people().eligible_for(id,phaseb::JobRequirements{}))continue;
        const People n=std::min(s.count-c.people().employed(id),need);
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
    // Count explicitly scripted OR real contractual payroll, never both.
    const Money per_worker=policy.wage_per_worker_per_day>0?
        policy.wage_per_worker_per_day:
        actual_daily_worker_wage(c,f.owner,day,days);
    if(per_worker>0)
        cost_per_batch=phaseb::add_checked(cost_per_batch,
                 times(r.workers_per_batch,per_worker));
    if(revenue<=cost_per_batch)return 0;
    const Quantity stock_shortfall=policy.target_inventory-c.stock(f.owner,f.region,primary.good);
    Quantity batches=stock_shortfall/primary.per_unit+
        (stock_shortfall%primary.per_unit!=0?1:0);
    const Quantity used=used_site_capacity(state,r.site,d.month());
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
    const People a=c.people().slice(e.attacker_slice).count;
    const People b=c.people().slice(e.defender_slice).count;
    // The combat order targets a real assigned slice. The parent unit may
    // retain other active cohorts after an earlier day's injuries or splits.
    if(a<=0||b<=0||c.unit_slice_strength(e.attacker,e.attacker_slice)!=a||
       c.unit_slice_strength(e.defender,e.defender_slice)!=b)return;
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
    explicit DailyGame(daily::World world)
      :DailyGame(std::move(world),Policies{}){}
    [[nodiscard]] const daily::World& world()const {return world_;}
    [[nodiscard]] const Policies& policies()const {return policies_;}
    // Campaign/save data owns the policy definitions; base day snapshots remain
    // fully compatible and replay exactly when the same policy set is restored.
    daily::DayReport advance_day(const phasegh::World::Plan& additional_today={}) {
        const auto date=world_.date();
        const int dim=daily::days_in_month(date.year,date.month);
        // Scenario policies are preferences, not a required exhaustive list
        // of operating firms. Derive conservative supply plans from the live
        // C/D ledger for sites that have no explicitly managed policy.
        Policies decisions=policies_;
        const auto& game_c=world_.engine().base().core().core();
        const auto& industry=world_.engine().base().core().state();
        std::set<std::string> payroll_needs_explicit_placement;
        for(const auto& [id,job]:game_c.people().jobs()) {
            if(job.count<=0||job.wage_per_month<=0)continue;
            const auto& pop=game_c.people().slice(id);
            const auto& dem=game_c.people().demographic(pop.key.demographic);
            const auto household="HH:"+std::to_string(pop.key.household);
            const auto& country=game_c.sovereignty(dem.region);
            const bool treasury_ready=game_c.has_government(country)?
                game_c.has_country_treasury(country):
                (!game_c.has_any_government()&&game_c.has_actor(country));
            if(!game_c.has_actor(job.employer)||!game_c.has_actor(household)||
               !treasury_ready)
                payroll_needs_explicit_placement.insert(job.employer);
        }
        for(const auto& [id,recipe]:industry.recipes) {
            if(recipe.outputs.empty() ||
               std::any_of(decisions.manufacturers.begin(),
                           decisions.manufacturers.end(),
                    [&](const Manufacturer& p){return p.recipe==id;}))
                continue;
            const auto& facility=game_c.facility(recipe.site);
            const auto& output=recipe.outputs.front();
            if(payroll_needs_explicit_placement.contains(facility.owner) ||
               !game_c.has_market(facility.region,output.good))
                continue; // Do not invent a household wage recipient.
            const auto& quote=game_c.market(facility.region,output.good);
            decisions.manufacturers.push_back(Manufacturer{
                id,std::max<Quantity>(output.per_unit,quote.target_stock)});
        }
        for(const auto& [id,site]:industry.resource_sites) {
            if(std::any_of(decisions.extractors.begin(),
                           decisions.extractors.end(),
                    [&](const Extractor& p){return p.resource_site==id;}))
                continue;
            if(payroll_needs_explicit_placement.contains(site.owner) ||
               !game_c.has_market(site.region,site.good))continue;
            const auto& quote=game_c.market(site.region,site.good);
            decisions.extractors.push_back(Extractor{
                id,std::max<Quantity>(1,quote.target_stock)});
        }
        for(const auto& [id,research]:industry.researches) {
            if(research.reviewed||research.verified_work>=research.required_evidence||
               std::any_of(decisions.researches.begin(),decisions.researches.end(),
                    [&](const ResearchPolicy& p){return p.project==id;}))
                continue;
            decisions.researches.push_back(ResearchPolicy{id,1});
        }
        for(const auto& [id,project]:industry.constructions) {
            if(project.ready||project.commissioned||
               project.accepted_work>=project.work_required||
               std::any_of(decisions.investments.begin(),decisions.investments.end(),
                    [&](const Investment& p){return p.construction_project==id;}))
                continue;
            for(const auto& [recipe_id,recipe]:industry.recipes) {
                (void)recipe_id;
                if(recipe.site!=project.target_site||recipe.outputs.empty())continue;
                const auto& good=recipe.outputs.front().good;
                if(!game_c.has_market(project.region,good))continue;
                decisions.investments.push_back(Investment{id,good,0,1,1});
                break;
            }
        }
        phasegh::World::Plan plan;
        auto salary_shortfall=std::make_shared<std::set<std::string>>();
        // A real Employment contract can pay a real household wallet each
        // calendar day. The C bank/tax ledger remains the sole authority.
        // P0 scenario payroll, when explicitly configured, owns that firm's
        // remuneration and must not be paid for a second time.
        std::set<std::string> scenario_payroll;
        for(const auto& p:decisions.manufacturers)
            if(p.wage_per_worker_per_day>0)
                scenario_payroll.insert(
                    game_c.facility(industry.recipes.at(p.recipe).site).owner);
        for(const auto& p:decisions.extractors)
            if(p.wage_per_unit>0)
                scenario_payroll.insert(industry.resource_sites.at(p.resource_site).owner);
        for(const auto& p:decisions.maintenance)
            if(p.wage_per_worker_per_day>0)
                scenario_payroll.insert(game_c.facility(p.facility).owner);
        for(const auto& [slice_id,contract]:game_c.people().jobs()) {
            if(contract.count<=0||contract.wage_per_month<=0||
               scenario_payroll.contains(contract.employer))continue;
            const auto& slice=game_c.people().slice(slice_id);
            const auto& dem=game_c.people().demographic(slice.key.demographic);
            const auto wallet="HH:"+std::to_string(slice.key.household);
            if(!game_c.has_actor(wallet)||!game_c.has_actor(contract.employer))
                continue;
            const auto sovereign=game_c.sovereignty(dem.region);
            if(game_c.has_government(sovereign)&&
               !game_c.has_country_treasury(sovereign))
                continue;
            if(!game_c.has_government(sovereign)&&
               (game_c.has_any_government()||!game_c.has_actor(sovereign)))
                continue;
            const auto treasury=game_c.has_government(sovereign)?
                                 game_c.country_treasury(sovereign):sovereign;
            if(contract.employer==wallet||contract.employer==treasury||
               wallet==treasury)continue;
            const auto employer=contract.employer;
            const auto purpose="P0:CONTRACT-WAGE:"+std::to_string(world_.day_index())+
                               ":"+std::to_string(slice_id);
            auto reserved=std::make_shared<Money>(0);
            plan[1].push_back(
                [slice_id,employer,purpose,date,dim,reserved,
                 salary_shortfall](phasegh::Stage& gh) {
                    auto& c=gh.phase_c();
                    const auto* now=c.people().job(slice_id);
                    if(!now||now->employer!=employer||now->count<=0||
                       now->wage_per_month<=0)return;
                    const Money full_month=times(now->count,now->wage_per_month);
                    const Money today=daily_increment(full_month,date.day,dim);
                    const Money available=c.phase_d_cash_available(employer);
                    const Money payable=std::min(today,std::max<Money>(0,available));
                    if(payable<today)salary_shortfall->insert(employer);
                    if(payable<=0)return;
                    c.phase_d_reserve_cash(employer,purpose,payable);
                    *reserved=payable;
                });
            plan[5].push_back(
                [employer,wallet,treasury,sovereign,purpose,reserved](phasegh::Stage& gh) {
                    if(*reserved<=0)return;
                    auto& c=gh.phase_c();
                    c.phase_d_release_cash(employer,purpose,*reserved);
                    c.wages_under_law(employer,wallet,treasury,*reserved,
                                      c.has_government(sovereign)?sovereign:"");
                });
        }
        for(const auto& ai:decisions.countries) {
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
        for(const auto& investment:decisions.investments) {
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
        for(const auto& staffing:decisions.staffing)
            plan[2].push_back([staffing](phasegh::Stage& gh){
                hire_available(gh.phase_c(),staffing);
            });
        for(const auto& firm:decisions.manufacturers) {
            auto workers=std::make_shared<People>(0);
            plan[4].push_back([firm,date,dim,workers,
                               salary_shortfall](phasegh::Stage& gh) {
                auto& c=gh.phase_c();
                const auto& recipe=gh.phase_d().state().recipes.at(firm.recipe);
                if(salary_shortfall->contains(c.facility(recipe.site).owner))
                    return;
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
                                                  firm.treasury_wallet,gross,
                                                  gh.phase_c().sovereignty(f.region));
                });
        }
        // A site operator chooses a real output quantity from target stock,
        // market price, labor, available wages and *cumulative* extraction
        // plus site throughput. No underground reserve counter is created.
        for(const auto& policy:decisions.extractors) {
            auto completed=std::make_shared<Quantity>(0);
            plan[4].push_back([policy,date,dim,completed,
                               salary_shortfall](phasegh::Stage& gh) {
                auto& c=gh.phase_c();
                const auto& ds=gh.phase_d().state();
                const auto& src=ds.resource_sites.at(policy.resource_site);
                if(salary_shortfall->contains(src.owner))return;
                const auto& facility=c.facility(src.site);
                const auto& quote=c.market(src.region,src.good);
                const Money wage_per_unit=policy.wage_per_unit>0?
                    policy.wage_per_unit:
                    times(actual_daily_worker_wage(c,src.owner,date.day,dim),
                          src.workers_per_unit);
                if(quote.price<=wage_per_unit ||
                   c.stock(src.owner,src.region,src.good)>=policy.target_inventory)
                    return;
                if(src.monthly_capacity<src.produced_this_month||
                   facility.capacity<=facility.damaged) return;
                const Quantity target=policy.target_inventory-
                                      c.stock(src.owner,src.region,src.good);
                Quantity available=proportional(src.monthly_capacity,date.day,dim);
                if(src.produced_this_month>=available)return;
                available=std::min(available-src.produced_this_month,target);
                if(src.renewable)available=std::min(available,src.standing_biomass);
                const People workers=c.phase_d_workforce(src.owner)-
                                     c.phase_d_shared_labor_used(src.owner);
                if(workers<=0)return;
                available=std::min(available,
                                  static_cast<Quantity>(workers/src.workers_per_unit));
                const Quantity already_site=used_site_capacity(
                    ds,src.site,gh.phase_d().month());
                const Quantity facility_limit=proportional(
                       facility.capacity-facility.damaged,date.day,dim);
                if(already_site>=facility_limit)return;
                available=std::min(available,
                         (facility_limit-already_site)/src.site_service_per_unit);
                if(policy.wage_per_unit>0)
                    available=std::min(available,
                         c.phase_d_cash_available(src.owner)/policy.wage_per_unit);
                if(available<=0)return;
                gh.phase_d().extract_resource_daily(policy.resource_site,
                                                    available,date.day,dim);
                *completed=available;
            });
            if(policy.wage_per_unit>0)
                plan[5].push_back([policy,completed](phasegh::Stage& gh){
                    if(*completed<=0)return;
                    const auto& src=gh.phase_d().state().resource_sites.at(
                                              policy.resource_site);
                    gh.phase_c().wages_under_law(src.owner,policy.employee_wallet,
                                  policy.treasury_wallet,
                                  times(*completed,policy.wage_per_unit),
                                  gh.phase_c().sovereignty(src.region));
                });
        }
        // Funded, self-scheduling research is an actual D work order:
        // stage1 cash reservation, stage4 lab work/materials, stage5 pay,
        // stage9 verified result. Every lab shares cumulative DAY capacity
        // with manufacturing and geological extraction.
        for(const auto& research:decisions.researches) {
            auto work=std::make_shared<Quantity>(0);
            plan[1].push_back([research,date,dim,work](phasegh::Stage& gh) {
                const auto& ds=gh.phase_d().state();
                const auto& p=ds.researches.at(research.project);
                if(p.reviewed||p.verified_work>=p.required_evidence)return;
                auto& c=gh.phase_c();
                const auto& lab=c.facility(p.lab_site);
                if(lab.capacity<=lab.damaged)return;
                const Quantity used=used_site_capacity(
                    ds,p.lab_site,gh.phase_d().month());
                const Quantity cap=proportional(lab.capacity-lab.damaged,
                                                date.day,dim);
                if(used>=cap)return;
                const People labor=c.phase_d_workforce(p.holder)-
                                  c.phase_d_shared_labor_used(p.holder);
                if(labor<=0)return;
                const Quantity material=c.phase_d_material_available(
                                    p.holder,p.region,p.material_good);
                const Money funds=c.phase_d_cash_available(p.sponsor);
                if(funds<=0)return;
                const Quantity n=std::min({research.maximum_daily_work,
                    p.required_evidence-p.verified_work,cap-used,
                    material/p.material_per_work,
                    static_cast<Quantity>(labor/p.workers_per_work),
                    static_cast<Quantity>(funds/p.cash_cost_per_work)});
                if(n<=0)return;
                gh.phase_d().plan_research(research.project,n);
                *work=n;
            });
            plan[4].push_back([research,date,dim,work](phasegh::Stage& gh){
                if(*work>0)
                    gh.phase_d().perform_research_daily(research.project,
                                                   *work,date.day,dim);
            });
            plan[5].push_back([research,work](phasegh::Stage& gh){
                if(*work>0)gh.phase_d().pay_research(research.project,*work);
            });
            plan[9].push_back([research](phasegh::Stage& gh){
                const auto& p=gh.phase_d().state().researches.at(research.project);
                if(!p.reviewed&&p.verified_work>=p.required_evidence&&
                   p.paid_work>=p.required_evidence)
                    gh.phase_d().review_research(research.project);
            });
        }
        for(const auto& maintenance:decisions.maintenance) {
            auto completed=std::make_shared<Quantity>(0);
            plan[4].push_back([maintenance,completed](phasegh::Stage& gh) {
                auto& c=gh.phase_c();
                const auto& site=c.facility(maintenance.facility);
                if(site.damaged<=0)return;
                const Quantity available=c.phase_d_material_available(
                       site.owner,site.region,maintenance.building_good);
                const People used=c.phase_d_shared_labor_used(site.owner);
                const People employed=c.phase_d_workforce(site.owner);
                if(employed<=used)return;
                Quantity work=std::min({site.damaged,
                       maintenance.max_daily_restoration,
                       available/maintenance.material_per_unit,
                       (employed-used)/maintenance.workers_per_unit});
                if(work<=0)return;
                if(maintenance.wage_per_worker_per_day>0) {
                    const Money cost=times(times(work,maintenance.workers_per_unit),
                                           maintenance.wage_per_worker_per_day);
                    if(cost>c.phase_d_cash_available(site.owner))return;
                }
                c.phase_time_maintain_facility(maintenance.facility,
                         maintenance.building_good,work,
                         maintenance.material_per_unit,
                         times(work,maintenance.workers_per_unit));
                *completed=work;
            });
            if(maintenance.wage_per_worker_per_day>0)
                plan[5].push_back([maintenance,completed](phasegh::Stage& gh) {
                    if(*completed<=0)return;
                    const auto& site=gh.phase_c().facility(maintenance.facility);
                    const Money gross=times(times(*completed,
                          maintenance.workers_per_unit),
                          maintenance.wage_per_worker_per_day);
                    gh.phase_c().wages_under_law(site.owner,
                          maintenance.employee_wallet,
                          maintenance.treasury_wallet,gross,
                          gh.phase_c().sovereignty(site.region));
                });
        }
        for(const auto& credit:decisions.debt_service)
            plan[8].push_back([credit](phasegh::Stage& gh){
                auto& c=gh.phase_c();
                const Money debt=c.outstanding_loan(credit.borrower);
                const Money available=c.phase_d_cash_available(credit.borrower);
                if(debt<=0||available<=credit.minimum_cash_reserve)return;
                Money remaining=std::min({debt,credit.max_daily_principal,
                               available-credit.minimum_cash_reserve});
                // G trade credit is a bounded legal facility backed by this
                // SAME C bank loan. Retire its utilization before a direct
                // C repayment, otherwise G.drawn would disagree with C debt.
                for(const auto& [id,facility]:gh.commerce().credit) {
                    if(remaining<=0)break;
                    if(!facility.active||facility.borrower!=credit.borrower ||
                       facility.drawn<=0)continue;
                    const Money amount=std::min(remaining,facility.drawn);
                    gh.repay_trade_credit(id,amount);
                    remaining-=amount;
                }
                if(remaining>0)c.repay(credit.borrower,remaining);
            });
        for(const auto& b:decisions.buyers) {
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
                    // Household deprivation is NOT the same as solvent unmet
                    // demand: cash-poor POP also generate a real food grievance.
                    if(b.good=="food"&&purchased<demand)
                        c.phase_time_record_food_need_gap(b.region,demand-purchased);
                }
            });
        }
        // All canonical households make endogenous daily essential-goods
        // decisions after wages and actual production.  A household wallet is
        // required to spend money; self-held food/clothing can be consumed
        // in kind.  Explicit scenario Buyer policies take precedence per POP
        // and good, so no duplicate consumption is created.
        std::set<std::pair<Id,std::string>> explicitly_managed_demand;
        for(const auto& buyer:decisions.buyers)
            explicitly_managed_demand.insert({buyer.population_slice,buyer.good});
        plan[7].push_back(
            [explicitly_managed_demand](phasegh::Stage& gh) {
                auto& c=gh.phase_c();
                const auto& people=c.people();
                using NeedKey=std::tuple<std::string,Id,std::string>;
                std::map<NeedKey,Quantity> needs;
                for(const auto& [slice_id,pop]:people.slices()) {
                    if(pop.count<=0)continue;
                    const auto& d=people.demographic(pop.key.demographic);
                    const auto age=people.age_months(pop.key.demographic);
                    const std::array<std::pair<std::string,Quantity>,2> baskets{{
                        {"food",age<10*12?2:3},{"clothing",1}
                    }};
                    for(const auto& [good,per_person]:baskets) {
                        if(explicitly_managed_demand.contains({slice_id,good}))
                            continue;
                        auto& due=needs[{d.region,pop.key.household,good}];
                        due=phaseb::add_checked(due,times(pop.count,per_person));
                    }
                }
                for(const auto& [key,demand]:needs) {
                    const auto& [region,household,good]=key;
                    const std::string wallet="HH:"+std::to_string(household);
                    Quantity still_due=demand;
                    const Quantity in_kind=std::min(still_due,
                        c.phase_d_material_available(wallet,region,good));
                    if(in_kind>0) {
                        c.consume(wallet,region,good,in_kind);
                        still_due-=in_kind;
                    }
                    if(still_due>0 && c.has_actor(wallet) &&
                       c.has_market(region,good)) {
                        const Quantity purchased=c.clear_household_market(
                            wallet,region,good,still_due);
                        if(purchased>0) {
                            c.consume(wallet,region,good,purchased);
                            still_due-=purchased;
                        }
                    }
                    if(good=="food" && still_due>0)
                        c.phase_time_record_food_need_gap(region,still_due);
                }
            });
        for(const auto& p:decisions.political_reactions) {
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
        for(const auto& election:decisions.elections) {
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
                    const People taxes=times(voters,c.income_tax_ppm(election.country))/4'000'000;
                    const People preference=std::min(voters,
                       phaseb::add_checked(joblessness,phaseb::add_checked(scarcity,taxes)));
                    reform=phaseb::add_checked(reform,preference);
                    incumbent=phaseb::add_checked(incumbent,voters-preference);
                }
                const People registered=phaseb::add_checked(incumbent,reform);
                if(registered>0)
                    c.hold_country_election(election.country,registered,0,
                         {{election.incumbent_party,incumbent},
                          {election.reform_party,reform}});
            });
        }
        for(const auto& dues:decisions.organization_dues)
            plan[5].push_back([dues](phasegh::Stage& gh) {
                const auto& org=gh.diplomacy().organizations.at(dues.organization);
                if(!org.active||!org.members.contains(dues.country)||
                   org.dues_per_member<=0)return;
                if(gh.phase_c().government(dues.country).prime_minister!=
                   dues.prime_minister)
                    throw std::invalid_argument("P0_DUES_UNAUTHORIZED_GOVERNMENT");
                const auto key=std::make_pair(gh.phase_c().month(),dues.country);
                const auto it=org.paid.find(key);
                const Money already=it==org.paid.end()?0:it->second;
                if(already>=org.dues_per_member)return;
                const Money due=org.dues_per_member-already;
                if(gh.phase_c().phase_d_cash_available(gh.phase_c().country_financial_wallet(dues.country))<due)return;
                gh.pay_dues(dues.organization,dues.country,due);
            });
        for(const auto& ai:decisions.crisis_mandates)
            plan[9].push_back([ai](phasegh::Stage& gh) {
                if(gh.phase_c().government(ai.country).prime_minister!=
                   ai.prime_minister)
                    throw std::invalid_argument("P0_CRISIS_UNAUTHORIZED_GOVERNMENT");
                for(const auto& [id,crisis]:gh.diplomacy().crises) {
                    if(crisis.status==phasegh::CrisisStatus::settled||
                       crisis.status==phasegh::CrisisStatus::withdrawn||
                       crisis.status==phasegh::CrisisStatus::escalated||
                       crisis.status==phasegh::CrisisStatus::scheduled)
                        continue;
                    if(ai.mediate_neutral_crises&&
                       crisis.status==phasegh::CrisisStatus::active&&
                       ai.country!=crisis.claimant&&
                       ai.country!=crisis.respondent&&crisis.mediator.empty()) {
                        gh.mediate_crisis(id,ai.country,ai.prime_minister);
                        continue;
                    }
                    if(ai.country!=crisis.claimant&&ai.country!=crisis.respondent)
                        continue;
                    // No arbitrary war/claim generation or unfunded concession:
                    // select an actual proposal and compare real treasury risk.
                    const phasegh::CrisisProposal* selected=nullptr;
                    for(const auto& [offer_id,offer]:crisis.proposals) {
                        (void)offer_id;
                        if(offer.approvers.contains(ai.country)||
                           (offer.freeze_claim&&
                            ai.country==crisis.claimant&&
                            !ai.accept_claim_withdrawal))continue;
                        if(offer.compensation>0&&offer.payer==ai.country &&
                           (offer.compensation>ai.maximum_cash_settlement ||
                            offer.compensation>
                              gh.phase_c().phase_d_cash_available(gh.phase_c().country_financial_wallet(ai.country))))
                            continue;
                        if(!selected||offer.compensation<selected->compensation||
                           (offer.compensation==selected->compensation&&
                            offer.id<selected->id))
                            selected=&offer;
                    }
                    if(selected)gh.respond_crisis_offer(
                        id,selected->id,ai.country,true,ai.prime_minister);
                }
            });
        for(const auto& rule:decisions.treaty_ratifications)
            plan[9].push_back([rule](phasegh::Stage& gh){
                auto& c=gh.phase_c();
                if(c.government(rule.country).prime_minister!=rule.prime_minister)
                    throw std::invalid_argument("P0_H_TREATY_UNAUTHORIZED_PRIME");
                for(const auto& [id,t]:gh.diplomacy().treaties) {
                    if(t.parties.empty()||!t.parties.contains(rule.country)||
                       t.active||t.pending_termination||
                       t.ratifications.contains(rule.country))
                        continue;
                    Money fiscal_exposure=0;
                    bool eligible=true;
                    for(const auto& term:t.clauses) {
                        if(term.obligor!=rule.country)continue;
                        const auto kind=term.kind;
                        if(kind==phasegh::ClauseKind::territorial_cession &&
                           !rule.allow_territorial_cession)
                            eligible=false;
                        if((kind==phasegh::ClauseKind::trade_embargo ||
                            kind==phasegh::ClauseKind::trade_quota) &&
                           !rule.allow_trade_constraints)
                            eligible=false;
                        if((kind==phasegh::ClauseKind::military_access ||
                            kind==phasegh::ClauseKind::defense_guarantee ||
                            kind==phasegh::ClauseKind::protectorate) &&
                           !rule.allow_military_commitments)
                            eligible=false;
                        if(kind==phasegh::ClauseKind::financial_aid ||
                           kind==phasegh::ClauseKind::reparations ||
                           kind==phasegh::ClauseKind::cultural_exchange)
                            fiscal_exposure=phaseb::add_checked(
                                fiscal_exposure,term.amount);
                    }
                    if(!eligible||
                       fiscal_exposure>rule.maximum_new_fiscal_obligation||
                       fiscal_exposure>c.phase_d_cash_available(c.country_financial_wallet(rule.country)))
                        continue;
                    gh.ratify_package_member(id,rule.country,rule.prime_minister);
                }
            });
        for(const auto& rule:decisions.council_decisions)
            plan[9].push_back([rule](phasegh::Stage& gh){
                const auto& org=gh.diplomacy().organizations.at(rule.organization);
                if(!org.active||org.decisions.contains(rule.decision_id))
                    return;
                if(!org.members.contains(rule.proposer)||
                   gh.phase_c().government(rule.proposer).prime_minister!=
                         rule.prime_minister)
                    throw std::invalid_argument("P0_H_COUNCIL_NO_PROPOSER");
                std::map<std::string,phasegh::VoteChoice> votes;
                for(const auto& m:rule.member_rules) {
                    if(!org.members.contains(m.country)||
                       gh.phase_c().government(m.country).prime_minister!=
                            m.prime_minister)
                        throw std::invalid_argument("P0_H_COUNCIL_UNAUTHORIZED_MEMBER");
                    const Money actual=gh.phase_c().phase_d_cash_available(gh.phase_c().country_financial_wallet(m.country));
                    votes.emplace(m.country,actual>=m.minimum_free_treasury_for_yes?
                        phasegh::VoteChoice::yes:phasegh::VoteChoice::no);
                }
                if(votes.size()<org.governance.quorum)return;
                gh.hold_organization_vote(rule.organization,rule.decision_id,
                       rule.subject,votes,rule.proposer,rule.prime_minister);
            });
        for(const auto& combat:decisions.engagements)
            plan[10].push_back([combat](phasegh::Stage& gh){
                fight(gh.phase_c(),combat);
            });
        // Closing market observations is a daily accounting operation,
        // independent of whether a scenario supplied explicit household buyers.
        plan[11].push_back([ordinal=world_.day_index()](phasegh::Stage& gh){
            gh.phase_c().phase_time_close_daily_markets(ordinal);
        });
        // Supplemental commands execute today in the canonical day
        // transaction. No monthly callback staging or adapter remains.
        for(std::size_t i=0;i<plan.size();++i)
            plan[i].insert(plan[i].end(),
                additional_today[i].begin(),additional_today[i].end());
        if(decisions.freight_lanes.empty())
            return world_.advance_day(plan);
        // Freight planning may enqueue several dated physical commands. They
        // must be committed together with today's B/C/D/G/H events, never
        // leave a stray queue entry when a carrier/insurance payment fails.
        daily::World staged=world_;
        phasef::DayPlanningReservations freight_plan;
        for(const auto& lane:decisions.freight_lanes)
            if(auto job=phasef::plan_next_hop(staged,lane,freight_plan))
                staged.enqueue(date,std::move(*job));
        const auto report=staged.advance_day(plan);
        world_=std::move(staged);
        return report;
    }
};
} // namespace vic25::p0
