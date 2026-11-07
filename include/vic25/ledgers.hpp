#pragma once

#include "vic25/population.hpp"
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace vic25 { namespace phasec { struct SnapshotCodec; }
using Amount = std::int64_t; // Smallest currency unit, never floating-point money.
using Quantity = std::int64_t; // Fixed-point count of commodity micro-units.

enum class AccountType { asset, liability, equity, revenue, expense };
struct Account {
    std::string id;
    std::string owner_id;
    AccountType type{};
    Amount debit_minus_credit{};
};
struct Posting {
    std::string account_id;
    Amount debit{};
    Amount credit{};
};
struct JournalEntry {
    std::string transaction_id;
    std::string memo;
    std::vector<Posting> postings;
};

class FinancialLedger {
    friend struct phasec::SnapshotCodec;
public:
    void add_account(std::string account_id, std::string owner_id, AccountType type) {
        if (account_id.empty() || owner_id.empty()) throw std::invalid_argument("empty account identifier");
        const auto [it, inserted] = accounts_.emplace(account_id, Account{account_id, std::move(owner_id), type, 0});
        (void)it;
        if (!inserted) throw std::invalid_argument("duplicate account");
    }

    void post(const JournalEntry& entry) {
        if (entry.transaction_id.empty() || entry.postings.size() < 2) {
            throw std::invalid_argument("journal entry requires ID and at least two lines");
        }
        if (transaction_ids_.contains(entry.transaction_id)) throw std::invalid_argument("duplicate transaction ID");
        std::map<std::string, Amount> by_owner;
        std::map<std::string, Amount> by_account;
        for (const auto& p : entry.postings) {
            if (p.debit < 0 || p.credit < 0 || (p.debit == 0) == (p.credit == 0)) {
                throw std::invalid_argument("each line requires exactly one positive debit or credit");
            }
            auto it = accounts_.find(p.account_id);
            if (it == accounts_.end()) throw std::invalid_argument("unknown account");
            const Amount delta = checked_add(p.debit, -p.credit);
            const Amount existing_owner = by_owner.contains(it->second.owner_id) ? by_owner.at(it->second.owner_id) : 0;
            by_owner[it->second.owner_id] = checked_add(existing_owner, delta);
            const Amount existing_account = by_account.contains(p.account_id) ? by_account.at(p.account_id) : 0;
            by_account[p.account_id] = checked_add(existing_account, delta);
        }
        for (const auto& [owner, delta] : by_owner) {
            (void)owner;
            if (delta != 0) throw std::invalid_argument("unbalanced entity journal");
        }
        for (const auto& [id, delta] : by_account) {
            (void)checked_add(accounts_.at(id).debit_minus_credit, delta);
        }
        for (const auto& [id, delta] : by_account) accounts_.at(id).debit_minus_credit += delta;
        transaction_ids_.insert(entry.transaction_id);
    }

    [[nodiscard]] bool has_account(const std::string& id)const {
        return accounts_.contains(id);
    }
    [[nodiscard]] Amount balance(const std::string& id) const { return accounts_.at(id).debit_minus_credit; }
    [[nodiscard]] bool balanced() const {
        std::map<std::string, Amount> totals;
        for (const auto& [id, account] : accounts_) {
            (void)id;
            const Amount prior = totals.contains(account.owner_id) ? totals.at(account.owner_id) : 0;
            totals[account.owner_id] = checked_add(prior, account.debit_minus_credit);
        }
        for (const auto& [id, total] : totals) { (void)id; if (total != 0) return false; }
        return true;
    }
private:
    std::map<std::string, Account> accounts_;
    std::set<std::string> transaction_ids_;
};

struct InventoryKey {
    std::string holder_id, region_id, good_id;
    auto operator<=>(const InventoryKey&) const = default;
};

class InventoryLedger {
    friend struct phasec::SnapshotCodec;
public:
    // Opening stock must be an explicit event, not silently inserted.
    void initialize(const InventoryKey& key, Quantity quantity) {
        if (quantity <= 0) throw std::invalid_argument("invalid initial quantity");
        change(key, quantity);
        add_total(opening_, key.good_id, quantity);
    }
    void produce(const InventoryKey& key, Quantity quantity) {
        if (quantity <= 0) throw std::invalid_argument("invalid production quantity");
        change(key, quantity);
        add_total(produced_, key.good_id, quantity);
    }
    void consume(const InventoryKey& key, Quantity quantity) {
        if (quantity <= 0 || stock(key) < quantity) throw std::invalid_argument("insufficient inventory");
        change(key, -quantity);
        add_total(consumed_, key.good_id, quantity);
    }
    void transfer(const InventoryKey& from, const InventoryKey& to, Quantity quantity) {
        if (from.good_id != to.good_id || quantity <= 0 || stock(from) < quantity) {
            throw std::invalid_argument("invalid inventory transfer");
        }
        if (from == to) return;
        const auto next = checked_add(stock(to), quantity); (void)next;
        change(from, -quantity);
        change(to, quantity);
    }
    [[nodiscard]] Quantity stock(const InventoryKey& key) const {
        auto it = stock_.find(key); return it == stock_.end() ? 0 : it->second;
    }
    [[nodiscard]] const std::map<InventoryKey,Quantity>& holdings()const {
        return stock_;
    }
    [[nodiscard]] bool conserved(const std::string& good) const {
        Quantity in_stock = 0;
        for (const auto& [key, quantity] : stock_) if (key.good_id == good) in_stock = checked_add(in_stock, quantity);
        return in_stock == checked_add(checked_add(total(opening_, good), total(produced_, good)), -total(consumed_, good));
    }
private:
    static Quantity total(const std::map<std::string, Quantity>& values, const std::string& good) {
        auto it = values.find(good); return it == values.end() ? 0 : it->second;
    }
    static void add_total(std::map<std::string, Quantity>& values, const std::string& good, Quantity amount) {
        values[good] = checked_add(total(values, good), amount);
    }
    void change(const InventoryKey& key, Quantity delta) {
        const Quantity next = checked_add(stock(key), delta);
        if (next < 0) throw std::invalid_argument("negative inventory");
        stock_[key] = next;
    }
    std::map<InventoryKey, Quantity> stock_;
    std::map<std::string, Quantity> opening_, produced_, consumed_;
};

struct EmploymentRecord {
    std::string pop_id, employer_id, profession_id;
    People count{};
    Amount monthly_wage_per_worker{};
};

class EmploymentLedger {
public:
    void upsert(const EmploymentRecord& record) {
        if (record.count < 0 || record.monthly_wage_per_worker < 0) throw std::invalid_argument("invalid employment");
        const auto key = std::make_tuple(record.pop_id, record.employer_id, record.profession_id);
        records_[key] = record;
    }
    [[nodiscard]] People employed(const std::string& pop_id) const {
        People sum = 0;
        for (const auto& [key, record] : records_) {
            (void)key;
            if (record.pop_id == pop_id) sum = checked_add(sum, record.count);
        }
        return sum;
    }
    [[nodiscard]] bool within_population(const Pop& pop) const { return employed(pop.id) <= pop.population(); }
private:
    std::map<std::tuple<std::string, std::string, std::string>, EmploymentRecord> records_;
};

class OwnershipLedger {
    friend struct phasec::SnapshotCodec;
public:
    void issue(const std::string& asset, const std::string& owner, std::int64_t units) {
        if (units <= 0 || issued_.contains(asset)) throw std::invalid_argument("invalid share issue");
        issued_[asset] = units;
        shares_[{asset, owner}] = units;
    }
    void transfer(const std::string& asset, const std::string& from, const std::string& to, std::int64_t units) {
        if (!issued_.contains(asset) || units <= 0 || shares_[{asset, from}] < units) {
            throw std::invalid_argument("invalid share transfer");
        }
        if (from == to) return;
        const auto dest = shares_[{asset, to}]; (void)checked_add(dest, units);
        shares_[{asset, from}] -= units;
        shares_[{asset, to}] += units;
    }
    // Bond retirement reduces the original issued asset, never creates a
    // second financial or security record. Partial redemption is supported.
    void redeem(const std::string& asset,const std::string& owner,
                std::int64_t quantity) {
        auto issuance=issued_.find(asset);
        auto holding=shares_.find({asset,owner});
        if(issuance==issued_.end()||holding==shares_.end()||
           quantity<=0||holding->second<quantity||
           issuance->second<quantity)
            throw std::invalid_argument("invalid asset redemption");
        issuance->second-=quantity;
        holding->second-=quantity;
        if(holding->second==0)shares_.erase(holding);
        // Keep zero-issued asset identity to reject duplicate future issues.
    }
    [[nodiscard]] std::map<std::string,std::int64_t> holders(
            const std::string& asset)const {
        std::map<std::string,std::int64_t> result;
        for(const auto& [key,n]:shares_)
            if(key.first==asset&&n>0)result.emplace(key.second,n);
        return result;
    }
    [[nodiscard]] std::int64_t units(const std::string& asset, const std::string& owner) const {
        auto it = shares_.find({asset, owner}); return it == shares_.end() ? 0 : it->second;
    }
    [[nodiscard]] bool conserved(const std::string& asset) const {
        const auto it = issued_.find(asset);
        if (it == issued_.end()) return false;
        std::int64_t sum = 0;
        for (const auto& [key, qty] : shares_) {
            if (key.first == asset) { if (qty < 0) return false; sum = checked_add(sum, qty); }
        }
        return sum == it->second;
    }
private:
    std::map<std::string, std::int64_t> issued_;
    std::map<std::pair<std::string, std::string>, std::int64_t> shares_;
};
} // namespace vic25
