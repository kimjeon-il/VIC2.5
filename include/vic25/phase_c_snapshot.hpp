#pragma once
#include "vic25/phase_c.hpp"
#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace vic25::phasec {
namespace persistence_detail {
template<class T>struct is_map:std::false_type{};
template<class K,class V,class C,class A>struct is_map<std::map<K,V,C,A>>:std::true_type{};
template<class T>struct is_set:std::false_type{};
template<class K,class C,class A>struct is_set<std::set<K,C,A>>:std::true_type{};
template<class T>struct is_vector:std::false_type{};
template<class V,class A>struct is_vector<std::vector<V,A>>:std::true_type{};
template<class T>struct is_pair:std::false_type{};
template<class A,class B>struct is_pair<std::pair<A,B>>:std::true_type{};
template<class T>struct is_tuple:std::false_type{};
template<class... A>struct is_tuple<std::tuple<A...>>:std::true_type{};
template<class T>struct is_array:std::false_type{};
template<class V,std::size_t N>struct is_array<std::array<V,N>>:std::true_type{};

template<class Archive,class T>
void fields(Archive& a,T& x) {
    using X=std::remove_cvref_t<T>;
#define V(f) a(x.f)
    if constexpr(std::is_same_v<X,phaseb::Health>){
        V(ill);V(temporary_injury);V(long_term_impairment);
        V(limited_capabilities);V(illness_severity);V(injury_severity);
    } else if constexpr(std::is_same_v<X,phaseb::Education>){
        V(school);V(literacy);V(occupational_skill);
    } else if constexpr(std::is_same_v<X,phaseb::Demographic>){
        V(region);V(birth_month);V(sex);V(culture);V(religion);
        V(citizenship);V(legal_status);V(health);V(service);
    } else if constexpr(std::is_same_v<X,phaseb::Economic>){
        V(region);V(profession);V(livelihood);V(labor_class);V(education);
    } else if constexpr(std::is_same_v<X,phaseb::Household>){
        V(region);V(composition);V(pooling_policy);V(household_count_estimate);
    } else if constexpr(std::is_same_v<X,phaseb::SliceKey>){
        V(demographic);V(economic);V(household);V(partition);
    } else if constexpr(std::is_same_v<X,phaseb::Slice>){
        V(id);V(key);V(count);V(revision);
    } else if constexpr(std::is_same_v<X,phaseb::JobRequirements>){
        V(minimum_school);V(minimum_skill);V(required_capabilities);V(accommodation_mask);
        V(minimum_age_months);V(maximum_age_months);V(allowed_sexes_mask);
    } else if constexpr(std::is_same_v<X,phaseb::Employment>){
        V(employer);V(occupation);V(count);V(wage_per_month);V(requirements);
    } else if constexpr(std::is_same_v<X,phaseb::Event>){
        V(id);V(month);V(kind);V(from);V(to);V(count);
        V(employed_affected);V(provenance);
    } else if constexpr(std::is_same_v<X,phaseb::Vacancy>){
        V(employer);V(profession);V(slots);V(wage_per_month);V(requirements);
    } else if constexpr(std::is_same_v<X,phaseb::LaborRule>){
        V(minimum_age_months);V(maximum_age_months);V(allowed_sexes_mask);
    } else if constexpr(std::is_same_v<X,phaseb::DemographyRules>){
        V(fertility_ppm);V(infant_death_ppm);V(child_death_ppm);V(adult_death_ppm);
        V(senior_death_ppm);V(very_old_death_ppm);V(illness_incidence_ppm);
        V(illness_recovery_ppm);V(accident_ppm);V(injury_recovery_ppm);
        V(primary_access_ppm);V(education_access_multiplier_ppm);
        V(secondary_access_ppm);V(tertiary_access_ppm);V(adult_literacy_access_ppm);
        V(training_ppm);V(accident_permanent_share_ppm);V(migration_ppm);
        V(destination_by_region);V(monthly_vacancies);V(labor);
    } else if constexpr(std::is_same_v<X,vic25::Account>){
        V(id);V(owner_id);V(type);V(debit_minus_credit);
    } else if constexpr(std::is_same_v<X,vic25::InventoryKey>){
        V(holder_id);V(region_id);V(good_id);
    } else if constexpr(std::is_same_v<X,GovernmentState>){
        V(country);V(head_of_state);V(prime_minister);V(chamber_seats);V(quorum);V(revision);
    } else if constexpr(std::is_same_v<X,P1Constitution>){
        V(governance);V(power);V(legislature);V(legal_basis);
    } else if constexpr(std::is_same_v<X,P1Coalition>){
        V(ruling_groups);V(ruling_parties);V(caretaker);
    } else if constexpr(std::is_same_v<X,P1GovernmentChange>){
        V(country);V(new_head);V(new_prime);V(coalition);
        V(legal_basis);V(replace_head);V(replace_prime);
    } else if constexpr(std::is_same_v<X,P1Institution>){
        V(country);V(public_institution_id);V(kind);V(level);
    } else if constexpr(std::is_same_v<X,P1ValueAddedEvent>){
        V(source);V(country);V(region);V(month);V(day);
        V(gross_output);V(intermediate_input);V(fully_priced);
    } else if constexpr(std::is_same_v<X,P1OfficeEvidence>){
        V(institution);V(site);V(method);V(levels);V(workers_per_level);
    } else if constexpr(std::is_same_v<X,P1CommanderEvidence>){
        V(id);V(unit);V(anchor);V(cost);
    } else if constexpr(std::is_same_v<X,P1Mandate>){
        V(id);V(country);V(target);V(legal_basis);V(kind);
        V(adopted_day);V(effective_day);
    } else if constexpr(std::is_same_v<X,P1MandateChange>){
        V(id);V(mandate);V(terminate);
    } else if constexpr(std::is_same_v<X,P1StandingEvidence>){
        V(country);V(source);V(tier);V(recognized);V(confirmed_rank);
        V(observed_head);V(head_popularity);V(head_popularity_observed);
        V(own_gdp);V(subject_gdp);V(army_projection);V(navy_projection);
        V(goods_leadership);V(monuments);V(other_prestige);
        V(observed_day);V(complete);
    } else if constexpr(std::is_same_v<X,P3Stance>){
        V(preference_ppm);V(importance_ppm);
    } else if constexpr(std::is_same_v<X,P3Attitude>){
        V(slice);V(radical_ppm);V(loyal_ppm);V(engagement_ppm);
        V(observed_day);V(issues);
    } else if constexpr(std::is_same_v<X,P2CultureTraits>) {
        V(culture);V(heritages);V(heritage_groups);
        V(languages);V(language_groups);V(traditions);V(homelands);
    } else if constexpr(std::is_same_v<X,P2ReligionTraits>) {
        V(religion);V(traits);V(trait_groups);
    } else if constexpr(std::is_same_v<X,P2NationalIdentity>) {
        V(country);V(primary_cultures);V(state_religions);
    } else if constexpr(std::is_same_v<X,P3Group>){
        V(country);V(id);V(leader_anchor);V(issue_domain);
        V(preferred_choice);V(type);V(organization_ppm);
        V(institutional_access_ppm);
    } else if constexpr(std::is_same_v<X,P3Movement>){
        V(id);V(country);V(issue_domain);V(desired_choice);
        V(region);V(goal);V(groups);V(parties);
        V(activism_ppm);V(radical_ppm);V(active);
    } else if constexpr(std::is_same_v<X,P5CollectiveAction>){
        V(id);V(movement);V(group);V(region);V(institution_id);
        V(kind);V(slice);V(participants);V(strikers);V(day);
    } else if constexpr(std::is_same_v<X,P5Crisis>){
        V(id);V(country);V(movement);V(kind);V(stage);
        V(pressure_days);V(calm_days);V(last_checked_day);
        V(armed_day);V(settled_day);V(pledged_units);
        V(contested_regions);V(verified_engagements);
    } else if constexpr(std::is_same_v<X,P5ControlReconciliation>){
        V(crisis_id);V(country);V(successor_head);V(verified_regions);
    } else if constexpr(std::is_same_v<X,P5Settlement>){
        V(id);V(crisis_id);V(legal_basis);V(successor_head);
        V(new_sovereign);V(region);V(kind);V(day);
    } else if constexpr(std::is_same_v<X,P5GuerrillaCampaign>){
        V(id);V(country);V(movement);V(crisis_id);V(stage);
        V(started_day);V(next_review_day);V(ended_day);
        V(regions);V(units);V(verified_skirmishes);
    } else if constexpr(std::is_same_v<X,P5GuerrillaIncident>){
        V(id);V(campaign);V(region);V(irregular_unit);V(government_unit);
        V(kind);V(day);V(ammunition_spent);V(access_disruption_ppm);
    } else if constexpr(std::is_same_v<X,P5GuerrillaAccessEffect>){
        V(incident_id);V(campaign);V(region);V(from_day);
        V(through_day);V(penalty_ppm);
    } else if constexpr(std::is_same_v<X,P5GuerrillaOutcome>){
        V(campaign);V(legal_basis);V(kind);V(day);
    } else if constexpr(std::is_same_v<X,P4Party>){
        V(id);V(country);V(head_candidate);V(organization_ppm);
        V(registered);V(active);
    } else if constexpr(std::is_same_v<X,P4PersonTerm>){
        V(contest_id);V(person_id);V(party_id);V(total_terms);
        V(consecutive_terms);V(last_elected_day);V(incumbent);
    } else if constexpr(std::is_same_v<X,P4Contest>){
        V(id);V(country);V(jurisdiction);V(law_id);
        V(office);V(method);V(term_mode);V(term_months);
        V(turnout_ppm);V(seats);V(next_poll_day);V(regular_poll_day);
        V(last_poll_day);V(dissolution_authorized);V(active);V(dissolved);
    } else if constexpr(std::is_same_v<X,P4Result>){
        V(contest_id);V(law_id);V(day);V(eligible);V(registered);
        V(cast);V(invalid);V(valid);V(votes);V(seats);
        V(winning_party);V(office_holder);V(early);
    } else if constexpr(std::is_same_v<X,P4TermAmendment>){
        V(contest_id);V(law_id);V(effective_day);V(next_poll_day);
        V(term_months);V(applies_to_incumbent);V(term_mode);
    } else if constexpr(std::is_same_v<X,P2Clause>){
        V(id);V(target);V(kind);V(value);
    } else if constexpr(std::is_same_v<X,P2BaseLaw>){
        V(id);V(country);V(domain);V(scope);V(base_choice);V(legal_basis);
        V(tier);V(clauses);
    } else if constexpr(std::is_same_v<X,P2Amendment>){
        V(id);V(law_id);V(legal_basis);V(exclusivity_group);V(clauses);
    } else if constexpr(std::is_same_v<X,P2ElectionTransition>){
        V(contest_id);V(mode);V(next_poll_day);
    } else if constexpr(std::is_same_v<X,P2Bill>){
        V(id);V(country);V(proposer);V(legal_basis);V(action);V(status);
        V(proposed_base);V(proposed_amendment);V(target_id);V(transitions);
        V(introduced_day);V(promulgated_day);V(effective_day);
    } else if constexpr(std::is_same_v<X,P2LawEvent>){
        V(bill_id);V(country);V(law_id);V(amendment_id);V(legal_basis);
        V(action);V(effective_day);
    } else if constexpr(std::is_same_v<X,P2GroupDemand>){
        V(id);V(country);V(interest_group);V(law_domain);
        V(amendment);V(min_population_support_ppm);
    } else if constexpr(std::is_same_v<X,P2NegotiatedAmendment>){
        V(bill_id);V(demand_id);V(group);V(amendment);
        V(support_population);V(accepted_day);
    } else if constexpr(std::is_same_v<X,P2ChamberDecision>){
        V(bill_id);V(upper_contest_id);V(lower_poll_day);
        V(upper_poll_day);V(lower_passed);V(upper_passed);
    } else if constexpr(std::is_same_v<X,CabinetChange>){
        V(country);V(new_head);V(new_prime_minister);V(replace_head);V(replace_prime);
    } else if constexpr(std::is_same_v<X,TypedEntityId>){
        V(kind);V(id);
    } else if constexpr(std::is_same_v<X,MarketQuote>){
        V(price);V(floor);V(ceiling);V(target_stock);
        V(solvent_demand);V(delivered);V(unmet);V(stock_offered);
    } else if constexpr(std::is_same_v<X,PublicInstitution>){
        V(id);V(operator_id);V(region);V(good);
        V(monthly_slots);V(patients_per_worker);V(provided);
    } else if constexpr(std::is_same_v<X,Reparation>){
        V(id);V(payer);V(recipient);V(total);V(remaining);V(effective_month);V(recognized);
    } else if constexpr(std::is_same_v<X,ProductionFacility>){
        V(id);V(owner);V(region);V(capacity);V(damaged);
    } else if constexpr(std::is_same_v<X,ElectionResult>){
        V(eligible);V(registered);V(cast);V(invalid);V(valid);
        V(votes);V(seats);V(total_seats);
    } else if constexpr(std::is_same_v<X,Shipment>){
        V(id);V(from);V(to);V(origin);V(destination);V(good);
        V(quantity);V(loss);V(arrival_month);V(delivered);
    } else if constexpr(std::is_same_v<X,DividendDue>){
        V(asset);V(payer);V(holder);V(amount);V(due);
    } else if constexpr(std::is_same_v<X,WageArrearsClaim>) {
        V(id);V(employer);V(worker);V(treasury);V(country);
        V(accrued);V(outstanding);V(issued_day);
    } else if constexpr(std::is_same_v<X,SovereignBond>) {
        V(id);V(country);V(treasury);V(original_face);V(coupon_ppm);
        V(issue_day);V(maturity_day);
    } else if constexpr(std::is_same_v<X,ArmyDeployment>) {
        V(unit);V(country);V(region);
    } else {
        static_assert(sizeof(X)==0,"SnapshotCodec needs an explicit struct layout");
    }
#undef V
}

struct BinaryWriter {
    std::ostream& os;
    template<class T>void operator()(const T& x){
        if constexpr(std::is_same_v<T,std::string>){
            (*this)(static_cast<std::uint64_t>(x.size()));
            os.write(x.data(),static_cast<std::streamsize>(x.size()));
            if(!os)throw std::runtime_error("snapshot string write failed");
        }else if constexpr(std::is_same_v<T,bool>){
            const char b=x?'1':'0';os.write(&b,1);
        }else if constexpr(std::is_enum_v<T>){
            using U=std::underlying_type_t<T>;
            (*this)(static_cast<U>(x));
        }else if constexpr(std::is_integral_v<T>){
            using U=std::make_unsigned_t<T>;
            U bits=std::bit_cast<U>(x);
            for(std::size_t i=0;i<sizeof(T);++i){
                const char b=static_cast<char>((bits>>(i*8))&0xff);
                os.write(&b,1);
            }
        }else if constexpr(is_map<T>::value){
            (*this)(static_cast<std::uint64_t>(x.size()));
            for(const auto& [key,value]:x){(*this)(key);(*this)(value);}
        }else if constexpr(is_set<T>::value){
            (*this)(static_cast<std::uint64_t>(x.size()));
            for(const auto& v:x)(*this)(v);
        }else if constexpr(is_vector<T>::value){
            (*this)(static_cast<std::uint64_t>(x.size()));
            for(const auto& v:x)(*this)(v);
        }else if constexpr(is_pair<T>::value){
            (*this)(x.first);(*this)(x.second);
        }else if constexpr(is_tuple<T>::value){
            std::apply([&](const auto&... v){((*this)(v),...);},x);
        }else if constexpr(is_array<T>::value){
            for(const auto& v:x)(*this)(v);
        }else{fields(*this,x);}
    }
};

struct BinaryReader {
    std::istream& is;
    template<class T>void operator()(T& x){
        if constexpr(std::is_same_v<T,std::string>){
            std::uint64_t count{};(*this)(count);
            if(count>16'777'216)throw std::runtime_error("unbounded snapshot string");
            x.resize(static_cast<std::size_t>(count));
            is.read(x.data(),static_cast<std::streamsize>(count));
            if(!is)throw std::runtime_error("truncated snapshot string");
        }else if constexpr(std::is_same_v<T,bool>){
            char b{};is.read(&b,1);
            if(!is || (b!='0'&&b!='1'))throw std::runtime_error("corrupt bool");
            x=(b=='1');
        }else if constexpr(std::is_enum_v<T>){
            std::underlying_type_t<T> numeric{};(*this)(numeric);x=static_cast<T>(numeric);
        }else if constexpr(std::is_integral_v<T>){
            using U=std::make_unsigned_t<T>;
            U bits{};
            for(std::size_t i=0;i<sizeof(T);++i){
                char b{};is.read(&b,1);
                if(!is)throw std::runtime_error("truncated numeric value");
                bits|=static_cast<U>(static_cast<unsigned char>(b))<<(i*8);
            }
            x=std::bit_cast<T>(bits);
        }else if constexpr(is_map<T>::value){
            std::uint64_t n{};(*this)(n);if(n>10'000'000)throw std::runtime_error("oversized map");
            x.clear();
            for(std::uint64_t i=0;i<n;++i){
                typename T::key_type key{};
                typename T::mapped_type value{};
                (*this)(key);(*this)(value);
                if(!x.emplace(std::move(key),std::move(value)).second)
                    throw std::runtime_error("duplicate serialized map key");
            }
        }else if constexpr(is_set<T>::value){
            std::uint64_t n{};(*this)(n);if(n>10'000'000)throw std::runtime_error("oversized set");
            x.clear();
            for(std::uint64_t i=0;i<n;++i){
                typename T::value_type value{};(*this)(value);
                if(!x.insert(std::move(value)).second)
                    throw std::runtime_error("duplicate serialized set member");
            }
        }else if constexpr(is_vector<T>::value){
            std::uint64_t n{};(*this)(n);if(n>10'000'000)throw std::runtime_error("oversized vector");
            x.clear();x.reserve(static_cast<std::size_t>(n));
            for(std::uint64_t i=0;i<n;++i){
                typename T::value_type value{};(*this)(value);x.push_back(std::move(value));
            }
        }else if constexpr(is_pair<T>::value){
            (*this)(x.first);(*this)(x.second);
        }else if constexpr(is_tuple<T>::value){
            std::apply([&](auto&... v){((*this)(v),...);},x);
        }else if constexpr(is_array<T>::value){
            for(auto& v:x)(*this)(v);
        }else{fields(*this,x);}
    }
};
inline std::uint64_t checksum(const std::string& bytes){
    std::uint64_t hash=1469598103934665603ULL;
    for(unsigned char c:bytes){hash^=c;hash*=1099511628211ULL;}
    return hash;
}
} // namespace persistence_detail

// All persistent contents are accessed solely by the snapshot codec friend.
// Save is permitted only for a committed, fully validated world.
struct SnapshotCodec {
    template<class A,class WorldT>
    static void pop(A& a,WorldT& w){
#define F(v) a(w.v)
        F(next_id_);F(next_partition_);F(default_health_);F(default_education_);F(month_);
        F(health_ids_);F(health_);F(education_ids_);F(educations_);
        F(demographic_ids_);F(demographics_);F(economic_ids_);F(economics_);
        F(household_ids_);F(households_);F(slice_ids_);F(slices_);F(jobs_);
        F(events_);F(external_events_);F(residual_);
#undef F
    }
    template<class A,class Ledger>
    static void financial(A& a,Ledger& l){
        a(l.accounts_);a(l.transaction_ids_);
    }
    template<class A,class Ledger>
    static void inventory(A& a,Ledger& l){
        a(l.stock_);a(l.opening_);a(l.produced_);a(l.consumed_);
    }
    template<class A,class Ledger>
    static void ownership(A& a,Ledger& l){
        a(l.issued_);a(l.shares_);
    }
    template<class A,class WorldT>
    static void all(A& a,WorldT& w){
#define F(v) a(w.v)
        pop(a,w.people_);
        financial(a,w.money_);inventory(a,w.goods_);ownership(a,w.ownership_);
        F(actors_);F(processed_commands_);F(units_);
        F(next_typed_id_);F(typed_entities_);F(typed_active_ids_);
        F(retired_typed_ids_);F(typed_successor_);F(regions_with_initial_stock_);
        F(formations_);F(unit_to_formation_);F(equipment_assignment_);
        F(tracked_assets_);F(security_accounts_);F(dividends_);
        F(healthcare_right_);F(scheduled_public_healthcare_);
        F(public_institutions_);F(paid_staff_this_month_);
        F(total_bad_debt_);F(bank_insolvent_);F(regional_food_grievance_);
        F(foreign_holders_);
        if constexpr(std::is_const_v<std::remove_reference_t<WorldT>>){
            a(static_cast<std::uint64_t>(w.foreign_ledgers_.size()));
            for(const auto& [currency,ledger]:w.foreign_ledgers_){
                a(currency);financial(a,ledger);
            }
        }else{
            std::uint64_t size{};a(size);w.foreign_ledgers_.clear();
            if(size>10'000)throw std::runtime_error("too many currencies in snapshot");
            for(std::uint64_t i=0;i<size;++i){
                std::string currency;FinancialLedger ledger;
                a(currency);financial(a,ledger);
                if(!w.foreign_ledgers_.emplace(currency,std::move(ledger)).second)
                    throw std::runtime_error("duplicate serialized currency");
            }
        }
        F(reparations_);F(facilities_);F(scheduled_site_repairs_);
        F(markets_);F(household_output_);F(household_consumed_);
        F(person_anchors_);F(anchor_counts_);F(ballot_ids_);F(ballot_votes_);
        F(interest_groups_);F(membership_);F(participation_this_month_);
        F(strike_active_);F(scheduled_strikes_);
        F(governments_);F(scheduled_cabinets_);F(shipments_);
        F(tracked_goods_);F(sovereignty_);F(military_control_);
        F(scheduled_conscription_);F(scheduled_income_tax_);F(income_tax_ppm_);
        F(conscription_enabled_);F(scheduled_voting_age_);F(last_election_);
        F(demographic_rules_);F(events_);F(commit_id_);F(sequence_);
        F(stage_);F(voting_age_years_);F(election_this_month_);F(demography_enabled_);
#undef F
    }
    static std::string encode(const IntegratedWorld& w){
        if(w.stage_!=-1)throw std::runtime_error("cannot save in-progress month");
        w.validate();
        std::ostringstream os(std::ios::binary|std::ios::out);
        persistence_detail::BinaryWriter ar{os};
        all(ar,w);
        // Optional self-delimiting tail. Authentic old raw Phase C payloads
        // are byte-for-byte unchanged unless country election records exist.
        // D/E/GH/daily serializers embed the length-prefixed C payload.
        if(!w.country_elections_.empty()) {
            ar(std::string("VIC25_PHASEC_COUNTRY_ELECTIONS_1"));
            ar(w.country_elections_);
            ar(w.country_election_months_);
        }
        if(!w.country_income_tax_.empty()||
           !w.scheduled_country_income_tax_.empty()||
           !w.country_treasuries_.empty()) {
            ar(std::string("VIC25_PHASEC_COUNTRY_FISCAL_1"));
            ar(w.country_income_tax_);
            ar(w.scheduled_country_income_tax_);
            ar(w.country_treasuries_);
        }
        if(w.bank_interest_annual_ppm_!=0||
           !w.bank_interest_fraction_.empty()) {
            ar(std::string("VIC25_PHASEC_BANK_INTEREST_1"));
            ar(w.bank_interest_annual_ppm_);
            ar(w.bank_interest_fraction_);
        }
        if(!w.people_.exact_birth_days_.empty()) {
            ar(std::string("VIC25_PHASEC_EXACT_BIRTH_DAYS_1"));
            ar(w.people_.exact_birth_days_);
        }
        if(!w.wage_arrears_.empty()||!w.bankrupt_firms_.empty()) {
            ar(std::string("VIC25_PHASEC_WAGE_INSOLVENCY_1"));
            ar(w.wage_arrears_);
            ar(w.bankrupt_firms_);
        }
        if(!w.bank_principal_by_borrower_.empty()||
           !w.sovereign_bonds_.empty()||
           !w.sovereign_coupon_due_.empty()||
           !w.sovereign_coupon_fraction_.empty()) {
            ar(std::string("VIC25_PHASEC_SOVEREIGN_BONDS_1"));
            ar(w.bank_principal_by_borrower_);
            ar(w.sovereign_bonds_);
            ar(w.sovereign_coupon_due_);
            ar(w.sovereign_coupon_fraction_);
        }
        if(!w.army_deployments_.empty()) {
            ar(std::string("VIC25_PHASEC_ARMY_DEPLOYMENTS_1"));
            ar(w.army_deployments_);
        }
        if(!w.completed_liquidations_.empty()) {
            ar(std::string("VIC25_PHASEC_ESTATE_CLOSURES_1"));
            ar(w.completed_liquidations_);
        }
        if(!w.p1_constitutions_.empty()) {
            ar(std::string("VIC25_PHASEC_P1_GOVERNANCE_1"));
            ar(w.p1_constitutions_);
            ar(w.p1_coalitions_);
            ar(w.p1_institutions_);
            ar(w.p1_pending_);
        }
        if(!w.p1_active_mandates_.empty()||
           !w.p1_pending_mandates_.empty()||!w.p1_standing_.empty()) {
            ar(std::string("VIC25_PHASEC_P1_STRATEGIC_CAPACITIES_1"));
            ar(w.p1_active_mandates_);
            ar(w.p1_pending_mandates_);
            ar(w.p1_standing_);
        }
        if(!w.p1_value_added_.empty()) {
            ar(std::string("VIC25_PHASEC_VALUE_ADDED_1"));
            ar(w.p1_value_added_);
        }
        if(!w.p1_incorporation_.empty()||!w.p1_offices_.empty()||
           !w.p1_public_building_levels_.empty()||!w.p1_commanders_.empty()) {
            ar(std::string("VIC25_PHASEC_P1_BUREAUCRACY_1"));
            ar(w.p1_incorporation_);
            ar(w.p1_offices_);
            ar(w.p1_public_building_levels_);
            ar(w.p1_commanders_);
        }
        if(!w.p4_parties_.empty()||!w.p4_contests_.empty()) {
            ar(std::string("VIC25_PHASEC_P4_ELECTIONS_1"));
            ar(w.p4_parties_);
            ar(w.p4_affinities_);
            ar(w.p4_contests_);
            ar(w.p4_nonparliament_results_);
            ar(w.p4_history_);
            ar(w.p4_amendments_);
        }
        if(!w.p2_laws_.empty()||!w.p2_bills_.empty()) {
            ar(std::string("VIC25_PHASEC_P2_LAWS_1"));
            ar(w.p2_laws_);
            ar(w.p2_active_base_);
            ar(w.p2_amendments_);
            ar(w.p2_active_amendments_);
            ar(w.p2_bills_);
            ar(w.p2_history_);
        }
        // Keep old P4/P2 schema signatures byte-identical. Extra contracts
        // are optional trailers, never new writable population/seat ledgers.
        if(!w.p4_nominations_.empty()||!w.p4_person_terms_.empty()) {
            ar(std::string("VIC25_PHASEC_P4_PERSONAL_MANDATES_1"));
            ar(w.p4_nominations_);
            ar(w.p4_person_terms_);
        }
        if(!w.p2_upper_chambers_.empty()||
           !w.p2_chamber_decisions_.empty()||
           !w.p2_group_demands_.empty()||!w.p2_negotiated_.empty()) {
            ar(std::string("VIC25_PHASEC_P2_POLITICAL_PROCEDURES_1"));
            ar(w.p2_upper_chambers_);
            ar(w.p2_chamber_decisions_);
            ar(w.p2_group_demands_);
            ar(w.p2_negotiated_);
        }
        if(!w.p3_groups_.empty()||!w.p3_attitudes_.empty()||
           !w.p3_movements_.empty()) {
            ar(std::string("VIC25_PHASEC_P3_POLITICS_1"));
            ar(w.p3_attitudes_);
            ar(w.p3_groups_);
            ar(w.p3_movements_);
            ar(w.p3_last_reaction_day_);
        }
        if(!w.p3_ideology_overrides_.empty()||
           !w.p3_leader_ideologies_.empty()) {
            ar(std::string("VIC25_PHASEC_P3_IDEOLOGIES_1"));
            ar(w.p3_ideology_overrides_);
            ar(w.p3_leader_ideologies_);
        }
        if(!w.p2_cultures_.empty()||!w.p2_religions_.empty()||
           !w.p2_national_identities_.empty()||
           !w.p2_emancipation_due_.empty()) {
            ar(std::string("VIC25_PHASEC_P2_NATIONAL_IDENTITIES_1"));
            ar(w.p2_cultures_);
            ar(w.p2_religions_);
            ar(w.p2_national_identities_);
            ar(w.p2_emancipation_due_);
        }
        if(!w.p5_crises_.empty()||!w.p5_capitals_.empty()||
           !w.p5_actions_.empty()||
           !w.p5_settlements_.empty()||
           !w.p5_scheduled_control_reconciliation_.empty()||
           !w.p5_scheduled_daily_strikes_.empty()||
           !w.p5_active_daily_strikes_.empty()) {
            ar(std::string("VIC25_PHASEC_P5_UNREST_1"));
            ar(w.p5_crises_);
            ar(w.p5_capitals_);
            ar(w.p5_actions_);
            ar(w.p5_settlements_);
            ar(w.p5_control_evidence_);
            ar(w.p5_scheduled_control_reconciliation_);
            ar(w.p5_scheduled_daily_strikes_);
            ar(w.p5_active_daily_strikes_);
        }
        if(!w.p5_guerrilla_campaigns_.empty()||
           !w.p5_guerrilla_incidents_.empty()||
           !w.p5_guerrilla_outcomes_.empty()||
           !w.p5_guerrilla_effect_due_.empty()||
           !w.p5_guerrilla_effect_active_.empty()) {
            ar(std::string("VIC25_PHASEC_P5_GUERRILLA_1"));
            ar(w.p5_guerrilla_campaigns_);
            ar(w.p5_guerrilla_incidents_);
            ar(w.p5_guerrilla_outcomes_);
            ar(w.p5_guerrilla_effect_due_);
            ar(w.p5_guerrilla_effect_active_);
            // The active regional penalty, the journal's incident identity
            // set and 7-day due index are all reconstructed on load.
        }
        return os.str();
    }
    static IntegratedWorld decode(const std::string& bytes){
        std::istringstream is(bytes,std::ios::binary|std::ios::in);
        persistence_detail::BinaryReader ar{is};
        IntegratedWorld w;
        all(ar,w);
        bool seen_elections=false,seen_fiscal=false,seen_interest=false,
             seen_birth_days=false,seen_wages=false,seen_bonds=false,
             seen_armies=false,seen_estates=false,seen_p1=false,
             seen_p1_capacities=false,seen_p1_bureaucracy=false,
             seen_value_added=false,
             seen_p4=false,seen_p2=false,seen_people=false,seen_procedures=false,
             seen_p3=false,seen_ideologies=false,seen_identity=false,
             seen_p5=false,
             seen_guerrilla=false;
        while(is.peek()!=std::char_traits<char>::eof()) {
            std::string extension;
            ar(extension);
            if(extension=="VIC25_PHASEC_COUNTRY_ELECTIONS_1"&&!seen_elections) {
                seen_elections=true;
                ar(w.country_elections_);
                ar(w.country_election_months_);
                if(w.country_elections_.empty() ||
                   w.country_elections_.size()!=w.country_election_months_.size() ||
                   w.country_elections_.size()>100'000)
                    throw std::runtime_error("invalid national elections extension");
            } else if(extension=="VIC25_PHASEC_COUNTRY_FISCAL_1"&&!seen_fiscal) {
                seen_fiscal=true;
                ar(w.country_income_tax_);
                ar(w.scheduled_country_income_tax_);
                ar(w.country_treasuries_);
                if(w.country_income_tax_.size()>100'000 ||
                   w.scheduled_country_income_tax_.size()>1'000'000 ||
                   w.country_treasuries_.size()>100'000)
                    throw std::runtime_error("oversized country fiscal extension");
            } else if(extension=="VIC25_PHASEC_BANK_INTEREST_1"&&!seen_interest) {
                seen_interest=true;
                ar(w.bank_interest_annual_ppm_);
                ar(w.bank_interest_fraction_);
                if(w.bank_interest_annual_ppm_<0||
                   w.bank_interest_annual_ppm_>1'000'000||
                   w.bank_interest_fraction_.size()>1'000'000)
                    throw std::runtime_error("invalid bank interest extension");
            } else if(extension=="VIC25_PHASEC_EXACT_BIRTH_DAYS_1" &&
                      !seen_birth_days) {
                seen_birth_days=true;
                ar(w.people_.exact_birth_days_);
                if(w.people_.exact_birth_days_.size()>5'000'000)
                    throw std::runtime_error("oversized POP exact birth extension");
            } else if(extension=="VIC25_PHASEC_WAGE_INSOLVENCY_1"&&!seen_wages) {
                seen_wages=true;
                ar(w.wage_arrears_);
                ar(w.bankrupt_firms_);
                if(w.wage_arrears_.size()>5'000'000||
                   w.bankrupt_firms_.size()>1'000'000)
                    throw std::runtime_error("oversized real wage debt state");
            } else if(extension=="VIC25_PHASEC_SOVEREIGN_BONDS_1"&&
                      !seen_bonds) {
                seen_bonds=true;
                ar(w.bank_principal_by_borrower_);
                ar(w.sovereign_bonds_);
                ar(w.sovereign_coupon_due_);
                ar(w.sovereign_coupon_fraction_);
                if(w.sovereign_bonds_.size()>1'000'000 ||
                   w.bank_principal_by_borrower_.size()>1'000'000||
                   w.sovereign_coupon_due_.size()>5'000'000||
                   w.sovereign_coupon_fraction_.size()>5'000'000)
                    throw std::runtime_error("oversized sovereign bond state");
            } else if(extension=="VIC25_PHASEC_ARMY_DEPLOYMENTS_1"&&
                      !seen_armies) {
                seen_armies=true;
                ar(w.army_deployments_);
                if(w.army_deployments_.size()>1'000'000)
                    throw std::runtime_error("oversized army deployment records");
            } else if(extension=="VIC25_PHASEC_ESTATE_CLOSURES_1"&&
                      !seen_estates) {
                seen_estates=true;
                ar(w.completed_liquidations_);
                if(w.completed_liquidations_.size()>1'000'000)
                    throw std::runtime_error("oversized closed estate records");
            } else if(extension=="VIC25_PHASEC_P1_GOVERNANCE_1"&&!seen_p1) {
                seen_p1=true;
                ar(w.p1_constitutions_);
                ar(w.p1_coalitions_);
                ar(w.p1_institutions_);
                ar(w.p1_pending_);
                if(w.p1_constitutions_.empty()||
                   w.p1_constitutions_.size()>100'000||
                   w.p1_coalitions_.size()!=w.p1_constitutions_.size()||
                   w.p1_institutions_.size()>1'000'000||
                   w.p1_pending_.size()>1'000'000)
                    throw std::runtime_error("invalid P1 constitutional extension");
            } else if(extension=="VIC25_PHASEC_P1_STRATEGIC_CAPACITIES_1"&&
                      !seen_p1_capacities) {
                seen_p1_capacities=true;
                ar(w.p1_active_mandates_);
                ar(w.p1_pending_mandates_);
                ar(w.p1_standing_);
                if(w.p1_active_mandates_.size()>500'000||
                   w.p1_pending_mandates_.size()>500'000||
                   w.p1_standing_.size()>100'000)
                    throw std::runtime_error("oversized P1 capacity commitments");
            } else if(extension=="VIC25_PHASEC_VALUE_ADDED_1"&&
                      !seen_value_added) {
                seen_value_added=true;
                ar(w.p1_value_added_);
                if(w.p1_value_added_.size()>5'000'000)
                    throw std::runtime_error("oversized GDP observation history");
            } else if(extension=="VIC25_PHASEC_P1_BUREAUCRACY_1"&&
                      !seen_p1_bureaucracy) {
                seen_p1_bureaucracy=true;
                ar(w.p1_incorporation_);
                ar(w.p1_offices_);
                ar(w.p1_public_building_levels_);
                ar(w.p1_commanders_);
                if(w.p1_incorporation_.size()>1'000'000||
                   w.p1_offices_.size()>500'000||
                   w.p1_public_building_levels_.size()>500'000||
                   w.p1_commanders_.size()>500'000)
                    throw std::runtime_error("oversized P1 bureaucracy extension");
            } else if(extension=="VIC25_PHASEC_P4_ELECTIONS_1"&&!seen_p4) {
                seen_p4=true;
                ar(w.p4_parties_);
                ar(w.p4_affinities_);
                ar(w.p4_contests_);
                ar(w.p4_nonparliament_results_);
                ar(w.p4_history_);
                ar(w.p4_amendments_);
                if(w.p4_parties_.size()>100'000||
                   w.p4_affinities_.size()>2'000'000||
                   w.p4_contests_.size()>100'000||
                   w.p4_nonparliament_results_.size()>100'000||
                   w.p4_history_.size()>1'000'000||
                   w.p4_amendments_.size()>1'000'000)
                    throw std::runtime_error("oversized P4 election extension");
                w.p4_rebuild_due_index();
            } else if(extension=="VIC25_PHASEC_P2_LAWS_1"&&!seen_p2) {
                seen_p2=true;
                ar(w.p2_laws_);
                ar(w.p2_active_base_);
                ar(w.p2_amendments_);
                ar(w.p2_active_amendments_);
                ar(w.p2_bills_);
                ar(w.p2_history_);
                if(w.p2_laws_.size()>100'000||
                   w.p2_active_base_.size()>100'000||
                   w.p2_amendments_.size()>1'000'000||
                   w.p2_active_amendments_.size()>1'000'000||
                   w.p2_bills_.size()>1'000'000||
                   w.p2_history_.size()>1'000'000)
                    throw std::runtime_error("oversized P2 legislation extension");
                w.p2_rebuild_due_index();
            } else if(extension=="VIC25_PHASEC_P4_PERSONAL_MANDATES_1"&&
                      !seen_people) {
                seen_people=true;
                ar(w.p4_nominations_);
                ar(w.p4_person_terms_);
                if(w.p4_nominations_.size()>1'000'000||
                   w.p4_person_terms_.size()>5'000'000)
                    throw std::runtime_error("oversized P4 named mandate records");
            } else if(extension=="VIC25_PHASEC_P2_POLITICAL_PROCEDURES_1"&&
                      !seen_procedures) {
                seen_procedures=true;
                ar(w.p2_upper_chambers_);
                ar(w.p2_chamber_decisions_);
                ar(w.p2_group_demands_);
                ar(w.p2_negotiated_);
                if(w.p2_upper_chambers_.size()>100'000||
                   w.p2_chamber_decisions_.size()>1'000'000||
                   w.p2_group_demands_.size()>1'000'000||
                   w.p2_negotiated_.size()>1'000'000)
                    throw std::runtime_error("oversized P2 parliamentary record");
            } else if(extension=="VIC25_PHASEC_P3_POLITICS_1"&&!seen_p3) {
                seen_p3=true;
                ar(w.p3_attitudes_);
                ar(w.p3_groups_);
                ar(w.p3_movements_);
                ar(w.p3_last_reaction_day_);
                if(w.p3_attitudes_.size()>5'000'000||
                   w.p3_groups_.size()>500'000||
                   w.p3_movements_.size()>100'000)
                    throw std::runtime_error("oversized POP political state");
            } else if(extension=="VIC25_PHASEC_P3_IDEOLOGIES_1"&&
                      !seen_ideologies) {
                seen_ideologies=true;
                ar(w.p3_ideology_overrides_);
                ar(w.p3_leader_ideologies_);
                if(w.p3_ideology_overrides_.size()>500'000||
                   w.p3_leader_ideologies_.size()>500'000)
                    throw std::runtime_error("oversized P3 ideological profile");
            } else if(extension=="VIC25_PHASEC_P2_NATIONAL_IDENTITIES_1"&&
                      !seen_identity) {
                seen_identity=true;
                ar(w.p2_cultures_);
                ar(w.p2_religions_);
                ar(w.p2_national_identities_);
                ar(w.p2_emancipation_due_);
                if(w.p2_cultures_.size()>50'000||
                   w.p2_religions_.size()>50'000||
                   w.p2_national_identities_.size()>5'000||
                   w.p2_emancipation_due_.size()>5'000)
                    throw std::runtime_error(
                        "oversized P2 culture/religion evidence");
            } else if(extension=="VIC25_PHASEC_P5_UNREST_1"&&!seen_p5) {
                seen_p5=true;
                ar(w.p5_crises_);
                ar(w.p5_capitals_);
                ar(w.p5_actions_);
                ar(w.p5_settlements_);
                ar(w.p5_control_evidence_);
                ar(w.p5_scheduled_control_reconciliation_);
                ar(w.p5_scheduled_daily_strikes_);
                ar(w.p5_active_daily_strikes_);
                w.p5_action_ids_.clear();
                for(const auto& event:w.p5_actions_)
                    if(!w.p5_action_ids_.insert(event.id).second)
                        throw std::runtime_error(
                            "duplicate stored P5 collective action identity");
                if(w.p5_crises_.size()>100'000||
                   w.p5_capitals_.size()>100'000||
                   w.p5_actions_.size()>1'000'000||
                   w.p5_settlements_.size()>1'000'000||
                   w.p5_control_evidence_.size()>100'000||
                   w.p5_scheduled_control_reconciliation_.size()>1'000'000||
                   w.p5_scheduled_daily_strikes_.size()>1'000'000||
                   w.p5_active_daily_strikes_.size()>5'000'000)
                    throw std::runtime_error("oversized P5 real civic/crisis state");
            } else if(extension=="VIC25_PHASEC_P5_GUERRILLA_1"&&
                      !seen_guerrilla) {
                seen_guerrilla=true;
                ar(w.p5_guerrilla_campaigns_);
                ar(w.p5_guerrilla_incidents_);
                ar(w.p5_guerrilla_outcomes_);
                ar(w.p5_guerrilla_effect_due_);
                ar(w.p5_guerrilla_effect_active_);
                if(w.p5_guerrilla_campaigns_.size()>100'000||
                   w.p5_guerrilla_incidents_.size()>1'000'000||
                   w.p5_guerrilla_outcomes_.size()>1'000'000||
                   w.p5_guerrilla_effect_due_.size()>1'000'000||
                   w.p5_guerrilla_effect_active_.size()>100'000)
                    throw std::runtime_error(
                        "oversized P5-G regional guerrilla extension");
                w.p5_guerrilla_rebuild_indexes();
            } else {
                throw std::runtime_error("unknown or duplicate Phase C extension");
            }
        }
        if(w.stage_!=-1)throw std::runtime_error("staged month cannot be loaded");
        // Negotiation's derived due index depends on the optional chamber
        // decisions trailer, which may appear AFTER the P2 base-law trailer.
        if(seen_p2)w.p2_rebuild_due_index();
        w.validate();
        return w;
    }
    // Version lock identifies the exact POP/common/economy/politics/military
    // contract families and the limited numeric/ruleset semantics actually
    // implemented by this integration slice. It does not imply full
    // historical calibration or feature-complete engine certification.
    [[nodiscard]] static constexpr std::array<const char*,9> contract_header() {
        return {{"pop-design/1.1", "phase-c-common/1.0",
                 "phase-c-economy/1.0", "phase-c-politics/1.0",
                 "phase-c-military/1.0", "phase-c-integration/1.0",
                 "checked-int64/1", "integer-lot/1",
                 "vertical-slice-ruleset/1"}};
    }
    static void save(const IntegratedWorld& w,const std::filesystem::path& path){
        const auto payload=encode(w);
        std::ostringstream header(std::ios::binary|std::ios::out);
        persistence_detail::BinaryWriter ar{header};
        const std::string magic="VIC25_SNAPSHOT_PHASEC_2";
        ar(magic);
        for (const char* version : contract_header()) ar(std::string(version));
        ar(static_cast<std::uint64_t>(payload.size()));
        std::string bytes=header.str()+payload;
        std::ostringstream trailing(std::ios::binary|std::ios::out);
        persistence_detail::BinaryWriter trailing_writer{trailing};
        trailing_writer(persistence_detail::checksum(bytes));
        bytes+=trailing.str();
        const auto tmp=path.string()+".uncommitted";
        if(std::filesystem::exists(path))
            throw std::runtime_error("refusing to overwrite committed checkpoint");
        {
            std::ofstream out(tmp,std::ios::binary|std::ios::trunc);
            if(!out)throw std::runtime_error("snapshot temp file open failed");
            out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));
            out.flush();
            if(!out)throw std::runtime_error("snapshot temp write failed");
        }
        std::filesystem::rename(tmp,path);
    }
    static IntegratedWorld load(const std::filesystem::path& path){
        std::ifstream in(path,std::ios::binary);
        if(!in)throw std::runtime_error("missing snapshot file");
        const std::string bytes{std::istreambuf_iterator<char>{in},std::istreambuf_iterator<char>{}};
        if(bytes.size()<48||bytes.size()>512'000'000)
            throw std::runtime_error("invalid snapshot size");
        std::istringstream is(bytes,std::ios::binary|std::ios::in);
        persistence_detail::BinaryReader rd{is};
        std::string magic;std::uint64_t n{};
        rd(magic);
        if (magic!="VIC25_SNAPSHOT_PHASEC_2")
            throw std::runtime_error("unsupported checkpoint schema version");
        for (const char* expected_version : contract_header()) {
            std::string version;
            rd(version);
            if (version!=expected_version)
                throw std::runtime_error("checkpoint contract, numeric policy or ruleset version mismatch");
        }
        rd(n);
        const auto header_pos=is.tellg();
        if(header_pos<0 || n>512'000'000 ||
           static_cast<std::uint64_t>(bytes.size()-static_cast<std::size_t>(header_pos))!=n+8)
            throw std::runtime_error("truncated snapshot");
        std::string payload(static_cast<std::size_t>(n),'\0');
        is.read(payload.data(),static_cast<std::streamsize>(n));
        if(!is)throw std::runtime_error("truncated payload");
        std::uint64_t expected{};rd(expected);
        const auto prefix=bytes.substr(0,bytes.size()-8);
        if(expected!=persistence_detail::checksum(prefix))
            throw std::runtime_error("snapshot checksum mismatch");
        return decode(payload);
    }
};
} // namespace vic25::phasec
