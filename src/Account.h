#pragma once
// Account.h - Account base class + SavingsAccount / CurrentAccount (inheritance)

#include <memory>
#include <stdexcept>
#include <string>

#include "Utils.h"

using util::Money;

// All business-rule violations are reported with this exception.
class BankError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

// ---- Bank rules (all amounts in paisa) ----
constexpr Money SAVINGS_MIN_BALANCE = 50000;       // Rs. 500
constexpr Money SAVINGS_DAILY_LIMIT = 5000000;     // Rs. 50,000 per day
constexpr Money CURRENT_DAILY_LIMIT = 20000000;    // Rs. 200,000 per day
constexpr double SAVINGS_INTEREST_RATE = 5.0;      // % per year (applied monthly)
constexpr int LOAN_INTEREST_PERCENT = 10;          // flat, added to the loan
constexpr int LOAN_MAX_MULTIPLIER = 5;             // loan <= 5 x balance
constexpr int LOAN_MAX_MONTHS = 60;

class Account
{
public:
    Account(int number, std::string name, Money balance);
    virtual ~Account() = default;

    // ---- type specific rules ----
    virtual std::string type() const = 0;
    virtual Money minBalance() const = 0;
    virtual Money dailyLimit() const = 0;
    virtual Money applyInterest(const std::string& month) = 0;

    // ---- getters ----
    int number() const { return number_; }
    const std::string& name() const { return name_; }
    Money balance() const { return balance_; }
    Money loanBalance() const { return loanBalance_; }
    Money loanInstallment() const { return loanInstallment_; }
    Money withdrawnOn(const std::string& day) const;
    const std::string& lastInterestMonth() const { return lastInterestMonth_; }

    // ---- security ----
    void setPin(const std::string& pin);
    bool checkPin(const std::string& pin) const;

    // ---- operations (throw BankError when a rule is violated) ----
    void setName(const std::string& newName);
    void deposit(Money amount);
    void debit(Money amount, const std::string& day);  // withdraw / transfer out
    Money takeLoan(Money principal, int months);       // returns total payable
    Money repayLoan(Money amount);                     // returns amount actually paid
    void markInterestMonth(const std::string& month) { lastInterestMonth_ = month; }

    // ---- persistence ----
    std::string serialize() const;
    static std::unique_ptr<Account> deserialize(const std::string& line);
    static std::unique_ptr<Account> create(const std::string& type, int number,
                                           const std::string& name, Money balance);

protected:
    int number_;
    std::string name_;
    Money balance_;
    std::string pinSalt_;
    std::string pinHash_;
    std::string lastInterestMonth_;   // "YYYY-MM" or ""
    Money withdrawnToday_ = 0;
    std::string lastWithdrawDay_;     // "YYYY-MM-DD" or ""
    Money loanBalance_ = 0;
    Money loanInstallment_ = 0;
};

class SavingsAccount : public Account
{
public:
    using Account::Account;
    std::string type() const override { return "Savings"; }
    Money minBalance() const override { return SAVINGS_MIN_BALANCE; }
    Money dailyLimit() const override { return SAVINGS_DAILY_LIMIT; }
    Money applyInterest(const std::string& month) override;
};

class CurrentAccount : public Account
{
public:
    using Account::Account;
    std::string type() const override { return "Current"; }
    Money minBalance() const override { return 0; }
    Money dailyLimit() const override { return CURRENT_DAILY_LIMIT; }
    Money applyInterest(const std::string& month) override;
};
