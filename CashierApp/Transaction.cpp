// Transaction.cpp - cashier calculations and saving transactions.
#include "Transaction.h"
#include "Database.h"
#include "Product.h"
#include <cstdlib>

// Subtotal = Price x Quantity
long long CalculateSubtotal(long long price, int quantity)
{
    return price * quantity;
}

// Total = sum of all subtotals (a loop over the cart)
long long CalculateTotal(const std::vector<CartItem>& cart)
{
    long long total = 0;
    for (size_t i = 0; i < cart.size(); i++) {
        total = total + cart[i].subtotal;
    }
    return total;
}

// Compares payment and total, and calculates the change.
// Change = Payment - Total  (done here in C++).
int CheckPayment(long long total, long long payment, long long& change)
{
    if (payment < total) {
        change = 0;
        return PAYMENT_LESS;          // payment is insufficient
    }
    else if (payment == total) {
        change = 0;
        return PAYMENT_EXACT;         // exact payment
    }
    else {
        change = payment - total;     // calculate change
        return PAYMENT_MORE;
    }
}

// Saves the sale. All steps are inside START TRANSACTION ... COMMIT,
// so if one step fails we ROLLBACK and the database stays unchanged.
bool SaveTransaction(const std::vector<CartItem>& cart, long long total,
                     long long payment, long long change, std::string& error)
{
    if (!RunQuery("START TRANSACTION")) {
        error = "Gagal memulai transaksi: " + LastDbError();
        return false;
    }

    // 1) save the receipt
    std::string sql = "INSERT INTO transactions (total, payment, change_amount) VALUES ("
                      + std::to_string(total) + ", " + std::to_string(payment) + ", "
                      + std::to_string(change) + ")";
    if (!RunQuery(sql)) {
        error = "Gagal menyimpan transaksi: " + LastDbError();
        RunQuery("ROLLBACK");
        return false;
    }
    long long transactionId = LastInsertId();

    // 2) loop: save every cart item and reduce the stock
    for (size_t i = 0; i < cart.size(); i++) {
        sql = "INSERT INTO transaction_items (transaction_id, product_id, quantity, subtotal) VALUES ("
              + std::to_string(transactionId) + ", " + std::to_string(cart[i].productId) + ", "
              + std::to_string(cart[i].quantity) + ", " + std::to_string(cart[i].subtotal) + ")";
        if (!RunQuery(sql)) {
            error = "Gagal menyimpan barang transaksi: " + LastDbError();
            RunQuery("ROLLBACK");
            return false;
        }

        // new stock = old stock - quantity. "AND stock >= quantity" prevents negative stock.
        sql = "UPDATE products SET stock = stock - " + std::to_string(cart[i].quantity)
              + " WHERE id = " + std::to_string(cart[i].productId)
              + " AND stock >= " + std::to_string(cart[i].quantity);
        if (!RunQuery(sql)) {
            error = "Gagal mengurangi stok: " + LastDbError();
            RunQuery("ROLLBACK");
            return false;
        }
        else if (AffectedRows() != 1) {    // no row changed = not enough stock
            error = "Stok \"" + cart[i].name + "\" tidak cukup.";
            RunQuery("ROLLBACK");
            return false;
        }
    }

    // 3) everything worked
    if (!RunQuery("COMMIT")) {
        error = "Gagal menyimpan (COMMIT): " + LastDbError();
        RunQuery("ROLLBACK");
        return false;
    }
    return true;
}

bool LoadHistory(std::vector<HistoryRow>& list)
{
    list.clear();
    MYSQL_RES* result = GetResult(
        "SELECT id, transaction_date, total, payment, change_amount FROM transactions ORDER BY id DESC");
    if (result == NULL)
        return false;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)) != NULL) {
        HistoryRow h;
        h.id      = atoi(row[0]);
        h.date    = row[1];
        h.total   = atoll(row[2]);
        h.payment = atoll(row[3]);
        h.change  = atoll(row[4]);
        list.push_back(h);
    }
    mysql_free_result(result);
    return true;
}

// Best-selling product: add up quantity per product, biggest first, take 1 row.
bool GetBestSelling(std::string& name, int& totalSold)
{
    MYSQL_RES* result = GetResult(
        "SELECT p.name, SUM(ti.quantity) AS total_sold "
        "FROM transaction_items ti JOIN products p ON ti.product_id = p.id "
        "GROUP BY p.id, p.name ORDER BY total_sold DESC LIMIT 1");
    if (result == NULL)
        return false;

    MYSQL_ROW row = mysql_fetch_row(result);
    bool found = (row != NULL);
    if (found) {
        name = row[0];
        totalSold = atoi(row[1]);
    }
    mysql_free_result(result);
    return found;
}

// Same idea, but for ALL products (LEFT JOIN keeps products that never sold).
bool LoadSalesRanking(std::vector<SalesRow>& list)
{
    list.clear();
    MYSQL_RES* result = GetResult(
        "SELECT p.name, COALESCE(SUM(ti.quantity), 0) AS total_sold, p.stock "
        "FROM products p LEFT JOIN transaction_items ti ON ti.product_id = p.id "
        "GROUP BY p.id, p.name, p.stock ORDER BY total_sold DESC, p.name");
    if (result == NULL)
        return false;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)) != NULL) {
        SalesRow s;
        s.name  = row[0];
        s.sold  = atoi(row[1]);
        s.stock = atoi(row[2]);
        list.push_back(s);
    }
    mysql_free_result(result);
    return true;
}

// 25000 -> "Rp25.000"
std::string FormatRupiah(long long value)
{
    std::string digits = std::to_string(value < 0 ? -value : value);
    std::string result = "";
    int count = 0;
    for (int i = (int)digits.size() - 1; i >= 0; i--) {   // loop from the last digit
        result = digits[i] + result;
        count++;
        if (count % 3 == 0 && i > 0)
            result = "." + result;
    }
    return (value < 0 ? "-Rp" : "Rp") + result;
}

std::string Trim(const std::string& text)
{
    size_t start = 0;
    size_t end = text.size();
    while (start < end && (text[start] == ' ' || text[start] == '\t')) start++;
    while (end > start && (text[end - 1] == ' ' || text[end - 1] == '\t')) end--;
    return text.substr(start, end - start);
}

// Converts "50000" to the number 50000. Returns false for empty text,
// letters, minus sign or a number that is too long. The program never crashes on bad input.
bool ParseNumber(const std::string& text, long long& value)
{
    std::string t = Trim(text);
    value = 0;
    if (t.empty() || t.size() > 12)
        return false;
    for (size_t i = 0; i < t.size(); i++) {
        if (t[i] < '0' || t[i] > '9')
            return false;
        value = value * 10 + (t[i] - '0');
    }
    return true;
}
