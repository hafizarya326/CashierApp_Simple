// Transaction.h - cart items, calculations, saving a sale, history and best-seller.
#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <string>
#include <vector>

// One line in the shopping cart
struct CartItem {
    int productId;
    std::string name;
    long long price;
    int quantity;
    long long subtotal;      // price x quantity
};

// ---------- Calculations (done by C++, never by SQL or AI) ----------
long long CalculateSubtotal(long long price, int quantity);
long long CalculateTotal(const std::vector<CartItem>& cart);

// Result of CheckPayment()
enum { PAYMENT_LESS = 0, PAYMENT_EXACT = 1, PAYMENT_MORE = 2 };
int CheckPayment(long long total, long long payment, long long& change);

// ---------- Save the sale to MySQL ----------
bool SaveTransaction(const std::vector<CartItem>& cart, long long total,
                     long long payment, long long change, std::string& error);

// ---------- Reports ----------
struct HistoryRow {
    int id;
    std::string date;
    long long total, payment, change;
};
struct SalesRow {
    std::string name;
    int sold;
    int stock;
};
bool LoadHistory(std::vector<HistoryRow>& list);
bool GetBestSelling(std::string& name, int& totalSold);
bool LoadSalesRanking(std::vector<SalesRow>& list);   // every product, best seller first

// ---------- Small helpers ----------
std::string FormatRupiah(long long value);                  // 25000 -> "Rp25.000"
bool ParseNumber(const std::string& text, long long& value); // digits only, e.g. "50000"
std::string Trim(const std::string& text);                  // remove spaces at both ends

#endif
