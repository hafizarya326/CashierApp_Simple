-- database.sql : creates database, tables and sample data for the cashier app.
-- Import with phpMyAdmin (tab "Import"). Safe to import more than once.

CREATE DATABASE IF NOT EXISTS cashier_db;
USE cashier_db;

-- Products sold in the shop
CREATE TABLE IF NOT EXISTS products (
  id    INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
  name  VARCHAR(100) NOT NULL,
  price BIGINT NOT NULL,          -- price in Rupiah (whole number)
  stock INT NOT NULL DEFAULT 0
);

-- One row per receipt
CREATE TABLE IF NOT EXISTS transactions (
  id               INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
  total            BIGINT NOT NULL,
  payment          BIGINT NOT NULL,
  change_amount    BIGINT NOT NULL,
  transaction_date DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- One row per product inside a receipt
CREATE TABLE IF NOT EXISTS transaction_items (
  id             INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
  transaction_id INT NOT NULL,
  product_id     INT NOT NULL,
  quantity       INT NOT NULL,
  subtotal       BIGINT NOT NULL,
  FOREIGN KEY (transaction_id) REFERENCES transactions(id),
  FOREIGN KEY (product_id)     REFERENCES products(id)
);

-- Sample products (stock already matches the sample sales below)
INSERT IGNORE INTO products (id, name, price, stock) VALUES
  (1, 'Indomie Goreng', 3500,  50),
  (2, 'Aqua 600ml',     3000,  50),
  (3, 'Roti Tawar',     15000,  5),
  (4, 'Kopi Sachet',    2000,  30),
  (5, 'Coca-Cola',      5000,  24),
  (6, 'Teh Botol',      5000,  24),
  (7, 'Gula Pasir 1kg', 16000, 14),
  (8, 'Telur Ayam 1kg', 28000,  3),
  (9, 'Sabun Mandi',    4000,  25);

-- Sample sales so that history, best-selling and Gemini work immediately
INSERT IGNORE INTO transactions (id, total, payment, change_amount, transaction_date) VALUES
  (1,  50000,  50000,     0, '2026-10-01 09:15:00'),
  (2, 105000, 110000,  5000, '2026-10-01 14:40:00'),
  (3,  65000, 100000, 35000, '2026-10-02 10:05:00'),
  (4,  98500, 100000,  1500, '2026-10-03 16:20:00'),
  (5, 115000, 120000,  5000, '2026-10-04 11:30:00'),
  (6,  55500,  60000,  4500, '2026-10-05 13:10:00');

INSERT IGNORE INTO transaction_items (id, transaction_id, product_id, quantity, subtotal) VALUES
  ( 1, 1, 1, 10,  35000), ( 2, 1, 2,  5,  15000),
  ( 3, 2, 1, 20,  70000), ( 4, 2, 4, 10,  20000), ( 5, 2, 3, 1, 15000),
  ( 6, 3, 2, 15,  45000), ( 7, 3, 5,  4,  20000),
  ( 8, 4, 1, 15,  52500), ( 9, 4, 6,  6,  30000), (10, 4, 7, 1, 16000),
  (11, 5, 3,  3,  45000), (12, 5, 4, 20,  40000), (13, 5, 2, 10, 30000),
  (14, 6, 1,  5,  17500), (15, 6, 8,  1,  28000), (16, 6, 5,  2, 10000);
