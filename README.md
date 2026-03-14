# Hotel Admin Panel

A console-based C++ application for managing a hotel's day-to-day operations —
room bookings and staff payroll — from a single admin interface.

---

## What this project does

A hotel admin needs two things in one place: visibility over which
rooms are occupied and by whom, and control over the staff who run the hotel.
This project provides both through a clean menu-driven terminal interface.

**Room & Guest Management**
The admin can book a room for a guest, view or edit that booking, check the
guest out (which prints a final bill and frees the room), see all currently
occupied rooms at a glance, and check which room numbers are still available.

**Staff Management**
The admin can add hotel staff, assign them a role (Manager, Receptionist,
Housekeeping, Security, Kitchen Staff, or Daily Wage) and a grade (A–E) which
drives their salary structure. Records can be viewed, edited, and deleted.
A salary slip can be generated for any staff member at any time, with full
breakdown of allowances (HRA, CA, DA) and deductions (PF, loan).

---

## How the two modules connect

Both modules live under one admin session. The connection is natural:

- The hotel has **rooms** (occupied by guests) and **staff** (who run it).
- Staff roles are specific to a hotel context — not generic HR labels.
- Salary grade E (Daily Wage) mirrors temporary housekeeping or event staff
  that a hotel commonly hires; their pay is computed per day worked.
- Both modules use the same date utilities, the same file I/O pattern,
  and the same input validation — they feel like one system, not two projects
  stitched together.

---

## Project structure

```
hotel_admin_panel.cpp     — entire source (single-file project)
hotel_bookings.dat        — created automatically on first booking
hotel_staff.dat           — created automatically on first staff entry
```

All data is stored in binary flat files. No database or external library is
required. The files are created in the same directory as the executable.

---

## How to compile

**Linux / macOS (g++):**
```bash
g++ -std=c++17 hotel_admin_panel.cpp -o hotel_admin
```

**Windows (g++ via MinGW or WSL):**
```bash
g++ -std=c++17 hotel_admin_panel.cpp -o hotel_admin.exe
```

**Windows (MSVC via Developer Command Prompt):**
```bash
cl /std:c++17 hotel_admin_panel.cpp
```

No external libraries. No special flags beyond C++17.

---

## How to run

```bash
./hotel_admin          # Linux / macOS
hotel_admin.exe        # Windows
```

You will see the main menu:

```
============================================================
         Hotel Admin Panel
         Powered by C++

============================================================

  1. Room & Guest Management
  2. Staff Management
  3. Exit

  Select module:
```

Type the number and press Enter. All navigation works the same way throughout.

---

## Expected behaviour — Room & Guest Management

### Booking a room

1. Select **1 → 1 (Book a Room)**.
2. Enter a room number (1–100). If already occupied, the system tells you and
   returns to the menu.
3. Enter guest name, address, phone, check-in date, and number of nights.
4. The fare is calculated automatically: `nights × INR 900`.
5. The booking is saved to `hotel_bookings.dat`.

### Viewing / editing a booking

- **View**: enter the room number to see full guest details and fare.
- **Edit**: enter the room number, then press Enter to keep any field as-is,
  or type a new value. Nights can be changed; fare recalculates automatically.

### Checking out a guest

1. Select **Check Out Guest** and enter the room number.
2. A final bill is printed showing check-in date, nights stayed, rate, and
   total amount due.
3. Confirm with Y. The record is removed; the room becomes available.
4. Pressing N cancels — the booking stays intact.

### Listing occupied rooms

Shows a table of all currently booked rooms with guest name, phone, nights,
and fare. Displays total occupied count and cumulative revenue at the bottom.

### Checking availability

Lists every free room number (1–100) and gives a count of free vs occupied.
Use this before booking to see what is available.

---

## Expected behaviour — Staff Management

### Adding a staff member

1. Select **2 → 1 (Add Staff Member)**.
2. An ID is assigned automatically (increments from the last saved ID).
3. Enter name, phone, address.
4. Select a role from the predefined list:
   ```
   1. Manager
   2. Receptionist
   3. Housekeeping
   4. Security
   5. Kitchen Staff
   6. Daily Wage
   ```
5. Assign a grade:
   - **A** — Manager (highest salary tier)
   - **B** — Receptionist
   - **C** — Housekeeping / Security
   - **D** — Kitchen Staff
   - **E** — Daily Wage (no fixed basic; paid per day worked)
6. For grades A–D: enter house allowance eligibility (Y/N), conveyance
   allowance eligibility (Y/N), and basic salary.
7. For grade E: allowances are not applicable; skip to loan amount.
8. Enter loan amount (0 if none; max INR 50,000).

### Generating a salary slip

1. Select **Generate Salary Slip** and enter the staff ID.
2. **For grades A–D (salaried):**
   The slip is computed from stored data — no extra input needed.
   ```
   Basic Salary         : INR  25000.00
   ALLOWANCES
     HRA (5%)           : INR   1250.00
     Conveyance (2%)    : INR    500.00
     Dearness DA (5%)   : INR   1250.00
   Total Allowances     : INR   3000.00
   DEDUCTIONS
     Provident Fund (2%): INR    500.00
     Loan Deduction(15%): INR    750.00
   Total Deductions     : INR   1250.00
   =========================================
   NET SALARY           : INR  26750.00
   ```

3. **For grade E (daily wage):**
   You are prompted for days worked this month and overtime hours.
   ```
   Days worked this month: 26
   Overtime hours: 3
   Basic (days × INR 30) : INR    780.00
   Overtime              : INR     30.00
   Loan Deduction (15%)  : INR     75.00
   =========================================
   NET SALARY            : INR    735.00
   ```

### Department summary

Shows how many staff are in each hotel role — useful for a quick headcount.
```
Department             Staff Count
-----------------------------------
Manager                1
Receptionist           2
Housekeeping           4
Security               2
Kitchen Staff          3
Daily Wage             5
-----------------------------------
TOTAL                  17
```

---

## Salary calculation reference

| Component | Rule | Applies to |
|---|---|---|
| HRA | 5% of basic | Grades A–D, if house allowance = Y |
| CA  | 2% of basic | Grades A–D, if conveyance = Y |
| DA  | 5% of basic | Grades A–D, always |
| PF  | 2% of basic | Grades A–D, always (deduction) |
| LD  | 15% of loan | All grades, if loan > 0 (deduction) |
| Basic (E) | days × INR 30 | Grade E only |
| OT (E) | hours × INR 10 | Grade E only |

---

## Input validation

Every input field is validated before saving:
- Room numbers must be between 1 and 100.
- Dates are checked for validity including leap years.
- Salary and loan values have upper bounds (INR 50,000).
- String fields have maximum length limits.
- All character inputs (Y/N, grade, role) must be from the allowed set.
- Invalid input always re-prompts — the program never crashes on bad data.

---

## Technical notes

- **Binary file I/O.** Records are written as raw structs using `fstream`.
  Fixed-length `char` arrays keep `sizeof()` stable across reads and writes.
- **Deletion via temp-file swap.** When a record is deleted, the file is
  copied to a temp file (skipping the deleted record), then renamed back.
  This keeps the binary file contiguous with no gaps.
- **In-place editing.** Edits use `seekp()` to overwrite only the changed
  record without rewriting the entire file.
- **Single source file.** The entire project is `hotel_admin_panel.cpp` —
  easy to submit, review, and compile.

---

## C++ concepts demonstrated

This project is a portfolio demonstration of the following C++ skills:

- Classes and objects with clear separation of responsibilities
- Binary file handling (`fstream`, `ifstream`, `ofstream`) with `read` / `write`
- File seeking (`seekg`, `seekp`) for in-place record updates
- Structs as fixed-layout data records for binary I/O
- Input validation loops with `cin.clear()` and `cin.ignore()`
- `vector`, range-based `for`, lambdas (for availability check)
- `iomanip` (`setw`, `setprecision`, `fixed`) for formatted console output
- `ctime` for reading the system clock (salary slip date)

---

## Author

Amritanshu Singh
