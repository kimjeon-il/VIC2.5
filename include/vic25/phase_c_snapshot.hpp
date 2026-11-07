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
        return os.str();
    }
    static IntegratedWorld decode(const std::string& bytes){
        std::istringstream is(bytes,std::ios::binary|std::ios::in);
        persistence_detail::BinaryReader ar{is};
        IntegratedWorld w;
        all(ar,w);
        bool seen_elections=false,seen_fiscal=false,seen_interest=false,
             seen_birth_days=false,seen_wages=false,seen_bonds=false,
             seen_armies=false;
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
            } else {
                throw std::runtime_error("unknown or duplicate Phase C extension");
            }
        }
        if(w.stage_!=-1)throw std::runtime_error("staged month cannot be loaded");
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
