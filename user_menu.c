/******************************************************************************
 * @file    user_menu.c
 * @brief   Citizen service operations and menu dashboard implementation.
 *          Implements PAN details, ATM transactions, Voting with custom icons,
 *          and Driving License checking.
 * @author  RFID Pair Programming Team
 * @date    July 2026
 ******************************************************************************/

#include <lpc21xx.h>
#include "types.h"
#include "lcd_defines.h"
#include "delay.h"
#include "kpm.h"
#include "spi_eeprom_defines.h"
#include "user_menu.h"

// Define 3 system users' database using parallel arrays (no structs)
const char card_ids[NUM_USERS][9] = {
    "00338865",     // Card ID for User 1 (Akash)
    "12557813",     // Card ID for User 2 (Tagore)
    "12621259"      // Card ID for User 3 (Mithul)
};

const char pins[NUM_USERS][5] = {
    "1234",         // PIN for User 1
    "1234",         // PIN for User 2
    "1234"          // PIN for User 3
};

const char names[NUM_USERS][16] = {
    "Akash",
    "Tagore",
    "Mithul"
};

const char dobs[NUM_USERS][12] = {
    "25-06-2005",
    "15-08-2003",
    "10-10-2004"
};

const char pans[NUM_USERS][12] = {
    "ABCDE1234F",
    "XYZWV9876A",
    "PQRST5678B"
};

const char dl_numbers[NUM_USERS][12] = {
    "DL-14201101",
    "DL-22201302",
    "DL-33201403"
};

const char vehicle_classes[NUM_USERS][20] = {
    "2/4 Wheeler",
    "2 Wheeler",
    "4 Wheeler"
};

volatile u8 exp_days[NUM_USERS] = { 31, 15, 1 };
volatile u8 exp_months[NUM_USERS] = { 12, 8, 1 };
volatile u16 exp_years[NUM_USERS] = { 2030, 2029, 2025 }; // Mithul is Expired in 2025!

const u16 eeprom_balance_addrs[NUM_USERS] = { 0x0010, 0x0012, 0x0014 };
const u16 eeprom_vote_addrs[NUM_USERS] = { 0x0020, 0x0021, 0x0022 };
const char addresses[NUM_USERS][6] = { "BLR", "KOL", "DEL" };

volatile int current_user_index = -1; // Global index of the active logged-in user
volatile u8 rtc_interrupted_flag = 0;
volatile u8 auto_logout_flag = 0;

// PIN EEPROM Addresses
const u16 eeprom_login_pin_addrs[NUM_USERS] = { 0x0030, 0x0034, 0x0038 };
const u16 eeprom_atm_pin_addrs[NUM_USERS] = { 0x0040, 0x0044, 0x0048 };

// Custom party symbols data (8 bytes each, total 32 bytes)
const u8 party_symbols[32] = {
    0x04, 0x0E, 0x1F, 0x0E, 0x1B, 0x11, 0x00, 0x00, // Solid Star (0)
    0x04, 0x0E, 0x1F, 0x1F, 0x0E, 0x04, 0x00, 0x00, // Solid Diamond (1)
    0x1F, 0x1F, 0x1F, 0x0E, 0x04, 0x04, 0x0E, 0x00, // Solid Cup (2)
    0x0A, 0x1F, 0x1F, 0x1F, 0x0E, 0x04, 0x00, 0x00  // Solid Heart (3)
};

/**
 * @brief  Write user's ATM balance to external EEPROM.
 */
void write_balance(u16 bal)
{
    if (current_user_index < 0 || current_user_index >= NUM_USERS) return;
    ByteWrite_25LC512(eeprom_balance_addrs[current_user_index], (bal >> 8) & 0xFF);
    ByteWrite_25LC512(eeprom_balance_addrs[current_user_index] + 1, bal & 0xFF);
}

/**
 * @brief  Read user's ATM balance from external EEPROM.
 */
u16 read_balance(void)
{
    u8 high, low;
    u16 bal;
    if (current_user_index < 0 || current_user_index >= NUM_USERS) return 0;
    high = ByteRead_25LC512(eeprom_balance_addrs[current_user_index]);
    low = ByteRead_25LC512(eeprom_balance_addrs[current_user_index] + 1);
    bal = ((u16)high << 8) | low;
    
    if (bal == 0xFFFF)
    {
        bal = 5000; // Initialize with 5000 if EEPROM is blank
        write_balance(bal);
    }
    return bal;
}

/**
 * @brief  Verify and load defaults into EEPROM on first boot magic byte check.
 */
void check_eeprom_init(void)
{
    u8 magic = ByteRead_25LC512(0x0000);
    if (magic != 0xC5)
    {
        // Write magic byte
        ByteWrite_25LC512(0x0000, 0xC5);
        delay_ms(5);
        
        // Initialize User 1 (Akash): Balance = 10000, Vote = 0, Login PIN = "1234", ATM PIN = "1234"
        ByteWrite_25LC512(0x0010, (10000 >> 8) & 0xFF); delay_ms(5);
        ByteWrite_25LC512(0x0011, 10000 & 0xFF); delay_ms(5);
        ByteWrite_25LC512(0x0020, 0x00); delay_ms(5);
        ByteWrite_25LC512(0x0030, '1'); delay_ms(5);
        ByteWrite_25LC512(0x0031, '2'); delay_ms(5);
        ByteWrite_25LC512(0x0032, '3'); delay_ms(5);
        ByteWrite_25LC512(0x0033, '4'); delay_ms(5);
        ByteWrite_25LC512(0x0040, '1'); delay_ms(5);
        ByteWrite_25LC512(0x0041, '2'); delay_ms(5);
        ByteWrite_25LC512(0x0042, '3'); delay_ms(5);
        ByteWrite_25LC512(0x0043, '4'); delay_ms(5);
        
        // Initialize User 2 (Tagore): Balance = 450, Vote = 2, Login PIN = "1234", ATM PIN = "1234"
        ByteWrite_25LC512(0x0012, (450 >> 8) & 0xFF); delay_ms(5);
        ByteWrite_25LC512(0x0013, 450 & 0xFF); delay_ms(5);
        ByteWrite_25LC512(0x0021, 0x02); delay_ms(5);
        ByteWrite_25LC512(0x0034, '1'); delay_ms(5);
        ByteWrite_25LC512(0x0035, '2'); delay_ms(5);
        ByteWrite_25LC512(0x0036, '3'); delay_ms(5);
        ByteWrite_25LC512(0x0037, '4'); delay_ms(5);
        ByteWrite_25LC512(0x0044, '1'); delay_ms(5);
        ByteWrite_25LC512(0x0045, '2'); delay_ms(5);
        ByteWrite_25LC512(0x0046, '3'); delay_ms(5);
        ByteWrite_25LC512(0x0047, '4'); delay_ms(5);
        
        // Initialize User 3 (Mithul): Balance = 5000, Vote = 0, Login PIN = "1234", ATM PIN = "1234"
        ByteWrite_25LC512(0x0014, (5000 >> 8) & 0xFF); delay_ms(5);
        ByteWrite_25LC512(0x0015, 5000 & 0xFF); delay_ms(5);
        ByteWrite_25LC512(0x0022, 0x00); delay_ms(5);
        ByteWrite_25LC512(0x0038, '1'); delay_ms(5);
        ByteWrite_25LC512(0x0039, '2'); delay_ms(5);
        ByteWrite_25LC512(0x003A, '3'); delay_ms(5);
        ByteWrite_25LC512(0x003B, '4'); delay_ms(5);
        ByteWrite_25LC512(0x0048, '1'); delay_ms(5);
        ByteWrite_25LC512(0x0049, '2'); delay_ms(5);
        ByteWrite_25LC512(0x004A, '3'); delay_ms(5);
        ByteWrite_25LC512(0x004B, '4'); delay_ms(5);
    }
}

/**
 * @brief  Prompt and verify passcode with a 20s timeout and clear feature.
 */
u8 verify_password_flow(u8 type)
{
    u8 attempts = 3;
    char entered_pin[5] = {0};
    char correct_pin[5] = {0};
    u16 pin_eeprom_addr;
    u8 i, len;
    char key;
    
    if (current_user_index < 0 || current_user_index >= NUM_USERS) return 0;
    
    if (type == 0)
    {
        pin_eeprom_addr = eeprom_login_pin_addrs[current_user_index];
    }
    else
    {
        pin_eeprom_addr = eeprom_atm_pin_addrs[current_user_index];
    }
    
    while (attempts > 0)
    {
        // Load the correct PIN from EEPROM
        for (i = 0; i < 4; i++)
        {
            correct_pin[i] = ByteRead_25LC512(pin_eeprom_addr + i);
        }
        correct_pin[4] = 0;
        
        len = 0;
        entered_pin[0] = 0; entered_pin[1] = 0; entered_pin[2] = 0; entered_pin[3] = 0; entered_pin[4] = 0;
        
        CmdLCD(CLEAR_LCD);
        CmdLCD(GOTO_LINE1_POS0);
        if (type == 0)
        {
            StrLCD("Enter Login PIN:");
        }
        else
        {
            StrLCD("Enter ATM PIN:");
        }
        CmdLCD(GOTO_LINE4_POS0);
        StrLCD("#:Enter   *:Clr/Exit");
        
        CmdLCD(GOTO_LINE2_POS0); // Position cursor on Line 2
        
        while (1)
        {
            key = KeyScanWithTimeout(20000);
            if (key == 0 || key == 0xFE) return 0; // Timeout or Redraw -> exit
            
            if (key >= '0' && key <= '9')
            {
                if (len < 4)
                {
                    entered_pin[len] = key;
                    CharLCD('*'); // Write asterisk directly to screen
                    len++;
                }
            }
            else if (key == '*') // Backspace or Exit
            {
                if (len > 0)
                {
                    len--;
                    entered_pin[len] = 0;
                    CmdLCD(GOTO_LINE2_POS0 + len); // Move cursor back to deleted char
                    CharLCD(' ');                  // Overwrite with space
                    CmdLCD(GOTO_LINE2_POS0 + len); // Move cursor back again
                }
                else // len == 0 -> cancel/exit
                {
                    return 0;
                }
            }
            else if (key == '#')
            {
                if (len == 4)
                {
                    break; // PIN fully entered, proceed to compare
                }
            }
        }
        
        // Compare PIN
        if (entered_pin[0] == correct_pin[0] &&
            entered_pin[1] == correct_pin[1] &&
            entered_pin[2] == correct_pin[2] &&
            entered_pin[3] == correct_pin[3])
        {
            return 1; // Correct password
        }
        else
        {
            attempts--;
            CmdLCD(CLEAR_LCD);
            CmdLCD(GOTO_LINE1_POS0);
            StrLCD("Wrong PIN!");
            CmdLCD(GOTO_LINE2_POS0);
            StrLCD("Attempts Left: ");
            U32LCD(attempts);
            delay_ms(2000);
        }
    }
    
    return 0; // 3 wrong attempts, fail verification
}

u8 verify_password(void)
{
    return verify_password_flow(0); // type 0 = Login PIN
}

void change_password_flow(void)
{
    char entered_prev[5] = {0};
    char correct_prev[5] = {0};
    char entered_new[5] = {0};
    u16 pin_eeprom_addr;
    u8 i, len;
    char key;
    
    if (current_user_index < 0 || current_user_index >= NUM_USERS) return;
    
    pin_eeprom_addr = eeprom_login_pin_addrs[current_user_index];
    
    // 1. Ask for Previous Password
    len = 0;
    CmdLCD(CLEAR_LCD);
    CmdLCD(GOTO_LINE1_POS0);
    StrLCD("Prev Password:");
    CmdLCD(GOTO_LINE4_POS0);
    StrLCD("#:Enter   *:Clr/Exit");
    
    CmdLCD(GOTO_LINE2_POS0); // Position cursor on Line 2
    
    while (1)
    {
        key = KeyScanWithTimeout(20000);
        if (key == 0 || key == 0xFE) return; // Timeout or Exit
        
        if (key >= '0' && key <= '9')
        {
            if (len < 4)
            {
                entered_prev[len] = key;
                CharLCD('*'); // Print asterisk directly
                len++;
            }
        }
        else if (key == '*')
        {
            if (len > 0)
            {
                len--;
                entered_prev[len] = 0;
                CmdLCD(GOTO_LINE2_POS0 + len); // Move cursor to deleted character
                CharLCD(' ');                  // Overwrite with space
                CmdLCD(GOTO_LINE2_POS0 + len); // Move cursor back
            }
            else
            {
                return; // Cancel
            }
        }
        else if (key == '#')
        {
            if (len == 4) break;
        }
    }
    
    // Verify Previous Password
    for (i = 0; i < 4; i++)
    {
        correct_prev[i] = ByteRead_25LC512(pin_eeprom_addr + i);
    }
    correct_prev[4] = 0;
    
    if (entered_prev[0] != correct_prev[0] ||
        entered_prev[1] != correct_prev[1] ||
        entered_prev[2] != correct_prev[2] ||
        entered_prev[3] != correct_prev[3])
    {
        CmdLCD(CLEAR_LCD);
        CmdLCD(GOTO_LINE1_POS0);
        StrLCD("Wrong Password!");
        delay_ms(2000);
        return;
    }
    
    // 2. Ask for New Password
    len = 0;
    CmdLCD(CLEAR_LCD);
    CmdLCD(GOTO_LINE1_POS0);
    StrLCD("New Password:");
    CmdLCD(GOTO_LINE4_POS0);
    StrLCD("#:Enter   *:Clr/Exit");
    
    CmdLCD(GOTO_LINE2_POS0); // Position cursor on Line 2
    
    while (1)
    {
        key = KeyScanWithTimeout(20000);
        if (key == 0 || key == 0xFE) return; // Timeout or Exit
        
        if (key >= '0' && key <= '9')
        {
            if (len < 4)
            {
                entered_new[len] = key;
                CharLCD('*'); // Print asterisk directly
                len++;
            }
        }
        else if (key == '*')
        {
            if (len > 0)
            {
                len--;
                entered_new[len] = 0;
                CmdLCD(GOTO_LINE2_POS0 + len); // Move cursor to deleted character
                CharLCD(' ');                  // Overwrite with space
                CmdLCD(GOTO_LINE2_POS0 + len); // Move cursor back
            }
            else
            {
                return; // Cancel
            }
        }
        else if (key == '#')
        {
            if (len == 4) break;
        }
    }
    
    // 3. Ask to Confirm New Password
    {
        char entered_confirm[5] = {0};
        len = 0;
        CmdLCD(CLEAR_LCD);
        CmdLCD(GOTO_LINE1_POS0);
        StrLCD("Confirm Password:");
        CmdLCD(GOTO_LINE4_POS0);
        StrLCD("#:Enter   *:Clr/Exit");
        
        CmdLCD(GOTO_LINE2_POS0); // Position cursor on Line 2
        
        while (1)
        {
            key = KeyScanWithTimeout(20000);
            if (key == 0 || key == 0xFE) return; // Timeout or Exit
            
            if (key >= '0' && key <= '9')
            {
                if (len < 4)
                {
                    entered_confirm[len] = key;
                    CharLCD('*'); // Print asterisk directly
                    len++;
                }
            }
            else if (key == '*')
            {
                if (len > 0)
                {
                    len--;
                    entered_confirm[len] = 0;
                    CmdLCD(GOTO_LINE2_POS0 + len); // Move cursor to deleted character
                    CharLCD(' ');                  // Overwrite with space
                    CmdLCD(GOTO_LINE2_POS0 + len); // Move cursor back
                }
                else
                {
                    return; // Cancel
                }
            }
            else if (key == '#')
            {
                if (len == 4) break;
            }
        }
        
        // Compare new password with confirm password
        for (i = 0; i < 4; i++)
        {
            if (entered_new[i] != entered_confirm[i])
            {
                CmdLCD(CLEAR_LCD);
                CmdLCD(GOTO_LINE1_POS0);
                StrLCD("Not Match!");
                delay_ms(2000);
                return;
            }
        }
    }
    
    // Save New Password to EEPROM for BOTH Login PIN and ATM PIN
    for (i = 0; i < 4; i++)
    {
        ByteWrite_25LC512(eeprom_login_pin_addrs[current_user_index] + i, entered_new[i]);
        delay_ms(5);
        ByteWrite_25LC512(eeprom_atm_pin_addrs[current_user_index] + i, entered_new[i]);
        delay_ms(5);
    }
    
    CmdLCD(CLEAR_LCD);
    CmdLCD(GOTO_LINE1_POS0);
    StrLCD("Pass Changed!");
    delay_ms(2000);
}

/**
 * @brief  Enter numeric amount for deposits/withdrawals with backspace support.
 */
u32 enter_amount(u8 is_withdrawal)
{
    u32 val = 0;
    char key;
    u8 len = 0;
    
    CmdLCD(CLEAR_LCD);
    CmdLCD(GOTO_LINE1_POS0);
    if (is_withdrawal)
    {
        StrLCD("WD: 100/200/500 only");
        CmdLCD(GOTO_LINE2_POS0);
        StrLCD("Min Bal: Rs.500");
    }
    else
    {
        StrLCD("Dep:100/200/500 only");
        CmdLCD(GOTO_LINE2_POS0);
        StrLCD("Max Bal: Rs.65535");
    }
    CmdLCD(GOTO_LINE3_POS0);
    StrLCD("Amount: ");
    CmdLCD(GOTO_LINE4_POS0);
    StrLCD("#:Enter   *:Clr/Exit");
    CmdLCD(GOTO_LINE3_POS0 + 8); // Position cursor on Line 3, position 8 (after "Amount: ")
    
    while (1)
    {
        key = KeyScanWithTimeout(20000);
        if (key == 0) return 0xFFFFFFFF; // Timeout
        
        if (key >= '0' && key <= '9')
        {
            if (len < 5) // Limit input to maximum 5 digits
            {
                val = (val * 10) + (key - '0');
                CharLCD(key);
                len++;
            }
        }
        else if (key == '*') // Backspace or Exit
        {
            if (len > 0)
            {
                val = val / 10;
                len--;
                CmdLCD(GOTO_LINE3_POS0 + 8 + len); // Move cursor to deleted character
                CharLCD(' ');                      // Overwrite with space
                CmdLCD(GOTO_LINE3_POS0 + 8 + len); // Move cursor back
            }
            else // len == 0
            {
                return 0xFFFFFFFF; // Exit/Cancel amount entry!
            }
        }
        else if (key == '#')
        {
            if (len > 0) // Must enter at least one digit
            {
                return val;
            }
        }
    }
}

/**
 * @brief  Read generic numeric value for calendar adjustments (hour, minute, etc.)
 */
u32 rtc_read_num(u8 max_digits)
{
    u32 val = 0;
    char key;
    u8 len = 0;
    
    CmdLCD(GOTO_LINE2_POS0); // Force input on Line 2
    
    while (1)
    {
        key = KeyScanWithTimeout(20000);
        if (key == 0 || key == 0xFE) return 999999; // Return special timeout/cancel code
        
        if (key >= '0' && key <= '9')
        {
            if (len < max_digits) // Limit input to specified digit length
            {
                val = (val * 10) + (key - '0');
                CharLCD(key);
                len++;
            }
        }
        else if (key == '*') // Backspace or Exit
        {
            if (len > 0)
            {
                val = val / 10;
                len--;
                CmdLCD(GOTO_LINE2_POS0 + len); // Move cursor to deleted character
                CharLCD(' ');                  // Overwrite with space
                CmdLCD(GOTO_LINE2_POS0 + len); // Move cursor back
            }
            else // len == 0 -> Cancel/Exit
            {
                return 999999;
            }
        }
        else if (key == '#')
        {
            if (len > 0) // Must enter at least one digit
            {
                return val;
            }
        }
    }
}

/**
 * @brief  Show citizen PAN database.
 */
void PAN_menu(void)
{
    char key;
    if (!verify_password()) return;
    if (auto_logout_flag) return;
    
    CmdLCD(CLEAR_LCD);
    CmdLCD(GOTO_LINE1_POS0);
    StrLCD("Name: "); StrLCD((s8 *)names[current_user_index]);
    CmdLCD(GOTO_LINE2_POS0);
    StrLCD("DOB : "); StrLCD((s8 *)dobs[current_user_index]);
    CmdLCD(GOTO_LINE3_POS0);
    StrLCD("PAN : "); StrLCD((s8 *)pans[current_user_index]);
    CmdLCD(GOTO_LINE4_POS0);
    StrLCD("*:Exit");
    
    while (1)
    {
        if (auto_logout_flag) break;
        key = KeyScanWithTimeout(20000);
        if (key == 0 || key == '*' || auto_logout_flag) break;
    }
}

/**
 * @brief  ATM balance check, withdrawals, and deposits.
 */
void ATM_menu(void)
{
    char key;
    u16 bal;
    u32 amt;
    
    if (!verify_password_flow(1)) return;
    if (auto_logout_flag) return;
    
    while (1)
    {
        if (auto_logout_flag) break;
        
        CmdLCD(CLEAR_LCD);
        CmdLCD(GOTO_LINE1_POS0);
        StrLCD("1:Balance Enquiry");
        CmdLCD(GOTO_LINE2_POS0);
        StrLCD("2:Cash WD");
        CmdLCD(GOTO_LINE3_POS0);
        StrLCD("3:Cash Deposit");
        CmdLCD(GOTO_LINE4_POS0);
        StrLCD("4:Exit");
        
        key = KeyScanWithTimeout(20000);
        if (key == 0 || key == '4' || auto_logout_flag) break; // Timeout or Exit
        
        if (key == '1')
        {
            bal = read_balance();
            while (1)
            {
                CmdLCD(CLEAR_LCD);
                CmdLCD(GOTO_LINE1_POS0);
                StrLCD("Balance: Rs.");
                CmdLCD(GOTO_LINE2_POS0);
                U32LCD(bal);
                CmdLCD(GOTO_LINE4_POS0);
                StrLCD("*:Exit");
                
                key = KeyScanWithTimeout(20000);
                if (key == 0 || key == '*') break; // Timeout or Exit
            }
        }
        else if (key == '2')
        {
            while (1)
            {
                amt = enter_amount(1); // 1 = Withdrawal
                if (amt == 0xFFFFFFFF) break;   // Timeout or cancelled
                if (amt > 0 && amt % 100 == 0)
                {
                    break;
                }
                
                CmdLCD(CLEAR_LCD);
                CmdLCD(GOTO_LINE1_POS0);
                StrLCD("Invalid Amount!");
                CmdLCD(GOTO_LINE2_POS0);
                StrLCD("Must be multiples");
                CmdLCD(GOTO_LINE3_POS0);
                StrLCD("of 100/200/500");
                delay_ms(2500);
            }
            if (amt == 0xFFFFFFFF) continue;
            
            bal = read_balance();
            if (bal >= 500 && (bal - 500) >= amt)
            {
                bal -= amt;
                write_balance(bal);
                CmdLCD(CLEAR_LCD);
                StrLCD("Withdraw Success!");
                CmdLCD(GOTO_LINE2_POS0);
                StrLCD("New Bal: Rs.");
                U32LCD(bal);
            }
            else
            {
                CmdLCD(CLEAR_LCD);
                CmdLCD(GOTO_LINE1_POS0);
                StrLCD("Min Balance");
                CmdLCD(GOTO_LINE2_POS0);
                StrLCD("Required");
            }
            delay_ms(2500);
        }
        else if (key == '3')
        {
            while (1)
            {
                amt = enter_amount(0); // 0 = Deposit
                if (amt == 0xFFFFFFFF) break;   // Timeout or cancelled
                if (amt > 0 && amt % 100 == 0)
                {
                    break;
                }
                
                CmdLCD(CLEAR_LCD);
                CmdLCD(GOTO_LINE1_POS0);
                StrLCD("Invalid Amount!");
                CmdLCD(GOTO_LINE2_POS0);
                StrLCD("Must be multiples");
                CmdLCD(GOTO_LINE3_POS0);
                StrLCD("of 100/200/500");
                delay_ms(2500);
            }
            if (amt == 0xFFFFFFFF) continue;
            
            bal = read_balance();
            if (65535 - bal >= amt)
            {
                bal += amt;
                write_balance(bal);
                CmdLCD(CLEAR_LCD);
                StrLCD("Deposit Success!");
                CmdLCD(GOTO_LINE2_POS0);
                StrLCD("New Bal: Rs.");
                U32LCD(bal);
            }
            else
            {
                CmdLCD(CLEAR_LCD);
                CmdLCD(GOTO_LINE1_POS0);
                StrLCD("Limit Exceeded");
            }
            delay_ms(2500);
        }
    }
}

/**
 * @brief  Cast vote for parties (1-4) or automatically cast NOTA.
 */
void VOTE_menu(void)
{
    u8 status;
    char key;
    
    if (current_user_index < 0 || current_user_index >= NUM_USERS) return;
    if (!verify_password()) return;
    if (auto_logout_flag) return;
    
    status = ByteRead_25LC512(eeprom_vote_addrs[current_user_index]);
    if (status != 0 && status != 0xFF)
    {
        CmdLCD(CLEAR_LCD);
        CmdLCD(GOTO_LINE1_POS0);
        StrLCD("Already Voted!");
        delay_ms(2500);
        return;
    }
    
    // Load custom icons into CGRAM
    BuildCGRAM((u8*)party_symbols, 32);
    
    while (1)
    {
        if (auto_logout_flag) return;
        
        CmdLCD(CLEAR_LCD);
        CmdLCD(GOTO_LINE1_POS0);
        StrLCD("1.P1:"); CharLCD(0); StrLCD("      2.P2:"); CharLCD(1);
        CmdLCD(GOTO_LINE2_POS0);
        StrLCD("3.P3:"); CharLCD(2); StrLCD("      4.P4:"); CharLCD(3);
        CmdLCD(GOTO_LINE3_POS0);
        StrLCD("5:Exit");
        
        key = KeyScanWithTimeout(20000);
        if (key == 0 || key == '5' || auto_logout_flag) return; // Timeout or Exit
        
        if (key >= '1' && key <= '4')
        {
            u8 party = key - '0';
            ByteWrite_25LC512(eeprom_vote_addrs[current_user_index], party);
            
            CmdLCD(CLEAR_LCD);
            CmdLCD(GOTO_LINE1_POS0);
            StrLCD("Already Voted!");
            delay_ms(2500);
            break; // Vote recorded successfully, exit loop
        }
    }
}

// Forward declarations
void rtc_edit_menu(void);

/**
 * @brief  Show license details screen with custom bold check/cross validity indicators.
 */
void show_license_details(void)
{
    u8 d = DOM;
    u8 m = MONTH;
    u16 y = YEAR;
    u8 expired = 0;
    char *short_class = "2/4W";
    char *v_class;
    char k;
    
    // Bold Check-cross patterns for CGRAM (0 = Check, 1 = Cross)
    const u8 check_cross_symbols[16] = {
        0x00, 0x00, 0x01, 0x03, 0x16, 0x1C, 0x08, 0x00, // Bold Checkmark (0)
        0x00, 0x11, 0x1B, 0x0E, 0x04, 0x0E, 0x1B, 0x11  // Bold Cross (1)
    };
    
    if (current_user_index < 0 || current_user_index >= NUM_USERS) return;
    
    // Load custom check/cross icons into CGRAM
    BuildCGRAM((u8*)check_cross_symbols, 16);
    
    // Display Present Date & Time for 3 seconds (No buzzer/LEDs active)
    IOCLR0 = (1 << 19) | (1 << 20) | (1 << 21); // Ensure Buzzer and LEDs are OFF
    CmdLCD(CLEAR_LCD);
    CmdLCD(GOTO_LINE1_POS0);
    StrLCD("Current Date/Time:");
    
    CmdLCD(GOTO_LINE2_POS0);
    StrLCD("Date: ");
    if (d < 10) CharLCD('0');
    U32LCD(d);
    CharLCD('-');
    if (m < 10) CharLCD('0');
    U32LCD(m);
    CharLCD('-');
    U32LCD(y);
    
    CmdLCD(GOTO_LINE3_POS0);
    StrLCD("Time: ");
    if (HOUR < 10) CharLCD('0');
    U32LCD(HOUR);
    CharLCD(':');
    if (MIN < 10) CharLCD('0');
    U32LCD(MIN);
    CharLCD(':');
    if (SEC < 10) CharLCD('0');
    U32LCD(SEC);
    
    CmdLCD(GOTO_LINE4_POS0);
    StrLCD("Day : ");
    {
        const char *day_names[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
        StrLCD((s8 *)day_names[DOW % 7]);
    }
    
    delay_ms(3000);
    
    while (1)
    {
        d = DOM;
        m = MONTH;
        y = YEAR;
        expired = 0;
        short_class = "2/4W";
        
        // License Validity Check
        if (y > exp_years[current_user_index]) expired = 1;
        else if (y == exp_years[current_user_index] && m > exp_months[current_user_index]) expired = 1;
        else if (y == exp_years[current_user_index] && m == exp_months[current_user_index] && d > exp_days[current_user_index]) expired = 1;
        
        // Map class to short representation
        v_class = (char *)vehicle_classes[current_user_index];
        if (v_class[0] == '2' && v_class[1] == ' ' && v_class[2] == 'W') short_class = "2W";
        else if (v_class[0] == '4') short_class = "4W";
        
        CmdLCD(CLEAR_LCD);
        CmdLCD(GOTO_LINE1_POS0);
        StrLCD("DL No: ");
        StrLCD((s8 *)dl_numbers[current_user_index]);
        
        if (!expired)
        {
            CmdLCD(GOTO_LINE2_POS0);
            StrLCD("Cls: "); StrLCD((s8 *)short_class);
            StrLCD("   Add: "); StrLCD((s8 *)addresses[current_user_index]);
            
            CmdLCD(GOTO_LINE3_POS0);
            StrLCD("Exp: ");
            U32LCD(exp_days[current_user_index]); CharLCD('-');
            U32LCD(exp_months[current_user_index]); CharLCD('-');
            U32LCD(exp_years[current_user_index]);
            
            CmdLCD(GOTO_LINE4_POS0);
            StrLCD("Valid ");
            CharLCD(0); // Print Custom Checkmark symbol
            StrLCD("       *:Exit");
            
            IOSET0 = (1 << 21); // Turn ON Green LED
            IOCLR0 = (1 << 20) | (1 << 19); // Turn OFF Red LED and Buzzer
        }
        else
        {
            CmdLCD(GOTO_LINE2_POS0);
            StrLCD("Cls: "); StrLCD((s8 *)short_class);
            StrLCD("   Add: "); StrLCD((s8 *)addresses[current_user_index]);
            
            CmdLCD(GOTO_LINE3_POS0);
            StrLCD("Exp: ");
            U32LCD(exp_days[current_user_index]); CharLCD('-');
            U32LCD(exp_months[current_user_index]); CharLCD('-');
            U32LCD(exp_years[current_user_index]);
            
            CmdLCD(GOTO_LINE4_POS0);
            StrLCD("Invalid ");
            CharLCD(1); // Print Custom Cross symbol
            StrLCD("     *:Exit");
            
            IOSET0 = (1 << 20) | (1 << 19); // Turn ON Red LED and Buzzer
            IOCLR0 = (1 << 21); // Turn OFF Green LED
        }
        
        if (auto_logout_flag) break;
        k = KeyScanWithTimeout(20000);
        if (k == '*' || k == 0 || auto_logout_flag) break;
    }
    
    // Restore indicators back to active login state
    IOSET0 = (1 << 21);
    IOCLR0 = (1 << 20) | (1 << 19);
}

/**
 * @brief  Driving License menu dashboard.
 */
void DRIVING_menu(void)
{
    char key;
    while (1)
    {
        if (auto_logout_flag) break;
        
        CmdLCD(CLEAR_LCD);
        CmdLCD(GOTO_LINE1_POS0);
        StrLCD("1.Show Card Details");
        CmdLCD(GOTO_LINE2_POS0);
        StrLCD("*:Exit");
        
        key = KeyScanWithTimeout(20000);
        if (key == 0 || key == '*' || auto_logout_flag) break; // Timeout -> exit
        
        if (key == '1')
        {
            show_license_details();
        }
    }
}

/**
 * @brief  Citizen Options Dashboard.
 */
void user_menu(void)
{
    char key;
    while (1)
    {
        if (auto_logout_flag) return;
        
        CmdLCD(CLEAR_LCD);
        CmdLCD(GOTO_LINE1_POS0);
        StrLCD("1.PAN    2.ATM");
        CmdLCD(GOTO_LINE2_POS0);
        StrLCD("3.VOTE   4.DRIV LIC");
        CmdLCD(GOTO_LINE3_POS0);
        StrLCD("5:Exit   6.PWD.CHG");
        
        key = KeyScanWithTimeout(20000);
        if (key == 0 || key == '5' || auto_logout_flag) return; // Timeout -> auto log out (returns to scan)
        
        if (key == '1')
        {
            PAN_menu();
        }
        else if (key == '2')
        {
            ATM_menu();
        }
        else if (key == '3')
        {
            VOTE_menu();
        }
        else if (key == '4')
        {
            DRIVING_menu();
        }
        else if (key == '6')
        {
            change_password_flow();
        }
    }
}

/**
 * @brief  Calculate and update the RTC's Day of Week (DOW) register dynamically
 *         using Sakamoto's algorithm (0 = Sunday, 1 = Monday, ..., 6 = Saturday).
 */
void update_rtc_dow(void)
{
    u8 d = DOM;
    u8 m = MONTH;
    u16 y = YEAR;
    u8 dow;
    static const u8 t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    
    if (m < 3)
    {
        y -= 1;
    }
    dow = (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
    
    CCR = 0x02; // Disable RTC and reset CTC
    DOW = dow;
    CCR = 0x01; // Enable RTC
}

/**
 * @brief  Menu-based Clock adjust interface for RTC.
 */
void rtc_edit_menu(void)
{
    char key;
    u32 val;
    
    while (1)
    {
        CmdLCD(CLEAR_LCD);
        CmdLCD(GOTO_LINE1_POS0);
        StrLCD("1.Hour  2.Min  3.Sec");
        CmdLCD(GOTO_LINE2_POS0);
        StrLCD("4.Day   5.Mon  6.Yr");
        CmdLCD(GOTO_LINE3_POS0);
        StrLCD("*:Exit");
        
        delay_ms(300); // Prevent keypress leakage/double-trigger on cancel/exit
        key = KeyScanWithTimeout(20000);
        if (key == 0 || key == '*') break; // Timeout -> exit
        
        if (key == '1') // Hour
        {
            while (1)
            {
                CmdLCD(CLEAR_LCD);
                CmdLCD(GOTO_LINE1_POS0);
                StrLCD("Enter Hour (0-23):");
                CmdLCD(GOTO_LINE4_POS0);
                StrLCD("#:Enter      *:Clear");
                
                val = rtc_read_num(2);
                if (val == 999999) break; // Timeout
                
                if (val <= 23)
                {
                    CCR = 0x02;
                    HOUR = val;
                    CCR = 0x01;
                    
                    CmdLCD(CLEAR_LCD);
                    CmdLCD(GOTO_LINE1_POS0);
                    StrLCD("Hour Updated!");
                    delay_ms(1500);
                    break;
                }
                
                CmdLCD(CLEAR_LCD);
                CmdLCD(GOTO_LINE1_POS0);
                StrLCD("Invalid Hour!");
                CmdLCD(GOTO_LINE2_POS0);
                StrLCD("Must be 0 to 23");
                delay_ms(1500);
            }
        }
        else if (key == '2') // Minute
        {
            while (1)
            {
                CmdLCD(CLEAR_LCD);
                CmdLCD(GOTO_LINE1_POS0);
                StrLCD("Enter Min (0-59):");
                CmdLCD(GOTO_LINE4_POS0);
                StrLCD("#:Enter      *:Clear");
                
                val = rtc_read_num(2);
                if (val == 999999) break; // Timeout
                
                if (val <= 59)
                {
                    CCR = 0x02;
                    MIN = val;
                    CCR = 0x01;
                    
                    CmdLCD(CLEAR_LCD);
                    CmdLCD(GOTO_LINE1_POS0);
                    StrLCD("Minute Updated!");
                    delay_ms(1500);
                    break;
                }
                
                CmdLCD(CLEAR_LCD);
                CmdLCD(GOTO_LINE1_POS0);
                StrLCD("Invalid Minute!");
                CmdLCD(GOTO_LINE2_POS0);
                StrLCD("Must be 0 to 59");
                delay_ms(1500);
            }
        }
        else if (key == '3') // Second
        {
            while (1)
            {
                CmdLCD(CLEAR_LCD);
                CmdLCD(GOTO_LINE1_POS0);
                StrLCD("Enter Sec (0-59):");
                CmdLCD(GOTO_LINE4_POS0);
                StrLCD("#:Enter      *:Clear");
                
                val = rtc_read_num(2);
                if (val == 999999) break; // Timeout
                
                if (val <= 59)
                {
                    CCR = 0x02;
                    SEC = val;
                    CCR = 0x01;
                    
                    CmdLCD(CLEAR_LCD);
                    CmdLCD(GOTO_LINE1_POS0);
                    StrLCD("Second Updated!");
                    delay_ms(1500);
                    break;
                }
                
                CmdLCD(CLEAR_LCD);
                CmdLCD(GOTO_LINE1_POS0);
                StrLCD("Invalid Second!");
                CmdLCD(GOTO_LINE2_POS0);
                StrLCD("Must be 0 to 59");
                delay_ms(1500);
            }
        }
        else if (key == '4') // Day
        {
            while (1)
            {
                u8 cur_m = MONTH;
                u16 cur_y = YEAR;
                u8 max_days = 31;
                
                if (cur_m == 2)
                {
                    if (cur_y % 4 == 0) max_days = 29;
                    else max_days = 28;
                }
                else if (cur_m == 4 || cur_m == 6 || cur_m == 9 || cur_m == 11)
                {
                    max_days = 30;
                }
                
                CmdLCD(CLEAR_LCD);
                CmdLCD(GOTO_LINE1_POS0);
                StrLCD("Enter Day (1-");
                U32LCD(max_days);
                StrLCD("):");
                CmdLCD(GOTO_LINE4_POS0);
                StrLCD("#:Enter      *:Clear");
                
                val = rtc_read_num(2);
                if (val == 999999) break; // Timeout
                
                if (val >= 1 && val <= max_days)
                {
                    CCR = 0x02;
                    DOM = val;
                    CCR = 0x01;
                    
                    update_rtc_dow(); // Recalculate day of week
                    
                    CmdLCD(CLEAR_LCD);
                    CmdLCD(GOTO_LINE1_POS0);
                    StrLCD("Day Updated!");
                    delay_ms(1500);
                    break;
                }
                
                CmdLCD(CLEAR_LCD);
                CmdLCD(GOTO_LINE1_POS0);
                StrLCD("Invalid Day!");
                CmdLCD(GOTO_LINE2_POS0);
                StrLCD("Must be 1 to ");
                U32LCD(max_days);
                delay_ms(1500);
            }
        }
        else if (key == '5') // Month
        {
            while (1)
            {
                CmdLCD(CLEAR_LCD);
                CmdLCD(GOTO_LINE1_POS0);
                StrLCD("Enter Mon (1-12):");
                CmdLCD(GOTO_LINE4_POS0);
                StrLCD("#:Enter      *:Clear");
                
                val = rtc_read_num(2);
                if (val == 999999) break; // Timeout
                
                if (val >= 1 && val <= 12)
                {
                    u16 cur_y = YEAR;
                    u8 cur_d = DOM;
                    u8 max_days = 31;
                    if (val == 2)
                    {
                        if (cur_y % 4 == 0) max_days = 29;
                        else max_days = 28;
                    }
                    else if (val == 4 || val == 6 || val == 9 || val == 11)
                    {
                        max_days = 30;
                    }
                    
                    CCR = 0x02;
                    if (cur_d > max_days)
                    {
                        DOM = 1; // Safeguard out of range dates
                    }
                    MONTH = val;
                    CCR = 0x01;
                    
                    update_rtc_dow(); // Recalculate day of week
                    
                    CmdLCD(CLEAR_LCD);
                    CmdLCD(GOTO_LINE1_POS0);
                    StrLCD("Month Updated!");
                    delay_ms(1500);
                    break;
                }
                
                CmdLCD(CLEAR_LCD);
                CmdLCD(GOTO_LINE1_POS0);
                StrLCD("Invalid Month!");
                CmdLCD(GOTO_LINE2_POS0);
                StrLCD("Must be 1 to 12");
                delay_ms(1500);
            }
        }
        else if (key == '6') // Year
        {
            while (1)
            {
                CmdLCD(CLEAR_LCD);
                CmdLCD(GOTO_LINE1_POS0);
                StrLCD("Enter Year (YYYY):");
                CmdLCD(GOTO_LINE4_POS0);
                StrLCD("#:Enter      *:Clear");
                
                val = rtc_read_num(4);
                if (val == 999999) break; // Timeout
                
                if (val >= 2000 && val <= 2099)
                {
                    u8 cur_m = MONTH;
                    u8 cur_d = DOM;
                    if (cur_m == 2 && cur_d == 29 && (val % 4 != 0))
                    {
                        CCR = 0x02;
                        DOM = 28; // Adjust Feb 29 to Feb 28 on non-leap years
                        YEAR = val;
                        CCR = 0x01;
                    }
                    else
                    {
                        CCR = 0x02;
                        YEAR = val;
                        CCR = 0x01;
                    }
                    
                    update_rtc_dow(); // Recalculate day of week
                    
                    CmdLCD(CLEAR_LCD);
                    CmdLCD(GOTO_LINE1_POS0);
                    StrLCD("Year Updated!");
                    delay_ms(1500);
                    break;
                }
                
                CmdLCD(CLEAR_LCD);
                CmdLCD(GOTO_LINE1_POS0);
                StrLCD("Invalid Year!");
                CmdLCD(GOTO_LINE2_POS0);
                StrLCD("Must be 2000-2099");
                delay_ms(1500);
            }
        }
    }
}

extern volatile u8 rfid_ready;
extern unsigned char rfid_buffer[16];

/**
 * @brief  Scan and identify a user card for administrative edits.
 *         Returns user index (0 to NUM_USERS-1) or -1 on cancel/timeout.
 */
int scan_target_user(void)
{
    unsigned char card[9];
    u8 u_idx;
    
    rfid_ready = 0; // Reset scanner
    CmdLCD(CLEAR_LCD);
    CmdLCD(GOTO_LINE1_POS0);
    StrLCD("Scan User Card...");
    CmdLCD(GOTO_LINE4_POS0);
    StrLCD("*:Cancel");
    
    while (1)
    {
        if (auto_logout_flag) return -1;
        
        // Check if cancel was pressed
        if (ColScan() == 0)
        {
            char key = KeyScanWithTimeout(200);
            if (key == '*')
            {
                return -1;
            }
        }
        
        if (rfid_ready)
        {
            rfid_ready = 0;
            for (u_idx = 0; u_idx < 8; u_idx++) card[u_idx] = rfid_buffer[u_idx];
            card[8] = '\0';
            
            // Scan user database
            for (u_idx = 0; u_idx < NUM_USERS; u_idx++)
            {
                extern u8 is_same_id(const unsigned char *s1, const char *s2, u8 len);
                if (is_same_id(card, card_ids[u_idx], 8))
                {
                    return u_idx;
                }
            }
            
            // Invalid
            CmdLCD(CLEAR_LCD);
            CmdLCD(GOTO_LINE1_POS0);
            StrLCD("Invalid User Card!");
            delay_ms(2000);
            
            // Prompt again
            CmdLCD(CLEAR_LCD);
            CmdLCD(GOTO_LINE1_POS0);
            StrLCD("Scan User Card...");
            CmdLCD(GOTO_LINE4_POS0);
            StrLCD("*:Cancel");
        }
        delay_ms(10);
    }
}

void license_edit_menu(int target_idx)
{
    char key;
    u32 val;
    
    if (target_idx < 0 || target_idx >= NUM_USERS) return;
    
    while (1)
    {
        if (auto_logout_flag) break;
        
        CmdLCD(CLEAR_LCD);
        CmdLCD(GOTO_LINE1_POS0);
        StrLCD("Edit Exp: ");
        StrLCD((s8 *)names[target_idx]);
        CmdLCD(GOTO_LINE2_POS0);
        StrLCD("1.Day   2.Mon  3.Yr");
        CmdLCD(GOTO_LINE3_POS0);
        StrLCD("*:Exit");
        
        delay_ms(300);
        key = KeyScanWithTimeout(20000);
        if (key == 0 || key == '*' || auto_logout_flag) break;
        
        if (key == '1') // Day
        {
            while (1)
            {
                CmdLCD(CLEAR_LCD);
                CmdLCD(GOTO_LINE1_POS0);
                StrLCD("Enter Exp Day(1-31):");
                CmdLCD(GOTO_LINE4_POS0);
                StrLCD("#:Enter      *:Clear");
                
                val = rtc_read_num(2);
                if (val == 999999) break;
                
                if (val >= 1 && val <= 31)
                {
                    exp_days[target_idx] = val;
                    CmdLCD(CLEAR_LCD);
                    CmdLCD(GOTO_LINE1_POS0);
                    StrLCD("Day Updated!");
                    delay_ms(1500);
                    break;
                }
                
                CmdLCD(CLEAR_LCD);
                CmdLCD(GOTO_LINE1_POS0);
                StrLCD("Invalid Day!");
                delay_ms(1500);
            }
        }
        else if (key == '2') // Month
        {
            while (1)
            {
                CmdLCD(CLEAR_LCD);
                CmdLCD(GOTO_LINE1_POS0);
                StrLCD("Enter Exp Mon(1-12):");
                CmdLCD(GOTO_LINE4_POS0);
                StrLCD("#:Enter      *:Clear");
                
                val = rtc_read_num(2);
                if (val == 999999) break;
                
                if (val >= 1 && val <= 12)
                {
                    exp_months[target_idx] = val;
                    CmdLCD(CLEAR_LCD);
                    CmdLCD(GOTO_LINE1_POS0);
                    StrLCD("Month Updated!");
                    delay_ms(1500);
                    break;
                }
                
                CmdLCD(CLEAR_LCD);
                CmdLCD(GOTO_LINE1_POS0);
                StrLCD("Invalid Month!");
                delay_ms(1500);
            }
        }
        else if (key == '3') // Year
        {
            while (1)
            {
                CmdLCD(CLEAR_LCD);
                CmdLCD(GOTO_LINE1_POS0);
                StrLCD("Enter Exp Yr:");
                CmdLCD(GOTO_LINE4_POS0);
                StrLCD("#:Enter      *:Clear");
                
                val = rtc_read_num(4);
                if (val == 999999) break;
                
                if (val >= 2000 && val <= 2099)
                {
                    exp_years[target_idx] = val;
                    CmdLCD(CLEAR_LCD);
                    CmdLCD(GOTO_LINE1_POS0);
                    StrLCD("Year Updated!");
                    delay_ms(1500);
                    break;
                }
                
                CmdLCD(CLEAR_LCD);
                CmdLCD(GOTO_LINE1_POS0);
                StrLCD("Invalid Year!");
                delay_ms(1500);
            }
        }
    }
}

volatile u8 switch_pressed_flag = 0;

/**
 * @brief  Scan and identify the officer card.
 *         Returns 1 if officer card matches, 0 on cancel/timeout.
 */
u8 scan_officer_card(void)
{
    extern u8 is_same_id(const unsigned char *s1, const char *s2, u8 len);
    unsigned char card[9];
    u8 u_idx;
    
    rfid_ready = 0; // Reset scanner
    CmdLCD(CLEAR_LCD);
    CmdLCD(GOTO_LINE1_POS0);
    StrLCD("Scan Officer Card...");
    CmdLCD(GOTO_LINE4_POS0);
    StrLCD("*:Cancel");
    
    while (1)
    {
        if (auto_logout_flag) return 0;
        
        // Check if cancel was pressed
        if (ColScan() == 0)
        {
            char key = KeyScanWithTimeout(200);
            if (key == '*')
            {
                return 0;
            }
        }
        
        if (rfid_ready)
        {
            rfid_ready = 0;
            for (u_idx = 0; u_idx < 8; u_idx++) card[u_idx] = rfid_buffer[u_idx];
            card[8] = '\0';
            
            // Check if card is the Officer Card ("87654321")
            if (is_same_id(card, "12543380", 8))
            {
                return 1;
            }
            
            // Invalid
            CmdLCD(CLEAR_LCD);
            CmdLCD(GOTO_LINE1_POS0);
            StrLCD("Invalid Officer!");
            delay_ms(2000);
            
            // Prompt again
            CmdLCD(CLEAR_LCD);
            CmdLCD(GOTO_LINE1_POS0);
            StrLCD("Scan Officer Card...");
            CmdLCD(GOTO_LINE4_POS0);
            StrLCD("*:Cancel");
        }
        delay_ms(10);
    }
}

/**
 * @brief  Handle external switch press by verifying Officer Card and loading Officer Menu.
 */
void handle_external_switch(void)
{
    extern void officer_beep(void);
    
    // Sound buzzer for exactly 1 second on switch press
    IOSET0 = (1 << 19);  // Turn Buzzer ON (P0.19)
    delay_ms(1000);
    IOCLR0 = (1 << 19);  // Turn Buzzer OFF
    
    if (scan_officer_card())
    {
        officer_beep();
        
        // Open the Officer Menu
        officer_menu();
    }
    
    rtc_interrupted_flag = 1; // Mark that the screen needs to be redrawn
    CCR = 0x01;          // Ensure RTC is enabled when exiting editing
}

/**
 * @brief  EINT3 ISR handler on P0.30.
 *         Only triggers when outside citizen sessions. Sets flag to process in main loop context.
 */
void EINT3_ISR(void) __irq
{
    EXTINT = 0x08;       // Clear EINT3 interrupt flag (bit 3)
    
    // Ignore interrupt if a citizen user is logged in
    if (current_user_index >= 0 && current_user_index < NUM_USERS)
    {
        VICVectAddr = 0x00;  // End of Interrupt
        return;
    }
    
    switch_pressed_flag = 1;
    
    EXTINT = 0x08;       // Clear EINT3 interrupt flag (bit 3) again to clear any bounces
    VICVectAddr = 0x00;  // End of Interrupt
}

/**
 * @brief  Officer console options: Reset Voting database, System Time and Expiry edits.
 */
void officer_menu(void)
{
    char key;
    while (1)
    {
        CmdLCD(CLEAR_LCD);
        CmdLCD(GOTO_LINE1_POS0);
        StrLCD("1.Reset Voting");
        CmdLCD(GOTO_LINE2_POS0);
        StrLCD("2.Driving Lic Edit");
        CmdLCD(GOTO_LINE3_POS0);
        StrLCD("*:Exit");
        
        key = KeyScanWithTimeout(20000);
        if (key == 0 || key == '*' || auto_logout_flag)
        {
            break;
        }
        
        if (key == '1')
        {
            // Reset voting status for ALL 3 users in EEPROM
            ByteWrite_25LC512(eeprom_vote_addrs[0], 0x00); delay_ms(5);
            ByteWrite_25LC512(eeprom_vote_addrs[1], 0x00); delay_ms(5);
            ByteWrite_25LC512(eeprom_vote_addrs[2], 0x00); delay_ms(5);
            
            CmdLCD(CLEAR_LCD);
            CmdLCD(GOTO_LINE1_POS0);
            StrLCD("Voting Reset!");
            CmdLCD(GOTO_LINE2_POS0);
            StrLCD("Users can now vote");
            delay_ms(2000);
        }
        else if (key == '2')
        {
            // Sub-menu for Driving License Edit
            char sub_key;
            while (1)
            {
                CmdLCD(CLEAR_LCD);
                CmdLCD(GOTO_LINE1_POS0);
                StrLCD("1.System Time Edit");
                CmdLCD(GOTO_LINE2_POS0);
                StrLCD("2.License Edit");
                CmdLCD(GOTO_LINE3_POS0);
                StrLCD("*:Exit");
                
                sub_key = KeyScanWithTimeout(20000);
                if (sub_key == 0 || sub_key == '*' || auto_logout_flag)
                {
                    break;
                }
                
                if (sub_key == '1')
                {
                    rtc_edit_menu();
                    CCR = 0x01; // Enable clock to make it run!
                    while (ColScan() == 0) delay_ms(10); // Wait for key release before scanning parent menu
                }
                else if (sub_key == '2')
                {
                    int target = scan_target_user();
                    if (target != -1)
                    {
                        license_edit_menu(target);
                    }
                    while (ColScan() == 0) delay_ms(10); // Wait for key release before scanning parent menu
                }
            }
        }
    }
}
