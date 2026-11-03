#pragma once

// Phase C integration vertical slice. Reuses Phase B as the ONLY population
// headcount and job authority, and S0's checked double-entry and stock ledgers.
// Implements actual staged transactions; high-detail political/war models remain
// separate future modules (see docs/phase-c/integration/).
#include "vic25/phase_b.hpp"
#include "vic25/ledgers.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace vic25::phasec {
using Id = phaseb::Id;
using People = phaseb::People;
using Money = std::int64_t;
inline void ensure(bool yes, const char* msg) {
    if (!yes) throw std::invalid_argument(msg);
}
inline Money positive_product(Money a, Money b) {
    ensure(a >= 0 && b >= 0, "negative amount");
    return phaseb::mul_checked(a, b);
}

// Scope: one chamber per country, with government continuity across office changes.
struct GovernmentState {
    std::string country, head_of_state, prime_minister;
    People chamber_seats{}, quorum{};
    std::uint64_t revision{1};
};
struct CabinetChange {
    std::string country, new_head, new_prime_minister;
    bool replace_head{}, replace_prime{};
};
enum class EntityKind : std::uint8_t { actor, region, unit, formation, government, site, market };
struct TypedEntityId {
    EntityKind kind{};
    std::uint64_t id{};
    auto operator<=>(const TypedEntityId&) const = default;
};
struct PublicInstitution {
    std::string id, operator_id, region, good;
    People monthly_slots{}, patients_per_worker{3}, provided{};
};
struct Reparation {
    std::string id, payer, recipient;
    Money total{}, remaining{};
    std::int64_t effective_month{};
    bool recognized{};
};
struct ProductionFacility {
    std::string id, owner, region;
    Quantity capacity{}, damaged{};
};
struct MarketQuote {
    Money price{}, floor{1}, ceiling{}, target_stock{};
    Quantity solvent_demand{}, delivered{}, unmet{}, stock_offered{};
};
struct ElectionResult {
    People eligible{}, registered{}, cast{}, invalid{}, valid{};
    std::map<std::string, People> votes;
    std::map<std::string, People> seats;
    People total_seats{};
};
struct Shipment {
    std::string id, from, to, origin, destination, good;
    Quantity quantity{}, loss{};
    std::int64_t arrival_month{};
    bool delivered{};
};
struct UnitAssignment {
    std::string unit;
    Id slice{};
    People count{};
};
struct CombatLoss {
    People killed{}, acute{}, long_term{}, pow{}, missing{};
    Quantity ammunition{};
};
struct AdvanceReport {
    std::int64_t starting_month{};
    std::int64_t finished_month{};
    People population{};
    std::uint64_t commit_id{};
};

class IntegratedWorld {
public:
    using Task = std::function<void(IntegratedWorld&)>;
    using Plan = std::array<std::vector<Task>, 12>;

    explicit IntegratedWorld(std::int64_t month = 24000) : people_(month) {
        money_.add_account("BANK:deposit", "BANK", AccountType::liability);
        money_.add_account("BANK:reserves", "BANK", AccountType::asset);
        money_.add_account("BANK:loan", "BANK", AccountType::asset);
        money_.add_account("BANK:loan_loss", "BANK", AccountType::expense);
    }

    [[nodiscard]] const phaseb::World& people() const { return people_; }
    phaseb::World& seed_population() {
        ensure(stage_ == -1 && commit_id_ == 0, "population seed after simulation start");
        return people_;
    }
    [[nodiscard]] std::int64_t month() const { return people_.month(); }
    [[nodiscard]] std::uint64_t commit_id() const { return commit_id_; }
    [[nodiscard]] const FinancialLedger& financial() const { return money_; }
    [[nodiscard]] const InventoryLedger& inventory() const { return goods_; }
    [[nodiscard]] const std::vector<std::string>& journal() const { return events_; }
    [[nodiscard]] const ElectionResult& election() const { return last_election_; }
    [[nodiscard]] int voting_age() const { return voting_age_years_; }
    [[nodiscard]] const std::string& military_control(const std::string& region) const {
        return military_control_.at(region);
    }
    [[nodiscard]] const std::string& sovereignty(const std::string& region) const {
        return sovereignty_.at(region);
    }

    void seed_actor(const std::string& who, Money amount) {
        ensure(stage_ == -1 && commit_id_ == 0, "actor seed after simulation start");
        ensure(!who.empty() && !actors_.contains(who) && amount >= 0, "invalid actor seed");
        register_typed(EntityKind::actor,who);
        actors_.insert(who);
        for (const auto& [suffix, type] : std::array<std::pair<std::string, AccountType>, 7>{{
            {"cash", AccountType::asset}, {"capital", AccountType::equity},
            {"revenue", AccountType::revenue}, {"expense", AccountType::expense},
            {"debt", AccountType::liability},
            {"reparations_payable", AccountType::liability},
            {"reparations_receivable", AccountType::asset}}}) {
            money_.add_account(account(who, suffix), who, type);
        }
        if (amount) {
            money_.post({unique("seed"), "opening cash", {
                {account(who, "cash"), amount, 0}, {account(who, "capital"), 0, amount}}});
            money_.post({unique("bank-seed"), "matching opening deposits", {
                {"BANK:reserves", amount, 0}, {"BANK:deposit", 0, amount}}});
        }
    }
    void seed_stock(std::string holder, std::string region, std::string good, Quantity qty) {
        ensure(stage_ == -1 && commit_id_ == 0, "stock seed after simulation start");
        ensure(qty > 0, "invalid initial stock");
        regions_with_initial_stock_.insert(region);
        tracked_goods_.insert(good);
        goods_.initialize({std::move(holder), std::move(region), std::move(good)}, qty);
    }
    void seed_region(std::string region, std::string sovereign) {
        ensure(stage_ == -1 && commit_id_ == 0, "region seed after simulation start");
        ensure(!region.empty() && !sovereign.empty() && !sovereignty_.contains(region), "bad region");
        register_typed(EntityKind::region,region);
        military_control_[region] = sovereign;
        sovereignty_[std::move(region)] = std::move(sovereign);
    }
    [[nodiscard]] TypedEntityId typed_id(EntityKind kind,const std::string& name) const {
        auto it=typed_entities_.find({kind,name});
        ensure(it!=typed_entities_.end(),"unknown typed entity");
        return it->second;
    }
    void require_typed(const TypedEntityId& ref,EntityKind kind) const {
        ensure(ref.id!=0 && ref.kind==kind && !retired_typed_ids_.contains(ref.id) &&
               typed_active_ids_.contains(ref.id) && typed_active_ids_.at(ref.id)==kind,
               "wrong-type or retired entity reference");
    }
    void retire_unused_region(const std::string& original,const std::string& successor) {
        require_stage({9});
        const auto old=typed_id(EntityKind::region,original);
        const auto next=typed_id(EntityKind::region,successor);
        require_typed(old,EntityKind::region);
        require_typed(next,EntityKind::region);
        ensure(old.id!=next.id && !regions_with_initial_stock_.contains(original),
               "cannot retire region with active legacy stock");
        for(const auto& [id,sl]:people_.slices()) {
            (void)id;
            ensure(people_.demographic(sl.key.demographic).region!=original,
                   "cannot retire inhabited region");
        }
        for(const auto& [id,shipment]:shipments_) {
            (void)id;
            ensure(shipment.origin!=original&&shipment.destination!=original,
                   "cannot retire region with shipments");
        }
        retired_typed_ids_.insert(old.id);
        typed_successor_[old.id]=next.id;
        events_.push_back("typed-region-lineage-tombstone");
    }
    [[nodiscard]] TypedEntityId successor_of(const TypedEntityId& old) const {
        ensure(typed_successor_.contains(old.id),"no successor lineage");
        return {old.kind,typed_successor_.at(old.id)};
    }
    void seed_formation(const std::string& id) {
        ensure(stage_==-1 && commit_id_==0 && !id.empty() &&
               formations_.insert(id).second, "invalid formation");
        register_typed(EntityKind::formation,id);
    }
    void attach_to_formation(const std::string& formation,const std::string& unit) {
        ensure((stage_==-1 && commit_id_==0) || stage_==9,
               "formation changes only before simulation or at policy stage");
        ensure(formations_.contains(formation) && units_.contains(unit) &&
               !unit_to_formation_.contains(unit), "invalid/duplicate formation assignment");
        unit_to_formation_[unit]=formation;
        events_.push_back("unit-formation-link");
    }
    [[nodiscard]] People formation_strength(const std::string& formation) const {
        ensure(formations_.contains(formation), "unknown formation");
        People total=0;
        for(const auto& [unit,parent]:unit_to_formation_)
            if(parent==formation) total=phaseb::add_checked(total,active_strength(unit));
        return total;
    }
    void assign_equipment(const std::string& unit,const std::string& region,
                          const std::string& good,Quantity n) {
        require_stage({7});
        ensure(units_.contains(unit) && n>0 &&
               equipment_assignment_[{unit,region,good}]+n<=stock(unit,region,good),
               "equipment not physically present in unit");
        equipment_assignment_[{unit,region,good}]+=n;
        events_.push_back("real-equipment-assignment");
    }
    [[nodiscard]] Quantity assigned_equipment(const std::string& unit,const std::string& region,
                                               const std::string& good) const {
        auto it=equipment_assignment_.find({unit,region,good});
        return it==equipment_assignment_.end()?0:it->second;
    }
    void seed_unit(std::string unit) {
        ensure(stage_ == -1 && commit_id_ == 0 && !unit.empty() && !units_.contains(unit), "bad unit");
        register_typed(EntityKind::unit,unit);
        units_.emplace(std::move(unit), std::map<Id, People>{});
    }
    void seed_active(std::string unit, Id slice) {
        ensure(stage_ == -1 && commit_id_ == 0, "seed active after start");
        ensure(units_.contains(unit), "unknown unit");
        const auto& s = people_.slice(slice);
        ensure(people_.demographic(s.key.demographic).service == phaseb::Service::active, "not active");
        ensure(assigned(slice) == 0, "duplicate service assignment");
        units_[unit][slice] = s.count;
    }
    void seed_shares(const std::string& asset, const std::string& holder, Money units) {
        ensure(stage_ == -1 && commit_id_ == 0 && units > 0 &&
               actors_.contains(holder) && !tracked_assets_.contains(asset) &&
               !asset.empty(), "invalid share issue");
        ownership_.issue(asset, holder, units);
        tracked_assets_.insert(asset);
        ensure_security_account(holder, asset);
        money_.post({unique("share-seed"), "opening securities ownership", {
            {security_account(holder, asset), units, 0},
            {account(holder, "capital"), 0, units}}});
    }
    [[nodiscard]] Money shares(const std::string& asset, const std::string& holder) const {
        return ownership_.units(asset, holder);
    }
    // V1 fixed book price = one minor monetary unit per issued share.
    // This is a validated settlement slice, not a full stock exchange.
    void sell_shares(const std::string& asset, const std::string& seller,
                     const std::string& buyer, Money units) {
        require_stage({7});
        ensure(tracked_assets_.contains(asset) && units > 0 &&
               actors_.contains(seller) && actors_.contains(buyer) &&
               buyer != seller && shares(asset, seller) >= units &&
               cash(buyer) >= units, "invalid security transfer");
        ensure_security_account(buyer, asset);
        ownership_.transfer(asset, seller, buyer, units);
        money_.post({unique("share-sale"), "share ownership and cash swap", {
            {security_account(buyer, asset), units, 0},
            {account(buyer, "cash"), 0, units},
            {account(seller, "cash"), units, 0},
            {security_account(seller, asset), 0, units}}});
        events_.push_back("security-transfer");
    }
    void declare_dividend(const std::string& asset, const std::string& payer,
                          const std::string& holder, Money amount) {
        require_stage({9});
        ensure(!asset.empty() && tracked_assets_.contains(asset) &&
               actors_.contains(payer) && actors_.contains(holder) &&
               payer != holder && amount > 0 && shares(asset, holder) > 0 &&
               !dividends_.contains(asset + ":" + holder),
               "invalid dividend declaration");
        dividends_[asset + ":" + holder] = {asset, payer, holder, amount, month() + 1};
        events_.push_back("dividend-declared");
    }
    void pay_dividend(const std::string& asset, const std::string& holder) {
        require_stage({5});
        const std::string id = asset + ":" + holder;
        ensure(dividends_.contains(id), "no dividend payable");
        const auto d = dividends_.at(id);
        ensure(d.due <= month() && cash(d.payer) >= d.amount,
               "dividend not yet due or unfunded");
        money_.post({unique("dividend"), "dividend settles shareholder payable", {
            {account(d.payer, "capital"), d.amount, 0},
            {account(d.payer, "cash"), 0, d.amount},
            {account(holder, "cash"), d.amount, 0},
            {account(holder, "revenue"), 0, d.amount}}});
        dividends_.erase(id);
        events_.push_back("dividend-paid");
    }
    void issue_bond(const std::string& bond_id, const std::string& government,
                    const std::string& buyer, Money face) {
        require_stage({1});
        ensure(!bond_id.empty() && face > 0 && actors_.contains(government) &&
               actors_.contains(buyer) && government != buyer && cash(buyer) >= face &&
               !tracked_assets_.contains("bond:" + bond_id), "unfunded bond issue");
        const auto asset = "bond:" + bond_id;
        ownership_.issue(asset, buyer, face);
        tracked_assets_.insert(asset);
        ensure_security_account(buyer, asset);
        money_.post({unique("bond"), "paid bond subscription", {
            {account(government, "cash"), face, 0},
            {account(government, "debt"), 0, face},
            {security_account(buyer, asset), face, 0},
            {account(buyer, "cash"), 0, face}}});
        events_.push_back("bond-issued-paid");
    }
    void seed_public_institution(const std::string& id,const std::string& operator_id,
                                 const std::string& region,const std::string& medicine,
                                 People monthly_slots,People patients_per_worker=3) {
        ensure(stage_==-1&&commit_id_==0&&!id.empty()&&
               actors_.contains(operator_id)&&!region.empty()&&!medicine.empty()&&
               monthly_slots>0&&patients_per_worker>0&&!public_institutions_.contains(id),
               "invalid public institution");
        public_institutions_[id]={id,operator_id,region,medicine,monthly_slots,patients_per_worker,0};
    }
    void enact_public_healthcare(bool enabled) {
        require_stage({9});
        ensure(governments_.empty(),"constitutional government requires authorized public law");
        scheduled_public_healthcare_[month()+1]=enabled;
        events_.push_back("public-right-next-month");
    }
    [[nodiscard]] bool has_public_healthcare_right() const {return healthcare_right_;}
    void provide_public_healthcare(const std::string& institution,Id patient_slice,People patients) {
        require_stage({7});
        ensure(healthcare_right_&&public_institutions_.contains(institution)&&
               people_.has_slice(patient_slice)&&patients>0,
               "no legal entitlement or patient population");
        auto& service=public_institutions_.at(institution);
        ensure(patients<=people_.slice(patient_slice).count &&
               service.provided+patients<=service.monthly_slots &&
               patients<=positive_product(employed_by(service.operator_id),
                                           service.patients_per_worker) &&
               paid_staff_this_month_[service.operator_id]>0 &&
               stock(service.operator_id,service.region,service.good)>=patients,
               "public service cannot exceed actual paid staff facility and supplies");
        goods_.consume({service.operator_id,service.region,service.good},patients);
        service.provided+=patients;
        events_.push_back("actual-public-medical-service");
    }
    [[nodiscard]] People public_services_rendered(const std::string& id) const {
        return public_institutions_.at(id).provided;
    }
    void seed_production_facility(const std::string& id,const std::string& owner,
                                  const std::string& region,Quantity capacity) {
        ensure(stage_==-1 && commit_id_==0 && !id.empty() && !facilities_.contains(id) &&
               actors_.contains(owner) && !region.empty() && capacity>0,
               "invalid facility seed");
        facilities_[id]={id,owner,region,capacity,0};
    }
    [[nodiscard]] const ProductionFacility& facility(const std::string& id) const {
        return facilities_.at(id);
    }
    void war_damage_facility(const std::string& id,Quantity destroyed) {
        require_stage({10});
        auto& site=facilities_.at(id);
        ensure(destroyed>0 && site.damaged+destroyed<=site.capacity,
               "damage exceeds physical facility");
        site.damaged+=destroyed;
        events_.push_back("physical-facility-war-damage");
    }
    void rebuild_facility(const std::string& id,const std::string& building_good,
                          Quantity restored,People construction_workers) {
        require_stage({4});
        const auto& site=facilities_.at(id);
        ensure(restored>0 && construction_workers>0 && restored<=site.damaged &&
               stock(site.owner,site.region,building_good)>=restored &&
               employed_by(site.owner)>=construction_workers &&
               scheduled_site_repairs_[month()+1][id]+restored<=site.damaged,
               "rebuild requires actual material labor and destroyed capacity");
        goods_.consume({site.owner,site.region,building_good},restored);
        scheduled_site_repairs_[month()+1][id]+=restored;
        events_.push_back("rebuild-work-physical-input");
    }
    void sign_reparation_treaty(const std::string& id,const std::string& payer,
                               const std::string& recipient,Money amount) {
        require_stage({9});
        ensure(!id.empty() && !reparations_.contains(id) && amount>0 &&
               actors_.contains(payer)&&actors_.contains(recipient)&&payer!=recipient,
               "invalid treaty reparations");
        reparations_[id]={id,payer,recipient,amount,amount,month()+1,false};
        events_.push_back("treaty-reparations-effective-next-month");
    }
    void pay_reparation(const std::string& id,Money amount) {
        require_stage({5});
        ensure(reparations_.contains(id),"unknown reparations");
        auto& t=reparations_.at(id);
        ensure(t.recognized&&amount>0&&amount<=t.remaining &&
               cash(t.payer)>=amount,"unfunded/unrecognized reparation");
        money_.post({unique("reparation-paid"),"debt obligation settled by actual cash",{
            {account(t.payer,"reparations_payable"),amount,0},
            {account(t.payer,"cash"),0,amount},
            {account(t.recipient,"cash"),amount,0},
            {account(t.recipient,"reparations_receivable"),0,amount}}});
        t.remaining-=amount;
        events_.push_back("reparation-settled");
    }
    [[nodiscard]] Money reparations_outstanding(const std::string& id) const {
        return reparations_.at(id).remaining;
    }
    Id migrate_with_explicit_property_allocation(
        Id source,People count,const std::string& destination_region,Id destination_household,
        const std::string& old_owner,const std::string& new_owner,
        Money moving_cash,const std::string& asset,Money moving_shares) {
        require_stage({10});
        ensure(people_.has_slice(source) && count>0 &&
               count<=people_.slice(source).count && actors_.contains(old_owner) &&
               actors_.contains(new_owner)&&old_owner!=new_owner &&
               moving_cash>=0&&cash(old_owner)>=moving_cash &&
               moving_shares>=0 &&
               (moving_shares==0||(tracked_assets_.contains(asset) &&
                shares(asset,old_owner)>=moving_shares)),
               "invalid explicit migration and ownership allocation");
        const Id arrived=people_.migrate(source,count,destination_region,destination_household,0);
        if(moving_cash) {
            money_.post({unique("migration-cash"),"explicit household capital reallocation",{
                {account(old_owner,"capital"),moving_cash,0},
                {account(old_owner,"cash"),0,moving_cash},
                {account(new_owner,"cash"),moving_cash,0},
                {account(new_owner,"capital"),0,moving_cash}}});
        }
        if(moving_shares) {
            ensure_security_account(new_owner,asset);
            ownership_.transfer(asset,old_owner,new_owner,moving_shares);
            money_.post({unique("migration-share"),"explicit migration securities allocation",{
                {account(old_owner,"capital"),moving_shares,0},
                {security_account(old_owner,asset),0,moving_shares},
                {security_account(new_owner,asset),moving_shares,0},
                {account(new_owner,"capital"),0,moving_shares}}});
        }
        events_.push_back("population-migration-with-explicit-asset-rights");
        return arrived;
    }
    void demobilize(const std::string& unit,Id source,People n) {
        require_stage({10});
        ensure(units_.contains(unit)&&units_.at(unit).contains(source)&&
               n>0&&n<=units_.at(unit).at(source),
               "invalid demobilization");
        ensure(people_.has_slice(source),"missing active slice");
        const auto& row=people_.slice(source);
        ensure(people_.demographic(row.key.demographic).service==phaseb::Service::active,
               "nonactive demobilization");
        people_.set_service(source,n,phaseb::Service::reserve,0);
        auto& assigned=units_[unit].at(source);
        assigned-=n;
        if(assigned==0)units_[unit].erase(source);
        events_.push_back("demobilization");
    }
    // Multi-currency bilateral settlement. Each currency has an independent
    // double-entry ledger; no direct sum or implicit 1:1 conversion exists.
    void seed_forex_balance(const std::string& owner,const std::string& currency,Money amount) {
        ensure(stage_==-1&&commit_id_==0 && actors_.contains(owner) &&
               !currency.empty()&&currency!="TEST"&&amount>=0,
               "invalid foreign currency opening");
        ensure_fx_ledger(currency);
        ensure_fx_account(owner,currency);
        if(amount) {
            auto& l=foreign_ledgers_.at(currency);
            l.post({unique("fx-seed"),"foreign currency opening",{
                {fx_acc(owner,currency,"cash"),amount,0},
                {fx_acc(owner,currency,"capital"),0,amount}}});
            l.post({unique("fx-bank-seed"),"matching bank currency deposit",{
                {fx_bank(currency,"reserves"),amount,0},
                {fx_bank(currency,"deposit"),0,amount}}});
        }
    }
    [[nodiscard]] Money fx_balance(const std::string& owner,const std::string& currency) const {
        ensure(foreign_holders_.contains({owner,currency}),"no currency wallet");
        return foreign_ledgers_.at(currency).balance(fx_acc(owner,currency,"cash"));
    }
    void exchange_fx(const std::string& a,const std::string& b,
                     const std::string& sold_currency,Money sold_amount,
                     const std::string& bought_currency,Money bought_amount) {
        require_stage({7});
        ensure(a!=b&&sold_currency!=bought_currency &&
               sold_amount>0&&bought_amount>0 &&
               foreign_holders_.contains({a,sold_currency}) &&
               foreign_holders_.contains({b,bought_currency}) &&
               fx_balance(a,sold_currency)>=sold_amount &&
               fx_balance(b,bought_currency)>=bought_amount,
               "invalid currency exchange or insufficient actual counterparty liquidity");
        ensure_fx_account(a,bought_currency);
        ensure_fx_account(b,sold_currency);
        auto& l1=foreign_ledgers_.at(sold_currency);
        l1.post({unique("fx-sale"),"bilateral settled currency leg",{
            {fx_acc(a,sold_currency,"capital"),sold_amount,0},
            {fx_acc(a,sold_currency,"cash"),0,sold_amount},
            {fx_acc(b,sold_currency,"cash"),sold_amount,0},
            {fx_acc(b,sold_currency,"capital"),0,sold_amount}}});
        auto& l2=foreign_ledgers_.at(bought_currency);
        l2.post({unique("fx-buy"),"matched currency leg",{
            {fx_acc(b,bought_currency,"capital"),bought_amount,0},
            {fx_acc(b,bought_currency,"cash"),0,bought_amount},
            {fx_acc(a,bought_currency,"cash"),bought_amount,0},
            {fx_acc(a,bought_currency,"capital"),0,bought_amount}}});
        events_.push_back("bilateral-foreign-exchange-settled");
    }
    void seed_market(const std::string& region,const std::string& good,
                     Money price, Money floor, Money ceiling, Quantity target_stock=0) {
        ensure(stage_==-1 && commit_id_==0 && !region.empty() && !good.empty() &&
               price>0 && floor>0 && ceiling>=price && floor<=price &&
               target_stock>=0 && !markets_.contains({region,good}),
               "invalid market seed");
        markets_[{region,good}]={price,floor,ceiling,target_stock,0,0,0,0};
    }
    [[nodiscard]] const MarketQuote& market(const std::string& region,const std::string& good) const {
        return markets_.at({region,good});
    }
    Quantity clear_consumer_market(const std::string& seller,const std::string& buyer,
                                  const std::string& region,const std::string& good,
                                  Quantity requested) {
        require_stage({7});
        ensure(requested>=0 && markets_.contains({region,good}) &&
               actors_.contains(buyer) && actors_.contains(seller) && buyer!=seller,
               "invalid market order");
        auto& m=markets_.at({region,good});
        const Quantity solvent=std::min(requested,cash(buyer)/m.price);
        const Quantity available=stock(seller,region,good);
        const Quantity fulfilled=std::min(solvent,available);
        m.solvent_demand=phaseb::add_checked(m.solvent_demand,solvent);
        m.stock_offered=phaseb::add_checked(m.stock_offered,available);
        m.delivered=phaseb::add_checked(m.delivered,fulfilled);
        m.unmet=phaseb::add_checked(m.unmet,solvent-fulfilled);
        if(fulfilled>0) trade(seller,buyer,region,good,fulfilled,m.price);
        events_.push_back("solvent-demand-market-clearance");
        return fulfilled;
    }
    void household_produce(Id slice,const std::string& good,Quantity output,
                           Quantity max_output_per_person) {
        require_stage({4});
        ensure(people_.has_slice(slice) && !good.empty() && output>0 &&
               max_output_per_person>0, "invalid household production");
        const auto& row=people_.slice(slice);
        const auto& econ=people_.economic(row.key.economic);
        const auto& demo=people_.demographic(row.key.demographic);
        ensure(econ.livelihood=="subsistence"||econ.livelihood=="self_employed",
               "not own-account labor");
        const Quantity limit=positive_product(row.count,max_output_per_person);
        ensure(household_output_[slice]+output<=limit,"subsistence labor overproduced");
        household_output_[slice]+=output;
        tracked_goods_.insert(good);
        goods_.produce({"HH:"+std::to_string(row.key.household),demo.region,good},output);
        events_.push_back("household-actual-in-kind-output");
    }
    void household_consume(Id slice,const std::string& good,Quantity qty) {
        require_stage({7});
        ensure(people_.has_slice(slice)&&qty>0,"invalid household consumption");
        const auto& row=people_.slice(slice);
        const auto& demo=people_.demographic(row.key.demographic);
        goods_.consume({"HH:"+std::to_string(row.key.household),demo.region,good},qty);
        household_consumed_[{row.key.household,good}] =
            phaseb::add_checked(household_consumed_[{row.key.household,good}],qty);
        events_.push_back("household-actual-in-kind-consumption");
    }
    [[nodiscard]] Quantity actual_household_consumption(Id household,const std::string& good) const {
        auto it=household_consumed_.find({household,good});
        return it==household_consumed_.end()?0:it->second;
    }
    // A named voter occupies one real population place. The anchor never
    // creates headcount; duplicate anchors are capped by their source slice.
    void seed_person_anchor(const std::string& anchor, Id slice) {
        ensure(stage_==-1 && commit_id_==0 && !anchor.empty() &&
               !person_anchors_.contains(anchor) && people_.has_slice(slice) &&
               anchor_counts_[slice] < people_.slice(slice).count,
               "invalid or unbacked voter identity");
        person_anchors_[anchor]=slice;
        ++anchor_counts_[slice];
    }
    void cast_named_ballot(const std::string& election_id, const std::string& district,
                           const std::string& anchor, const std::string& party) {
        require_stage({9});
        ensure(!election_id.empty() && !district.empty() && !party.empty() &&
               person_anchors_.contains(anchor), "unknown election identity");
        const Id slice=person_anchors_.at(anchor);
        ensure(people_.has_slice(slice), "voter no longer exists");
        const auto& dem=people_.demographic(people_.slice(slice).key.demographic);
        ensure(dem.citizenship=="citizen" && dem.service!=phaseb::Service::active &&
               dem.service!=phaseb::Service::pow && dem.service!=phaseb::Service::missing &&
               people_.age_months(people_.slice(slice).key.demographic)>=
                   static_cast<std::int64_t>(voting_age_years_)*12,
               "ineligible voter");
        ensure(ballot_ids_.insert({election_id,anchor}).second, "duplicate ballot");
        ballot_votes_[election_id][party] =
            phaseb::add_checked(ballot_votes_[election_id][party],1);
        events_.push_back("named-ballot:"+election_id+":"+district);
    }
    [[nodiscard]] People named_votes(const std::string& election_id) const {
        auto it=ballot_votes_.find(election_id);
        if(it==ballot_votes_.end())return 0;
        People n=0;
        for(const auto& [party, count]:it->second){(void)party;n=phaseb::add_checked(n,count);}
        return n;
    }
    void seed_interest_group(std::string group) {
        ensure(stage_==-1 && commit_id_==0 && !group.empty() &&
               interest_groups_.insert(std::move(group)).second, "invalid interest group");
    }
    void join_interest_group(Id slice, const std::string& group, People n) {
        require_stage({9});
        ensure(people_.has_slice(slice) && interest_groups_.contains(group) &&
               n>0 && n<=people_.slice(slice).count, "invalid group membership");
        membership_[{slice,group}]=n;
        events_.push_back("interest-group-membership");
    }
    [[nodiscard]] People group_members(Id slice,const std::string& group) const {
        auto it=membership_.find({slice,group});
        return it==membership_.end()?0:it->second;
    }
    void protest(Id slice, const std::string& group, People participants,
                 People strikers=0) {
        require_stage({9});
        ensure(people_.has_slice(slice) && participants>0 && strikers>=0 &&
               participants<=group_members(slice,group) &&
               participation_this_month_[slice]+participants<=people_.slice(slice).count &&
               strikers<=participants && strikers<=people_.employed(slice) &&
               scheduled_strikes_[month()+1][slice]+strikers<=people_.employed(slice),
               "political action exceeds actual participants or workers");
        participation_this_month_[slice]+=participants;
        scheduled_strikes_[month()+1][slice]+=strikers;
        events_.push_back("verified-protest");
    }
    [[nodiscard]] People protest_participants(Id slice) const {
        auto it=participation_this_month_.find(slice);
        return it==participation_this_month_.end()?0:it->second;
    }
    [[nodiscard]] People currently_striking(Id slice) const {
        auto it=strike_active_.find(slice);
        return it==strike_active_.end()?0:it->second;
    }
    void seed_government(std::string country, std::string head,
                         std::string prime, People chamber_seats, People quorum) {
        ensure(stage_ == -1 && commit_id_ == 0 && !country.empty() &&
               !head.empty() && !prime.empty() && chamber_seats > 0 &&
               quorum > 0 && quorum <= chamber_seats &&
               !governments_.contains(country), "invalid government seed");
        bool sovereign_exists = false;
        for (const auto& [region, sovereign] : sovereignty_) {
            (void)region;
            if (sovereign == country) sovereign_exists = true;
        }
        ensure(sovereign_exists, "country has no region");
        governments_.emplace(country, GovernmentState{country, head, prime, chamber_seats, quorum, 1});
    }
    [[nodiscard]] const GovernmentState& government(const std::string& country) const {
        return governments_.at(country);
    }
    void propose_tax_law(const std::string& country, const std::string& proposer,
                         const std::vector<std::string>& supporting_parties, int ppm) {
        require_stage({9});
        ensure(ppm >= 0 && ppm <= 1'000'000, "invalid tax rate");
        const auto& g = governments_.at(country);
        ensure(g.prime_minister == proposer && last_election_.total_seats == g.chamber_seats,
               "unauthorized tax bill or missing legislature");
        ensure(coalition_seats(supporting_parties) >= g.quorum, "insufficient legislative votes");
        scheduled_income_tax_[month()+1] = ppm;
        events_.push_back("constitutional-tax-law");
    }
    void appoint_cabinet(const std::string& country, const std::string& head,
                        std::string minister, const std::vector<std::string>& supporting_parties) {
        require_stage({9});
        const auto& g=governments_.at(country);
        ensure(g.head_of_state == head && !minister.empty() &&
               last_election_.total_seats == g.chamber_seats &&
               coalition_seats(supporting_parties) >= g.quorum,
               "cabinet lacks appointment authority or majority");
        scheduled_cabinets_[month()+1].push_back({country, "", std::move(minister), false, true});
        events_.push_back("cabinet-approved");
    }
    void change_head_of_state(const std::string& country,
                              const std::string& current_head,
                              std::string successor) {
        require_stage({9});
        const auto& g=governments_.at(country);
        ensure(g.head_of_state == current_head && !successor.empty() &&
               current_head != successor, "invalid head-of-state succession");
        scheduled_cabinets_[month()+1].push_back({country, std::move(successor), "", true, false});
        events_.push_back("head-of-state-succession");
    }
    void set_voting_age(int age) {
        ensure(stage_ == -1 && commit_id_ == 0 && age >= 0 && age <= 120, "bad opening voting age");
        voting_age_years_ = age;
    }
    void enable_demography(phaseb::DemographyRules rules) {
        ensure(stage_ == -1 && commit_id_ == 0, "demography mode must be chosen before start");
        demography_enabled_ = true;
        demographic_rules_ = std::move(rules);
    }
    [[nodiscard]] Money cash(const std::string& who) const {
        ensure(actors_.contains(who), "missing wallet");
        return money_.balance(account(who, "cash"));
    }
    [[nodiscard]] Quantity stock(const std::string& holder, const std::string& region,
                                 const std::string& good) const {
        return goods_.stock({holder, region, good});
    }
    [[nodiscard]] People active_strength(const std::string& unit) const {
        const auto& a = units_.at(unit);
        People total = 0;
        for (const auto& [id, n] : a) { (void)id; total = phaseb::add_checked(total, n); }
        return total;
    }
    [[nodiscard]] People assigned(Id slice) const {
        People total = 0;
        for (const auto& [unit, assignments] : units_) {
            (void)unit;
            auto it = assignments.find(slice);
            if (it != assignments.end()) total = phaseb::add_checked(total, it->second);
        }
        return total;
    }
    [[nodiscard]] People eligible_voters() const {
        People total = 0;
        for (const auto& [id, s] : people_.slices()) {
            (void)id;
            const auto& d = people_.demographic(s.key.demographic);
            if (d.citizenship == "citizen" &&
                d.service != phaseb::Service::active &&
                d.service != phaseb::Service::pow &&
                d.service != phaseb::Service::missing &&
                people_.age_months(s.key.demographic) >= static_cast<std::int64_t>(voting_age_years_) * 12)
                total = phaseb::add_checked(total, s.count);
        }
        return total;
    }

    // Execute an externally identified command at most once across committed
    // months. A failed staged month also rolls back its dedupe set.
    void command_once(const std::string& external_id, const Task& command) {
        ensure(stage_ >= 0 && !external_id.empty() &&
               !processed_commands_.contains(external_id),
               "duplicate or invalid command");
        command(*this);
        processed_commands_.insert(external_id);
        events_.push_back("command:" + external_id);
    }
    void pay(const std::string& from, const std::string& to, Money amount) {
        require_stage({3, 5, 7, 8});
        ensure(amount > 0 && cash(from) >= amount && actors_.contains(to) && from != to,
               "unfunded payment");
        money_.post({unique("pay"), "confirmed transfer", {
            {account(from, "expense"), amount, 0}, {account(from, "cash"), 0, amount},
            {account(to, "cash"), amount, 0}, {account(to, "revenue"), 0, amount}}});
        events_.push_back("payment");
    }
    void wages(const std::string& payer, const std::string& worker,
               const std::string& treasury, Money gross, Money withheld) {
        require_stage({5});
        ensure(gross > 0 && withheld >= 0 && withheld <= gross && cash(payer) >= gross,
               "unfunded gross wage");
        ensure(actors_.contains(worker) && actors_.contains(treasury) &&
               worker != treasury && payer != worker && payer != treasury, "bad payroll parties");
        const Money net = gross - withheld;
        std::vector<Posting> p{{account(payer, "expense"), gross, 0},
                               {account(payer, "cash"), 0, gross}};
        if (net) {
            p.push_back({account(worker, "cash"), net, 0});
            p.push_back({account(worker, "revenue"), 0, net});
        }
        if (withheld) {
            p.push_back({account(treasury, "cash"), withheld, 0});
            p.push_back({account(treasury, "revenue"), 0, withheld});
        }
        money_.post({unique("payroll"), "gross payroll with withholding", p});
        paid_staff_this_month_[payer]=phaseb::add_checked(paid_staff_this_month_[payer],gross);
        events_.push_back("wages");
    }
    void lend(const std::string& borrower, Money amount) {
        require_stage({1});
        ensure(amount > 0 && actors_.contains(borrower) && !bank_insolvent_,
               "bank insolvent or borrower invalid");
        money_.post({unique("loan"), "bank loan creates matching deposit", {
            {"BANK:loan", amount, 0}, {"BANK:deposit", 0, amount},
            {account(borrower, "cash"), amount, 0}, {account(borrower, "debt"), 0, amount}}});
        events_.push_back("loan");
    }
    void forgive_defaulted_loan(const std::string& borrower, Money amount) {
        require_stage({8});
        ensure(actors_.contains(borrower)&&amount>0 &&
               -money_.balance(account(borrower,"debt"))>=amount &&
               money_.balance("BANK:loan")>=amount,
               "invalid defaulted loan debt forgiveness");
        money_.post({unique("loan-loss"),"bank recognizes loss, borrower released from debt",{
            {"BANK:loan_loss",amount,0},{"BANK:loan",0,amount},
            {account(borrower,"debt"),amount,0},
            {account(borrower,"revenue"),0,amount}}});
        total_bad_debt_=phaseb::add_checked(total_bad_debt_,amount);
        const Money loans=money_.balance("BANK:loan");
        const Money deposits=-money_.balance("BANK:deposit");
        const Money reserves=money_.balance("BANK:reserves");
        bank_insolvent_ = (loans+reserves < deposits);
        events_.push_back("defaulted-bank-asset-recognition");
    }
    [[nodiscard]] Money bank_losses() const { return total_bad_debt_; }
    [[nodiscard]] bool bank_insolvent() const { return bank_insolvent_; }
    [[nodiscard]] People food_grievance(const std::string& region) const {
        auto it=regional_food_grievance_.find(region);
        return it==regional_food_grievance_.end()?0:it->second;
    }
    void organize_from_food_grievance(Id slice,const std::string& group,People n) {
        require_stage({9});
        ensure(people_.has_slice(slice)&&n>0&&interest_groups_.contains(group),
               "invalid political issue association");
        const auto& d=people_.demographic(people_.slice(slice).key.demographic);
        ensure(n<=food_grievance(d.region)&&
               group_members(slice,group)+n<=people_.slice(slice).count,
               "political organization exceeds actual grievance or persons");
        membership_[{slice,group}]+=n;
        events_.push_back("material-shortage-political-organization");
    }
    void repay(const std::string& borrower, Money amount) {
        require_stage({8});
        ensure(amount > 0 && actors_.contains(borrower) && cash(borrower) >= amount &&
               -money_.balance(account(borrower, "debt")) >= amount, "invalid loan repayment");
        money_.post({unique("repay"), "loan principal retirement", {
            {account(borrower, "debt"), amount, 0}, {account(borrower, "cash"), 0, amount},
            {"BANK:deposit", amount, 0}, {"BANK:loan", 0, amount}}});
        events_.push_back("repay");
    }
    void hire(Id slice, People count, const std::string& employer, Money wage) {
        require_stage({2});
        ensure(actors_.contains(employer), "unknown employer");
        people_.hire(slice, count, employer, "worker", wage, phaseb::JobRequirements{});
        events_.push_back("hire");
    }
    void produce(const std::string& firm, const std::string& region,
                 const std::string& input, const std::string& output,
                 Quantity qty, People workers_required) {
        require_stage({4});
        ensure(qty > 0 && workers_required > 0 && workers_required <= employed_by(firm),
               "insufficient production labor");
        ensure(stock(firm, region, input) >= qty, "missing production input");
        goods_.consume({firm, region, input}, qty);
        goods_.produce({firm, region, output}, qty);
        tracked_goods_.insert(output);
        events_.push_back("production");
    }
    void trade(const std::string& seller, const std::string& buyer,
               const std::string& region, const std::string& good,
               Quantity qty, Money unit_price) {
        require_stage({3, 7});
        ensure(qty > 0 && unit_price > 0 && stock(seller, region, good) >= qty,
               "insufficient sale stock");
        const Money cost = positive_product(qty, unit_price);
        ensure(cash(buyer) >= cost, "buyer cannot pay");
        goods_.transfer({seller, region, good}, {buyer, region, good}, qty);
        pay(buyer, seller, cost);
        events_.push_back("trade");
    }
    void consume(const std::string& owner, const std::string& region,
                 const std::string& good, Quantity qty) {
        require_stage({7, 10});
        goods_.consume({owner, region, good}, qty);
        events_.push_back("consumption");
    }
    void ship(std::string id, std::string seller, std::string buyer,
              std::string origin, std::string destination, std::string good,
              Quantity quantity, std::int64_t travel_months, Quantity loss = 0) {
        require_stage({7});
        ensure(!id.empty() && !shipments_.contains(id) && quantity > 0 &&
               travel_months > 0 && loss >= 0 && loss <= quantity, "bad shipment");
        ensure(stock(seller, origin, good) >= quantity, "unfunded shipment stock");
        const auto key = InventoryKey{"TRANSIT:" + id, "in_transit", good};
        goods_.transfer({seller, origin, good}, key, quantity);
        shipments_.emplace(id, Shipment{id, std::move(seller), std::move(buyer),
                    std::move(origin), std::move(destination), std::move(good),
                    quantity, loss, month() + travel_months, false});
        events_.push_back("dispatch");
    }
    [[nodiscard]] bool conscription_enabled() const { return conscription_enabled_; }
    [[nodiscard]] int income_tax_ppm() const { return income_tax_ppm_; }
    void enact_conscription(bool enabled) {
        require_stage({9});
        ensure(governments_.empty(), "constitutional government requires authorized policy procedure");
        scheduled_conscription_[month() + 1] = enabled;
        events_.push_back("conscription-law-scheduled");
    }
    void enact_income_tax(int ppm) {
        require_stage({9});
        ensure(governments_.empty(), "constitutional government requires authorized policy procedure");
        ensure(ppm >= 0 && ppm <= 1'000'000, "invalid income tax ppm");
        scheduled_income_tax_[month() + 1] = ppm;
        events_.push_back("income-tax-law-scheduled");
    }
    void wages_under_law(const std::string& payer, const std::string& worker,
                         const std::string& treasury, Money gross) {
        require_stage({5});
        ensure(gross > 0, "invalid gross wage");
        const auto withholding = positive_product(gross, income_tax_ppm_) / 1'000'000;
        wages(payer, worker, treasury, gross, withholding);
    }
    void enact_voting_age(int new_age) {
        require_stage({9});
        ensure(governments_.empty(), "constitutional government requires authorized policy procedure");
        ensure(new_age >= 0 && new_age <= 120, "invalid voting age");
        scheduled_voting_age_[month() + 1] = new_age;
        events_.push_back("law-adopted-next-month");
    }
    void hold_election(People registered, People invalid, People seats,
                       const std::map<std::string, People>& votes) {
        require_stage({9});
        ensure(!election_this_month_, "duplicate election this month");
        const People eligible = eligible_voters();
        ensure(registered >= 0 && registered <= eligible && invalid >= 0 &&
               seats > 0 && !votes.empty(), "invalid election");
        People total = 0;
        for (const auto& [name, n] : votes) {
            ensure(!name.empty() && n >= 0, "invalid party vote");
            total = phaseb::add_checked(total, n);
        }
        ensure(total > 0 && total <= registered - invalid, "ballots exceed registration");
        ElectionResult result;
        result.eligible = eligible;
        result.registered = registered;
        result.valid = total;
        result.invalid = invalid;
        result.cast = phaseb::add_checked(total, invalid);
        result.votes = votes;
        result.total_seats = seats;
        for (const auto& [name, n] : votes) { (void)n; result.seats[name] = 0; }
        // Exact integer quotients; no platform-specific float tie break.
        for (People i = 0; i < seats; ++i) {
            std::string chosen;
            for (const auto& [name, n] : votes) {
                if (chosen.empty()) { chosen = name; continue; }
                const int order = compare_nonnegative_ratios(
                    static_cast<std::uint64_t>(n),
                    static_cast<std::uint64_t>(result.seats[name] + 1),
                    static_cast<std::uint64_t>(votes.at(chosen)),
                    static_cast<std::uint64_t>(result.seats[chosen] + 1));
                if (order > 0 || (order == 0 && name < chosen)) chosen = name;
            }
            ++result.seats[chosen];
        }
        last_election_ = std::move(result);
        election_this_month_ = true;
        events_.push_back("election");
    }
    void mobilize(Id source, People n, const std::string& unit) {
        require_stage({10});
        ensure(units_.contains(unit) && n > 0 && people_.has_slice(source),
               "invalid mobilization");
        const auto s = people_.slice(source);
        const auto service = people_.demographic(s.key.demographic).service;
        ensure(conscription_enabled_, "conscription not currently authorized");
        ensure(service == phaseb::Service::civilian || service == phaseb::Service::reserve,
               "not mobilizable");
        ensure(n <= s.count - assigned(source), "overallocated recruit");
        const People hired = people_.employed(source);
        const People to_release = std::min(hired, n);
        if (to_release) people_.release(source, to_release);
        const Id active = people_.set_service(source, n, phaseb::Service::active, 0);
        ensure(assigned(active) + n <= people_.slice(active).count, "duplicate military assignment");
        units_[unit][active] = phaseb::add_checked(units_[unit][active], n);
        events_.push_back("mobilization");
    }
    void battle(const std::string& unit, Id source, const std::string& region,
                CombatLoss loss) {
        require_stage({10});
        ensure(units_.contains(unit) && people_.has_slice(source) &&
               units_.at(unit).contains(source) &&
               units_.at(unit).at(source) == people_.slice(source).count,
               "battle requires exclusively assigned active slice");
        const auto count = people_.slice(source).count;
        const People casualties = phaseb::add_checked(
            phaseb::add_checked(loss.killed, loss.acute),
            phaseb::add_checked(loss.long_term, phaseb::add_checked(loss.pow, loss.missing)));
        ensure(casualties >= 0 && casualties <= count && loss.ammunition >= 0 &&
               stock(unit, region, "ammunition") >= loss.ammunition,
               "combat exceeds people or ammunition");
        if (loss.ammunition) goods_.consume({unit, region, "ammunition"}, loss.ammunition);
        const auto before = people_.events().size();
        people_.battle_losses(source, loss.killed, loss.acute, loss.long_term,
                              loss.pow, loss.missing);
        units_[unit].erase(source);
        if (people_.has_slice(source)) units_[unit][source] = people_.slice(source).count;
        const auto& log = people_.events();
        for (std::size_t i = before; i < log.size(); ++i) {
            const auto& e = log[i];
            if (e.from != source || e.to == 0 || e.to == source) continue;
            const auto service = people_.demographic(people_.slice(e.to).key.demographic).service;
            if (service == phaseb::Service::active)
                units_[unit][e.to] = phaseb::add_checked(units_[unit][e.to], e.count);
        }
        events_.push_back("battle");
    }
    void occupy(const std::string& region, const std::string& occupier) {
        require_stage({10});
        ensure(sovereignty_.contains(region) && !occupier.empty(), "invalid occupation");
        require_typed(typed_id(EntityKind::region,region),EntityKind::region);
        military_control_[region] = occupier;
        events_.push_back("military-control");
    }

    AdvanceReport advance(const Plan& plan = {}) {
        IntegratedWorld stage = *this;
        const auto start = month();
        stage.election_this_month_ = false;
        for (int n = 0; n != 12; ++n) {
            stage.stage_ = n;
            if (n == 0) stage.open_month();
            for (const auto& callback : plan[static_cast<std::size_t>(n)]) callback(stage);
            if (n == 9) stage.collect_market_grievances();
            if (n == 10) {
                // Strikes reserve actual working time only for this month's
                // production; they must not outlive a later death or split.
                stage.strike_active_.clear();
                const std::size_t before = stage.people_.events().size();
                if (stage.demography_enabled_) {
                    phaseb::Engine engine{stage.demographic_rules_};
                    (void)engine.advance(stage.people_);
                    stage.sync_service_transitions(before);
                } else {
                    stage.people_.finish_month();
                    // End-of-month aging must invalidate contracts before
                    // the stage-11 commit even when endogenous demography is off.
                    stage.people_.release_ineligible();
                }
            }
            if (n == 11) stage.reprice_markets();
            stage.validate();
        }
        stage.stage_ = -1;
        ++stage.commit_id_;
        *this = std::move(stage);
        return {start, month(), people_.population(), commit_id_};
    }
    void validate() const {
        ensure(people_.validate(), "invalid population registry");
        for(const auto& [key,id]:typed_entities_) {
            ensure(id.kind==key.first && id.id!=0 &&
                   typed_active_ids_.contains(id.id) &&
                   typed_active_ids_.at(id.id)==id.kind,"typed entity identity mismatch");
        }
        for(const auto& [old,newid]:typed_successor_)
            ensure(retired_typed_ids_.contains(old)&&typed_active_ids_.contains(newid) &&
                   !retired_typed_ids_.contains(newid),"invalid lineage/retirement");

        for (const auto& [anchor, slice]:person_anchors_) {
            (void)anchor;
            ensure(people_.has_slice(slice) && anchor_counts_.at(slice)<=people_.slice(slice).count,
                   "orphaned or duplicate voter identity");
        }
        for (const auto& [key, n]:membership_) {
            ensure(interest_groups_.contains(key.second) && people_.has_slice(key.first) &&
                   n>=0 && n<=people_.slice(key.first).count,"unbacked interest-group membership");
        }
        for (const auto& [slice,n]:strike_active_)
            ensure(people_.has_slice(slice)&&n<=people_.employed(slice),"invalid strike allocation");
        for(const auto& [id,t]:reparations_) {
            (void)id;
            ensure(t.total>0&&t.remaining>=0&&t.remaining<=t.total,
                   "invalid reparation amount");
        }
        for(const auto& [id,svc]:public_institutions_) {
            ensure(id==svc.id && svc.provided>=0 &&
                   svc.provided<=svc.monthly_slots &&
                   svc.monthly_slots>0,"invalid public services accounting");
        }
        for(const auto& [id,site]:facilities_) {
            ensure(id==site.id&&site.capacity>0&&site.damaged>=0&&
                   site.damaged<=site.capacity,"invalid physical facility");
        }
        for (const auto& [id,g] : governments_) {
            ensure(id==g.country && !g.head_of_state.empty() && !g.prime_minister.empty() &&
                   g.chamber_seats>0 && g.quorum>0 && g.quorum<=g.chamber_seats,
                   "invalid government institution");
        }
        ensure(money_.balanced(), "unbalanced financial ledger");
        for(const auto& [currency,l]:foreign_ledgers_){
            ensure(l.balanced(),"unbalanced foreign currency ledger");
            Money total=0;
            for(const auto& [who,c]:foreign_holders_)
                if(c==currency){
                    const Money amount=fx_balance(who,c);
                    ensure(amount>=0,"negative currency position");
                    total=phaseb::add_checked(total,amount);
                }
            ensure(-l.balance(fx_bank(currency,"deposit"))==total,
                   "foreign bank deposit does not match actual currency claims");
        }

        Money deposits = 0;
        for (const auto& actor : actors_) {
            ensure(cash(actor) >= 0, "negative spendable cash");
            deposits = phaseb::add_checked(deposits, cash(actor));
        }
        ensure(-money_.balance("BANK:deposit") == deposits,
               "bank deposit liabilities do not match customer assets");
        for (const auto& g : tracked_goods_)
            ensure(goods_.conserved(g), "inconsistent commodity conservation");
        for(const auto& [unit,formation]:unit_to_formation_)
            ensure(units_.contains(unit)&&formations_.contains(formation),"orphaned formation membership");
        for(const auto& [key,n]:equipment_assignment_) {
            const auto& [unit,region,good]=key;
            ensure(units_.contains(unit)&&n>=0&&n<=stock(unit,region,good),
                   "equipment assignment exceeds real inventory");
        }

        for (const auto& a : tracked_assets_)
            ensure(ownership_.conserved(a), "inconsistent asset ownership");
        for (const auto& [owner, asset] : security_accounts_)
            ensure(money_.balance(security_account(owner, asset)) ==
                   ownership_.units(asset, owner), "financial security and title disagree");
        std::map<Id, People> assigned_by_slice;
        for (const auto& [unit, people] : units_) {
            (void)unit;
            for (const auto& [slice, n] : people) {
                ensure(n > 0 && people_.has_slice(slice), "orphaned service assignment");
                const auto& row = people_.slice(slice);
                ensure(people_.demographic(row.key.demographic).service == phaseb::Service::active,
                       "non-active soldier assignment");
                assigned_by_slice[slice] = phaseb::add_checked(assigned_by_slice[slice], n);
            }
        }
        for (const auto& [id, n] : assigned_by_slice)
            ensure(n <= people_.slice(id).count, "military headcount duplicated");
        for (const auto& [id, s] : people_.slices()) {
            const auto& d = people_.demographic(s.key.demographic);
            if (d.service == phaseb::Service::active)
                ensure(assigned_by_slice[id] == s.count, "active personnel without assignment");
        }
    }

private:
    phaseb::World people_;
    FinancialLedger money_;
    InventoryLedger goods_;
    OwnershipLedger ownership_;
    std::set<std::string> actors_;
    std::set<std::string> processed_commands_;
    std::map<std::string, std::map<Id, People>> units_;
    std::uint64_t next_typed_id_{1};
    std::map<std::pair<EntityKind,std::string>,TypedEntityId> typed_entities_;
    std::map<std::uint64_t,EntityKind> typed_active_ids_;
    std::set<std::uint64_t> retired_typed_ids_;
    std::map<std::uint64_t,std::uint64_t> typed_successor_;
    std::set<std::string> regions_with_initial_stock_;
    std::set<std::string> formations_;
    std::map<std::string,std::string> unit_to_formation_;
    std::map<std::tuple<std::string,std::string,std::string>,Quantity> equipment_assignment_;

    std::set<std::string> tracked_assets_;
    std::set<std::pair<std::string, std::string>> security_accounts_;
    struct DividendDue {
        std::string asset, payer, holder;
        Money amount{};
        std::int64_t due{};
    };
    std::map<std::string, DividendDue> dividends_;
    bool healthcare_right_{};
    std::map<std::int64_t,bool> scheduled_public_healthcare_;
    std::map<std::string,PublicInstitution> public_institutions_;
    std::map<std::string,Money> paid_staff_this_month_;
    Money total_bad_debt_{};
    bool bank_insolvent_{};
    std::map<std::string,People> regional_food_grievance_;
    std::map<std::string,FinancialLedger> foreign_ledgers_;
    std::set<std::pair<std::string,std::string>> foreign_holders_;
    std::map<std::string,Reparation> reparations_;
    std::map<std::string,ProductionFacility> facilities_;
    std::map<std::int64_t,std::map<std::string,Quantity>> scheduled_site_repairs_;
    std::map<std::pair<std::string,std::string>, MarketQuote> markets_;
    std::map<Id, Quantity> household_output_;
    std::map<std::pair<Id,std::string>, Quantity> household_consumed_;
    std::map<std::string, Id> person_anchors_;
    std::map<Id, People> anchor_counts_;
    std::set<std::pair<std::string,std::string>> ballot_ids_;
    std::map<std::string,std::map<std::string,People>> ballot_votes_;
    std::set<std::string> interest_groups_;
    std::map<std::pair<Id,std::string>, People> membership_;
    std::map<Id, People> participation_this_month_, strike_active_;
    std::map<std::int64_t,std::map<Id,People>> scheduled_strikes_;
    std::map<std::string, GovernmentState> governments_;
    std::map<std::int64_t, std::vector<CabinetChange>> scheduled_cabinets_;
    std::map<std::string, Shipment> shipments_;
    std::set<std::string> tracked_goods_;
    std::map<std::string, std::string> sovereignty_, military_control_;
    std::map<std::int64_t, bool> scheduled_conscription_;
    std::map<std::int64_t, int> scheduled_income_tax_;
    int income_tax_ppm_{100'000};
    bool conscription_enabled_{true};
    std::map<std::int64_t, int> scheduled_voting_age_;
    ElectionResult last_election_;
    phaseb::DemographyRules demographic_rules_;
    std::vector<std::string> events_;
    std::uint64_t commit_id_{};
    std::uint64_t sequence_{};
    int stage_{-1}, voting_age_years_{21};
    bool election_this_month_{}, demography_enabled_{};

    // Exact nonnegative fraction comparison using Euclid/continued fractions:
    // no 128-bit GNU extension, overflow or floating-point ties (MSVC friendly).
    static int compare_nonnegative_ratios(std::uint64_t a, std::uint64_t b,
                                          std::uint64_t c, std::uint64_t d) {
        ensure(b != 0 && d != 0, "zero election divisor");
        int direction = 1;
        for (;;) {
            const auto qa = a / b, qc = c / d;
            if (qa != qc) return (qa > qc ? 1 : -1) * direction;
            a %= b; c %= d;
            if (a == 0 || c == 0) {
                if (a == c) return 0;
                return (a == 0 ? -1 : 1) * direction;
            }
            std::swap(a, b);
            std::swap(c, d);
            direction = -direction;
        }
    }
    static std::string security_account(const std::string& who, const std::string& asset) {
        return who + ":security:" + asset;
    }
    void ensure_security_account(const std::string& who, const std::string& asset) {
        if (security_accounts_.insert({who, asset}).second)
            money_.add_account(security_account(who, asset), who, AccountType::asset);
    }
    void collect_market_grievances() {
        for(const auto& [key,m]:markets_) {
            const auto& [region,good]=key;
            if(good=="food"&&m.unmet>0) {
                regional_food_grievance_[region]=phaseb::add_checked(
                    regional_food_grievance_[region],m.unmet);
                events_.push_back("observed-food-shortage");
            }
        }
    }
    void reprice_markets() {
        for(auto& [key,m]:markets_) {
            (void)key;
            if(m.unmet>0) {
                const Money step=std::max<Money>(1,m.price/10);
                m.price=std::min(m.ceiling,phaseb::add_checked(m.price,step));
            } else if(m.solvent_demand>0 && m.delivered==m.solvent_demand &&
                      m.stock_offered>m.solvent_demand+m.target_stock) {
                const Money step=std::max<Money>(1,m.price/10);
                m.price=std::max(m.floor,m.price-step);
            }
        }
    }
    [[nodiscard]] People coalition_seats(const std::vector<std::string>& party_ids) const {
        std::set<std::string> unique;
        People total = 0;
        for (const auto& party : party_ids) {
            ensure(unique.insert(party).second && last_election_.seats.contains(party),
                   "duplicate or unknown coalition party");
            total = phaseb::add_checked(total, last_election_.seats.at(party));
        }
        return total;
    }
    TypedEntityId register_typed(EntityKind kind,const std::string& name) {
        ensure(!name.empty()&&!typed_entities_.contains({kind,name}),"duplicate typed entity");
        const TypedEntityId id{kind,next_typed_id_++};
        typed_entities_[{kind,name}]=id;
        typed_active_ids_[id.id]=kind;
        return id;
    }
    static std::string fx_bank(const std::string& c,const std::string& k){
        return "FXBANK:"+c+":"+k;
    }
    static std::string fx_acc(const std::string& owner,const std::string& c,const std::string& k){
        return "FX:"+c+":"+owner+":"+k;
    }
    void ensure_fx_ledger(const std::string& currency){
        if(foreign_ledgers_.contains(currency))return;
        FinancialLedger l;
        l.add_account(fx_bank(currency,"reserves"),"FXBANK:"+currency,AccountType::asset);
        l.add_account(fx_bank(currency,"deposit"),"FXBANK:"+currency,AccountType::liability);
        foreign_ledgers_.emplace(currency,std::move(l));
    }
    void ensure_fx_account(const std::string& owner,const std::string& currency){
        ensure(actors_.contains(owner),"unknown currency account owner");
        ensure_fx_ledger(currency);
        if(foreign_holders_.insert({owner,currency}).second){
            auto& l=foreign_ledgers_.at(currency);
            l.add_account(fx_acc(owner,currency,"cash"),owner,AccountType::asset);
            l.add_account(fx_acc(owner,currency,"capital"),owner,AccountType::equity);
        }
    }
    static std::string account(const std::string& who, const std::string& suffix) {
        return who + ":" + suffix;
    }
    std::string unique(const char* kind) {
        return std::string(kind) + ":" + std::to_string(month()) + ":" + std::to_string(++sequence_);
    }
    void require_stage(std::initializer_list<int> accepted) const {
        for (int s : accepted) if (s == stage_) return;
        throw std::logic_error("event submitted in wrong stage");
    }
    [[nodiscard]] People employed_by(const std::string& who) const {
        People n = 0;
        for (const auto& [id, job] : people_.jobs()) {
            (void)id;
            if (job.employer == who) {
                const People unavailable = currently_striking(id);
                ensure(unavailable<=job.count, "strike exceeds employment");
                n = phaseb::add_checked(n, job.count-unavailable);
            }
        }
        return n;
    }
    void open_month() {
        paid_staff_this_month_.clear();
        for(auto& [key,svc]:public_institutions_) {(void)key;svc.provided=0;}
        if(auto law=scheduled_public_healthcare_.find(month());
           law!=scheduled_public_healthcare_.end()){
            healthcare_right_=law->second;
            scheduled_public_healthcare_.erase(law);
        }
        for(auto& [id,t]:reparations_) {
            (void)id;
            if(!t.recognized&&t.effective_month<=month()){
                money_.post({unique("reparation-liability"),"treaty creates payable and receivable",{
                    {account(t.payer,"expense"),t.total,0},
                    {account(t.payer,"reparations_payable"),0,t.total},
                    {account(t.recipient,"reparations_receivable"),t.total,0},
                    {account(t.recipient,"revenue"),0,t.total}}});
                t.recognized=true;
            }
        }
        if(auto pending=scheduled_site_repairs_.find(month());
           pending!=scheduled_site_repairs_.end()){
            for(const auto& [id,n]:pending->second){
                auto& site=facilities_.at(id);
                ensure(n<=site.damaged,"rebuild exceeds war damage");
                site.damaged-=n;
            }
            scheduled_site_repairs_.erase(pending);
        }
        household_output_.clear();
        household_consumed_.clear();
        for(auto& [key,m]:markets_) {
            (void)key;
            m.solvent_demand=0;m.delivered=0;m.unmet=0;m.stock_offered=0;
        }
        participation_this_month_.clear();
        strike_active_.clear();
        if(auto strikers=scheduled_strikes_.find(month());strikers!=scheduled_strikes_.end()){
            strike_active_=strikers->second;
            scheduled_strikes_.erase(strikers);
        }
        if (auto pending=scheduled_cabinets_.find(month()); pending!=scheduled_cabinets_.end()) {
            for (const auto& c : pending->second) {
                auto& g=governments_.at(c.country);
                if (c.replace_head) g.head_of_state=c.new_head;
                if (c.replace_prime) g.prime_minister=c.new_prime_minister;
                ++g.revision;
            }
            scheduled_cabinets_.erase(pending);
        }
        auto it = scheduled_voting_age_.find(month());
        if (it != scheduled_voting_age_.end()) {
            voting_age_years_ = it->second;
            scheduled_voting_age_.erase(it);
        }
        if (auto c = scheduled_conscription_.find(month()); c != scheduled_conscription_.end()) {
            conscription_enabled_ = c->second;
            scheduled_conscription_.erase(c);
        }
        if (auto t = scheduled_income_tax_.find(month()); t != scheduled_income_tax_.end()) {
            income_tax_ppm_ = t->second;
            scheduled_income_tax_.erase(t);
        }
        for (auto& [key, cargo] : shipments_) {
            (void)key;
            if (cargo.delivered || cargo.arrival_month > month()) continue;
            auto transit = InventoryKey{"TRANSIT:" + cargo.id, "in_transit", cargo.good};
            if (cargo.loss) goods_.consume(transit, cargo.loss);
            const Quantity delivered = cargo.quantity - cargo.loss;
            if (delivered) goods_.transfer(transit, {cargo.to, cargo.destination, cargo.good}, delivered);
            cargo.delivered = true;
            events_.push_back("cargo-arrival");
        }
    }
    // Phase B's autonomous population dynamics is allowed only as part of the
    // same staging world. Full-slice active military transitions are reconciled.
    // Ambiguous partial military allocations fail rather than duplicating people.
    void sync_service_transitions(std::size_t begin) {
        const auto& log = people_.events();
        for (std::size_t i = begin; i < log.size(); ++i) {
            const auto& e = log[i];
            if (!e.from || e.from == e.to || !e.count) continue;
            People n = assigned(e.from);
            if (!n) continue;
            ensure(n >= e.count, "ambiguous military demographic allocation");
            for (auto& [unit, rows] : units_) {
                auto it = rows.find(e.from);
                if (it == rows.end()) continue;
                const People moving = std::min(it->second, e.count);
                it->second -= moving;
                if (!it->second) rows.erase(it);
                if (e.to && moving && people_.has_slice(e.to) &&
                    people_.demographic(people_.slice(e.to).key.demographic).service ==
                        phaseb::Service::active)
                    rows[e.to] = phaseb::add_checked(rows[e.to], moving);
                break;
            }
        }
    }
};
} // namespace vic25::phasec
