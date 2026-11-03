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
        actors_.insert(who);
        for (const auto& [suffix, type] : std::array<std::pair<std::string, AccountType>, 5>{{
            {"cash", AccountType::asset}, {"capital", AccountType::equity},
            {"revenue", AccountType::revenue}, {"expense", AccountType::expense},
            {"debt", AccountType::liability}}}) {
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
        tracked_goods_.insert(good);
        goods_.initialize({std::move(holder), std::move(region), std::move(good)}, qty);
    }
    void seed_region(std::string region, std::string sovereign) {
        ensure(stage_ == -1 && commit_id_ == 0, "region seed after simulation start");
        ensure(!region.empty() && !sovereign.empty() && !sovereignty_.contains(region), "bad region");
        military_control_[region] = sovereign;
        sovereignty_[std::move(region)] = std::move(sovereign);
    }
    void seed_unit(std::string unit) {
        ensure(stage_ == -1 && commit_id_ == 0 && !unit.empty() && !units_.contains(unit), "bad unit");
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
        events_.push_back("wages");
    }
    void lend(const std::string& borrower, Money amount) {
        require_stage({1});
        ensure(amount > 0 && actors_.contains(borrower), "invalid loan");
        money_.post({unique("loan"), "bank loan creates matching deposit", {
            {"BANK:loan", amount, 0}, {"BANK:deposit", 0, amount},
            {account(borrower, "cash"), amount, 0}, {account(borrower, "debt"), 0, amount}}});
        events_.push_back("loan");
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
        scheduled_conscription_[month() + 1] = enabled;
        events_.push_back("conscription-law-scheduled");
    }
    void enact_income_tax(int ppm) {
        require_stage({9});
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
            if (n == 10) {
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
            stage.validate();
        }
        stage.stage_ = -1;
        ++stage.commit_id_;
        *this = std::move(stage);
        return {start, month(), people_.population(), commit_id_};
    }
    void validate() const {
        ensure(people_.validate(), "invalid population registry");
        for (const auto& [id,g] : governments_) {
            ensure(id==g.country && !g.head_of_state.empty() && !g.prime_minister.empty() &&
                   g.chamber_seats>0 && g.quorum>0 && g.quorum<=g.chamber_seats,
                   "invalid government institution");
        }
        ensure(money_.balanced(), "unbalanced financial ledger");
        Money deposits = 0;
        for (const auto& actor : actors_) {
            ensure(cash(actor) >= 0, "negative spendable cash");
            deposits = phaseb::add_checked(deposits, cash(actor));
        }
        ensure(-money_.balance("BANK:deposit") == deposits,
               "bank deposit liabilities do not match customer assets");
        for (const auto& g : tracked_goods_)
            ensure(goods_.conserved(g), "inconsistent commodity conservation");
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
    std::set<std::string> tracked_assets_;
    std::set<std::pair<std::string, std::string>> security_accounts_;
    struct DividendDue {
        std::string asset, payer, holder;
        Money amount{};
        std::int64_t due{};
    };
    std::map<std::string, DividendDue> dividends_;
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
            if (job.employer == who) n = phaseb::add_checked(n, job.count);
        }
        return n;
    }
    void open_month() {
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
