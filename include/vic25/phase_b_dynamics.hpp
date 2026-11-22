#pragma once
#include "vic25/phase_b_world.hpp"
#include "vic25/day_budget.hpp"
#include "vic25/monthly.hpp"
#include <functional>

namespace vic25::phaseb {

struct DemographyRules {
    // Values are monthly rates per million exposed individuals; example calibration only.
    People fertility_ppm{7'000};
    People infant_death_ppm{800};
    People child_death_ppm{120};
    People adult_death_ppm{130};
    People senior_death_ppm{2'800};
    People very_old_death_ppm{12'000};
    People illness_incidence_ppm{450};
    People illness_recovery_ppm{130'000};
    People accident_ppm{120};
    People injury_recovery_ppm{85'000};
    People primary_access_ppm{850'000};
    std::array<People,4> education_access_multiplier_ppm{PPM,PPM,PPM,PPM};
    People secondary_access_ppm{580'000};
    People tertiary_access_ppm{150'000};
    People adult_literacy_access_ppm{90'000};
    People training_ppm{120'000};
    People accident_permanent_share_ppm{50'000};
    People migration_ppm{0};
    std::map<std::string,std::string> destination_by_region{}; // exogenous scenario, no endogenous decisions yet.
    std::vector<Vacancy> monthly_vacancies{}; // newly available slots each month, not active employment cap.
    LaborRule labor{};
};
struct MonthReport {
    People start_population{},births{},deaths{},new_illness{},recovered{},injuries{},permanent_injuries{},rehabilitated{},graduated{},trained{},migrants{},end_population{};
    std::size_t active_slices{},events_added{};
    std::int64_t month_index{};
    std::uint64_t digest{};
};

// Phase B-only simulation. Other phases in MONTH_STAGES remain deliberate no-ops;
// economic production, actual goods settlement and full Ledger atomicity are future tasks.
class Engine {
public:
    using RateAdjustment=std::function<People(
        const World&,Id,const char*,People)>;
    explicit Engine(DemographyRules rules={}) : rules_(rules) { validate_rules(); }
    template<class Hook>
    MonthReport advance(World& world,Hook&& stage_hook) const {
        World staged=world;
        MonthReport result;result.month_index=world.month();result.start_population=world.population();
        const auto previous_events=world.events().size();
        for(const auto stage:vic25::MONTH_STAGES){
            stage_hook(stage); // a throwing hook must leave real world unchanged.
            if(stage==vic25::MonthStage::labor_clearance && !rules_.monthly_vacancies.empty())
                (void)staged.match_jobs(rules_.monthly_vacancies,rules_.labor);
            if(stage==vic25::MonthStage::pop_demography_and_health) do_demography(staged,result);
        }
        staged.finish_month();staged.release_ineligible(rules_.labor);staged.prune_residuals();
        if(!staged.validate(rules_.labor))throw std::logic_error("Phase B invariant failure");
        result.end_population=staged.population();
        if(result.end_population != add_checked(add_checked(result.start_population,result.births),-result.deaths))
            throw std::logic_error("population balance failure");
        result.events_added=staged.events().size()-previous_events;
        result.active_slices=staged.slices().size();result.digest=staged.fingerprint();
        world=std::move(staged);
        return result;
    }
    MonthReport advance(World& w) const{return advance(w,[](vic25::MonthStage){});}

    // A real daily POP update. The monthly PPM is distributed over the
    // calendar's exact number of days using integer cumulative quotas.
    // Sum(day_ppm, 1..days_in_month) == monthly_ppm, without float drift.
    // The caller must stage the entire C/D/E/G/H world atomically.
    MonthReport advance_day(World& w,int day,int days_in_month,
                            RateAdjustment adjustment={})const {
        require(days_in_month>=28&&days_in_month<=31&&day>=1&&
                day<=days_in_month,"invalid autonomous POP calendar day");
        w.set_native_calendar_day(day);
        const auto per_day=[&](People monthly_ppm)->People {
            return daily_budget::increment(monthly_ppm,day,days_in_month);
        };
        DemographyRules today=rules_;
        today.fertility_ppm=per_day(rules_.fertility_ppm);
        today.infant_death_ppm=per_day(rules_.infant_death_ppm);
        today.child_death_ppm=per_day(rules_.child_death_ppm);
        today.adult_death_ppm=per_day(rules_.adult_death_ppm);
        today.senior_death_ppm=per_day(rules_.senior_death_ppm);
        today.very_old_death_ppm=per_day(rules_.very_old_death_ppm);
        today.illness_incidence_ppm=per_day(rules_.illness_incidence_ppm);
        today.illness_recovery_ppm=per_day(rules_.illness_recovery_ppm);
        today.accident_ppm=per_day(rules_.accident_ppm);
        today.injury_recovery_ppm=per_day(rules_.injury_recovery_ppm);
        today.migration_ppm=per_day(rules_.migration_ppm);
        for(auto& vacancy:today.monthly_vacancies){
            const auto monthly=vacancy.slots;
            vacancy.slots=daily_budget::increment(monthly,day,days_in_month);
        }
        std::erase_if(today.monthly_vacancies,
                      [](const Vacancy& v){return v.slots==0;});
        // Educational opportunity and occupational training are genuine
        // daily hazards, using the same cumulative integer allowance as
        // births and mortality. The birth-month eligibility window is not
        // mistaken for a known individual birthday.
        today.primary_access_ppm=per_day(rules_.primary_access_ppm);
        today.secondary_access_ppm=per_day(rules_.secondary_access_ppm);
        today.tertiary_access_ppm=per_day(rules_.tertiary_access_ppm);
        today.adult_literacy_access_ppm=per_day(rules_.adult_literacy_access_ppm);
        today.training_ppm=per_day(rules_.training_ppm);
        const bool month_end=day==days_in_month;
        Engine engine{today};
        engine.adjustment_=std::move(adjustment);
        MonthReport report;
        report.month_index=w.month();
        report.start_population=w.population();
        const auto prior=w.events().size();
        if(!today.monthly_vacancies.empty())
            (void)w.match_jobs(today.monthly_vacancies,today.labor);
        engine.do_demography(w,report,true,month_end);
        w.release_ineligible(today.labor);
        w.prune_residuals();
        require(w.validate(today.labor),"autonomous daily demographic invariant");
        report.end_population=w.population();
        if(report.end_population!=add_checked(add_checked(report.start_population,
                                           report.births),-report.deaths))
            throw std::logic_error("autonomous daily population balance failure");
        report.events_added=w.events().size()-prior;
        report.active_slices=w.slices().size();
        report.digest=w.fingerprint();
        return report;
    }
    // An otherwise idle bare native day must still apply a real age
    // change. On a known day, the hypothetical age is the month quotient.
    static bool known_birthday_labor_due(const World& w,const LaborRule& rule,int day) {
        for(const auto& [id,born]:w.exact_birth_days()) {
            if(born!=day)continue;
            const auto age=w.age_months(w.slice(id).key.demographic);
            if(contract_age_ineligible(w,id,age,rule)||
               life_course_kind(w,id,age,rule)!=LifeCourseKind::none)return true;
        }
        return false;
    }

    // This is a calendar/contract rule, not a stochastic demography hazard.
    // The bare clock already processes first days at month opening.
    static void apply_known_birthday_labor(
            World& w,const LaborRule& rule,int previous_month_final_day) {
        const auto day=w.native_calendar_day();
        if(day<=0)return;
        for(const auto& [id,born]:w.exact_birth_days()) {
            const bool missed=day==1&&previous_month_final_day>0&&
                              born>previous_month_final_day;
            if(born!=day&&!missed)continue;
            // A missing calendar date can advance exact age on the next
            // actual day. The registered birth day is never substituted.
            const auto age=w.age_months_slice(id);
            if(contract_age_ineligible(w,id,age,rule))
                w.release(id,w.employed(id));
            if(!missed||age==rule.minimum_age_months||age==rule.maximum_age_months)
                labor_life_course(w,id,rule,true);
        }
    }
private:
    enum class LifeCourseKind {none,entry,retirement};

    static bool contract_age_ineligible(const World& w,Id id,std::int64_t age,
                                        const LaborRule& rule) {
        const auto* job=w.job(id);
        if(!job)return false;
        const LaborRule base;
        const auto minimum=std::max({base.minimum_age_months,
            rule.minimum_age_months,job->requirements.minimum_age_months});
        const auto maximum=std::min({base.maximum_age_months,
            rule.maximum_age_months,job->requirements.maximum_age_months});
        return age<minimum||age>=maximum;
    }
    static LifeCourseKind life_course_kind(const World& w,Id id,
            std::int64_t age,const LaborRule& rule) {
        const auto& row=w.slice(id);
        const auto& d=w.demographic(row.key.demographic);
        if(d.service!=Service::civilian && d.service!=Service::reserve)
            return LifeCourseKind::none;
        const auto& econ=w.economic(row.key.economic);
        if(age>=rule.minimum_age_months&&age<rule.maximum_age_months&&
           econ.profession=="not_assigned"&&econ.livelihood=="inactive")
            return LifeCourseKind::entry;
        if(age>=rule.maximum_age_months&&econ.livelihood=="wage")
            return LifeCourseKind::retirement;
        return LifeCourseKind::none;
    }
    static void labor_life_course(World& w,Id id,const LaborRule& rule,
                                  bool same_identity=false) {
        if(!w.has_slice(id))return;
        const auto change=life_course_kind(w,id,w.age_months_slice(id),rule);
        if(change==LifeCourseKind::none)return;
        const auto current=w.slice(id);
        auto next=w.economic(current.key.economic);
        if(change==LifeCourseKind::entry) {
            next.profession="unskilled_worker";next.livelihood="wage";
            next.labor_class="unemployed";
        } else {
            if(w.employed(id)>0)w.release(id,w.employed(id));
            next.livelihood="inactive";next.labor_class="retired";
        }
        if(same_identity)w.rekey_birthday_labor(id,next);
        else (void)w.reclassify_economic(id,current.count,next,0);
    }
    DemographyRules rules_;
    RateAdjustment adjustment_;
    [[nodiscard]] People adjusted_rate(const World& w,Id id,
                                       const char* kind,People base)const {
        if(!adjustment_)return base;
        const auto value=adjustment_(w,id,kind,base);
        require(value>=0&&value<=PPM,"politically adjusted POP hazard invalid");
        return value;
    }
    static People job_allocation(const World& w, Id id,People total){
        const auto& s=w.slice(id);const People jobs=w.employed(id);
        if(total==s.count)return jobs;
        return (jobs/s.count)*total + mul_checked(jobs%s.count,total)/s.count;
    }
    [[nodiscard]] People death_rate(const World& w,const Slice& s)const{
        const auto age=w.age_months_slice(s.id);
        People rate= age<12?rules_.infant_death_ppm:age<15*12?rules_.child_death_ppm:
                     age<65*12?rules_.adult_death_ppm:age<85*12?rules_.senior_death_ppm:rules_.very_old_death_ppm;
        const auto& hp=w.health(w.demographic(s.key.demographic).health);
        if(hp.ill)rate=std::min(PPM,rate*std::int64_t(2));
        if(hp.temporary_injury)rate=std::min(PPM,rate*std::int64_t(2));
        return rate;
    }
    void do_demography(World& w,MonthReport& report,
                       bool education_milestones=true,
                       bool lifecycle_due=true)const{
        const auto original=w.slices();
        for(const auto& [id,initial]:original){
            if(!w.has_slice(id))continue;
            const auto d=w.demographic(initial.key.demographic);
            const auto econ=w.economic(initial.key.economic);
            const auto hp=w.health(d.health);
            const auto ed=w.education(econ.education);
            const auto age=w.age_months_slice(id);
            const auto deaths=w.allocate_rate(id,"mortality",initial.count,
                adjusted_rate(w,id,"mortality",death_rate(w,initial)));
            if(deaths){w.death(id,deaths,job_allocation(w,id,deaths));report.deaths=add_checked(report.deaths,deaths);}
            if(d.sex==Sex::female && age>=18*12 && age<45*12){
                // Birth cohort must join a NONWORKER economic profile, never mother's profession.
                const auto remaining_mothers=w.has_slice(id)?w.slice(id).count:0;
                const auto births=w.allocate_rate(id,"fertility",remaining_mothers,
                        adjusted_rate(w,id,"fertility",rules_.fertility_ppm));
                if(births){
                    Economic child=econ;child.profession="not_assigned";child.livelihood="inactive";
                    child.labor_class="ineligible";child.education=w.default_education();
                    const Id baby_e=w.economic_profile(child);
                    const People girls=births/2 + (static_cast<People>(w.month()%2) && births%2);
                    const People boys=births-girls;
                    if(girls)w.birth(initial.key.household,d,baby_e,Sex::female,girls);
                    if(boys)w.birth(initial.key.household,d,baby_e,Sex::male,boys);
                    report.births=add_checked(report.births,births);
                }
            }
            if(!w.has_slice(id))continue;
            People remaining=w.slice(id).count;
            if(hp.ill){
                auto recovered=w.allocate_rate(id,"ill-recovery",remaining,rules_.illness_recovery_ppm);
                if(recovered){Health next=hp;next.ill=false;next.illness_severity=0;
                    w.change_health(id,recovered,next,EventKind::recovery,job_allocation(w,id,recovered));
                    report.recovered=add_checked(report.recovered,recovered);}
            }else{
                auto sick=w.allocate_rate(id,"ill-incidence",remaining,rules_.illness_incidence_ppm);
                if(sick){Health next=hp;next.ill=true;next.illness_severity=1;
                    w.change_health(id,sick,next,EventKind::illness,job_allocation(w,id,sick));
                    report.new_illness=add_checked(report.new_illness,sick);}
            }
            if(!w.has_slice(id))continue;
            remaining=w.slice(id).count;
            if(hp.temporary_injury){
                auto healed=w.allocate_rate(id,"injury-recovery",remaining,rules_.injury_recovery_ppm);
                if(healed){Health next=hp;next.temporary_injury=false;next.injury_severity=0;
                    w.change_health(id,healed,next,EventKind::rehabilitation,job_allocation(w,id,healed));
                    report.rehabilitated=add_checked(report.rehabilitated,healed);}
            }else if(age>=16*12 && age<75*12 &&
                     (econ.livelihood=="wage"||econ.livelihood=="self_employed"||econ.livelihood=="subsistence")){
                auto injured=w.allocate_rate(id,"accident",remaining,rules_.accident_ppm);
                if(injured){
                    const auto permanent=w.allocate_rate(id,"permanent-share",injured,rules_.accident_permanent_share_ppm);
                    if(permanent){
                        Health chronic=hp;chronic.long_term_impairment=true;
                        chronic.limited_capabilities|=1U;
                        w.change_health(id,permanent,chronic,EventKind::injury,job_allocation(w,id,permanent));
                        report.permanent_injuries=add_checked(report.permanent_injuries,permanent);
                    }
                    const People temporary=injured-permanent;
                    if(temporary && w.has_slice(id)){
                        Health next=hp;next.temporary_injury=true;next.injury_severity=1;
                        w.change_health(id,temporary,next,EventKind::injury,job_allocation(w,id,temporary));
                    }
                    report.injuries=add_checked(report.injuries,injured);
                }
            }
            if(!w.has_slice(id))continue;
            remaining=w.slice(id).count;
            if(education_milestones && age >= 0 && age%12==0){
                Education next=ed;People rate=0;
                if(ed.school==School::none && age>=8*12 && age<65*12){
                    rate=age<18*12?rules_.primary_access_ppm:rules_.adult_literacy_access_ppm;
                    next.school=School::primary;next.literacy=std::max<std::uint8_t>(next.literacy,60);
                }else if(ed.school==School::primary && age>=13*12 && age<30*12){
                    rate=rules_.secondary_access_ppm;next.school=School::secondary;next.literacy=std::max<std::uint8_t>(next.literacy,90);
                }else if(ed.school==School::secondary && age>=18*12 && age<27*12){
                    rate=rules_.tertiary_access_ppm;next.school=School::tertiary;next.literacy=100;
                }
                if(rate){
                    const auto multiplier=rules_.education_access_multiplier_ppm[static_cast<std::size_t>(d.sex)];
                    rate=(rate/PPM)*multiplier + (rate%PPM)*multiplier/PPM;
                    const auto n=w.allocate_rate(id,"school",remaining,
                        adjusted_rate(w,id,"school",rate));
                    if(n){w.change_education(id,n,next,EventKind::schooling,job_allocation(w,id,n));
                          report.graduated=add_checked(report.graduated,n);}
                }
            }
            if(!w.has_slice(id))continue;
            remaining=w.slice(id).count;
            if(education_milestones && age >= 18*12 && age%12==0 &&
               w.employed(id)>0 && ed.occupational_skill<100){
                const auto n=w.allocate_rate(id,"training",w.employed(id),rules_.training_ppm);
                if(n){Education next=ed;next.occupational_skill=static_cast<std::uint8_t>(std::min(100,static_cast<int>(next.occupational_skill)+10));
                    w.change_education(id,n,next,EventKind::training,n);report.trained=add_checked(report.trained,n);}
            }

        }
        // Entering and leaving civilian working age is a real economic status
        // transition; it must not be inferred solely from a family POP's trade.
        // Retirement preserves occupational identity/history while changing livelihood.
        const auto life_course=lifecycle_due?w.slices():std::map<Id,Slice>{};
        for(const auto& [id,snapshot]:life_course) {
            (void)snapshot;
            labor_life_course(w,id,rules_.labor);
        }
        // Exogenous monthly migration: move actual people and cancel their local
        // employment before future job matching; no new money or population.
        if(rules_.migration_ppm>0 && !rules_.destination_by_region.empty()){
            const auto sources=w.slices();
            for(const auto& [id,initial]:sources){
                if(!w.has_slice(id))continue;
                const auto d=w.demographic(initial.key.demographic);
                const auto dest_it=rules_.destination_by_region.find(d.region);
                if(dest_it==rules_.destination_by_region.end()||dest_it->second==d.region)continue;
                const People available=std::min(initial.count,w.slice(id).count);
                const People moving=w.allocate_rate(id,"migration",available,
                    adjusted_rate(w,id,"migration",rules_.migration_ppm));
                if(!moving)continue;
                const auto& source=w.slice(id);
                const auto& econ=w.economic(source.key.economic);
                const auto& hh=w.household(source.key.household);
                auto next_d=d;next_d.region=dest_it->second;
                auto next_e=econ;next_e.region=dest_it->second;
                auto next_h=hh;next_h.region=dest_it->second;
                next_h.household_count_estimate=0; // no imaginary family count transfers
                const Id d_id=w.demographic_profile(next_d);
                const Id e_id=w.economic_profile(next_e);
                const Id h_id=w.household_profile(next_h);
                const People moving_jobs=job_allocation(w,id,moving);
                const Id arrival=w.transfer(id,moving,d_id,e_id,h_id,EventKind::migration,moving_jobs,
                                            "exogenous migration");
                // Jobs belong to employers in the origin region, not to the migrant.
                if(moving_jobs)w.release(arrival,moving_jobs);
                report.migrants=add_checked(report.migrants,moving);
            }
        }
    }
    void validate_rules()const{
        const std::array<People,14> r={rules_.fertility_ppm,rules_.infant_death_ppm,rules_.child_death_ppm,
            rules_.adult_death_ppm,rules_.senior_death_ppm,rules_.very_old_death_ppm,
            rules_.illness_incidence_ppm,rules_.illness_recovery_ppm,rules_.accident_ppm,
            rules_.injury_recovery_ppm,rules_.primary_access_ppm,rules_.secondary_access_ppm,
            rules_.tertiary_access_ppm,rules_.adult_literacy_access_ppm};
        for(People p:r)require(p>=0&&p<=PPM,"invalid probability in rules");
        for(People p:rules_.education_access_multiplier_ppm)
            require(p>=0&&p<=PPM,"invalid sex-specific school access");
        require(rules_.training_ppm>=0&&rules_.training_ppm<=PPM,"invalid training probability");
        require(rules_.accident_permanent_share_ppm>=0&&rules_.accident_permanent_share_ppm<=PPM,
                "invalid long-term injury probability");
        require(rules_.migration_ppm>=0&&rules_.migration_ppm<=PPM,"invalid migration probability");
        require(rules_.labor.minimum_age_months>=0&&rules_.labor.maximum_age_months>rules_.labor.minimum_age_months,
                "invalid labor ages");
    }
};

} // namespace vic25::phaseb
