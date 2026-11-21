#pragma once
// Bureaucracy is a derived national flow, not a money account. The office
// method, building capacity, POP workers, incorporation and actual owner are
// all explicit game-state inputs. No fictitious admin headcount is generated.
namespace vic25::phasec {

inline void IntegratedWorld::p1_set_incorporation(
        const std::string& region,bool incorporated) {
    ensure((stage_==-1&&commit_id_==0)||stage_==9,
           "P1 incorporation must be declared at setup or political stage");
    ensure(sovereignty_.contains(region),
           "P1 incorporation must reference sovereign territory");
    p1_incorporation_[region]=incorporated;
    events_.push_back("p1-real-territory-incorporation:"+region);
}
inline void IntegratedWorld::p1_register_office(P1OfficeEvidence office) {
    ensure((stage_==-1&&commit_id_==0)||stage_==9,
           "P1 government office requires setup or political stage");
    ensure(!office.institution.empty()&&!office.site.empty()&&
           office.levels>0&&office.workers_per_level>0&&
           office.method<=P1OfficeMethod::switchboards&&
           p1_institutions_.contains(office.institution)&&
           p1_institutions_.at(office.institution).kind==
               P1InstitutionKind::administration&&
           public_institutions_.contains(office.institution)&&
           facilities_.contains(office.site)&&
           !p1_offices_.contains(office.institution),
           "P1 unsupported or duplicate physical government office");
    const auto& service=public_institutions_.at(office.institution);
    const auto& site=facilities_.at(office.site);
    ensure(site.region==service.region&&site.owner==service.operator_id&&
           site.capacity>=office.levels,
           "P1 government office must be backed by an actual owned building");
    for(const auto& [id,prior]:p1_offices_) {
        (void)id;
        ensure(prior.site!=office.site&&
               public_institutions_.at(prior.institution).operator_id!=
                   service.operator_id,
               "P1 cannot allocate one office or payroll twice");
    }
    p1_offices_.emplace(office.institution,std::move(office));
}
inline void IntegratedWorld::p1_register_national_building_level(
        const std::string& site,std::uint32_t levels) {
    ensure((stage_==-1&&commit_id_==0)||stage_==9,
           "P1 public site level requires setup or political stage");
    ensure(!site.empty()&&facilities_.contains(site)&&
           levels>0&&levels<=facilities_.at(site).capacity,
           "P1 government building level not physically backed");
    p1_public_building_levels_[site]=levels;
}
inline void IntegratedWorld::p1_register_commander(P1CommanderEvidence officer) {
    ensure((stage_==-1&&commit_id_==0)||stage_==9,
           "P1 commander registration requires political stage");
    ensure(!officer.id.empty()&&!officer.unit.empty()&&
           !officer.anchor.empty()&&officer.cost>=0&&
           officer.cost<=1000&&
           !p1_commanders_.contains(officer.id)&&
           person_anchors_.contains(officer.anchor)&&
           army_deployments_.contains(officer.unit),
           "P1 administrative officer must be an existing person and unit");
    for(const auto& [id,prior]:p1_commanders_) {
        (void)id;
        ensure(prior.anchor!=officer.anchor&&prior.unit!=officer.unit,
               "P1 same commander or military unit has duplicate cost");
    }
    p1_commanders_.emplace(officer.id,std::move(officer));
}
inline void IntegratedWorld::phase_time_consume_administration_inputs() {
    require_stage({7});
    const auto day=people_.native_calendar_day();
    if(day<=0)return; // legacy month engine is not re-executed daily
    const auto year=month()/12;
    const int mo=static_cast<int>(month()%12)+1;
    const bool leap=year%4==0&&(year%100!=0||year%400==0);
    const int dim=mo==2?(leap?29:28):
        ((mo==4||mo==6||mo==9||mo==11)?30:31);
    const auto usage=[&](std::int64_t monthly)->Quantity {
        return (monthly*day)/dim-(monthly*(day-1))/dim;
    };
    for(const auto& [id,office]:p1_offices_) {
        if(p1_office_consumed_.contains(id)&&
           p1_office_consumed_.at(id)==std::make_pair(month(),day))continue;
        const auto& service=public_institutions_.at(id);
        const auto& site=facilities_.at(office.site);
        if(site.capacity<site.damaged||
           site.capacity-site.damaged<office.levels||
           employed_by(service.operator_id)<=0)continue;
        const auto paper=usage(phaseb::mul_checked(
            office.levels,office.method==P1OfficeMethod::simple?0:
            office.method==P1OfficeMethod::horizontal?10:20));
        const auto phones=usage(phaseb::mul_checked(
            office.levels,office.method==P1OfficeMethod::switchboards?5:0));
        if((paper>0&&stock(service.operator_id,service.region,"paper")<paper)||
           (phones>0&&stock(service.operator_id,service.region,"telephones")<phones))
            continue;
        if(paper>0) {
            if(phase_d_reservation_mode_)
                resources_.assert_goods_available(
                    {service.operator_id,service.region,"paper"},
                    paper,stock(service.operator_id,service.region,"paper"));
            goods_.consume({service.operator_id,service.region,"paper"},paper);
        }
        if(phones>0) {
            if(phase_d_reservation_mode_)
                resources_.assert_goods_available(
                    {service.operator_id,service.region,"telephones"},
                    phones,stock(service.operator_id,service.region,"telephones"));
            goods_.consume({service.operator_id,service.region,"telephones"},phones);
        }
        p1_office_consumed_[id]={month(),day};
        events_.push_back("p1-real-government-office-inputs:"+id);
    }
}

inline P1BureaucracyView IntegratedWorld::p1_bureaucracy(
        const std::string& country)const {
    ensure(governments_.contains(country),
           "P1 bureaucracy requires an actual sovereign government");
    P1BureaucracyView v;
    v.country=country;
    bool known=true;
    // E assigns geographical regions; C is sovereign and owns their
    // incorporation law. Unknown incorporation is not assumed true or false.
    std::map<std::string,People> actual_population;
    for(const auto& [id,row]:people_.slices()) {
        (void)id;
        const auto& dem=people_.demographic(row.key.demographic);
        if(auto owner=sovereignty_.find(dem.region);
           owner!=sovereignty_.end()&&owner->second==country)
            actual_population[dem.region]=phaseb::add_checked(
                actual_population[dem.region],row.count);
    }
    // Base administrative cost per incorporated POP is law-sensitive.
    // The flat state overhead is unchanged; hereditary/local bureaucracy
    // and other enacted policy modifiers affect only population base cost.
    constexpr auto population_cost_key=
        "state_bureaucracy_population_base_cost_factor_mult";
    std::int64_t legal_population_cost=0;
    // The eight direct rows belong to these recognized national slots.
    // A local or custom lookalike cannot change every incorporated POP's cost.
    for(const auto* group:{"bureaucracy","caste_hegemony","taxation","economic_system"})
        if(const auto selected=p2_selected_law(country,group))
            legal_population_cost=phaseb::add_checked(
                legal_population_cost,static_cast<std::int64_t>(std::llround(
                    p2_catalog_raw_modifier(p2_catalog_canonical_choice(*selected),
                                            population_cost_key)*1'000'000.0)));
    const auto population_cost_factor=std::clamp<std::int64_t>(
        phaseb::add_checked(1'000'000,phaseb::add_checked(
            legal_population_cost,p3_trait_modifier_ppm(country,population_cost_key))),
        0,5'000'000);
    for(const auto& [region,owner]:sovereignty_) {
        if(owner!=country)continue;
        auto it=p1_incorporation_.find(region);
        if(it==p1_incorporation_.end()) {
            known=false;
            continue;
        }
        if(!it->second)continue;
        const People population=actual_population[region];
        v.incorporated_population=phaseb::add_checked(
            v.incorporated_population,population);
        ++v.incorporated_regions;
        v.state_cost=phaseb::add_checked(v.state_cost,10);
        // 1.15 STATE_BUREAUCRACY_POP_MULTIPLE=100000,
        // POP_BASE_COST=4. Integer ceilings avoid ignoring small regions.
        const auto base_population_cost=phaseb::add_checked(
            phaseb::mul_checked(population/100'000,4),
            (phaseb::mul_checked(population%100'000,4)+99'999)/100'000);
        // Fixed-point ceiling: a small but incorporated population still
        // incurs real administration demand, without rounding every
        // policy multiplier to zero before its national aggregation.
        const auto scaled_population_cost=phaseb::add_checked(
            phaseb::mul_checked(base_population_cost/1'000'000,
                                population_cost_factor),
            (phaseb::mul_checked(base_population_cost%1'000'000,
                                 population_cost_factor)+999'999)/1'000'000);
        v.state_cost=phaseb::add_checked(
            v.state_cost,scaled_population_cost);
    }
    for(const auto& [id,link]:p1_institutions_)
        if(link.country==country&&
           link.kind==P1InstitutionKind::administration&&
           !p1_offices_.contains(id))known=false;
    for(const auto& [id,office]:p1_offices_) {
        const auto& link=p1_institutions_.at(id);
        if(link.country!=country)continue;
        const auto& service=public_institutions_.at(id);
        const auto& site=facilities_.at(office.site);
        if(site.capacity<site.damaged||
           site.capacity-site.damaged<office.levels)continue;
        const auto workers=employed_by(service.operator_id);
        const auto day=people_.native_calendar_day();
        if(day>0&&office.method!=P1OfficeMethod::simple) {
            const auto year=month()/12;
            const auto mo=static_cast<int>(month()%12)+1;
            const bool leap=year%4==0&&(year%100!=0||year%400==0);
            const int dim=mo==2?(leap?29:28):
                ((mo==4||mo==6||mo==9||mo==11)?30:31);
            const auto usage=[&](std::int64_t monthly)->Quantity {
                return (monthly*day)/dim-(monthly*(day-1))/dim;
            };
            const auto paper=usage(phaseb::mul_checked(
                office.levels,office.method==P1OfficeMethod::horizontal?10:20));
            const auto phones=usage(phaseb::mul_checked(
                office.levels,office.method==P1OfficeMethod::switchboards?5:0));
            const bool consumed=p1_office_consumed_.contains(id)&&
                p1_office_consumed_.at(id)==std::make_pair(month(),day);
            if(!consumed&&
               ((paper>0&&stock(service.operator_id,service.region,"paper")<paper)||
                (phones>0&&stock(service.operator_id,service.region,"telephones")<phones)))
                continue;
        }
        const auto needed=phaseb::mul_checked(office.levels,
                                             office.workers_per_level);
        const auto productive=std::min(workers,needed);
        const bool funded=cash(service.operator_id)>0||
            (has_country_treasury(country)&&cash(country_treasury(country))>0);
        if(!funded||productive<=0)continue;
        const std::int64_t per_level=office.method==P1OfficeMethod::simple?10:
            office.method==P1OfficeMethod::horizontal?50:
            office.method==P1OfficeMethod::vertical?65:100;
        const auto nominal=phaseb::mul_checked(per_level,office.levels);
        const auto supply=phaseb::add_checked(
            phaseb::mul_checked(nominal/needed,productive),
            phaseb::mul_checked(nominal%needed,productive)/needed);
        v.office_output=phaseb::add_checked(v.office_output,supply);
    }
    // Distinct P1 institutions are only charged once per legal family, not
    // once per regional facility. A source-less institution level is unknown.
    std::map<P1InstitutionKind,std::uint8_t> institutions;
    for(const auto& [id,link]:p1_institutions_) {
        (void)id;
        if(link.country!=country||
           link.kind==P1InstitutionKind::administration||
           link.kind==P1InstitutionKind::elections)continue;
        institutions[link.kind]=std::max(institutions[link.kind],link.level);
    }
    // Here "investment" means the recurring bureaucracy committed by the
    // existing institution level, not an upgrade fee or a treasury outlay.
    std::int64_t general_institution_cost_ppm=0;
    if(!institutions.empty()) {
        constexpr std::string_view key=
            "country_bureaucracy_investment_cost_factor_mult";
        constexpr const char* specialists="amendment_bourgeois_specialists";
        const auto raw_ppm=[](std::string_view source,std::string_view effect) {
            return static_cast<std::int64_t>(std::llround(
                p2_catalog_raw_modifier(source,effect)*1'000'000.0));
        };
        for(const auto& [slot,id]:p2_active_base_) {
            (void)slot;
            const auto& law=p2_laws_.at(id);
            if(law.country!=country||law.tier==P2NormTier::local_ordinance||
               (!law.scope.empty()&&law.scope!="national")||
               politics::bare_group(law.domain)!="bureaucracy"||
               !politics::catalogue().has(law.domain,law.base_choice))
                continue;
            const auto choice=p2_catalog_canonical_choice(law.base_choice);
            if(choice=="law_elected_bureaucrats")
                general_institution_cost_ppm=raw_ppm(choice,key);
            if(p2_active_amendments_.contains(specialists)&&
               p2_amendments_.at(specialists).law_id==id) {
                const auto* source=p2_catalog_amendment(specialists);
                if(source&&p2_catalog_amendment_applies(*source,choice))
                    general_institution_cost_ppm=phaseb::add_checked(
                        general_institution_cost_ppm,raw_ppm(specialists,key));
            }
            break; // Only one recognized active national bureaucracy slot.
        }
    }
    // Specific school and health rates are scoped to actual national laws.
    // The link level above is the recurring administration commitment even
    // when a site's independent service delivery is unfunded or unstaffed.
    const auto service_base_cost_ppm=[&](const char* group,const char* key) {
        for(const auto& [slot,id]:p2_active_base_) {
            (void)slot;
            const auto& law=p2_laws_.at(id);
            if(law.country!=country||law.tier==P2NormTier::local_ordinance||
               (!law.scope.empty()&&law.scope!="national")||
               politics::bare_group(law.domain)!=group||
               !politics::catalogue().has(law.domain,law.base_choice))continue;
            return static_cast<std::int64_t>(std::llround(
                p2_catalog_raw_modifier(
                    p2_catalog_canonical_choice(law.base_choice),key)*1'000'000.0));
        }
        return std::int64_t{0};
    };
    for(const auto& [kind,level]:institutions) {
        const auto rate=phaseb::add_checked(
            phaseb::mul_checked(v.incorporated_population/100'000,10),
            phaseb::mul_checked(v.incorporated_population%100'000,10)/
            100'000);
        const char* law_key=nullptr;
        switch(kind) {
            case P1InstitutionKind::healthcare:
                law_key="country_institution_cost_institution_health_system_mult";
                break;
            case P1InstitutionKind::education:
                law_key="country_institution_cost_institution_schools_mult";
                break;
            case P1InstitutionKind::policing:
                law_key="country_institution_cost_institution_home_affairs_mult";
                break;
            case P1InstitutionKind::welfare:
                law_key="country_institution_cost_institution_social_security_mult";
                break;
            default:break;
        }
        auto cost=phaseb::mul_checked(rate,level);
        if(law_key||general_institution_cost_ppm!=0) {
            // Welfare's two workhouse discounts have exact national
            // poor-law parents. Generic same-ID attachments cannot reduce
            // real operating commitments; leave other cost paths intact.
            std::int64_t legal_specific=0;
            switch(kind) {
                case P1InstitutionKind::healthcare:
                    legal_specific=service_base_cost_ppm("health_system",law_key);
                    break;
                case P1InstitutionKind::education:
                    legal_specific=service_base_cost_ppm("education_system",law_key);
                    for(const auto* id:{"amendment_ragged_schools",
                            "amendment_co_ed_schools","amendment_single_sex_education",
                            "amendment_provided_school_supplies",
                            "amendment_unfunded_school_supplies",
                            "amendment_secular_school_system"})
                        legal_specific=phaseb::add_checked(legal_specific,
                            p2_active_national_enactment_points(country,id,law_key));
                    break;
                case P1InstitutionKind::welfare:
                    legal_specific=phaseb::add_checked(
                        p2_active_national_enactment_points(country,
                            "amendment_religious_workhouses",law_key),
                        p2_active_national_enactment_points(country,
                            "amendment_workhouse_regulations",law_key));
                    break;
                default:
                    if(law_key)legal_specific=p2_modifier_ppm(country,law_key);
                    break;
            }
            const auto specific=law_key?phaseb::add_checked(
                legal_specific,p3_trait_modifier_ppm(country,law_key)):0;
            // VIC2.5 adds same-cost percentage points before its existing
            // clamp and integer rounding, not sequential rounded discounts.
            const auto factor=std::clamp<std::int64_t>(
                phaseb::add_checked(1'000'000,phaseb::add_checked(
                    general_institution_cost_ppm,specific)),
                0,5'000'000);
            cost=phaseb::add_checked(
                phaseb::mul_checked(cost/1'000'000,factor),
                phaseb::mul_checked(cost%1'000'000,factor)/1'000'000);
        }
        v.institution_cost=phaseb::add_checked(v.institution_cost,cost);
    }
    // Only confirmed owned building levels are billable. Other sovereign
    // facilities require explicit source level data, never assumed to be 1.
    if(has_country_treasury(country)) {
        const auto& treasury=country_treasury(country);
        for(const auto& [site_id,site]:facilities_) {
            if(site.owner!=treasury)continue;
            const auto territory=sovereignty_.find(site.region);
            if(territory==sovereignty_.end()||
               territory->second!=country)continue;
            bool administrative=false;
            for(const auto& [id,office]:p1_offices_) {
                (void)id;
                if(office.site==site_id){administrative=true;break;}
            }
            if(administrative)continue;
            const auto l=p1_public_building_levels_.find(site_id);
            if(l==p1_public_building_levels_.end()){known=false;continue;}
            v.nationalized_cost=phaseb::add_checked(
                v.nationalized_cost,l->second);
        }
    }
    for(const auto& [id,officer]:p1_commanders_) {
        (void)id;
        if(auto deployment=army_deployments_.find(officer.unit);
           deployment!=army_deployments_.end()&&
           deployment->second.country==country)
            v.commander_cost=phaseb::add_checked(
                v.commander_cost,officer.cost);
    }
    constexpr const char* bureaucracy_key="country_bureaucracy_add";
    auto legal=phaseb::add_checked(
        p2_modifier_ppm(country,bureaucracy_key),
        p3_trait_modifier_ppm(country,bureaucracy_key));
    if(const auto welfare=p2_selected_law(country,"welfare");
       welfare && p2_catalog_canonical_choice(*welfare)=="law_chiefs_distribute_aid") {
        // Existing service capacity is independent of this bureaucracy
        // calculation. An unfunded, unstaffed or unavailable link cannot
        // turn an institutional law modifier into administrative output.
        int effective_level=0;
        for(const auto& [id,link]:p1_institutions_) {
            if(link.country!=country||link.kind!=P1InstitutionKind::welfare)continue;
            const auto& service=public_institutions_.at(id);
            const auto sovereign=sovereignty_.find(service.region);
            const auto incorporated=p1_incorporation_.find(service.region);
            if(sovereign==sovereignty_.end()||sovereign->second!=country||
               incorporated==p1_incorporation_.end()||!incorporated->second)
                continue;
            const auto& jobs=people_.jobs();
            const bool local_staff=std::any_of(jobs.begin(),jobs.end(),
                [&](const auto& entry) {
                    const auto& [slice,job]=entry;
                    return job.employer==service.operator_id&&
                        people_.demographic(people_.slice(slice).key.demographic)
                            .region==service.region&&
                        job.count>currently_striking(slice);
                });
            if(!local_staff)continue;
            const auto active=p1_institution_capacity(id);
            if(active.usable_slots>0)
                effective_level=std::max(effective_level,static_cast<int>(active.level));
        }
        if(effective_level>0) {
            const auto canonical=p2_catalog_canonical_choice(*welfare);
            // One national welfare institution, with no duplicate flat value.
            const auto delta=p2_catalog_raw_modifier(canonical,bureaucracy_key,effective_level)-
                             p2_catalog_raw_modifier(canonical,bureaucracy_key,0);
            legal=phaseb::add_checked(legal,
                static_cast<std::int64_t>(std::llround(delta*1'000'000.0)));
        }
    }
    const auto base=std::max<std::int64_t>(0,phaseb::add_checked(
        100,phaseb::add_checked(v.office_output,legal/1'000'000)));
    constexpr auto generation_key="country_bureaucracy_mult";
    // The flat generation penalty is attached to each amendment's active,
    // compatible national parent, not merely its catalogue identifier.
    const auto legal_generation=phaseb::add_checked(
        p2_active_national_enactment_points(country,"amendment_cult_of_personality",
                                           generation_key),
        p2_active_national_enactment_points(country,"amendment_open_legal_questions",
                                           generation_key));
    const auto pct=std::clamp<std::int64_t>(
        1'000'000+phaseb::add_checked(
            legal_generation,p3_trait_modifier_ppm(country,generation_key)),
        0,5'000'000);
    const auto generated=phaseb::mul_checked(base,pct)/1'000'000;
    const auto committed=phaseb::add_checked(
        phaseb::add_checked(v.state_cost,v.institution_cost),
        phaseb::add_checked(v.nationalized_cost,v.commander_cost));
    v.capacity=p1power::flow(generated,committed,known);
    // Missing incorporation or ownership evidence is never interpreted as
    // "no administrative overburden"; callers must check complete.
    if(known) {
        v.construction_bonus_ppm=static_cast<std::int32_t>(
            phaseb::mul_checked(v.capacity.excess_ppm,100'000)/1'000'000);
        v.tax_waste_ppm=v.capacity.shortage_ppm;
        v.dividend_waste_ppm=v.capacity.shortage_ppm;
    }
    return v;
}
} // namespace vic25::phasec
