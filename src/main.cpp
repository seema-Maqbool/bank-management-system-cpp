// Bank Management System - console application
// Features: accounts (Savings/Current), PIN login (hashed), admin login, deposit, withdraw,
// transfer, monthly interest, loans, mini statement, full history with date/time,
// search & sort, update account, daily limit, minimum balance.

#include <iomanip>
#include <iostream>

#include "Bank.h"

using namespace std;
using namespace util;

// ================================================================= printing helpers

static void printAccount(const Account& a)
{
    cout << "\n========== ACCOUNT DETAILS ==========\n";
    cout << "Account Number : " << a.number() << "\n";
    cout << "Account Holder : " << a.name() << "\n";
    cout << "Account Type   : " << a.type() << "\n";
    cout << "Balance        : " << formatMoney(a.balance()) << "\n";
    if (a.loanBalance() > 0)
    {
        cout << "Loan Due       : " << formatMoney(a.loanBalance()) << "\n";
        cout << "Installment    : " << formatMoney(a.loanInstallment()) << "\n";
    }
}

static void printAccountTable(const vector<const Account*>& list)
{
    if (list.empty())
    {
        cout << "\nNo accounts found.\n";
        return;
    }
    cout << "\n" << left << setw(10) << "Acc No" << setw(24) << "Name" << setw(10) << "Type" << setw(18)
         << "Balance" << "Loan Due\n";
    cout << string(72, '-') << "\n";
    for (const Account* a : list)
    {
        cout << left << setw(10) << a->number() << setw(24) << a->name() << setw(10) << a->type() << setw(18)
             << formatMoney(a->balance()) << (a->loanBalance() > 0 ? formatMoney(a->loanBalance()) : "-") << "\n";
    }
}

static void printTransactions(const vector<Transaction>& list)
{
    if (list.empty())
    {
        cout << "\nNo transactions found.\n";
        return;
    }
    cout << "\n" << left << setw(21) << "Date & Time" << setw(20) << "Type" << setw(16) << "Amount"
         << "Balance After\n";
    cout << string(72, '-') << "\n";
    for (const auto& t : list)
    {
        cout << left << setw(21) << t.timestamp << setw(20) << t.type << setw(16) << formatMoney(t.amount)
             << formatMoney(t.balanceAfter) << "\n";
    }
}

static void printError(const exception& e) { cout << "\nError: " << e.what() << "\n"; }

// ================================================================= customer menu

static void customerMenu(Bank& bank, int number)
{
    while (true)
    {
        cout << "\n=====================================\n";
        cout << "  Welcome, " << bank.get(number).name() << "  (Acc " << number << ")\n";
        cout << "=====================================\n";
        cout << " 1. Deposit Money\n";
        cout << " 2. Withdraw Money\n";
        cout << " 3. Check Balance\n";
        cout << " 4. Account Details\n";
        cout << " 5. Mini Statement (last 5)\n";
        cout << " 6. Full Transaction History\n";
        cout << " 7. Transfer Money\n";
        cout << " 8. Calculate Interest (Savings, once a month)\n";
        cout << " 9. Take Loan\n";
        cout << "10. Pay Loan Installment\n";
        cout << "11. Pay Custom Loan Amount\n";
        cout << "12. Update Account Name\n";
        cout << "13. Change PIN\n";
        cout << "14. Logout\n";

        int choice = readInt("Enter your choice: ", 1, 14);

        try
        {
            switch (choice)
            {
                case 1:
                {
                    Money amt = readAmount("Enter amount to deposit: ");
                    bank.deposit(number, amt);
                    cout << "\nDeposited successfully! New balance: " << formatMoney(bank.get(number).balance())
                         << "\n";
                    break;
                }
                case 2:
                {
                    Money amt = readAmount("Enter amount to withdraw: ");
                    bank.withdraw(number, amt);
                    cout << "\nWithdrawn successfully! Remaining balance: "
                         << formatMoney(bank.get(number).balance()) << "\n";
                    break;
                }
                case 3:
                {
                    const Account& a = bank.get(number);
                    cout << "\nAccount Type    : " << a.type() << "\n";
                    cout << "Current Balance : " << formatMoney(a.balance()) << "\n";
                    break;
                }
                case 4:
                    printAccount(bank.get(number));
                    break;
                case 5:
                    printTransactions(bank.miniStatement(number, 5));
                    break;
                case 6:
                    printTransactions(bank.history(number));
                    break;
                case 7:
                {
                    int to = readInt("Enter receiver account number: ");
                    if (!bank.exists(to))
                    {
                        cout << "\nReceiver account not found.\n";
                        break;
                    }
                    Money amt = readAmount("Enter amount to transfer: ");
                    bank.transfer(number, to, amt);
                    cout << "\nTransferred " << formatMoney(amt) << " to account " << to
                         << ". Remaining balance: " << formatMoney(bank.get(number).balance()) << "\n";
                    break;
                }
                case 8:
                {
                    Money interest = bank.addInterest(number);
                    cout << "\nInterest credited: " << formatMoney(interest) << " (" << SAVINGS_INTEREST_RATE
                         << "% per year). New balance: " << formatMoney(bank.get(number).balance()) << "\n";
                    break;
                }
                case 9:
                {
                    cout << "\nLoan terms: " << LOAN_INTEREST_PERCENT << "% flat interest, max "
                         << LOAN_MAX_MULTIPLIER << "x your balance, up to " << LOAN_MAX_MONTHS << " months.\n";
                    Money amt = readAmount("Enter loan amount: ");
                    int months = readInt("Enter duration in months: ", 1, LOAN_MAX_MONTHS);
                    Money total = bank.takeLoan(number, amt, months);
                    cout << "\nLoan approved! Total payable: " << formatMoney(total)
                         << "\nMonthly installment: " << formatMoney(bank.get(number).loanInstallment()) << "\n";
                    break;
                }
                case 10:
                {
                    Money paid = bank.payInstallment(number);
                    cout << "\nPaid " << formatMoney(paid)
                         << ". Loan remaining: " << formatMoney(bank.get(number).loanBalance()) << "\n";
                    break;
                }
                case 11:
                {
                    if (bank.get(number).loanBalance() <= 0)
                    {
                        cout << "\nYou have no active loan.\n";
                        break;
                    }
                    cout << "\nLoan remaining: " << formatMoney(bank.get(number).loanBalance()) << "\n";
                    Money amt = readAmount("Enter amount to pay: ");
                    Money paid = bank.repayLoan(number, amt);
                    cout << "\nPaid " << formatMoney(paid)
                         << ". Loan remaining: " << formatMoney(bank.get(number).loanBalance()) << "\n";
                    break;
                }
                case 12:
                {
                    string name = readName("Enter new name: ");
                    bank.updateName(number, name);
                    cout << "\nName updated successfully!\n";
                    break;
                }
                case 13:
                {
                    string oldPin = readPin("Enter current PIN: ");
                    string newPin = readPin("Enter new 4-digit PIN: ");
                    string again = readPin("Confirm new PIN: ");
                    if (newPin != again)
                    {
                        cout << "\nPINs do not match. PIN not changed.\n";
                        break;
                    }
                    bank.changePin(number, oldPin, newPin);
                    cout << "\nPIN changed successfully!\n";
                    break;
                }
                case 14:
                    cout << "\nLogged out.\n";
                    return;
            }
        }
        catch (const BankError& e)
        {
            printError(e);
        }
    }
}

// ================================================================= admin menu

static void adminMenu(Bank& bank)
{
    if (bank.verifyAdmin("admin123"))
        cout << "\nWARNING: You are using the default admin password. Please change it (option 8).\n";

    while (true)
    {
        cout << "\n=====================================\n";
        cout << "            ADMIN PANEL\n";
        cout << "=====================================\n";
        cout << "1. All Accounts (sorted)\n";
        cout << "2. Search Account by Name\n";
        cout << "3. Search Account by Number\n";
        cout << "4. View Account Transaction History\n";
        cout << "5. Delete Account\n";
        cout << "6. Apply Interest to All Savings Accounts\n";
        cout << "7. Total Bank Summary\n";
        cout << "8. Change Admin Password\n";
        cout << "9. Logout\n";

        int choice = readInt("Enter your choice: ", 1, 9);

        try
        {
            switch (choice)
            {
                case 1:
                {
                    cout << "\nSort by: 1. Account Number  2. Name  3. Balance (high to low)\n";
                    int s = readInt("Enter choice: ", 1, 3);
                    SortKey key = s == 1 ? SortKey::Number : (s == 2 ? SortKey::Name : SortKey::BalanceHighToLow);
                    printAccountTable(bank.sortedAccounts(key));
                    break;
                }
                case 2:
                {
                    string q = readLine("Enter name (or part of it): ");
                    printAccountTable(bank.searchByName(q));
                    break;
                }
                case 3:
                {
                    int n = readInt("Enter account number: ");
                    printAccount(bank.get(n));
                    break;
                }
                case 4:
                {
                    int n = readInt("Enter account number: ");
                    printTransactions(bank.history(n));
                    break;
                }
                case 5:
                {
                    int n = readInt("Enter account number to delete: ");
                    const Account& a = bank.get(n);
                    cout << "Account: " << a.name() << " | Balance: " << formatMoney(a.balance()) << "\n";
                    string sure = toLower(readLine("Type YES to confirm deletion (its transactions will be deleted too): "));
                    if (sure != "yes")
                    {
                        cout << "\nDeletion cancelled.\n";
                        break;
                    }
                    Money payout = bank.deleteAccount(n);
                    cout << "\nAccount deleted. Pay out remaining balance to customer: " << formatMoney(payout) << "\n";
                    break;
                }
                case 6:
                {
                    int count = bank.addInterestToAll();
                    cout << "\nInterest applied to " << count << " account(s).\n";
                    break;
                }
                case 7:
                {
                    Money total = 0, loans = 0;
                    for (const Account* a : bank.sortedAccounts(SortKey::Number))
                    {
                        total += a->balance();
                        loans += a->loanBalance();
                    }
                    cout << "\nTotal accounts       : " << bank.accountCount() << "\n";
                    cout << "Total deposits       : " << formatMoney(total) << "\n";
                    cout << "Total loans pending  : " << formatMoney(loans) << "\n";
                    break;
                }
                case 8:
                {
                    string oldPw = readLine("Enter current admin password: ");
                    string newPw = readLine("Enter new password (min 6 chars): ");
                    bank.changeAdminPassword(oldPw, newPw);
                    cout << "\nAdmin password changed successfully!\n";
                    break;
                }
                case 9:
                    cout << "\nAdmin logged out.\n";
                    return;
            }
        }
        catch (const BankError& e)
        {
            printError(e);
        }
    }
}

// ================================================================= main flows

static void createAccountFlow(Bank& bank)
{
    cout << "\n========== CREATE ACCOUNT ==========\n";
    int number = readInt("Enter Account Number: ", 1, 999999999);
    if (bank.exists(number))
    {
        cout << "\nAccount number already exists!\n";
        return;
    }

    string name = readName("Enter Account Holder Name: ");

    cout << "\nSelect Account Type:\n1. Savings Account (min balance " << formatMoney(SAVINGS_MIN_BALANCE)
         << ")\n2. Current Account\n";
    string type = readInt("Enter choice: ", 1, 2) == 1 ? "Savings" : "Current";

    string pin = readPin("Set a 4-digit PIN: ");
    string again = readPin("Confirm PIN: ");
    if (pin != again)
    {
        cout << "\nPINs do not match. Account not created.\n";
        return;
    }

    Money initial = readAmount("Enter Initial Balance: ", true);

    try
    {
        bank.createAccount(type, number, name, pin, initial);
        cout << "\nAccount created successfully!\n";
    }
    catch (const BankError& e)
    {
        printError(e);
    }
}

static void customerLoginFlow(Bank& bank)
{
    cout << "\n========== CUSTOMER LOGIN ==========\n";
    int number = readInt("Enter Account Number: ");

    for (int attempt = 1; attempt <= 3; ++attempt)
    {
        string pin = readPin("Enter PIN: ");
        if (bank.authenticate(number, pin))
        {
            customerMenu(bank, number);
            return;
        }
        cout << "Wrong account number or PIN. Attempts left: " << (3 - attempt) << "\n";
    }
    cout << "\nToo many failed attempts. Returning to main menu.\n";
}

static void adminLoginFlow(Bank& bank)
{
    cout << "\n========== ADMIN LOGIN ==========\n";
    for (int attempt = 1; attempt <= 3; ++attempt)
    {
        string pw = readLine("Enter admin password: ");
        if (bank.verifyAdmin(pw))
        {
            adminMenu(bank);
            return;
        }
        cout << "Wrong password. Attempts left: " << (3 - attempt) << "\n";
    }
    cout << "\nToo many failed attempts. Returning to main menu.\n";
}

int main()
{
    try
    {
        Bank bank("data");

        if (bank.skippedRecords() > 0)
            cout << "Note: " << bank.skippedRecords() << " corrupt record(s) in data files were skipped.\n";

        // Monthly interest is applied automatically once per month for every Savings account.
        int credited = bank.addInterestToAll();
        if (credited > 0) cout << "Monthly interest applied to " << credited << " savings account(s).\n";

        while (true)
        {
            cout << "\n=====================================\n";
            cout << "       BANK MANAGEMENT SYSTEM\n";
            cout << "=====================================\n";
            cout << "1. Create Account\n";
            cout << "2. Customer Login\n";
            cout << "3. Admin Login\n";
            cout << "4. Exit\n";

            int choice = readInt("Enter your choice: ", 1, 4);
            if (choice == 1) createAccountFlow(bank);
            else if (choice == 2) customerLoginFlow(bank);
            else if (choice == 3) adminLoginFlow(bank);
            else
            {
                cout << "\nThank you for using Bank Management System.\n";
                return 0;
            }
        }
    }
    catch (const InputClosed&)
    {
        cout << "\nInput closed. Goodbye.\n";
        return 0;
    }
    catch (const exception& e)
    {
        cerr << "\nFatal error: " << e.what() << "\n";
        return 1;
    }
}
