#pragma once
#include "vic25/phase_b_types.hpp"

namespace vic25::phasec { struct SnapshotCodec; }
namespace vic25::phaseb {

class World {
    friend struct ::vic25::phasec::SnapshotCodec;
public:
    explicit World(std::int64_t month = 0) : month_(month) { default_health_ = health_profile(Health{}); default_education_ = education_profile(Education{}); }
    [[nodiscard]] std::int64_t month() const { return month_; }
    [[nodiscard]] Id default_health() const { return default_health_; }
    [[nodiscard]] Id default_education() const { return default_education_; }
    [[nodiscard]] const std::map<Id, Slice>& slices() const { return slices_; }
    [[nodiscard]] const std::vector<Event>& events() const { return events_; }
    [[nodiscard]] const std::map<Id, Employment>& jobs() const { return jobs_; }
    [[nodiscard]] const Demographic& demographic(Id id) const { return demographics_.at(id); }
    [[nodiscard]] const Economic& economic(Id id) const { return economics_.at(id); }
    [[nodiscard]] const Household& household(Id id) const { return households_.at(id); }
    [[nodiscard]] const Health& health(Id id) const { return health_.at(id); }
    [[nodiscard]] const Education& education(Id id) const { return educations_.at(id); }
    [[nodiscard]] const Slice& slice(Id id) const { return slices_.at(id); }
    [[nodiscard]] bool has_slice(Id id) const { return slices_.contains(id); }
    [[nodiscard]] const Employment* job(Id slice_id) const {
        auto it = jobs_.find(slice_id);
        return it == jobs_.end() ? nullptr : &it->second;
    }
    [[nodiscard]] People population() const {
        People n = 0; for (const auto& [id, s] : slices_) { (void)id; n = add_checked(n, s.count); } return n;
    }
    [[nodiscard]] People group_population(Id cohort_id, Id economic_id = 0, Id household_id = 0) const {
        People n = 0;
        for (const auto& [id, s] : slices_) {
            (void)id;
            if ((!cohort_id || s.key.demographic == cohort_id) && (!economic_id || s.key.economic == economic_id) &&
                (!household_id || s.key.household == household_id)) n = add_checked(n, s.count);
        }
        return n;
    }
    [[nodiscard]] People employed(Id slice_id) const { const auto* e = job(slice_id); return e ? e->count : 0; }
    [[nodiscard]] std::int64_t age_months(Id cohort_id) const { return month_ - demographic(cohort_id).birth_month; }

    Id health_profile(const Health& health) {
        auto it = health_ids_.find(health); if (it != health_ids_.end()) return it->second;
        const Id id = next_id_++; health_ids_[health] = id; health_[id] = health; return id;
    }
    Id education_profile(const Education& ed) {
        require(ed.literacy <= 100 && ed.occupational_skill <= 100, "invalid education score");
        auto it = education_ids_.find(ed); if (it != education_ids_.end()) return it->second;
        const Id id = next_id_++; education_ids_[ed] = id; educations_[id] = ed; return id;
    }
    Id demographic_profile(const Demographic& key) {
        require(!key.region.empty() && key.health && health_.contains(key.health), "invalid demographic profile");
        require(key.birth_month <= month_, "future birth cohort");
        auto it = demographic_ids_.find(key); if (it != demographic_ids_.end()) return it->second;
        const Id id = next_id_++; demographic_ids_[key] = id; demographics_[id] = key; return id;
    }
    Id economic_profile(const Economic& key) {
        require(!key.region.empty() && key.education && educations_.contains(key.education), "invalid economic profile");
        auto it = economic_ids_.find(key); if (it != economic_ids_.end()) return it->second;
        const Id id = next_id_++; economic_ids_[key] = id; economics_[id] = key; return id;
    }
    Id household_profile(const Household& key) {
        require(!key.region.empty() && key.household_count_estimate >= 0, "invalid household profile");
        auto it = household_ids_.find(key); if (it != household_ids_.end()) return it->second;
        const Id id = next_id_++; household_ids_[key] = id; households_[id] = key; return id;
    }
    // Explicitly create one population allocation. A repeated tuple coalesces.
    Id add_population(Id d, Id e, Id h, People count, Id partition = 1) {
        require(count > 0 && partition != 0, "invalid population addition");
        check_regions(d, e, h);
        const SliceKey key{d, e, h, partition};
        auto it = slice_ids_.find(key);
        if (it != slice_ids_.end()) {
            Slice& dest = slices_.at(it->second);
            const People value = add_checked(dest.count, count);
            dest.count = value; ++dest.revision; return dest.id;
        }
        const Id id = next_id_++;
        slices_.emplace(id, Slice{id, key, count, 1}); slice_ids_.emplace(key, id);
        return id;
    }
    // The only public headcount mutation routines are logged changes.
    Id seed_population(Id d, Id e, Id h, People count, Id partition = 1) {
        const Id id = add_population(d, e, h, count, partition);
        log(EventKind::initial_population, 0, id, count, 0, "synthetic/initial"); return id;
    }
    // Partial splitting of employed slices requires an explicit employment allocation.
    // Source job and destination job may differ: a new partition is chosen then.
    // High-level multi-event calls use a staged world; primitive transfers prevalidate.
    Id transfer(Id from_id, People count, Id d, Id e, Id h, EventKind reason,
                std::optional<People> jobs_moving = std::nullopt,
                std::string provenance = "explicit") {
        return transfer_impl(from_id, count, d, e, h, reason, jobs_moving, std::move(provenance));
    }
    Id split_partition(Id source,People count,People employed_moving=0){
        const auto old=slice(source);
        require(count<old.count && count>0,"partition split requires a proper subset");
        Id new_partition=next_partition_++;
        while(slice_ids_.contains(SliceKey{old.key.demographic,old.key.economic,old.key.household,new_partition}))
            new_partition=next_partition_++;
        return transfer_impl(source,count,old.key.demographic,old.key.economic,old.key.household,
                             EventKind::occupation,employed_moving,"allocation partition split",new_partition);
    }
    Id reclassify_economic(Id source,People count,const Economic& next,
                            std::optional<People> employed_moving=std::nullopt){
        const auto old=slice(source);
        const Id e=economic_profile(next);
        if(e==old.key.economic)return source;
        return transfer(source,count,old.key.demographic,e,old.key.household,EventKind::occupation,
                        employed_moving,"economic life-course transition");
    }
    Id change_profession(Id source,People count,std::string profession,std::string livelihood,
                         std::optional<People> employed_moving=std::nullopt){
        const auto old=slice(source);auto econ=economic(old.key.economic);
        econ.profession=std::move(profession);econ.livelihood=std::move(livelihood);
        const Id next_e=economic_profile(econ);
        if(next_e==old.key.economic)return source;
        return transfer(source,count,old.key.demographic,next_e,old.key.household,EventKind::occupation,employed_moving);
    }
    Id change_identity(Id source,People count,std::string culture,std::string religion,std::string legal,
                       std::optional<People> employed_moving=std::nullopt){
        const auto old=slice(source);auto demo=demographic(old.key.demographic);
        demo.culture=std::move(culture);demo.religion=std::move(religion);demo.legal_status=std::move(legal);
        const Id next_d=demographic_profile(demo);
        if(next_d==old.key.demographic)return source;
        return transfer(source,count,next_d,old.key.economic,old.key.household,
                        EventKind::occupation,employed_moving,"identity/legal change");
    }
    Id reassign_household(Id source,People count,Id dest_household,
                          std::optional<People> employed_moving=std::nullopt){
        const auto old=slice(source);
        if(dest_household==old.key.household)return source;
        return transfer(source,count,old.key.demographic,old.key.economic,dest_household,
                        EventKind::household_change,employed_moving);
    }
    Id set_service(Id source,People count,Service service,
                   std::optional<People> employed_moving=std::nullopt){
        const auto old=slice(source);auto demo=demographic(old.key.demographic);
        demo.service=service;const Id d=demographic_profile(demo);
        if(d==old.key.demographic)return source;
        const auto kind=service==Service::active?EventKind::enlistment:
                      service==Service::civilian?EventKind::discharge:EventKind::capture;
        return transfer(source,count,d,old.key.economic,old.key.household,kind,employed_moving);
    }
    void battle_losses(Id source,People killed,People acute,People long_term,People captured=0,People missing=0){
        require(killed>=0&&acute>=0&&long_term>=0&&captured>=0&&missing>=0,"negative battle casualty");
        const auto& src=slice(source);const auto& demo=demographic(src.key.demographic);
        require(demo.service==Service::active,"battle losses require active service");
        const People casualty=add_checked(add_checked(add_checked(killed,acute),add_checked(long_term,captured)),missing);
        require(casualty<=src.count,"battle losses exceed cohort");
        // All event effects are first applied to a private copy. Reject the entire battle on failure.
        World staged=*this;
        const auto original_hp=staged.health(demo.health);
        auto loss=[&](People n,EventKind kind,auto mutator){
            if(!n)return;
            const auto original=staged.slice(source);
            auto next=staged.demographic(original.key.demographic);
            mutator(next);
            const Id d=staged.demographic_profile(next);
            const People jobs_to_move=staged.employed(source)==0?0:
                          (n==original.count?staged.employed(source):0);
            staged.transfer(source,n,d,original.key.economic,original.key.household,
                            kind,jobs_to_move,"battle casualty");
        };
        if(killed)staged.death(source,killed,0,"battle");
        if(acute)loss(acute,EventKind::injury,[&](Demographic& d){auto hp=original_hp;hp.temporary_injury=true;
                     hp.injury_severity=2;d.health=staged.health_profile(hp);});
        if(long_term)loss(long_term,EventKind::injury,[&](Demographic& d){auto hp=original_hp;
                       hp.long_term_impairment=true;hp.limited_capabilities|=1U;
                       d.health=staged.health_profile(hp);});
        if(captured)loss(captured,EventKind::capture,[](Demographic& d){d.service=Service::pow;});
        if(missing)loss(missing,EventKind::capture,[](Demographic& d){d.service=Service::missing;});
        *this=std::move(staged);
    }
    // Ensure a retried command cannot apply the same externally identified event twice.
    template<class Fn>
    void apply_once(const std::string& event_key,Fn&& command){
        require(!event_key.empty()&&!external_events_.contains(event_key),"duplicate event key");
        World staged=*this;
        command(staged);
        require(staged.validate(),"event breaks population invariants");
        staged.external_events_.insert(event_key);
        *this=std::move(staged);
    }
    // Birth uses the same household pool as the source mother, but nonworker economics.
    void birth(Id household_id, const Demographic& baby, Id nonworker_economic, Sex sex, People count) {
        require(count > 0, "invalid births");
        Demographic newborn = baby; newborn.birth_month = month_; newborn.sex = sex;
        newborn.service = Service::civilian; newborn.health = default_health_;
        const Id d = demographic_profile(newborn);
        const Id to = add_population(d, nonworker_economic, household_id, count);
        log(EventKind::birth, 0, to, count, 0, "birth");
    }
    void death(Id source, People count, std::optional<People> employed_affected = std::nullopt, std::string cause = "natural") {
        require(count > 0 && slices_.contains(source) && slices_.at(source).count >= count, "invalid deaths");
        const auto s = slice(source); const People employed_src = employed(source);
        if (!employed_affected) {
            require(employed_src == 0 || count == s.count, "death of employed cohort requires employment allocation");
            employed_affected = count == s.count ? employed_src : 0;
        }
        require(*employed_affected >= 0 && *employed_affected <= employed_src &&
                count - *employed_affected <= s.count - employed_src, "invalid death employment allocation");
        if (count == s.count) { slices_.erase(source); slice_ids_.erase(s.key); jobs_.erase(source); }
        else { auto& current = slices_.at(source); current.count -= count; ++current.revision;
               if (*employed_affected) { auto& j=jobs_.at(source); j.count -= *employed_affected;
                                        if (!j.count) jobs_.erase(source); } }
        log(EventKind::death, source, 0, count, *employed_affected, std::move(cause));
    }
    Id change_health(Id slice_id, People count, const Health& health, EventKind kind = EventKind::illness,
                     std::optional<People> employed_allocation = std::nullopt) {
        const auto old = slice(slice_id); auto key = demographic(old.key.demographic);
        key.health = health_profile(health);
        Id d = demographic_profile(key);
        if (d == old.key.demographic) return slice_id;
        return transfer(slice_id, count, d, old.key.economic, old.key.household, kind, employed_allocation);
    }
    Id change_education(Id slice_id, People count, const Education& ed, EventKind kind=EventKind::schooling,
                        std::optional<People> employed_allocation=std::nullopt) {
        const auto old = slice(slice_id); auto key = economic(old.key.economic);
        key.education = education_profile(ed); const Id e = economic_profile(key);
        if (e == old.key.economic) return slice_id;
        return transfer(slice_id, count, old.key.demographic, e, old.key.household, kind, employed_allocation);
    }
    Id migrate(Id slice_id, People count, const std::string& region, Id destination_household,
               std::optional<People> employed_allocation=std::nullopt) {
        const auto old = slice(slice_id); auto d = demographic(old.key.demographic); auto e = economic(old.key.economic);
        d.region=region; e.region=region;
        const Id next_d=demographic_profile(d), next_e=economic_profile(e);
        return transfer(slice_id,count,next_d,next_e,destination_household,EventKind::migration,employed_allocation);
    }
    void hire(Id slice_id, People count, std::string employer, std::string occupation,
              std::int64_t wage, const JobRequirements& r, const LaborRule& policy={}) {
        require(count > 0 && wage >= 0 && !employer.empty(), "invalid hiring");
        require(eligible_for(slice_id,r,policy), "not qualified for job");
        const auto& s = slice(slice_id);
        require(count <= s.count - employed(slice_id), "insufficient unemployed persons");
        auto it = jobs_.find(slice_id);
        if(it != jobs_.end()) {
            require(it->second.employer==employer && it->second.occupation==occupation &&
                    it->second.wage_per_month==wage && it->second.requirements==r,
                    "job conflict: split slice first");
            it->second.count=add_checked(it->second.count,count);
        } else jobs_[slice_id] = Employment{std::move(employer),std::move(occupation),count,wage,r};
        log(EventKind::employment,slice_id,slice_id,count,count,"hire");
    }
    void release(Id slice_id, People count) {
        require(count > 0 && jobs_.contains(slice_id) && jobs_.at(slice_id).count >= count,
                "invalid job release");
        jobs_.at(slice_id).count -= count;
        if(jobs_.at(slice_id).count == 0) jobs_.erase(slice_id);
        log(EventKind::release,slice_id,slice_id,count,count,"job release");
    }
    People match_jobs(std::vector<Vacancy> vacancies,const LaborRule& policy={}){
        std::sort(vacancies.begin(),vacancies.end(),[](const Vacancy& a,const Vacancy& b){
            return std::tuple{-a.wage_per_month,a.employer,a.profession}<
                   std::tuple{-b.wage_per_month,b.employer,b.profession};
        });
        People matched=0;
        for(auto& vacancy:vacancies){
            require(vacancy.slots>=0&&vacancy.wage_per_month>=0,"invalid vacancy");
            for(auto it=slices_.begin();it!=slices_.end()&&vacancy.slots>0;){
                const Id id=it->first; ++it;
                const auto& econ=economic(slice(id).key.economic);
                if(econ.profession!=vacancy.profession || !eligible_for(id,vacancy.requirements,policy))continue;
                const auto* prior=job(id);
                if(prior&&(prior->employer!=vacancy.employer ||
                           prior->occupation!=vacancy.profession ||
                           prior->wage_per_month!=vacancy.wage_per_month ||
                           prior->requirements!=vacancy.requirements)){
                    const People available=slice(id).count-prior->count;
                    if(available<=0)continue;
                    const People count=std::min(available,vacancy.slots);
                    const Id partition=split_partition(id,count,0);
                    hire(partition,count,vacancy.employer,vacancy.profession,vacancy.wage_per_month,vacancy.requirements,policy);
                    vacancy.slots-=count;matched=add_checked(matched,count);
                    continue;
                }
                const People n=std::min(slice(id).count-employed(id),vacancy.slots);
                if(n<=0)continue;
                hire(id,n,vacancy.employer,vacancy.profession,vacancy.wage_per_month,vacancy.requirements,policy);
                vacancy.slots-=n;matched=add_checked(matched,n);
            }
        }
        require(validate(policy),"invalid labor matching state");
        return matched;
    }
    [[nodiscard]] bool eligible_for(Id slice_id, const JobRequirements& r, const LaborRule& policy={}) const {
        const auto& s=slice(slice_id);
        const auto& d=demographic(s.key.demographic);
        const auto& ed=education(economic(s.key.economic).education);
        const auto& hp=health(d.health);
        const auto age=age_months(s.key.demographic);
        if(age < std::max(policy.minimum_age_months,r.minimum_age_months) ||
           age >= std::min(policy.maximum_age_months,r.maximum_age_months))return false;
        if(d.service != Service::civilian && d.service != Service::reserve)return false;
        const auto sex_flag = static_cast<std::uint8_t>(1U << static_cast<std::uint8_t>(d.sex));
        if(!(policy.allowed_sexes_mask & sex_flag) || !(r.allowed_sexes_mask & sex_flag))return false;
        if(ed.school < r.minimum_school || ed.occupational_skill < r.minimum_skill) return false;
        const std::uint32_t temporary_limits=hp.temporary_injury ? std::uint32_t{1} : std::uint32_t{0};
        const auto unaccommodated=(hp.limited_capabilities | temporary_limits) & ~r.accommodation_mask;
        return (r.required_capabilities & unaccommodated)==0;
    }
    void release_ineligible(const LaborRule& policy={}){
        std::vector<Id> bad;
        for(const auto& [id,job]:jobs_)if(!eligible_for(id,job.requirements,policy))bad.push_back(id);
        for(Id id:bad)release(id,employed(id));
    }
    [[nodiscard]] LaborReport labor(const LaborRule& rule={}) const {
        LaborReport result; result.population=population();
        for(const auto& [id,s]:slices_){
            const auto& d=demographic(s.key.demographic);
            const auto& e=economic(s.key.economic);
            const auto age=age_months(s.key.demographic);
            if(age<rule.minimum_age_months || age>=rule.maximum_age_months){ result.inactive=add_checked(result.inactive,s.count);continue; }
            result.working_age=add_checked(result.working_age,s.count);
            const auto sex_flag = static_cast<std::uint8_t>(1U << static_cast<std::uint8_t>(d.sex));
            if((d.service != Service::civilian && d.service != Service::reserve) ||
               !(rule.allowed_sexes_mask & sex_flag)) {
                result.inactive=add_checked(result.inactive,s.count);continue;
            }
            result.eligible=add_checked(result.eligible,s.count);
            const auto emp=employed(id);
            result.employed=add_checked(result.employed,emp);
            if(e.livelihood=="self_employed" || e.livelihood=="subsistence") {
                result.self_employed=add_checked(result.self_employed,s.count-emp);
            } else if(e.livelihood=="inactive") result.inactive=add_checked(result.inactive,s.count-emp);
            else result.unemployed=add_checked(result.unemployed,s.count-emp);
        }
        return result;
    }
    [[nodiscard]] HouseholdReport household_report(Id household_id) const {
        HouseholdReport result;
        (void)household(household_id);
        for(const auto& [id,s]:slices_){
            if(s.key.household != household_id)continue;
            const auto& d=demographic(s.key.demographic);
            const auto& e=economic(s.key.economic);
            const auto& health_now=health(d.health);
            const auto age=age_months(s.key.demographic);
            result.people=add_checked(result.people,s.count);
            if(age<18*12){result.children=add_checked(result.children,s.count);result.dependents=add_checked(result.dependents,s.count);}
            else if(age>=65*12){result.elder=add_checked(result.elder,s.count);result.dependents=add_checked(result.dependents,s.count);}
            const People workers=employed(id);result.employed=add_checked(result.employed,workers);
            const auto* contract=job(id);
            if(contract)result.wage_income=add_checked(result.wage_income,mul_checked(workers,contract->wage_per_month));
            if(e.livelihood=="subsistence") result.subsistence_value=add_checked(result.subsistence_value,mul_checked(s.count-workers,5));
            result.food_need=add_checked(result.food_need,mul_checked(s.count,age<10*12?2:3));
            result.clothing_need=add_checked(result.clothing_need,s.count);
            // Housing common cost is applied once per estimated household below.

            const People care_persons=(health_now.ill||health_now.temporary_injury)?s.count:0;
            result.medical_need=add_checked(result.medical_need,mul_checked(care_persons,4));
        }
        const People h_estimate=household(household_id).household_count_estimate;
        // One shared base unit per household plus occupancy-sensitive units.
        result.housing_need=add_checked(std::min(result.people,h_estimate),result.people);
        // Example accounting units only: needs, not delivered goods/market pricing.
        result.planned_basket_value=add_checked(add_checked(result.food_need,result.clothing_need),
                                               add_checked(result.housing_need,result.medical_need));
        return result;
    }
    [[nodiscard]] bool validate(const LaborRule& policy={}) const {
        try {
            People total=0;
            if(slices_.size()!=slice_ids_.size()) return false;
            for(const auto& [id,s]:slices_){
                if(id!=s.id||s.count<=0||!slice_ids_.contains(s.key)||slice_ids_.at(s.key)!=id)return false;
                check_regions(s.key.demographic,s.key.economic,s.key.household);
                const auto* j=job(id);
                if(j && (j->count<=0 || j->count>s.count || !eligible_for(id,j->requirements,policy)))return false;
                total=add_checked(total,s.count);
            }
            for(const auto& [id,job]:jobs_) if(!slices_.contains(id)||job.count<=0)return false;
            if(total<0 || next_id_==0)return false;
            return true;
        }catch(const std::exception&){return false;}
    }
    // Called exclusively on the staged world just before a successful month commit.
    void finish_month() { ++month_; }
    // deterministic, conservative fixed-point count (rate = per million people per month).
    People allocate_rate(Id id, const std::string& category, People exposed, People ppm) {
        require(ppm>=0 && ppm<=PPM && exposed>=0,"invalid rate");
        const auto key=std::make_pair(id,category);
        const People carry = residual_[key];
        const People part = (exposed%PPM)*ppm + carry;
        const People whole = add_checked((exposed/PPM)*ppm,part/PPM);
        residual_[key]=part%PPM;
        return std::min(exposed,whole);
    }
    void prune_residuals() {
        for(auto it=residual_.begin();it!=residual_.end();){
            if(!slices_.contains(it->first.first)) it=residual_.erase(it); else ++it;
        }
    }
    // Deterministic digest for regression tests. No reliance on unordered iteration.
    [[nodiscard]] std::uint64_t fingerprint() const {
        std::uint64_t value=1469598103934665603ULL;
        const auto add=[&](std::uint64_t x){ value^=x;value*=1099511628211ULL; };
        const auto str=[&](const std::string& x){
            add(x.size());for(unsigned char c:x)add(static_cast<std::uint64_t>(c));
        };
        add(static_cast<std::uint64_t>(month_)); add(next_id_);add(next_partition_);
        for(const auto& [id,h]:health_){add(id);add(h.ill);add(h.temporary_injury);
            add(h.long_term_impairment);add(h.limited_capabilities);add(h.illness_severity);add(h.injury_severity);}
        for(const auto& [id,e]:educations_){add(id);add(static_cast<unsigned>(e.school));
            add(e.literacy);add(e.occupational_skill);}
        for(const auto& [id,d]:demographics_){add(id);str(d.region);add(static_cast<std::uint64_t>(d.birth_month));
            add(static_cast<unsigned>(d.sex));str(d.culture);str(d.religion);str(d.citizenship);
            str(d.legal_status);add(d.health);add(static_cast<unsigned>(d.service));}
        for(const auto& [id,e]:economics_){add(id);str(e.region);str(e.profession);
            str(e.livelihood);str(e.labor_class);add(e.education);}
        for(const auto& [id,h]:households_){add(id);str(h.region);str(h.composition);
            str(h.pooling_policy);add(static_cast<std::uint64_t>(h.household_count_estimate));}
        for(const auto& [id,s]:slices_){add(id);add(s.key.demographic);add(s.key.economic);
            add(s.key.household);add(s.key.partition);add(static_cast<std::uint64_t>(s.count));add(s.revision);}
        for(const auto& [id,j]:jobs_){add(id);str(j.employer);str(j.occupation);
            add(static_cast<std::uint64_t>(j.count));add(static_cast<std::uint64_t>(j.wage_per_month));
            add(static_cast<unsigned>(j.requirements.minimum_school));
            add(j.requirements.minimum_skill);add(j.requirements.required_capabilities);
            add(j.requirements.accommodation_mask);
            add(static_cast<std::uint64_t>(j.requirements.minimum_age_months));
            add(static_cast<std::uint64_t>(j.requirements.maximum_age_months));
            add(j.requirements.allowed_sexes_mask);
        }
        for(const auto& [key,remainder]:residual_){add(key.first);str(key.second);add(static_cast<std::uint64_t>(remainder));}
        for(const auto& key:external_events_)str(key);
        for(const auto& ev:events_){add(ev.id);add(static_cast<std::uint64_t>(ev.month));
            add(static_cast<unsigned>(ev.kind));add(ev.from);add(ev.to);
            add(static_cast<std::uint64_t>(ev.count));add(static_cast<std::uint64_t>(ev.employed_affected));
            str(ev.provenance);}
        return value;
    }
private:
    Id next_id_{1},next_partition_{10'000},default_health_{},default_education_{};
    std::int64_t month_{};
    std::map<Health,Id> health_ids_;
    std::map<Id,Health> health_;
    std::map<Education,Id> education_ids_;
    std::map<Id,Education> educations_;
    std::map<Demographic,Id> demographic_ids_;
    std::map<Id,Demographic> demographics_;
    std::map<Economic,Id> economic_ids_;
    std::map<Id,Economic> economics_;
    std::map<Household,Id> household_ids_;
    std::map<Id,Household> households_;
    std::map<SliceKey,Id> slice_ids_;
    std::map<Id,Slice> slices_;
    std::map<Id,Employment> jobs_;
    std::vector<Event> events_;
    std::set<std::string> external_events_;
    std::map<std::pair<Id,std::string>,People> residual_;
    void check_regions(Id d,Id e,Id h)const{
        require(demographics_.contains(d)&&economics_.contains(e)&&households_.contains(h),"unknown population references");
        require(demographics_.at(d).region==economics_.at(e).region &&
                demographics_.at(d).region==households_.at(h).region,"population region mismatch");
    }
    void log(EventKind kind,Id from,Id to,People count,People employed_affected,std::string provenance){
        events_.push_back(Event{next_id_++,month_,kind,from,to,count,employed_affected,std::move(provenance)});
    }
    Id transfer_impl(Id from_id,People count,Id d,Id e,Id h,EventKind reason,
                     std::optional<People> jobs_moving,std::string provenance,Id partition_override=0){
        require(count>0&&slices_.contains(from_id),"invalid transfer source");
        const Slice old=slice(from_id);
        require(count<=old.count,"insufficient transfer source count");
        check_regions(d,e,h);
        const People prior_employed=employed(from_id);
        if(!jobs_moving){
            require(prior_employed==0||count==old.count,"partial employed transfer requires allocation");
            jobs_moving=count==old.count?prior_employed:0;
        }
        const People n_jobs=*jobs_moving;
        require(n_jobs>=0&&n_jobs<=prior_employed&&count-n_jobs<=old.count-prior_employed,"invalid employment allocation");
        const Employment prior_job=prior_employed?jobs_.at(from_id):Employment{};
        SliceKey next_key{d,e,h,partition_override?partition_override:old.key.partition};
        require(!(next_key==old.key),"same destination group");
        auto existing=slice_ids_.find(next_key);
        if(existing!=slice_ids_.end()){
            const auto* other=job(existing->second);
            if(n_jobs>0 && other &&
                (other->employer!=prior_job.employer||other->occupation!=prior_job.occupation||other->wage_per_month!=prior_job.wage_per_month ||
                 other->requirements!=prior_job.requirements)){
                do { next_key.partition=next_partition_++; } while(slice_ids_.contains(next_key));
                existing=slice_ids_.find(next_key);
            }
        }
        if(existing!=slice_ids_.end()){
            (void)add_checked(slice(existing->second).count,count);
            (void)add_checked(employed(existing->second),n_jobs);
        }
        if(count==old.count){slice_ids_.erase(old.key);slices_.erase(from_id);jobs_.erase(from_id);}
        else {auto& current=slices_.at(from_id);current.count-=count;++current.revision;
              if(n_jobs){auto& j=jobs_.at(from_id);j.count-=n_jobs;if(j.count==0)jobs_.erase(from_id);}}
        const Id dest=add_population(d,e,h,count,next_key.partition);
        if(n_jobs){
            auto& j=jobs_[dest];
            if(j.count==0){j=prior_job;j.count=0;}
            j.count=add_checked(j.count,n_jobs);
        }
        // Move fractional demographic-risk credits with the actual people.
        // A split must not silently erase the risk/exposure remainder.
        std::vector<std::pair<std::string,People>> carry_updates;
        for(const auto& [key,remainder]:residual_){
            if(key.first!=from_id || remainder==0)continue;
            const People moved=count==old.count?remainder:mul_checked(remainder,count)/old.count;
            if(moved)carry_updates.emplace_back(key.second,moved);
        }
        for(const auto& [category,moved]:carry_updates){
            const auto origin=std::make_pair(from_id,category);
            const auto target=std::make_pair(dest,category);
            residual_[origin]-=moved;
            residual_[target]=add_checked(residual_[target],moved);
        }
        if(n_jobs && !eligible_for(dest,jobs_.at(dest).requirements)){
            // A demographic change can invalidate physical or legal job eligibility.
            // Release the affected workers, retaining their profession and headcount.
            release(dest,n_jobs);
        }
        log(reason,from_id,dest,count,n_jobs,std::move(provenance));
        return dest;
    }
};

} // namespace vic25::phaseb
