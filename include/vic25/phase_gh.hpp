#pragma once
// VIC2.5 Phase G/H bounded C++ integration into the existing C/D/E world.
// G owns trade decisions and execution references; H owns diplomatic legal
// states. ALL money, goods, geography, manpower and physical shipments are
// mutated through Phase C/D/E, never through shadow H/G ledgers.
// This is a functional vertical slice, NOT the full 62+176 acceptance engine.

#include "vic25/phase_e.hpp"
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
    Month transit_months{};
    bool enabled{true};
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
};

struct PathQuote {
    Money freight_per_unit{};
    Month travel_months{};
    Quantity capacity{};
    std::vector<std::string> edges;
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
    std::set<std::string> executed;
    std::uint64_t revision{};
};

struct Tariff {
    std::string importer_country, good;
    std::int64_t ppm{};
};
struct Treaty {
    std::string id, country_a, country_b, good;
    Month agreed_month{}, effective_month{};
    bool ratified_a{}, ratified_b{}, active{};
    // This bounded model only supports a bilateral tariff-exemption term.
};
struct Organization {
    std::string id;
    std::set<std::string> members;
    Month effective_month{};
    Money dues_per_member{};
    bool active{};
    std::map<std::pair<Month,std::string>, Money> paid;
};
struct Protectorate {
    std::string id, principal, dependent;
    Month effective_month{};
    bool active{}, exclusive_foreign_policy{};
};
enum class CrisisStatus : std::uint8_t {
    scheduled, active, mediation, settled
};
struct Crisis {
    std::string id, claimant, respondent, mediator;
    Id province{};
    Month effective_month{};
    CrisisStatus status{CrisisStatus::scheduled};
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
struct DiplomacyState {
    std::map<std::pair<std::string,std::string>,Tariff> tariffs;
    std::vector<PendingTariff> scheduled_tariffs;
    std::map<std::string,Treaty> treaties;
    std::map<std::string,Organization> organizations;
    std::map<std::string,Protectorate> protectorates;
    std::map<std::string,Crisis> crises;
    std::vector<Recognition> recognitions;
    std::set<std::string> reparation_links; // C is the monetary obligation owner.
    std::set<std::string> commands;
    std::uint64_t revision{};
};

struct InfluenceView {
    bool observed{};                 // false means unknown, NOT zero influence.
    Money bilateral_imports{};       // Only G's actually settled trade evidence.
    Money target_total_imports{};
    std::int64_t trade_dependency_ppm{};
    // Military/political/cultural dimensions remain unsupported, not zero.
    bool military_known{}, political_known{}, cultural_known{};
};

class Stage {
    phasee::SpatialStage& spatial_;
    TradeState& trade_;
    DiplomacyState& diplomacy_;
    int phase_{};
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
                              const std::string& good) const {
        for (const auto& [id, treaty] : diplomacy_.treaties) {
            (void)id;
            if (!treaty.active || treaty.good != good) continue;
            if ((treaty.country_a == seller && treaty.country_b == buyer) ||
                (treaty.country_b == seller && treaty.country_a == buyer))
                return true;
        }
        return false;
    }
    [[nodiscard]] std::int64_t duty_rate(const std::string& seller,
                                          const std::string& buyer,
                                          const std::string& good) const {
        if (seller == buyer || exempt(seller, buyer, good)) return 0;
        auto it = diplomacy_.tariffs.find({buyer, good});
        return it == diplomacy_.tariffs.end() ? 0 : it->second.ppm;
    }
public:
    Stage(phasee::SpatialStage& spatial, TradeState& trade,
          DiplomacyState& diplomacy, int phase)
        : spatial_(spatial), trade_(trade), diplomacy_(diplomacy), phase_(phase) {}
    [[nodiscard]] int phase() const { return phase_; }
    [[nodiscard]] phasee::SpatialStage& geography() { return spatial_; }
    [[nodiscard]] phased::Stage& phase_d() { return spatial_.phase_d(); }
    [[nodiscard]] phasec::IntegratedWorld& phase_c() { return spatial_.phase_c(); }
    [[nodiscard]] const DiplomacyState& diplomacy() const { return diplomacy_; }
    [[nodiscard]] const TradeState& commerce() const { return trade_; }

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
        treaty.effective_month = std::max(treaty.effective_month,
                                         phase_c().month()+1);
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
        for(auto& [id,t]:diplomacy_.treaties) {
            (void)id;
            if(!t.active && t.ratified_a && t.ratified_b &&
               t.effective_month<=phase_c().month()){
                t.active=true;
                ++diplomacy_.revision;
            }
        }
        for(auto& [id,o]:diplomacy_.organizations) {
            (void)id;
            if(!o.active && o.effective_month<=phase_c().month()){
                o.active=true;
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
            require(id==o.id && o.members.size()>=2 &&
                    o.dues_per_member>=0,"H_ERR_ORGANIZATION_INVARIANT");
        for(const auto& [id,c]:h.crises)
            require(id==c.id && s.spatial().provinces.contains(c.province),
                    "H_ERR_CRISIS_INVARIANT");
        require(g.pending.empty(), "G_ERR_UNSETTLED_RESERVATIONS");
    }
public:
    explicit World(Month starting_month=24000):core_(starting_month){}
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
                edge.transit_months>0 && !edge.carrier.empty() &&
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
    [[nodiscard]] const TradeState& commerce() const{return trade_;}
    [[nodiscard]] const DiplomacyState& diplomacy()const{return diplomacy_}
    [[nodiscard]] const TradeContract& contract(const std::string& id)const{
        return trade_.contracts.at(id);
    }
    [[nodiscard]] const CargoInsurance& insurance(const std::string& id)const{
        return trade_.insurance.at(id);
    }
    [[nodiscard]] const CreditFacility& trade_credit(const std::string& id)const{
        return trade_.credit.at(id);
    }
    [[nodiscard]] InfluenceView trade_influence(const std::string& source,
                                                  const std::string& target) const {
        InfluenceView view;
        for(const auto& fill:trade_.fills) {
            if(fill.buyer_country!=target ||
               fill.seller_country==target)continue;
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
        for(const auto& [id,t]:diplomacy_.treaties)
            require(id==t.id && (!t.active||(t.ratified_a&&t.ratified_b)),
                    "H_ERR_TREATY_INVARIANT");
        for(const auto& [id,r]:trade_.routes)
            require(id==r.id && core_.spatial().provinces.contains(r.origin)&&
                    core_.spatial().provinces.contains(r.destination),
                    "G_ERR_INVALID_ROUTE");
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


inline void Stage::plan_autonomous_trade() {
    at(1);
    auto& core=phase_c();
    trade_.pending.clear();
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
        require(price>0 && ppm>=0 && ppm<=1'000'000,
                "G_ERR_INVALID_MARKET_QUOTE");
        const Money unit_duty_ceiling=
            (phaseb::mul_checked(price,ppm)+999'999)/1'000'000;
        const Money landed_unit=phaseb::add_checked(price,unit_duty_ceiling);
        // No compulsory country-wide export quotas: the merchant buys only
        // when its destination reference price covers landed cost.
        if(target_quote.price<=landed_unit)continue;
        const auto available_stock=core.phase_d_material_available(
                                 route.seller,origin,route.good);
        const auto available_cash=core.phase_d_cash_available(route.buyer);
        const Quantity qty=std::min({
            route.maximum_quantity,available_stock,
            available_cash/landed_unit
        });
        if(qty<=0)continue;
        const Money goods_cost=phaseb::mul_checked(qty,price);
        const Money duty=phaseb::mul_checked(goods_cost,ppm)/1'000'000;
        const Money total=phaseb::add_checked(goods_cost,duty);
        const std::string eid="G:"+route.id+":"+std::to_string(core.month());
        require(!trade_.executed.contains(eid),"G_ERR_DUPLICATE_EXECUTION");
        if(duty>0)(void)core.cash(to); // An actual importing treasury is needed.
        core.phase_d_reserve_cash(route.buyer,eid,total);
        core.phase_d_reserve_material(route.seller,origin,route.good,eid,qty);
        trade_.pending.push_back({eid,route.id,route.seller,route.buyer,route.good,
                         origin,dest,from,to,qty,route.travel_months,
                         price,total,duty});
    }
}
inline void Stage::execute_autonomous_trade() {
    at(7);
    auto& core=phase_c();
    for(const auto& order:trade_.pending) {
        require(trade_.executed.insert(order.execution_id).second,
                "G_ERR_DUPLICATE_EXECUTION");
        core.phase_d_release_cash(order.buyer,order.execution_id,
                                  order.total_cost);
        core.phase_g_release_reserved_goods(order.seller,order.origin_region,
                      order.good,order.execution_id,order.quantity);
        if(order.origin_region==order.destination_region) {
            core.trade(order.seller,order.buyer,order.origin_region,
                       order.good,order.quantity,order.unit_price);
        } else {
            require(order.travel_months>0,"G_ERR_UNSUPPORTED_TRANSPORT");
            core.ship(order.execution_id,order.seller,order.buyer,
                      order.origin_region,order.destination_region,
                      order.good,order.quantity,order.travel_months);
            core.pay(order.buyer,order.seller,
                     phaseb::mul_checked(order.unit_price,order.quantity));
        }
        if(order.duty>0)
            core.pay(order.buyer,order.buyer_country,order.duty);
        trade_.fills.push_back({order.execution_id,order.route_id,
            order.seller_country,order.buyer_country,core.month(),
            order.quantity,phaseb::mul_checked(order.unit_price,
                                               order.quantity),
            order.duty,
            order.origin_region!=order.destination_region});
        ++trade_.revision;
    }
    trade_.pending.clear();
}

} // namespace vic25::phasegh
