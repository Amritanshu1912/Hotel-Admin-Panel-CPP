/*
 * ================================================================
 *  Hotel Admin Panel
 *  A console-based C++ application for hotel administrators
 *
 *  Author   : Amritanshu Singh
 *  Language : C++ (Standard: C++17)
 *  Compiler : g++ -std=c++17 hotel_admin_panel.cpp -o hotel_admin
 *
 *  Modules:
 *    1. Room & Guest Management
 *       - Book a room, view booking, edit booking
 *       - Check out a guest (prints final bill)
 *       - List all occupied rooms
 *       - Check room availability
 *
 *    2. Staff Management
 *       - Add / view / edit / delete hotel staff
 *       - List all staff with department summary
 *       - Generate salary slip (supports salaried + daily-wage staff)
 *
 *  Data storage: binary flat files (one per module)
 *    hotel_bookings.dat  — room booking records
 *    hotel_staff.dat     — staff records
 *
 *  Design principles:
 *    - Input validation centralised in reusable helpers
 *    - No platform-specific headers (compiles on Windows/Linux/Mac)
 *    - Deletion uses temp-file swap to keep binary file contiguous
 *    - All monetary values in INR
 * ================================================================
 */

#include <algorithm>
#include <cctype>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace std;

// ================================================================
//  CONSTANTS & CONFIGURATION
// ================================================================

const string BOOKINGS_FILE   = "hotel_bookings.dat";
const string STAFF_FILE      = "hotel_staff.dat";
const string TEMP_FILE       = "hotel_temp.dat";

const float  RATE_PER_NIGHT  = 900.0f;   // default room rate (INR/night)
const float  MAX_LOAN        = 50000.0f;
const float  MAX_BASIC       = 50000.0f;
const int    MAX_ROOMS       = 100;      // hotel capacity

// Hotel staff roles — predefined to keep data consistent
const vector<string> STAFF_ROLES = {
    "Manager",
    "Receptionist",
    "Housekeeping",
    "Security",
    "Kitchen Staff",
    "Daily Wage"
};

// Grade determines salary structure
// A = Manager, B = Receptionist, C = Housekeeping/Security,
// D = Kitchen Staff, E = Daily Wage (paid per day, no fixed basic)
const vector<char> STAFF_GRADES = {'A', 'B', 'C', 'D', 'E'};


// ================================================================
//  UTILITY / INPUT HELPERS
// ================================================================

void clearScreen()
{
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void printDivider(int width = 60, char ch = '-')
{
    cout << string(width, ch) << "\n";
}

void printHeading(const string& title)
{
    cout << "\n";
    printDivider(60, '=');
    cout << "  " << title << "\n";
    printDivider(60, '=');
    cout << "\n";
}

// Consume leftover newline in the buffer — needed after cin >> N
void flushInput()
{
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

void pressEnterToContinue()
{
    cout << "\n  Press Enter to continue...";
    flushInput();
}

// Read an integer in [minVal, maxVal], re-prompts on bad input
int readInt(const string& prompt, int minVal, int maxVal)
{
    int value;
    while (true)
    {
        cout << prompt;
        if (cin >> value && value >= minVal && value <= maxVal)
        {
            flushInput();
            return value;
        }
        cin.clear();
        flushInput();
        cout << "  [!] Enter a number between " << minVal
             << " and " << maxVal << ".\n";
    }
}

// Read a float in [minVal, maxVal]
float readFloat(const string& prompt, float minVal, float maxVal)
{
    float value;
    while (true)
    {
        cout << prompt;
        if (cin >> value && value >= minVal && value <= maxVal)
        {
            flushInput();
            return value;
        }
        cin.clear();
        flushInput();
        cout << "  [!] Enter a value between " << minVal
             << " and " << maxVal << ".\n";
    }
}

// Read a non-empty string up to maxLen characters
string readString(const string& prompt, int maxLen = 50)
{
    string value;
    while (true)
    {
        cout << prompt;
        getline(cin, value);
        // Trim leading whitespace
        value.erase(0, value.find_first_not_of(" \t"));
        if (value.empty())
            cout << "  [!] Input cannot be empty.\n";
        else if ((int)value.length() > maxLen)
        {
            cout << "  [!] Too long (max " << maxLen << " chars).\n";
            value.clear();
        }
        else
            return value;
    }
}

// Read a single char from a set of valid chars (case-insensitive)
char readChar(const string& prompt, const string& validChars)
{
    string upper = validChars;
    for (char& c : upper) c = toupper(c);

    while (true)
    {
        cout << prompt;
        string input;
        getline(cin, input);
        if (!input.empty())
        {
            char ch = toupper(input[0]);
            if (upper.find(ch) != string::npos)
                return ch;
        }
        cout << "  [!] Enter one of: " << validChars << "\n";
    }
}

// Prompt with existing value shown; return existing if user just hits Enter
string readOptional(const string& prompt, const string& current, int maxLen = 50)
{
    cout << prompt << " [" << current << "]: ";
    string input;
    getline(cin, input);
    input.erase(0, input.find_first_not_of(" \t"));
    if (input.empty()) return current;
    if ((int)input.length() > maxLen)
    {
        cout << "  [!] Too long, keeping current value.\n";
        return current;
    }
    return input;
}

// Validate a calendar date (handles leap years)
bool isValidDate(int day, int month, int year)
{
    if (month < 1 || month > 12) return false;
    if (day < 1 || day > 31)     return false;
    int dim[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    if (leap) dim[1] = 29;
    return day <= dim[month - 1];
}

void getTodayDate(int& day, int& month, int& year)
{
    time_t now = time(nullptr);
    tm*    lt  = localtime(&now);
    day   = lt->tm_mday;
    month = 1 + lt->tm_mon;
    year  = 1900 + lt->tm_year;
}

string monthName(int m)
{
    const string names[] = {
        "January","February","March","April","May","June",
        "July","August","September","October","November","December"
    };
    return (m >= 1 && m <= 12) ? names[m - 1] : "Unknown";
}


// ================================================================
//  MODULE 1 — ROOM & GUEST MANAGEMENT
// ================================================================

/*
 * RoomBooking stores one guest's reservation.
 * Fixed-length char arrays keep sizeof() stable for binary I/O.
 */
struct RoomBooking
{
    int   roomNumber;
    char  guestName[40];
    char  address[60];
    char  phone[16];
    int   checkInDay;
    int   checkInMonth;
    int   checkInYear;
    int   nights;           // duration of stay
    float totalFare;        // nights × RATE_PER_NIGHT
};

class RoomManager
{
public:
    void run();

private:
    // Core CRUD
    void bookRoom();
    void viewBooking();
    void editBooking();
    void checkOutGuest();   // removes record + prints final bill
    void listOccupied();
    void checkAvailability();

    // Internal helpers
    bool   isRoomOccupied(int roomNo) const;
    void   printBooking(const RoomBooking& b, bool detailed = true) const;
};

// ── Helpers ──────────────────────────────────────────────────────

bool RoomManager::isRoomOccupied(int roomNo) const
{
    ifstream file(BOOKINGS_FILE, ios::binary);
    if (!file) return false;
    RoomBooking rec;
    while (file.read(reinterpret_cast<char*>(&rec), sizeof(RoomBooking)))
        if (rec.roomNumber == roomNo) return true;
    return false;
}

void RoomManager::printBooking(const RoomBooking& b, bool detailed) const
{
    if (detailed)
    {
        printDivider(50);
        cout << "  Room No.     : " << b.roomNumber << "\n"
             << "  Guest        : " << b.guestName  << "\n"
             << "  Address      : " << b.address    << "\n"
             << "  Phone        : " << b.phone      << "\n"
             << "  Check-in     : " << b.checkInDay << "/"
                                    << b.checkInMonth << "/"
                                    << b.checkInYear  << "\n"
             << "  Nights       : " << b.nights     << "\n"
             << "  Total Fare   : INR " << fixed
                                        << setprecision(2)
                                        << b.totalFare << "\n";
        printDivider(50);
    }
    else
    {
        // Compact one-line summary for the list view
        cout << left
             << setw(8)  << b.roomNumber
             << setw(22) << b.guestName
             << setw(12) << b.phone
             << setw(8)  << b.nights
             << "INR " << fixed << setprecision(0) << b.totalFare
             << "\n";
    }
}

// ── Book a Room ──────────────────────────────────────────────────

void RoomManager::bookRoom()
{
    clearScreen();
    printHeading("Book a Room");

    int roomNo = readInt("  Room number (1-" + to_string(MAX_ROOMS) + "): ",
                         1, MAX_ROOMS);

    if (isRoomOccupied(roomNo))
    {
        cout << "  [!] Room " << roomNo
             << " is already occupied. Use 'Check Availability' to see free rooms.\n";
        pressEnterToContinue();
        return;
    }

    RoomBooking booking{};
    booking.roomNumber = roomNo;

    string name = readString("  Guest name    : ", 39);
    string addr = readString("  Address       : ", 59);
    string phone = readString("  Phone         : ", 15);

    strncpy(booking.guestName, name.c_str(),  sizeof(booking.guestName)  - 1);
    strncpy(booking.address,   addr.c_str(),  sizeof(booking.address)    - 1);
    strncpy(booking.phone,     phone.c_str(), sizeof(booking.phone)      - 1);

    // Check-in date
    cout << "\n  Check-in date:\n";
    bool validDate = false;
    do {
        booking.checkInDay   = readInt("    Day   (1-31) : ", 1, 31);
        booking.checkInMonth = readInt("    Month (1-12) : ", 1, 12);
        booking.checkInYear  = readInt("    Year         : ", 2000, 2099);
        validDate = isValidDate(booking.checkInDay,
                                booking.checkInMonth,
                                booking.checkInYear);
        if (!validDate)
            cout << "  [!] Invalid date. Try again.\n";
    } while (!validDate);

    booking.nights    = readInt("\n  Number of nights: ", 1, 365);
    booking.totalFare = booking.nights * RATE_PER_NIGHT;

    ofstream outFile(BOOKINGS_FILE, ios::binary | ios::app);
    outFile.write(reinterpret_cast<const char*>(&booking), sizeof(RoomBooking));

    cout << "\n  [✓] Room " << roomNo << " booked for " << name << ".\n"
         << "      Nights : " << booking.nights << "\n"
         << "      Fare   : INR " << fixed << setprecision(2)
                                  << booking.totalFare << "\n";
    pressEnterToContinue();
}

// ── View a Booking ────────────────────────────────────────────────

void RoomManager::viewBooking()
{
    clearScreen();
    printHeading("View Booking");

    int roomNo = readInt("  Enter room number: ", 1, MAX_ROOMS);

    ifstream file(BOOKINGS_FILE, ios::binary);
    if (!file)
    {
        cout << "  [!] No booking records found.\n";
        pressEnterToContinue();
        return;
    }

    RoomBooking rec;
    bool found = false;
    while (file.read(reinterpret_cast<char*>(&rec), sizeof(RoomBooking)))
    {
        if (rec.roomNumber == roomNo)
        {
            printBooking(rec);
            found = true;
            break;
        }
    }

    if (!found)
        cout << "  [!] Room " << roomNo << " is not currently booked.\n";

    pressEnterToContinue();
}

// ── Edit a Booking ────────────────────────────────────────────────

void RoomManager::editBooking()
{
    clearScreen();
    printHeading("Edit Booking");

    int roomNo = readInt("  Enter room number to edit: ", 1, MAX_ROOMS);

    fstream file(BOOKINGS_FILE, ios::binary | ios::in | ios::out);
    if (!file)
    {
        cout << "  [!] No records found.\n";
        pressEnterToContinue();
        return;
    }

    RoomBooking rec;
    bool found = false;
    streampos pos;

    while (true)
    {
        pos = file.tellg();
        if (!file.read(reinterpret_cast<char*>(&rec), sizeof(RoomBooking))) break;
        if (rec.roomNumber == roomNo) { found = true; break; }
    }

    if (!found)
    {
        cout << "  [!] Room " << roomNo << " is not booked.\n";
        pressEnterToContinue();
        return;
    }

    cout << "  Current record:\n";
    printBooking(rec);
    cout << "  (Press Enter to keep current value)\n\n";

    string newName  = readOptional("  Guest name",  rec.guestName, 39);
    string newAddr  = readOptional("  Address",     rec.address,   59);
    string newPhone = readOptional("  Phone",       rec.phone,     15);

    strncpy(rec.guestName, newName.c_str(),  sizeof(rec.guestName)  - 1);
    strncpy(rec.address,   newAddr.c_str(),  sizeof(rec.address)    - 1);
    strncpy(rec.phone,     newPhone.c_str(), sizeof(rec.phone)      - 1);

    cout << "  Nights [" << rec.nights << "]: ";
    string nightsInput;
    getline(cin, nightsInput);
    if (!nightsInput.empty())
    {
        int n = stoi(nightsInput);
        if (n >= 1 && n <= 365)
        {
            rec.nights    = n;
            rec.totalFare = n * RATE_PER_NIGHT;
        }
    }

    file.seekp(pos);
    file.write(reinterpret_cast<const char*>(&rec), sizeof(RoomBooking));

    cout << "\n  [✓] Booking updated.\n";
    pressEnterToContinue();
}

// ── Check Out a Guest ─────────────────────────────────────────────
/*
 * Prints a final bill, then removes the record from the file.
 * The temp-file swap ensures the binary file stays contiguous.
 */
void RoomManager::checkOutGuest()
{
    clearScreen();
    printHeading("Guest Check-Out");

    int roomNo = readInt("  Enter room number to check out: ", 1, MAX_ROOMS);

    ifstream inFile(BOOKINGS_FILE, ios::binary);
    if (!inFile)
    {
        cout << "  [!] No booking records found.\n";
        pressEnterToContinue();
        return;
    }

    ofstream tempFile(TEMP_FILE, ios::binary);
    RoomBooking rec;
    bool found = false;

    while (inFile.read(reinterpret_cast<char*>(&rec), sizeof(RoomBooking)))
    {
        if (rec.roomNumber == roomNo)
        {
            found = true;

            // Print the final bill before removing
            int today, todayM, todayY;
            getTodayDate(today, todayM, todayY);

            printDivider(50, '=');
            cout << "              FINAL BILL\n";
            printDivider(50, '=');
            cout << "  Hotel         : Hotel Admin Panel\n"
                 << "  Check-out Date: " << today << "/"
                                         << todayM << "/"
                                         << todayY  << "\n";
            printDivider(50);
            cout << "  Room No.      : " << rec.roomNumber << "\n"
                 << "  Guest         : " << rec.guestName  << "\n"
                 << "  Check-in      : " << rec.checkInDay << "/"
                                         << rec.checkInMonth << "/"
                                         << rec.checkInYear  << "\n"
                 << "  Nights stayed : " << rec.nights    << "\n"
                 << "  Rate/night    : INR " << fixed
                                             << setprecision(2)
                                             << RATE_PER_NIGHT << "\n";
            printDivider(50);
            cout << "  TOTAL AMOUNT  : INR " << rec.totalFare << "\n";
            printDivider(50, '=');

            char confirm = readChar("\n  Confirm check-out? (Y/N): ", "YN");
            if (confirm == 'N')
            {
                // Guest is staying — write record back
                tempFile.write(reinterpret_cast<const char*>(&rec),
                               sizeof(RoomBooking));
                cout << "  Check-out cancelled.\n";
            }
            else
            {
                cout << "\n  [✓] " << rec.guestName
                     << " has checked out. Room " << roomNo
                     << " is now available.\n";
                // Record is NOT written to temp → effectively deleted
            }
        }
        else
        {
            tempFile.write(reinterpret_cast<const char*>(&rec),
                           sizeof(RoomBooking));
        }
    }

    inFile.close();
    tempFile.close();

    if (!found)
        cout << "  [!] Room " << roomNo << " is not currently booked.\n";
    else
    {
        remove(BOOKINGS_FILE.c_str());
        rename(TEMP_FILE.c_str(), BOOKINGS_FILE.c_str());
    }

    pressEnterToContinue();
}

// ── List All Occupied Rooms ───────────────────────────────────────

void RoomManager::listOccupied()
{
    clearScreen();
    printHeading("All Occupied Rooms");

    ifstream file(BOOKINGS_FILE, ios::binary);
    if (!file)
    {
        cout << "  No bookings on record.\n";
        pressEnterToContinue();
        return;
    }

    cout << left
         << setw(8)  << "Room"
         << setw(22) << "Guest Name"
         << setw(12) << "Phone"
         << setw(8)  << "Nights"
         << "Total Fare\n";
    printDivider(60);

    RoomBooking rec;
    int count = 0;
    float totalRevenue = 0.0f;

    while (file.read(reinterpret_cast<char*>(&rec), sizeof(RoomBooking)))
    {
        printBooking(rec, false);   // compact one-line format
        totalRevenue += rec.totalFare;
        ++count;
    }

    printDivider(60);

    if (count == 0)
        cout << "  No rooms currently occupied.\n";
    else
        cout << "  Occupied rooms : " << count << " / " << MAX_ROOMS << "\n"
             << "  Total revenue  : INR " << fixed << setprecision(2)
                                          << totalRevenue << "\n";

    pressEnterToContinue();
}

// ── Check Room Availability ───────────────────────────────────────
/*
 * Collects all occupied room numbers, then lists which rooms
 * in 1..MAX_ROOMS are still free.
 */
void RoomManager::checkAvailability()
{
    clearScreen();
    printHeading("Room Availability");

    // Load all occupied room numbers into a vector
    vector<int> occupied;
    ifstream file(BOOKINGS_FILE, ios::binary);
    if (file)
    {
        RoomBooking rec;
        while (file.read(reinterpret_cast<char*>(&rec), sizeof(RoomBooking)))
            occupied.push_back(rec.roomNumber);
    }

    auto isOccupied = [&](int n) {
        return find(occupied.begin(), occupied.end(), n) != occupied.end();
    };

    int freeCount = 0;
    cout << "  Available rooms:\n  ";
    for (int r = 1; r <= MAX_ROOMS; ++r)
    {
        if (!isOccupied(r))
        {
            cout << setw(4) << r;
            ++freeCount;
            if (freeCount % 15 == 0) cout << "\n  ";  // wrap every 15
        }
    }
    cout << "\n\n";
    printDivider(60);
    cout << "  Free     : " << freeCount << "\n"
         << "  Occupied : " << (int)occupied.size() << "\n"
         << "  Total    : " << MAX_ROOMS << "\n";

    pressEnterToContinue();
}

// ── Room Manager Menu ─────────────────────────────────────────────

void RoomManager::run()
{
    int choice = 0;
    while (choice != 7)
    {
        clearScreen();
        printHeading("Room & Guest Management");
        cout << "  1. Book a Room\n"
             << "  2. View Booking\n"
             << "  3. Edit Booking\n"
             << "  4. Check Out Guest\n"
             << "  5. List Occupied Rooms\n"
             << "  6. Check Room Availability\n"
             << "  7. Back\n\n";

        choice = readInt("  Your choice: ", 1, 7);

        switch (choice)
        {
            case 1: bookRoom();          break;
            case 2: viewBooking();       break;
            case 3: editBooking();       break;
            case 4: checkOutGuest();     break;
            case 5: listOccupied();      break;
            case 6: checkAvailability(); break;
            case 7: break;
        }
    }
}


// ================================================================
//  MODULE 2 — HOTEL STAFF MANAGEMENT
// ================================================================

/*
 * Staff grade → salary structure mapping:
 *   A (Manager)       — salaried, all allowances eligible
 *   B (Receptionist)  — salaried, all allowances eligible
 *   C (Housekeeping)  — salaried, all allowances eligible
 *   D (Security /     — salaried, all allowances eligible
 *      Kitchen Staff)
 *   E (Daily Wage)    — paid per day worked; overtime eligible;
 *                       no HRA/CA/DA/PF (no fixed basic)
 *
 * Salary slip components (grades A–D):
 *   HRA = 5% of basic  (if house allowance granted)
 *   CA  = 2% of basic  (if conveyance allowance granted)
 *   DA  = 5% of basic  (dearness allowance — always applied)
 *   PF  = 2% of basic  (provident fund — always deducted)
 *   LD  = 15% of loan  (loan deduction — if loan > 0)
 *   Net = basic + HRA + CA + DA − PF − LD
 *
 * Salary slip components (grade E):
 *   Basic = daysWorked × 30
 *   OT    = overtimeHours × 10
 *   LD    = 15% of loan
 *   Net   = basic + OT − LD
 */
struct StaffMember
{
    int   id;               // auto-incremented, never reused
    char  name[40];
    char  phone[16];
    char  address[60];
    int   joiningDay;
    int   joiningMonth;
    int   joiningYear;
    char  role[20];         // one of STAFF_ROLES
    char  grade;            // 'A'..'E' — drives salary logic
    char  hasHouseAllowance;      // 'Y' / 'N'
    char  hasConveyanceAllowance; // 'Y' / 'N'
    float basicSalary;      // 0 for grade E
    float loanAmount;
};

class StaffManager
{
public:
    void run();

private:
    void addStaff();
    void viewStaff();
    void editStaff();
    void deleteStaff();
    void listStaff();
    void departmentSummary();
    void salarySlip();

    // Internal helpers
    int  getLastId() const;
    bool idExists(int id) const;
    bool findById(int id, StaffMember& out, streampos* pos = nullptr) const;
    void printStaff(const StaffMember& s, bool compact = false) const;
    void printSalarySlip(const StaffMember& s) const;
    void selectRole(char roleOut[20]) const;
};

// ── Internal Helpers ─────────────────────────────────────────────

int StaffManager::getLastId() const
{
    ifstream file(STAFF_FILE, ios::binary);
    if (!file) return 0;
    StaffMember s;
    int last = 0;
    while (file.read(reinterpret_cast<char*>(&s), sizeof(StaffMember)))
        last = s.id;
    return last;
}

bool StaffManager::idExists(int id) const
{
    StaffMember s;
    return findById(id, s);
}

bool StaffManager::findById(int id, StaffMember& out, streampos* pos) const
{
    // We open as binary in — the caller handles write mode separately
    ifstream file(STAFF_FILE, ios::binary);
    if (!file) return false;
    while (true)
    {
        streampos p = file.tellg();
        if (!file.read(reinterpret_cast<char*>(&out), sizeof(StaffMember))) break;
        if (out.id == id)
        {
            if (pos) *pos = p;
            return true;
        }
    }
    return false;
}

void StaffManager::printStaff(const StaffMember& s, bool compact) const
{
    if (compact)
    {
        cout << left
             << setw(6)  << s.id
             << setw(22) << s.name
             << setw(18) << s.role
             << setw(6)  << s.grade;
        if (s.grade != 'E')
            cout << "INR " << fixed << setprecision(0) << s.basicSalary;
        else
            cout << "Daily";
        cout << "\n";
        return;
    }

    printDivider(55);
    cout << "  ID           : " << s.id        << "\n"
         << "  Name         : " << s.name      << "\n"
         << "  Phone        : " << s.phone     << "\n"
         << "  Address      : " << s.address   << "\n"
         << "  Joining Date : " << s.joiningDay   << "/"
                                << s.joiningMonth << "/"
                                << s.joiningYear  << "\n"
         << "  Role         : " << s.role      << "\n"
         << "  Grade        : " << s.grade     << "\n";
    if (s.grade != 'E')
    {
        cout << "  House Allow. : " << s.hasHouseAllowance      << "\n"
             << "  Conv. Allow. : " << s.hasConveyanceAllowance << "\n"
             << "  Basic Salary : INR " << fixed << setprecision(2)
                                        << s.basicSalary        << "\n";
    }
    cout << "  Loan         : INR " << fixed << setprecision(2)
                                    << s.loanAmount             << "\n";
    printDivider(55);
}

// Let the admin pick from the predefined hotel roles
void StaffManager::selectRole(char roleOut[20]) const
{
    cout << "\n  Select role:\n";
    for (int i = 0; i < (int)STAFF_ROLES.size(); ++i)
        cout << "    " << (i + 1) << ". " << STAFF_ROLES[i] << "\n";

    int pick = readInt("  Choice: ", 1, (int)STAFF_ROLES.size());
    strncpy(roleOut, STAFF_ROLES[pick - 1].c_str(), 19);
    roleOut[19] = '\0';
}

// ── Add Staff ─────────────────────────────────────────────────────

void StaffManager::addStaff()
{
    clearScreen();
    printHeading("Add New Staff Member");

    StaffMember s{};
    s.id = getLastId() + 1;
    cout << "  Auto-assigned ID: " << s.id << "\n\n";

    string name  = readString("  Name        : ", 39);
    string phone = readString("  Phone       : ", 15);
    string addr  = readString("  Address     : ", 59);

    strncpy(s.name,    name.c_str(),  sizeof(s.name)    - 1);
    strncpy(s.phone,   phone.c_str(), sizeof(s.phone)   - 1);
    strncpy(s.address, addr.c_str(),  sizeof(s.address) - 1);

    // Role selection from predefined hotel roles
    selectRole(s.role);

    // Grade is tied to role for salary calculation
    cout << "\n  Grade (A=Manager, B=Receptionist, C=Housekeeping,\n"
            "        D=Kitchen/Security, E=Daily Wage): ";
    s.grade = readChar("  Grade: ", "ABCDE");

    // Joining date
    cout << "\n  Joining Date:\n";
    bool validDate = false;
    do {
        s.joiningDay   = readInt("    Day   (1-31) : ", 1, 31);
        s.joiningMonth = readInt("    Month (1-12) : ", 1, 12);
        s.joiningYear  = readInt("    Year         : ", 2000, 2099);
        validDate = isValidDate(s.joiningDay, s.joiningMonth, s.joiningYear);
        if (!validDate) cout << "  [!] Invalid date. Try again.\n";
    } while (!validDate);

    if (s.grade != 'E')
    {
        // Salaried staff — collect allowance flags and basic pay
        s.hasHouseAllowance      = readChar("\n  House allowance? (Y/N): ",      "YN");
        s.hasConveyanceAllowance = readChar("  Conveyance allowance? (Y/N): ",  "YN");
        s.basicSalary = readFloat("  Basic salary (INR, max 50000): ",
                                  0.0f, MAX_BASIC);
    }
    else
    {
        // Daily-wage staff — no fixed basic; salary computed per slip
        s.hasHouseAllowance      = 'N';
        s.hasConveyanceAllowance = 'N';
        s.basicSalary            = 0.0f;
    }

    s.loanAmount = readFloat("  Loan amount (INR, 0 if none): ", 0.0f, MAX_LOAN);

    ofstream outFile(STAFF_FILE, ios::binary | ios::app);
    outFile.write(reinterpret_cast<const char*>(&s), sizeof(StaffMember));

    cout << "\n  [✓] Staff member added. ID: " << s.id << "\n";
    pressEnterToContinue();
}

// ── View Staff ────────────────────────────────────────────────────

void StaffManager::viewStaff()
{
    clearScreen();
    printHeading("View Staff Member");

    int id = readInt("  Enter staff ID: ", 1, 99999);

    StaffMember s;
    if (findById(id, s))
        printStaff(s);
    else
        cout << "  [!] No staff member with ID " << id << ".\n";

    pressEnterToContinue();
}

// ── Edit Staff ────────────────────────────────────────────────────

void StaffManager::editStaff()
{
    clearScreen();
    printHeading("Edit Staff Member");

    int id = readInt("  Enter staff ID to edit: ", 1, 99999);

    // Read current record and remember its position in the file
    fstream file(STAFF_FILE, ios::binary | ios::in | ios::out);
    if (!file)
    {
        cout << "  [!] No records found.\n";
        pressEnterToContinue();
        return;
    }

    StaffMember s;
    bool found = false;
    streampos pos;

    while (true)
    {
        pos = file.tellg();
        if (!file.read(reinterpret_cast<char*>(&s), sizeof(StaffMember))) break;
        if (s.id == id) { found = true; break; }
    }

    if (!found)
    {
        cout << "  [!] Staff ID " << id << " not found.\n";
        pressEnterToContinue();
        return;
    }

    cout << "  Current record:\n";
    printStaff(s);
    cout << "  (Press Enter to keep current value)\n\n";

    string newName  = readOptional("  Name",    s.name,    39);
    string newPhone = readOptional("  Phone",   s.phone,   15);
    string newAddr  = readOptional("  Address", s.address, 59);

    strncpy(s.name,    newName.c_str(),  sizeof(s.name)    - 1);
    strncpy(s.phone,   newPhone.c_str(), sizeof(s.phone)   - 1);
    strncpy(s.address, newAddr.c_str(),  sizeof(s.address) - 1);

    cout << "  Change role? (Y/N): ";
    char changeRole = readChar("", "YN");
    if (changeRole == 'Y') selectRole(s.role);

    if (s.grade != 'E')
    {
        cout << "  House allowance [" << s.hasHouseAllowance << "]: ";
        string ha; getline(cin, ha);
        if (!ha.empty()) s.hasHouseAllowance = toupper(ha[0]);

        cout << "  Conveyance allowance [" << s.hasConveyanceAllowance << "]: ";
        string ca; getline(cin, ca);
        if (!ca.empty()) s.hasConveyanceAllowance = toupper(ca[0]);

        cout << "  Basic salary [" << s.basicSalary << "]: ";
        string bs; getline(cin, bs);
        if (!bs.empty()) s.basicSalary = stof(bs);
    }

    cout << "  Loan amount [" << s.loanAmount << "]: ";
    string ln; getline(cin, ln);
    if (!ln.empty()) s.loanAmount = stof(ln);

    // Overwrite the record in-place at the saved position
    file.seekp(pos);
    file.write(reinterpret_cast<const char*>(&s), sizeof(StaffMember));

    cout << "\n  [✓] Staff record updated.\n";
    pressEnterToContinue();
}

// ── Delete Staff ──────────────────────────────────────────────────

void StaffManager::deleteStaff()
{
    clearScreen();
    printHeading("Delete Staff Member");

    int id = readInt("  Enter staff ID to delete: ", 1, 99999);

    ifstream inFile(STAFF_FILE, ios::binary);
    if (!inFile)
    {
        cout << "  [!] No records found.\n";
        pressEnterToContinue();
        return;
    }

    ofstream tempFile(TEMP_FILE, ios::binary);
    StaffMember s;
    bool found = false;

    while (inFile.read(reinterpret_cast<char*>(&s), sizeof(StaffMember)))
    {
        if (s.id == id)
        {
            found = true;
            printStaff(s);
            char confirm = readChar("  Confirm deletion? (Y/N): ", "YN");
            if (confirm == 'N')
            {
                // Put it back
                tempFile.write(reinterpret_cast<const char*>(&s),
                               sizeof(StaffMember));
                cout << "  Deletion cancelled.\n";
            }
            else
                cout << "  [✓] Staff member deleted.\n";
        }
        else
            tempFile.write(reinterpret_cast<const char*>(&s), sizeof(StaffMember));
    }

    inFile.close();
    tempFile.close();

    if (!found)
        cout << "  [!] Staff ID " << id << " not found.\n";
    else
    {
        remove(STAFF_FILE.c_str());
        rename(TEMP_FILE.c_str(), STAFF_FILE.c_str());
    }

    pressEnterToContinue();
}

// ── List All Staff ────────────────────────────────────────────────

void StaffManager::listStaff()
{
    clearScreen();
    printHeading("All Staff Members");

    ifstream file(STAFF_FILE, ios::binary);
    if (!file)
    {
        cout << "  No staff records found.\n";
        pressEnterToContinue();
        return;
    }

    cout << left
         << setw(6)  << "ID"
         << setw(22) << "Name"
         << setw(18) << "Role"
         << setw(6)  << "Grade"
         << "Salary\n";
    printDivider(60);

    StaffMember s;
    int count = 0;
    while (file.read(reinterpret_cast<char*>(&s), sizeof(StaffMember)))
    {
        printStaff(s, true);    // compact one-line
        ++count;
    }

    printDivider(60);
    cout << "  Total staff: " << count << "\n";
    pressEnterToContinue();
}

// ── Department Summary ────────────────────────────────────────────
/*
 * Counts how many staff members belong to each hotel role.
 * Useful for a quick operational overview.
 */
void StaffManager::departmentSummary()
{
    clearScreen();
    printHeading("Department Summary");

    ifstream file(STAFF_FILE, ios::binary);
    if (!file)
    {
        cout << "  No staff records found.\n";
        pressEnterToContinue();
        return;
    }

    // Initialise count map for each role
    vector<pair<string, int>> counts;
    for (const string& role : STAFF_ROLES)
        counts.push_back({role, 0});

    StaffMember s;
    int total = 0;
    while (file.read(reinterpret_cast<char*>(&s), sizeof(StaffMember)))
    {
        for (auto& entry : counts)
            if (entry.first == string(s.role)) ++entry.second;
        ++total;
    }

    cout << left << setw(22) << "Department"
                 << "Staff Count\n";
    printDivider(35);
    for (const auto& entry : counts)
        cout << left << setw(22) << entry.first << entry.second << "\n";
    printDivider(35);
    cout << left << setw(22) << "TOTAL" << total << "\n";

    pressEnterToContinue();
}

// ── Salary Slip ───────────────────────────────────────────────────

void StaffManager::printSalarySlip(const StaffMember& s) const
{
    int day, month, year;
    getTodayDate(day, month, year);

    clearScreen();
    printDivider(60, '=');
    cout << "                    SALARY SLIP\n";
    printDivider(60, '=');
    cout << "  Date         : " << day << "/" << month << "/" << year
         << "  (" << monthName(month) << " " << year << ")\n"
         << "  Staff ID     : " << s.id   << "\n"
         << "  Name         : " << s.name << "\n"
         << "  Role         : " << s.role << "\n"
         << "  Grade        : " << s.grade << "\n";
    printDivider(60);

    if (s.grade == 'E')
    {
        // Daily-wage — ask how many days worked and overtime hours this month
        int daysWorked = readInt("  Days worked this month (1-31): ", 1, 31);
        int otHours    = readInt("  Overtime hours (0-8)         : ", 0, 8);

        float basic = daysWorked * 30.0f;
        float ot    = otHours    * 10.0f;
        float ld    = (15.0f * s.loanAmount) / 100.0f;
        float net   = basic + ot - ld;

        cout << "\n  Basic (days × INR 30)  : INR " << fixed
             << setprecision(2) << basic << "\n"
             << "  Overtime               : INR " << ot << "\n";
        printDivider(40);
        cout << "  Loan Deduction (15%)   : INR " << ld << "\n";
        printDivider(40, '=');
        cout << "  NET SALARY             : INR " << net << "\n";
        printDivider(40, '=');
    }
    else
    {
        // Salaried staff — compute allowances and deductions
        float hra = (s.hasHouseAllowance      == 'Y') ? (5.0f  * s.basicSalary) / 100.0f : 0.0f;
        float ca  = (s.hasConveyanceAllowance == 'Y') ? (2.0f  * s.basicSalary) / 100.0f : 0.0f;
        float da  =                                      (5.0f  * s.basicSalary) / 100.0f;
        float pf  =                                      (2.0f  * s.basicSalary) / 100.0f;
        float ld  =                                      (15.0f * s.loanAmount)  / 100.0f;

        float totalAllowance  = hra + ca + da;
        float totalDeductions = pf + ld;
        float net             = s.basicSalary + totalAllowance - totalDeductions;

        cout << "\n  Basic Salary           : INR " << fixed
             << setprecision(2) << s.basicSalary << "\n\n"
             << "  ALLOWANCES\n"
             << "    HRA (5%)             : INR " << hra << "\n"
             << "    Conveyance / CA (2%) : INR " << ca  << "\n"
             << "    Dearness / DA (5%)   : INR " << da  << "\n";
        printDivider(45);
        cout << "  Total Allowances       : INR " << totalAllowance << "\n\n"
             << "  DEDUCTIONS\n"
             << "    Provident Fund (2%)  : INR " << pf << "\n"
             << "    Loan Deduction (15%) : INR " << ld << "\n";
        printDivider(45);
        cout << "  Total Deductions       : INR " << totalDeductions << "\n";
        printDivider(45, '=');
        cout << "  NET SALARY             : INR " << net << "\n";
        printDivider(45, '=');
    }
}

void StaffManager::salarySlip()
{
    clearScreen();
    printHeading("Generate Salary Slip");

    int id = readInt("  Enter staff ID: ", 1, 99999);

    StaffMember s;
    if (!findById(id, s))
    {
        cout << "  [!] Staff ID " << id << " not found.\n";
        pressEnterToContinue();
        return;
    }

    printSalarySlip(s);
    pressEnterToContinue();
}

// ── Staff Manager Menu ────────────────────────────────────────────

void StaffManager::run()
{
    int choice = 0;
    while (choice != 8)
    {
        clearScreen();
        printHeading("Staff Management");
        cout << "  1. Add Staff Member\n"
             << "  2. View Staff Member\n"
             << "  3. Edit Staff Member\n"
             << "  4. Delete Staff Member\n"
             << "  5. List All Staff\n"
             << "  6. Department Summary\n"
             << "  7. Generate Salary Slip\n"
             << "  8. Back\n\n";

        choice = readInt("  Your choice: ", 1, 8);

        switch (choice)
        {
            case 1: addStaff();          break;
            case 2: viewStaff();         break;
            case 3: editStaff();         break;
            case 4: deleteStaff();       break;
            case 5: listStaff();         break;
            case 6: departmentSummary(); break;
            case 7: salarySlip();        break;
            case 8: break;
        }
    }
}


// ================================================================
//  MAIN — APPLICATION ENTRY POINT
// ================================================================

int main()
{
    RoomManager  rooms;
    StaffManager staff;

    int choice = 0;
    while (choice != 3)
    {
        clearScreen();
        printDivider(60, '=');
        cout << "\n"
             << "         Hotel Admin Panel\n"
             << "         Powered by C++\n\n";
        printDivider(60, '=');
        cout << "\n"
             << "  1. Room & Guest Management\n"
             << "  2. Staff Management\n"
             << "  3. Exit\n\n";

        choice = readInt("  Select module: ", 1, 3);

        switch (choice)
        {
            case 1: rooms.run(); break;
            case 2: staff.run(); break;
            case 3:
                clearScreen();
                cout << "\n  Thank you for using Hotel Admin Panel. Goodbye!\n\n";
                break;
        }
    }

    return 0;
}
