// Simple dependency-free unit tests. Build & run with:  make test

#include <filesystem>
#include <fstream>
#include <iostream>

#include "../src/Bank.h"

namespace fs = std::filesystem;
using namespace util;

static int passed = 0, failed = 0;

#define CHECK(cond)                                                                    \
    do                                                                                 \
    {                                                                                  \
        if (cond) ++passed;                                                            \
        else                                                                           \
        {                                                                              \
            ++failed;                                                                  \
            std::cout << "  FAIL (line " << __LINE__ << "): " #cond "\n";              \
        }                                                                              \
    } while (0)

#define CHECK_THROWS(expr)                                                             \
    do                                                                                 \
    {                                                                                  \
        bool threw = false;                                                            \
        try { expr; } catch (const BankError&) { threw = true; }                       \
        if (threw) ++passed;                                                           \
        else                                                                           \
        {                                                                              \
            ++failed;                                                                  \
            std::cout << "  FAIL (line " << __LINE__ << "): expected BankError from " #expr "\n"; \
        }                                                                              \
    } while (0)

static std::string fakeNow = "2026-01-15 10:00:00";

static fs::path freshDir(const std::string& name)
{
    fs::path p = fs::temp_directory_path() / ("bank_test_" + name);
    fs::remove_all(p);
    return p;
}

static Bank makeBank(const fs::path& dir)
{
    Bank b(dir.string());
    b.setClock([] { return fakeNow; });
    return b;
}

static void testUtils()
{
    std::cout << "Utils\n";
    CHECK(sha256("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(sha256("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    Money m;
    CHECK(parseMoney("500", m) && m == 50000);
    CHECK(parseMoney("12.5", m) && m == 1250);
    CHECK(parseMoney("0.07", m) && m == 7);
    CHECK(!parseMoney("abc", m));
    CHECK(!parseMoney("-5", m));
    CHECK(!parseMoney("1.234", m));
    CHECK(!parseMoney("", m));
    CHECK(!parseMoney("5abc", m));

    CHECK(formatMoney(123456) == "Rs. 1,234.56");
    CHECK(formatMoney(5) == "Rs. 0.05");
    CHECK(formatMoney(100000000) == "Rs. 1,000,000.00");
}

static void testCreateAndAuth()
{
    std::cout << "Create account & PIN\n";
    fakeNow = "2026-01-15 10:00:00";
    Bank b = makeBank(freshDir("create"));

    b.createAccount("Savings", 1, "Ali Khan", "1234", 100000);
    CHECK(b.exists(1));
    CHECK(b.authenticate(1, "1234"));
    CHECK(!b.authenticate(1, "0000"));
    CHECK(!b.authenticate(99, "1234"));

    CHECK_THROWS(b.createAccount("Savings", 1, "Dup", "1234", 100000));        // duplicate number
    CHECK_THROWS(b.createAccount("Savings", 2, "Poor", "1234", 100));          // below min balance
    CHECK_THROWS(b.createAccount("Savings", 3, "", "1234", 100000));           // empty name
    CHECK_THROWS(b.createAccount("Savings", 4, "Bad Pin", "12", 100000));      // bad PIN
    CHECK_THROWS(b.createAccount("Savings", -1, "Neg", "1234", 100000));       // bad number
    CHECK_THROWS(b.createAccount("Gold", 5, "Type", "1234", 100000));          // bad type
    CHECK(b.createAccount("Current", 6, "Zero Start", "1111", 0).balance() == 0);  // current allows 0

    b.changePin(1, "1234", "9999");
    CHECK(b.authenticate(1, "9999"));
    CHECK(!b.authenticate(1, "1234"));
    CHECK_THROWS(b.changePin(1, "wrong", "5555"));

    b.updateName(1, "Ali Raza");
    CHECK(b.get(1).name() == "Ali Raza");
    CHECK_THROWS(b.updateName(1, "   "));

    b.createAccount("Current", 7, "Pipe|Name", "1234", 1000);
    CHECK(b.get(7).name().find('|') == std::string::npos);  // separator can't corrupt the file
}

static void testDepositWithdraw()
{
    std::cout << "Deposit / withdraw / limits\n";
    fakeNow = "2026-01-15 10:00:00";
    Bank b = makeBank(freshDir("money"));
    b.createAccount("Savings", 1, "Sara", "1234", 200000);   // Rs. 2,000
    b.createAccount("Current", 2, "Biz", "1234", 100000);

    b.deposit(1, 50000);
    CHECK(b.get(1).balance() == 250000);
    CHECK_THROWS(b.deposit(1, 0));
    CHECK_THROWS(b.deposit(1, -5));

    b.withdraw(1, 100000);
    CHECK(b.get(1).balance() == 150000);
    CHECK_THROWS(b.withdraw(1, 120000));   // would drop below Rs. 500 minimum
    CHECK_THROWS(b.withdraw(1, 999999));   // insufficient
    CHECK(b.get(1).balance() == 150000);   // failed operations change nothing

    b.withdraw(2, 100000);                 // current account may go to zero
    CHECK(b.get(2).balance() == 0);

    // daily limit (Savings: Rs. 50,000)
    b.deposit(1, 10000000);                // +Rs. 100,000
    b.withdraw(1, 4000000);                // Rs. 40,000
    CHECK_THROWS(b.withdraw(1, 1500000));  // 40,000 + 15,000 > 50,000
    b.withdraw(1, 900000);                 // 1,000 + 40,000 + 9,000 = exactly Rs. 50,000
    CHECK_THROWS(b.withdraw(1, 100));      // limit used up
    fakeNow = "2026-01-16 09:00:00";       // next day -> limit resets
    b.withdraw(1, 100000);
    CHECK(b.get(1).withdrawnOn("2026-01-16") == 100000);
}

static void testTransfer()
{
    std::cout << "Transfer\n";
    fakeNow = "2026-01-15 10:00:00";
    Bank b = makeBank(freshDir("transfer"));
    b.createAccount("Savings", 1, "A", "1234", 200000);
    b.createAccount("Current", 2, "B", "1234", 50000);

    b.transfer(1, 2, 100000);
    CHECK(b.get(1).balance() == 100000);
    CHECK(b.get(2).balance() == 150000);
    CHECK_THROWS(b.transfer(1, 1, 100));        // same account
    CHECK_THROWS(b.transfer(1, 99, 100));       // receiver missing
    CHECK_THROWS(b.transfer(99, 1, 100));       // sender missing
    CHECK_THROWS(b.transfer(1, 2, 90000));      // would break savings minimum
    CHECK(b.get(1).balance() == 100000);
    CHECK(b.get(2).balance() == 150000);        // receiver untouched after failure

    auto h2 = b.history(2);
    CHECK(h2.back().type == "Transfer Received");
    CHECK(b.history(1).back().type == "Transfer Sent");
}

static void testInterest()
{
    std::cout << "Interest\n";
    fakeNow = "2026-01-15 10:00:00";
    Bank b = makeBank(freshDir("interest"));
    b.createAccount("Savings", 1, "S", "1234", 100000);   // Rs. 1,000
    b.createAccount("Current", 2, "C", "1234", 100000);

    CHECK_THROWS(b.addInterest(1));   // already "applied" for the creation month
    CHECK_THROWS(b.addInterest(2));   // current accounts never get interest

    fakeNow = "2026-02-01 08:00:00";
    Money i = b.addInterest(1);       // 1000 * 5% / 12 = 4.17
    CHECK(i == 417);
    CHECK(b.get(1).balance() == 100417);
    CHECK_THROWS(b.addInterest(1));   // only once per month

    fakeNow = "2026-03-01 08:00:00";
    CHECK(b.addInterestToAll() == 1); // only the savings account
    CHECK(b.addInterestToAll() == 0); // already applied this month
    CHECK(b.get(2).balance() == 100000);
}

static void testLoan()
{
    std::cout << "Loan\n";
    fakeNow = "2026-01-15 10:00:00";
    Bank b = makeBank(freshDir("loan"));
    b.createAccount("Current", 1, "L", "1234", 100000);   // Rs. 1,000

    CHECK_THROWS(b.takeLoan(1, 600000, 6));   // > 5 x balance
    CHECK_THROWS(b.takeLoan(1, 100000, 0));   // bad months
    CHECK_THROWS(b.takeLoan(1, 100000, 61));
    CHECK_THROWS(b.payInstallment(1));        // no loan yet

    Money total = b.takeLoan(1, 100000, 4);   // Rs. 1,000 loan, 10% -> Rs. 1,100
    CHECK(total == 110000);
    CHECK(b.get(1).balance() == 200000);
    CHECK(b.get(1).loanInstallment() == 27500);
    CHECK_THROWS(b.takeLoan(1, 1000, 2));     // already has a loan
    CHECK_THROWS(b.deleteAccount(1));         // cannot delete with loan

    b.payInstallment(1);
    CHECK(b.get(1).loanBalance() == 82500);
    CHECK(b.get(1).balance() == 172500);
    b.repayLoan(1, 10000000);                 // overpay -> only pays what is due
    CHECK(b.get(1).loanBalance() == 0);
    CHECK(b.get(1).balance() == 90000);
    CHECK_THROWS(b.repayLoan(1, 100));        // loan finished
}

static void testReports()
{
    std::cout << "History, search, sort\n";
    fakeNow = "2026-01-15 10:00:00";
    Bank b = makeBank(freshDir("reports"));
    b.createAccount("Current", 3, "Zainab Ahmed", "1234", 5000);
    b.createAccount("Current", 1, "ali hassan", "1234", 90000);
    b.createAccount("Savings", 2, "Bilal Ali", "1234", 70000);

    for (int i = 0; i < 7; ++i) b.deposit(3, 100);
    CHECK(b.history(3).size() == 8);                 // opening + 7 deposits
    CHECK(b.miniStatement(3, 5).size() == 5);
    CHECK(b.miniStatement(3).back().timestamp == fakeNow);
    CHECK_THROWS(b.history(99));

    CHECK(b.searchByName("ALI").size() == 2);        // case-insensitive, partial
    CHECK(b.searchByName("zain").size() == 1);
    CHECK(b.searchByName("nobody").empty());
    CHECK(b.searchByName("").empty());

    auto byNumber = b.sortedAccounts(SortKey::Number);
    CHECK(byNumber[0]->number() == 1 && byNumber[2]->number() == 3);
    auto byName = b.sortedAccounts(SortKey::Name);
    CHECK(byName[0]->name() == "ali hassan");
    auto byBal = b.sortedAccounts(SortKey::BalanceHighToLow);
    CHECK(byBal[0]->number() == 1);
}

static void testPersistenceAndDelete()
{
    std::cout << "Persistence, delete, admin\n";
    fakeNow = "2026-01-15 10:00:00";
    fs::path dir = freshDir("persist");
    {
        Bank b = makeBank(dir);
        b.createAccount("Savings", 1, "Keep Me", "4321", 100000);
        b.createAccount("Current", 2, "Delete Me", "1111", 5000);
        b.deposit(1, 2500);
        b.withdraw(1, 500);
        b.deposit(2, 100);
    }
    {
        Bank b = makeBank(dir);   // reload from disk
        CHECK(b.accountCount() == 2);
        CHECK(b.get(1).balance() == 102000);
        CHECK(b.authenticate(1, "4321"));              // hashed PIN survives restart
        CHECK(!b.authenticate(1, "1234"));
        CHECK(b.history(1).size() == 3);
        CHECK(b.skippedRecords() == 0);

        Money payout = b.deleteAccount(2);
        CHECK(payout == 5100);
        CHECK(!b.exists(2));
        CHECK_THROWS(b.deleteAccount(2));
    }
    {
        Bank b = makeBank(dir);
        CHECK(b.accountCount() == 1);
        // deleted account's transactions are gone, so a new account with same number starts clean
        b.createAccount("Current", 2, "New Owner", "2222", 0);
        CHECK(b.history(2).empty());

        // admin
        CHECK(b.verifyAdmin("admin123"));
        CHECK(!b.verifyAdmin("wrong"));
        CHECK_THROWS(b.changeAdminPassword("wrong", "newpass123"));
        CHECK_THROWS(b.changeAdminPassword("admin123", "abc"));
        b.changeAdminPassword("admin123", "newpass123");
    }
    {
        Bank b = makeBank(dir);
        CHECK(b.verifyAdmin("newpass123"));
        CHECK(!b.verifyAdmin("admin123"));
    }
}

static void testCorruptFiles()
{
    std::cout << "Corrupt data handling\n";
    fs::path dir = freshDir("corrupt");
    fs::create_directories(dir);
    {
        std::ofstream(dir / "accounts.txt") << "garbage line\nSavings|1|Ok|s|h|100000|-|0|-|0|0\nSavings|x|Bad|s|h|1|-|0|-|0|0\n";
        std::ofstream(dir / "transactions.txt") << "1|2026-01-01 00:00:00|Deposit|100|100\nnot|enough\n";
    }
    Bank b = makeBank(dir);
    CHECK(b.accountCount() == 1);
    CHECK(b.skippedRecords() == 3);   // 2 bad accounts + 1 bad transaction
    CHECK(b.history(1).size() == 1);
}

int main()
{
    testUtils();
    testCreateAndAuth();
    testDepositWithdraw();
    testTransfer();
    testInterest();
    testLoan();
    testReports();
    testPersistenceAndDelete();
    testCorruptFiles();

    std::cout << "\n" << passed << " checks passed, " << failed << " failed.\n";
    return failed == 0 ? 0 : 1;
}
