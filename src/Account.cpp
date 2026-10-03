#include "Account.h"

#include <algorithm>
#include <cmath>

using namespace util;

Account::Account(int number, std::string name, Money balance)
    : number_(number), name_(std::move(name)), balance_(balance)
{
}

Money Account::withdrawnOn(const std::string& day) const
{
    return lastWithdrawDay_ == day ? withdrawnToday_ : 0;
}

// ------------------------------------------------------------ security

void Account::setPin(const std::string& pin)
{
    if (pin.size() != 4 || !allDigits(pin)) throw BankError("PIN must be exactly 4 digits.");
    pinSalt_ = randomSalt();
    pinHash_ = hashSecret(pinSalt_, pin);
}

bool Account::checkPin(const std::string& pin) const
{
    return !pinHash_.empty() && hashSecret(pinSalt_, pin) == pinHash_;
}

// ------------------------------------------------------------ operations

void Account::setName(const std::string& newName)
{
    std::string clean = sanitizeField(newName);
    if (clean.empty()) throw BankError("Name cannot be empty.");
    if (clean.size() > 40) throw BankError("Name is too long (max 40 characters).");
    name_ = clean;
}

void Account::deposit(Money amount)
{
    if (amount <= 0) throw BankError("Amount must be greater than 0.");
    balance_ += amount;
}

void Account::debit(Money amount, const std::string& day)
{
    if (amount <= 0) throw BankError("Amount must be greater than 0.");

    if (balance_ - amount < minBalance())
    {
        if (minBalance() > 0 && amount <= balance_)
            throw BankError("A minimum balance of " + formatMoney(minBalance()) +
                            " must be maintained in a " + type() + " account.");
        throw BankError("Insufficient balance. Current balance: " + formatMoney(balance_));
    }

    Money alreadyToday = withdrawnOn(day);
    if (alreadyToday + amount > dailyLimit())
    {
        throw BankError("Daily limit of " + formatMoney(dailyLimit()) + " exceeded. Already used today: " +
                        formatMoney(alreadyToday));
    }

    balance_ -= amount;
    withdrawnToday_ = alreadyToday + amount;
    lastWithdrawDay_ = day;
}

Money Account::takeLoan(Money principal, int months)
{
    if (loanBalance_ > 0) throw BankError("You already have an active loan. Repay it first.");
    if (principal <= 0) throw BankError("Loan amount must be greater than 0.");
    if (months < 1 || months > LOAN_MAX_MONTHS)
        throw BankError("Loan duration must be between 1 and " + std::to_string(LOAN_MAX_MONTHS) + " months.");

    Money maxLoan = balance_ * LOAN_MAX_MULTIPLIER;
    if (principal > maxLoan)
        throw BankError("Maximum loan you can take is " + formatMoney(maxLoan) + " (" +
                        std::to_string(LOAN_MAX_MULTIPLIER) + " x your balance).");

    Money total = principal + principal * LOAN_INTEREST_PERCENT / 100;
    loanBalance_ = total;
    loanInstallment_ = (total + months - 1) / months;  // round up
    balance_ += principal;
    return total;
}

Money Account::repayLoan(Money amount)
{
    if (loanBalance_ <= 0) throw BankError("You have no active loan.");
    if (amount <= 0) throw BankError("Amount must be greater than 0.");

    Money pay = std::min(amount, loanBalance_);
    if (balance_ - pay < minBalance())
        throw BankError("Not enough balance to pay " + formatMoney(pay) + " (keeping minimum balance " +
                        formatMoney(minBalance()) + ").");

    balance_ -= pay;
    loanBalance_ -= pay;
    if (loanBalance_ == 0) loanInstallment_ = 0;
    return pay;
}

// ------------------------------------------------------------ Savings / Current

Money SavingsAccount::applyInterest(const std::string& month)
{
    if (lastInterestMonth_ == month) throw BankError("Interest has already been applied for " + month + ".");
    Money interest = static_cast<Money>(std::llround(static_cast<double>(balance_) * SAVINGS_INTEREST_RATE / 1200.0));
    balance_ += interest;
    lastInterestMonth_ = month;
    return interest;
}

Money CurrentAccount::applyInterest(const std::string&)
{
    throw BankError("Interest is only available for Savings accounts.");
}

// ------------------------------------------------------------ persistence
// type|number|name|pinSalt|pinHash|balance|lastInterestMonth|withdrawnToday|lastWithdrawDay|loanBalance|loanInstallment

static std::string orDash(const std::string& s) { return s.empty() ? "-" : s; }
static std::string fromDash(const std::string& s) { return s == "-" ? "" : s; }

std::string Account::serialize() const
{
    return type() + "|" + std::to_string(number_) + "|" + name_ + "|" + pinSalt_ + "|" + pinHash_ + "|" +
           std::to_string(balance_) + "|" + orDash(lastInterestMonth_) + "|" + std::to_string(withdrawnToday_) +
           "|" + orDash(lastWithdrawDay_) + "|" + std::to_string(loanBalance_) + "|" +
           std::to_string(loanInstallment_);
}

std::unique_ptr<Account> Account::create(const std::string& type, int number, const std::string& name,
                                         Money balance)
{
    if (type == "Savings") return std::make_unique<SavingsAccount>(number, name, balance);
    if (type == "Current") return std::make_unique<CurrentAccount>(number, name, balance);
    throw BankError("Unknown account type: " + type);
}

std::unique_ptr<Account> Account::deserialize(const std::string& line)
{
    auto f = split(line, '|');
    if (f.size() != 11) throw BankError("Corrupt account record.");

    try
    {
        auto acc = create(f[0], std::stoi(f[1]), f[2], std::stoll(f[5]));
        acc->pinSalt_ = f[3];
        acc->pinHash_ = f[4];
        acc->lastInterestMonth_ = fromDash(f[6]);
        acc->withdrawnToday_ = std::stoll(f[7]);
        acc->lastWithdrawDay_ = fromDash(f[8]);
        acc->loanBalance_ = std::stoll(f[9]);
        acc->loanInstallment_ = std::stoll(f[10]);
        return acc;
    }
    catch (const BankError&)
    {
        throw;
    }
    catch (const std::exception&)
    {
        throw BankError("Corrupt account record.");
    }
}
