#pragma once
// Bank.h - owns all accounts, transactions, admin login and file storage.

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Account.h"

struct Transaction
{
    int accountNumber;
    std::string timestamp;  // "YYYY-MM-DD HH:MM:SS"
    std::string type;       // Deposit, Withdrawal, Transfer Sent, ...
    Money amount;
    Money balanceAfter;
};

enum class SortKey
{
    Number,
    Name,
    BalanceHighToLow
};

class Bank
{
public:
    // All data files live inside dataDir (created if missing).
    explicit Bank(std::string dataDir = "data");

    // For unit tests: replace the clock with a fake one.
    void setClock(std::function<std::string()> clock) { clock_ = std::move(clock); }

    // ---- admin ----
    bool verifyAdmin(const std::string& password) const;
    void changeAdminPassword(const std::string& oldPassword, const std::string& newPassword);

    // ---- accounts ----
    Account& createAccount(const std::string& type, int number, const std::string& name,
                           const std::string& pin, Money initialBalance);
    bool exists(int number) const;
    Account& get(int number);
    const Account& get(int number) const;
    bool authenticate(int number, const std::string& pin) const;
    Money deleteAccount(int number);  // returns balance paid out
    void updateName(int number, const std::string& newName);
    void changePin(int number, const std::string& oldPin, const std::string& newPin);

    // ---- money ----
    void deposit(int number, Money amount);
    void withdraw(int number, Money amount);
    void transfer(int from, int to, Money amount);
    Money addInterest(int number);          // returns interest credited
    int addInterestToAll();                 // returns how many accounts got interest
    Money takeLoan(int number, Money principal, int months);  // returns total payable
    Money payInstallment(int number);
    Money repayLoan(int number, Money amount);

    // ---- reports ----
    std::vector<Transaction> history(int number) const;
    std::vector<Transaction> miniStatement(int number, size_t count = 5) const;
    std::vector<const Account*> searchByName(const std::string& query) const;
    std::vector<const Account*> sortedAccounts(SortKey key) const;
    size_t accountCount() const { return accounts_.size(); }
    int skippedRecords() const { return skipped_; }

private:
    std::string dir_;
    std::function<std::string()> clock_;
    std::vector<std::unique_ptr<Account>> accounts_;
    std::vector<Transaction> transactions_;
    std::string adminSalt_;
    std::string adminHash_;
    int skipped_ = 0;

    std::string path(const char* file) const;
    std::string today() const { return clock_().substr(0, 10); }
    std::string month() const { return clock_().substr(0, 7); }

    void loadAdmin();
    void saveAdmin() const;
    void loadAccounts();
    void saveAccounts() const;
    void loadTransactions();
    void rewriteTransactions() const;
    void record(int number, const std::string& type, Money amount, Money balanceAfter);
};
