#include "Bank.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

using namespace util;
namespace fs = std::filesystem;

static constexpr const char* DEFAULT_ADMIN_PASSWORD = "admin123";

Bank::Bank(std::string dataDir) : dir_(std::move(dataDir)), clock_(util::timestamp)
{
    fs::create_directories(dir_);
    loadAdmin();
    loadAccounts();
    loadTransactions();
}

std::string Bank::path(const char* file) const { return (fs::path(dir_) / file).string(); }

// ================================================================= admin

void Bank::loadAdmin()
{
    std::ifstream in(path("admin.txt"));
    std::string line;
    if (in && std::getline(in, line))
    {
        auto f = split(trim(line), '|');
        if (f.size() == 2)
        {
            adminSalt_ = f[0];
            adminHash_ = f[1];
            return;
        }
    }
    // First run: create the default admin password (admin should change it).
    adminSalt_ = randomSalt();
    adminHash_ = hashSecret(adminSalt_, DEFAULT_ADMIN_PASSWORD);
    saveAdmin();
}

void Bank::saveAdmin() const
{
    std::ofstream out(path("admin.txt"));
    if (!out) throw BankError("Could not write admin file.");
    out << adminSalt_ << "|" << adminHash_ << "\n";
}

bool Bank::verifyAdmin(const std::string& password) const
{
    return hashSecret(adminSalt_, password) == adminHash_;
}

void Bank::changeAdminPassword(const std::string& oldPassword, const std::string& newPassword)
{
    if (!verifyAdmin(oldPassword)) throw BankError("Current admin password is incorrect.");
    if (newPassword.size() < 6) throw BankError("New password must be at least 6 characters.");
    adminSalt_ = randomSalt();
    adminHash_ = hashSecret(adminSalt_, newPassword);
    saveAdmin();
}

// ================================================================= storage

void Bank::loadAccounts()
{
    std::ifstream in(path("accounts.txt"));
    if (!in) return;

    std::string line;
    while (std::getline(in, line))
    {
        line = trim(line);
        if (line.empty()) continue;
        try
        {
            auto acc = Account::deserialize(line);
            if (exists(acc->number())) throw BankError("Duplicate account number.");
            accounts_.push_back(std::move(acc));
        }
        catch (const BankError&)
        {
            ++skipped_;
        }
    }
}

void Bank::saveAccounts() const
{
    std::string tmp = path("accounts.txt.tmp");
    {
        std::ofstream out(tmp);
        if (!out) throw BankError("Could not write accounts file.");
        for (const auto& a : accounts_) out << a->serialize() << "\n";
        out.flush();
        if (!out) throw BankError("Could not write accounts file.");
    }
    fs::rename(tmp, path("accounts.txt"));  // write-then-rename: file never ends up half written
}

void Bank::loadTransactions()
{
    std::ifstream in(path("transactions.txt"));
    if (!in) return;

    std::string line;
    while (std::getline(in, line))
    {
        line = trim(line);
        if (line.empty()) continue;
        auto f = split(line, '|');
        if (f.size() != 5)
        {
            ++skipped_;
            continue;
        }
        try
        {
            transactions_.push_back({std::stoi(f[0]), f[1], f[2], std::stoll(f[3]), std::stoll(f[4])});
        }
        catch (const std::exception&)
        {
            ++skipped_;
        }
    }
}

void Bank::rewriteTransactions() const
{
    std::ofstream out(path("transactions.txt"), std::ios::trunc);
    if (!out) throw BankError("Could not write transactions file.");
    for (const auto& t : transactions_)
        out << t.accountNumber << "|" << t.timestamp << "|" << t.type << "|" << t.amount << "|"
            << t.balanceAfter << "\n";
}

void Bank::record(int number, const std::string& type, Money amount, Money balanceAfter)
{
    Transaction t{number, clock_(), type, amount, balanceAfter};
    std::ofstream out(path("transactions.txt"), std::ios::app);
    if (!out) throw BankError("Could not write transactions file.");
    out << t.accountNumber << "|" << t.timestamp << "|" << t.type << "|" << t.amount << "|" << t.balanceAfter
        << "\n";
    transactions_.push_back(t);
}

// ================================================================= accounts

bool Bank::exists(int number) const
{
    return std::any_of(accounts_.begin(), accounts_.end(),
                       [&](const std::unique_ptr<Account>& a) { return a->number() == number; });
}

Account& Bank::get(int number)
{
    for (auto& a : accounts_)
        if (a->number() == number) return *a;
    throw BankError("Account not found.");
}

const Account& Bank::get(int number) const
{
    for (const auto& a : accounts_)
        if (a->number() == number) return *a;
    throw BankError("Account not found.");
}

bool Bank::authenticate(int number, const std::string& pin) const
{
    return exists(number) && get(number).checkPin(pin);
}

Account& Bank::createAccount(const std::string& type, int number, const std::string& name,
                             const std::string& pin, Money initialBalance)
{
    if (number <= 0) throw BankError("Account number must be positive.");
    if (exists(number)) throw BankError("Account number already exists.");
    if (initialBalance < 0) throw BankError("Initial balance cannot be negative.");

    std::string cleanName = sanitizeField(name);
    if (cleanName.empty()) throw BankError("Account holder name cannot be empty.");
    if (cleanName.size() > 40) throw BankError("Name is too long (max 40 characters).");

    auto acc = Account::create(type, number, cleanName, initialBalance);
    if (initialBalance < acc->minBalance())
        throw BankError(type + " account needs an opening balance of at least " + formatMoney(acc->minBalance()) +
                        ".");

    acc->setPin(pin);
    acc->markInterestMonth(month());  // first interest is due next month

    accounts_.push_back(std::move(acc));
    Account& ref = *accounts_.back();
    if (initialBalance > 0) record(number, "Opening Deposit", initialBalance, initialBalance);
    saveAccounts();
    return ref;
}

Money Bank::deleteAccount(int number)
{
    Account& acc = get(number);
    if (acc.loanBalance() > 0)
        throw BankError("Cannot delete: account has an unpaid loan of " + formatMoney(acc.loanBalance()) + ".");

    Money payout = acc.balance();
    accounts_.erase(std::remove_if(accounts_.begin(), accounts_.end(),
                                   [&](const std::unique_ptr<Account>& a) { return a->number() == number; }),
                    accounts_.end());
    transactions_.erase(std::remove_if(transactions_.begin(), transactions_.end(),
                                       [&](const Transaction& t) { return t.accountNumber == number; }),
                        transactions_.end());
    saveAccounts();
    rewriteTransactions();  // old transactions of a deleted account are removed too
    return payout;
}

void Bank::updateName(int number, const std::string& newName)
{
    get(number).setName(newName);
    saveAccounts();
}

void Bank::changePin(int number, const std::string& oldPin, const std::string& newPin)
{
    Account& acc = get(number);
    if (!acc.checkPin(oldPin)) throw BankError("Current PIN is incorrect.");
    acc.setPin(newPin);
    saveAccounts();
}

// ================================================================= money

void Bank::deposit(int number, Money amount)
{
    Account& acc = get(number);
    acc.deposit(amount);
    record(number, "Deposit", amount, acc.balance());
    saveAccounts();
}

void Bank::withdraw(int number, Money amount)
{
    Account& acc = get(number);
    acc.debit(amount, today());
    record(number, "Withdrawal", amount, acc.balance());
    saveAccounts();
}

void Bank::transfer(int from, int to, Money amount)
{
    if (from == to) throw BankError("Sender and receiver accounts cannot be the same.");
    Account& sender = get(from);
    Account& receiver = get(to);  // both must exist before any money moves

    sender.debit(amount, today());
    receiver.deposit(amount);
    record(from, "Transfer Sent", amount, sender.balance());
    record(to, "Transfer Received", amount, receiver.balance());
    saveAccounts();
}

Money Bank::addInterest(int number)
{
    Account& acc = get(number);
    Money interest = acc.applyInterest(month());
    if (interest > 0) record(number, "Interest", interest, acc.balance());
    saveAccounts();
    return interest;
}

int Bank::addInterestToAll()
{
    int count = 0;
    for (auto& a : accounts_)
    {
        try
        {
            Money interest = a->applyInterest(month());
            if (interest > 0) record(a->number(), "Interest", interest, a->balance());
            ++count;
        }
        catch (const BankError&)
        {
            // Current account, or interest already applied this month -> skip
        }
    }
    if (count > 0) saveAccounts();
    return count;
}

Money Bank::takeLoan(int number, Money principal, int months)
{
    Account& acc = get(number);
    Money total = acc.takeLoan(principal, months);
    record(number, "Loan Received", principal, acc.balance());
    saveAccounts();
    return total;
}

Money Bank::repayLoan(int number, Money amount)
{
    Account& acc = get(number);
    Money paid = acc.repayLoan(amount);
    record(number, "Loan Payment", paid, acc.balance());
    saveAccounts();
    return paid;
}

Money Bank::payInstallment(int number)
{
    Account& acc = get(number);
    if (acc.loanBalance() <= 0) throw BankError("You have no active loan.");
    return repayLoan(number, std::min(acc.loanInstallment(), acc.loanBalance()));
}

// ================================================================= reports

std::vector<Transaction> Bank::history(int number) const
{
    get(number);  // throws if account is missing
    std::vector<Transaction> out;
    for (const auto& t : transactions_)
        if (t.accountNumber == number) out.push_back(t);
    return out;
}

std::vector<Transaction> Bank::miniStatement(int number, size_t count) const
{
    auto all = history(number);
    if (all.size() > count) all.erase(all.begin(), all.end() - static_cast<long>(count));
    return all;
}

std::vector<const Account*> Bank::searchByName(const std::string& query) const
{
    std::string q = toLower(trim(query));
    std::vector<const Account*> out;
    if (q.empty()) return out;
    for (const auto& a : accounts_)
        if (toLower(a->name()).find(q) != std::string::npos) out.push_back(a.get());
    return out;
}

std::vector<const Account*> Bank::sortedAccounts(SortKey key) const
{
    std::vector<const Account*> out;
    for (const auto& a : accounts_) out.push_back(a.get());

    std::sort(out.begin(), out.end(), [key](const Account* x, const Account* y) {
        switch (key)
        {
            case SortKey::Name:
                if (toLower(x->name()) != toLower(y->name())) return toLower(x->name()) < toLower(y->name());
                return x->number() < y->number();
            case SortKey::BalanceHighToLow:
                if (x->balance() != y->balance()) return x->balance() > y->balance();
                return x->number() < y->number();
            default:
                return x->number() < y->number();
        }
    });
    return out;
}
