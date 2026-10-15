// Product.h - product data and the functions that manage products in MySQL.
#ifndef PRODUCT_H
#define PRODUCT_H

#include <string>
#include <vector>

struct Product {
    int id;
    std::string name;
    long long price;
    int stock;
};

// Returns an error message, or "" if the input is valid.
std::string ValidateProduct(const std::string& name, long long price, long long stock);

bool LoadProducts(const std::string& keyword, std::vector<Product>& list); // keyword "" = all
bool GetProductById(int id, Product& product);
bool AddProduct(const std::string& name, long long price, long long stock, std::string& error);
bool EditProduct(int id, const std::string& name, long long price, long long stock, std::string& error);
bool UpdateStock(int id, long long newStock, std::string& error);
bool DeleteProduct(int id, std::string& error);

#endif
