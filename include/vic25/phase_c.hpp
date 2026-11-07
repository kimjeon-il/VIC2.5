#pragma once

// Phase C integration vertical slice. Reuses Phase B as the ONLY population
// headcount and job authority, and S0's checked double-entry and stock ledgers.
// Implements actual staged transactions; high-detail political/war models remain
// separate future modules (see docs/phase-c/integration/).
#include "vic25/phase_b.hpp"
#include "vic25/ledgers.hpp"
#include "vic25/shared_reservations.hpp"
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
// A query projection over C's existing financial accounts; not another ledger.
struct CountryFiscalView {
    std::string country, treasury;
    Money cash{}, bank_debt{};
    int income_tax_ppm{};
    Money bond_principal{},unpaid_coupons{};
};
struct WageArrearsClaim {
    std::string id,employer,worker,treasury,country;
    Money accrued{},outstanding{};
    std::int64_t issued_day{};
};
struct SovereignBond {
    std::string id,country,treasury;
    Money original_face{};
    int coupon_ppm{};
    std::int64_t issue_day{},maturity_day{};
};
struct ArmyDeployment {
    std::string unit,country,region;
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
struct DividendDue {
    std::string asset, payer, holder;
    Money amount{};
    std::int64_t due{};
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
    friend struct SnapshotCodec;
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
    void seed_annual_bank_interest_ppm(int ppm) {
        ensure(stage_==-1&&commit_id_==0&&ppm>=0&&ppm<=1'000'000,
               "invalid annual bank interest policy");
        bank_interest_annual_ppm_=ppm;
    }
    [[nodiscard]] int annual_bank_interest_ppm()const {
        return bank_interest_annual_ppm_;
    }
    // C and D share the single transient month-local authority, while
    // FinancialLedger / InventoryLedger / Population remain the only stocks.
    void phase_d_enable_shared_resources() {
        require_stage({0});
        phase_d_reservation_mode_=true;
    }
    [[nodiscard]] bool phase_d_shared_resources_enabled() const {
        return phase_d_reservation_mode_;
    }
    [[nodiscard]] Money phase_d_cash_available(const std::string& owner) const {
        ensure(actors_.contains(owner),"unknown funds owner");
        return cash(owner)-resources_.cash_held(owner);
    }
    [[nodiscard]] Quantity phase_d_material_available(const std::string& actor,
             const std::string& region,const std::string& good)const{
        const InventoryKey key{actor,region,good};
        if(stage_==4&&phase_d_reservation_mode_)
            return resources_.available_for_stage4(key,stock(actor,region,good));
        return stock(actor,region,good)-resources_.goods_held(key);
    }
    void phase_d_reserve_cash(const std::string& owner,const std::string& purpose,
                              Money amount) {
        require_stage({1});ensure(phase_d_reservation_mode_,"unbound resource phase");
        resources_.reserve_cash(owner,purpose,amount,cash(owner));
    }
    void phase_d_release_cash(const std::string& owner,const std::string& purpose,
                              Money amount) {
        require_stage({3,5,7,8});
        resources_.release_cash(owner,purpose,amount);
    }
    void phase_d_reserve_material(const std::string& owner,const std::string& region,
          const std::string& good,const std::string& purpose,Quantity amount) {
        require_stage({1,3});
        ensure(phase_d_reservation_mode_,"unbound resource phase");
        const InventoryKey key{owner,region,good};
        resources_.reserve_goods(key,purpose,amount,stock(owner,region,good));
    }
    void phase_d_use_reserved_material(const std::string& owner,
          const std::string& region,const std::string& good,
          const std::string& purpose,Quantity amount){
        require_stage({4});ensure(phase_d_reservation_mode_,"unbound resource phase");
        const InventoryKey key{owner,region,good};
        resources_.release_goods(key,purpose,amount);
        phase_d_use_material(owner,region,good,amount);
    }
    // Phase G releases the same Phase C/D inventory reservations at a real
    // settlement stage; the actual C inventory ledger remains the sole owner.
    void phase_g_release_reserved_goods(const std::string& owner,
                 const std::string& region,const std::string& good,
                 const std::string& purpose,Quantity amount) {
        require_stage({3,7});
        ensure(phase_d_reservation_mode_,"phase G reservation mode disabled");
        resources_.release_goods({owner,region,good},purpose,amount);
    }
    Id phase_d_certify_occupational_training(Id source,People count,
                                             std::uint8_t new_skill){
        require_stage({10});
        ensure(phase_d_reservation_mode_&&people_.has_slice(source) &&
               count>0 && count<=people_.slice(source).count &&
               people_.employed(source)==0 && new_skill<=100,
               "training outcome needs real unemployed eligible POP partition");
        const auto& slice=people_.slice(source);
        const auto& economy=people_.economic(slice.key.economic);
        auto education=people_.education(economy.education);
        ensure(new_skill>education.occupational_skill,
               "unearned or unchanged occupational qualification");
        education.occupational_skill=new_skill;
        const Id target=people_.change_education(source,count,education,
                                                    phaseb::EventKind::training,0);
        events_.push_back("phase-d-real-POP-occupational-training");
        return target;
    }
    void phase_d_use_labor(const std::string& owner,People required){
        require_stage({4});
        ensure(phase_d_reservation_mode_,"unbound resource phase");
        resources_.use_labor(owner,required,employed_by(owner));
    }
    void phase_d_use_facility(const std::string& id,Quantity units){
        require_stage({4});
        ensure(phase_d_reservation_mode_,"unbound resource phase");
        const auto& site=facilities_.at(id);
        resources_.use_site(id,units,site.capacity-site.damaged);
    }
    [[nodiscard]] People phase_d_shared_labor_used(const std::string& owner)const{
        return resources_.labor_used(owner);
    }
    [[nodiscard]] Quantity phase_d_shared_site_used(const std::string& site)const{
        return resources_.site_used(site);
    }
    [[nodiscard]] Money phase_d_shared_cash_held(const std::string& owner)const{
        return resources_.cash_held(owner);
    }
    [[nodiscard]] const InventoryLedger& inventory() const { return goods_; }
    [[nodiscard]] const std::vector<std::string>& journal() const { return events_; }
    [[nodiscard]] const ElectionResult& election() const { return last_election_; }
    // Nationally authoritative election results. The original global result
    // is retained for previously encoded Phase C saves and standalone tests.
    [[nodiscard]] const ElectionResult& country_election(
            const std::string& country) const {
        return country_elections_.at(country);
    }
    [[nodiscard]] const std::map<std::string,ElectionResult>&
            country_elections() const { return country_elections_; }
    [[nodiscard]] std::int64_t country_election_month(
            const std::string& country) const {
        return country_election_months_.at(country);
    }
    [[nodiscard]] int voting_age() const { return voting_age_years_; }
    [[nodiscard]] const std::string& military_control(const std::string& region) const {
        return military_control_.at(region);
    }
    [[nodiscard]] const std::string& sovereignty(const std::string& region) const {
        return sovereignty_.at(region);
    }

    [[nodiscard]] bool has_actor(const std::string& who)const {
        return actors_.contains(who);
    }
    [[nodiscard]] bool has_government(const std::string& country)const {
        return governments_.contains(country);
    }
    [[nodiscard]] bool has_any_government()const {
        return !governments_.empty();
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
    // Army national command and actual location are separate from the
    // civilian sovereignty of the province containing its soldiers.
    void seed_army_deployment(const std::string& unit,
                              const std::string& country,
                              const std::string& region) {
        ensure(stage_==-1&&commit_id_==0&&units_.contains(unit)&&
               governments_.contains(country)&&
               sovereignty_.contains(region)&&
               !army_deployments_.contains(unit),
               "invalid historical military deployment");
        ensure(sovereignty_.at(region)==country||
               military_control_.at(region)==country,
               "initial army not on its country's controlled territory");
        army_deployments_.emplace(unit,ArmyDeployment{unit,country,region});
    }
    [[nodiscard]] const std::map<std::string,ArmyDeployment>&
            army_deployments()const{return army_deployments_;}
    [[nodiscard]] const std::map<Id,People>& unit_slices(
            const std::string& unit)const {
        return units_.at(unit);
    }
    void phase_time_move_army(const std::string& unit,
                              const std::string& destination) {
        require_stage({10});
        auto it=army_deployments_.find(unit);
        ensure(it!=army_deployments_.end()&&
               sovereignty_.contains(destination)&&
               active_strength(unit)>0,"invalid real army move");
        it->second.region=destination;
        events_.push_back("dated-army-relocation");
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
        if(phase_d_reservation_mode_)
            resources_.assert_cash_available(buyer,units,cash(buyer));
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
        if(phase_d_reservation_mode_)
            resources_.assert_cash_available(d.payer,d.amount,cash(d.payer));
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
        if(phase_d_reservation_mode_)
            resources_.assert_cash_available(buyer,face,cash(buyer));
        ensure(!bond_id.empty() && face > 0 && actors_.contains(government) &&
               actors_.contains(buyer) && government != buyer && cash(buyer) >= face &&
               !tracked_assets_.contains("bond:" + bond_id), "unfunded bond issue");
        // Separate actual bank principal from new sovereign securities before
        // the common historical debt account receives the bond liability.
        if(!bank_principal_by_borrower_.contains(government))
            bank_principal_by_borrower_[government]=outstanding_loan(government);
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
    [[nodiscard]] Money sovereign_bond_principal(const std::string& country)const {
        Money total=0;
        for(const auto& [id,bond]:sovereign_bonds_) {
            if(bond.country!=country)continue;
            for(const auto& [holder,units]:ownership_.holders("bond:"+id)) {
                (void)holder;
                total=phaseb::add_checked(total,units);
            }
        }
        return total;
    }
    [[nodiscard]] const std::map<std::string,SovereignBond>& sovereign_bonds()const {
        return sovereign_bonds_;
    }
    [[nodiscard]] bool has_active_sovereign_bonds()const {
        if(!sovereign_coupon_due_.empty())return true;
        for(const auto& [id,bond]:sovereign_bonds_) {
            (void)bond;
            for(const auto& [owner,units]:ownership_.holders("bond:"+id)) {
                (void)owner;
                if(units>0)return true;
            }
        }
        return false;
    }
    // The investor must have the actual cash. Signing a budget does not
    // create fictitious bond demand or state funds.
    void issue_sovereign_bond(const std::string& id,
                              const std::string& country,
                              const std::string& prime_minister,
                              const std::string& buyer,Money face,
                              int coupon_ppm,std::int64_t today,
                              std::int64_t maturity_day) {
        require_stage({1});
        ensure(governments_.contains(country)&&
               governments_.at(country).prime_minister==prime_minister&&
               has_country_treasury(country)&&
               !sovereign_bonds_.contains(id)&&coupon_ppm>=0&&
               coupon_ppm<=1'000'000&&today>=0&&maturity_day>today,
               "unlawful or unbacked sovereign bond offer");
        const auto& treasury=country_treasury(country);
        ensure(treasury!=buyer,"government cannot self-buy its bond");
        issue_bond(id,treasury,buyer,face);
        sovereign_bonds_.emplace(id,SovereignBond{
            id,country,treasury,face,coupon_ppm,today,maturity_day});
        events_.push_back("real-sovereign-bond-financed");
    }
    void phase_time_service_sovereign_bonds(std::int64_t today) {
        require_stage({5});
        ensure(today>=0,"invalid sovereign bond servicing day");
        constexpr Money denominator=365'000'000;
        for(const auto& [id,bond]:sovereign_bonds_) {
            if(today<=bond.issue_day)continue;
            const auto holders=ownership_.holders("bond:"+id);
            for(const auto& [holder,principal]:holders) {
                if(principal<=0||bond.coupon_ppm<=0)continue;
                const auto key=std::make_pair(id,holder);
                const Money old=sovereign_coupon_fraction_[key];
                const Money whole=phaseb::mul_checked(
                    principal/denominator,bond.coupon_ppm);
                const Money part=phaseb::add_checked(phaseb::mul_checked(
                    principal%denominator,bond.coupon_ppm),old);
                const Money coupon=phaseb::add_checked(whole,part/denominator);
                sovereign_coupon_fraction_[key]=part%denominator;
                if(coupon<=0)continue;
                const auto payable=account(bond.treasury,"bond_coupon_payable");
                const auto receivable=account(holder,"bond_coupon_receivable");
                if(!money_.has_account(payable))
                    money_.add_account(payable,bond.treasury,AccountType::liability);
                if(!money_.has_account(receivable))
                    money_.add_account(receivable,holder,AccountType::asset);
                money_.post({unique("bond-coupon-accrual"),
                    "interest on outstanding real government security",{
                    {account(bond.treasury,"expense"),coupon,0},
                    {payable,0,coupon},
                    {receivable,coupon,0},
                    {account(holder,"revenue"),0,coupon}}});
                sovereign_coupon_due_[key]=phaseb::add_checked(
                    sovereign_coupon_due_[key],coupon);
            }
        }
        for(auto it=sovereign_coupon_due_.begin();
            it!=sovereign_coupon_due_.end();) {
            const auto& [id,holder]=it->first;
            const auto& bond=sovereign_bonds_.at(id);
            const Money free=std::max<Money>(0,
                phase_d_cash_available(bond.treasury));
            const Money paid=std::min(it->second,free);
            if(paid<=0){++it;continue;}
            money_.post({unique("bond-coupon-paid"),
                         "interest paid from existing country cash",{
                {account(bond.treasury,"bond_coupon_payable"),paid,0},
                {account(bond.treasury,"cash"),0,paid},
                {account(holder,"cash"),paid,0},
                {account(holder,"bond_coupon_receivable"),0,paid}}});
            it->second-=paid;
            if(it->second==0)it=sovereign_coupon_due_.erase(it);
            else ++it;
        }
        for(const auto& [id,bond]:sovereign_bonds_) {
            if(today<bond.maturity_day)continue;
            const auto asset="bond:"+id;
            const auto holders=ownership_.holders(asset);
            for(const auto& [holder,principal]:holders) {
                if(principal<=0)continue;
                const Money free=std::max<Money>(0,
                    phase_d_cash_available(bond.treasury));
                const Money paid=std::min(principal,free);
                if(paid<=0)continue; // valid unpaid maturing principal
                money_.post({unique("sovereign-bond-redemption"),
                    "cash-financed repayment of actual government bonds",{
                    {account(bond.treasury,"debt"),paid,0},
                    {account(bond.treasury,"cash"),0,paid},
                    {account(holder,"cash"),paid,0},
                    {security_account(holder,asset),0,paid}}});
                ownership_.redeem(asset,holder,paid);
                events_.push_back("sovereign-principal-actually-repaid");
            }
        }
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
        if(phase_d_reservation_mode_)
            resources_.assert_goods_available({service.operator_id,service.region,service.good},
                                              patients,stock(service.operator_id,service.region,service.good));
        if(phase_d_reservation_mode_)
            resources_.use_labor(service.operator_id,
                                 (patients+service.patients_per_worker-1)/service.patients_per_worker,
                                 employed_by(service.operator_id));
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
    // Phase D additive bridge: material/production share C's real inventory
    // and Phase B's real employment, not an independent Phase D stock.
    [[nodiscard]] People phase_d_workforce(const std::string& owner) const {
        ensure(actors_.contains(owner), "unknown phase D employer");
        return employed_by(owner);
    }
    void phase_d_use_material(const std::string& actor,const std::string& region,
                              const std::string& good,Quantity qty) {
        require_stage({4});
        ensure(actors_.contains(actor) && qty>0 &&
               stock(actor,region,good)>=qty, "unbacked phase D material");
        resources_.assert_stage4_available({actor,region,good},qty,stock(actor,region,good));
        goods_.consume({actor,region,good},qty);
        events_.push_back("phase-d-actual-material-used");
    }
    void phase_d_transform(const std::string& actor,const std::string& region,
                           const std::vector<std::pair<std::string,Quantity>>& inputs,
                           const std::vector<std::pair<std::string,Quantity>>& outputs,
                           People workers_required) {
        require_stage({4});
        ensure(actors_.contains(actor) && sovereignty_.contains(region) &&
               workers_required>0 && workers_required<=employed_by(actor) &&
               !inputs.empty() && !outputs.empty(),
               "phase D production missing actor, worker, inputs or outputs");
        std::map<std::string,Quantity> grouped;
        for(const auto& [good,qty]:inputs) {
            ensure(!good.empty() && qty>0,"invalid phase D production material");
            grouped[good]=phaseb::add_checked(grouped[good],qty);
        }
        for(const auto& [good,qty]:grouped)
            resources_.assert_stage4_available({actor,region,good},qty,stock(actor,region,good));
        for(const auto& [good,qty]:outputs)
            ensure(!good.empty() && qty>0,"invalid phase D output");
        for(const auto& [good,qty]:grouped)
            goods_.consume({actor,region,good},qty);
        for(const auto& [good,qty]:outputs) {
            goods_.produce({actor,region,good},qty);
            resources_.record_stage4_output({actor,region,good},qty);
            tracked_goods_.insert(good);
        }
        events_.push_back("phase-d-input-output-production");
    }
    // A nonrenewable/renewable resource lot is emitted only after Phase D
    // has validated an actual deposit, extraction workers and facility.
    // Unlike manufacturing, verified extraction has an external in-situ
    // resource debit in the Phase D domain journal.
    void phase_d_emit_extraction(const std::string& holder,const std::string& region,
                                 const std::string& good,Quantity qty,
                                 const std::string& deposit_id) {
        require_stage({4});
        ensure(phase_d_reservation_mode_&&actors_.contains(holder) &&
               sovereignty_.contains(region)&&!good.empty() &&
               !deposit_id.empty()&&qty>0,"invalid geological extraction");
        goods_.produce({holder,region,good},qty);
        resources_.record_stage4_output({holder,region,good},qty);
        tracked_goods_.insert(good);
        events_.push_back("phase-d-verified-extraction:"+deposit_id);
    }
    // Cash-settled pilot path only: a verified cost was already paid via
    // pay(), hence the expense balance can be capitalized once into WIP.
    // This is NOT unpaid-progress invoicing or full CE accrual accounting.
    void phase_d_capitalize_paid_cost(const std::string& owner,Money amount) {
        require_stage({8});
        ensure(actors_.contains(owner) && amount>0 &&
               money_.balance(account(owner,"expense"))>=amount,
               "phase D WIP lacks matching paid cost");
        const auto key=account(owner,"wip");
        try { (void)money_.balance(key); }
        catch(const std::out_of_range&) {
            money_.add_account(key,owner,AccountType::asset);
        }
        money_.post({unique("phase-d-wip"),"paid construction expense capitalized",{
            {key,amount,0},{account(owner,"expense"),0,amount}}});
        events_.push_back("phase-d-capitalized-verified-paid-work");
    }
    // Accrual path is separate from the older paid-cost capitalization path.
    // Verified progress creates a *receivable/payable*, not spendable cash.
    void phase_d_accrue_certified_work(const std::string& client,
                       const std::string& contractor,Money amount) {
        require_stage({8});
        ensure(client!=contractor&&actors_.contains(client) &&
               actors_.contains(contractor)&&amount>0,
               "invalid certified progress counterparties");
        const auto register_account=[&](const std::string& owner,
                                        const std::string& suffix,AccountType type){
            const auto key=account(owner,suffix);
            try {(void)money_.balance(key);}
            catch(const std::out_of_range&) {money_.add_account(key,owner,type);}
        };
        register_account(client,"wip",AccountType::asset);
        register_account(client,"construction_payable",AccountType::liability);
        register_account(contractor,"construction_receivable",AccountType::asset);
        money_.post({unique("phase-d-accrual"),"certified construction work accrued",{
            {account(client,"wip"),amount,0},
            {account(client,"construction_payable"),0,amount},
            {account(contractor,"construction_receivable"),amount,0},
            {account(contractor,"revenue"),0,amount}}});
        events_.push_back("phase-d-certified-construction-accrued");
    }
    void phase_d_settle_certified_work(const std::string& client,
                        const std::string& contractor,Money amount) {
        require_stage({5,8});
        ensure(actors_.contains(client)&&actors_.contains(contractor)&&
               client!=contractor&&amount>0&&cash(client)>=amount &&
               -money_.balance(account(client,"construction_payable"))>=amount &&
               money_.balance(account(contractor,"construction_receivable"))>=amount,
               "unfunded or unbacked certified work payment");
        if(phase_d_reservation_mode_)
            resources_.assert_cash_available(client,amount,cash(client));
        money_.post({unique("phase-d-accrual-settle"),"cash settles construction receivable",{
            {account(client,"construction_payable"),amount,0},
            {account(client,"cash"),0,amount},
            {account(contractor,"cash"),amount,0},
            {account(contractor,"construction_receivable"),0,amount}}});
        events_.push_back("phase-d-certified-invoice-paid");
    }
    [[nodiscard]] Money phase_d_payable(const std::string& client)const{
        try {return -money_.balance(account(client,"construction_payable"));}
        catch(const std::out_of_range&){return 0;}
    }
    [[nodiscard]] Money phase_d_wip(const std::string& owner) const {
        ensure(actors_.contains(owner),"unknown phase D WIP owner");
        try { return money_.balance(account(owner,"wip")); }
        catch(const std::out_of_range&) { return 0; }
    }
    // Called at month m+1 stage0 only after Phase D work / finance /
    // inspection gates have been validated against the same staged World.
    void phase_d_commission_facility(const std::string& id,const std::string& owner,
                                     const std::string& region,Quantity capacity,
                                     Money book_value) {
        require_stage({0});
        ensure(!id.empty() && !facilities_.contains(id) && actors_.contains(owner) &&
               sovereignty_.contains(region) && capacity>0 && book_value>0 &&
               phase_d_wip(owner)>=book_value,"phase D commission prerequisites");
        const auto fixed=account(owner,"fixed_asset");
        try { (void)money_.balance(fixed); }
        catch(const std::out_of_range&) {
            money_.add_account(fixed,owner,AccountType::asset);
        }
        money_.post({unique("phase-d-commission"),"WIP becomes installed fixed capital",{
            {fixed,book_value,0},{account(owner,"wip"),0,book_value}}});
        facilities_.emplace(id,ProductionFacility{id,owner,region,capacity,0});
        const std::string title="FACILITY:"+id;
        ownership_.issue(title,owner,1);
        tracked_assets_.insert(title);
        register_typed(EntityKind::site,id);
        events_.push_back("phase-d-commissioned-existing-paid-WIP");
    }
    void war_damage_facility(const std::string& id,Quantity destroyed) {
        require_stage({10});
        auto& site=facilities_.at(id);
        ensure(destroyed>0 && site.damaged+destroyed<=site.capacity,
               "damage exceeds physical facility");
        site.damaged+=destroyed;
        events_.push_back("physical-facility-war-damage");
    }
    // Owner-directed physical maintenance is performed at stage 4 on the
    // actual game date. Shares the same C/D POP, stock and facility ledgers.
    void phase_time_maintain_facility(const std::string& id,
                                     const std::string& good,
                                     Quantity units,Quantity material_per_unit,
                                     People workers) {
        require_stage({4});
        auto& site=facilities_.at(id);
        ensure(units>0&&material_per_unit>0&&workers>0&&
               units<=site.damaged&&!good.empty()&&
               phase_d_reservation_mode_,
               "TIME_D_INVALID_FACILITY_MAINTENANCE");
        Quantity reserved=0;
        for(const auto& [due,sites]:scheduled_site_repairs_) {
            if(due<=month())continue;
            if(const auto it=sites.find(id);it!=sites.end())
                reserved=phaseb::add_checked(reserved,it->second);
        }
        ensure(reserved<=site.damaged&&units<=site.damaged-reserved,
               "TIME_D_REPAIR_ALREADY_SCHEDULED");
        const Quantity consumed=positive_product(units,material_per_unit);
        ensure(phase_d_material_available(site.owner,site.region,good)>=consumed &&
               employed_by(site.owner)>=workers,
               "TIME_D_MAINTENANCE_UNFUNDED_INPUTS");
        resources_.use_labor(site.owner,workers,employed_by(site.owner));
        resources_.assert_stage4_available(
            {site.owner,site.region,good},consumed,
            stock(site.owner,site.region,good));
        goods_.consume({site.owner,site.region,good},consumed);
        site.damaged-=units;
        events_.push_back("daily-facility-restoration-actual-material-and-labor");
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
        if(phase_d_reservation_mode_) {
            resources_.use_labor(site.owner,construction_workers,employed_by(site.owner));
            resources_.assert_goods_available({site.owner,site.region,building_good},restored,
                                               stock(site.owner,site.region,building_good));
        }
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
        if(phase_d_reservation_mode_)
            resources_.assert_cash_available(t.payer,amount,cash(t.payer));
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
            if(phase_d_reservation_mode_)
                resources_.assert_cash_available(old_owner,moving_cash,cash(old_owner));
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
    // Foreign-denominated barter/merchandise transfer still uses the same
    // physical inventory owner, but is settled by the foreign currency ledger.
    void phase_g_transfer_goods(const std::string& seller,const std::string& buyer,
                                const std::string& region,const std::string& good,
                                Quantity amount) {
        require_stage({3,7});
        ensure(seller!=buyer && amount>0 && stock(seller,region,good)>=amount,
               "G_ERR_UNFUNDED_GOODS");
        if(phase_d_reservation_mode_)
            resources_.assert_goods_available({seller,region,good},amount,
                                                stock(seller,region,good));
        goods_.transfer({seller,region,good},{buyer,region,good},amount);
        events_.push_back("GH:foreign-settlement-physical-title-transfer");
    }
    // Phase G/ H must settle foreign-denominated obligations through C's
    // actual per-currency financial ledger. No exchange rate is implied.
    void phase_g_pay_foreign(const std::string& from,const std::string& to,
                             const std::string& currency,Money amount) {
        require_stage({3,5,7,8});
        ensure(from!=to && !from.empty() && !to.empty() &&
               !currency.empty() && currency!="TEST" && amount>0 &&
               foreign_holders_.contains({from,currency}) &&
               fx_balance(from,currency)>=amount,
               "G_ERR_UNFUNDED_FOREIGN_PAYMENT");
        ensure_fx_account(to,currency);
        auto& ledger=foreign_ledgers_.at(currency);
        ledger.post({unique("gh-fx-pay"),"actual foreign-currency transfer",{
            {fx_acc(from,currency,"capital"),amount,0},
            {fx_acc(from,currency,"cash"),0,amount},
            {fx_acc(to,currency,"cash"),amount,0},
            {fx_acc(to,currency,"capital"),0,amount}}});
        events_.push_back("GH:foreign-currency-payment:"+currency);
    }
    // Customs tax is paid into a real government account in C's FinancialLedger.
    // G's customs record must reference an actual event, not become money itself.
    void phase_g_pay_customs(const std::string& importer,
                              const std::string& treasury,Money amount) {
        require_stage({7});
        ensure(!importer.empty() && !treasury.empty() &&
               importer!=treasury && amount>0,
               "G_ERR_INVALID_CUSTOMS_ACCOUNT");
        pay(importer,treasury,amount);
        events_.push_back("GH:customs-duty-paid");
    }
    [[nodiscard]] const Shipment& phase_g_shipment(const std::string& id) const {
        return shipments_.at(id);
    }
    void seed_market(const std::string& region,const std::string& good,
                     Money price, Money floor, Money ceiling, Quantity target_stock=0) {
        ensure(stage_==-1 && commit_id_==0 && !region.empty() && !good.empty() &&
               price>0 && floor>0 && ceiling>=price && floor<=price &&
               target_stock>=0 && !markets_.contains({region,good}),
               "invalid market seed");
        markets_[{region,good}]={price,floor,ceiling,target_stock,0,0,0,0};
    }
    [[nodiscard]] bool has_market(const std::string& region,
                                  const std::string& good)const {
        return markets_.contains({region,good});
    }
    [[nodiscard]] const MarketQuote& market(const std::string& region,const std::string& good) const {
        return markets_.at({region,good});
    }
    // An ephemeral regional seller index derived from actual C Inventory.
    // It is built once per active market day, not once per POP household.
    using MarketSellerIndex=std::map<
        std::pair<std::string,std::string>,std::vector<std::string>>;
    [[nodiscard]] MarketSellerIndex market_seller_index()const {
        MarketSellerIndex result;
        for(const auto& [key,stock_now]:goods_.holdings()) {
            if(stock_now<=0||!actors_.contains(key.holder_id)||
               !markets_.contains({key.region_id,key.good_id}))continue;
            result[{key.region_id,key.good_id}].push_back(key.holder_id);
        }
        for(auto& [key,sellers]:result) {
            (void)key;
            std::sort(sellers.begin(),sellers.end());
            sellers.erase(std::unique(sellers.begin(),sellers.end()),
                          sellers.end());
        }
        return result;
    }
    // One household demand is cleared against real current remaining stock,
    // never the sum of stale seller offers captured earlier in the day.
    Quantity clear_household_market(const std::string& buyer,
                                    const std::string& region,
                                    const std::string& good,Quantity requested,
                                    const std::vector<std::string>& seller_ids) {
        require_stage({7});
        ensure(requested>=0 && markets_.contains({region,good}) &&
               actors_.contains(buyer),"invalid household market request");
        auto& quote=markets_.at({region,good});
        const Quantity solvent=std::min(requested,cash(buyer)/quote.price);
        Quantity total_offered=0;
        std::vector<std::pair<std::string,Quantity>> sellers;
        for(const auto& seller:seller_ids) {
            if(seller==buyer||!actors_.contains(seller))continue;
            const Quantity free=phase_d_material_available(seller,region,good);
            if(free<=0)continue;
            sellers.push_back({seller,free});
            total_offered=phaseb::add_checked(total_offered,free);
        }
        quote.stock_offered=std::max(quote.stock_offered,
            phaseb::add_checked(quote.delivered,total_offered));
        quote.solvent_demand=phaseb::add_checked(quote.solvent_demand,solvent);
        Quantity remaining=solvent;
        for(const auto& [seller,available]:sellers) {
            if(remaining<=0)break;
            const Quantity quantity=std::min(remaining,available);
            if(quantity<=0)continue;
            trade(seller,buyer,region,good,quantity,quote.price);
            remaining-=quantity;
        }
        const Quantity fulfilled=solvent-remaining;
        quote.delivered=phaseb::add_checked(quote.delivered,fulfilled);
        quote.unmet=phaseb::add_checked(quote.unmet,remaining);
        events_.push_back("daily-household-market-clearing");
        return fulfilled;
    }
    Quantity clear_household_market(const std::string& buyer,
                                    const std::string& region,
                                    const std::string& good,Quantity requested) {
        const auto index=market_seller_index();
        const auto it=index.find({region,good});
        return clear_household_market(buyer,region,good,requested,
                   it==index.end()?std::vector<std::string>{}:it->second);
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
        // A second household viewing the same seller's remaining inventory
        // must not count the same physical stock as newly offered supply.
        // This conservative upper bound is exact for one seller per market.
        m.stock_offered=std::max(m.stock_offered,
            phaseb::add_checked(m.delivered-fulfilled,available));
        m.delivered=phaseb::add_checked(m.delivered,fulfilled);
        m.unmet=phaseb::add_checked(m.unmet,solvent-fulfilled);
        if(fulfilled>0) trade(seller,buyer,region,good,fulfilled,m.price);
        events_.push_back("solvent-demand-market-clearance");
        return fulfilled;
    }
    // P0 consumes the goods through the real inventory ledger and records
    // the difference between essential demand and fulfilled goods. Unlike the
    // market-price rule this includes unmet needs caused by insolvency.
    void phase_time_record_food_need_gap(const std::string& region,People unmet){
        require_stage({7});
        ensure(!region.empty()&&unmet>0,"invalid observed food need gap");
        regional_food_grievance_[region]=phaseb::add_checked(
              regional_food_grievance_[region],unmet);
        events_.push_back("daily-pop-food-need-gap");
    }
    // P0 native-day market close. The MonthClose compatibility path must not
    // reprice these same trades or count the same food shortage twice.
    // A small integer price moves no faster than its currency tick permits.
    void phase_time_close_daily_markets(std::int64_t day_index) {
        require_stage({11});
        ensure(day_index>=0,"invalid absolute daily market date");
        // Scarcity grievances reflect conditions, not a permanent ever-growing
        // score. The legacy MonthClose path remains unchanged when P0 is off.
        for(auto& [region,n]:regional_food_grievance_) {
            (void)region;
            if(n>0)n-=std::max<People>(1,n/20);
        }
        for(auto& [key,m]:markets_) {
            const auto& [region,good]=key;
            (void)region;
            (void)good; // Need gaps were already counted once at stage 7.
            // 1% nominal pressure per active day, with deterministic discrete
            // currency ticks for prices below 100 units.
            const Money step=m.price>=100?std::max<Money>(1,m.price/100):
                    (day_index%std::max<Money>(1,100/m.price)==0?1:0);
            if(m.unmet>0 && step>0)
                m.price=std::min(m.ceiling,phaseb::add_checked(m.price,step));
            else if(m.unmet==0 && m.solvent_demand>0 &&
                    m.stock_offered>phaseb::add_checked(m.solvent_demand,m.target_stock) &&
                    step>0)
                m.price=std::max(m.floor,m.price-step);
            m.solvent_demand=0;
            m.delivered=0;
            m.unmet=0;
            m.stock_offered=0;
        }
        events_.push_back("daily-market-clearing-closed");
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
        if(phase_d_reservation_mode_)
            resources_.record_stage4_output({"HH:"+std::to_string(row.key.household),demo.region,good},output);
        events_.push_back("household-actual-in-kind-output");
    }
    void household_consume(Id slice,const std::string& good,Quantity qty) {
        require_stage({7});
        ensure(people_.has_slice(slice)&&qty>0,"invalid household consumption");
        const auto& row=people_.slice(slice);
        const auto& demo=people_.demographic(row.key.demographic);
        if(phase_d_reservation_mode_)
            resources_.assert_goods_available({"HH:"+std::to_string(row.key.household),demo.region,good},qty,
                stock("HH:"+std::to_string(row.key.household),demo.region,good));
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
               people_.age_months_slice(slice)>=
                   static_cast<std::int64_t>(voting_age_years_)*12,
               "ineligible voter");
        ensure(ballot_ids_.insert({election_id,anchor}).second, "duplicate ballot");
        ballot_votes_[election_id][party] =
            phaseb::add_checked(ballot_votes_[election_id][party],1);
        events_.push_back("named-ballot:"+election_id+":"+district);
    }
    // Phase E uses the Phase C eligibility and named-person registry,
    // not a duplicated writable population/electoral ledger.
    [[nodiscard]] Id phase_e_anchor_slice(const std::string& anchor)const {
        ensure(person_anchors_.contains(anchor),"unknown election anchor");
        return person_anchors_.at(anchor);
    }
    [[nodiscard]] People phase_e_eligible_slice(Id slice)const {
        if(!people_.has_slice(slice))return 0;
        const auto& d=people_.demographic(people_.slice(slice).key.demographic);
        return (d.citizenship=="citizen" &&
               d.service!=phaseb::Service::active &&
               d.service!=phaseb::Service::pow &&
               d.service!=phaseb::Service::missing &&
               people_.age_months_slice(slice)>=
                   static_cast<std::int64_t>(voting_age_years_)*12)?
               people_.slice(slice).count:0;
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
    [[nodiscard]] const std::map<std::string,GovernmentState>&
            governments()const {return governments_;}
    [[nodiscard]] bool has_pending_country_tax(const std::string& country)const {
        const auto scheduled=scheduled_country_income_tax_.find(month()+1);
        return scheduled!=scheduled_country_income_tax_.end() &&
               scheduled->second.contains(country);
    }
    // Optional explicit country->treasury binding. The wallet remains in the
    // original C FinancialLedger; no sovereign cash/debt is duplicated.
    void seed_country_treasury(const std::string& country,
                               const std::string& wallet) {
        ensure(stage_==-1&&commit_id_==0&&governments_.contains(country)&&
               actors_.contains(wallet)&&!country_treasuries_.contains(country),
               "invalid country treasury binding");
        for(const auto& [other,actor]:country_treasuries_)
            ensure(actor!=wallet||other==country,
                   "shared treasury across sovereign countries");
        country_treasuries_.emplace(country,wallet);
    }
    [[nodiscard]] bool has_country_treasury(const std::string& country)const {
        return governments_.contains(country) &&
               (country_treasuries_.contains(country)||actors_.contains(country));
    }
    [[nodiscard]] const std::string& country_treasury(
            const std::string& country)const {
        ensure(governments_.contains(country),"unknown fiscal country");
        const auto bound=country_treasuries_.find(country);
        if(bound!=country_treasuries_.end())return bound->second;
        // Explicit country wallet is the only safe convention, never infer a
        // treasury from a random company or another state's wallet.
        ensure(actors_.contains(country),"country treasury needs explicit binding");
        return governments_.at(country).country;
    }
    [[nodiscard]] const std::string& country_financial_wallet(
            const std::string& country)const {
        if(governments_.contains(country))return country_treasury(country);
        ensure(actors_.contains(country),"customs country wallet missing");
        return country;
    }
    [[nodiscard]] CountryFiscalView country_fiscal(
            const std::string& country)const {
        const auto& treasury=country_treasury(country);
        Money due=0;
        for(const auto& [key,amount]:sovereign_coupon_due_)
            if(sovereign_bonds_.at(key.first).country==country)
                due=phaseb::add_checked(due,amount);
        return {country,treasury,cash(treasury),
                outstanding_loan(treasury),income_tax_ppm(country),
                sovereign_bond_principal(country),due};
    }
    void spend_country_budget(const std::string& country,
                              const std::string& recipient,Money amount) {
        require_stage({5,7,8});
        pay(country_treasury(country),recipient,amount);
        events_.push_back("country-budget-paid:"+country);
    }
    void borrow_country_budget_from_bank(const std::string& country,Money amount) {
        require_stage({1});
        lend(country_treasury(country),amount);
        events_.push_back("country-bank-credit:"+country);
    }
    void propose_tax_law(const std::string& country, const std::string& proposer,
                         const std::vector<std::string>& supporting_parties, int ppm) {
        require_stage({9});
        ensure(ppm >= 0 && ppm <= 1'000'000, "invalid tax rate");
        const auto& g = governments_.at(country);
        const auto& result=legislature_for(country);
        ensure(g.prime_minister == proposer && result.total_seats == g.chamber_seats,
               "unauthorized tax bill or missing national legislature");
        ensure(coalition_seats(supporting_parties,result) >= g.quorum &&
               !has_pending_country_tax(country),
               "insufficient votes or country tax law already scheduled");
        scheduled_country_income_tax_[month()+1][country]=ppm;
        events_.push_back("constitutional-tax-law:"+country);
    }
    void appoint_cabinet(const std::string& country, const std::string& head,
                        std::string minister, const std::vector<std::string>& supporting_parties) {
        require_stage({9});
        const auto& g=governments_.at(country);
        const auto& result=legislature_for(country);
        ensure(g.head_of_state == head && !minister.empty() &&
               result.total_seats == g.chamber_seats &&
               coalition_seats(supporting_parties,result) >= g.quorum,
               "cabinet lacks appointment authority or national majority");
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
    [[nodiscard]] People unit_slice_strength(const std::string& unit,Id slice) const {
        auto owner=units_.find(unit);
        if(owner==units_.end())return 0;
        auto assigned=owner->second.find(slice);
        return assigned==owner->second.end()?0:assigned->second;
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
                people_.age_months_slice(id) >= static_cast<std::int64_t>(voting_age_years_) * 12)
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
        if(phase_d_reservation_mode_)
            resources_.assert_cash_available(from,amount,cash(from));
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
        if(phase_d_reservation_mode_)
            resources_.assert_cash_available(payer,gross,cash(payer));
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
    // Interest is a claim, never a new cash deposit or trade-credit draw.
    // It accrues on real outstanding loan PRINCIPAL at ACT/365 (including
    // leap-year days); integer fractional micro-units are carried per debtor.
    void phase_time_accrue_and_settle_interest() {
        require_stage({8});
        if(bank_interest_annual_ppm_<=0)return;
        constexpr Money denominator=365'000'000;
        if(!money_.has_account("BANK:interest_receivable"))
            money_.add_account("BANK:interest_receivable","BANK",
                               AccountType::asset);
        if(!money_.has_account("BANK:interest_revenue"))
            money_.add_account("BANK:interest_revenue","BANK",
                               AccountType::revenue);
        for(const auto& debtor:actors_) {
            const Money principal=outstanding_loan(debtor);
            if(principal<=0)continue;
            const Money carry=bank_interest_fraction_[debtor];
            ensure(carry>=0&&carry<denominator,"invalid interest carry");
            const Money high=phaseb::mul_checked(
                principal/denominator,bank_interest_annual_ppm_);
            const Money low=phaseb::add_checked(
                phaseb::mul_checked(principal%denominator,
                                    bank_interest_annual_ppm_),carry);
            const Money interest=phaseb::add_checked(high,low/denominator);
            bank_interest_fraction_[debtor]=low%denominator;
            if(interest<=0)continue;
            const auto account_id=account(debtor,"interest_payable");
            if(!money_.has_account(account_id))
                money_.add_account(account_id,debtor,AccountType::liability);
            money_.post({unique("bank-interest-accrual"),
                "interest receivable from actual principal",{
                {account(debtor,"expense"),interest,0},
                {account_id,0,interest},
                {"BANK:interest_receivable",interest,0},
                {"BANK:interest_revenue",0,interest}}});
            events_.push_back("daily-bank-interest-accrued");
        }
        for(const auto& debtor:actors_) {
            const auto payable=account(debtor,"interest_payable");
            if(!money_.has_account(payable))continue;
            const Money due=-money_.balance(payable);
            if(due<=0)continue;
            const Money liquid=std::max<Money>(
                0,phase_d_cash_available(debtor));
            const Money paid=std::min(due,liquid);
            if(paid<=0)continue;
            money_.post({unique("bank-interest-payment"),
                "pay recognized loan interest from existing deposit",{
                {payable,paid,0},
                {account(debtor,"cash"),0,paid},
                {"BANK:deposit",paid,0},
                {"BANK:interest_receivable",0,paid}}});
            events_.push_back("daily-bank-interest-paid");
        }
    }
    [[nodiscard]] bool firm_bankrupt(const std::string& firm)const {
        return bankrupt_firms_.contains(firm);
    }
    [[nodiscard]] Money outstanding_worker_wages(const std::string& employer)const {
        Money due=0;
        for(const auto& [id,claim]:wage_arrears_) {
            (void)id;
            if(claim.employer==employer)
                due=phaseb::add_checked(due,claim.outstanding);
        }
        return due;
    }
    [[nodiscard]] const std::map<std::string,WageArrearsClaim>&
            wage_arrears()const{return wage_arrears_;}
    [[nodiscard]] const std::map<std::string,std::int64_t>&
            bankrupt_firms()const{return bankrupt_firms_;}

    // Accrued salaries are legal employer liabilities and employee receivables,
    // not fabricated cash. The existing C FinanceLedger holds all balances.
    void phase_time_accrue_contract_wage(const std::string& claim_id,
                                          const std::string& employer,
                                          const std::string& worker,
                                          const std::string& treasury,
                                          const std::string& country,
                                          Money gross,std::int64_t today) {
        require_stage({5});
        ensure(!claim_id.empty()&&today>=0&&gross>0&&
               actors_.contains(employer)&&actors_.contains(worker)&&
               actors_.contains(treasury)&&employer!=worker&&
               worker!=treasury&&employer!=treasury&&
               !bankrupt_firms_.contains(employer)&&
               !wage_arrears_.contains(claim_id),
               "invalid or duplicate wage debt");
        if(!country.empty()&&governments_.contains(country))
            ensure(country_treasury(country)==treasury,
                   "wage debt foreign treasury");
        const auto employer_payable=account(employer,"wage_payable");
        const auto worker_receivable=account(worker,"wage_receivable");
        if(!money_.has_account(employer_payable))
            money_.add_account(employer_payable,employer,AccountType::liability);
        if(!money_.has_account(worker_receivable))
            money_.add_account(worker_receivable,worker,AccountType::asset);
        money_.post({unique("wage-claim"),
            "earned wages become an employer debt",{
            {account(employer,"expense"),gross,0},
            {employer_payable,0,gross},
            {worker_receivable,gross,0},
            {account(worker,"revenue"),0,gross}}});
        wage_arrears_.emplace(claim_id,WageArrearsClaim{
            claim_id,employer,worker,treasury,country,gross,gross,today});
        events_.push_back("earned-daily-wages-owed");
    }
    void phase_time_settle_wage_arrears() {
        require_stage({5});
        for(auto it=wage_arrears_.begin();it!=wage_arrears_.end();) {
            auto& claim=it->second;
            const Money free=std::max<Money>(0,
                phase_d_cash_available(claim.employer));
            const Money paid=std::min(free,claim.outstanding);
            if(paid<=0){++it;continue;}
            const int tax=claim.country.empty()?income_tax_ppm_:
                (governments_.contains(claim.country)?
                    income_tax_ppm(claim.country):income_tax_ppm_);
            const Money withholding=positive_product(paid,tax)/1'000'000;
            const Money net=paid-withholding;
            std::vector<Posting> journal{
                {account(claim.employer,"wage_payable"),paid,0},
                {account(claim.employer,"cash"),0,paid},
                {account(claim.worker,"wage_receivable"),0,paid}};
            if(net)journal.push_back({account(claim.worker,"cash"),net,0});
            if(withholding) {
                journal.push_back({account(claim.worker,"expense"),withholding,0});
                journal.push_back({account(claim.treasury,"cash"),withholding,0});
                journal.push_back({account(claim.treasury,"revenue"),0,withholding});
            }
            money_.post({unique("wage-claim-paid"),
                         "settlement of previously earned wages",journal});
            claim.outstanding-=paid;
            events_.push_back("earned-wage-arrears-paid");
            if(claim.outstanding==0)it=wage_arrears_.erase(it);
            else ++it;
        }
    }
    // A firm is insolvent only after actual salary debt remains unpaid for at
    // least 30 real days. Its contracts end; assets, inventory and creditor
    // claims remain in C, rather than being erased or assigned fictional value.
    void phase_time_resolve_firm_insolvency(std::int64_t today) {
        require_stage({8});
        ensure(today>=0,"invalid insolvency day");
        for(const auto& [id,claim]:wage_arrears_) {
            (void)id;
            if(bankrupt_firms_.contains(claim.employer) ||
               claim.issued_day>today || today-claim.issued_day<30)
                continue;
            if(outstanding_worker_wages(claim.employer)<=
               cash(claim.employer))continue;
            bankrupt_firms_[claim.employer]=today;
            std::vector<Id> layoffs;
            for(const auto& [slice,job]:people_.jobs())
                if(job.employer==claim.employer)layoffs.push_back(slice);
            for(Id slice:layoffs)
                people_.release(slice,people_.employed(slice));
            events_.push_back("firm-insolvency-creditor-claims-preserved");
        }
    }
    void lend(const std::string& borrower, Money amount) {
        require_stage({1});
        ensure(amount > 0 && actors_.contains(borrower) && !bank_insolvent_,
               "bank insolvent or borrower invalid");
        const Money prior=outstanding_loan(borrower);
        money_.post({unique("loan"), "bank loan creates matching deposit", {
            {"BANK:loan", amount, 0}, {"BANK:deposit", 0, amount},
            {account(borrower, "cash"), amount, 0}, {account(borrower, "debt"), 0, amount}}});
        bank_principal_by_borrower_[borrower]=phaseb::add_checked(prior,amount);
        events_.push_back("loan");
    }
    void forgive_defaulted_loan(const std::string& borrower, Money amount) {
        require_stage({8});
        ensure(actors_.contains(borrower)&&amount>0 &&
               outstanding_loan(borrower)>=amount &&
               money_.balance("BANK:loan")>=amount,
               "invalid defaulted loan debt forgiveness");
        const Money bank_prior=outstanding_loan(borrower);
        money_.post({unique("loan-loss"),"bank recognizes loss, borrower released from debt",{
            {"BANK:loan_loss",amount,0},{"BANK:loan",0,amount},
            {account(borrower,"debt"),amount,0},
            {account(borrower,"revenue"),0,amount}}});
        bank_principal_by_borrower_[borrower]=bank_prior-amount;
        total_bad_debt_=phaseb::add_checked(total_bad_debt_,amount);
        const Money loans=money_.balance("BANK:loan");
        const Money deposits=-money_.balance("BANK:deposit");
        const Money reserves=money_.balance("BANK:reserves");
        bank_insolvent_ = (loans+reserves < deposits);
        events_.push_back("defaulted-bank-asset-recognition");
    }
    [[nodiscard]] Money bank_losses() const { return total_bad_debt_; }
    [[nodiscard]] bool bank_insolvent() const { return bank_insolvent_; }
    [[nodiscard]] const std::map<std::string,People>&
            regional_food_grievances()const {
        return regional_food_grievance_;
    }
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
    [[nodiscard]] Money outstanding_loan(const std::string& borrower) const {
        ensure(actors_.contains(borrower),"unknown bank-debt obligor");
        auto bank=bank_principal_by_borrower_.find(borrower);
        if(bank!=bank_principal_by_borrower_.end())return bank->second;
        // Pre-upgrade snapshots lack per-obligor bank loan attribution.
        // Preserve the old behavior for historical stand-alone saves.
        const Money debt=money_.balance(account(borrower,"debt"));
        ensure(debt<=0&&debt>std::numeric_limits<Money>::min(),
               "invalid financial liability balance");
        return -debt;
    }
    void repay(const std::string& borrower, Money amount) {
        require_stage({8});
        if(phase_d_reservation_mode_)
            resources_.assert_cash_available(borrower,amount,cash(borrower));
        ensure(amount > 0 && actors_.contains(borrower) && cash(borrower) >= amount &&
               outstanding_loan(borrower)>=amount &&
               money_.balance("BANK:loan")>=amount,"invalid loan repayment");
        const Money old_principal=outstanding_loan(borrower);
        money_.post({unique("repay"), "loan principal retirement", {
            {account(borrower, "debt"), amount, 0}, {account(borrower, "cash"), 0, amount},
            {"BANK:deposit", amount, 0}, {"BANK:loan", 0, amount}}});
        bank_principal_by_borrower_[borrower]=old_principal-amount;
        events_.push_back("repay");
    }
    void hire(Id slice, People count, const std::string& employer, Money wage) {
        require_stage({2});
        ensure(actors_.contains(employer)&&!firm_bankrupt(employer),
               "unknown or insolvent employer");
        people_.hire(slice, count, employer, "worker", wage, phaseb::JobRequirements{});
        events_.push_back("hire");
    }
    void produce(const std::string& firm, const std::string& region,
                 const std::string& input, const std::string& output,
                 Quantity qty, People workers_required) {
        require_stage({4});
        ensure(qty > 0 && workers_required > 0 && workers_required <= employed_by(firm),
               "insufficient production labor");
        if(phase_d_reservation_mode_) {
            resources_.use_labor(firm,workers_required,employed_by(firm));
            resources_.assert_stage4_available({firm,region,input},qty,stock(firm,region,input));
        }
        ensure(stock(firm, region, input) >= qty, "missing production input");
        goods_.consume({firm, region, input}, qty);
        goods_.produce({firm, region, output}, qty);
        if(phase_d_reservation_mode_)
            resources_.record_stage4_output({firm,region,output},qty);
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
        if(phase_d_reservation_mode_) {
            resources_.assert_goods_available({seller,region,good},qty,stock(seller,region,good));
            resources_.assert_cash_available(buyer,cost,cash(buyer));
        }
        ensure(cash(buyer) >= cost, "buyer cannot pay");
        goods_.transfer({seller, region, good}, {buyer, region, good}, qty);
        pay(buyer, seller, cost);
        events_.push_back("trade");
    }
    void consume(const std::string& owner, const std::string& region,
                 const std::string& good, Quantity qty) {
        require_stage({7, 10});
        if(phase_d_reservation_mode_)
            resources_.assert_goods_available({owner,region,good},qty,stock(owner,region,good));
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
        if(phase_d_reservation_mode_)
            resources_.assert_goods_available({seller,origin,good},quantity,stock(seller,origin,good));
        const auto key = InventoryKey{"TRANSIT:" + id, "in_transit", good};
        goods_.transfer({seller, origin, good}, key, quantity);
        shipments_.emplace(id, Shipment{id, std::move(seller), std::move(buyer),
                    std::move(origin), std::move(destination), std::move(good),
                    quantity, loss, month() + travel_months, false});
        events_.push_back("dispatch");
    }
    [[nodiscard]] const Shipment& phase_e_shipment(const std::string& id)const {
        return shipments_.at(id);
    }
    [[nodiscard]] bool conscription_enabled() const { return conscription_enabled_; }
    [[nodiscard]] int income_tax_ppm() const { return income_tax_ppm_; } // legacy only
    [[nodiscard]] int income_tax_ppm(const std::string& country)const {
        ensure(governments_.contains(country),"unknown tax jurisdiction");
        const auto found=country_income_tax_.find(country);
        return found==country_income_tax_.end()?income_tax_ppm_:found->second;
    }
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
    void wages_under_law(const std::string& payer,const std::string& worker,
                         const std::string& treasury,Money gross,
                         const std::string& country="") {
        require_stage({5});
        ensure(gross>0,"invalid gross wage");
        std::string authority=country;
        if(authority.empty()&&!governments_.empty()) {
            if(governments_.contains(treasury))authority=treasury;
            else if(governments_.size()==1)
                authority=governments_.begin()->first; // historical fixture
            else throw std::invalid_argument("ambiguous national tax authority");
        }
        const int rate=authority.empty()?income_tax_ppm_:
                        income_tax_ppm(authority);
        if(!authority.empty()) {
            auto bound=country_treasuries_.find(authority);
            if(bound!=country_treasuries_.end())
                ensure(bound->second==treasury,"foreign treasury withholding");
            else if(governments_.size()>1)
                ensure(treasury==authority,"national treasury binding required");
        }
        const auto withholding=positive_product(gross,rate)/1'000'000;
        wages(payer,worker,treasury,gross,withholding);
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
    // Day-native E/C country elections use a separate record per country.
    // Valid ballots are backed by real eligible citizens in that sovereign
    // jurisdiction; a different country's legislature cannot authorize a
    // cabinet, treaty funding or tax decision.
    void hold_country_election(const std::string& country,
            People registered,People invalid,
            const std::map<std::string,People>& votes) {
        require_stage({9});
        const auto& g=governments_.at(country);
        ensure(!country_election_months_.contains(country)||
               country_election_months_.at(country)!=month(),
               "duplicate national election in calendar month");
        People eligible=0;
        for(const auto& [id,row]:people_.slices()) {
            (void)id;
            const auto& demo=people_.demographic(row.key.demographic);
            const auto sovereign=sovereignty_.find(demo.region);
            if(sovereign==sovereignty_.end()||sovereign->second!=country||
               demo.citizenship!="citizen"||
               demo.service==phaseb::Service::active||
               demo.service==phaseb::Service::pow||
               demo.service==phaseb::Service::missing||
               people_.age_months_slice(slice)<
                   static_cast<std::int64_t>(voting_age_years_)*12)
                continue;
            eligible=phaseb::add_checked(eligible,row.count);
        }
        ensure(registered>0&&registered<=eligible&&
               invalid>=0&&invalid<=registered&&
               g.chamber_seats>0&&!votes.empty(),
               "national election has invalid eligible-voter scope");
        People valid=0;
        for(const auto& [party,n]:votes) {
            ensure(!party.empty()&&n>=0,"invalid national party ballot");
            valid=phaseb::add_checked(valid,n);
        }
        ensure(valid>0&&valid<=registered-invalid,
               "national ballots exceed real voter registration");
        ElectionResult result;
        result.eligible=eligible;
        result.registered=registered;
        result.cast=phaseb::add_checked(valid,invalid);
        result.valid=valid;
        result.invalid=invalid;
        result.total_seats=g.chamber_seats;
        result.votes=votes;
        for(const auto& [party,n]:votes){(void)n;result.seats.emplace(party,0);}
        // Integer D'Hondt allocation with portable exact-fraction ties.
        for(People i=0;i<result.total_seats;++i) {
            std::string chosen;
            for(const auto& [party,n]:votes) {
                if(chosen.empty()){chosen=party;continue;}
                const int order=compare_nonnegative_ratios(
                     static_cast<std::uint64_t>(n),
                     static_cast<std::uint64_t>(result.seats[party]+1),
                     static_cast<std::uint64_t>(votes.at(chosen)),
                     static_cast<std::uint64_t>(result.seats[chosen]+1));
                if(order>0||(order==0&&party<chosen))chosen=party;
            }
            ++result.seats[chosen];
        }
        country_elections_[country]=std::move(result);
        country_election_months_[country]=month();
        events_.push_back("national-election:"+country);
    }
    // Actual epidemic shock: death plus a non-overlapping illness allocation
    // through the same Phase B population source of truth. Applied in stage 10.
    void epidemic(Id source,People newly_ill,People deaths) {
        require_stage({10});
        ensure(people_.has_slice(source) && newly_ill>=0 && deaths>=0 &&
               newly_ill+deaths>0 &&
               newly_ill+deaths<=people_.slice(source).count,
               "epidemic exceeds actual exposed population");
        const auto& before=people_.slice(source);
        const auto current=people_.demographic(before.key.demographic);
        ensure(!people_.health(current.health).ill,"already sick cohort");
        // The source slice can have an existing job contract. Use the
        // explicitly uncontracted subset for this simple epidemic fixture.
        const People available=before.count-people_.employed(source);
        ensure(newly_ill+deaths<=available,"cannot silently terminate unknown labor allocations");
        if(deaths) people_.death(source,deaths,0,"epidemic");
        if(newly_ill){
            auto hp=people_.health(current.health);
            hp.ill=true;
            (void)people_.change_health(source,newly_ill,hp,phaseb::EventKind::illness,0);
        }
        events_.push_back("verified-epidemic-population-shock");
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
        if(loss.ammunition && phase_d_reservation_mode_)
            resources_.assert_goods_available({unit,region,"ammunition"},loss.ammunition,
                                              stock(unit,region,"ammunition"));
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
    // Phase E single-authority adapter: sovereignty lives only in Phase C.
    // Called by the spatial staged month during stage 0; Phase E never stores
    // an independent writable country-per-province copy.
    void phase_e_transfer_sovereignty(const std::string& region,
                                      const std::string& sovereign) {
        require_stage({0});
        ensure(sovereignty_.contains(region) && !sovereign.empty(),
               "phase E invalid territory or sovereign");
        require_typed(typed_id(EntityKind::region,region), EntityKind::region);
        sovereignty_[region]=sovereign;
        events_.push_back("phase-e-sovereignty-transfer");
    }

    // Called only during Phase E stage-0 atomic territorial independence.
    // Country / government identity is distinct from province ownership,
    // and does not create money, POPs, state debt or company titles.
    void phase_e_found_government(const std::string& country,
                                  const std::string& head,
                                  const std::string& prime,
                                  People seats,People quorum) {
        require_stage({0});
        ensure(!country.empty()&&!head.empty()&&!prime.empty()&&
               seats>0&&quorum>0&&quorum<=seats&&
               !governments_.contains(country),"phase E invalid new country");
        bool has_sovereignty=false;
        for(const auto& [region,sovereign]:sovereignty_) {
            (void)region;
            if(sovereign==country) {has_sovereignty=true;break;}
        }
        ensure(has_sovereignty,"phase E government requires actual new territory");
        governments_.emplace(country,GovernmentState{
            country,head,prime,seats,quorum,1
        });
        events_.push_back("phase-e-country-government-founded");
    }

    void occupy(const std::string& region, const std::string& occupier) {
        require_stage({10});
        ensure(sovereignty_.contains(region) && !occupier.empty(), "invalid occupation");
        require_typed(typed_id(EntityKind::region,region),EntityKind::region);
        military_control_[region] = occupier;
        events_.push_back("military-control");
    }

    // Actual dated POP events enter the sole Phase B population ledger.
    // Birth month remains the legacy cohort classification until the daily
    // birth-date cohort contract is upgraded; event day is journaled outside.
    void phase_time_birth(Id mother, People babies) {
        require_stage({10});
        ensure(babies>0&&people_.has_slice(mother),"TIME_POP_INVALID_MOTHER");
        const auto& source=people_.slice(mother);
        const auto demographic=people_.demographic(source.key.demographic);
        const auto age=people_.age_months_slice(mother);
        ensure(demographic.sex==phaseb::Sex::female &&
               age>=18*12 && age<45*12 &&
               demographic.service==phaseb::Service::civilian,
               "TIME_POP_INELIGIBLE_MOTHER");
        auto economic=people_.economic(source.key.economic);
        economic.profession="not_assigned";
        economic.livelihood="inactive";
        economic.labor_class="ineligible";
        economic.education=people_.default_education();
        const auto baby_economic=people_.economic_profile(economic);
        const People girls=babies/2;
        if(girls)
            people_.birth(source.key.household,demographic,baby_economic,
                          phaseb::Sex::female,girls);
        if(babies-girls)
            people_.birth(source.key.household,demographic,baby_economic,
                          phaseb::Sex::male,babies-girls);
        events_.push_back("daily-pop-real-birth");
    }
    void phase_time_death(Id source, People count, People employed_affected=0) {
        require_stage({10});
        ensure(count>0&&employed_affected>=0&&people_.has_slice(source),
               "TIME_POP_INVALID_DEATH");
        const auto& row=people_.slice(source);
        const auto service=people_.demographic(row.key.demographic).service;
        ensure(service!=phaseb::Service::active &&
               service!=phaseb::Service::pow,
               "TIME_POP_ARMY_CASUALTY_REQUIRES_MILITARY_EVENT");
        people_.death(source,count,employed_affected,"daily-natural-event");
        events_.push_back("daily-pop-real-death");
    }
    void phase_time_dispatch(const std::string& id,const std::string& sender,
                             const std::string& origin,const std::string& good,
                             Quantity count) {
        require_stage({7});
        ensure(!id.empty()&&!sender.empty()&&!origin.empty()&&!good.empty()&&
               count>0&&stock(sender,origin,good)>=count,
               "TIME_LOGISTICS_UNFUNDED_DISPATCH");
        if(phase_d_reservation_mode_)
            resources_.assert_goods_available({sender,origin,good},count,
                                               stock(sender,origin,good));
        goods_.transfer({sender,origin,good},
                        {"TRANSIT:DAY:"+id,"in_transit",good},count);
        events_.push_back("daily-physical-cargo-dispatched");
    }
    void phase_time_arrive(const std::string& id,const std::string& recipient,
                           const std::string& destination,
                           const std::string& good,Quantity quantity,
                           Quantity loss) {
        require_stage({0});
        ensure(!id.empty()&&!recipient.empty()&&!destination.empty()&&
               !good.empty()&&quantity>0&&loss>=0&&loss<=quantity&&
               stock("TRANSIT:DAY:"+id,"in_transit",good)==quantity,
               "TIME_LOGISTICS_INVALID_ARRIVAL");
        if(loss)goods_.consume({"TRANSIT:DAY:"+id,"in_transit",good},loss);
        if(quantity>loss)
            goods_.transfer({"TRANSIT:DAY:"+id,"in_transit",good},
                            {recipient,destination,good},quantity-loss);
        events_.push_back("daily-physical-cargo-arrived");
    }
    void phase_d_daily_finish_resources() {
        require_stage({11});
        resources_.clear_month();
        phase_d_reservation_mode_=false;
    }
    void phase_time_set_calendar_day(int day_of_month) {
        require_stage({0});
        ensure(day_of_month>=1&&day_of_month<=31,
               "invalid native day for POP birth/eligibility");
        people_.set_native_calendar_day(day_of_month);
    }
    // Calendar day executes in the caller's one-day staging transaction.
    // This is the only routine that applies autonomous POP risk on a date.
    [[nodiscard]] bool daily_auto_demography_enabled() const {
        return demography_enabled_;
    }
    void phase_time_auto_demography(int day,int days_in_month) {
        require_stage({10});
        if(!demography_enabled_)return;
        const auto before=people_.events().size();
        (void)phaseb::Engine{demographic_rules_}.advance_day(
            people_,day,days_in_month);
        sync_service_transitions(before);
        events_.push_back("daily-auto-demography");
    }
    // Apply monthly policy expirations, inventories, tax/strike/clinic
    // availability on the true first DAY, not at the last day of that
    // month. The ordinary legacy month engine remains unchanged.
    void phase_time_begin_calendar_month() {
        require_stage({0});
        open_month();
    }

    // Daily real-ledger transaction, deliberately separate from legacy
    // monthly settlement. No open_month(), no autonomous market clearing,
    // no monthly POP mortality/aging/production, and no finish_month().
    // Call only whitelisted stage-valid C commands from the day coordinator.
    AdvanceReport advance_daily(const Plan& plan,
                                bool final_calendar_day=false) {
        IntegratedWorld staged = *this;
        const auto opening_month = month();
        for (int n=0; n<12; ++n) {
            staged.stage_=n;
            for (const auto& command:plan[static_cast<std::size_t>(n)])
                command(staged);
            if(n==8)staged.phase_time_accrue_and_settle_interest();
            if(n==10&&final_calendar_day) {
                // Update month-based age classification after today's real
                // population events; never rerun month-sized risk rates.
                staged.strike_active_.clear();
                staged.people_.finish_month();
                staged.people_.release_ineligible();
            }
            if(n==11&&final_calendar_day) {
                staged.election_this_month_=false;
            }
            // Real daily events already enforce local preconditions. One
            // full invariant pass at Stage 11 is sufficient for the single
            // atomic day commit; avoid scanning every POP/financial account
            // twelve times per calendar day.
            if(n==11)staged.validate();
        }
        staged.stage_=-1;
        ensure(staged.month()==opening_month+(final_calendar_day?1:0),
               "TIME_ERR_NATIVE_CALENDAR_ROLLOVER");
        ensure(staged.commit_id_<std::numeric_limits<std::uint64_t>::max(),
               "daily commit id exhausted");
        ++staged.commit_id_;
        *this=std::move(staged);
        return {opening_month,month(),people_.population(),commit_id_};
    }

    // Promote a canonical Phase B population registry in memory without
    // synthesizing extra people, money, employers or economic assets.
    static IntegratedWorld from_legacy_phase_b(phaseb::World pop) {
        IntegratedWorld result(pop.month());
        result.people_=std::move(pop);
        result.validate();
        return result;
    }

    AdvanceReport advance(const Plan& plan = {}) {
        IntegratedWorld stage = *this;
        const auto start = month();
        stage.election_this_month_ = false;
        for (int n = 0; n != 12; ++n) {
            stage.stage_ = n;
            if (n == 0) stage.open_month();
            if (n == 9) stage.collect_market_grievances();
            for (const auto& callback : plan[static_cast<std::size_t>(n)]) callback(stage);
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
            if (n == 11) {
                stage.reprice_markets();
            }
            stage.validate();
            if (n == 11) {
                stage.resources_.clear_month();
                stage.phase_d_reservation_mode_=false;
            }
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
        ensure(country_elections_.size()==country_election_months_.size()&&
               country_elections_.size()<=governments_.size(),
               "national election registry size mismatch");
        for(const auto& [country,result]:country_elections_) {
            const auto government=governments_.find(country);
            const auto when=country_election_months_.find(country);
            ensure(government!=governments_.end()&&
                   when!=country_election_months_.end()&&
                   when->second>=0&&when->second<=month()&&
                   result.total_seats==government->second.chamber_seats&&
                   result.registered>=0&&result.eligible>=result.registered&&
                   result.invalid>=0&&result.valid>0&&
                   result.cast==phaseb::add_checked(result.valid,result.invalid)&&
                   result.cast<=result.registered,
                   "national election not backed by government or voters");
            People votes=0,seats=0;
            ensure(result.votes.size()==result.seats.size()&&
                   !result.votes.empty(),"national election missing vote distribution");
            for(const auto& [party,n]:result.votes) {
                ensure(!party.empty()&&n>=0&&result.seats.contains(party)&&
                       result.seats.at(party)>=0,
                       "national election invalid party count");
                votes=phaseb::add_checked(votes,n);
                seats=phaseb::add_checked(seats,result.seats.at(party));
            }
            ensure(votes==result.valid&&seats==result.total_seats,
                   "national election votes/seats not conserved");
        }
        for (const auto& [id,g] : governments_) {
            ensure(id==g.country && !g.head_of_state.empty() && !g.prime_minister.empty() &&
                   g.chamber_seats>0 && g.quorum>0 && g.quorum<=g.chamber_seats,
                   "invalid government institution");
        }
        for(const auto& [country,ppm]:country_income_tax_)
            ensure(governments_.contains(country)&&ppm>=0&&ppm<=1'000'000,
                   "invalid country tax state");
        for(const auto& [effective,policies]:scheduled_country_income_tax_) {
            ensure(effective>0,"invalid scheduled country tax date");
            for(const auto& [country,ppm]:policies)
                ensure(governments_.contains(country)&&ppm>=0&&ppm<=1'000'000,
                       "invalid scheduled country tax rate");
        }
        std::set<std::string> assigned_treasuries;
        for(const auto& [country,wallet]:country_treasuries_)
            ensure(governments_.contains(country)&&actors_.contains(wallet)&&
                   assigned_treasuries.insert(wallet).second,
                   "invalid or shared country treasury");

        ensure(resources_.valid(), "shared resource reservation journal inconsistent");
        if(phase_d_reservation_mode_) {
            for(const auto& actor:actors_)
                ensure(resources_.cash_held(actor)<=cash(actor),
                       "reserved cash exceeds realized available balance");
        }
        ensure(bank_interest_annual_ppm_>=0&&bank_interest_annual_ppm_<=1'000'000,
               "invalid ACT/365 bank interest rate");
        for(const auto& [borrower,carry]:bank_interest_fraction_)
            ensure(actors_.contains(borrower)&&carry>=0&&carry<365'000'000,
                   "invalid bank interest carry");
        if(money_.has_account("BANK:interest_receivable")) {
            Money receivables=0;
            for(const auto& actor:actors_) {
                const auto id=account(actor,"interest_payable");
                if(!money_.has_account(id))continue;
                const Money owed=-money_.balance(id);
                ensure(owed>=0,"negative recognized interest payable");
                receivables=phaseb::add_checked(receivables,owed);
            }
            ensure(receivables==money_.balance("BANK:interest_receivable"),
                   "bank and borrower interest balances diverged");
        }
        std::map<std::string,Money> employer_wage_due,worker_wage_receivable;
        for(const auto& [debtor,principal]:bank_principal_by_borrower_)
            ensure(actors_.contains(debtor)&&principal>=0&&
                   principal<=-money_.balance(account(debtor,"debt")),
                   "bank loan principal exceeds actual finance debt");
        std::map<std::string,Money> coupons_payable,coupons_receivable;
        for(const auto& [id,bond]:sovereign_bonds_) {
            ensure(id==bond.id&&governments_.contains(bond.country)&&
                   bond.treasury==country_treasury(bond.country)&&
                   bond.original_face>0&&bond.coupon_ppm>=0&&
                   bond.coupon_ppm<=1'000'000&&bond.issue_day>=0&&
                   bond.maturity_day>bond.issue_day&&
                   tracked_assets_.contains("bond:"+id),
                   "invalid sovereign bond owner or issue");
            Money live_face=0;
            for(const auto& [owner,n]:ownership_.holders("bond:"+id)) {
                ensure(actors_.contains(owner)&&n>0,
                       "invalid live sovereign bond owner");
                live_face=phaseb::add_checked(live_face,n);
            }
            ensure(live_face<=bond.original_face,
                   "sovereign bond share principal exceeds issue");
        }
        for(const auto& [key,amount]:sovereign_coupon_due_) {
            ensure(sovereign_bonds_.contains(key.first)&&
                   actors_.contains(key.second)&&amount>0,
                   "invalid unpaid bond interest claim");
            const auto& treasury=sovereign_bonds_.at(key.first).treasury;
            coupons_payable[treasury]=phaseb::add_checked(
                coupons_payable[treasury],amount);
            coupons_receivable[key.second]=phaseb::add_checked(
                coupons_receivable[key.second],amount);
        }
        for(const auto& [key,carry]:sovereign_coupon_fraction_)
            ensure(sovereign_bonds_.contains(key.first)&&
                   actors_.contains(key.second)&&
                   carry>=0&&carry<365'000'000,
                   "invalid sovereign coupon fraction");
        for(const auto& [treasury,amount]:coupons_payable)
            ensure(money_.has_account(account(treasury,"bond_coupon_payable"))&&
                   -money_.balance(account(treasury,"bond_coupon_payable"))==
                     amount,"sovereign bond coupon liability not backed");
        for(const auto& [holder,amount]:coupons_receivable)
            ensure(money_.has_account(account(holder,"bond_coupon_receivable"))&&
                   money_.balance(account(holder,"bond_coupon_receivable"))==
                     amount,"sovereign bond coupon creditor not backed");
        for(const auto& [id,claim]:wage_arrears_) {
            ensure(id==claim.id&&claim.accrued>0&&claim.outstanding>0&&
                   claim.outstanding<=claim.accrued&&claim.issued_day>=0&&
                   actors_.contains(claim.employer)&&
                   actors_.contains(claim.worker)&&
                   actors_.contains(claim.treasury),
                   "invalid earned wage creditor record");
            employer_wage_due[claim.employer]=phaseb::add_checked(
                employer_wage_due[claim.employer],claim.outstanding);
            worker_wage_receivable[claim.worker]=phaseb::add_checked(
                worker_wage_receivable[claim.worker],claim.outstanding);
        }
        for(const auto& [employer,total]:employer_wage_due)
            ensure(money_.has_account(account(employer,"wage_payable"))&&
                   -money_.balance(account(employer,"wage_payable"))==total,
                   "employer wage payables not backed by contracts");
        for(const auto& [worker,total]:worker_wage_receivable)
            ensure(money_.has_account(account(worker,"wage_receivable"))&&
                   money_.balance(account(worker,"wage_receivable"))==total,
                   "household earned wage receivables not backed by contracts");
        for(const auto& [firm,day]:bankrupt_firms_)
            ensure(actors_.contains(firm)&&day>=0,
                   "orphaned bankrupt firm");
        for(const auto& [unit,deploy]:army_deployments_)
            ensure(units_.contains(unit)&&deploy.unit==unit&&
                   governments_.contains(deploy.country)&&
                   sovereignty_.contains(deploy.region),
                   "orphaned real national army deployment");
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
    // Reservations are transient for one staged month; never another money/POP stock.
    SharedResourceReservations resources_;
    bool phase_d_reservation_mode_{};
    InventoryLedger goods_;
    OwnershipLedger ownership_;
    std::set<std::string> actors_;
    std::set<std::string> processed_commands_;
    std::map<std::string, std::map<Id, People>> units_;
    std::map<std::string,ArmyDeployment> army_deployments_;
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
    std::map<std::string, DividendDue> dividends_;
    bool healthcare_right_{};
    std::map<std::int64_t,bool> scheduled_public_healthcare_;
    std::map<std::string,PublicInstitution> public_institutions_;
    std::map<std::string,Money> paid_staff_this_month_;
    Money total_bad_debt_{};
    bool bank_insolvent_{};
    std::map<std::string,Money> bank_principal_by_borrower_;
    std::map<std::string,SovereignBond> sovereign_bonds_;
    std::map<std::pair<std::string,std::string>,Money> sovereign_coupon_due_;
    std::map<std::pair<std::string,std::string>,Money> sovereign_coupon_fraction_;
    std::map<std::string,WageArrearsClaim> wage_arrears_;
    std::map<std::string,std::int64_t> bankrupt_firms_;
    std::map<std::string,People> regional_food_grievance_;
    int bank_interest_annual_ppm_{}; // zero until historically configured
    std::map<std::string,Money> bank_interest_fraction_;
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
    std::map<std::int64_t, int> scheduled_income_tax_; // legacy-only world
    int income_tax_ppm_{100'000};
    // Country-scoped tax policies and treasury identities only; country cash,
    // loans and spending remain authoritative in the C financial ledger.
    std::map<std::string,int> country_income_tax_;
    std::map<std::int64_t,std::map<std::string,int>> scheduled_country_income_tax_;
    std::map<std::string,std::string> country_treasuries_;
    bool conscription_enabled_{true};
    std::map<std::int64_t, int> scheduled_voting_age_;
    ElectionResult last_election_;
    // Append-only save extension; absent in authentic Phase C legacy payloads.
    std::map<std::string,ElectionResult> country_elections_;
    std::map<std::string,std::int64_t> country_election_months_;
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
    [[nodiscard]] const ElectionResult& legislature_for(
            const std::string& country) const {
        const auto it=country_elections_.find(country);
        if(it!=country_elections_.end())return it->second;
        // Legacy single-legislature fixtures retain their original meaning.
        // Once a modern country-specific election exists, other nations must
        // have their OWN election rather than reuse a neighbour's seats.
        ensure(country_elections_.empty(),
               "missing country-specific legislative election");
        return last_election_;
    }
    [[nodiscard]] People coalition_seats(
            const std::vector<std::string>& party_ids,
            const ElectionResult& legislature) const {
        std::set<std::string> unique;
        People total=0;
        for(const auto& party:party_ids) {
            ensure(unique.insert(party).second&&
                   legislature.seats.contains(party),
                   "duplicate or unknown national coalition party");
            total=phaseb::add_checked(total,legislature.seats.at(party));
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
        resources_.clear_month();
        phase_d_reservation_mode_=false;
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
        if (auto t=scheduled_income_tax_.find(month());t!=scheduled_income_tax_.end()) {
            // No constitutional country ever reads this global legacy change.
            ensure(governments_.empty(),"global income tax in multi-country world");
            income_tax_ppm_=t->second;
            scheduled_income_tax_.erase(t);
        }
        if(auto taxes=scheduled_country_income_tax_.find(month());
           taxes!=scheduled_country_income_tax_.end()) {
            for(const auto& [country,ppm]:taxes->second) {
                ensure(governments_.contains(country),
                       "country tax effective without government");
                country_income_tax_[country]=ppm;
            }
            scheduled_country_income_tax_.erase(taxes);
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
