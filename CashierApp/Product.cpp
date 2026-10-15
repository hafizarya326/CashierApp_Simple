// Product.cpp - add / edit / delete / search products (table "products").
#include "Product.h"
#include "Database.h"
#include <cstdlib>

// Checks the input with if / else if / else.
std::string ValidateProduct(const std::string& name, long long price, long long stock)
{
    if (name.empty()) {
        return "Nama barang tidak boleh kosong.";
    }
    else if (name.size() > 100) {
        return "Nama barang terlalu panjang (maksimal 100 huruf).";
    }
    else if (price <= 0) {
        return "Harga tidak valid. Harga harus lebih dari 0.";
    }
    else if (stock < 0) {
        return "Stok tidak valid. Stok tidak boleh negatif.";
    }
    else {
        return "";   // empty text = everything is OK
    }
}

// SELECT products whose name contains the keyword.
bool LoadProducts(const std::string& keyword, std::vector<Product>& list)
{
    list.clear();
    std::string sql = "SELECT id, name, price, stock FROM products "
                      "WHERE name LIKE '%" + Escape(keyword) + "%' ORDER BY name";
    MYSQL_RES* result = GetResult(sql);
    if (result == NULL)
        return false;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)) != NULL) {   // loop: one pass per product row
        Product p;
        p.id    = atoi(row[0]);
        p.name  = row[1];
        p.price = atoll(row[2]);
        p.stock = atoi(row[3]);
        list.push_back(p);
    }
    mysql_free_result(result);
    return true;
}

bool GetProductById(int id, Product& product)
{
    std::string sql = "SELECT id, name, price, stock FROM products WHERE id = " + std::to_string(id);
    MYSQL_RES* result = GetResult(sql);
    if (result == NULL)
        return false;

    MYSQL_ROW row = mysql_fetch_row(result);
    bool found = (row != NULL);
    if (found) {
        product.id    = atoi(row[0]);
        product.name  = row[1];
        product.price = atoll(row[2]);
        product.stock = atoi(row[3]);
    }
    mysql_free_result(result);
    return found;
}

bool AddProduct(const std::string& name, long long price, long long stock, std::string& error)
{
    std::string sql = "INSERT INTO products (name, price, stock) VALUES ('" + Escape(name) + "', "
                      + std::to_string(price) + ", " + std::to_string(stock) + ")";
    if (!RunQuery(sql)) {
        error = "Gagal menyimpan barang: " + LastDbError();
        return false;
    }
    return true;
}

bool EditProduct(int id, const std::string& name, long long price, long long stock, std::string& error)
{
    std::string sql = "UPDATE products SET name = '" + Escape(name) + "', price = " + std::to_string(price)
                      + ", stock = " + std::to_string(stock) + " WHERE id = " + std::to_string(id);
    if (!RunQuery(sql)) {
        error = "Gagal mengubah barang: " + LastDbError();
        return false;
    }
    return true;
}

bool UpdateStock(int id, long long newStock, std::string& error)
{
    std::string sql = "UPDATE products SET stock = " + std::to_string(newStock)
                      + " WHERE id = " + std::to_string(id);
    if (!RunQuery(sql)) {
        error = "Gagal mengubah stok: " + LastDbError();
        return false;
    }
    return true;
}

bool DeleteProduct(int id, std::string& error)
{
    std::string sql = "DELETE FROM products WHERE id = " + std::to_string(id);
    if (!RunQuery(sql)) {
        if (LastDbErrorCode() == 1451)   // 1451 = product is used by a past transaction
            error = "Barang tidak bisa dihapus karena sudah pernah terjual.";
        else
            error = "Gagal menghapus barang: " + LastDbError();
        return false;
    }
    return true;
}
