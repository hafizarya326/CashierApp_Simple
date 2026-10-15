# Simple Cashier App (C++ + XAMPP MySQL)

A beginner-friendly cashier (POS) program. Plain C++ and the Windows API for the GUI (no Qt, no
frameworks), XAMPP MySQL for data, Gemini AI only for sales advice. The screens are in Indonesian.

## 1. Install XAMPP
Download from https://www.apachefriends.org and install it.

## 2. Start MySQL
Open **XAMPP Control Panel** and click **Start** next to **MySQL** (and **Apache**, which phpMyAdmin needs).

## 3. Open phpMyAdmin
Go to http://localhost/phpmyadmin in your browser.

## 4. Import database.sql
Click the **Import** tab -> **Choose file** -> select `database.sql` -> **Import**.
You should now see the database `cashier_db` with 3 tables and 9 sample products
(plus 6 sample sales so history, best-seller and Gemini work right away).

## 5. Install the C++ compiler and the MySQL library
1. Install **MSYS2** from https://www.msys2.org (default folder `C:\msys64`).
2. Open **MSYS2 MSYS** or **MSYS2 UCRT64** from the Start menu and run:
   ```
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-libmariadbclient
   ```
3. Add `C:\msys64\ucrt64\bin` to your Windows **PATH**
   (Start -> "Edit the system environment variables" -> Environment Variables -> Path -> New).
   This lets Windows find `g++` and the MySQL library DLL.

## 6. Configure the MySQL connection
Open `Database.cpp`. The settings are at the top (already the XAMPP defaults):
```cpp
static const char* DB_HOST = "127.0.0.1";   // same as localhost
static const unsigned int DB_PORT = 3306;
static const char* DB_USER = "root";
static const char* DB_PASSWORD = "";        // empty
static const char* DB_NAME = "cashier_db";
```
Change them only if your XAMPP is different (for example another port).

## 7. Configure GEMINI_API_KEY
1. Create a key at https://aistudio.google.com/apikey
2. In Command Prompt run: `setx GEMINI_API_KEY "YOUR_KEY_HERE"`
3. **Close and reopen** the Command Prompt. Never write the key in the code.

(Optional) `setx GEMINI_MODEL "model-name"` changes the model. Default: `gemini-flash-latest`.
Everything except the Gemini tab works without a key.

## 8. Compile
Double-click `build.bat` (or run it in Command Prompt). The script auto-detects the MSYS2 environment and runs the correct command:
```
C:\msys64\ucrt64\bin\g++.exe main.cpp Cashier.cpp Database.cpp Product.cpp Transaction.cpp Gemini.cpp -o CashierApp.exe ^
  -I"C:\msys64\ucrt64\include" -I"C:\msys64\ucrt64\include\mariadb" -lmariadb -lwinhttp -lcomctl32 -mwindows -static-libgcc -static-libstdc++
```
If it says `mysql.h` is not found, re-run:
`pacman -S --needed mingw-w64-ucrt-x86_64-libmariadbclient`

## 9. Run
Make sure MySQL is started in XAMPP, then double-click `CashierApp.exe`.

---

## How to use it
* **Kasir**: pick a product -> type quantity -> *Tambah ke Keranjang* -> type payment (digits only, e.g. `50000`)
  -> *Proses Transaksi*. Total and change update while you type.
* **Data Barang**: search, add, edit, delete products, or change only the stock.
* **Riwayat & Terlaris**: sales history and the best-selling product.
* **Analisis Gemini AI**: click the button to get recommendations.

## What each file does
| File | Purpose |
|---|---|
| `main.cpp` | Starts the program: connects to MySQL (shows an error box if it fails), opens the window. |
| `Cashier.cpp/.h` | The GUI: tabs, buttons, lists, and the function behind every button. |
| `Database.cpp/.h` | The MySQL connection and three helpers: `RunQuery`, `GetResult`, `Escape`. |
| `Product.cpp/.h` | Product validation, search, add, edit, update stock, delete. |
| `Transaction.cpp/.h` | Subtotal, total, change, saving a sale, history, best-seller, number helpers. |
| `Gemini.cpp/.h` | Builds the sales question and talks to Gemini. |
| `database.sql` | Creates `cashier_db`, the tables and sample data. |
| `build.bat` | One-click compile. |

## Where `if` / `else if` / `else`, variables, functions and loops are used
* **Quantity check** - `OnAddToCart()` in `Cashier.cpp` and `Product` stock:
  `if (quantity <= 0)` -> invalid, `else if (quantity + inCart > stock)` -> not enough stock, `else` -> add to cart.
* **Payment and change** - `CheckPayment()` in `Transaction.cpp`:
  `if (payment < total)` insufficient, `else if (payment == total)` exact, `else change = payment - total;`.
  `OnProcess()` in `Cashier.cpp` uses it again to decide whether the sale may be saved.
* **Product input** - `ValidateProduct()` in `Product.cpp` (empty name, price <= 0, stock < 0).
* **Gemini status** - `AskGemini()` in `Gemini.cpp` (HTTP 429, 400/401/403, other errors).
* **Variables** - e.g. `long long total`, `int quantity`, `std::string name`.
* **Functions** - every action is a small function (`CalculateSubtotal`, `CalculateTotal`, `SaveTransaction` ...).
* **Loops** - `for` loops sum the cart (`CalculateTotal`), redraw lists, and save each cart item; `while` loops read SQL rows.

## How C++ connects to XAMPP MySQL
XAMPP's MySQL is a server listening on port 3306. `Database.cpp` uses the **MariaDB C connector**:
1. `mysql_init()` prepares a connection object.
2. `mysql_real_connect(host, user, password, database, port ...)` opens the connection. `NULL` means failure.
3. `mysql_query()` sends an SQL text. `mysql_store_result()` + `mysql_fetch_row()` read a SELECT row by row.
4. `mysql_real_escape_string()` (our `Escape()`) makes typed text safe so nobody can inject SQL.

## How the SQL works
* **Search products**: `SELECT ... FROM products WHERE name LIKE '%keyword%'`.
* **Save a sale** (`SaveTransaction`): `START TRANSACTION` -> `INSERT` into `transactions` ->
  for each cart item `INSERT` into `transaction_items` and
  `UPDATE products SET stock = stock - quantity WHERE id = ... AND stock >= quantity` -> `COMMIT`.
  If anything fails -> `ROLLBACK`, so the database never keeps half a sale.
  The total, payment and change values are calculated by C++ first; SQL only stores them.
* **Best-selling product**:
  ```sql
  SELECT p.name, SUM(ti.quantity) AS total_sold
  FROM transaction_items ti JOIN products p ON ti.product_id = p.id
  GROUP BY p.id, p.name ORDER BY total_sold DESC LIMIT 1;
  ```
  `JOIN` links each sold item to its product name, `GROUP BY` adds up the quantity per product,
  `ORDER BY ... DESC` puts the biggest first, `LIMIT 1` keeps only the winner.

## How Gemini works (`Gemini.cpp`)
1. `BuildSalesPrompt()` reads the sales per product from MySQL and makes a text like
   `Indomie Goreng = 50 sold, stock left 50`.
2. `AskGemini()` reads `GEMINI_API_KEY`, wraps the text in a small JSON, and sends an HTTPS POST to the
   Gemini API using **WinHTTP** (already part of Windows).
3. The reply is JSON; the program finds the `"text"` field and shows it in the window.
Gemini never receives or calculates prices, payments, change or stock. The program waits while Gemini answers
(up to about 30 seconds), so the window may look frozen for a moment.

## Error messages
MySQL connection failed, product not found, invalid quantity, insufficient stock, insufficient payment,
empty cart, transaction failed and Gemini failure all show a message box. Number boxes accept digits only;
anything else is treated as invalid instead of crashing.

## Troubleshooting
| Problem | Fix |
|---|---|
| "Gagal terhubung ke MySQL" | Start MySQL in XAMPP; check port/password in `Database.cpp`; import `database.sql`. |
| Port 3306 in use | Stop the other MySQL, or change the XAMPP port and `DB_PORT`. |
| `g++` not recognized | Add `C:\msys64\mingw64\bin` to PATH and reopen Command Prompt. |
| `mysql.h: No such file` | See step 8 (fix the `-I` folder). |
| EXE says `libmariadb-3.dll` missing | Add `C:\msys64\mingw64\bin` to PATH (step 5). |
| Gemini: key not set | Do step 7 and reopen the program from a new terminal. |
| Gemini HTTP 400/403 | Wrong key, or model unavailable (set `GEMINI_MODEL`). |
| Gemini HTTP 429 | Free quota used up; wait and retry. |
| Gemini cannot connect | Check internet / firewall. |
