#pragma once
// One transient month-local reservation authority shared by legacy Phase C
// and Phase D. Actual money, commodities and people remain in their existing
// FinancialLedger, InventoryLedger and Phase B Population Registry.
// These are reservations and non-storable service usages, NOT duplicate stocks.
#include "vic25/ledgers.hpp"
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

namespace vic25::phasec {
class SharedResourceReservations {
    using Ticket=std::pair<std::string,std::string>; // resource owner, purpose
    using GoodsTicket=std::pair<InventoryKey,std::string>;
    std::map<std::string,Amount> cash_held_;
    std::map<Ticket,Amount> cash_tickets_;
    std::map<InventoryKey,Quantity> goods_held_;
    std::map<GoodsTicket,Quantity> goods_tickets_;
    std::map<std::string,std::int64_t> labor_used_;
    std::map<std::string,std::int64_t> site_services_used_;
    static void check(bool okay,const char* error) {
        if(!okay) throw std::invalid_argument(error);
    }
public:
    void clear_month() {
        cash_held_.clear();cash_tickets_.clear();
        goods_held_.clear();goods_tickets_.clear();
        labor_used_.clear();site_services_used_.clear();
    }
    [[nodiscard]] Amount cash_held(const std::string& who) const {
        auto it=cash_held_.find(who);return it==cash_held_.end()?0:it->second;
    }
    [[nodiscard]] Quantity goods_held(const InventoryKey& key) const {
        auto it=goods_held_.find(key);return it==goods_held_.end()?0:it->second;
    }
    [[nodiscard]] std::int64_t labor_used(const std::string& who) const {
        auto it=labor_used_.find(who);return it==labor_used_.end()?0:it->second;
    }
    [[nodiscard]] std::int64_t site_used(const std::string& site) const {
        auto it=site_services_used_.find(site);
        return it==site_services_used_.end()?0:it->second;
    }
    void assert_cash_available(const std::string& who, Amount amount,Amount available) const {
        check(amount>0 && available>=0 && cash_held(who)<=available &&
              amount<=available-cash_held(who),
              "shared financial reservation excludes attempted payment");
    }
    void reserve_cash(const std::string& who,const std::string& purpose,
                      Amount amount,Amount available) {
        check(!who.empty()&&!purpose.empty()&&amount>0,"bad cash reservation");
        assert_cash_available(who,amount,available);
        const auto t=Ticket{who,purpose};
        const auto ticket_next=phaseb::add_checked(cash_tickets_[t],amount);
        const auto held_next=phaseb::add_checked(cash_held_[who],amount);
        cash_tickets_[t]=ticket_next; cash_held_[who]=held_next;
    }
    void release_cash(const std::string& who,const std::string& purpose,Amount amount) {
        const auto ticket=Ticket{who,purpose};
        auto it=cash_tickets_.find(ticket);
        check(amount>0 && it!=cash_tickets_.end() && it->second>=amount,
              "cash payment lacks matching reserved purpose");
        check(cash_held(who)>=amount,"missing shared funds reservation");
        it->second-=amount;cash_held_[who]-=amount;
        if(!it->second)cash_tickets_.erase(it);
        if(!cash_held_[who])cash_held_.erase(who);
    }
    void assert_goods_available(const InventoryKey& key,Quantity amount,
                                Quantity actual_stock) const {
        check(amount>0&&actual_stock>=0&&goods_held(key)<=actual_stock &&
              amount<=actual_stock-goods_held(key),
              "shared goods reservation excludes attempted consumption/transfer");
    }
    void reserve_goods(const InventoryKey& key,const std::string& purpose,
                       Quantity amount, Quantity actual_stock) {
        check(!purpose.empty()&&amount>0,"bad goods reservation");
        assert_goods_available(key,amount,actual_stock);
        const auto t=GoodsTicket{key,purpose};
        const auto v=phaseb::add_checked(goods_tickets_[t],amount);
        const auto held=phaseb::add_checked(goods_held_[key],amount);
        goods_tickets_[t]=v;goods_held_[key]=held;
    }
    void release_goods(const InventoryKey& key,const std::string& purpose,
                       Quantity amount) {
        const auto ticket=GoodsTicket{key,purpose};
        auto it=goods_tickets_.find(ticket);
        check(amount>0&&it!=goods_tickets_.end()&&it->second>=amount,
              "actual material use lacks matching reservation");
        check(goods_held(key)>=amount,"missing inventory reservation");
        it->second-=amount;goods_held_[key]-=amount;
        if(!it->second)goods_tickets_.erase(it);
        if(!goods_held_[key])goods_held_.erase(key);
    }
    void use_labor(const std::string& employer,std::int64_t work_units,
                   std::int64_t available) {
        check(!employer.empty()&&work_units>0&&available>=0,
              "invalid shared labor service");
        const auto next=phaseb::add_checked(labor_used(employer),work_units);
        check(next<=available,"shared POP labor capacity overallocated");
        labor_used_[employer]=next;
    }
    void use_site(const std::string& site,std::int64_t work_units,
                  std::int64_t available) {
        check(!site.empty()&&work_units>0&&available>=0,
              "invalid shared capital service");
        const auto next=phaseb::add_checked(site_used(site),work_units);
        check(next<=available,"shared equipment capacity overallocated");
        site_services_used_[site]=next;
    }
    [[nodiscard]] bool empty_reservations() const {
        return cash_held_.empty()&&cash_tickets_.empty()&&
               goods_held_.empty()&&goods_tickets_.empty();
    }
    [[nodiscard]] bool valid() const {
        std::map<std::string,Amount> cash;
        std::map<InventoryKey,Quantity> goods;
        for(const auto& [ticket,n]:cash_tickets_) {
            if(n<=0)return false;cash[ticket.first]=phaseb::add_checked(cash[ticket.first],n);
        }
        for(const auto& [ticket,n]:goods_tickets_) {
            if(n<=0)return false;
            goods[ticket.first]=phaseb::add_checked(goods[ticket.first],n);
        }
        if(cash!=cash_held_||goods!=goods_held_)return false;
        for(const auto& [_,v]:labor_used_)if(v<0)return false;
        for(const auto& [_,v]:site_services_used_)if(v<0)return false;
        return true;
    }
};
} // namespace vic25::phasec
