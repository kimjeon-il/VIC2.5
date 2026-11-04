#pragma once
// Phase D bounded vertical integration with the Phase C atomic staging engine.
// phase-d-cpp/0.1: construction + production recipes + funded research +
// evidence-based knowledge activation + existing-site technique adoption.
// This deliberately does not claim all D-C/I/T acceptance cases are implemented.

#include "vic25/phase_c.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
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
};
struct IndustryRecipe {
    std::string id, site, tech_id;
    std::vector<Input> inputs, outputs;
    People workers_per_batch{};
    Quantity capacity_per_batch{1};
    bool needs_retrofit{}; // retrofit pipeline remains unsupported by this slice
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
    std::map<std::string,IndustryRecipe> recipes;
    std::map<std::string,ResearchProject> researches;
    std::map<std::pair<std::string,std::string>,KnowledgeLevel> knowledge;
    std::map<std::string,std::string> active_site_tech;
    std::map<std::int64_t,std::vector<KnowledgePromotion>> promotions;
    std::map<std::int64_t,std::vector<Adoption>> pending_adoptions;
    std::vector<Evidence> research_evidence;
    std::vector<std::string> journal;
    std::map<std::string,People> labor_used;
    std::map<std::string,Quantity> site_capacity_used;
    std::map<std::string,Money> funds_reserved;
    std::uint64_t event_sequence{};
    // Phase C's stage10 increments people_.month() before stage11 validation.
    // Keep the original opening month for Phase D stage11 effective dates.
    std::int64_t opening_month{-1};

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
        labor_used.clear();site_capacity_used.clear();funds_reserved.clear();
        for(auto& [id,p]:constructions){(void)id;p.planned_work=0;}
        for(auto& [id,p]:researches){(void)id;p.planned_work=0;}
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
                        level(a.owner,a.tech)==KnowledgeLevel::usable,
                        "tech adoption missing knowledge or usable asset");
                active_site_tech[a.site]=a.tech;
                append(c.month(),"technique-effective",a.site);
            }
            pending_adoptions.erase(it);
        }
    }
    void finalize(const phasec::IntegratedWorld& c) {
        for(auto& [id,p]:constructions) {
            require(p.work_required>0 && p.accepted_work>=0 &&
                    p.accepted_work<=p.work_required && p.paid_work<=p.accepted_work &&
                    p.capitalized_work<=p.paid_work,
                    "construction work/finance over-allocation");
            require(p.capitalized_value==product(p.capitalized_work,p.cash_price_per_work),
                    "WIP record does not match certified work");
            if(!p.commissioned && !p.ready && p.accepted_work==p.work_required &&
               p.paid_work==p.accepted_work && p.capitalized_work==p.paid_work) {
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

class Stage {
    phasec::IntegratedWorld& c_;
    DomainState& d_;
    int phase_;
    void must(int n) const {require(phase_==n,"phase D event in wrong stage");}
    void occupy_labor(const std::string& owner,People people) {
        require(people>0 && c_.phase_d_workforce(owner)>=people,
                "phase D labor lacks actual Phase B employment");
        const auto next=phaseb::add_checked(d_.labor_used[owner],people);
        require(next<=c_.phase_d_workforce(owner),
                "phase D double-used worker service");
        d_.labor_used[owner]=next;
    }
    void occupy_site(const std::string& site,Quantity units) {
        require(units>0,"nonpositive site usage");
        const auto& f=c_.facility(site);
        require(f.capacity>=f.damaged,"invalid facility damage");
        const auto next=phaseb::add_checked(d_.site_capacity_used[site],units);
        require(next<=f.capacity-f.damaged,"facility capacity exceeded");
        d_.site_capacity_used[site]=next;
    }
    void reserve_cash(const std::string& owner, Money amount) {
        require(amount>0,"zero budget reservation");
        const auto next=phaseb::add_checked(d_.funds_reserved[owner],amount);
        require(next<=c_.cash(owner),"phase D unfunded or double-reserved budget");
        d_.funds_reserved[owner]=next;
    }
public:
    Stage(phasec::IntegratedWorld& c,DomainState& d,int stage)
        :c_(c),d_(d),phase_(stage){}
    [[nodiscard]] int phase() const {return phase_;}
    [[nodiscard]] std::int64_t month() const {return d_.opening_month;}
    [[nodiscard]] phasec::IntegratedWorld& phase_c() {return c_;}
    [[nodiscard]] const DomainState& state() const {return d_;}

    // Stage1 promises are bounded by real existing cash. A full Phase C
    // reservation ledger is a future L1 requirement; Stage5 actual pay is
    // mandatory before WIP / Knowledge changes.
    void plan_work(const std::string& project,Quantity units) {
        must(1);
        auto& p=d_.constructions.at(project);
        require(units>0 && p.accepted_work+units<=p.work_required &&
                !p.ready&&!p.commissioned,"invalid construction plan");
        reserve_cash(p.client,product(units,p.cash_price_per_work));
        p.planned_work=phaseb::add_checked(p.planned_work,units);
    }
    void perform_work(const std::string& project,Quantity units) {
        must(4);
        auto& p=d_.constructions.at(project);
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
        require(d_.funds_reserved[p.client]>=amount,
                "construction lacks authorized budget reservation");
        c_.pay(p.client,p.contractor,amount);
        d_.funds_reserved[p.client]-=amount;
        p.paid_work=phaseb::add_checked(p.paid_work,units);
        d_.append(c_.month(),"work-paid",project);
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
        const auto capacity=product(batches,r.capacity_per_batch);
        const auto workers=product(batches,r.workers_per_batch);
        for(const auto& x:r.inputs)
            require(c_.stock(f.owner,f.region,x.good)>=product(batches,x.per_unit),
                    "production ingredient not delivered");
        occupy_labor(f.owner,workers);
        occupy_site(r.site,capacity);
        std::vector<std::pair<std::string,Quantity>> inputs,outputs;
        for(const auto& x:r.inputs)inputs.emplace_back(x.good,product(batches,x.per_unit));
        for(const auto& x:r.outputs)outputs.emplace_back(x.good,product(batches,x.per_unit));
        c_.phase_d_transform(f.owner,f.region,inputs,outputs,workers);
        d_.append(c_.month(),"industry-production",recipe);
    }
    void plan_research(const std::string& project,Quantity units) {
        must(1);
        auto& p=d_.researches.at(project);
        require(units>0 && !p.reviewed,"invalid research plan");
        reserve_cash(p.sponsor,product(units,p.cash_cost_per_work));
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
    void pay_research(const std::string& project,Quantity units) {
        must(5);
        auto& p=d_.researches.at(project);
        require(units>0 && p.paid_work+units<=p.verified_work,
                "cannot finance nonexistent research");
        const auto amount=product(units,p.cash_cost_per_work);
        require(d_.funds_reserved[p.sponsor]>=amount,
                "research amount was not cash-reserved");
        c_.pay(p.sponsor,p.holder,amount);
        d_.funds_reserved[p.sponsor]-=amount;
        p.paid_work=phaseb::add_checked(p.paid_work,units);
        d_.append(c_.month(),"paid-research",project);
    }
    void review_research(const std::string& project) {
        must(9);
        auto& p=d_.researches.at(project);
        require(!p.reviewed && p.required_evidence>0 &&
                p.verified_work>=p.required_evidence &&
                p.paid_work>=p.required_evidence,
                "research lacks paid validated evidence");
        p.reviewed=true;
        d_.promotions[c_.month()+1].push_back({p.holder,p.tech_id,KnowledgeLevel::usable});
        d_.append(c_.month(),"knowledge-review-next-month",project);
    }
    void propose_adoption(const std::string& site,const std::string& tech) {
        must(9);
        const auto& f=c_.facility(site);
        require(d_.level(f.owner,tech)==KnowledgeLevel::usable &&
                f.capacity>f.damaged,"facility lacks usable knowledge or assets");
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
public:
    using Task=std::function<void(Stage&)>;
    using Plan=std::array<std::vector<Task>,12>;
private:
    phasec::IntegratedWorld core_;
    DomainState domain_;
public:
    explicit World(std::int64_t starting_month=24000):core_(starting_month){}
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
    void seed_recipe(IndustryRecipe r) {
        require(core_.commit_id()==0&&!r.id.empty()&&!r.site.empty()&&
                !domain_.recipes.contains(r.id) && r.workers_per_batch>0 &&
                r.capacity_per_batch>0&&!r.inputs.empty()&&!r.outputs.empty(),
                "invalid production recipe seed");
        for(const auto& x:r.inputs)
            require(!x.good.empty()&&x.per_unit>0,"invalid production input");
        for(const auto& x:r.outputs)
            require(!x.good.empty()&&x.per_unit>0,"invalid production output");
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
                if(i==0)next.begin_month(live);
                Stage stage{live,next,i};
                for(const auto& task:plan[static_cast<std::size_t>(i)])task(stage);
                if(i==11)next.finalize(live);
            });
        }
        auto staged_core=core_;
        const auto report=staged_core.advance(bridge);
        core_=std::move(staged_core);
        domain_=std::move(next);
        return report;
    }
};
} // namespace vic25::phased
