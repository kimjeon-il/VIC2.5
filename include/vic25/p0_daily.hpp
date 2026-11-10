#pragma once
#include "vic25/phase_f.hpp"
#include "vic25/phase_h_front_ai.hpp"
#include "vic25/day_budget.hpp"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <iterator>
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
// Player/campaign cabinet choice takes effect the following real day.
struct ParliamentNoConfidence {
    std::string country;
    std::vector<std::string> supporting_parties;
    daily::Date decision_date{};
};
struct GovernmentFormation {
    std::string country, incumbent_head, new_prime, legal_basis;
    std::vector<std::string> ruling_groups, ruling_parties;
    std::int32_t year{},month{1},day{1};
    bool caretaker{};
};
struct ParliamentDissolution {
    std::string contest_id,incumbent_head,legal_basis;
    daily::Date decision_date{},early_poll_date{};
};
struct ElectionTermReform {
    phasec::P4TermAmendment amendment;
    std::string proposer;
    std::vector<std::string> supporting_parties;
    daily::Date decision_date{};
};
enum class LegislativeAction :std::uint8_t {
    introduce, approve, promulgate, approve_upper,
    sign, veto, override_veto, withdraw
};
struct LegislativeOrder {
    daily::Date date{};
    LegislativeAction action{LegislativeAction::introduce};
    phasec::P2Bill bill; // populated for introduction only
    std::string bill_id,head;
    std::vector<std::string> supporting_parties;
    std::vector<std::string> upper_supporting_parties;
};
enum class PoliticalOrderKind : std::uint8_t {
    activate_mandate, cancel_mandate, event_decision, register_lobby
};
struct PoliticalOrder {
    daily::Date date{};
    PoliticalOrderKind kind{PoliticalOrderKind::activate_mandate};
    phasec::P1Mandate mandate;
    std::string mandate_id,event_id;
    phasec::P1PoliticalEventDecision choice{
        phasec::P1PoliticalEventDecision::acknowledge};
    phasec::P1Lobby lobby;
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
struct SovereignFinancing {
    std::string country,investor;
    Money target_treasury_liquidity{},investor_reserve{};
    Money max_daily_bond_issue{},max_total_bond_principal{};
    int annual_coupon_ppm{};
    std::int64_t maturity_days{};
};
struct PublicBudgetProgram {
    std::string country,recipient;
    Money daily_spending_limit{},treasury_cash_reserve{};
};
// These bids represent real agents willing to buy bankrupt property. Assets
// are transferred only with actual existing C cash, never at an invented price.
// One national cash budget and bounded competitive bond auction. Quotes are
// actual buyer mandates; an unclaimed issue cannot create treasury deposits.
struct NationalBudgetEnvelope {
    std::string country;
    Money reserve_cash{},maximum_daily_spending{},target_cash{};
    Money maximum_daily_new_bonds{},maximum_total_bond_principal{};
    int minimum_coupon_ppm{},maximum_coupon_ppm{};
    std::int64_t maturity_days{};
    bool offer_to_operating_firms{true};
    bool finance_real_public_services{true};
};
struct BondBuyerOrder {
    std::string country,buyer;
    Money max_daily_subscription{},cash_reserve{};
    int minimum_coupon_ppm{};
};
struct BondSecondaryAsk {
    std::string order_id,bond_id,seller;
    Money face_units{};
    int minimum_price_ppm{1'000'000};
};
struct BondSecondaryBid {
    std::string order_id,bond_id,buyer;
    Money max_face_units{};
    int maximum_price_ppm{1'000'000};
};
struct LiquidationStockBid {
    std::string firm,buyer,region,good;
    Quantity maximum_daily_units{};
    Money maximum_unit_price{};
};
struct LiquidationFactoryBid {
    std::string site,buyer;
    Money actual_bid_price{};
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
    // Optional AI defaults follow the original aggregate-initialization
    // fields, preserving the source layout of old campaign policy fixtures.
    int autonomous_tax_step_ppm{5'000};
    int autonomous_tax_ceiling_ppm{300'000};
    Quantity daily_food_units_per_person{1};
    Quantity monthly_clothing_units_per_person{1};
    std::vector<SovereignFinancing> sovereign_financing;
    std::vector<PublicBudgetProgram> public_budget;
    bool allow_endogenous_factory_expansion{true};
    Money autonomous_factory_cash_reserve{};
    bool allow_endogenous_crisis_wars{true};
    bool allow_operational_battle_ai{true};
    bool allow_autonomous_war_peace{true};
    std::vector<LiquidationStockBid> liquidation_stock_bids;
    std::vector<LiquidationFactoryBid> liquidation_factory_bids;
    bool retire_bank_debt_after_wages{true};
    bool allow_market_liquidation_of_real_stock{true};
    bool allow_autonomous_army_resupply{true};
    bool auto_national_budgets_from_actual_services{true};
    std::vector<NationalBudgetEnvelope> national_budget_envelopes;
    std::vector<BondBuyerOrder> bond_buyer_orders;
    std::vector<BondSecondaryAsk> bond_secondary_asks;
    std::vector<BondSecondaryBid> bond_secondary_bids;
    std::vector<GovernmentFormation> government_formations; // append-only
    std::vector<ParliamentNoConfidence> no_confidence_votes;
    std::vector<ParliamentDissolution> dissolutions;
    std::vector<ElectionTermReform> election_term_reforms;
    std::vector<LegislativeOrder> legislative_orders; // append-only
    std::set<std::string> player_controlled_countries; // never autonomously governed
    std::vector<PoliticalOrder> political_orders; // date-scoped lawful P1 commands
    void validate() const {
        if(autonomous_tax_step_ppm<0||autonomous_tax_step_ppm>1'000'000||
           autonomous_tax_ceiling_ppm<0||
           autonomous_tax_ceiling_ppm>1'000'000||
           daily_food_units_per_person<0||
           monthly_clothing_units_per_person<0 ||
           autonomous_factory_cash_reserve<0)
            throw std::invalid_argument("P0_INVALID_AUTONOMOUS_FISCAL_RULE");
        std::set<std::string> seen_firms,seen_projects,seen_countries;
        std::set<std::string> financed_countries;
        for(const auto& b:sovereign_financing)
            if(b.country.empty()||b.investor.empty()||
               !financed_countries.insert(b.country).second||
               b.target_treasury_liquidity<0||b.investor_reserve<0||
               b.max_daily_bond_issue<=0||b.max_total_bond_principal<=0||
               b.annual_coupon_ppm<0||b.annual_coupon_ppm>1'000'000||
               b.maturity_days<=0)
                throw std::invalid_argument("P0_INVALID_SOVEREIGN_FINANCING");
        std::set<std::pair<std::string,std::string>> budget_programs;
        for(const auto& p:public_budget)
            if(p.country.empty()||p.recipient.empty()||
               p.daily_spending_limit<=0||p.treasury_cash_reserve<0||
               !budget_programs.insert({p.country,p.recipient}).second)
                throw std::invalid_argument("P0_INVALID_PUBLIC_PROGRAM");
        std::set<std::tuple<std::string,std::string,std::string,std::string>>
            unique_liquidation_lots;
        for(const auto& b:liquidation_stock_bids)
            if(b.firm.empty()||b.buyer.empty()||b.firm==b.buyer||
               b.region.empty()||b.good.empty()||b.maximum_daily_units<=0||
               b.maximum_unit_price<=0||
               !unique_liquidation_lots.insert(
                    {b.firm,b.buyer,b.region,b.good}).second)
                throw std::invalid_argument("P0_INVALID_LIQUIDATION_GOODS_BID");
        std::set<std::string> unique_liquidation_sites;
        for(const auto& b:liquidation_factory_bids)
            if(b.site.empty()||b.buyer.empty()||b.actual_bid_price<=0||
               !unique_liquidation_sites.insert(b.site).second)
                throw std::invalid_argument("P0_INVALID_LIQUIDATION_SITE_BID");
        std::set<std::string> unique_national_budgets;
        for(const auto& b:national_budget_envelopes)
            if(b.country.empty()||
               financed_countries.contains(b.country)||
               !unique_national_budgets.insert(b.country).second||
               b.reserve_cash<0||b.maximum_daily_spending<0||
               b.target_cash<0||b.maximum_daily_new_bonds<=0||
               b.maximum_total_bond_principal<=0||
               b.minimum_coupon_ppm<0||b.maximum_coupon_ppm<0||
               b.minimum_coupon_ppm>b.maximum_coupon_ppm||
               b.maximum_coupon_ppm>1'000'000||b.maturity_days<=0)
                throw std::invalid_argument("P0_INVALID_NATIONAL_BUDGET");
        std::set<std::pair<std::string,std::string>> unique_bond_orders;
        for(const auto& order:bond_buyer_orders)
            if(order.country.empty()||order.buyer.empty()||
               order.max_daily_subscription<=0||order.cash_reserve<0||
               order.minimum_coupon_ppm<0||order.minimum_coupon_ppm>1'000'000||
               !unique_bond_orders.insert({order.country,order.buyer}).second)
                throw std::invalid_argument("P0_INVALID_BOND_MARKET_ORDER");
        std::set<std::string> seen_bond_secondary_orders;
        for(const auto& order:bond_secondary_asks)
            if(order.order_id.empty()||
               order.order_id.find(':')!=std::string::npos||
               !seen_bond_secondary_orders.insert("ASK:"+order.order_id).second||
               order.bond_id.empty()||order.seller.empty()||
               order.face_units<=0||order.minimum_price_ppm<=0||
               order.minimum_price_ppm>5'000'000)
                throw std::invalid_argument("P0_INVALID_SECONDARY_ASK");
        for(const auto& order:bond_secondary_bids)
            if(order.order_id.empty()||
               order.order_id.find(':')!=std::string::npos||
               !seen_bond_secondary_orders.insert("BID:"+order.order_id).second||
               order.bond_id.empty()||order.buyer.empty()||
               order.max_face_units<=0||order.maximum_price_ppm<=0||
               order.maximum_price_ppm>5'000'000)
                throw std::invalid_argument("P0_INVALID_SECONDARY_BID");
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
        std::set<std::tuple<std::string,int,int,int>> government_decision_days;
        for(const auto& change:government_formations) {
            if(change.country.empty()||change.incumbent_head.empty()||
               change.new_prime.empty()||change.legal_basis.empty()||
               !government_decision_days.insert({change.country,change.year,
                                                 change.month,change.day}).second)
                throw std::invalid_argument("P0_INVALID_P1_GOVERNMENT_FORMATION");
            (void)daily::to_day_index({change.year,change.month,change.day});
        }
        std::set<std::pair<std::string,daily::DayIndex>> dissolution_decisions;
        for(const auto& action:dissolutions) {
            if(action.contest_id.empty()||action.incumbent_head.empty()||
               action.legal_basis.empty()||
               daily::to_day_index(action.early_poll_date)<=
                   daily::to_day_index(action.decision_date)||
               !dissolution_decisions.insert({
                   action.contest_id,daily::to_day_index(action.decision_date)
               }).second)
                throw std::invalid_argument("P0_INVALID_P4_DISSOLUTION");
        }
        std::set<std::string> reform_targets;
        for(const auto& reform:election_term_reforms)
            if(reform.proposer.empty()||
               reform.amendment.contest_id.empty()||
               !reform_targets.insert(reform.amendment.contest_id).second||
               reform.amendment.effective_day<=
                   daily::to_day_index(reform.decision_date))
                throw std::invalid_argument("P0_INVALID_P4_TERM_REFORM");
        std::set<std::pair<daily::DayIndex,std::string>> confidence_motions;
        for(const auto& motion:no_confidence_votes) {
            if(motion.country.empty()||motion.supporting_parties.empty()||
               !confidence_motions.insert(
                   {daily::to_day_index(motion.decision_date),motion.country}).second)
                throw std::invalid_argument("P0_INVALID_NO_CONFIDENCE_MOTION");
        }
        std::set<std::tuple<daily::DayIndex,std::string,LegislativeAction>>
            p2_legislative_events;
        for(const auto& order:legislative_orders) {
            const auto day=daily::to_day_index(order.date);
            const auto bill_id=(order.action==LegislativeAction::introduce)?
                order.bill.id:order.bill_id;
            if(bill_id.empty()||
               static_cast<unsigned>(order.action)>
                   static_cast<unsigned>(LegislativeAction::withdraw)||
               !p2_legislative_events.insert({day,bill_id,order.action}).second||
               (order.action==LegislativeAction::introduce&&
                (order.bill.proposer.empty()||order.bill.country.empty()))||
               ((order.action==LegislativeAction::promulgate||
                 order.action==LegislativeAction::sign||
                 order.action==LegislativeAction::veto)&&order.head.empty())||
               (order.action==LegislativeAction::override_veto&&
                 order.supporting_parties.empty()))
                throw std::invalid_argument("P0_INVALID_P2_LEGISLATIVE_ORDER");
        }
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
    if(c.firm_bankrupt(p.employer))return;
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
            if(game_c.firm_bankrupt(facility.owner) ||
               payroll_needs_explicit_placement.contains(facility.owner) ||
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
            if(game_c.firm_bankrupt(site.owner) ||
               payroll_needs_explicit_placement.contains(site.owner) ||
               !game_c.has_market(site.region,site.good))continue;
            const auto& quote=game_c.market(site.region,site.good);
            decisions.extractors.push_back(Extractor{
                id,std::max<Quantity>(1,quote.target_stock)});
        }
        for(const auto& [id,research]:industry.researches) {
            if(game_c.firm_bankrupt(research.sponsor) ||
               research.reviewed||research.verified_work>=research.required_evidence||
               std::any_of(decisions.researches.begin(),decisions.researches.end(),
                    [&](const ResearchPolicy& p){return p.project==id;}))
                continue;
            decisions.researches.push_back(ResearchPolicy{id,1});
        }
        for(const auto& [id,project]:industry.constructions) {
            if(game_c.firm_bankrupt(project.client) ||
               project.ready||project.commissioned||
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
        // When a country has no explicit national budget, derive only the
        // obligations backed by current statutory services and contracts.
        // Unobserved transport rates, historical coupons and fictional tax
        // income are never inserted. An unknown bank rate disables endogenous
        // commercial bids; only real investor orders can finance such debt.
        if(decisions.auto_national_budgets_from_actual_services) {
            std::set<std::string> managed;
            for(const auto& b:decisions.national_budget_envelopes)
                managed.insert(b.country);
            for(const auto& b:decisions.sovereign_financing)
                managed.insert(b.country);
            for(const auto& [country,government]:game_c.governments()) {
                (void)government;
                if(managed.contains(country)||!game_c.has_country_treasury(
                        country))continue;
                const auto fiscal=game_c.country_fiscal(country);
                Money real_need=0;
                if(game_c.has_public_healthcare_right())
                    for(const auto& [id,svc]:game_c.public_institutions()) {
                        (void)id;
                        if(game_c.sovereignty(svc.region)!=country||
                           !game_c.has_market(svc.region,svc.good)||
                           game_c.phase_d_workforce(svc.operator_id)<=0)
                            continue;
                        const Quantity throughput=daily_budget::increment(
                            svc.monthly_slots,date.day,dim);
                        const Quantity on_hand=game_c.stock(
                            svc.operator_id,svc.region,svc.good);
                        const Quantity units=std::min(throughput,on_hand);
                        if(units>0)
                            real_need=phaseb::add_checked(real_need,
                                times(units,game_c.market(
                                    svc.region,svc.good).price));
                    }
                for(const auto& public_program:decisions.public_budget)
                    if(public_program.country==country)
                        real_need=phaseb::add_checked(real_need,
                            public_program.daily_spending_limit);
                if(real_need<=0&&fiscal.unpaid_coupons<=0)continue;
                const Money observed_receipts=std::max<Money>(
                    0,fiscal.realized_revenue);
                // The cumulative borrowing ceiling is anchored to
                // independently realized treasury receipts, NOT existing
                // bond principal plus receipts. Adding existing principal
                // here would create new unearned debt headroom every day.
                const Money unsecured_ceiling=std::max<Money>(
                    1,observed_receipts);
                const Money daily_financing_need=phaseb::add_checked(
                    real_need,fiscal.unpaid_coupons);
                NationalBudgetEnvelope envelope;
                envelope.country=country;
                envelope.reserve_cash=0;
                envelope.maximum_daily_spending=real_need;
                envelope.target_cash=fiscal.unpaid_coupons;
                envelope.maximum_daily_new_bonds=
                    std::max<Money>(1,daily_financing_need);
                envelope.maximum_total_bond_principal=
                    std::max<Money>(1,unsecured_ceiling);
                envelope.minimum_coupon_ppm=
                    game_c.annual_bank_interest_ppm();
                envelope.maximum_coupon_ppm=envelope.minimum_coupon_ppm;
                envelope.maturity_days=365; // declared game policy, not historical rate
                envelope.offer_to_operating_firms=
                    envelope.minimum_coupon_ppm>0;
                envelope.finance_real_public_services=true;
                decisions.national_budget_envelopes.push_back(
                    std::move(envelope));
            }
        }
        phasegh::World::Plan plan;
        // A solvent private buyer must really fund each bond. Mere
        // government demand for funds never creates an investor deposit.
        for(const auto& financing:decisions.sovereign_financing)
            plan[1].push_back([financing,today=world_.day_index()](phasegh::Stage& gh) {
                auto& c=gh.phase_c();
                if(!c.has_country_treasury(financing.country)||
                   !c.has_actor(financing.investor))return;
                const auto fiscal=c.country_fiscal(financing.country);
                if(fiscal.cash>=financing.target_treasury_liquidity)return;
                const Money live=c.sovereign_bond_principal(financing.country);
                if(live>=financing.max_total_bond_principal)return;
                const Money investor_free=std::max<Money>(0,
                    c.phase_d_cash_available(financing.investor)-
                    financing.investor_reserve);
                const Money face=std::min({
                    financing.target_treasury_liquidity-fiscal.cash,
                    financing.max_daily_bond_issue,
                    financing.max_total_bond_principal-live,
                    investor_free});
                if(face<=0)return;
                const auto id="P0:SOVEREIGN:"+financing.country+":"+
                              std::to_string(today);
                const auto& prime=c.government(financing.country).prime_minister;
                c.issue_sovereign_bond(id,financing.country,prime,
                    financing.investor,face,financing.annual_coupon_ppm,
                    today,phaseb::add_checked(today,financing.maturity_days));
            });
        // Each country's financing requirement is determined from its real
        // C treasury, not a fictitious construction point budget. Private
        // bids are sorted by required coupon and capped by actual cash.
        for(const auto& national:decisions.national_budget_envelopes)
            plan[1].push_back(
                [national,orders=decisions.bond_buyer_orders,
                 today=world_.day_index()](phasegh::Stage& gh) {
                    auto& c=gh.phase_c();
                    if(!c.has_country_treasury(national.country))return;
                    const auto fiscal=c.country_fiscal(national.country);
                    const Money desired=phaseb::add_checked(
                        national.target_cash,national.maximum_daily_spending);
                    if(fiscal.cash>=desired||
                       fiscal.bond_principal>=
                         national.maximum_total_bond_principal)return;
                    Money want=std::min({
                        desired-fiscal.cash,
                        national.maximum_daily_new_bonds,
                        national.maximum_total_bond_principal-
                            fiscal.bond_principal});
                    std::vector<BondBuyerOrder> bids;
                    for(const auto& order:orders)
                        if(order.country==national.country)
                            bids.push_back(order);
                    if(national.offer_to_operating_firms&&
                       national.minimum_coupon_ppm>0&&
                       national.maximum_coupon_ppm>=
                         c.annual_bank_interest_ppm()) {
                        std::set<std::string> contacted;
                        for(const auto& [site,f]:c.facilities()) {
                            (void)site;
                            const auto& owner=f.owner;
                            if(!contacted.insert(owner).second||
                               c.firm_bankrupt(owner)||!c.has_actor(owner)||
                               c.has_government(owner)||
                               owner==fiscal.treasury ||
                               std::any_of(bids.begin(),bids.end(),
                                    [&](const auto& bid){
                                        return bid.buyer==owner;
                                    }))continue;
                            const Money free=std::max<Money>(0,
                                c.phase_d_cash_available(owner));
                            if(free<=1)continue;
                            const Money reserve=free-free/4;
                            if(free-reserve<=0)continue;
                            bids.push_back({national.country,owner,
                                free-reserve,reserve,
                                c.annual_bank_interest_ppm()});
                        }
                    }
                    std::sort(bids.begin(),bids.end(),
                        [](const auto& a,const auto& b) {
                            return std::tuple{a.minimum_coupon_ppm,a.buyer}<
                                   std::tuple{b.minimum_coupon_ppm,b.buyer};
                        });
                    for(const auto& bid:bids) {
                        if(want<=0)break;
                        if(!c.has_actor(bid.buyer)||
                           c.firm_bankrupt(bid.buyer)||
                           bid.buyer==fiscal.treasury)continue;
                        const int coupon=std::max(
                            national.minimum_coupon_ppm,
                            bid.minimum_coupon_ppm);
                        if(coupon>national.maximum_coupon_ppm)continue;
                        const Money liquidity=c.phase_d_cash_available(
                            bid.buyer);
                        if(liquidity<=bid.cash_reserve)continue;
                        const Money purchase=std::min({
                            want,bid.max_daily_subscription,
                            liquidity-bid.cash_reserve});
                        if(purchase<=0)continue;
                        const auto id="P0:AUCTION:"+national.country+":"+
                            std::to_string(today)+":"+bid.buyer;
                        const auto& prime=
                            c.government(national.country).prime_minister;
                        c.issue_sovereign_bond(id,national.country,prime,
                                              bid.buyer,purchase,coupon,today,
                            phaseb::add_checked(today,national.maturity_days));
                        want-=purchase;
                    }
                });
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
               scenario_payroll.contains(contract.employer)||
               game_c.firm_bankrupt(contract.employer))continue;
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
                       now->wage_per_month<=0||c.firm_bankrupt(employer))return;
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
                [slice_id,employer,wallet,treasury,sovereign,purpose,
                 date,dim,reserved,today=world_.day_index()](phasegh::Stage& gh) {
                    auto& c=gh.phase_c();
                    if(*reserved>0)
                        c.phase_d_release_cash(employer,purpose,*reserved);
                    const auto* actual=c.people().job(slice_id);
                    if(!actual||actual->employer!=employer||
                       actual->count<=0||actual->wage_per_month<=0||
                       c.firm_bankrupt(employer))return;
                    const Money gross=daily_increment(
                        times(actual->count,actual->wage_per_month),
                        date.day,dim);
                    if(gross<=0)return;
                    c.phase_time_accrue_contract_wage(
                        purpose,employer,wallet,treasury,
                        c.has_government(sovereign)?sovereign:"",
                        gross,today);
                });
        }
        // Earned wages are accrued by the preceding Stage5 employer
        // commands. Creditors are now paid before optional public transfers,
        // dividends or other discretionary local business expenditures.
        plan[5].push_back([](phasegh::Stage& gh) {
            gh.phase_c().phase_time_settle_wage_arrears();
            gh.phase_c().phase_time_settle_bankrupt_reparations();
        });
        if(date.day==1 && decisions.autonomous_tax_step_ppm>0)
            plan[9].push_back(
                [step=decisions.autonomous_tax_step_ppm,
                 ceiling=decisions.autonomous_tax_ceiling_ppm](phasegh::Stage& gh) {
                    auto& c=gh.phase_c();
                    for(const auto& [country,gov]:c.governments()) {
                        if(!c.has_country_treasury(country) ||
                           c.has_pending_country_tax(country))continue;
                        const auto ballot=c.country_elections().find(country);
                        if(ballot==c.country_elections().end())continue;
                        const auto fiscal=c.country_fiscal(country);
                        if(fiscal.bank_debt<=fiscal.cash ||
                           fiscal.income_tax_ppm>=ceiling)continue;
                        // Never presume that opposition parties will vote
                        // together. Only a recorded single-party majority
                        // may authorize this bounded default fiscal response.
                        std::string majority;
                        People seats=0;
                        for(const auto& [party,n]:ballot->second.seats)
                            if(n>=gov.quorum && (majority.empty()||n>seats)) {
                                majority=party;
                                seats=n;
                            }
                        if(majority.empty())continue;
                        const auto legal_ceiling=c.p2_rule(
                            country,country,phasec::P2ClauseKind::income_tax_max_ppm)
                            .value_or(1'000'000);
                        const auto legal_floor=c.p2_rule(
                            country,country,phasec::P2ClauseKind::income_tax_min_ppm)
                            .value_or(0);
                        const int next=std::min({ceiling,
                            static_cast<int>(legal_ceiling),
                            fiscal.income_tax_ppm+step});
                        if(next>fiscal.income_tax_ppm&&next>=legal_floor)
                            c.propose_tax_law(country,gov.prime_minister,
                                               {majority},next);
                    }
                });
        // Keep this default state reaction deliberately defensive:
        // when residents already lacked real food, an existing food import
        // tariff can be removed. It cannot create a free subsidy, goods,
        // policies for unrelated commodities, or override an explicit
        // campaign CountryAI directive.
        std::set<std::string> scripted_food_tariffs;
        for(const auto& p:decisions.countries)
            if(p.good=="food")scripted_food_tariffs.insert(p.country);
        plan[0].push_back(
            [date,scripted_food_tariffs](phasegh::Stage& gh) {
                const auto& c=gh.phase_c();
                std::set<std::string> treated;
                for(const auto& [region,unmet]:c.regional_food_grievances()) {
                    if(unmet<=0||!c.has_market(region,"food"))continue;
                    const auto& country=c.sovereignty(region);
                    if(scripted_food_tariffs.contains(country) ||
                       !c.has_government(country) ||
                       !treated.insert(country).second)continue;
                    const auto key=std::make_pair(country,std::string("food"));
                    auto it=gh.diplomacy().tariffs.find(key);
                    if(it==gh.diplomacy().tariffs.end()||it->second.ppm==0)
                        continue;
                    const auto& prime=c.government(country).prime_minister;
                    gh.adopt_tariff_daily(
                        "P0:auto-food-import-relief:"+country+":"+
                        std::to_string(date.year)+"-"+std::to_string(date.month)+
                        "-"+std::to_string(date.day),
                        country,"food",0,prime);
                }
            });
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
                const auto bonus=c.p1_region_decree_bonus_ppm(
                     c.sovereignty(p.region),p.region,
                     phasec::P1DecreeKind::construction_drive);
                const Quantity extra=phaseb::add_checked(
                     phaseb::mul_checked(investment.maximum_daily_work/
                        1'000'000,bonus),
                     phaseb::mul_checked(investment.maximum_daily_work%
                        1'000'000,bonus)/1'000'000);
                const Quantity ceiling=phaseb::add_checked(
                     investment.maximum_daily_work,extra);
                const Quantity n=std::min({ceiling,
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
            [explicitly_managed_demand,date,dim,
             food_units=decisions.daily_food_units_per_person,
             clothing_monthly=decisions.monthly_clothing_units_per_person]
            (phasegh::Stage& gh) {
                auto& c=gh.phase_c();
                const auto& people=c.people();
                using NeedKey=std::tuple<std::string,Id,std::string>;
                std::map<NeedKey,Quantity> needs;
                for(const auto& [slice_id,pop]:people.slices()) {
                    if(pop.count<=0)continue;
                    const auto& d=people.demographic(pop.key.demographic);
                    const std::array<std::pair<std::string,Quantity>,2> baskets{{
                        {"food",times(pop.count,food_units)},
                        {"clothing",daily_increment(
                            times(pop.count,clothing_monthly),date.day,dim)}
                    }};
                    for(const auto& [good,amount]:baskets) {
                        if(amount<=0 ||
                           explicitly_managed_demand.contains({slice_id,good}))
                            continue;
                        auto& due=needs[{d.region,pop.key.household,good}];
                        due=phaseb::add_checked(due,amount);
                    }
                }
                const auto sellers=c.market_seller_index();
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
                        const auto market_sellers=sellers.find({region,good});
                        const Quantity purchased=c.clear_household_market(
                            wallet,region,good,still_due,
                            market_sellers==sellers.end()?
                                std::vector<std::string>{}:market_sellers->second);
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
                if(new_members>0&&
                   c.p2_rule(c.sovereignty(p.region),c.sovereignty(p.region),
                        phasec::P2ClauseKind::political_association_allowed)
                        .value_or(1)!=0)
                    c.organize_from_food_grievance(
                                      p.population_slice,p.interest_group,new_members);
                const People remaining=total-c.protest_participants(p.population_slice);
                const People protest=std::min({remaining,
                     c.group_members(p.population_slice,p.interest_group),
                     p.max_protesters_per_day});
                if(protest>0)c.protest(p.population_slice,p.interest_group,protest);
            });
        }
        // Human-controlled countries never receive autonomous political
        // decisions. Their own dated P0 commands remain the authority.
        if(!decisions.player_controlled_countries.empty())
            plan[9].push_back([countries=decisions.player_controlled_countries]
                  (phasegh::Stage& gh) {
                for(const auto& country:countries)
                    gh.phase_c().p1_mark_player_country(country,true);
            });
        // Execute independent national parliamentary, local and elected-head
        // contests only on their scheduled day. Voters and party preference
        // are read from actual Phase B/P3 state in the Stage9 transaction.
        for(const auto& contest_id:game_c.p4_due_contests(world_.day_index())) {
            plan[9].push_back([contest_id,today=world_.day_index(),
                               date](phasegh::Stage& gh) {
                auto& c=gh.phase_c();
                const auto& contest=c.p4_contest(contest_id);
                const auto next=daily::to_day_index(
                    daily::add_calendar_months(date,contest.term_months));
                c.p4_hold_contest(contest_id,today,next,
                                  c.p4_autonomous_ballot(contest_id));
            });
        }
        for(const auto& election:decisions.elections) {
            if(date.year!=election.year||date.month!=election.month||date.day!=election.day)continue;
            plan[9].push_back([election](phasegh::Stage& gh){
                const auto& registered=gh.phase_c().p4_contests();
                for(const auto& [id,contest]:registered) {
                    (void)id;
                    if(contest.country==election.country&&
                       contest.office==phasec::P4Office::legislature)
                        throw std::invalid_argument(
                            "P0_P4_LEGACY_PARLIAMENTARY_ELECTION_CONFLICT");
                }
                auto& c=gh.phase_c();
                const auto& pop=c.people();
                const auto& g=c.government(election.country);
                People incumbent=0,reform=0;
                for(const auto& [id,s]:pop.slices()){
                    const auto& d=pop.demographic(s.key.demographic);
                    if(c.sovereignty(d.region)!=election.country||d.citizenship!="citizen"||
                       d.service==phaseb::Service::active||d.service==phaseb::Service::pow||
                       d.service==phaseb::Service::missing||
                       (d.sex==phaseb::Sex::female&&
                        c.p2_rule(election.country,election.country,
                           phasec::P2ClauseKind::female_suffrage_allowed)
                           .value_or(1)==0)||
                       pop.age_months_slice(id)<static_cast<std::int64_t>(c.p2_voting_age(election.country))*12)
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
        // P2 bills run through the same Stage9 government/legislative
        // authority. An Act may be introduced, approved and promulgated in
        // distinct political decisions without rewriting economic ledgers.
        for(const auto& order:decisions.legislative_orders) {
            if(date!=order.date)continue;
            plan[9].push_back([order,today=world_.day_index()]
                             (phasegh::Stage& gh) {
                auto& c=gh.phase_c();
                switch(order.action) {
                    case LegislativeAction::sign:
                        c.p2_sign_bill(order.bill_id,order.head,today);break;
                    case LegislativeAction::veto:
                        c.p2_veto_bill(order.bill_id,order.head,today);break;
                    case LegislativeAction::override_veto:
                        c.p2_override_veto(order.bill_id,
                           order.supporting_parties,
                           order.upper_supporting_parties,today);break;
                    case LegislativeAction::withdraw:
                        c.p2_cancel_bill(order.bill_id,today);break;
                    case LegislativeAction::introduce:
                        c.p2_submit_bill(order.bill,today);
                        break;
                    case LegislativeAction::approve:
                        c.p2_approve_bill(order.bill_id,
                                          order.supporting_parties);
                        break;
                    case LegislativeAction::promulgate:
                        c.p2_promulgate_bill(order.bill_id,order.head,today);
                        break;
                    case LegislativeAction::approve_upper:
                        c.p2_approve_upper_bill(order.bill_id,
                                                order.supporting_parties);
                        break;
                }
            });
        }
        for(const auto& action:decisions.dissolutions) {
            if(date!=action.decision_date)continue;
            plan[9].push_back([action,today=world_.day_index()]
                             (phasegh::Stage& gh) {
                gh.phase_c().p4_dissolve(
                    action.contest_id,action.incumbent_head,
                    action.legal_basis,today,
                    daily::to_day_index(action.early_poll_date));
            });
        }
        for(const auto& reform:decisions.election_term_reforms) {
            if(date!=reform.decision_date)continue;
            plan[9].push_back([reform,today=world_.day_index()]
                             (phasegh::Stage& gh) {
                gh.phase_c().p4_schedule_term_amendment(
                    reform.amendment,reform.proposer,
                    reform.supporting_parties,today);
            });
        }
        for(const auto& motion:decisions.no_confidence_votes) {
            if(date!=motion.decision_date)continue;
            plan[9].push_back([motion,today=world_.day_index()]
                              (phasegh::Stage& gh) {
                gh.phase_c().p1_vote_no_confidence(
                    motion.country,motion.supporting_parties,today);
            });
        }
        for(const auto& formation:decisions.government_formations) {
            if(date.year!=formation.year||date.month!=formation.month||
               date.day!=formation.day)continue;
            plan[9].push_back(
                [formation,tomorrow=world_.day_index()+1](phasegh::Stage& gh) {
                    gh.phase_c().schedule_p1_cabinet_daily(
                        formation.country,formation.incumbent_head,
                        formation.new_prime,
                        phasec::P1Coalition{formation.ruling_groups,
                                           formation.ruling_parties,
                                           formation.caretaker},
                        tomorrow,formation.legal_basis);
                });
        }
        for(const auto& order:decisions.political_orders) {
            if(date!=order.date)continue;
            plan[9].push_back([order,today=world_.day_index()]
                              (phasegh::Stage& gh) {
                auto& c=gh.phase_c();
                switch(order.kind) {
                  case PoliticalOrderKind::activate_mandate:
                    c.p1_activate_mandate(order.mandate,today);break;
                  case PoliticalOrderKind::cancel_mandate:
                    c.p1_deactivate_mandate(order.mandate_id);break;
                  case PoliticalOrderKind::event_decision:
                    c.p1_resolve_political_event(
                       order.event_id,order.choice,today);break;
                  case PoliticalOrderKind::register_lobby:
                    c.p1_register_lobby(order.lobby,today);break;
                }
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
        // Ordinary member governments honour an already-active real treaty
        // obligation when their actual treasury can pay. This does not vote
        // for new charters, grant memberships or fabricate an organization.
        plan[5].push_back([](phasegh::Stage& gh) {
            auto& c=gh.phase_c();
            for(const auto& [id,org]:gh.diplomacy().organizations) {
                if(!org.active||org.dues_per_member<=0||
                   !c.has_actor("ORG:"+id))continue;
                for(const auto& member:org.members) {
                    if(!c.has_country_treasury(member))continue;
                    const auto key=std::make_pair(c.month(),member);
                    const auto it=org.paid.find(key);
                    const Money prior=it==org.paid.end()?0:it->second;
                    if(prior>=org.dues_per_member)continue;
                    const Money free=std::max<Money>(0,
                        c.phase_d_cash_available(
                            c.country_financial_wallet(member)));
                    const Money payable=std::min(
                        org.dues_per_member-prior,free);
                    if(payable>0)gh.pay_dues(id,member,payable);
                }
            }
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
        // A claim does not itself declare war. Only an unresolved
        // documented crisis with both sides' real, armed deployed units at
        // the same contested province may create this state's lawful war.
        // The actual prime minister is read from the current government.
        if(decisions.allow_endogenous_crisis_wars)
            plan[9].push_back([](phasegh::Stage& gh) {
                const auto& d=gh.diplomacy();
                const auto& c=gh.phase_c();
                for(const auto& [id,crisis]:d.crises) {
                    if(crisis.status!=phasegh::CrisisStatus::active||
                       crisis.effective_month>=c.month() ||
                       !c.has_government(crisis.claimant)||
                       !c.has_government(crisis.respondent))
                        continue;
                    const auto& pm=c.government(crisis.claimant).prime_minister;
                    (void)gh.begin_war_from_crisis(id,crisis.claimant,pm);
                }
            });
        if(decisions.allow_autonomous_war_peace)
            plan[9].push_back([today=world_.day_index()](phasegh::Stage& gh) {
                const auto& c=gh.phase_c();
                for(const auto& [id,war]:gh.diplomacy().wars) {
                    if(war.ended_day>=0||
                       !c.has_government(war.attacker)||
                       !c.has_government(war.defender))continue;
                    const auto fronts=gh.war_front_regions(war);
                    if(fronts.empty())continue;
                    // Do not call a ceasefire while either army is actively
                    // armed on any of its documented real fronts. After a
                    // legally effective armistice, continued non-combat is
                    // sufficient for a later permanent peace proposal.
                    bool fighting_capability=false;
                    if(!war.armistice)
                        for(const auto& [unit,deployment]:c.army_deployments()) {
                            if(c.p5_guerrilla_is_insurgent_unit(unit)||
                               c.active_strength(unit)<=0||
                               c.stock(unit,deployment.region,"ammunition")<=0||
                               std::find(fronts.begin(),fronts.end(),
                                   deployment.region)==fronts.end())continue;
                            if(deployment.country==war.attacker||
                               deployment.country==war.defender) {
                                fighting_capability=true;
                                break;
                            }
                        }
                    const auto treaty_id=
                        std::string(war.armistice?"AUTO:PEACE:":"AUTO:CEASEFIRE:")+id;
                    if(gh.diplomacy().treaties.contains(treaty_id))continue;
                    const auto idle_from=std::max(war.started_day,
                                                  war.last_combat_day);
                    // An armed but wholly inactive front can also be a
                    // stalemate. Do not lock countries into endless war
                    // merely because both sides still own cartridges.
                    const auto required_idle=war.armistice?90:
                        (fighting_capability?90:30);
                    if(today<idle_from||today-idle_from<required_idle)
                        continue;
                    phasegh::TreatyClause term;
                    term.id="end-hostilities";
                    term.obligor=war.attacker;
                    term.beneficiary=war.defender;
                    term.kind=war.armistice?
                        phasegh::ClauseKind::peace_settlement:
                        phasegh::ClauseKind::ceasefire;
                    const auto& first_pm=
                        c.government(war.attacker).prime_minister;
                    const auto& second_pm=
                        c.government(war.defender).prime_minister;
                    gh.propose_treaty_package(treaty_id,
                        {war.attacker,war.defender},{term},
                        war.attacker,first_pm);
                    gh.ratify_package_member(treaty_id,
                        war.defender,second_pm);
                }
            });
        if(decisions.allow_operational_battle_ai) {
            std::set<std::string> manual;
            for(const auto& battle:decisions.engagements) {
                manual.insert(battle.attacker);
                manual.insert(battle.defender);
            }
            plan[10].push_back([manual](phasegh::Stage& gh) {
                frontai::allocate_marches(gh,manual);
            });
        }
        for(const auto& combat:decisions.engagements)
            plan[10].push_back([combat](phasegh::Stage& gh){
                fight(gh.phase_c(),combat);
            });
        if(decisions.allow_operational_battle_ai) {
            std::set<std::string> manual_forces;
            for(const auto& combat:decisions.engagements) {
                manual_forces.insert(combat.attacker);
                manual_forces.insert(combat.defender);
            }
            plan[10].push_back([manual_forces](phasegh::Stage& gh) {
                auto& c=gh.phase_c();
                std::set<std::string> used=manual_forces;
                for(const auto& [id,war]:gh.diplomacy().wars) {
                    if(!war.active||war.armistice)continue;
                    for(const auto& front:gh.war_front_regions(war)) {
                    std::string attacker,defender;
                    for(const auto& [unit,deployment]:c.army_deployments()) {
                        if(deployment.region!=front||used.contains(unit)||
                           c.p5_guerrilla_is_insurgent_unit(unit)||
                           c.active_strength(unit)<=0||
                           c.stock(unit,front,"ammunition")<=0)continue;
                        if(deployment.country==war.attacker&&attacker.empty())
                            attacker=unit;
                        else if(deployment.country==war.defender&&defender.empty())
                            defender=unit;
                    }
                    if(attacker.empty()||defender.empty())continue;
                    const auto first_active=[&](const std::string& unit)->Id {
                        for(const auto& [slice,count]:c.unit_slices(unit))
                            if(c.people().has_slice(slice)&&
                               c.people().slice(slice).count==count&&count>0)
                                return slice;
                        return 0;
                    };
                    const Id a=first_active(attacker);
                    const Id b=first_active(defender);
                    if(!a||!b)continue;
                    const auto ammo_before=c.stock(attacker,front,"ammunition");
                    const Engagement engagement{attacker,defender,front,a,b,
                                                1'000'000};
                    fight(c,engagement);
                    if(c.stock(attacker,front,"ammunition")<ammo_before)
                        gh.record_actual_war_engagement(id);
                    used.insert(attacker);
                    used.insert(defender);
                    }
                }
            });
        }
        // Secondary order book for already-issued sovereign securities.
        // Ask minimums and bid maximums are actual actor policies; matching
        // transfers existing cash and registered face value, not reserves
        // printed by an invented dealer or a synthetic market-clearing price.
        if(!decisions.bond_secondary_asks.empty()&&
           !decisions.bond_secondary_bids.empty())
            plan[7].push_back(
                [asks=decisions.bond_secondary_asks,
                 bids=decisions.bond_secondary_bids](phasegh::Stage& gh) {
                    auto& c=gh.phase_c();
                    struct Ask {
                        BondSecondaryAsk limit;
                        Money left{};
                    };
                    struct Bid {
                        BondSecondaryBid limit;
                        Money left{};
                    };
                    std::map<std::string,std::vector<Ask>> offers;
                    std::map<std::string,std::vector<Bid>> demands;
                    for(const auto& ask:asks)
                        offers[ask.bond_id].push_back({ask,ask.face_units});
                    for(const auto& bid:bids)
                        demands[bid.bond_id].push_back({bid,bid.max_face_units});
                    for(auto& [bond_id,book]:offers) {
                        if(!c.sovereign_bonds().contains(bond_id)||
                           !demands.contains(bond_id))continue;
                        auto& take=demands.at(bond_id);
                        std::sort(book.begin(),book.end(),
                            [](const Ask& a,const Ask& b){
                                return std::tuple{a.limit.minimum_price_ppm,
                                                  a.limit.order_id}<
                                       std::tuple{b.limit.minimum_price_ppm,
                                                  b.limit.order_id};
                            });
                        std::sort(take.begin(),take.end(),
                            [](const Bid& a,const Bid& b){
                                return std::tuple{-a.limit.maximum_price_ppm,
                                                  a.limit.order_id}<
                                       std::tuple{-b.limit.maximum_price_ppm,
                                                  b.limit.order_id};
                            });
                        for(auto& ask:book) {
                            if(!c.has_actor(ask.limit.seller))continue;
                            for(auto& bid:take) {
                                if(ask.left<=0)break;
                                if(bid.left<=0||
                                   bid.limit.buyer==ask.limit.seller||
                                   bid.limit.maximum_price_ppm<
                                       ask.limit.minimum_price_ppm||
                                   !c.has_actor(bid.limit.buyer)||
                                   c.firm_bankrupt(bid.limit.buyer))continue;
                                const auto command="P0:BOND-SECONDARY:"+
                                    ask.limit.order_id+":"+
                                    bid.limit.order_id;
                                if(c.command_processed(command))continue;
                                const auto asset="bond:"+bond_id;
                                const Money available=c.shares(
                                    asset,ask.limit.seller);
                                Money maximum=std::min({ask.left,bid.left,
                                                        available});
                                if(maximum<=0)continue;
                                const Money free=std::max<Money>(0,
                                    c.phase_d_cash_available(bid.limit.buyer));
                                if(free<=0)continue;
                                const Money rate=ask.limit.minimum_price_ppm;
                                const auto affordable=[&](Money qty)->bool {
                                    if(qty<=0)return false;
                                    const Money major=qty/1'000'000;
                                    if(major>free/rate)return false;
                                    const Money hi=major*rate;
                                    const Money lo=(qty%1'000'000)*rate/1'000'000;
                                    return lo<=free-hi;
                                };
                                Money left=1,right=maximum;
                                Money fill=0;
                                while(left<=right) {
                                    const Money mid=left+(right-left)/2;
                                    if(affordable(mid)) {
                                        fill=mid;
                                        if(mid==maximum)break; // no signed overflow
                                        left=mid+1;
                                    } else right=mid-1;
                                }
                                if(fill<=0)continue;
                                const Money gross=phaseb::add_checked(
                                    phaseb::mul_checked(fill/1'000'000,rate),
                                    phaseb::mul_checked(fill%1'000'000,rate)/
                                    1'000'000);
                                if(gross<=0)continue;
                                c.command_once(command,
                                    [&](phasec::IntegratedWorld& state) {
                                        state.exchange_sovereign_bond(
                                            bond_id,ask.limit.seller,
                                            bid.limit.buyer,fill,
                                            static_cast<int>(rate));
                                    });
                                ask.left-=fill;
                                bid.left-=fill;
                            }
                        }
                    }
                });
        // Liquidation buyers commit real cash against quoted goods and
        // commissioned plant. These actions precede final Stage8 creditor
        // retirement; employees have already received the Stage5 payments.
        for(const auto& bid:decisions.liquidation_stock_bids)
            plan[7].push_back([bid](phasegh::Stage& gh) {
                const auto& c=gh.phase_c();
                if(!c.firm_bankrupt(bid.firm)||!c.has_actor(bid.buyer)||
                   !c.has_market(bid.region,bid.good))return;
                (void)gh.phase_c().sell_bankrupt_inventory(
                    bid.firm,bid.buyer,bid.region,bid.good,
                    bid.maximum_daily_units,bid.maximum_unit_price);
            });
        for(const auto& bid:decisions.liquidation_factory_bids)
            plan[7].push_back([bid](phasegh::Stage& gh) {
                (void)gh.phase_d().liquidate_commissioned_factory(
                    bid.site,bid.buyer,bid.actual_bid_price);
            });
        // Operating businesses may buy an insolvent supplier's existing
        // merchandise only to fill their own observed physical stock target,
        // at the true current regional price and with surplus actual cash.
        if(decisions.allow_market_liquidation_of_real_stock)
            plan[7].push_back([](phasegh::Stage& gh) {
                auto& c=gh.phase_c();
                const auto index=c.market_seller_index();
                for(const auto& [key,sellers]:index) {
                    const auto& [region,good]=key;
                    if(!c.has_market(region,good))continue;
                    const auto& quote=c.market(region,good);
                    if(quote.price<=0||quote.target_stock<=0)continue;
                    for(const auto& insolvent:sellers) {
                        if(!c.firm_bankrupt(insolvent))continue;
                        for(const auto& [id,site]:c.facilities()) {
                            (void)id;
                            const auto& buyer=site.owner;
                            if(site.region!=region||buyer==insolvent||
                               c.firm_bankrupt(buyer))continue;
                            const Quantity current=c.stock(buyer,region,good);
                            if(current>=quote.target_stock)continue;
                            const Money free=c.phase_d_cash_available(buyer);
                            const Quantity affordable=free>0?
                                (free/quote.price)/4:0;
                            const Quantity qty=std::min({
                                quote.target_stock-current,affordable,
                                c.phase_d_material_available(
                                    insolvent,region,good)});
                            if(qty>0)
                                (void)c.sell_bankrupt_inventory(
                                    insolvent,buyer,region,good,qty,
                                    quote.price);
                        }
                    }
                }
            });
        // Outstanding wages take precedence over unsecured bank principal.
        // Credits backed by G are retired through their actual facility first,
        // so G drawn balances cannot silently diverge from C loan principal.
        if(decisions.retire_bank_debt_after_wages)
            plan[8].push_back([](phasegh::Stage& gh) {
                auto& c=gh.phase_c();
                for(const auto& [firm,day]:c.bankrupt_firms()) {
                    (void)day;
                    if(c.outstanding_worker_wages(firm)>0)continue;
                    Money affordable=std::min(c.outstanding_loan(firm),
                        std::max<Money>(0,c.phase_d_cash_available(firm)));
                    if(affordable<=0)continue;
                    for(const auto& [id,line]:gh.commerce().credit) {
                        if(affordable<=0)break;
                        if(!line.active||line.borrower!=firm||
                           line.drawn<=0)continue;
                        const Money paid=std::min(affordable,line.drawn);
                        gh.repay_trade_credit(id,paid);
                        affordable-=paid;
                    }
                    if(affordable>0)c.repay(firm,affordable);
                }
            });
        if(decisions.allow_operational_battle_ai)
            plan[10].push_back([](phasegh::Stage& gh) {
                frontai::occupy_decisive_fronts(gh);
            });
        // Aggregate all policy categories under a single national cash
        // envelope. Real health providers are funded from existing treasury
        // deposits, actual medicines, and staffed operating facilities.
        std::map<std::string,NationalBudgetEnvelope> national_envelopes;
        for(const auto& b:decisions.national_budget_envelopes)
            national_envelopes.emplace(b.country,b);
        auto budget_spent_today=
            std::make_shared<std::map<std::string,Money>>();
        for(const auto& [country,budget]:national_envelopes) {
            if(!budget.finance_real_public_services||
               budget.maximum_daily_spending<=0)continue;
            plan[5].push_back([budget,date,dim,budget_spent_today]
                              (phasegh::Stage& gh) {
                auto& c=gh.phase_c();
                if(!c.has_country_treasury(budget.country)||
                   !c.has_public_healthcare_right())return;
                const auto& treasury=c.country_treasury(budget.country);
                for(const auto& [id,svc]:c.public_institutions()) {
                    (void)id;
                    if(c.sovereignty(svc.region)!=budget.country ||
                       svc.operator_id==treasury||
                       !c.has_market(svc.region,svc.good))continue;
                    const auto available_slots=daily_budget::remaining(
                        svc.monthly_slots,svc.provided,date.day,dim);
                    if(available_slots<=0||c.phase_d_workforce(
                            svc.operator_id)<=0||
                       c.stock(svc.operator_id,svc.region,svc.good)<=0)
                        continue;
                    const Money unit=c.market(svc.region,svc.good).price;
                    if(unit<=0)continue;
                    const auto daily_left=budget.maximum_daily_spending-
                        (*budget_spent_today)[budget.country];
                    const auto cash_left=c.phase_d_cash_available(treasury)-
                        budget.reserve_cash;
                    if(daily_left<=0||cash_left<=0)break;
                    const auto intended=phaseb::mul_checked(
                        std::min<Quantity>(available_slots,
                            c.stock(svc.operator_id,svc.region,svc.good)),unit);
                    const Money paid=std::min({intended,daily_left,cash_left});
                    if(paid<=0)continue;
                    c.spend_country_budget(budget.country,svc.operator_id,paid);
                    (*budget_spent_today)[budget.country]=
                        phaseb::add_checked(
                            (*budget_spent_today)[budget.country],paid);
                }
            });
        }
        // Public obligations are paid directly from the real C fiscal
        // wallet to an existing service provider with a declared policy cap.
        // An absent provider or empty treasury cannot produce "free" output.
        for(const auto& program:decisions.public_budget)
            plan[5].push_back([program,national_envelopes,budget_spent_today]
                              (phasegh::Stage& gh) {
                auto& c=gh.phase_c();
                if(!c.has_country_treasury(program.country)||
                   !c.has_actor(program.recipient))return;
                const auto& treasury=c.country_treasury(program.country);
                if(treasury==program.recipient)return;
                const Money free=c.phase_d_cash_available(treasury);
                if(free<=program.treasury_cash_reserve)return;
                Money amount=std::min(program.daily_spending_limit,
                                      free-program.treasury_cash_reserve);
                auto budget=national_envelopes.find(program.country);
                if(budget!=national_envelopes.end()) {
                    const auto& envelope=budget->second;
                    const auto remaining=envelope.maximum_daily_spending-
                                           (*budget_spent_today)[program.country];
                    const auto treasury_left=free-envelope.reserve_cash;
                    amount=std::min({amount,
                        std::max<Money>(0,remaining),
                        std::max<Money>(0,treasury_left)});
                }
                if(amount>0) {
                    c.spend_country_budget(program.country,program.recipient,amount);
                    (*budget_spent_today)[program.country]=
                        phaseb::add_checked(
                            (*budget_spent_today)[program.country],amount);
                }
            });
        // Only observed profitable-demand-shortfall industrial examples may
        // open a new project. The D construction ledger will still require
        // actual investor cash, materials, employees, payments and WIP before
        // Stage0 creates the new facility on a later date.
        if(decisions.allow_endogenous_factory_expansion && date.day==1)
            plan[9].push_back([reserve=decisions.autonomous_factory_cash_reserve,
                              today=world_.day_index()](phasegh::Stage& gh) {
                const auto& domain=gh.phase_d().state();
                for(const auto& [id,p]:domain.constructions) {
                    if(!p.commissioned)continue;
                    for(const auto& [recipe_id,recipe]:domain.recipes) {
                        if(recipe.site!=p.target_site)continue;
                        const auto expansion_key=id+":"+std::to_string(today);
                        (void)gh.phase_d().propose_firm_expansion(
                            id,recipe_id,expansion_key,reserve);
                        break;
                    }
                }
            });
        // Actual creditor entries remain in C's balance sheet until paid.
        // No additional late-day payment pass is needed: employee wages
        // are prioritized before discretionary spending in Stage5.
        plan[8].push_back([today=world_.day_index()](phasegh::Stage& gh) {
            gh.phase_c().phase_time_resolve_firm_insolvency(today);
        });
        plan[8].push_back([](phasegh::Stage& gh) {
            gh.phase_c().phase_time_distribute_liquidation_equity();
        });
        plan[8].push_back([today=world_.day_index()](phasegh::Stage& gh) {
            gh.phase_c().phase_time_close_empty_bankrupt_estates(today);
        });
        // A stocked national arsenal in the exact same province-region as
        // its deployed unit can issue ammunition without a fictional shipping
        // trip. Transfer only physical C inventory, never free army supply.
        if(decisions.allow_autonomous_army_resupply)
            plan[7].push_back([](phasegh::Stage& gh) {
                auto& c=gh.phase_c();
                std::set<std::string> at_war;
                for(const auto& [id,war]:gh.diplomacy().wars) {
                    (void)id;
                    if(war.active&&!war.armistice) {
                        at_war.insert(war.attacker);
                        at_war.insert(war.defender);
                    }
                }
                for(const auto& [unit,d]:c.army_deployments()) {
                    if(c.p5_guerrilla_is_insurgent_unit(unit)||
                       !at_war.contains(d.country)||
                       !c.has_country_treasury(d.country))continue;
                    const auto& treasury=c.country_treasury(d.country);
                    if(treasury==unit)continue;
                    const Quantity desired=c.active_strength(unit);
                    const Quantity existing=c.stock(unit,d.region,"ammunition");
                    if(desired<=existing)continue;
                    const Quantity free=c.phase_d_material_available(
                        treasury,d.region,"ammunition");
                    const Quantity issued=std::min(desired-existing,free);
                    if(issued>0)
                        c.phase_g_transfer_goods(
                            treasury,unit,d.region,"ammunition",issued);
                }
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
        // Build one provisional stock/cash/edge allocation for all civilian
        // and military freight. Avoid copying an enormous 30k-province
        // world on days with no new transport orders.
        phasef::DayPlanningReservations freight_plan;
        std::vector<daily::Command> jobs;
        for(const auto& lane:decisions.freight_lanes)
            if(auto job=phasef::plan_next_hop(world_,lane,freight_plan))
                jobs.push_back(std::move(*job));
        if(decisions.allow_autonomous_army_resupply) {
            auto supplies=phasef::plan_direct_army_supply(world_,freight_plan);
            jobs.insert(jobs.end(),
                        std::make_move_iterator(supplies.begin()),
                        std::make_move_iterator(supplies.end()));
        }
        if(jobs.empty())return world_.advance_day(plan);
        daily::World staged=world_;
        for(auto& cargo:jobs)staged.enqueue(date,std::move(cargo));
        const auto report=staged.advance_day(plan);
        world_=std::move(staged);
        return report;
    }
};
} // namespace vic25::p0
