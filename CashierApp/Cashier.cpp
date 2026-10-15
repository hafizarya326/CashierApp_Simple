// Cashier.cpp - the whole GUI, written with plain Windows (Win32) controls.
//
// The window has 4 tabs:  Kasir | Data Barang | Riwayat & Terlaris | Analisis Gemini AI
// Every control (button, text box, list) is created once in CreateControls().
// When the user clicks something, Windows calls WndProc() -> HandleCommand(),
// which runs the matching function (OnAddToCart, OnProcess, ...).
#include "Cashier.h"
#include "Database.h"
#include "Product.h"
#include "Transaction.h"
#include "Gemini.h"
#include <commctrl.h>
#include <string>
#include <vector>

// ---------- Control IDs: every control gets a number so we can find it ----------
enum {
    ID_TAB = 100,
    // tab 1: Kasir
    ID_C_SEARCH = 110, ID_C_PRODLIST, ID_C_NAME, ID_C_PRICE, ID_C_STOCK, ID_C_QTY, ID_C_ADD,
    ID_C_CARTLIST, ID_C_REMOVE, ID_C_CLEAR, ID_C_TOTAL, ID_C_PAYMENT, ID_C_CHANGE, ID_C_PROCESS,
    // tab 2: Data Barang
    ID_P_SEARCH, ID_P_LIST, ID_P_NAME, ID_P_PRICE, ID_P_STOCK,
    ID_P_ADD, ID_P_EDIT, ID_P_DELETE, ID_P_STOCKBTN, ID_P_CLEAR,
    // tab 3: Riwayat & Terlaris
    ID_H_REFRESH, ID_H_BEST, ID_H_LIST,
    // tab 4: Gemini
    ID_G_BUTTON, ID_G_OUTPUT
};

// ---------- Global variables (data shared by all functions) ----------
static HWND g_main = NULL;                       // main window
static HWND g_tab = NULL;                        // tab bar
static bool g_ready = false;                     // true when all controls exist
static std::vector<HWND> g_pageControls[4];      // which controls belong to which tab

static std::vector<Product> g_products;          // product list shown on the Kasir tab
static std::vector<Product> g_manageProducts;    // product list shown on the Data Barang tab
static std::vector<CartItem> g_cart;             // the shopping cart

// ---------- Small GUI helpers ----------
static HWND Ctl(int id) { return GetDlgItem(g_main, id); }

static std::string GetText(int id)
{
    char buffer[512];
    GetWindowTextA(Ctl(id), buffer, sizeof(buffer));
    return buffer;
}
static void SetText(int id, const std::string& text) { SetWindowTextA(Ctl(id), text.c_str()); }

static void ClearList(int id) { SendMessage(Ctl(id), LB_RESETCONTENT, 0, 0); }
static void AddLine(int id, const std::string& line) { SendMessageA(Ctl(id), LB_ADDSTRING, 0, (LPARAM)line.c_str()); }
static int  GetSelected(int id) { return (int)SendMessage(Ctl(id), LB_GETCURSEL, 0, 0); }

static void ShowError(const std::string& text) { MessageBoxA(g_main, text.c_str(), "Kesalahan", MB_ICONERROR); }
static void ShowInfo(const std::string& text)  { MessageBoxA(g_main, text.c_str(), "Informasi", MB_ICONINFORMATION); }

// Creates one control and remembers which tab it belongs to.
static HWND Make(int page, const char* className, const char* text, DWORD style,
                 int x, int y, int w, int h, int id)
{
    HWND control = CreateWindowExW(0, Utf8ToWide(className).c_str(), Utf8ToWide(text).c_str(),
                                   WS_CHILD | style, x, y, w, h, g_main, (HMENU)(INT_PTR)id,
                                   GetModuleHandle(NULL), NULL);
    SendMessage(control, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    g_pageControls[page].push_back(control);
    return control;
}

static HWND Label(int page, const char* text, int x, int y, int w, int h, int id = -1)
{
    return Make(page, "STATIC", text, SS_LEFT, x, y, w, h, id);
}
static HWND TextBox(int page, int x, int y, int w, int h, int id, DWORD extra = 0)
{
    return Make(page, "EDIT", "", WS_BORDER | ES_AUTOHSCROLL | extra, x, y, w, h, id);
}
static HWND Button(int page, const char* text, int x, int y, int w, int h, int id)
{
    return Make(page, "BUTTON", text, BS_PUSHBUTTON, x, y, w, h, id);
}
static HWND ListBox(int page, int x, int y, int w, int h, int id, int t1, int t2, int t3, int t4 = 0)
{
    HWND list = Make(page, "LISTBOX", "", WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_USETABSTOPS | LBS_NOINTEGRALHEIGHT,
                     x, y, w, h, id);
    int tabs[4] = { t1, t2, t3, t4 };                 // column positions (dialog units)
    SendMessage(list, LB_SETTABSTOPS, t4 > 0 ? 4 : 3, (LPARAM)tabs);
    return list;
}

// Show only the controls of the chosen tab and reload its data from MySQL.
static void RefreshCashierProducts();
static void RefreshCart();
static void RefreshManageProducts();
static void RefreshHistory();

static void ShowPage(int page)
{
    for (int p = 0; p < 4; p++) {                      // loop over the tabs
        for (size_t i = 0; i < g_pageControls[p].size(); i++)
            ShowWindow(g_pageControls[p][i], p == page ? SW_SHOW : SW_HIDE);
    }
    if (page == 0)      { RefreshCashierProducts(); RefreshCart(); }
    else if (page == 1) { RefreshManageProducts(); }
    else if (page == 2) { RefreshHistory(); }
}

// ================= TAB 1 : KASIR =================

static void ClearSelectedProduct()
{
    SetText(ID_C_NAME, ""); SetText(ID_C_PRICE, ""); SetText(ID_C_STOCK, "");
}

static void RefreshCashierProducts()
{
    if (!LoadProducts(Trim(GetText(ID_C_SEARCH)), g_products))
        ShowError("Gagal membaca data barang: " + LastDbError());
    ClearList(ID_C_PRODLIST);
    for (size_t i = 0; i < g_products.size(); i++) {
        const Product& p = g_products[i];
        AddLine(ID_C_PRODLIST, std::to_string(p.id) + "\t" + p.name + "\t" + FormatRupiah(p.price)
                               + "\t" + std::to_string(p.stock));
    }
    ClearSelectedProduct();
}

// Show name, price and stock of the product chosen in the list.
static void ShowSelectedProduct()
{
    int sel = GetSelected(ID_C_PRODLIST);
    if (sel < 0 || sel >= (int)g_products.size()) {
        ClearSelectedProduct();
        return;
    }
    SetText(ID_C_NAME, g_products[sel].name);
    SetText(ID_C_PRICE, FormatRupiah(g_products[sel].price));
    SetText(ID_C_STOCK, std::to_string(g_products[sel].stock));
}

// Show the payment/change text. Uses the same CheckPayment() as the real transaction.
static void UpdateChangeLabel()
{
    long long total = CalculateTotal(g_cart);
    long long payment = 0;
    if (!ParseNumber(GetText(ID_C_PAYMENT), payment)) {
        SetText(ID_C_CHANGE, "Kembalian : Rp0");
        return;
    }
    long long change = 0;
    int result = CheckPayment(total, payment, change);
    if (result == PAYMENT_LESS)
        SetText(ID_C_CHANGE, "Kembalian : - (pembayaran kurang)");
    else
        SetText(ID_C_CHANGE, "Kembalian : " + FormatRupiah(change));
}

// Redraw the cart list and the total price.
static void RefreshCart()
{
    ClearList(ID_C_CARTLIST);
    for (size_t i = 0; i < g_cart.size(); i++) {
        const CartItem& item = g_cart[i];
        AddLine(ID_C_CARTLIST, item.name + "\t" + FormatRupiah(item.price) + "\t"
                               + std::to_string(item.quantity) + "\t" + FormatRupiah(item.subtotal));
    }
    SetText(ID_C_TOTAL, "Total Belanja : " + FormatRupiah(CalculateTotal(g_cart)));
    UpdateChangeLabel();
}

static void OnAddToCart()
{
    // 1) which product is selected?
    int sel = GetSelected(ID_C_PRODLIST);
    if (sel < 0 || sel >= (int)g_products.size()) {
        ShowError("Barang tidak ditemukan. Pilih barang dari daftar terlebih dahulu.");
        return;
    }
    const Product& product = g_products[sel];

    // 2) read the quantity (bad input becomes 0)
    long long quantity = 0;
    if (!ParseNumber(GetText(ID_C_QTY), quantity))
        quantity = 0;

    // 3) how many of this product are already in the cart?
    int inCart = 0;
    for (size_t i = 0; i < g_cart.size(); i++) {
        if (g_cart[i].productId == product.id)
            inCart = g_cart[i].quantity;
    }

    // 4) decide what to do
    if (quantity <= 0) {
        ShowError("Jumlah tidak valid. Jumlah harus lebih dari 0.");
    }
    else if (quantity + inCart > product.stock) {
        ShowError("Stok tidak cukup.\nStok tersedia: " + std::to_string(product.stock)
                  + "\nSudah di keranjang: " + std::to_string(inCart));
    }
    else {
        bool found = false;
        for (size_t i = 0; i < g_cart.size(); i++) {      // same product already in cart? add to it
            if (g_cart[i].productId == product.id) {
                g_cart[i].quantity += (int)quantity;
                g_cart[i].subtotal = CalculateSubtotal(g_cart[i].price, g_cart[i].quantity);
                found = true;
            }
        }
        if (!found) {                                      // new line in the cart
            CartItem item;
            item.productId = product.id;
            item.name = product.name;
            item.price = product.price;
            item.quantity = (int)quantity;
            item.subtotal = CalculateSubtotal(item.price, item.quantity);   // Subtotal = Price x Quantity
            g_cart.push_back(item);
        }
        SetText(ID_C_QTY, "1");
        RefreshCart();
    }
}

static void OnRemoveFromCart()
{
    int sel = GetSelected(ID_C_CARTLIST);
    if (sel < 0 || sel >= (int)g_cart.size()) {
        ShowError("Pilih barang di keranjang terlebih dahulu.");
        return;
    }
    g_cart.erase(g_cart.begin() + sel);
    RefreshCart();
}

static void OnClearCart()
{
    g_cart.clear();
    RefreshCart();
}

// The "Proses Transaksi" button. Follows the 9 steps of the assignment.
static void OnProcess()
{
    // Step 1: validate the cart
    if (g_cart.empty()) {
        ShowError("Keranjang kosong. Tambahkan barang terlebih dahulu.");
        return;
    }

    // Step 2: check the stock again, using the real stock in MySQL
    for (size_t i = 0; i < g_cart.size(); i++) {
        Product current;
        if (!GetProductById(g_cart[i].productId, current)) {
            ShowError("Barang tidak ditemukan: " + g_cart[i].name);
            return;
        }
        else if (g_cart[i].quantity > current.stock) {
            ShowError("Stok tidak cukup untuk " + current.name + ".\nStok tersedia: " + std::to_string(current.stock));
            return;
        }
    }

    // Step 3: total (C++)
    long long total = CalculateTotal(g_cart);

    // Step 4: check the payment
    long long payment = 0;
    if (!ParseNumber(GetText(ID_C_PAYMENT), payment)) {
        ShowError("Pembayaran tidak valid. Isi dengan angka saja, contoh: 50000");
        return;
    }

    // Step 5: change (C++)
    long long change = 0;
    int result = CheckPayment(total, payment, change);
    if (result == PAYMENT_LESS) {
        ShowError("Pembayaran kurang.\nTotal: " + FormatRupiah(total) + "\nPembayaran: " + FormatRupiah(payment));
        return;                                           // do NOT save anything
    }
    else if (result == PAYMENT_EXACT) {
        change = 0;                                       // exact payment, no change
    }
    else {
        change = payment - total;                         // payment is more than total
    }

    // Steps 6, 7, 8: save transaction + items and reduce stock (in Transaction.cpp)
    std::string error;
    if (!SaveTransaction(g_cart, total, payment, change, error)) {
        ShowError("Transaksi gagal.\n" + error);
        RefreshCashierProducts();
        return;
    }

    // Step 9: success message
    ShowInfo("Total       : " + FormatRupiah(total) + "\nPembayaran  : " + FormatRupiah(payment)
             + "\nKembalian   : " + FormatRupiah(change) + "\n\nTransaksi berhasil!");

    g_cart.clear();
    SetText(ID_C_PAYMENT, "");
    RefreshCashierProducts();       // show the new (smaller) stock
    RefreshCart();
}

// ================= TAB 2 : DATA BARANG =================

static void ClearProductForm()
{
    SetText(ID_P_NAME, ""); SetText(ID_P_PRICE, ""); SetText(ID_P_STOCK, "");
    SendMessage(Ctl(ID_P_LIST), LB_SETCURSEL, (WPARAM)-1, 0);
}

static void RefreshManageProducts()
{
    if (!LoadProducts(Trim(GetText(ID_P_SEARCH)), g_manageProducts))
        ShowError("Gagal membaca data barang: " + LastDbError());
    ClearList(ID_P_LIST);
    for (size_t i = 0; i < g_manageProducts.size(); i++) {
        const Product& p = g_manageProducts[i];
        AddLine(ID_P_LIST, std::to_string(p.id) + "\t" + p.name + "\t" + FormatRupiah(p.price)
                           + "\t" + std::to_string(p.stock));
    }
}

static void ShowManageSelected()
{
    int sel = GetSelected(ID_P_LIST);
    if (sel < 0 || sel >= (int)g_manageProducts.size())
        return;
    SetText(ID_P_NAME, g_manageProducts[sel].name);
    SetText(ID_P_PRICE, std::to_string(g_manageProducts[sel].price));
    SetText(ID_P_STOCK, std::to_string(g_manageProducts[sel].stock));
}

// Reads the form. Bad numbers become -1 so ValidateProduct() reports them.
static void ReadProductForm(std::string& name, long long& price, long long& stock)
{
    name = Trim(GetText(ID_P_NAME));
    if (!ParseNumber(GetText(ID_P_PRICE), price)) price = -1;
    if (!ParseNumber(GetText(ID_P_STOCK), stock)) stock = -1;
}

static void OnAddProduct()
{
    std::string name, error;
    long long price, stock;
    ReadProductForm(name, price, stock);

    error = ValidateProduct(name, price, stock);
    if (error != "") { ShowError(error); return; }

    if (AddProduct(name, price, stock, error)) {
        ShowInfo("Barang berhasil ditambahkan.");
        ClearProductForm();
        RefreshManageProducts();
    } else {
        ShowError(error);
    }
}

static void OnEditProduct()
{
    int sel = GetSelected(ID_P_LIST);
    if (sel < 0 || sel >= (int)g_manageProducts.size()) {
        ShowError("Barang tidak ditemukan. Pilih barang di daftar terlebih dahulu.");
        return;
    }
    std::string name, error;
    long long price, stock;
    ReadProductForm(name, price, stock);

    error = ValidateProduct(name, price, stock);
    if (error != "") { ShowError(error); return; }

    if (EditProduct(g_manageProducts[sel].id, name, price, stock, error)) {
        ShowInfo("Data barang berhasil diubah.");
        ClearProductForm();
        RefreshManageProducts();
    } else {
        ShowError(error);
    }
}

static void OnUpdateStock()
{
    int sel = GetSelected(ID_P_LIST);
    if (sel < 0 || sel >= (int)g_manageProducts.size()) {
        ShowError("Barang tidak ditemukan. Pilih barang di daftar terlebih dahulu.");
        return;
    }
    long long stock = -1;
    if (!ParseNumber(GetText(ID_P_STOCK), stock)) stock = -1;

    if (stock < 0) {
        ShowError("Stok tidak valid. Isi dengan angka 0 atau lebih.");
        return;
    }
    std::string error;
    if (UpdateStock(g_manageProducts[sel].id, stock, error)) {
        ShowInfo("Stok berhasil diubah.");
        RefreshManageProducts();
    } else {
        ShowError(error);
    }
}

static void OnDeleteProduct()
{
    int sel = GetSelected(ID_P_LIST);
    if (sel < 0 || sel >= (int)g_manageProducts.size()) {
        ShowError("Barang tidak ditemukan. Pilih barang di daftar terlebih dahulu.");
        return;
    }
    std::string question = "Hapus barang \"" + g_manageProducts[sel].name + "\"?";
    if (MessageBoxA(g_main, question.c_str(), "Hapus Barang", MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    std::string error;
    if (DeleteProduct(g_manageProducts[sel].id, error)) {
        ClearProductForm();
        RefreshManageProducts();
    } else {
        ShowError(error);
    }
}

// ================= TAB 3 : RIWAYAT & TERLARIS =================

static void RefreshHistory()
{
    // best-selling product (SQL query in Transaction.cpp)
    std::string bestName;
    int bestSold = 0;
    if (GetBestSelling(bestName, bestSold))
        SetText(ID_H_BEST, "Barang Terlaris:\r\n" + bestName + "\r\n\r\nTotal Terjual:\r\n"
                           + std::to_string(bestSold) + " item");
    else
        SetText(ID_H_BEST, "Barang Terlaris:\r\nBelum ada penjualan");

    // sales history
    std::vector<HistoryRow> history;
    if (!LoadHistory(history))
        ShowError("Gagal membaca riwayat: " + LastDbError());
    ClearList(ID_H_LIST);
    for (size_t i = 0; i < history.size(); i++) {
        const HistoryRow& h = history[i];
        AddLine(ID_H_LIST, std::to_string(h.id) + "\t" + h.date + "\t" + FormatRupiah(h.total) + "\t"
                           + FormatRupiah(h.payment) + "\t" + FormatRupiah(h.change));
    }
}

// ================= TAB 4 : GEMINI =================

static void OnGemini()
{
    std::string prompt = BuildSalesPrompt();               // sales numbers from MySQL
    if (prompt == "") {
        ShowInfo("Belum ada data penjualan untuk dianalisis.");
        return;
    }
    SetWindowTextW(Ctl(ID_G_OUTPUT), L"Mengirim data ke Gemini AI, mohon tunggu...");
    UpdateWindow(Ctl(ID_G_OUTPUT));

    std::wstring answer;
    std::string error;
    if (AskGemini(prompt, answer, error)) {
        SetWindowTextW(Ctl(ID_G_OUTPUT), answer.c_str());
    } else {
        SetWindowTextW(Ctl(ID_G_OUTPUT), L"");
        ShowError("Gemini API gagal.\n\n" + error);
    }
}

// ================= WINDOW =================

static void CreateControls(HWND hwnd)
{
    // ----- tab bar -----
    g_tab = CreateWindowExA(0, WC_TABCONTROLA, "", WS_CHILD | WS_VISIBLE, 0, 0, 900, 28,
                            hwnd, (HMENU)(INT_PTR)ID_TAB, GetModuleHandle(NULL), NULL);
    SendMessage(g_tab, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    const char* tabNames[4] = { "Kasir", "Data Barang", "Riwayat & Terlaris", "Analisis Gemini AI" };
    for (int i = 0; i < 4; i++) {
        TCITEMA item;
        ZeroMemory(&item, sizeof(item));
        item.mask = TCIF_TEXT;
        item.pszText = (LPSTR)tabNames[i];
        SendMessageA(g_tab, TCM_INSERTITEMA, i, (LPARAM)&item);
    }

    // ----- tab 1: Kasir (page 0) -----
    Label(0, "Cari Barang:", 10, 45, 80, 20);
    TextBox(0, 95, 42, 220, 24, ID_C_SEARCH);
    Label(0, "Daftar barang  (ID | Nama | Harga | Stok)", 10, 72, 400, 18);
    ListBox(0, 10, 92, 420, 235, ID_C_PRODLIST, 22, 105, 160);
    Label(0, "Nama:", 10, 340, 50, 20);
    TextBox(0, 70, 337, 360, 24, ID_C_NAME, ES_READONLY);
    Label(0, "Harga:", 10, 372, 50, 20);
    TextBox(0, 70, 369, 150, 24, ID_C_PRICE, ES_READONLY);
    Label(0, "Stok:", 240, 372, 40, 20);
    TextBox(0, 285, 369, 80, 24, ID_C_STOCK, ES_READONLY);
    Label(0, "Jumlah:", 10, 406, 55, 20);
    HWND qty = TextBox(0, 70, 403, 80, 24, ID_C_QTY, ES_NUMBER);
    SetWindowTextA(qty, "1");
    Button(0, "Tambah ke Keranjang", 165, 400, 200, 30, ID_C_ADD);

    Label(0, "Keranjang  (Barang | Harga | Jumlah | Subtotal)", 450, 45, 430, 18);
    ListBox(0, 450, 66, 430, 260, ID_C_CARTLIST, 100, 160, 205);
    Button(0, "Hapus dari Keranjang", 450, 334, 200, 28, ID_C_REMOVE);
    Button(0, "Kosongkan Keranjang", 660, 334, 200, 28, ID_C_CLEAR);
    Label(0, "Total Belanja : Rp0", 450, 380, 430, 22, ID_C_TOTAL);
    Label(0, "Pembayaran (Rp):", 450, 412, 110, 20);
    TextBox(0, 565, 409, 160, 24, ID_C_PAYMENT);
    Label(0, "Kembalian : Rp0", 450, 445, 430, 22, ID_C_CHANGE);
    Button(0, "Proses Transaksi", 450, 485, 430, 40, ID_C_PROCESS);

    // ----- tab 2: Data Barang (page 1) -----
    Label(1, "Cari Barang:", 10, 45, 80, 20);
    TextBox(1, 95, 42, 250, 24, ID_P_SEARCH);
    Label(1, "Daftar barang  (ID | Nama | Harga | Stok)", 10, 72, 400, 18);
    ListBox(1, 10, 92, 520, 430, ID_P_LIST, 22, 140, 200);
    Label(1, "Nama Barang:", 550, 95, 90, 20);
    TextBox(1, 645, 92, 235, 24, ID_P_NAME);
    Label(1, "Harga (Rp):", 550, 130, 90, 20);
    TextBox(1, 645, 127, 235, 24, ID_P_PRICE);
    Label(1, "Stok:", 550, 165, 90, 20);
    TextBox(1, 645, 162, 235, 24, ID_P_STOCK);
    Button(1, "Tambah Barang", 550, 210, 160, 30, ID_P_ADD);
    Button(1, "Edit Barang", 720, 210, 160, 30, ID_P_EDIT);
    Button(1, "Hapus Barang", 550, 250, 160, 30, ID_P_DELETE);
    Button(1, "Ubah Stok Saja", 720, 250, 160, 30, ID_P_STOCKBTN);
    Button(1, "Bersihkan Form", 550, 290, 160, 30, ID_P_CLEAR);
    Label(1, "Klik barang di daftar, ubah isian, lalu klik Edit Barang\r\natau Ubah Stok Saja.", 550, 340, 330, 40);

    // ----- tab 3: Riwayat & Terlaris (page 2) -----
    Button(2, "Muat Ulang", 10, 42, 120, 28, ID_H_REFRESH);
    Label(2, "", 10, 80, 400, 90, ID_H_BEST);
    Label(2, "Riwayat penjualan  (ID | Tanggal | Total | Pembayaran | Kembalian)", 10, 180, 600, 18);
    ListBox(2, 10, 200, 870, 330, ID_H_LIST, 30, 130, 230, 330);

    // ----- tab 4: Gemini (page 3) -----
    Button(3, "Analisis Penjualan dengan Gemini AI", 10, 42, 330, 32, ID_G_BUTTON);
    Label(3, "Data penjualan dikirim ke Gemini hanya untuk analisis. Perhitungan tetap dilakukan C++.", 10, 82, 870, 20);
    Make(3, "EDIT", "", WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
         10, 108, 870, 420, ID_G_OUTPUT);

    g_ready = true;
    ShowPage(0);
}

// Runs when a button is clicked or a text box / list changes.
static void HandleCommand(int id, int code)
{
    if (!g_ready)
        return;

    if (id == ID_C_SEARCH && code == EN_CHANGE)          RefreshCashierProducts();
    else if (id == ID_C_PRODLIST && code == LBN_SELCHANGE) ShowSelectedProduct();
    else if (id == ID_C_ADD)                             OnAddToCart();
    else if (id == ID_C_REMOVE)                          OnRemoveFromCart();
    else if (id == ID_C_CLEAR)                           OnClearCart();
    else if (id == ID_C_PAYMENT && code == EN_CHANGE)    UpdateChangeLabel();
    else if (id == ID_C_PROCESS)                         OnProcess();

    else if (id == ID_P_SEARCH && code == EN_CHANGE)     RefreshManageProducts();
    else if (id == ID_P_LIST && code == LBN_SELCHANGE)   ShowManageSelected();
    else if (id == ID_P_ADD)                             OnAddProduct();
    else if (id == ID_P_EDIT)                            OnEditProduct();
    else if (id == ID_P_DELETE)                          OnDeleteProduct();
    else if (id == ID_P_STOCKBTN)                        OnUpdateStock();
    else if (id == ID_P_CLEAR)                           ClearProductForm();

    else if (id == ID_H_REFRESH)                         RefreshHistory();
    else if (id == ID_G_BUTTON)                          OnGemini();
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_CREATE) {
        g_main = hwnd;
        CreateControls(hwnd);
        return 0;
    }
    else if (message == WM_NOTIFY) {                     
        NMHDR* header = (NMHDR*)lParam;
        if (g_ready && header->idFrom == ID_TAB && header->code == TCN_SELCHANGE)
            ShowPage(TabCtrl_GetCurSel(g_tab));
        return 0;
    }
    else if (message == WM_COMMAND) {
        HandleCommand(LOWORD(wParam), HIWORD(wParam));
        return 0;
    }
    else if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, message, wParam, lParam);
}

bool CreateMainWindow(HINSTANCE hInstance, int nCmdShow)
{
    WNDCLASSEXA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "CashierWindow";
    if (!RegisterClassExA(&wc))
        return false;

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT area = { 0, 0, 900, 545 };                       
    AdjustWindowRect(&area, style, FALSE);

    HWND window = CreateWindowExA(0, "CashierWindow", "Aplikasi Kasir", style, CW_USEDEFAULT, CW_USEDEFAULT,
                                  area.right - area.left, area.bottom - area.top, NULL, NULL, hInstance, NULL);
    if (window == NULL)
        return false;
    ShowWindow(window, nCmdShow);
    UpdateWindow(window);
    return true;
}
