#pragma once
// Phase D bounded vertical integration with the Phase C atomic staging engine.
// phase-d-cpp/0.1: construction + production recipes + funded research +
// evidence-based knowledge activation + existing-site technique adoption.
// This deliberately does not claim all D-C/I/T acceptance cases are implemented.

#include "vic25/phase_c.hpp"
#include "vic25/day_budget.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <functional>
#include <map>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace vic25::phased {
using Money=phasec::Money;
using People=phaseb::People;
using Quantity=vic25::Quantity;
using Id=phaseb::Id;

inline void require(bool p,const char* why) {
    if(!p) throw std::invalid_argument(why);
}
inline std::int64_t product(std::int64_t a,std::int64_t b) {
    require(a>=0 && b>=0,"phase D negative factor");
    return phaseb::mul_checked(a,b);
}
enum class KnowledgeLevel : std::uint8_t { unknown, observed, understood, usable };
struct Input { std::string good; Quantity per_unit{}; };

struct ConstructionProject {
    std::string id, client, contractor, region, material_good, target_site;
    Quantity work_required{},material_per_work{},target_capacity{};
    People workers_per_work{};
    Money cash_price_per_work{};
    Quantity accepted_work{}, paid_work{}, capitalized_work{}, planned_work{};
    Money capitalized_value{};
    bool ready{}, commissioned{};
    std::int64_t ready_month{-1};
    bool permit_required{}, permit_valid{true};
    bool inspection_required{}, inspected{true};
    bool allow_accrued_invoice_commission{};
};
struct WorkPackage {
    std::string id, project, material_good;
    Quantity required_work{}, accepted_work{}, rejected_work{}, planned_work{};
    Quantity material_per_work{};
    People workers_per_work{};
    std::int64_t earliest_month{}, curing_months{}, finished_month{-1};
    std::vector<std::string> predecessors;
};
struct IndustryClass {
    std::string id, parent, description;
};
// An active extraction/harvest site. Underground reserve totals never exist here.
struct ResourceSite {
    std::string id,site,owner,region,good;
    Quantity monthly_capacity{};          // operational flow, NOT an underground stock
    Quantity produced_this_month{}, cumulative_output{}; // actual produced goods
    Quantity standing_biomass{}, renewal_per_month{}, carrying_capacity{}; // renewable ONLY
    People workers_per_unit{1};
    Quantity site_service_per_unit{1};
    std::uint32_t decline_ppm_active_month{}; // activity-based next-month throughput decline
    Quantity decline_remainder{};        // fractional ppm remainder, NOT reserve quantity
    bool renewable{};
};
struct IndustryRecipe {
    std::string id, site, tech_id, industry_class;
    std::vector<Input> inputs, outputs, byproducts;
    Quantity nonmarket_waste_per_batch{};
    People workers_per_batch{};
    Quantity capacity_per_batch{1};
    bool needs_retrofit{}; // retrofit pipeline remains unsupported by this slice
    bool requires_same_stage_nonstorable_energy{}; // I2-DISPATCH not yet contracted
};
struct ProductionRun {
    std::string recipe;
    std::int64_t month{};
    Quantity batches{};
};
struct ResearchProject {
    std::string id, holder, sponsor, lab_site, region, material_good, tech_id;
    Quantity material_per_work{}, required_evidence{}, verified_work{}, planned_work{};
    People workers_per_work{};
    Money cash_cost_per_work{};
    Quantity paid_work{};
    bool reviewed{};
};
struct Evidence {
    std::string project;
    std::int64_t month{};
    Quantity units{};
};
struct TechnologyDefinition {
    std::string id;
    std::vector<std::string> prerequisites;
    bool licensed_for_commercial_use{};
};
struct TechnologyLicense {
    std::string id,tech,grantor,grantee;
    Money actual_fee{};
    std::int64_t valid_from{},valid_until{-1};
    bool paid{};
};
struct KnowledgeContact {
    std::string id,source,target,tech;
    Quantity required_instruction{},verified_instruction{},paid_instruction{};
    People teachers_per_work{1};
    Money tuition_per_work{};
    Quantity planned_instruction{};
    bool reviewed{};
};
struct TrainingProgram {
    std::string id,trainer,sponsor;
    Id trainee_slice{};
    People trainee_count{};
    std::uint8_t target_occupational_skill{};
    Quantity required_service{},verified_service{},paid_service{},planned_service{};
    Money fee_per_service{};
    bool certified{};
    Id resulting_slice{};
};
struct KnowledgePromotion {
    std::string holder, tech;
    KnowledgeLevel next{KnowledgeLevel::usable};
};
struct Adoption {
    std::string site, owner, tech;
};

class World;
class Stage;

struct DomainState {
    std::map<std::string,ConstructionProject> constructions;
    std::map<std::string,WorkPackage> work_packages;
    std::map<std::string,IndustryClass> industry_classes;
    std::map<std::string,ResourceSite> resource_sites;
    std::map<std::string,Quantity> cumulative_nonmarket_waste;
    std::map<std::string,IndustryRecipe> recipes;
    std::vector<ProductionRun> actual_production_runs;
    std::map<std::string,ResearchProject> researches;
    std::map<std::string,TechnologyDefinition> technologies;
    std::map<std::string,TechnologyLicense> licenses;
    std::map<std::string,KnowledgeContact> contacts;
    std::map<std::string,TrainingProgram> training;
    std::map<std::pair<std::string,std::string>,KnowledgeLevel> knowledge;
    std::map<std::string,std::string> active_site_tech;
    std::map<std::int64_t,std::vector<KnowledgePromotion>> promotions;
    std::map<std::int64_t,std::vector<Adoption>> pending_adoptions;
    std::vector<Evidence> research_evidence;
    std::vector<std::string> journal;
    std::uint64_t event_sequence{};
    // Phase C's stage10 increments people_.month() before stage11 validation.
    // Keep the original opening month for Phase D stage11 effective dates.
    std::int64_t opening_month{-1};

    [[nodiscard]] bool licensed(const std::string& holder,
                                 const std::string& tech,
                                 std::int64_t effective_month)const {
        auto t=technologies.find(tech);
        if(t==technologies.end() || !t->second.licensed_for_commercial_use)
            return true;
        for(const auto& [id,license]:licenses){
            (void)id;
            if(license.tech==tech && license.grantee==holder&&license.paid &&
               license.valid_from<=effective_month &&
               (license.valid_until<0||license.valid_until>=effective_month))
                return true;
        }
        return false;
    }
    [[nodiscard]] KnowledgeLevel level(const std::string& holder,
                                         const std::string& tech) const {
        auto it=knowledge.find({holder,tech});
        return it==knowledge.end()?KnowledgeLevel::unknown:it->second;
    }
    void append(std::int64_t month,const char* kind,const std::string& entity) {
        journal.push_back("D:"+std::to_string(month)+":"+
                          std::to_string(++event_sequence)+":"+kind+":"+entity);
    }
    void begin_month(phasec::IntegratedWorld& c) {
        opening_month=c.month();
        c.phase_d_enable_shared_resources();
        for(auto& [id,source]:resource_sites) {
            if(!source.renewable && source.produced_this_month>0 &&
               source.decline_ppm_active_month>0) {
                // Throughput decline does not refer to a hidden finite reserve.
                constexpr Quantity scale=1'000'000;
                const auto whole=phaseb::mul_checked(source.monthly_capacity/scale,
                         static_cast<Quantity>(source.decline_ppm_active_month));
                const auto partial=phaseb::add_checked(
                     phaseb::mul_checked(source.monthly_capacity%scale,
                         static_cast<Quantity>(source.decline_ppm_active_month)),
                     source.decline_remainder);
                const auto decrease=phaseb::add_checked(whole,partial/scale);
                require(decrease<=source.monthly_capacity,"invalid resource flow decline");
                source.monthly_capacity-=decrease;
                source.decline_remainder=partial%scale;
                if(decrease>0)append(opening_month,"resource-flow-declined",id);
            }
            source.produced_this_month=0;
        }
        for(auto& [id,p]:constructions){(void)id;p.planned_work=0;}
        for(auto& [id,p]:work_packages){(void)id;p.planned_work=0;}
        for(auto& [id,p]:researches){(void)id;p.planned_work=0;}
        for(auto& [id,p]:contacts){(void)id;p.planned_instruction=0;}
        for(auto& [id,p]:training){(void)id;p.planned_service=0;}
        if(auto it=promotions.find(c.month());it!=promotions.end()) {
            for(const auto& u:it->second) {
                require(!u.holder.empty()&&!u.tech.empty(),"bad knowledge promotion");
                auto& prior=knowledge[{u.holder,u.tech}];
                if(static_cast<int>(u.next)>static_cast<int>(prior))prior=u.next;
                append(c.month(),"knowledge-effective",u.tech);
            }
            promotions.erase(it);
        }
        // Stage0 commissioning follows actual stage11 ready, and uses Phase C's
        // financial WIP and physical/Ownership facility authority.
        for(auto& [id,p]:constructions) {
            if(!p.ready||p.commissioned||p.ready_month>=c.month())continue;
            const Money actual_wip=c.phase_d_wip(p.client);
            require(actual_wip>=p.capitalized_value && p.capitalized_value>0,
                    "commission without financial WIP");
            c.phase_d_commission_facility(p.target_site,p.client,p.region,
                                         p.target_capacity,p.capitalized_value);
            p.commissioned=true;p.ready=false;
            append(c.month(),"facility-commissioned",id);
        }
        if(auto it=pending_adoptions.find(c.month());it!=pending_adoptions.end()) {
            for(const auto& a:it->second) {
                const auto& facility=c.facility(a.site);
                require(facility.owner==a.owner&&facility.capacity>facility.damaged &&
                        level(a.owner,a.tech)==KnowledgeLevel::usable &&
                        licensed(a.owner,a.tech,c.month()),
                        "tech adoption missing knowledge, license or usable asset");
                active_site_tech[a.site]=a.tech;
                append(c.month(),"technique-effective",a.site);
            }
            pending_adoptions.erase(it);
        }
    }
    // Historical standalone monthly fixtures keep their existing one-month
    // renewable allowance; the actual game never calls this legacy path.
    void regenerate_for_legacy_month() {
        for(auto& [id,source]:resource_sites) {
            if(!source.renewable)continue;
            require(source.carrying_capacity>=source.standing_biomass &&
                    source.renewal_per_month>=0,"invalid renewable pool");
            const auto grown=std::min(
                source.renewal_per_month,
                source.carrying_capacity-source.standing_biomass);
            if(grown<=0)continue;
            source.standing_biomass=phaseb::add_checked(
                source.standing_biomass,grown);
            append(opening_month,"natural-resource-regeneration",id);
        }
    }
    // Real renewable inflow becomes available incrementally each day, never
    // as a free whole-month grant at month open. Nonrenewable throughput
    // deterioration remains based on the preceding closed month's activity.
    void regenerate_for_day(int day,int days_in_month) {
        for(auto& [id,source]:resource_sites) {
            if(!source.renewable)continue;
            require(source.carrying_capacity>=source.standing_biomass &&
                    source.renewal_per_month>=0,"invalid renewable pool");
            const auto due=daily_budget::increment(
                source.renewal_per_month,day,days_in_month);
            const auto grown=std::min(
                due,source.carrying_capacity-source.standing_biomass);
            if(grown<=0)continue;
            source.standing_biomass=phaseb::add_checked(source.standing_biomass,grown);
            append(opening_month,"daily-resource-regeneration",id);
        }
    }
    // Construction efficiency changes the paid work needed to commission
    // an agricultural facility; the site's original capacity remains the
    // same and every completed work unit still consumes real materials and
    // receives real payment. Packaged works retain explicit stage contracts.
    [[nodiscard]] Quantity effective_construction_work(
            const ConstructionProject& p,
            const phasec::IntegratedWorld& c)const {
        if(p.work_required<=0)return p.work_required;
        // Most countries do not activate this specific amendment. Short
        // circuit before scanning work packages and global industry classes.
        const auto law=std::max(0.0,c.p2_region_modifier_value(
            p.region,
            "building_group_bg_agriculture_construction_efficiency_add"));
        if(law<=0)return p.work_required;
        for(const auto& [id,work]:work_packages) {
            (void)id;
            if(work.project==p.id)return p.work_required;
        }
        bool agriculture=false;
        for(const auto& [id,r]:recipes) {
            (void)id;
            if(r.site!=p.target_site)continue;
            std::string kind=r.industry_class;
            std::set<std::string> visited;
            for(int depth=0;depth<12&&!kind.empty()&&
                    visited.insert(kind).second;++depth) {
                if(kind=="agriculture"||kind=="farming"||
                   kind=="farm") {agriculture=true;break;}
                auto parent=industry_classes.find(kind);
                if(parent==industry_classes.end())break;
                kind=parent->second.parent;
            }
            if(agriculture)break;
        }
        if(!agriculture)return p.work_required;
        return static_cast<Quantity>(std::clamp(
            std::ceil(static_cast<long double>(p.work_required)/
                      (1.0L+static_cast<long double>(law))),
            1.0L,static_cast<long double>(p.work_required)));
    }
    void finalize(const phasec::IntegratedWorld& c) {
        for(auto& [id,p]:constructions) {
            require(p.work_required>0 && p.accepted_work>=0 &&
                    p.accepted_work<=p.work_required && p.paid_work<=p.capitalized_work &&
                    p.capitalized_work<=p.accepted_work,
                    "construction work/finance over-allocation");
            if(!p.allow_accrued_invoice_commission)
                require(p.paid_work==p.capitalized_work,
                        "unpaid construction requires an explicit title/credit policy");
            Quantity package_work=0, package_required=0;
            for(const auto& [package_id,wp]:work_packages) {
                (void)package_id;
                if(wp.project!=id)continue;
                require(wp.required_work>0&&wp.material_per_work>0&&wp.workers_per_work>0&&
                        wp.accepted_work>=0&&wp.accepted_work<=wp.required_work &&
                        wp.rejected_work>=0,
                        "invalid work-package quantities");
                for(const auto& prerequisite:wp.predecessors)
                    require(work_packages.contains(prerequisite) &&
                            work_packages.at(prerequisite).project==id,
                            "missing or cross-project predecessor");
                package_work=phaseb::add_checked(package_work,wp.accepted_work);
                package_required=phaseb::add_checked(package_required,wp.required_work);
            }
            if(package_required)
                require(package_required==p.work_required &&
                        package_work==p.accepted_work,
                        "work-package sum disagrees with project");
            require(p.capitalized_value==product(p.capitalized_work,p.cash_price_per_work),
                    "WIP record does not match certified work");
            if(!p.commissioned && !p.ready &&
               p.accepted_work>=effective_construction_work(p,c) &&
               p.capitalized_work==p.accepted_work &&
               (!p.permit_required||p.permit_valid) &&
               (!p.inspection_required||p.inspected)) {
                p.ready=true;
                p.ready_month=opening_month;
                append(opening_month,"construction-ready",id);
            }
            if(p.commissioned) {
                const auto& facility=c.facility(p.target_site);
                require(facility.owner==p.client && facility.region==p.region &&
                        facility.capacity==p.target_capacity,
                        "commissioned facility and project differ");
            }
        }
        std::map<std::string,Money> outstanding;
        for(const auto& [id,p]:constructions){
            (void)id;
            if(!p.commissioned)
                outstanding[p.client]=phaseb::add_checked(outstanding[p.client],
                                                           p.capitalized_value);
        }
        for(const auto& [owner,amount]:outstanding)
            require(c.phase_d_wip(owner)>=amount,"unbacked project WIP");
        for(const auto& [id,source]:resource_sites) {
            require(id==source.id && source.monthly_capacity>=0 &&
                    source.produced_this_month>=0 &&
                    source.produced_this_month<=source.monthly_capacity &&
                    source.cumulative_output>=source.produced_this_month &&
                    source.workers_per_unit>0 && source.site_service_per_unit>0 &&
                    source.decline_ppm_active_month<=1'000'000 &&
                    source.decline_remainder>=0 && source.decline_remainder<1'000'000 &&
                    (source.renewable ?
                       (source.carrying_capacity>0 && source.standing_biomass>=0 &&
                        source.carrying_capacity>=source.standing_biomass &&
                        source.renewal_per_month>=0 &&
                        source.decline_ppm_active_month==0 && source.decline_remainder==0)
                       : (source.standing_biomass==0 && source.renewal_per_month==0 &&
                          source.carrying_capacity==0)),
                    "invalid resource site flow/renewable conservation");
        }
        for(const auto& [id,cl]:industry_classes) {
            require(id==cl.id && (cl.parent.empty()||
                    industry_classes.contains(cl.parent)),
                    "unknown industrial taxonomy ancestry");
        }
        for(const auto& [id,v]:cumulative_nonmarket_waste){
            (void)id;require(v>=0,"negative accumulated nonmarket waste");
        }
        for(const auto& [id,tech]:technologies) {
            require(tech.id==id,"technical knowledge identity mismatch");
            for(const auto& prior:tech.prerequisites)
                require(technologies.contains(prior)&&prior!=id,
                        "unknown technology prerequisite");
        }
        for(const auto& [id,license]:licenses) {
            require(license.id==id&&!license.tech.empty() &&
                    license.actual_fee>=0&&
                    (license.valid_until<0||license.valid_until>=license.valid_from),
                    "invalid technology license/term");
        }
        for(const auto& [id,contact]:contacts) {
            require(id==contact.id && contact.required_instruction>0 &&
                    contact.verified_instruction>=0 &&
                    contact.paid_instruction<=contact.verified_instruction,
                    "invalid contact training evidence");
        }
        for(const auto& [id,p]:training) {
            require(id==p.id&&p.required_service>0 && p.verified_service>=0 &&
                    p.paid_service<=p.verified_service,
                    "invalid POP occupational training evidence");
        }
        for(const auto& [id,p]:researches) {
            require(p.verified_work>=0&&p.paid_work<=p.verified_work,
                    "unpaid or negative research evidence");
            Quantity sum=0;
            for(const auto& event:research_evidence)
                if(event.project==id)sum=phaseb::add_checked(sum,event.units);
            require(sum==p.verified_work,"orphaned research progress");
        }
    }
};

// Research-rate law effects reduce the *required funded evidence*; they do
// not mint fictitious scientists, goods, cash or completed research work.
// Speed is evaluated at review time, so repealing a reform also matters.
// A throughput law changes the yields from *verified paid-material and labor
// batches*, not the plant capacity, not the market quote, and not a phantom
// free source. Cumulative rounding makes ten one-unit batches with a +10% law
// yield exactly eleven units, even across separate daily production orders.
inline Quantity law_verified_yield(Quantity before,Quantity produced,
                                  double throughput) {
    require(before>=0&&produced>=0,"negative industry yield units");
    const auto cumulative=phaseb::add_checked(before,produced);
    const long double multiplier=std::clamp(
        1.0L+static_cast<long double>(throughput),0.05L,4.0L);
    const auto adjusted=[&](Quantity n)->Quantity {
        const long double total=std::floor(static_cast<long double>(n)*
                                            multiplier);
        require(total>=0&&total<=static_cast<long double>(
                   std::numeric_limits<Quantity>::max()/4),
                "law throughput overflows physical goods");
        return static_cast<Quantity>(total);
    };
    return adjusted(cumulative)-adjusted(before);
}
// Industry-class parents are explicit Phase D taxonomy: do not assign a
// plantation/farm law to an unknown mine from the name of its output good.
inline double law_industry_throughput(const DomainState& state,
        const phasec::IntegratedWorld& c,const IndustryRecipe& recipe) {
    const auto& facility=c.facility(recipe.site);
    const auto& country=c.sovereignty(facility.region);
    std::string current=recipe.industry_class;
    std::set<std::string> visited;
    double bonus=0;
    for(int depth=0;depth<12&&!current.empty()&&
            visited.insert(current).second;++depth) {
        const auto alias=current=="manufacture"?"manufacturing":
                         current=="mining"?"extraction":current;
        if(alias=="manufacturing"||alias=="agriculture"||
           alias=="plantations"||alias=="extraction"||
           alias=="ranching"||alias=="logging")
            bonus+=c.p2_catalog_modifier_value(country,
                "building_group_bg_"+alias+"_throughput_add");
        const auto parent=state.industry_classes.find(current);
        if(parent==state.industry_classes.end())break;
        current=parent->second.parent;
    }
    return std::clamp(bonus,-0.95,3.0);
}
inline Quantity effective_research_evidence(
        const ResearchProject& p,const DomainState& d,
        const phasec::IntegratedWorld& c) {
    if(p.required_evidence<=0)return p.required_evidence;
    const auto& country=c.sovereignty(p.region);
    if(!c.has_government(country))return p.required_evidence;
    double speed=c.p2_catalog_modifier_value(
        country,"country_tech_research_speed_mult");
    const bool production=std::any_of(d.recipes.begin(),d.recipes.end(),
        [&](const auto& entry){return entry.second.tech_id==p.tech_id;});
    if(production)
        speed+=c.p2_catalog_modifier_value(
            country,"country_production_tech_research_speed_mult");
    const double multiplier=std::clamp(1.0+speed,0.25,4.0);
    return static_cast<Quantity>(std::clamp(
        std::ceil(static_cast<double>(p.required_evidence)/multiplier),
        1.0,static_cast<double>(
            std::numeric_limits<Quantity>::max()/4)));
}

inline Quantity effective_contact_instruction(
        const KnowledgeContact& contact,const DomainState& d,
        const phasec::IntegratedWorld& c) {
    std::string country;
    if(c.has_government(contact.target))country=contact.target;
    else {
        for(const auto& [id,site]:c.facilities()) {
            (void)id;
            if(site.owner!=contact.target)continue;
            const auto sovereign=c.sovereignty(site.region);
            if(country.empty())country=sovereign;
            else if(country!=sovereign)return contact.required_instruction;
        }
    }
    if(country.empty()||!c.has_government(country))
        return contact.required_instruction;
    double speed=c.p2_catalog_modifier_value(
        country,"country_tech_spread_mult");
    const bool production=std::any_of(d.recipes.begin(),d.recipes.end(),
        [&](const auto& entry){return entry.second.tech_id==contact.tech;});
    if(production)
        speed+=c.p2_catalog_modifier_value(
            country,"country_production_tech_spread_mult");
    const double multiplier=std::clamp(1.0+speed,0.25,4.0);
    return static_cast<Quantity>(std::clamp(
        std::ceil(static_cast<double>(contact.required_instruction)/multiplier),
        1.0,static_cast<double>(
            std::numeric_limits<Quantity>::max()/4)));
}

class Stage {
    phasec::IntegratedWorld& c_;
    DomainState& d_;
    int phase_;
    void must(int n) const {require(phase_==n,"phase D event in wrong stage");}
    void occupy_labor(const std::string& owner,People people) {
        c_.phase_d_use_labor(owner,people);
    }
    void occupy_site(const std::string& site,Quantity units) {
        c_.phase_d_use_facility(site,units);
    }
public:
    Stage(phasec::IntegratedWorld& c,DomainState& d,int stage)
        :c_(c),d_(d),phase_(stage){}
    [[nodiscard]] int phase() const {return phase_;}
    [[nodiscard]] std::int64_t month() const {return d_.opening_month;}
    [[nodiscard]] phasec::IntegratedWorld& phase_c() {return c_;}
    [[nodiscard]] const DomainState& state() const {return d_;}
    void command_once(const std::string& external_id,
                      const std::function<void(Stage&)>& operation) {
        require(!external_id.empty() && static_cast<bool>(operation),
                "invalid Phase D external command");
        c_.command_once("PHASE_D:"+external_id,
            [&](phasec::IntegratedWorld&){operation(*this);});
    }

    // Endogenous enterprise investment reuses an existing fully paid and
    // commissioned construction design. The new project is merely a plan:
    // no factory, labor, money or inventory appears until real work is paid.
    // Permit-sensitive or unsupported high-tech replicas cannot silently
    // acquire regulatory permissions or technology.
    bool propose_firm_expansion(const std::string& template_project,
                                const std::string& template_recipe,
                                const std::string& new_id,
                                Quantity minimum_untapped_cash) {
        must(9);
        auto old=d_.constructions.find(template_project);
        auto ri=d_.recipes.find(template_recipe);
        if(old==d_.constructions.end()||ri==d_.recipes.end()||
           !old->second.commissioned||old->second.permit_required||
           old->second.inspection_required||old->second.allow_accrued_invoice_commission||
           !ri->second.tech_id.empty()||ri->second.needs_retrofit||
           ri->second.requires_same_stage_nonstorable_energy||
           ri->second.site!=old->second.target_site||
           c_.firm_bankrupt(old->second.client)||
           c_.firm_bankrupt(old->second.contractor))return false;
        const auto& model=old->second;
        const auto& recipe=ri->second;
        // Under a command economy there is no autonomous private
        // investment pool. Explicitly voted government construction,
        // still financed from the real treasury, remains lawful.
        if(c_.p2_catalog_modifier_value(c_.sovereignty(model.region),
                "country_disable_investment_pool_bool")>0)return false;
        if(model.work_required<=0||model.cash_price_per_work<=0||
           recipe.outputs.empty()||model.material_per_work<=0||
           model.workers_per_work<=0)return false;
        const auto good=recipe.outputs.front().good;
        if(!c_.has_market(model.region,good))return false;
        const auto& quote=c_.market(model.region,good);
        // Demand alone is not enough: the observed sales value of a real
        // production recipe must exceed the quoted actual input costs.
        if(quote.unmet<=0)return false;
        // Statutory self-investment likelihood applies only to a real,
        // profitable firm with demand for its existing output. The FNV-1a
        // selection is reproducible across platforms and reloads: it is
        // not a separate mutable random state or fictional investor.
        const auto country=c_.sovereignty(model.region);
        if(c_.p2_catalog_group_active(country,"economic_system")) {
            const auto chance=std::clamp(
                0.5+c_.p2_catalog_modifier_value(
                    country,"building_self_investment_chance_add"),
                0.0,1.0);
            std::uint64_t hash=14695981039346656037ULL;
            const std::string stable=model.client+":"+new_id;
            for(const unsigned char ch:stable) {
                hash^=ch;
                hash*=1099511628211ULL;
            }
            const auto roll=static_cast<double>(
                hash%1'000'000ULL)/1'000'000.0;
            if(roll>=chance)return false;
        }
        // Anti-monopoly rules count only real operational capacity of actual
        // firms making this good in this region. No market-share proxy,
        // fictitious competing enterprise or quota ledger is created.
        if(c_.p2_catalog_modifier_value(c_.sovereignty(model.region),
                  "country_forbid_monopoly_bool")>0) {
            Quantity total=0,owned=0;
            std::set<std::string> counted;
            for(const auto& [rid,other]:d_.recipes) {
                (void)rid;
                if(other.outputs.empty()||
                   other.outputs.front().good!=good||
                   !c_.facilities().contains(other.site)||
                   !counted.insert(other.site).second)continue;
                const auto& facility=c_.facility(other.site);
                if(facility.region!=model.region)continue;
                const Quantity capacity=std::max<Quantity>(
                    0,facility.capacity-facility.damaged);
                total=phaseb::add_checked(total,capacity);
                if(facility.owner==c_.facility(recipe.site).owner)
                    owned=phaseb::add_checked(owned,capacity);
            }
            if(total>0&&owned>total/2)return false;
        }
        const auto& output=recipe.outputs.front();
        const Money revenue=product(output.per_unit,quote.price);
        Money inputs=0;
        for(const auto& item:recipe.inputs) {
            if(!c_.has_market(model.region,item.good))return false;
            inputs=phaseb::add_checked(inputs,
                product(item.per_unit,
                        c_.market(model.region,item.good).price));
        }
        if(revenue<=inputs ||
           c_.phase_d_workforce(model.contractor)<model.workers_per_work)
            return false;
        const Money cost=product(model.work_required,model.cash_price_per_work);
        if(c_.phase_d_cash_available(model.client)<
           phaseb::add_checked(cost,minimum_untapped_cash))
            return false;
        const Quantity stock=c_.phase_d_material_available(
            model.contractor,model.region,model.material_good);
        if(stock<model.material_per_work)return false;
        // The work model is copied, but no old payment, ownership, permit or
        // financed progress is inherited.
        const auto id="AUTO:PROJECT:"+new_id;
        const auto site="AUTO:SITE:"+new_id;
        const auto recipe_id="AUTO:RECIPE:"+new_id;
        if(d_.constructions.contains(id)||d_.recipes.contains(recipe_id))
            return false;
        const auto charter_bonus=std::clamp(
            static_cast<int>(c_.p2_catalog_modifier_value(
                country,"country_free_charters_add")),0,5);
        int other_open=0;
        for(const auto& [project_id,p]:d_.constructions) {
            (void)project_id;
            if(!p.commissioned&&p.client==model.client&&
               p.region==model.region&&p.target_site!=model.target_site)
                ++other_open;
        }
        // Legal charters only increase the concurrent number of real,
        // fully financed construction filings; they do not spawn companies.
        if(other_open>=1+charter_bonus)return false;
        auto project=model;
        project.id=id;project.target_site=site;
        project.accepted_work=0;project.paid_work=0;
        project.capitalized_work=0;project.planned_work=0;
        project.capitalized_value=0;project.ready=false;
        project.commissioned=false;project.ready_month=-1;
        project.permit_valid=true;project.inspected=true;
        auto production=recipe;
        production.id=recipe_id;production.site=site;
        d_.constructions.emplace(id,std::move(project));
        d_.recipes.emplace(recipe_id,std::move(production));
        d_.append(c_.month(),"endogenous-funded-factory-project",id);
        return true;
    }

    // Only a commissioned industry site without a separate extraction
    // concession, active laboratory contract or unfinished development may
    // change hands through a simple cash sale. All others require their
    // dedicated legal transfer process, not silent ownership substitution.
    bool liquidate_commissioned_factory(const std::string& site,
                                       const std::string& buyer,Money price) {
        must(7);
        if(price<=0||!c_.has_actor(buyer)||!d_.constructions.size())return false;
        const auto& facility=c_.facility(site);
        const auto& sovereign=c_.sovereignty(facility.region);
        const bool liquidating=c_.firm_bankrupt(facility.owner);
        const bool forced_private=
            c_.has_country_treasury(sovereign)&&
            facility.owner==c_.country_treasury(sovereign)&&
            c_.p2_catalog_modifier_value(sovereign,
                  "country_force_privatization_bool")>0;
        // A non-company public asset cannot be quietly privatized when
        // national economic law explicitly reserves it.
        if(c_.p2_catalog_modifier_value(sovereign,
                  "country_disable_non_company_privatization_bool")>0&&
           !c_.p2_has_registered_company_equity(facility.owner))
            return false;
        if((!liquidating&&!forced_private)||
           buyer==facility.owner||c_.firm_bankrupt(buyer))return false;
        std::string project_id;
        for(const auto& [id,project]:d_.constructions) {
            if(project.target_site!=site)continue;
            if(!project.commissioned||!project_id.empty())
                return false;
            project_id=id;
        }
        if(project_id.empty())return false;
        for(const auto& [id,source]:d_.resource_sites) {
            (void)id;
            if(source.site==site)return false;
        }
        for(const auto& [id,research]:d_.researches) {
            (void)id;
            if(research.lab_site==site&&!research.reviewed)return false;
        }
        if(c_.phase_d_cash_available(buyer)<price)return false;
        c_.phase_d_transfer_factory_paid(
            site,buyer,price,
            d_.constructions.at(project_id).capitalized_value);
        d_.constructions.at(project_id).client=buyer;
        d_.append(c_.month(),forced_private?
            "lawful-funded-factory-privatization":
            "cash-real-factory-liquidation",site);
        return true;
    }

    // A workers' cooperative is an actual share purchase in the existing
    // C OwnershipLedger. A statute merely permits the transaction: without
    // a documented seller, employed worker, shares and cash nothing moves.
    bool purchase_collective_shares(const std::string& site,
                                    const std::string& seller,
                                    const std::string& worker_household,
                                    Money units) {
        must(7);
        if(units<=0||seller==worker_household||
           !c_.has_actor(worker_household)||
           !c_.has_actor(seller)||!c_.facilities().contains(site))
            return false;
        const auto& facility=c_.facility(site);
        const auto& country=c_.sovereignty(facility.region);
        if(!c_.has_government(country)||c_.firm_bankrupt(facility.owner))
            return false;
        const auto asset="company:"+facility.owner;
        if(!c_.p2_has_registered_company_equity(facility.owner)||
           c_.shares(asset,seller)<units||
           c_.phase_d_cash_available(worker_household)<units)
            return false;
        bool real_worker=false;
        for(const auto& [slice,job]:c_.people().jobs()) {
            if(job.count<=0||job.employer!=facility.owner||
               !c_.people().has_slice(slice))continue;
            const auto& row=c_.people().slice(slice);
            const auto& dem=c_.people().demographic(row.key.demographic);
            if(dem.region==facility.region&&
               worker_household=="HH:"+
                   std::to_string(row.key.household)) {
                real_worker=true;break;
            }
        }
        if(!real_worker)return false;
        bool allowed=false;
        for(const auto& [id,recipe]:d_.recipes) {
            (void)id;
            if(recipe.site!=site)continue;
            std::string kind=recipe.industry_class;
            std::set<std::string> visited;
            for(int depth=0;depth<12&&!kind.empty()&&
                  visited.insert(kind).second;++depth) {
                const auto group=kind=="manufacture"?"manufacturing":
                    kind=="mining"?"extraction":kind;
                if((group=="manufacturing"||group=="extraction"||
                    group=="power"||group=="private_infrastructure"||
                    group=="arts")&&
                   c_.p2_catalog_modifier_value(country,
                       "building_group_bg_"+group+
                       "_allowed_collectivization_add")>0)
                    allowed=true;
                const auto ancestor=d_.industry_classes.find(kind);
                if(ancestor==d_.industry_classes.end())break;
                kind=ancestor->second.parent;
            }
        }
        if(!allowed)return false;
        bool foreign_seller=c_.has_government(seller)&&seller!=country;
        if(!foreign_seller)
            for(const auto& [id,f]:c_.facilities()) {
                (void)id;
                if(f.owner==seller&&
                   c_.sovereignty(f.region)!=country)
                    foreign_seller=true;
            }
        // Household titles are foreign when their real POP holders reside
        // under another sovereign government. Never guess an investor's
        // nationality solely from an arbitrary actor ID.
        if(!foreign_seller&&seller.rfind("HH:",0)==0)
            for(const auto& [id,pop]:c_.people().slices()) {
                (void)id;
                if(pop.count<=0||
                   seller!="HH:"+std::to_string(pop.key.household))
                    continue;
                const auto& dem=c_.people().demographic(
                    pop.key.demographic);
                if(c_.sovereignty(dem.region)!=country) {
                    foreign_seller=true;break;
                }
            }
        if(foreign_seller&&
           c_.p2_catalog_modifier_value(
               country,"country_foreign_collectivization_bool")<=0)
            return false;
        // C sell_shares charges the real worker 1 monetary unit per share
        // and transfers the matching registered security and company title.
        c_.sell_shares(asset,seller,worker_household,units);
        d_.append(c_.month(),"funded-worker-cooperative-share-purchase",
                  site);
        return true;
    }
    // Cash reservation is now owned by the SAME Phase C authority used by
    // legacy payments, wages, transactions and Phase D activity planning.
    // Payments must actually settle before capitalisation or validation.
    void plan_work(const std::string& project,Quantity units) {
        must(1);
        auto& p=d_.constructions.at(project);
        require(units>0 && p.accepted_work+units<=p.work_required &&
                !p.ready&&!p.commissioned &&
                (!p.permit_required||p.permit_valid),
                "invalid construction plan, permit or physical scope");
        if(!p.allow_accrued_invoice_commission)
            c_.phase_d_reserve_cash(p.client,"construction:"+project,
                                     product(units,p.cash_price_per_work));
        p.planned_work=phaseb::add_checked(p.planned_work,units);
    }
    void plan_package(const std::string& id,Quantity attempt_units) {
        must(1);
        auto& wp=d_.work_packages.at(id);
        auto& p=d_.constructions.at(wp.project);
        require(attempt_units>0 &&
                wp.accepted_work+wp.planned_work+attempt_units<=wp.required_work &&
                d_.opening_month>=wp.earliest_month &&
                (!p.permit_required||p.permit_valid),
                "cannot schedule unauthorized or overfilled work package");
        for(const auto& pred:wp.predecessors){
            const auto& other=d_.work_packages.at(pred);
            require(other.accepted_work==other.required_work &&
                    other.finished_month>=0 &&
                    other.finished_month+other.curing_months<=d_.opening_month,
                    "work-package predecessor not complete or cured");
        }
        plan_work(wp.project,attempt_units);
        wp.planned_work=phaseb::add_checked(wp.planned_work,attempt_units);
    }
    void perform_package(const std::string& id,Quantity attempted,Quantity rejected=0) {
        must(4);
        auto& wp=d_.work_packages.at(id);
        auto& p=d_.constructions.at(wp.project);
        require(attempted>0 && rejected>=0 && rejected<=attempted &&
                attempted<=wp.planned_work && attempted<=p.planned_work &&
                wp.accepted_work+attempted-rejected<=wp.required_work,
                "invalid construction work-package attempt");
        for(const auto& pred:wp.predecessors){
            const auto& other=d_.work_packages.at(pred);
            require(other.accepted_work==other.required_work &&
                    other.finished_month+other.curing_months<=d_.opening_month,
                    "physical predecessor/cure not complete");
        }
        const auto required_material=product(attempted,wp.material_per_work);
        require(c_.phase_d_material_available(p.contractor,p.region,wp.material_good)>=required_material,
                "undelivered work-package material");
        occupy_labor(p.contractor,product(attempted,wp.workers_per_work));
        c_.phase_d_use_material(p.contractor,p.region,wp.material_good,required_material);
        wp.planned_work-=attempted; p.planned_work-=attempted;
        wp.rejected_work=phaseb::add_checked(wp.rejected_work,rejected);
        const auto accepted=attempted-rejected;
        wp.accepted_work=phaseb::add_checked(wp.accepted_work,accepted);
        p.accepted_work=phaseb::add_checked(p.accepted_work,accepted);
        if(wp.accepted_work==wp.required_work)wp.finished_month=d_.opening_month;
        d_.append(d_.opening_month,rejected?"construction-quality-rework":"construction-package-accepted",id);
    }
    void approve_inspection(const std::string& project){
        must(7);
        auto& p=d_.constructions.at(project);
        require(p.accepted_work==p.work_required &&
                (!p.permit_required||p.permit_valid) &&
                !p.commissioned,"cannot inspect unfinished or unauthorized project");
        p.inspected=true;
        d_.append(d_.opening_month,"construction-inspection",project);
    }
    void perform_work(const std::string& project,Quantity units) {
        must(4);
        auto& p=d_.constructions.at(project);
        for(const auto& [id,wp]:d_.work_packages) {
            (void)id;
            require(wp.project!=project,"packaged projects must use actual WorkPackage work");
        }
        require(units>0 && p.planned_work>=units &&
                p.accepted_work+units<=p.work_required,"construction plan exhausted");
        const auto real_stock=product(units,p.material_per_work);
        const auto staff=product(units,p.workers_per_work);
        require(c_.stock(p.contractor,p.region,p.material_good)>=real_stock,
                "construction requires delivered material");
        occupy_labor(p.contractor,staff);
        c_.phase_d_use_material(p.contractor,p.region,p.material_good,real_stock);
        p.accepted_work=phaseb::add_checked(p.accepted_work,units);
        p.planned_work-=units;
        d_.append(c_.month(),"actual-construction-work",project);
    }
    void pay_work(const std::string& project,Quantity units) {
        must(5);
        auto& p=d_.constructions.at(project);
        require(units>0&&p.paid_work+units<=p.accepted_work,
                "cannot pay uncertified construction units");
        const auto amount=product(units,p.cash_price_per_work);
        c_.phase_d_release_cash(p.client,"construction:"+project,amount);
        c_.pay(p.client,p.contractor,amount);
        p.paid_work=phaseb::add_checked(p.paid_work,units);
        d_.append(c_.month(),"work-paid",project);
    }
    void accrue_work_invoice(const std::string& project,Quantity units) {
        must(8);
        auto& p=d_.constructions.at(project);
        require(p.allow_accrued_invoice_commission && units>0 &&
                p.capitalized_work+units<=p.accepted_work,
                "unpaid invoicing requires explicit credit and certified work");
        const auto amount=product(units,p.cash_price_per_work);
        c_.phase_d_accrue_certified_work(p.client,p.contractor,amount);
        p.capitalized_work=phaseb::add_checked(p.capitalized_work,units);
        p.capitalized_value=phaseb::add_checked(p.capitalized_value,amount);
        d_.append(d_.opening_month,"contractor-claim-accrued",project);
    }
    void settle_work_invoice(const std::string& project,Quantity units) {
        must(8);
        auto& p=d_.constructions.at(project);
        require(units>0 && p.paid_work+units<=p.capitalized_work,
                "unbacked contractor invoice settlement");
        const auto amount=product(units,p.cash_price_per_work);
        c_.phase_d_settle_certified_work(p.client,p.contractor,amount);
        p.paid_work=phaseb::add_checked(p.paid_work,units);
        d_.append(d_.opening_month,"contractor-claim-settled",project);
    }
    void capitalize_work(const std::string& project,Quantity units) {
        must(8);
        auto& p=d_.constructions.at(project);
        require(units>0&&p.capitalized_work+units<=p.paid_work,
                "construction capitalized without paid verified work");
        const auto amount=product(units,p.cash_price_per_work);
        c_.phase_d_capitalize_paid_cost(p.client,amount);
        p.capitalized_work=phaseb::add_checked(p.capitalized_work,units);
        p.capitalized_value=phaseb::add_checked(p.capitalized_value,amount);
        d_.append(c_.month(),"capitalized-wip",project);
    }
    void extract_resource(const std::string& id,Quantity units){
        must(4);
        auto& source=d_.resource_sites.at(id);
        require(units>0 && source.produced_this_month<=source.monthly_capacity &&
                units<=source.monthly_capacity-source.produced_this_month,
                "extraction exceeds current monthly operational flow");
        require(!source.renewable || units<=source.standing_biomass,
                "harvest exceeds current renewable biomass");
        const auto& site=c_.facility(source.site);
        require(site.owner==source.owner&&site.region==source.region,
                "extraction has no effective owned extraction site");
        occupy_labor(source.owner,product(units,source.workers_per_unit));
        occupy_site(source.site,product(units,source.site_service_per_unit));
        // Extraction output is boosted from a verified deposit/harvest flow.
        // The depleted biomass and extraction service remain the *base* units.
        // Production is accounted in the original C goods ledger only once.
        const double throughput=c_.p2_catalog_modifier_value(
            c_.sovereignty(source.region),
            "building_group_bg_extraction_throughput_add");
        const auto yielded=law_verified_yield(
            source.produced_this_month,units,throughput);
        require(yielded>0,"law extraction yield is zero");
        c_.phase_d_emit_extraction(source.owner,source.region,source.good,
                                   yielded,id);
        source.produced_this_month=phaseb::add_checked(source.produced_this_month,units);
        source.cumulative_output=phaseb::add_checked(source.cumulative_output,yielded);
        if(source.renewable)source.standing_biomass-=units;
        d_.append(d_.opening_month,"resource-extracted",id);
    }
    // Native resource extraction is bounded by both the source's observed
    // monthly flow and the shared physical facility's prorated capacity.
    // Neither is an underground total reserve or a hidden depletion stock.
    void extract_resource_daily(const std::string& id,Quantity units,
                                std::int32_t day,std::int32_t days_in_month){
        must(4);
        require(day>0&&days_in_month>=28&&days_in_month<=31&&
                day<=days_in_month&&units>0,"D_DAILY_INVALID_EXTRACTION_DATE");
        const auto& source=d_.resource_sites.at(id);
        const auto& site=c_.facility(source.site);
        require(site.capacity>=site.damaged,"D_DAILY_DAMAGED_EXTRACTION_SITE");
        const auto prorate=[&](Quantity cap){
            return phaseb::add_checked(
                phaseb::mul_checked(cap/days_in_month,day),
                phaseb::mul_checked(cap%days_in_month,day)/days_in_month);
        };
        const Quantity source_limit=prorate(source.monthly_capacity);
        require(source.produced_this_month<=source_limit&&
                units<=source_limit-source.produced_this_month,
                "D_DAILY_RESOURCE_FLOW_BUDGET_EXCEEDED");
        Quantity used_site=0;
        for(const auto& run:d_.actual_production_runs) {
            if(run.month!=d_.opening_month)continue;
            const auto& recipe=d_.recipes.at(run.recipe);
            if(recipe.site==source.site)
                used_site=phaseb::add_checked(used_site,
                    phaseb::mul_checked(run.batches,recipe.capacity_per_batch));
        }
        for(const auto& [site_id,other]:d_.resource_sites) {
            (void)site_id;
            if(other.site==source.site)
                used_site=phaseb::add_checked(used_site,
                    phaseb::mul_checked(other.produced_this_month,
                                        other.site_service_per_unit));
        }
        for(const auto& evidence:d_.research_evidence) {
            if(evidence.month!=d_.opening_month)continue;
            if(d_.researches.at(evidence.project).lab_site==source.site)
                used_site=phaseb::add_checked(used_site,evidence.units);
        }
        const Quantity cap=prorate(site.capacity-site.damaged);
        const Quantity needed=phaseb::mul_checked(units,source.site_service_per_unit);
        require(used_site<=cap&&needed<=cap-used_site,
                "D_DAILY_SHARED_SITE_FLOW_EXCEEDED");
        extract_resource(id,units);
    }
    void manufacture(const std::string& recipe,Quantity batches) {
        must(4);
        const auto& r=d_.recipes.at(recipe);
        const auto& f=c_.facility(r.site);
        require(batches>0 && f.owner.size()>0 &&
                (r.tech_id.empty() ||
                 (d_.active_site_tech.contains(r.site) &&
                  d_.active_site_tech.at(r.site)==r.tech_id)),
                "manufacture requires installed and adopted technique");
        require(!r.needs_retrofit,"phase D recipe retrofit not commissioned");
        require(!r.requires_same_stage_nonstorable_energy,
                "I2-DISPATCH nonstorable same-stage energy unsupported");
        const auto capacity=product(batches,r.capacity_per_batch);
        const auto workers=product(batches,r.workers_per_batch);
        for(const auto& x:r.inputs)
            require(c_.stock(f.owner,f.region,x.good)>=product(batches,x.per_unit),
                    "production ingredient not delivered");
        occupy_labor(f.owner,workers);
        occupy_site(r.site,capacity);
        const double throughput=law_industry_throughput(d_,c_,r);
        Quantity previous=0;
        for(const auto& run:d_.actual_production_runs)
            if(run.month==d_.opening_month&&run.recipe==recipe)
                previous=phaseb::add_checked(previous,run.batches);
        std::vector<std::pair<std::string,Quantity>> inputs,outputs;
        for(const auto& x:r.inputs)
            inputs.emplace_back(x.good,product(batches,x.per_unit));
        for(const auto& x:r.outputs) {
            const auto before=product(previous,x.per_unit);
            const auto now=product(batches,x.per_unit);
            const auto quantity=law_verified_yield(before,now,throughput);
            if(quantity>0)outputs.emplace_back(x.good,quantity);
        }
        for(const auto& x:r.byproducts) {
            const auto quantity=law_verified_yield(
                product(previous,x.per_unit),product(batches,x.per_unit),
                throughput);
            if(quantity>0)outputs.emplace_back(x.good,quantity);
        }
        require(!outputs.empty(),"industry throughput requires positive goods");
        c_.phase_d_transform(f.owner,f.region,inputs,outputs,workers);
        d_.cumulative_nonmarket_waste[recipe]=phaseb::add_checked(
            d_.cumulative_nonmarket_waste[recipe],
            product(batches,r.nonmarket_waste_per_batch));
        d_.actual_production_runs.push_back({recipe,d_.opening_month,batches});
        d_.append(d_.opening_month,"industry-production",recipe);
    }
    // A legacy ProductionFacility.capacity is month-scoped. Native daily
    // manufacturing must not silently grant that capacity every single day.
    // Unused quota is carried inside the same month by comparing cumulative
    // real completed batches against the proportional day-of-month budget.
    void manufacture_daily(const std::string& recipe,Quantity batches,
                           std::int32_t day,std::int32_t days_in_month) {
        must(4);
        require(day>0&&days_in_month>=28&&days_in_month<=31 &&
                day<=days_in_month&&batches>0,
                "D_DAILY_INVALID_PRODUCTION_DATE");
        const auto& spec=d_.recipes.at(recipe);
        const auto& asset=c_.facility(spec.site);
        require(asset.capacity>=asset.damaged,"D_DAILY_DAMAGED_CAPACITY");
        const Quantity monthly=asset.capacity-asset.damaged;
        const Quantity allowed=phaseb::add_checked(
            phaseb::mul_checked(monthly/days_in_month,static_cast<Quantity>(day)),
            phaseb::mul_checked(monthly%days_in_month,
                                static_cast<Quantity>(day))/days_in_month);
        Quantity used=0;
        for(const auto& run:d_.actual_production_runs) {
            if(run.month!=d_.opening_month)continue;
            const auto& previous=d_.recipes.at(run.recipe);
            if(previous.site==spec.site)
                used=phaseb::add_checked(used,
                    phaseb::mul_checked(run.batches,previous.capacity_per_batch));
        }
        for(const auto& [resource_id,source]:d_.resource_sites) {
            (void)resource_id;
            if(source.site==spec.site)
                used=phaseb::add_checked(used,
                     phaseb::mul_checked(source.produced_this_month,
                                         source.site_service_per_unit));
        }
        for(const auto& evidence:d_.research_evidence) {
            if(evidence.month!=d_.opening_month)continue;
            if(d_.researches.at(evidence.project).lab_site==spec.site)
                used=phaseb::add_checked(used,evidence.units);
        }
        const Quantity required=phaseb::mul_checked(batches,spec.capacity_per_batch);
        require(used<=allowed&&required<=allowed-used,
                "D_DAILY_MONTHLY_FACILITY_BUDGET_EXCEEDED");
        manufacture(recipe,batches);
    }

    // One actual market-priced asset purchase by the national treasury.
    // The physical facility, commissioned construction title and C security
    // ownership move together; stock, workers and money remain in C.
    bool nationalize_commissioned_factory(const std::string& site) {
        must(7);
        const auto& asset=c_.facility(site);
        const auto& country=c_.sovereignty(asset.region);
        if(!c_.has_country_treasury(country)||
           c_.p2_catalog_modifier_value(
               country,"country_disable_nationalization_bool")>0)return false;
        const auto& treasury=c_.country_treasury(country);
        if(asset.owner==treasury||c_.firm_bankrupt(asset.owner))return false;
        std::string project_id;
        for(const auto& [id,p]:d_.constructions)
            if(p.target_site==site) {
                if(!p.commissioned||!project_id.empty())return false;
                project_id=id;
            }
        if(project_id.empty())return false;
        for(const auto& [id,resource]:d_.resource_sites) {
            (void)id;
            if(resource.site==site)return false;
        }
        for(const auto& [id,research]:d_.researches) {
            (void)id;
            if(research.lab_site==site&&!research.reviewed)return false;
        }
        const auto& p=d_.constructions.at(project_id);
        if(p.capitalized_value<=0)return false;
        const auto cost_factor=std::clamp(1.0+
            c_.p2_catalog_modifier_value(
                country,"building_nationalization_cost_mult"),0.1,4.0);
        const auto owner_return=std::clamp(1.0+
            c_.p2_catalog_modifier_value(
                country,"building_nationalization_investment_return_add"),
            0.1,4.0);
        const auto required=std::ceil(
            static_cast<long double>(p.capitalized_value)*
            cost_factor*owner_return);
        if(required>static_cast<long double>(
             std::numeric_limits<Money>::max()/4))return false;
        const auto price=static_cast<Money>(required);
        if(price<=0)return false;
        const bool can_pay=c_.phase_d_cash_available(treasury)>=price;
        const bool compensation_required=c_.p2_catalog_modifier_value(
            country,"country_disable_nationalization_without_compensation_bool")>0;
        if(!can_pay&&compensation_required)return false;
        if(can_pay)
            c_.phase_d_transfer_factory_paid(
                site,treasury,price,p.capitalized_value);
        else
            c_.phase_d_transfer_factory_uncompensated(
                site,treasury,p.capitalized_value);
        d_.constructions.at(project_id).client=treasury;
        d_.append(c_.month(),can_pay?
            "lawfully-compensated-factory-nationalization":
            "uncompensated-nationalization-subject-to-current-law",site);
        return true;
    }
    void plan_research(const std::string& project,Quantity units) {
        must(1);
        auto& p=d_.researches.at(project);
        require(units>0 && !p.reviewed,"invalid research plan");
        c_.phase_d_reserve_cash(p.sponsor,"research:"+project,
                                 product(units,p.cash_cost_per_work));
        p.planned_work=phaseb::add_checked(p.planned_work,units);
    }
    void perform_research(const std::string& project,Quantity units) {
        must(4);
        auto& p=d_.researches.at(project);
        require(units>0 && p.planned_work>=units && !p.reviewed,
                "research lacks accepted plan");
        const auto& lab=c_.facility(p.lab_site);
        require(lab.owner==p.holder && lab.region==p.region,
                "research lab not owned by actual performer");
        const auto material=product(units,p.material_per_work);
        require(c_.stock(p.holder,p.region,p.material_good)>=material,
                "research inputs not actually delivered");
        occupy_labor(p.holder,product(units,p.workers_per_work));
        occupy_site(p.lab_site,units);
        c_.phase_d_use_material(p.holder,p.region,p.material_good,material);
        p.verified_work=phaseb::add_checked(p.verified_work,units);
        p.planned_work-=units;
        d_.research_evidence.push_back({project,c_.month(),units});
        d_.append(c_.month(),"verified-research-work",project);
    }
    void perform_research_daily(const std::string& project,Quantity units,
                                std::int32_t day,std::int32_t days_in_month) {
        must(4);
        require(day>0&&days_in_month>=28&&days_in_month<=31&&
                day<=days_in_month&&units>0,"D_DAILY_INVALID_RESEARCH_DATE");
        const auto& p=d_.researches.at(project);
        const auto& site=c_.facility(p.lab_site);
        require(site.capacity>=site.damaged,"D_DAILY_DAMAGED_RESEARCH_LAB");
        const Quantity monthly=site.capacity-site.damaged;
        const Quantity cap=phaseb::add_checked(
               phaseb::mul_checked(monthly/days_in_month,day),
               phaseb::mul_checked(monthly%days_in_month,day)/days_in_month);
        Quantity used=0;
        for(const auto& run:d_.actual_production_runs) {
            if(run.month!=d_.opening_month)continue;
            const auto& recipe=d_.recipes.at(run.recipe);
            if(recipe.site==p.lab_site)
                used=phaseb::add_checked(used,
                     phaseb::mul_checked(run.batches,recipe.capacity_per_batch));
        }
        for(const auto& [source_id,source]:d_.resource_sites) {
            (void)source_id;
            if(source.site==p.lab_site)
                used=phaseb::add_checked(used,
                     phaseb::mul_checked(source.produced_this_month,
                                         source.site_service_per_unit));
        }
        for(const auto& evidence:d_.research_evidence) {
            if(evidence.month!=d_.opening_month)continue;
            if(d_.researches.at(evidence.project).lab_site==p.lab_site)
                used=phaseb::add_checked(used,evidence.units);
        }
        require(used<=cap&&units<=cap-used,
                "D_DAILY_RESEARCH_MONTHLY_LAB_BUDGET");
        perform_research(project,units);
    }
    void pay_research(const std::string& project,Quantity units) {
        must(5);
        auto& p=d_.researches.at(project);
        require(units>0 && p.paid_work+units<=p.verified_work,
                "cannot finance nonexistent research");
        const auto amount=product(units,p.cash_cost_per_work);
        c_.phase_d_release_cash(p.sponsor,"research:"+project,amount);
        c_.pay(p.sponsor,p.holder,amount);
        p.paid_work=phaseb::add_checked(p.paid_work,units);
        d_.append(c_.month(),"paid-research",project);
    }
    void review_research(const std::string& project) {
        must(9);
        auto& p=d_.researches.at(project);
        const auto needed=effective_research_evidence(p,d_,c_);
        require(!p.reviewed && needed>0 &&
                p.verified_work>=needed && p.paid_work>=needed,
                "research lacks paid validated evidence");
        if(d_.technologies.contains(p.tech_id)){
            for(const auto& prereq:d_.technologies.at(p.tech_id).prerequisites)
                require(static_cast<int>(d_.level(p.holder,prereq))>=
                        static_cast<int>(KnowledgeLevel::understood),
                        "research lacks technical prerequisites");
        }
        p.reviewed=true;
        d_.promotions[d_.opening_month+1].push_back({p.holder,p.tech_id,KnowledgeLevel::usable});
        d_.append(c_.month(),"knowledge-review-next-month",project);
    }
    void purchase_license(const std::string& id) {
        must(7);
        auto& license=d_.licenses.at(id);
        require(!license.paid && license.valid_from<=d_.opening_month+1 &&
                (license.valid_until<0||license.valid_until>=d_.opening_month+1),
                "invalid license availability or repeated payment");
        require(c_.shares("PATENT:"+license.tech,license.grantor)>0,
                "technology licensor has no documented patent right");
        if(license.actual_fee)
            c_.pay(license.grantee,license.grantor,license.actual_fee);
        license.paid=true;
        d_.append(d_.opening_month,"lawful-tech-license-purchased",id);
    }
    void plan_contact(const std::string& id,Quantity units){
        must(1);
        auto& contact=d_.contacts.at(id);
        require(!contact.reviewed&&units>0&&
                contact.verified_instruction+contact.planned_instruction+units<=
                    contact.required_instruction,
                "invalid knowledge transfer plan");
        if(contact.tuition_per_work)
            c_.phase_d_reserve_cash(contact.target,"contact:"+id,
                                    product(units,contact.tuition_per_work));
        contact.planned_instruction=phaseb::add_checked(contact.planned_instruction,units);
    }
    void instruct_contact(const std::string& id,Quantity units){
        must(4);
        auto& contact=d_.contacts.at(id);
        require(units>0&&units<=contact.planned_instruction &&
                d_.level(contact.source,contact.tech)==KnowledgeLevel::usable,
                "knowledge transfer requires actual knowledgeable teacher/contact");
        occupy_labor(contact.source,product(units,contact.teachers_per_work));
        contact.verified_instruction=phaseb::add_checked(contact.verified_instruction,units);
        contact.planned_instruction-=units;
        d_.append(d_.opening_month,"actual-knowledge-instruction",id);
    }
    void pay_contact(const std::string& id,Quantity units){
        must(5);
        auto& contact=d_.contacts.at(id);
        require(units>0&&contact.paid_instruction+units<=contact.verified_instruction,
                "unverified knowledge instruction cannot be paid");
        const Money fee=product(units,contact.tuition_per_work);
        if(fee){
            c_.phase_d_release_cash(contact.target,"contact:"+id,fee);
            c_.pay(contact.target,contact.source,fee);
        }
        contact.paid_instruction=phaseb::add_checked(contact.paid_instruction,units);
        d_.append(d_.opening_month,"actual-knowledge-instruction-paid",id);
    }
    void review_contact(const std::string& id){
        must(9);
        auto& contact=d_.contacts.at(id);
        require(!contact.reviewed&&contact.verified_instruction>=
                effective_contact_instruction(contact,d_,c_)&&
                contact.paid_instruction>=effective_contact_instruction(
                    contact,d_,c_),
                "knowledge transfer lacks real instruction or legitimate payment");
        contact.reviewed=true;
        d_.promotions[d_.opening_month+1].push_back({
            contact.target,contact.tech,KnowledgeLevel::understood});
        d_.append(d_.opening_month,"verified-knowledge-transfer-next-month",id);
    }
    void plan_training(const std::string& id,Quantity units){
        must(1);
        auto& p=d_.training.at(id);
        require(!p.certified&&units>0 &&
                p.verified_service+p.planned_service+units<=p.required_service,
                "invalid POP occupational training plan");
        if(p.fee_per_service)
            c_.phase_d_reserve_cash(p.sponsor,"training:"+id,
                                    product(units,p.fee_per_service));
        p.planned_service=phaseb::add_checked(p.planned_service,units);
    }
    void deliver_training(const std::string& id,Quantity units){
        must(4);
        auto& p=d_.training.at(id);
        require(!p.certified&&units>0&&units<=p.planned_service &&
                c_.people().has_slice(p.trainee_slice) &&
                c_.people().slice(p.trainee_slice).count>=p.trainee_count &&
                c_.people().employed(p.trainee_slice)==0,
                "training would duplicate employed trainees");
        occupy_labor(p.trainer,units);
        p.verified_service=phaseb::add_checked(p.verified_service,units);
        p.planned_service-=units;
        d_.append(d_.opening_month,"real-POP-occupational-training",id);
    }
    void pay_training(const std::string& id,Quantity units){
        must(5);
        auto& p=d_.training.at(id);
        require(units>0&&p.paid_service+units<=p.verified_service,
                "training fee lacks actual instructional service");
        const Money fee=product(units,p.fee_per_service);
        if(fee){
            c_.phase_d_release_cash(p.sponsor,"training:"+id,fee);
            c_.pay(p.sponsor,p.trainer,fee);
        }
        p.paid_service=phaseb::add_checked(p.paid_service,units);
        d_.append(d_.opening_month,"training-service-financially-settled",id);
    }
    void certify_training(const std::string& id){
        must(10);
        auto& p=d_.training.at(id);
        require(!p.certified&&p.verified_service>=p.required_service &&
                p.paid_service>=p.required_service,
                "new POP qualification lacks verified training input");
        p.resulting_slice=c_.phase_d_certify_occupational_training(
            p.trainee_slice,p.trainee_count,p.target_occupational_skill);
        p.certified=true;
        d_.append(d_.opening_month,"next-month-valid-POP-skill",id);
    }
    void propose_adoption(const std::string& site,const std::string& tech) {
        must(9);
        const auto& f=c_.facility(site);
        require(d_.level(f.owner,tech)==KnowledgeLevel::usable &&
                d_.licensed(f.owner,tech,d_.opening_month+1) &&
                f.capacity>f.damaged,
                "facility lacks usable knowledge, lawful license or assets");
        bool compatible=false;
        for(const auto& [id,r]:d_.recipes) {
            (void)id;
            if(r.site==site && r.tech_id==tech && !r.needs_retrofit)compatible=true;
        }
        require(compatible,"no compatible commissioned recipe for adoption");
        for(const auto& [month,targets]:d_.pending_adoptions)
            for(const auto& p:targets)
                require(p.site!=site,"duplicate pending adoption");
        d_.pending_adoptions[c_.month()+1].push_back({site,f.owner,tech});
        d_.append(c_.month(),"tech-adoption-next-month",site);
    }
};

class World {
    friend struct SnapshotCodec;
public:
    using Task=std::function<void(Stage&)>;
    using Plan=std::array<std::vector<Task>,12>;
private:
    phasec::IntegratedWorld core_;
    DomainState domain_;
public:
    explicit World(std::int64_t starting_month=24000):core_(starting_month){}
    static World from_legacy_phase_c(phasec::IntegratedWorld legacy) {
        World result(legacy.month());
        result.core_=std::move(legacy);
        result.validate();
        return result;
    }
    phasec::AdvanceReport advance_daily(const phasec::IntegratedWorld::Plan& commands) {
        World staged=*this;
        const auto report=staged.core_.advance_daily(commands);
        staged.validate();
        *this=std::move(staged);
        return report;
    }
    // Run Phase D real construction and manufacturing tasks against the
    // same stage-valid Phase C financial/inventory/POP world each day.
    // begin_month() must NOT run here: it renews natural pools and resets
    // monthly throughput. Those effects still happen only at MonthClose.
    phasec::AdvanceReport advance_daily(const Plan& tasks,
                                         bool first_calendar_day=false,
                                         bool final_calendar_day=false,
                                         int day_of_month=0,
                                         int days_in_month=0) {
        DomainState next=domain_;
        phasec::IntegratedWorld::Plan bridge;
        for(int i=0;i<12;++i) {
            bridge[static_cast<std::size_t>(i)].push_back([&,i](phasec::IntegratedWorld& live) {
                if(i==0) {
                    if(first_calendar_day) {
                        live.phase_time_begin_calendar_month();
                        next.begin_month(live); // resource season, tech, financing
                    } else {
                        next.opening_month=live.month();
                        live.phase_d_enable_shared_resources();
                    }
                    if(day_of_month>0)
                        next.regenerate_for_day(day_of_month,days_in_month);
                    for(auto& [id,project]:next.constructions) {
                        if(!project.ready || project.commissioned)continue;
                        const auto cost=project.capitalized_value;
                        require(cost>0,"daily commissioning lacks financial WIP");
                        live.phase_d_commission_facility(project.target_site,
                                project.client,project.region,
                                project.target_capacity,cost);
                        project.commissioned=true;
                        project.ready=false;
                        next.append(live.month(),"daily-facility-commissioned",id);
                    }
                }
                Stage stage{live,next,i};
                for(const auto& callback:tasks[static_cast<std::size_t>(i)])
                    callback(stage);
                if(i==11) {
                    next.finalize(live);
                    live.phase_d_daily_finish_resources();
                }
            });
        }
        static_assert(std::is_nothrow_move_assignable_v<DomainState>);
        const auto report=core_.advance_daily(bridge,final_calendar_day);
        domain_=std::move(next);
        return report;
    }
    [[nodiscard]] bool has_ready_daily_commission()const {
        for(const auto& [id,project]:domain_.constructions) {
            (void)id;
            if(project.ready&&!project.commissioned)return true;
        }
        return false;
    }
    [[nodiscard]] const phasec::IntegratedWorld& core() const {return core_;}
    phasec::IntegratedWorld& seed_core() {
        require(core_.commit_id()==0,"cannot seed Phase C after first committed month");
        return core_;
    }
    void seed_construction(ConstructionProject p) {
        require(core_.commit_id()==0&&!p.id.empty()&&!p.target_site.empty()&&
                !domain_.constructions.contains(p.id)&&p.work_required>0 &&
                p.material_per_work>0 && p.workers_per_work>0 &&
                p.cash_price_per_work>0 && p.target_capacity>0 &&
                !p.client.empty()&&!p.contractor.empty()&&!p.region.empty(),
                "invalid construction seed");
        domain_.constructions.emplace(p.id,std::move(p));
    }
    void seed_work_package(WorkPackage wp) {
        require(core_.commit_id()==0 && !wp.id.empty() &&
                !domain_.work_packages.contains(wp.id) &&
                domain_.constructions.contains(wp.project) &&
                wp.required_work>0 && wp.material_per_work>0 && wp.workers_per_work>0 &&
                !wp.material_good.empty() && wp.curing_months>=0,
                "invalid work package seed");
        for(const auto& pred:wp.predecessors)
            require(domain_.work_packages.contains(pred) &&
                    domain_.work_packages.at(pred).project==wp.project,
                    "work-package predecessors must be seeded in DAG order");
        domain_.work_packages.emplace(wp.id,std::move(wp));
    }
    void seed_industry_class(IndustryClass cl){
        require(core_.commit_id()==0 && !cl.id.empty() &&
                !domain_.industry_classes.contains(cl.id) &&
                (cl.parent.empty()||domain_.industry_classes.contains(cl.parent)),
                "invalid industry taxonomy parent or ID");
        domain_.industry_classes.emplace(cl.id,std::move(cl));
    }
    void seed_resource_site(ResourceSite source){
        require(core_.commit_id()==0&&!source.id.empty() &&
                !domain_.resource_sites.contains(source.id) &&
                !source.owner.empty()&&!source.region.empty()&&!source.good.empty() &&
                source.monthly_capacity>0 && source.produced_this_month==0 &&
                source.cumulative_output==0 &&
                source.workers_per_unit>0 && source.site_service_per_unit>0 &&
                source.decline_ppm_active_month<=1'000'000 &&
                source.decline_remainder==0 &&
                (source.renewable ?
                    (source.carrying_capacity>0 && source.standing_biomass>=0 &&
                     source.carrying_capacity>=source.standing_biomass &&
                     source.renewal_per_month>=0 && source.decline_ppm_active_month==0)
                    : (source.standing_biomass==0 && source.renewal_per_month==0 &&
                       source.carrying_capacity==0)),
                "invalid operational resource site seed");
        const auto& facility=core_.facility(source.site);
        require(facility.owner==source.owner && facility.region==source.region,
                "resource source not served by owned physical extraction site");
        domain_.resource_sites.emplace(source.id,std::move(source));
    }
    void seed_recipe(IndustryRecipe r) {
        require(core_.commit_id()==0&&!r.id.empty()&&!r.site.empty()&&
                !domain_.recipes.contains(r.id) && r.workers_per_batch>0 &&
                r.capacity_per_batch>0&&!r.inputs.empty()&&!r.outputs.empty(),
                "invalid production recipe seed");
        for(const auto& x:r.inputs)
            require(!x.good.empty()&&x.per_unit>0,"invalid production input");
        for(const auto& x:r.outputs)
            require(!x.good.empty()&&x.per_unit>0,"invalid production output");
        for(const auto& x:r.byproducts)
            require(!x.good.empty()&&x.per_unit>0,"invalid co-product");
        require(r.nonmarket_waste_per_batch>=0 &&
                (r.industry_class.empty()||domain_.industry_classes.contains(r.industry_class)),
                "invalid industry taxonomy or waste coefficient");
        domain_.recipes.emplace(r.id,std::move(r));
    }
    void seed_research(ResearchProject p) {
        require(core_.commit_id()==0&&!p.id.empty()&&!p.holder.empty()&&
                !p.sponsor.empty()&&!p.tech_id.empty()&&!p.lab_site.empty()&&
                !p.region.empty()&&!p.material_good.empty() &&
                p.material_per_work>0&&p.required_evidence>0 &&
                p.workers_per_work>0&&p.cash_cost_per_work>0 &&
                !domain_.researches.contains(p.id),"invalid research seed");
        domain_.researches.emplace(p.id,std::move(p));
    }
    void seed_technology(TechnologyDefinition t){
        require(core_.commit_id()==0&&!t.id.empty()&&
                !domain_.technologies.contains(t.id),"invalid technology definition");
        for(const auto& prior:t.prerequisites)
            require(domain_.technologies.contains(prior)&&prior!=t.id,
                    "technical prerequisites must be seeded in topological order");
        domain_.technologies.emplace(t.id,std::move(t));
    }
    void seed_license(TechnologyLicense license){
        require(core_.commit_id()==0&&!license.id.empty() &&
                !domain_.licenses.contains(license.id) &&
                !license.tech.empty() && license.actual_fee>=0 &&
                !license.grantor.empty()&&!license.grantee.empty() &&
                license.grantor!=license.grantee &&
                (license.valid_until<0||license.valid_until>=license.valid_from),
                "invalid historical/license seed");
        domain_.licenses.emplace(license.id,std::move(license));
    }
    void seed_contact(KnowledgeContact contact){
        require(core_.commit_id()==0 && !contact.id.empty() &&
                !domain_.contacts.contains(contact.id) &&
                !contact.source.empty()&&!contact.target.empty() &&
                contact.source!=contact.target && !contact.tech.empty() &&
                contact.required_instruction>0&&contact.teachers_per_work>0 &&
                contact.tuition_per_work>=0,
                "invalid knowledge contact");
        domain_.contacts.emplace(contact.id,std::move(contact));
    }
    void seed_training(TrainingProgram p){
        require(core_.commit_id()==0&&!p.id.empty() &&
                !domain_.training.contains(p.id) &&
                !p.trainer.empty()&&!p.sponsor.empty()&&
                p.trainee_count>0 && p.trainee_slice>0 &&
                p.required_service>0 && p.fee_per_service>=0 &&
                p.target_occupational_skill<=100 &&
                core_.people().has_slice(p.trainee_slice) &&
                core_.people().slice(p.trainee_slice).count>=p.trainee_count,
                "invalid occupational training program");
        domain_.training.emplace(p.id,std::move(p));
    }
    void seed_knowledge(std::string holder,std::string tech,KnowledgeLevel level) {
        require(core_.commit_id()==0&&!holder.empty()&&!tech.empty() &&
                level!=KnowledgeLevel::unknown &&
                !domain_.knowledge.contains({holder,tech}),
                "invalid initial historical knowledge");
        domain_.knowledge.emplace(std::make_pair(std::move(holder),std::move(tech)),level);
    }
    [[nodiscard]] const DomainState& state() const {return domain_;}
    [[nodiscard]] KnowledgeLevel knowledge(const std::string& holder,
                                           const std::string& tech) const {
        return domain_.level(holder,tech);
    }
    [[nodiscard]] const ConstructionProject& construction(const std::string& id)const {
        return domain_.constructions.at(id);
    }
    [[nodiscard]] Quantity realized_batches(const std::string& industry_class,
                                               std::int64_t from_month,
                                               std::int64_t through_month)const{
        require(from_month<=through_month,"invalid industry statistic period");
        Quantity total=0;
        for(const auto& event:domain_.actual_production_runs){
            if(event.month<from_month||event.month>through_month)continue;
            if(domain_.recipes.at(event.recipe).industry_class!=industry_class)continue;
            total=phaseb::add_checked(total,event.batches);
        }
        return total;
    }
    [[nodiscard]] const ResourceSite& resource_site(const std::string& id)const{
        return domain_.resource_sites.at(id);
    }
    [[nodiscard]] Quantity nonmarket_waste(const std::string& recipe)const{
        auto it=domain_.cumulative_nonmarket_waste.find(recipe);
        return it==domain_.cumulative_nonmarket_waste.end()?0:it->second;
    }
    [[nodiscard]] const WorkPackage& work_package(const std::string& id) const {
        return domain_.work_packages.at(id);
    }
    [[nodiscard]] const KnowledgeContact& contact(const std::string& id)const{
        return domain_.contacts.at(id);
    }
    [[nodiscard]] const TrainingProgram& training(const std::string& id)const{
        return domain_.training.at(id);
    }
    [[nodiscard]] const TechnologyLicense& license(const std::string& id)const{
        return domain_.licenses.at(id);
    }
    [[nodiscard]] const ResearchProject& research(const std::string& id)const {
        return domain_.researches.at(id);
    }
    [[nodiscard]] std::int64_t month() const {return core_.month();}
    [[nodiscard]] std::uint64_t commit_id() const {return core_.commit_id();}
    void validate()const {core_.validate(); domain_validation();}
private:
    void domain_validation()const {
        // Reuse the same checks without publishing a commit.
        DomainState copy=domain_;
        copy.finalize(core_);
    }
public:
    phasec::AdvanceReport advance(const Plan& plan={}) {
        // Both the domain state and Phase C's internal snapshot are staged.
        // No mutable Phase D state escapes when C throws in any of stages 0..11.
        DomainState next=domain_;
        phasec::IntegratedWorld::Plan bridge;
        for(int i=0;i<12;++i) {
            bridge[static_cast<std::size_t>(i)].push_back([&,i](phasec::IntegratedWorld& live) {
                if(i==0) {
                    next.begin_month(live);
                    next.regenerate_for_legacy_month();
                }
                Stage stage{live,next,i};
                for(const auto& task:plan[static_cast<std::size_t>(i)])task(stage);
                if(i==11)next.finalize(live);
            });
        }
        // Phase C::advance already clones and atomically commits the complete
        // core. Avoid an additional full POP copy: at 1m sparse slices it
        // triples peak population RAM without adding rollback safety.
        static_assert(std::is_nothrow_move_assignable_v<DomainState>,
                      "Phase D domain commit must never throw after Phase C commit");
        const auto report=core_.advance(bridge);
        domain_=std::move(next);
        return report;
    }
};
} // namespace vic25::phased
