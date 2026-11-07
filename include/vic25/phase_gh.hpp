#pragma once
// VIC2.5 Phase G/H bounded C++ integration into the existing C/D/E world.
// G owns trade decisions and execution references; H owns diplomatic legal
// states. ALL money, goods, geography, manpower and physical shipments are
// mutated through Phase C/D/E, never through shadow H/G ledgers.
// This is a functional vertical slice, NOT the full 62+176 acceptance engine.

#include "vic25/phase_e.hpp"
#include "vic25/day_budget.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <queue>
#include <tuple>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace vic25::phasegh {
using Id = phasee::Id;
using Month = phasee::Month;
using Money = phasec::Money;
using Quantity = vic25::Quantity;

inline void require(bool p, const char* message) {
    if (!p) throw std::invalid_argument(message);
}
inline constexpr const char* g_version = "phase-g-cpp-bounded/0.1";
inline constexpr const char* h_version = "phase-h-cpp-bounded/0.1";

struct Route {
    std::string id, seller, buyer, good;
    Id origin{}, destination{};
    Quantity maximum_quantity{};
    Month travel_months{}; // cross-province shipment must be > 0
    Quantity arrival_loss{}; // confirmed physical accident input, not a risk forecast
};

struct TradeFill {
    std::string execution_id, route_id, seller_country, buyer_country;
    Month month{};
    Quantity quantity{};
    Money goods_value{}, duty_paid{};
    bool shipped{};
    Money freight_paid{};
    std::string contract_id, currency{"TEST"};
    std::vector<std::string> physical_path;
};
struct TransportEdge {
    std::string id, carrier;
    Id from{}, to{};
    Quantity monthly_capacity{};
    Money freight_per_unit{};
    Month transit_months{}; // historical standalone monthly input
    bool enabled{true};
    std::int32_t transit_days{}; // native game: verified days, zero = unknown
};
struct CustomsRecord {
    std::string execution_id, importer, treasury, good, policy_source;
    Month month{};
    Money taxable_value{}, assessed{}, paid{};
    Quantity cleared_quantity{};
};
struct TradeContract {
    std::string id, seller, buyer, good, currency{"TEST"};
    Id origin{}, destination{};
    Quantity contracted{}, fulfilled{}, monthly_limit{};
    Money unit_price{};
    Month effective{}, expires{};
    bool partial{true}, active{true};
};
struct CreditFacility {
    std::string id, borrower, pledge_region, pledge_good;
    Quantity pledged_goods{};
    Money limit{}, drawn{};
    Month effective{};
    bool active{true};
};
struct CargoInsurance {
    std::string id, shipment_id, insurer, beneficiary;
    Money premium{}, maximum_claim{}, claim_due{}, claim_paid{};
    Quantity insured_quantity{};
    bool premium_paid{}, claim_registered{};
};

struct PlannedTrade {
    std::string execution_id, route_id, seller, buyer, good, origin_region,
                destination_region, seller_country, buyer_country;
    Quantity quantity{};
    Month travel_months{};
    Money unit_price{}, total_cost{}, duty{}, freight{};
    std::string contract_id, settlement_currency{"TEST"};
    std::vector<std::string> path_edges;
    Quantity confirmed_arrival_loss{};
};

struct PathQuote {
    Money freight_per_unit{};
    Month travel_months{};
    Quantity capacity{};
    std::vector<std::string> edges;
};
struct DailyTradeTransit {
    std::string id, buyer, destination, good;
    Quantity quantity{}, loss{};
    std::int64_t arrival_day{};
};
struct TradeState {
    std::map<std::string, Route> routes;
    std::map<std::string, TransportEdge> transport_edges;
    std::map<Id,std::vector<std::string>> transport_adjacency;
    std::map<std::pair<Month,std::string>,Quantity> edge_usage;
    std::map<std::string,TradeContract> contracts;
    std::map<std::string,CreditFacility> credit;
    std::map<std::string,CargoInsurance> insurance;
    std::vector<CustomsRecord> customs;
    std::map<std::pair<std::string,std::string>,Money> fx_held;
    std::vector<PlannedTrade> pending;
    std::vector<TradeFill> fills;
    // Only timing/recipient metadata: physical inventory is always in C.
    std::map<std::string,DailyTradeTransit> daily_shipments;
    std::set<std::string> executed;
    std::uint64_t revision{};
};

// One F/G route oracle for both autonomous trade and autonomous physical
// freight. It uses the authoritative G edge ledger (including already-used
// throughput), not a second sidecar graph.
inline std::optional<PathQuote> quote_daily_freight_path(
        const TradeState& trade,Id from,Id to,Month month,
        std::int32_t day_of_month=0,std::int32_t days_in_month=0,
        const std::map<std::string,Quantity>* provisional_edges=nullptr) {
    if(from==to)return PathQuote{0,0,std::numeric_limits<Quantity>::max(),{}};
    if(trade.transport_edges.empty())return std::nullopt;
    using Item=std::tuple<Money,Month,Id>;
    std::priority_queue<Item,std::vector<Item>,std::greater<Item>> queue;
    std::map<Id,std::pair<Money,Month>> best;
    std::map<Id,std::pair<Id,std::string>> parent;
    best[from]={0,0};
    queue.push({0,0,from});
    while(!queue.empty()) {
        const auto [cost,travel,node]=queue.top();
        queue.pop();
        if(best.at(node)!=std::pair<Money,Month>{cost,travel})continue;
        if(node==to)break;
        auto it=trade.transport_adjacency.find(node);
        if(it==trade.transport_adjacency.end())continue;
        // World::seed_transport_edge inserts in deterministic ID order.
        for(const auto& edge_id:it->second) {
            const auto& edge=trade.transport_edges.at(edge_id);
            const Quantity committed=trade.edge_usage.contains({month,edge_id})?
                                     trade.edge_usage.at({month,edge_id}):0;
            const Quantity pending=(provisional_edges&&
                provisional_edges->contains(edge_id))?
                provisional_edges->at(edge_id):0;
            const Quantity used=phaseb::add_checked(committed,pending);
            const Quantity ceiling=day_of_month>0?
                daily_budget::cumulative(edge.monthly_capacity,
                                         day_of_month,days_in_month):
                edge.monthly_capacity;
            if(!edge.enabled || used>=ceiling)continue;
            require(edge.from==node && used>=0,
                    "G_ERR_LOGISTICS_INDEX_CORRUPT");
            const Money next_cost=phaseb::add_checked(cost,edge.freight_per_unit);
            const Month next_travel=phaseb::add_checked(travel,edge.transit_months);
            const std::pair<Money,Month> candidate{next_cost,next_travel};
            auto old=best.find(edge.to);
            if(old==best.end() || candidate<old->second) {
                best[edge.to]=candidate;
                parent[edge.to]={node,edge_id};
                queue.push({next_cost,next_travel,edge.to});
            }
        }
    }
    if(!best.contains(to))return std::nullopt;
    PathQuote result;
    result.freight_per_unit=best.at(to).first;
    result.travel_months=best.at(to).second;
    result.capacity=std::numeric_limits<Quantity>::max();
    for(Id p=to;p!=from;) {
        auto it=parent.find(p);
        require(it!=parent.end(),"G_ERR_LOGISTICS_PARENT_MISSING");
        const auto& edge=trade.transport_edges.at(it->second.second);
        const Quantity committed=trade.edge_usage.contains({month,edge.id})?
                                 trade.edge_usage.at({month,edge.id}):0;
        const Quantity pending=(provisional_edges&&
            provisional_edges->contains(edge.id))?
            provisional_edges->at(edge.id):0;
        const Quantity used=phaseb::add_checked(committed,pending);
        const Quantity ceiling=day_of_month>0?
            daily_budget::cumulative(edge.monthly_capacity,day_of_month,days_in_month):edge.monthly_capacity;
        require(used<ceiling,"G_ERR_LOGISTICS_CAPACITY");
        result.capacity=std::min(result.capacity,ceiling-used);
        result.edges.push_back(edge.id);
        p=it->second.first;
    }
    std::reverse(result.edges.begin(),result.edges.end());
    return result;
}

struct Tariff {
    std::string importer_country, good;
    std::int64_t ppm{};
};
enum class ClauseKind : std::uint8_t {
    tariff_exemption, trade_embargo, trade_quota, military_access,
    defense_guarantee, non_aggression, territorial_cession,
    financial_aid, reparations, protectorate, research_cooperation,
    cultural_exchange, ceasefire, military_withdrawal
};
struct TreatyClause {
    std::string id, obligor, beneficiary, good;
    ClauseKind kind{ClauseKind::tariff_exemption};
    Id province{}, receiving_admin{};
    Money amount{}, fulfilled{};
    Month effective{}, expires{-1};
    bool queued{};
};
enum class VoteChoice : std::uint8_t { yes, no, abstain };
struct OrganizationRule {
    std::size_t quorum{2}, yes_required{2};
    std::set<std::string> veto_members;
};
struct OrganizationVote {
    std::string decision_id, organization_id, subject;
    std::map<std::string,VoteChoice> ballots;
    bool adopted{};
    Month effective{};
};
struct CulturalExchange {
    std::string event_id, sponsor, beneficiary;
    Money funded{};
    Month month{};
};
struct MilitaryAssistance {
    std::string event_id, provider, receiver, good, shipment_id;
    Quantity quantity{};
    Money market_value{};
    Month month{};
};
struct CrisisPosition {
    std::string country, stance;
    bool conditional{};
    Month month{};
};
struct CrisisProposal {
    std::string id, proposer;
    Money compensation{};
    std::string payer, recipient;
    bool freeze_claim{};
    std::set<std::string> approvers;
    Month month{};
};
struct CrisisAdvice {
    bool can_mediate{}, has_underlying_dispute{};
    bool lawful_war_entry_supported{};
    Money observed_commercial_exposure{};
    std::string recommendation;
};
struct Treaty {
    std::string id, country_a, country_b, good;
    Month agreed_month{}, effective_month{};
    bool ratified_a{}, ratified_b{}, active{};
    std::set<std::string> parties, ratifications;
    std::vector<TreatyClause> clauses;
    bool pending_termination{};
    std::set<std::string> termination_approvals;
};
struct Organization {
    std::string id;
    std::set<std::string> members;
    Month effective_month{};
    Money dues_per_member{};
    bool active{};
    std::map<std::pair<Month,std::string>, Money> paid;
    OrganizationRule governance;
    std::map<std::string,OrganizationVote> decisions;
    std::map<Month,std::set<std::string>> pending_admissions;
    std::map<Month,std::set<std::string>> pending_exits;
};
struct Protectorate {
    std::string id, principal, dependent;
    Month effective_month{};
    bool active{}, exclusive_foreign_policy{};
};
enum class CrisisStatus : std::uint8_t {
    scheduled, active, mediation, settled, frozen,
    partly_settled, escalated, withdrawn
};
struct Crisis {
    std::string id, claimant, respondent, mediator;
    Id province{};
    Month effective_month{};
    CrisisStatus status{CrisisStatus::scheduled};
    std::map<std::string,CrisisPosition> positions;
    std::map<std::string,CrisisProposal> proposals;
    std::set<std::string> issues;
};
struct Recognition {
    std::string recognizer, recognized;
    Month effective_month{};
};
struct PendingTariff {
    Month effective{};
    std::string importer, good;
    std::int64_t ppm{};
};
// Native day-resolved bilateral agreement, owned by H. Unlike an
// inherited monthly Treaty, this uses explicit day timestamps and a real
// effective state. Never copy an old Treaty and guess its missing dates.
struct DailyTreaty {
    std::string id, country_a, country_b, good;
    std::int64_t proposed_day{}, effective_day{};
    bool approved_a{}, approved_b{}, active{};
};
struct DiplomacyState {
    std::map<std::pair<std::string,std::string>,Tariff> tariffs;
    std::vector<PendingTariff> scheduled_tariffs;
    std::map<std::string,Treaty> treaties;
    std::map<std::string,DailyTreaty> daily_treaties;
    std::map<std::string,Organization> organizations;
    std::map<std::string,Protectorate> protectorates;
    std::map<std::string,Crisis> crises;
    std::vector<Recognition> recognitions;
    std::set<std::string> reparation_links; // C is the monetary obligation owner.
    std::set<std::string> commands;
    std::vector<CulturalExchange> funded_exchanges;
    std::vector<MilitaryAssistance> military_assistance;
    std::map<std::tuple<std::string,std::string,std::string,Month>,Quantity> quota_used;
    std::uint64_t revision{};
};

struct InfluenceView {
    bool observed{};                 // false means unknown, NOT zero influence.
    Money bilateral_imports{};       // Only G's actually settled trade evidence.
    Money target_total_imports{};
    std::int64_t trade_dependency_ppm{};
    // Military/political/cultural dimensions remain unsupported, not zero.
    bool military_known{}, political_known{}, cultural_known{};
    Quantity military_goods_delivered{};
    Money military_material_value{}, cultural_funding{};
    std::uint64_t political_cooperation_actions{};
    std::string comparable_currency{"TEST"};
    bool foreign_currency_excluded{};
};

class Stage {
    friend class World;
    phasee::SpatialStage& spatial_;
    TradeState& trade_;
    DiplomacyState& diplomacy_;
    int phase_{};
    std::int32_t day_of_month_{};
    std::int32_t days_in_month_{};
    std::int64_t day_index_{-1};
    // Cumulative allowance: never regrant a full month of quota daily.
    Quantity day_budget(Quantity total,Quantity used)const {
        if(day_of_month_<=0)return total; // historical stand-alone month engine
        return daily_budget::remaining(total,used,day_of_month_,days_in_month_);
    }
    Quantity monthly_fill(const std::string& id,bool contract) {
        Quantity used=0;
        for(const auto& fill:trade_.fills) {
            if(fill.month!=phase_c().month())continue;
            if(contract?fill.contract_id==id:fill.route_id==id)
                used=phaseb::add_checked(used,fill.quantity);
        }
        return used;
    }
    void at(int expected) const {
        require(phase_ == expected, "GH_ERR_INVALID_STAGE");
    }
    static void leader(const phasec::IntegratedWorld& c,
                       const std::string& country, const std::string& person) {
        require(!country.empty() && !person.empty() &&
                c.government(country).prime_minister == person,
                "H_ERR_UNAUTHORIZED_GOVERNMENT");
    }
    [[nodiscard]] std::optional<PathQuote> quote_path(
        Id from, Id to, Month month) const;
    void plan_contracts();
    [[nodiscard]] Quantity lawful_trade_quantity(
        const std::string& seller_country,const std::string& buyer_country,
        const std::string& good,Quantity requested);
    void reserve_trade_quota(const std::string& seller_country,
                            const std::string& buyer_country,
                            const std::string& good,Quantity amount);
    void queue_package_effects_if_ratified(Treaty& treaty);
    void reserve_collateral();
    void register_arrived_losses();
    void pay_insurance_premiums();
    void settle_insurance_claims();
    void allow_foreign_policy(const std::string& country) const {
        for (const auto& [id, link] : diplomacy_.protectorates) {
            (void)id;
            require(!(link.active && link.dependent == country &&
                      link.exclusive_foreign_policy),
                    "H_ERR_DELEGATED_FOREIGN_POLICY");
        }
    }
    [[nodiscard]] bool exempt(const std::string& seller,
                              const std::string& buyer,
                              const std::string& good) {
        for(const auto& [id,daily]:diplomacy_.daily_treaties) {
            (void)id;
            if(daily.active && daily.good==good &&
               ((daily.country_a==seller&&daily.country_b==buyer) ||
                (daily.country_b==seller&&daily.country_a==buyer)))
                return true;
        }
        for (const auto& [id, treaty] : diplomacy_.treaties) {
            (void)id;
            if (!treaty.active) continue;
            const bool in_parties=(treaty.country_a==seller&&treaty.country_b==buyer)||
                                  (treaty.country_b==seller&&treaty.country_a==buyer);
            if(in_parties&&treaty.good==good)return true;
            for(const auto& clause:treaty.clauses)
                if(clause.kind==ClauseKind::tariff_exemption &&
                   clause.good==good &&
                   clause.effective<=phase_c().month() &&
                   (clause.expires<0||phase_c().month()<=clause.expires) &&
                   ((clause.obligor==seller&&clause.beneficiary==buyer)||
                    (clause.beneficiary==seller&&clause.obligor==buyer)))
                    return true;
        }
        return false;
    }
    [[nodiscard]] std::int64_t duty_rate(const std::string& seller,
                                          const std::string& buyer,
                                          const std::string& good) {
        if (seller == buyer || exempt(seller, buyer, good)) return 0;
        auto it = diplomacy_.tariffs.find({buyer, good});
        return it == diplomacy_.tariffs.end() ? 0 : it->second.ppm;
    }
public:
    Stage(phasee::SpatialStage& spatial, TradeState& trade,
          DiplomacyState& diplomacy, int phase,
          std::int32_t day=0,std::int32_t dim=0,std::int64_t ordinal=-1)
        : spatial_(spatial), trade_(trade), diplomacy_(diplomacy),
          phase_(phase), day_of_month_(day), days_in_month_(dim),
          day_index_(ordinal) {}
    [[nodiscard]] int phase() const { return phase_; }
    [[nodiscard]] phasee::SpatialStage& geography() { return spatial_; }
    [[nodiscard]] phased::Stage& phase_d() { return spatial_.phase_d(); }
    [[nodiscard]] phasec::IntegratedWorld& phase_c() { return spatial_.phase_c(); }
    [[nodiscard]] const DiplomacyState& diplomacy() const { return diplomacy_; }
    [[nodiscard]] const TradeState& commerce() const { return trade_; }

    // New daily H treaties use the same authenticated government identities
    // and one H-owned diplomacy registry. Terms are active from exact days,
    // not fabricated month timestamps.
    // Daily logistics settlement uses the authoritative G transport-edge
    // month-budget (shared with MonthClose) and the actual C inventory.
    // F/G daily physical arrival is a stage-0 event in the same authoritative
    // world transaction as the C inventory delivery. Insurance is paid only
    // when coverage was funded before loss; never fabricate a claim.
    void acknowledge_daily_cargo_arrival(const std::string& cargo_id,
                                         Quantity actual_quantity,
                                         Quantity confirmed_loss) {
        at(0);
        require(!cargo_id.empty()&&actual_quantity>0&&confirmed_loss>=0&&
                confirmed_loss<=actual_quantity&&
                trade_.executed.contains("DAY:"+cargo_id)&&
                trade_.executed.insert("ARRIVED:DAY:"+cargo_id).second,
                "F_ERR_UNVERIFIED_OR_DUPLICATE_DAILY_ARRIVAL");
        for(auto& [id,coverage]:trade_.insurance) {
            (void)id;
            if(coverage.shipment_id!=cargo_id||coverage.claim_registered)
                continue;
            require(actual_quantity>=coverage.insured_quantity,
                    "F_ERR_INSURANCE_EXCEEDS_SHIPMENT");
            coverage.claim_due=coverage.premium_paid?
                phaseb::mul_checked(coverage.maximum_claim,
                      std::min(confirmed_loss,coverage.insured_quantity))/
                      coverage.insured_quantity:0;
            coverage.claim_registered=true;
        }
        ++trade_.revision;
    }
    void dispatch_daily_cargo(const std::string& cargo_id,
                              const std::string& edge_id,
                              const std::string& sender,
                              const std::string& origin,
                              const std::string& destination,
                              const std::string& good,
                              Quantity quantity,
                              std::int32_t day_of_month,
                              std::int32_t days_in_month) {
        at(7);
        require(quantity>0&&days_in_month>=28&&days_in_month<=31 &&
                day_of_month>=1&&day_of_month<=days_in_month,
                "G_DAILY_INVALID_THROUGHPUT");
        const auto& edge=trade_.transport_edges.at(edge_id);
        const auto& geo=spatial_.spatial();
        const auto origin_id=geo.regions.at(origin);
        const auto destination_id=geo.regions.at(destination);
        require(edge.enabled&&
                ((edge.from==origin_id&&edge.to==destination_id) ||
                 (edge.to==origin_id&&edge.from==destination_id)),
                "G_DAILY_UNREACHABLE_DESTINATION");
        const auto key=std::make_pair(phase_c().month(),edge_id);
        const auto used=trade_.edge_usage[key];
        const auto capacity=daily_budget::cumulative(
            edge.monthly_capacity,day_of_month,days_in_month);
        require(used<=capacity&&quantity<=capacity-used,
                "G_DAILY_MONTHLY_CAPACITY_EXCEEDED");
        require(!cargo_id.empty()&&
                !trade_.executed.contains("DAY:"+cargo_id),
                "F_ERR_DUPLICATE_DAILY_DISPATCH");
        const Money freight=phaseb::mul_checked(quantity,edge.freight_per_unit);
        // Freight is a real monetary transaction, not a shadow transport
        // points balance. An own-carrier route needs no internal transfer.
        if(freight>0&&sender!=edge.carrier)
            phase_c().pay(sender,edge.carrier,freight);
        phase_c().phase_time_dispatch(cargo_id,sender,origin,good,quantity);
        trade_.executed.insert("DAY:"+cargo_id);
        trade_.edge_usage[key]=phaseb::add_checked(used,quantity);
        ++trade_.revision;
    }

    void propose_daily_treaty(const std::string& id,
                              const std::string& a,const std::string& b,
                              const std::string& good,const std::string& prime,
                              std::int64_t today,std::int64_t effective_day) {
        at(9);
        require(!id.empty()&&!a.empty()&&!b.empty()&&a!=b&&
                !good.empty()&&effective_day>today&&
                !diplomacy_.treaties.contains(id)&&
                !diplomacy_.daily_treaties.contains(id),
                "H_DAILY_INVALID_PROPOSAL");
        leader(phase_c(),a,prime);
        (void)phase_c().government(b);
        allow_foreign_policy(a);
        diplomacy_.daily_treaties.emplace(id,DailyTreaty{
            id,a,b,good,today,effective_day,true,false,false});
        ++diplomacy_.revision;
    }
    void ratify_daily_treaty(const std::string& id,
                             const std::string& country,
                             const std::string& prime) {
        at(9);
        auto& treaty=diplomacy_.daily_treaties.at(id);
        require(country==treaty.country_a||country==treaty.country_b,
                "H_DAILY_INVALID_PARTY");
        leader(phase_c(),country,prime);
        allow_foreign_policy(country);
        bool& approval=(country==treaty.country_a)?
                        treaty.approved_a:treaty.approved_b;
        require(!approval,"H_DAILY_DUPLICATE_RATIFICATION");
        approval=true;
        ++diplomacy_.revision;
    }
    // Time-specific, authenticated policy using the canonical H tariff
    // registry. Existing monthly adopt_tariff() keeps its old semantics.
    void adopt_tariff_daily(const std::string& command,
                            const std::string& importer,
                            const std::string& good,std::int64_t ppm,
                            const std::string& authorized_prime) {
        at(0);
        require(!command.empty()&&!importer.empty()&&!good.empty()&&
                ppm>=0&&ppm<=1'000'000 &&
                !diplomacy_.commands.contains("DAY_TARIFF:"+command),
                "H_ERR_DAILY_TARIFF_POLICY");
        leader(phase_c(),importer,authorized_prime);
        allow_foreign_policy(importer);
        diplomacy_.commands.insert("DAY_TARIFF:"+command);
        diplomacy_.tariffs[{importer,good}]={importer,good,ppm};
        ++diplomacy_.revision;
    }
    void adopt_tariff(const std::string& command, const std::string& importer,
                      const std::string& good, std::int64_t ppm,
                      const std::string& authorized_prime) {
        at(9);
        require(!command.empty() && !good.empty() && ppm >= 0 &&
                ppm <= 1'000'000 && !diplomacy_.commands.contains(command),
                "H_ERR_TARIFF_POLICY");
        leader(phase_c(), importer, authorized_prime);
        allow_foreign_policy(importer);
        // A tariff adopted now has no effect on earlier trades. Pilot policies
        // are carried as next-month schedule by the pending tariff table.
        diplomacy_.commands.insert(command);
        diplomacy_.scheduled_tariffs.push_back({phase_c().month()+1, importer, good, ppm});
        ++diplomacy_.revision;
    }

    void propose_tariff_treaty(const std::string& id,
                               const std::string& a, const std::string& b,
                               const std::string& good,
                               const std::string& a_prime) {
        at(9);
        require(!id.empty() && !good.empty() && a != b &&
                !diplomacy_.treaties.contains(id),
                "H_ERR_INVALID_TREATY");
        leader(phase_c(), a, a_prime);
        allow_foreign_policy(a);
        diplomacy_.treaties.emplace(id,Treaty{id,a,b,good,phase_c().month(),
                               phase_c().month()+1,true,false,false});
        ++diplomacy_.revision;
    }
    void ratify_treaty(const std::string& id, const std::string& country,
                       const std::string& prime) {
        at(9);
        auto& treaty = diplomacy_.treaties.at(id);
        leader(phase_c(), country, prime);
        allow_foreign_policy(country);
        require(!treaty.active && (country==treaty.country_a ||
                  country==treaty.country_b), "H_ERR_INVALID_RATIFICATION");
        bool& approved = country==treaty.country_a ?
                         treaty.ratified_a : treaty.ratified_b;
        require(!approved, "H_ERR_DUPLICATE_RATIFICATION");
        approved = true;
        treaty.ratifications.insert(country);
        treaty.effective_month = std::max(treaty.effective_month,
                                         phase_c().month()+1);
        queue_package_effects_if_ratified(treaty);
        ++diplomacy_.revision;
    }
    void establish_protectorate(const std::string& id,
                                 const std::string& principal,
                                 const std::string& dependent,
                                 const std::string& principal_prime,
                                 const std::string& dependent_prime) {
        at(9);
        require(!id.empty() && principal!=dependent &&
                !diplomacy_.protectorates.contains(id),
                "H_ERR_INVALID_PROTECTORATE");
        leader(phase_c(),principal,principal_prime);
        leader(phase_c(),dependent,dependent_prime);
        diplomacy_.protectorates.emplace(id,Protectorate{id,principal,dependent,
                                             phase_c().month()+1,false,true});
        ++diplomacy_.revision;
    }
    void create_organization(const std::string& id,
                             const std::set<std::string>& founders,
                             Money dues, const std::map<std::string,std::string>& primes) {
        at(9);
        require(!id.empty() && founders.size()>=2 && dues>=0 &&
                !diplomacy_.organizations.contains(id),
                "H_ERR_INVALID_ORGANIZATION");
        for (const auto& country:founders) {
            leader(phase_c(),country,primes.at(country));
            allow_foreign_policy(country);
        }
        // The treasury must be a genuine, pre-seeded C economic actor.
        if (dues>0) (void)phase_c().cash("ORG:"+id);
        diplomacy_.organizations.emplace(id,Organization{id,founders,
                             phase_c().month()+1,dues,false,{}});
        ++diplomacy_.revision;
    }
    void pay_dues(const std::string& id, const std::string& member,
                  Money amount) {
        at(5);
        auto& org=diplomacy_.organizations.at(id);
        require(org.active && org.members.contains(member) &&
                amount>0 && org.dues_per_member>0,
                "H_ERR_INVALID_CONTRIBUTION");
        auto& total=org.paid[{phase_c().month(),member}];
        require(total<=org.dues_per_member &&
                amount<=org.dues_per_member-total,
                "H_ERR_CONTRIBUTION_OVERPAY");
        phase_c().pay(member,"ORG:"+id,amount);
        total=phaseb::add_checked(total,amount);
        ++diplomacy_.revision;
    }
    void link_reparation_treaty(const std::string& id,
                               const std::string& payer,
                               const std::string& recipient, Money amount,
                               const std::string& prime) {
        at(9);
        leader(phase_c(),payer,prime);
        require(!diplomacy_.reparation_links.contains(id),
                "H_ERR_DUPLICATE_REPARATION");
        phase_c().sign_reparation_treaty(id,payer,recipient,amount);
        diplomacy_.reparation_links.insert(id);
        ++diplomacy_.revision;
    }
    void pay_reparation(const std::string& id, Money amount) {
        at(5);
        require(diplomacy_.reparation_links.contains(id),
                "H_ERR_UNKNOWN_REPARATION");
        phase_c().pay_reparation(id,amount);
    }
    void open_crisis(const std::string& id, Id province,
                     const std::string& claimant,
                     const std::string& respondent,
                     const std::string& claimant_prime) {
        at(9);
        require(!id.empty() && !diplomacy_.crises.contains(id) &&
                spatial_.spatial().provinces.contains(province),
                "H_ERR_INVALID_CRISIS");
        leader(phase_c(),claimant,claimant_prime);
        const auto& claims=spatial_.spatial().claims;
        require(claims.contains(province) &&
                claims.at(province).contains(claimant) &&
                phase_c().sovereignty(spatial_.spatial().provinces.at(province).region)
                    ==respondent && claimant!=respondent,
                "H_ERR_MISSING_UNDERLYING_DISPUTE");
        diplomacy_.crises.emplace(id,Crisis{id,claimant,respondent,"",province,
                                  phase_c().month()+1,CrisisStatus::scheduled});
        ++diplomacy_.revision;
    }
    void mediate_crisis(const std::string& id,
                        const std::string& mediator,
                        const std::string& mediator_prime) {
        at(9);
        leader(phase_c(),mediator,mediator_prime);
        auto& crisis=diplomacy_.crises.at(id);
        require(crisis.status==CrisisStatus::active &&
                mediator!=crisis.claimant && mediator!=crisis.respondent,
                "H_ERR_INVALID_MEDIATOR");
        crisis.mediator=mediator;
        crisis.status=CrisisStatus::mediation;
        ++diplomacy_.revision;
    }
    void resolve_crisis_without_war(const std::string& id,
                        const std::string& claimant_prime,
                        const std::string& respondent_prime) {
        at(9);
        auto& crisis=diplomacy_.crises.at(id);
        require(crisis.status==CrisisStatus::mediation,
                "H_ERR_CRISIS_NOT_IN_MEDIATION");
        leader(phase_c(),crisis.claimant,claimant_prime);
        leader(phase_c(),crisis.respondent,respondent_prime);
        crisis.status=CrisisStatus::settled; // No E title/war transaction.
        ++diplomacy_.revision;
    }
    void record_recognition(const std::string& from, const std::string& to,
                            const std::string& prime) {
        at(9);
        leader(phase_c(),from,prime);
        (void)phase_c().government(to);
        require(from!=to,"H_ERR_INVALID_RECOGNITION");
        for(const auto& r:diplomacy_.recognitions)
            require(!(r.recognizer==from&&r.recognized==to),
                    "H_ERR_DUPLICATE_RECOGNITION");
        diplomacy_.recognitions.push_back({from,to,phase_c().month()+1});
        ++diplomacy_.revision;
    }

    void plan_autonomous_trade();
    void execute_autonomous_trade();
    void settle_due_daily_trade_cargo() {
        at(0);
        if(day_index_<0)return;
        for(auto it=trade_.daily_shipments.begin();it!=trade_.daily_shipments.end();) {
            const auto cargo=it->second;
            if(cargo.arrival_day>day_index_) {++it;continue;}
            phase_c().phase_time_arrive(cargo.id,cargo.buyer,cargo.destination,
                                       cargo.good,cargo.quantity,cargo.loss);
            acknowledge_daily_cargo_arrival(cargo.id,cargo.quantity,cargo.loss);
            it=trade_.daily_shipments.erase(it);
        }
    }
    [[nodiscard]] bool verified_day_route(const PathQuote& path)const {
        if(path.edges.empty())return false;
        for(const auto& id:path.edges)
            if(trade_.transport_edges.at(id).transit_days<=0)return false;
        return true;
    }
    void propose_treaty_package(const std::string& id,
             const std::set<std::string>& parties,
             std::vector<TreatyClause> clauses,
             const std::string& proposer,const std::string& prime);
    void ratify_package_member(const std::string& id,
             const std::string& country,const std::string& prime);
    void terminate_treaty(const std::string& id,
             const std::string& requesting_country,const std::string& prime);
    void configure_organization(const std::string& id,
             OrganizationRule rule,const std::string& proposing_prime);
    void hold_organization_vote(const std::string& org,const std::string& id,
             const std::string& subject,
             const std::map<std::string,VoteChoice>& ballots,
             const std::string& proposer,const std::string& prime);
    void admit_organization_member(const std::string& org,
             const std::string& member,const std::string& decision,
             const std::string& applicant_prime);
    void leave_organization(const std::string& org,
             const std::string& member,const std::string& prime);
    void fund_cultural_exchange(const std::string& id,
             const std::string& sponsor,const std::string& recipient,
             Money amount,const std::string& sponsor_prime);
    void dispatch_military_assistance(const std::string& id,
             const std::string& donor,const std::string& recipient,
             const std::string& good,Id origin,Id destination,
             Quantity amount,Money reference_unit_value,Month duration,
             const std::string& donor_prime);
    void submit_crisis_position(const std::string& crisis,
             const std::string& actor,const std::string& stance,
             bool conditional,const std::string& prime);
    void offer_crisis_compromise(const std::string& crisis,
             CrisisProposal proposal,const std::string& prime);
    void respond_crisis_offer(const std::string& crisis,
             const std::string& proposal,const std::string& actor,
             bool accept,const std::string& prime);
    void freeze_crisis(const std::string& crisis,
             const std::string& claimant_prime,const std::string& respondent_prime);
    void disburse_treaty_aid(const std::string& treaty,
             const std::string& term,Money amount);
    void create_trade_contract(TradeContract c);
    void offer_trade_credit(CreditFacility credit);
    void draw_trade_credit(const std::string& id, Money amount);
    void repay_trade_credit(const std::string& id, Money amount);
    void insure_cargo(CargoInsurance policy);
    void exchange_currencies(const std::string& a, const std::string& b,
                             const std::string& sold, Money sold_amount,
                             const std::string& bought, Money bought_amount);
    void advance_legal_state() {
        at(0);
        for(const auto& rule:diplomacy_.scheduled_tariffs) {
            if(rule.effective<=phase_c().month()) {
                diplomacy_.tariffs[{rule.importer,rule.good}] =
                    {rule.importer,rule.good,rule.ppm};
                ++diplomacy_.revision;
            }
        }
        std::erase_if(diplomacy_.scheduled_tariffs,[&](const PendingTariff& rule){
            return rule.effective<=phase_c().month();
        });
        std::erase_if(trade_.edge_usage,[&](const auto& kv){
            return kv.first.first<phase_c().month();
        });
        std::erase_if(diplomacy_.quota_used,[&](const auto& kv){
            return std::get<3>(kv.first)<phase_c().month();
        });
        for(auto& [id,t]:diplomacy_.treaties) {
            (void)id;
            if(t.pending_termination){
                t.active=false;
                t.pending_termination=false;
                ++diplomacy_.revision;
                continue;
            }
            const bool fully_ratified=t.parties.empty()?
                               (t.ratified_a&&t.ratified_b):
                               std::all_of(t.parties.begin(),t.parties.end(),
                                 [&](const std::string& p){
                                     return t.ratifications.contains(p);
                                 });
            if(!t.active && fully_ratified &&
               t.effective_month<=phase_c().month()){
                t.active=true;
                ++diplomacy_.revision;
            }
        }
        for(auto& [id,o]:diplomacy_.organizations) {
            (void)id;
            if(!o.active && o.effective_month<=phase_c().month() &&
               o.members.size()>=2) {
                o.active=true;
                ++diplomacy_.revision;
            }
            if(auto pending=o.pending_admissions.find(phase_c().month());
               pending!=o.pending_admissions.end()) {
                o.members.insert(pending->second.begin(),pending->second.end());
                o.pending_admissions.erase(pending);
                ++diplomacy_.revision;
            }
            if(auto pending=o.pending_exits.find(phase_c().month());
               pending!=o.pending_exits.end()) {
                for(const auto& member:pending->second)o.members.erase(member);
                o.pending_exits.erase(pending);
                if(o.members.size()<2)o.active=false;
                ++diplomacy_.revision;
            }
        }
        for(auto& [id,p]:diplomacy_.protectorates) {
            (void)id;
            if(!p.active && p.effective_month<=phase_c().month()){
                p.active=true;
                ++diplomacy_.revision;
            }
        }
        for(auto& [id,c]:diplomacy_.crises) {
            (void)id;
            if(c.status==CrisisStatus::scheduled &&
               c.effective_month<=phase_c().month()){
                c.status=CrisisStatus::active;
                ++diplomacy_.revision;
            }
        }
    }
};

class World {
    friend struct SnapshotCodec;
public:
    using Task=std::function<void(Stage&)>;
    using Plan=std::array<std::vector<Task>,12>;
private:
    phasee::World core_;
    TradeState trade_;
    DiplomacyState diplomacy_;
    void check(const TradeState& g, const DiplomacyState& h,
               const phasee::SpatialStage& s) const {
        for(const auto& [id,r]:g.routes) {
            require(!id.empty() && r.id==id && r.maximum_quantity>0 &&
                    s.spatial().provinces.contains(r.origin) &&
                    s.spatial().provinces.contains(r.destination),
                    "G_ERR_INVALID_ROUTE");
        }
        for(const auto& [id,t]:h.treaties)
            require(id==t.id && t.country_a!=t.country_b &&
                    t.effective_month>t.agreed_month &&
                    (!t.active || (t.ratified_a&&t.ratified_b)),
                    "H_ERR_TREATY_INVARIANT");
        for(const auto& [id,o]:h.organizations)
            require(id==o.id && (!o.active||o.members.size()>=2) &&
                    o.dues_per_member>=0,"H_ERR_ORGANIZATION_INVARIANT");
        for(const auto& [id,c]:h.crises)
            require(id==c.id && s.spatial().provinces.contains(c.province),
                    "H_ERR_CRISIS_INVARIANT");
        for(const auto& [id,cargo]:g.daily_shipments) {
            require(id==cargo.id&&!id.empty()&&!cargo.buyer.empty()&&
                    !cargo.destination.empty()&&!cargo.good.empty()&&
                    cargo.quantity>0&&cargo.loss>=0&&cargo.loss<=cargo.quantity&&
                    cargo.arrival_day>0,
                    "G_ERR_NATIVE_CARGO_INVARIANT");
        }
        require(g.pending.empty(), "G_ERR_UNSETTLED_RESERVATIONS");
    }
public:
    explicit World(Month starting_month=24000):core_(starting_month){}
    static World from_legacy_phase_e(phasee::World legacy) {
        World result(legacy.month());
        result.core_=std::move(legacy);
        result.validate();
        return result;
    }
    // Native day-aware H + D + E + C staged transaction. No monthly
    // autonomous merchant/price/demography processing on an ordinary day.
    phasec::AdvanceReport advance_daily(const Plan& tasks,std::int64_t day,
                                         bool first_calendar_day=false,
                                         std::int32_t day_of_month=0,
                                         std::int32_t days_in_month=0,
                                         bool final_calendar_day=false) {
        TradeState new_trade=trade_;
        DiplomacyState new_diplomacy=diplomacy_;
        phasee::World::Plan bridge;
        for(int n=0;n<12;++n)
            bridge[static_cast<std::size_t>(n)].push_back(
                [&,n](phasee::SpatialStage& spatial) {
                    Stage stage{spatial,new_trade,new_diplomacy,n,
                                day_of_month,days_in_month,day};
                    if(n==0){
                        if(first_calendar_day)stage.advance_legal_state();
                        if(day_of_month>0) {
                            stage.settle_due_daily_trade_cargo();
                            stage.register_arrived_losses();
                        }
                        for(auto& [id,t]:new_diplomacy.daily_treaties) {
                            (void)id;
                            if(!t.active&&t.approved_a&&t.approved_b&&
                               t.effective_day<=day) {
                                t.active=true;
                                ++new_diplomacy.revision;
                            }
                        }
                    }
                    for(const auto& fn:tasks[static_cast<std::size_t>(n)])
                        fn(stage);
                    if(day_of_month>0){
                        if(n==1){
                            stage.reserve_collateral();
                            stage.plan_contracts();
                            stage.plan_autonomous_trade();
                        }
                        if(n==5)stage.settle_insurance_claims();
                        if(n==7){
                            stage.execute_autonomous_trade();
                            stage.pay_insurance_premiums();
                        }
                    }
                    if(n==11)check(new_trade,new_diplomacy,spatial);
                });
        static_assert(std::is_nothrow_move_assignable_v<TradeState>);
        static_assert(std::is_nothrow_move_assignable_v<DiplomacyState>);
        const auto result=core_.advance_daily(bridge,first_calendar_day,
                                               final_calendar_day);
        trade_=std::move(new_trade);
        diplomacy_=std::move(new_diplomacy);
        return result;
    }
    // Real daily Phase C events go through the same authoritative C ledger.
    // G/H autonomous trade, monthly capacity, treaty schedules and Phase B/D
    // background calculations are executed ONLY in advance() at MonthClose.
    [[nodiscard]] bool has_autonomous_daily_trade()const {
        return !trade_.routes.empty()||!trade_.contracts.empty()||
               !trade_.credit.empty()||!trade_.insurance.empty()||
               !trade_.daily_shipments.empty();
    }
    phasec::AdvanceReport advance_daily(const phasec::IntegratedWorld::Plan& commands) {
        World staged=*this;
        const auto report=staged.core_.advance_daily(commands);
        staged.validate();
        *this=std::move(staged);
        return report;
    }
    phasee::World& seed_geography() {
        require(core_.commit_id()==0, "GH_ERR_SEED_AFTER_COMMIT");
        return core_;
    }
    void seed_route(Route r) {
        require(core_.commit_id()==0 && !r.id.empty() && !r.seller.empty() &&
                !r.buyer.empty() && r.seller!=r.buyer && !r.good.empty() &&
                r.maximum_quantity>0 && r.origin>0 && r.destination>0 &&
                (r.origin==r.destination || r.travel_months>0) &&
                core_.spatial().provinces.contains(r.origin) &&
                core_.spatial().provinces.contains(r.destination) &&
                !trade_.routes.contains(r.id),"G_ERR_INVALID_ROUTE");
        trade_.routes.emplace(r.id,std::move(r));
    }
    void seed_transport_edge(TransportEdge edge) {
        require(core_.commit_id()==0 && !edge.id.empty() &&
                edge.from>0 && edge.to>0 && edge.from!=edge.to &&
                edge.monthly_capacity>0 && edge.freight_per_unit>=0 &&
                edge.transit_months>0 && edge.transit_days>=0 &&
                !edge.carrier.empty() &&
                core_.spatial().provinces.contains(edge.from) &&
                core_.spatial().provinces.contains(edge.to) &&
                !trade_.transport_edges.contains(edge.id),
                "G_ERR_INVALID_LOGISTICS_EDGE");
        const auto& a=core_.spatial().provinces.at(edge.from);
        const auto& b=core_.spatial().provinces.at(edge.to);
        bool coastal=false;
        for(Id sea:a.coastal_seas)if(b.coastal_seas.contains(sea))coastal=true;
        require(a.neighbors.contains(edge.to)||coastal,
                "G_ERR_UNCONNECTED_LOGISTICS_EDGE");
        (void)core_.core().core().cash(edge.carrier);
        trade_.transport_adjacency[edge.from].push_back(edge.id);
        trade_.transport_edges.emplace(edge.id,std::move(edge));
    }
    void seed_tariff(std::string importer, std::string good,
                     std::int64_t ppm) {
        require(core_.commit_id()==0 && !importer.empty() && !good.empty() &&
                ppm>=0 && ppm<=1'000'000,"H_ERR_INVALID_TARIFF");
        diplomacy_.tariffs[{importer,good}]={importer,good,ppm};
    }
    [[nodiscard]] const phasee::World& base() const {return core_;}
    [[nodiscard]] Month month() const{return core_.month();}
    [[nodiscard]] std::uint64_t commit_id()const{return core_.commit_id();}
    [[nodiscard]] bool has_due_daily_treaty(std::int64_t day)const {
        for(const auto& [id,t]:diplomacy_.daily_treaties) {
            (void)id;
            if(!t.active&&t.approved_a&&t.approved_b&&t.effective_day<=day)
                return true;
        }
        return false;
    }
    [[nodiscard]] bool has_ready_daily_commission()const {
        return core_.has_ready_daily_commission();
    }
    [[nodiscard]] const TradeState& commerce() const{return trade_;}
    [[nodiscard]] const DiplomacyState& diplomacy()const{return diplomacy_;}
    [[nodiscard]] const TradeContract& contract(const std::string& id)const{
        return trade_.contracts.at(id);
    }
    [[nodiscard]] const CargoInsurance& insurance(const std::string& id)const{
        return trade_.insurance.at(id);
    }
    [[nodiscard]] const CreditFacility& trade_credit(const std::string& id)const{
        return trade_.credit.at(id);
    }
    [[nodiscard]] InfluenceView assess_influence(const std::string& source,
                                                  const std::string& target) const;
    [[nodiscard]] CrisisAdvice advise_crisis(const std::string& id,
                                              const std::string& actor) const;
    [[nodiscard]] InfluenceView trade_influence(const std::string& source,
                                                  const std::string& target) const {
        InfluenceView view;
        for(const auto& fill:trade_.fills) {
            if(fill.buyer_country!=target ||
               fill.seller_country==target)continue;
            // Values denominated in different currencies are not additive.
            if(fill.currency!="TEST"){view.foreign_currency_excluded=true;continue;}
            view.target_total_imports=phaseb::add_checked(
                                view.target_total_imports,fill.goods_value);
            if(fill.seller_country==source)
                view.bilateral_imports=phaseb::add_checked(
                                view.bilateral_imports,fill.goods_value);
        }
        if(view.target_total_imports>0) {
            view.observed=true;
            view.trade_dependency_ppm=phaseb::mul_checked(
                view.bilateral_imports,1'000'000)/view.target_total_imports;
        }
        return view;
    }
    void validate()const{
        core_.validate();
        for(const auto& [id,t]:diplomacy_.daily_treaties)
            require(id==t.id&&!id.empty()&&
                    t.country_a!=t.country_b && !t.good.empty()&&
                    t.effective_day>t.proposed_day &&
                    (!t.active||(t.approved_a&&t.approved_b)),
                    "H_DAILY_TREATY_INVARIANT");
        for(const auto& [id,t]:diplomacy_.treaties)
            require(id==t.id && (!t.active||
              (t.parties.empty()?(t.ratified_a&&t.ratified_b):
                std::all_of(t.parties.begin(),t.parties.end(),
                  [&](const std::string& p){return t.ratifications.contains(p);}))),
                    "H_ERR_TREATY_INVARIANT");
        for(const auto& [id,r]:trade_.routes)
            require(id==r.id && core_.spatial().provinces.contains(r.origin)&&
                    core_.spatial().provinces.contains(r.destination),
                    "G_ERR_INVALID_ROUTE");
        const auto& inventory=core_.core().core();
        for(const auto& [id,cargo]:trade_.daily_shipments)
            require(inventory.stock("TRANSIT:DAY:"+id,"in_transit",cargo.good)==
                        cargo.quantity,
                    "G_ERR_NATIVE_CARGO_NOT_BACKED_BY_INVENTORY");
        require(trade_.pending.empty(),"G_ERR_PENDING_OUTSIDE_MONTH");
    }
    phasec::AdvanceReport advance(const Plan& plan={}) {
        TradeState next_trade=trade_;
        DiplomacyState next_diplomacy=diplomacy_;
        phasee::World::Plan bridge;
        for(int n=0;n<12;++n) {
            bridge[static_cast<std::size_t>(n)].push_back(
                [&,n](phasee::SpatialStage& spatial){
                    Stage stage{spatial,next_trade,next_diplomacy,n};
                    if(n==0){
                        stage.advance_legal_state();
                        stage.register_arrived_losses();
                    }
                    for(const auto& callback:plan[static_cast<std::size_t>(n)])
                        callback(stage);
                    // Standalone legacy monthly API still runs its own
                    // isolated monthly simulation; native day never calls it.
                    if(n==1){
                        stage.reserve_collateral();
                        stage.plan_contracts();
                        stage.plan_autonomous_trade();
                    }
                    if(n==5)stage.settle_insurance_claims();
                    if(n==7){
                        stage.execute_autonomous_trade();
                        stage.pay_insurance_premiums();
                    }
                    if(n==11)check(next_trade,next_diplomacy,spatial);
                });
        }
        // E advances D and C on the same staged copy; commit sidecars only
        // after every C/D/E/G/H operation succeeds.
        static_assert(std::is_nothrow_move_assignable_v<TradeState>);
        static_assert(std::is_nothrow_move_assignable_v<DiplomacyState>);
        const auto report=core_.advance(bridge);
        trade_=std::move(next_trade);
        diplomacy_=std::move(next_diplomacy);
        return report;
    }
};



inline void Stage::plan_contracts() {
    at(1);
    auto& core=phase_c();
    trade_.pending.clear();
    trade_.fx_held.clear();
    const auto month=core.month();
    const auto& geo=spatial_.spatial();
    for(const auto& [id,agreement]:trade_.contracts) {
        if(!agreement.active||agreement.effective>month||
           agreement.expires<month||agreement.fulfilled>=agreement.contracted)
           continue;
        const auto& origin=geo.provinces.at(agreement.origin).region;
        const auto& dest=geo.provinces.at(agreement.destination).region;
        const auto& country_from=core.sovereignty(origin);
        const auto& country_to=core.sovereignty(dest);
        const auto duty_ppm=duty_rate(country_from,country_to,agreement.good);
        const auto path=quote_path(agreement.origin,agreement.destination,month);
        if(agreement.origin!=agreement.destination&&
           !trade_.transport_edges.empty()&&!path)continue;
        if(day_of_month_>0 && agreement.origin!=agreement.destination &&
           (!path || !verified_day_route(*path)))continue;
        const Money freight_unit=path?path->freight_per_unit:0;
        const Month travel=path?path->travel_months:
                                (agreement.origin==agreement.destination?0:1);
        const Quantity max_route=path?path->capacity:
                                     std::numeric_limits<Quantity>::max();
        const Quantity available=core.phase_d_material_available(
                      agreement.seller,origin,agreement.good);
        const Money unit_customs=(phaseb::mul_checked(agreement.unit_price,duty_ppm)
                                   +999'999)/1'000'000;
        const Money main_unit=phaseb::add_checked(
                  phaseb::add_checked(agreement.currency=="TEST"?
                                    agreement.unit_price:0,unit_customs),
                                    freight_unit);
        const Quantity used_in_month=monthly_fill(id,true);
        const Quantity available_on_day=day_budget(agreement.monthly_limit,used_in_month);
        Quantity q=std::min({agreement.contracted-agreement.fulfilled,
                             available_on_day,max_route,available});
        if(agreement.currency=="TEST") {
            if(main_unit>0)q=std::min(q,core.phase_d_cash_available(
                                      agreement.buyer)/main_unit);
        } else {
            const auto available_foreign=core.fx_balance(agreement.buyer,
                                                         agreement.currency);
            const auto held=trade_.fx_held.contains({agreement.buyer,agreement.currency})?
                      trade_.fx_held.at({agreement.buyer,agreement.currency}):0;
            require(available_foreign>=held,"G_ERR_FX_OVERRESERVED");
            q=std::min(q,(available_foreign-held)/agreement.unit_price);
            const auto main_cash=core.phase_d_cash_available(agreement.buyer);
            if(main_unit>0)q=std::min(q,main_cash/main_unit);
        }
        q=lawful_trade_quantity(country_from,country_to,agreement.good,q);
        if(q<=0)continue;
        if(!agreement.partial && q!=agreement.contracted-agreement.fulfilled)
            continue;
        const Money base=phaseb::mul_checked(q,agreement.unit_price);
        const Money duty=phaseb::mul_checked(base,duty_ppm)/1'000'000;
        const Money freight=phaseb::mul_checked(q,freight_unit);
        const Money local_total=phaseb::add_checked(
                       agreement.currency=="TEST"?base:0,
                       phaseb::add_checked(duty,freight));
        const std::string event="G:contract:"+id+":"+std::to_string(month)+
            (day_of_month_>0?":D"+std::to_string(day_index_):"");
        if(local_total>0)core.phase_d_reserve_cash(agreement.buyer,event,local_total);
        core.phase_d_reserve_material(agreement.seller,origin,agreement.good,event,q);
        if(agreement.currency!="TEST")
            trade_.fx_held[{agreement.buyer,agreement.currency}]=phaseb::add_checked(
                  trade_.fx_held[{agreement.buyer,agreement.currency}],base);
        reserve_trade_quota(country_from,country_to,agreement.good,q);
        if(path)for(const auto& edge_id:path->edges) {
            trade_.edge_usage[{month,edge_id}]=phaseb::add_checked(
                      trade_.edge_usage[{month,edge_id}],q);
        }
        trade_.pending.push_back({event,id,agreement.seller,agreement.buyer,
               agreement.good,origin,dest,country_from,country_to,q,travel,
               agreement.unit_price,local_total,duty,freight,id,
               agreement.currency,path?path->edges:std::vector<std::string>{},0});
    }
}
inline void Stage::plan_autonomous_trade() {
    at(1);
    auto& core=phase_c();
    const auto& geo=spatial_.spatial();
    for(const auto& [id,route]:trade_.routes) {
        (void)id;
        const auto& origin=geo.provinces.at(route.origin).region;
        const auto& dest=geo.provinces.at(route.destination).region;
        const auto& from=core.sovereignty(origin);
        const auto& to=core.sovereignty(dest);
        const auto& source_quote=core.market(origin,route.good);
        const auto& target_quote=core.market(dest,route.good);
        const auto ppm=duty_rate(from,to,route.good);
        const Money price=source_quote.price;
        require(price>0&&ppm>=0&&ppm<=1'000'000,
                "G_ERR_INVALID_MARKET_QUOTE");
        const auto path=quote_path(route.origin,route.destination,core.month());
        if(route.origin!=route.destination&&
           !trade_.transport_edges.empty()&&!path)continue;
        if(day_of_month_>0 && route.origin!=route.destination &&
           (!path || !verified_day_route(*path)))continue;
        const Money unit_freight=path?path->freight_per_unit:0;
        const Quantity max_route=path?path->capacity:route.maximum_quantity;
        const Month travel=path?path->travel_months:route.travel_months;
        const Money unit_duty=(phaseb::mul_checked(price,ppm)+999'999)/1'000'000;
        const Money landed_unit=phaseb::add_checked(
                          phaseb::add_checked(price,unit_duty),unit_freight);
        if(target_quote.price<=landed_unit)continue;
        const auto available_stock=core.phase_d_material_available(
                                 route.seller,origin,route.good);
        const auto available_cash=core.phase_d_cash_available(route.buyer);
        const Quantity used_in_month=monthly_fill(route.id,false);
        const Quantity available_on_day=day_budget(route.maximum_quantity,used_in_month);
        const Quantity qty=lawful_trade_quantity(from,to,route.good,
             std::min({available_on_day,max_route,available_stock,
                       available_cash/landed_unit}));
        if(qty<=0)continue;
        const Money goods_cost=phaseb::mul_checked(qty,price);
        const Money duty=phaseb::mul_checked(goods_cost,ppm)/1'000'000;
        const Money freight=phaseb::mul_checked(qty,unit_freight);
        const Money total=phaseb::add_checked(goods_cost,
                                             phaseb::add_checked(duty,freight));
        const std::string eid="G:"+route.id+":"+std::to_string(core.month())+
            (day_of_month_>0?":D"+std::to_string(day_index_):"");
        require(!trade_.executed.contains(eid),"G_ERR_DUPLICATE_EXECUTION");
        if(duty>0)(void)core.cash(to);
        core.phase_d_reserve_cash(route.buyer,eid,total);
        core.phase_d_reserve_material(route.seller,origin,route.good,eid,qty);
        reserve_trade_quota(from,to,route.good,qty);
        if(path)for(const auto& edge_id:path->edges)
            trade_.edge_usage[{core.month(),edge_id}]=phaseb::add_checked(
                trade_.edge_usage[{core.month(),edge_id}],qty);
        trade_.pending.push_back({eid,route.id,route.seller,route.buyer,
                         route.good,origin,dest,from,to,qty,travel,
                         price,total,duty,freight,"","TEST",
                         path?path->edges:std::vector<std::string>{},
                         std::min(qty,route.arrival_loss)});
    }
}
inline void Stage::execute_autonomous_trade() {
    at(7);
    auto& core=phase_c();
    for(const auto& order:trade_.pending) {
        require(trade_.executed.insert(order.execution_id).second,
                "G_ERR_DUPLICATE_EXECUTION");
        if(order.total_cost>0)
            core.phase_d_release_cash(order.buyer,order.execution_id,
                                      order.total_cost);
        core.phase_g_release_reserved_goods(order.seller,order.origin_region,
                           order.good,order.execution_id,order.quantity);
        const Money goods_cost=phaseb::mul_checked(
                                      order.unit_price,order.quantity);
        if(order.origin_region==order.destination_region) {
            if(order.settlement_currency=="TEST")
                core.trade(order.seller,order.buyer,order.origin_region,
                           order.good,order.quantity,order.unit_price);
            else core.phase_g_transfer_goods(order.seller,order.buyer,
                                 order.origin_region,order.good,order.quantity);
        } else {
            if(day_of_month_>0) {
                // Native G shipments use the same physical C TRANSIT:DAY
                // inventory and arrival method as Phase F's cargo commands.
                require(day_index_>=0&&!order.path_edges.empty(),
                        "G_ERR_NATIVE_ROUTE_REQUIRED");
                std::int64_t duration=0;
                for(const auto& edge_id:order.path_edges) {
                    const auto days=trade_.transport_edges.at(edge_id).transit_days;
                    require(days>0,"G_ERR_UNVERIFIED_TRANSIT_DAYS");
                    duration=phaseb::add_checked(duration,days);
                }
                const auto arrival=phaseb::add_checked(day_index_,duration);
                require(arrival>day_index_ &&
                        !trade_.daily_shipments.contains(order.execution_id),
                        "G_ERR_INVALID_NATIVE_ETA");
                core.phase_time_dispatch(order.execution_id,order.seller,
                                         order.origin_region,order.good,
                                         order.quantity);
                trade_.daily_shipments.emplace(order.execution_id,
                    DailyTradeTransit{order.execution_id,order.buyer,
                        order.destination_region,order.good,order.quantity,
                        order.confirmed_arrival_loss,arrival});
                trade_.executed.insert("DAY:"+order.execution_id);
            } else {
                // Standalone legacy monthly scenario only; not day gameplay.
                require(order.travel_months>0,"G_ERR_UNSUPPORTED_TRANSPORT");
                core.ship(order.execution_id,order.seller,order.buyer,
                          order.origin_region,order.destination_region,
                          order.good,order.quantity,order.travel_months,
                          order.confirmed_arrival_loss);
            }
            if(order.settlement_currency=="TEST")
                core.pay(order.buyer,order.seller,goods_cost);
        }
        if(order.settlement_currency!="TEST")
            core.phase_g_pay_foreign(order.buyer,order.seller,
                                     order.settlement_currency,goods_cost);
        if(order.freight>0) {
            require(!order.path_edges.empty(),"G_ERR_MISSING_FREIGHT_ROUTE");
            Money actual_freight=0;
            for(const auto& edge_id:order.path_edges) {
                const auto& edge=trade_.transport_edges.at(edge_id);
                const Money amount=phaseb::mul_checked(
                                     order.quantity,edge.freight_per_unit);
                if(amount>0){
                    require(edge.carrier!=order.buyer,
                            "G_ERR_SELF_BILLED_FREIGHT_UNSUPPORTED");
                    core.pay(order.buyer,edge.carrier,amount);
                    actual_freight=phaseb::add_checked(actual_freight,amount);
                }
            }
            require(actual_freight==order.freight,
                    "G_ERR_FREIGHT_ACCOUNTING_MISMATCH");
        }
        if(order.duty>0) {
            core.phase_g_pay_customs(order.buyer,order.buyer_country,order.duty);
            trade_.customs.push_back({order.execution_id,order.buyer_country,
                 order.buyer_country,order.good,"H:effective-domestic-policy",
                 core.month(),goods_cost,order.duty,order.duty,order.quantity});
        }
        if(!order.contract_id.empty()) {
            auto& contract=trade_.contracts.at(order.contract_id);
            require(contract.fulfilled<=contract.contracted &&
                    order.quantity<=contract.contracted-contract.fulfilled,
                    "G_ERR_CONTRACT_OVERDELIVERY");
            contract.fulfilled=phaseb::add_checked(
                                   contract.fulfilled,order.quantity);
        }
        trade_.fills.push_back({order.execution_id,order.route_id,
            order.seller_country,order.buyer_country,core.month(),
            order.quantity,goods_cost,order.duty,
            order.origin_region!=order.destination_region,
            order.freight,order.contract_id,order.settlement_currency,
            order.path_edges});
        ++trade_.revision;
    }
    trade_.pending.clear();
    trade_.fx_held.clear();
}

} // namespace vic25::phasegh
#include "vic25/phase_gh_extensions.hpp"
#include "vic25/phase_gh_diplomacy.hpp"
