#pragma once
// Verified transport bootstrap. Human-sourced capacities/transit days are
// mandatory; mere roads, airports and ports are NOT proof of freight service.
// Run only before the first simulation commit, when E and G seed functions
// still accept historical geography.
#include "vic25/phase_gh.hpp"
#include <charconv>
#include <cstdint>
#include <istream>
#include <map>
#include <tuple>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace vic25::verified_transport {
inline std::vector<std::string> split_tsv(const std::string& row) {
    std::vector<std::string> fields;
    std::size_t begin=0;
    while(true) {
        auto tab=row.find('\t',begin);
        fields.push_back(row.substr(begin,
            tab==std::string::npos?tab:tab-begin));
        if(tab==std::string::npos)break;
        begin=tab+1;
    }
    return fields;
}
template<class T>inline T required_number(const std::string& v) {
    T n{};
    const auto end=v.data()+v.size();
    const auto result=std::from_chars(v.data(),end,n);
    if(v.empty()||result.ec!=std::errc{}||result.ptr!=end)
        throw std::invalid_argument("GEO_ERR_UNVERIFIED_TRANSPORT_NUMBER");
    return n;
}
inline void check_columns(const std::vector<std::string>& actual,
                          const std::vector<std::string>& expected) {
    if(actual!=expected)
        throw std::invalid_argument("GEO_ERR_TRANSPORT_SCHEMA_MISMATCH");
}
inline bool next_row(std::istream& input,std::vector<std::string>& parts,
                     std::size_t columns) {
    std::string row;
    if(!std::getline(input,row))return false;
    if(!row.empty()&&row.back()=='\r')row.pop_back();
    parts=split_tsv(row);
    if(parts.size()!=columns)
        throw std::invalid_argument("GEO_ERR_TRANSPORT_ROW_WIDTH");
    for(const auto& p:parts)
        if(p.empty())throw std::invalid_argument(
            "GEO_ERR_UNVERIFIED_TRANSPORT_SOURCE_FIELD");
    return true;
}
// Schema intentionally separates PHYSICAL GEOGRAPHY from service data:
// A verified railway line alone does not establish a tariff, carrier,
// capacity, transit time, or the existence of that service in 1836.
inline void seed(phasegh::World& world,
                 std::istream& geographic_links,
                 std::istream& transport_services,
                 std::int64_t starting_month) {
    const std::vector<std::string> link_header{
        "link_id","source_feature","from_province","to_province","mode"};
    const std::vector<std::string> service_header{
        "service_id","link_id","carrier","from_province","to_province",
        "mode","monthly_capacity","freight_per_unit","transit_days",
        "valid_from_month","valid_through_month","source_hash"};
    std::vector<std::string> cols;
    if(!next_row(geographic_links,cols,link_header.size()))
        throw std::invalid_argument("GEO_ERR_NO_PROVENANCE");
    check_columns(cols,link_header);
    std::map<std::string,std::tuple<phasee::Id,phasee::Id,std::string,
                                    std::string>> real_links;
    std::size_t n=0;
    while(next_row(geographic_links,cols,link_header.size())) {
        if(++n>50'000'000)
            throw std::runtime_error("GEO_ERR_TOO_MANY_TRANSPORT_LINKS");
        const auto from=required_number<phasee::Id>(cols[2]);
        const auto to=required_number<phasee::Id>(cols[3]);
        if(from==0||to==0||from==to)
            throw std::invalid_argument("GEO_ERR_INVALID_PROVINCE_JOIN");
        if(!real_links.emplace(cols[0],
             std::make_tuple(from,to,cols[4],cols[1])).second)
            throw std::invalid_argument("GEO_ERR_DUPLICATE_TRANSPORT_LINK");
        world.seed_verified_geographic_link(
            cols[0],from,to,cols[4]);
    }
    if(!next_row(transport_services,cols,service_header.size()))
        throw std::invalid_argument("GEO_ERR_NO_SERVICE_SOURCE");
    check_columns(cols,service_header);
    std::set<std::string> ids;
    n=0;
    while(next_row(transport_services,cols,service_header.size())) {
        if(++n>20'000'000)
            throw std::runtime_error("GEO_ERR_TOO_MANY_TRANSPORT_SERVICES");
        const auto it=real_links.find(cols[1]);
        if(it==real_links.end()||!ids.insert(cols[0]).second)
            throw std::invalid_argument("GEO_ERR_ORPHAN_TRANSPORT_SERVICE");
        const auto from=required_number<phasee::Id>(cols[3]);
        const auto to=required_number<phasee::Id>(cols[4]);
        if(std::get<0>(it->second)!=from ||
           std::get<1>(it->second)!=to ||
           std::get<2>(it->second)!=cols[5])
            throw std::invalid_argument("GEO_ERR_SERVICE_SOURCE_DISAGREES");
        const auto first=required_number<phasegh::Month>(cols[9]);
        const auto last=required_number<phasegh::Month>(cols[10]);
        if(first<0||last<first)
            throw std::invalid_argument("GEO_ERR_UNDATED_TRANSPORT_SERVICE");
        // Preseed verified future services as dormant E/G evidence so
        // railways, roads and shipping can appear in their original month.
        // The path oracle will never use one before valid_from_month.
        // Services expired before the playable start need not consume memory.
        if(last<starting_month)
            continue;
        const auto cap=required_number<Quantity>(cols[6]);
        const auto fare=required_number<phasec::Money>(cols[7]);
        const auto days=required_number<std::int32_t>(cols[8]);
        if(cap<=0||fare<0||days<=0||cols[11].size()!=64)
            throw std::invalid_argument("GEO_ERR_UNVERIFIED_TRANSPORT_RATE");
        for(char h:cols[11])
            if(!((h>='0'&&h<='9')||(h>='a'&&h<='f')))
                throw std::invalid_argument("GEO_ERR_BAD_SOURCE_HASH");
        phasegh::TransportEdge edge;
        edge.id=cols[0];
        edge.carrier=cols[2];
        edge.from=from;
        edge.to=to;
        edge.monthly_capacity=cap;
        edge.freight_per_unit=fare;
        edge.transit_days=days;
        edge.mode=cols[5];
        edge.evidence_id=std::get<3>(it->second)+"@"+cols[11];
        edge.valid_from_month=first;
        edge.valid_through_month=last;
        world.seed_transport_edge(std::move(edge));
    }
}
} // namespace vic25::verified_transport
