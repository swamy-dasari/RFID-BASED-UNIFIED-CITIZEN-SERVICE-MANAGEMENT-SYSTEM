/******************************************************************************
 * @file    rfid_test.c
 * @brief   Main entry point, system initialization, and RFID citizen detection.
 * @author  RFID Pair Programming Team
 * @date    July 2026
 ******************************************************************************/

#include <lpc21xx.h>
#include "types.h"
#include "lcd_defines.h"
#include "kpm.h"
#include "spi_defines.h"
#include "spi_eeprom_defines.h"
#include "uart0.h"
#include "rfid.h"
#include "delay.h"
#include "user_menu.h"

// Pin configuration macros (P0.19, P0.20, P0.21)
#define BUZZER_PIN 19
#define RED_LED_PIN 20
#define GREEN_LED_PIN 21

/**
 * @brief  Sound three short beeps to indicate success.
 */
void success_blink(void)
{
    u8 i;
    for (i = 0; i < 3; i++)
    {
        IOSET0 = 1 << GREEN_LED_PIN;
        delay_ms(150);
        IOCLR0 = 1 << GREEN_LED_PIN;
        delay_ms(150);
    }
}

/**
 * @brief  Sound buzzer and blink red LED to indicate error.
 */
void error_alarm(void)
{
    u8 i;
    for (i = 0; i < 3; i++)
    {
        IOSET0 = (1 << RED_LED_PIN) | (1 << BUZZER_PIN);
        delay_ms(150);
        IOCLR0 = (1 << RED_LED_PIN) | (1 << BUZZER_PIN);
        delay_ms(150);
    }
}

/**
 * @brief  Beep once for officer entry.
 */
void officer_beep(void)
{
    IOSET0 = (1 << GREEN_LED_PIN) | (1 << BUZZER_PIN);
    delay_ms(400);
    IOCLR0 = (1 << GREEN_LED_PIN) | (1 << BUZZER_PIN);
}

/**
 * @brief  Configure LPC2148 peripherals: LCD, Keypad, UART0, SPI0, RTC, EINT3.
 */
void System_Init(void)
{
    // 1. Initialize LCD and matrix Keypad
    InitLCD();
    InitKPM();
    
    // 2. Initialize UART0 (for RFID Reader) and SPI0 (for EEPROM)
    UART0_Init();
    Init_SPI0();
    check_eeprom_init();
    
    // 3. Initialize built-in RTC clock (by default OFF)
    if (YEAR < 2000 || YEAR > 2099)
    {
        // Set default date if RTC values are invalid/uninitialized
        CCR = 0x02; // Disable RTC and reset CTC
        SEC = 0;
        MIN = 0;
        HOUR = 12;
        DOM = 26;
        MONTH = 6;
        YEAR = 2026;
        update_rtc_dow(); // Set default day of week (Friday)
    }
    CCR = 0x01; // Enable RTC clock by default to run automatically
    
    // 4. Configure GPIO for LEDs & Buzzer as outputs
    IODIR0 |= (1 << BUZZER_PIN) | (1 << RED_LED_PIN) | (1 << GREEN_LED_PIN);
    IOCLR0 = (1 << BUZZER_PIN) | (1 << RED_LED_PIN) | (1 << GREEN_LED_PIN); // Turn off
    
    // 5. Configure P0.30 as External Interrupt 3 (EINT3) for RTC Editing
    PINSEL1 &= ~0x30000000; // Clear bits 29:28 for P0.30
    PINSEL1 |= 0x20000000;  // Set bits 29:28 to 10 for EINT3
    
    EXTMODE |= 0x08;        // EINT3 edge-sensitive (bit 3)
    EXTPOLAR &= ~0x08;      // EINT3 falling-edge active (bit 3)
    EXTINT = 0x08;          // Clear existing interrupt flag
    
    // 6. Set up VIC (Vectored Interrupt Controller) for EINT3 (Source 17)
    VICVectAddr4 = (unsigned long)EINT3_ISR;
    VICVectCntl4 = 0x20 | 17; // Enable slot 4, map to source 17
    VICIntEnable = 1 << 17;   // Enable EINT3 interrupt
    
    // 7. Set up VIC (Vectored Interrupt Controller) for UART0 (Source 6)
    VICVectAddr5 = (unsigned long)UART0_ISR;
    VICVectCntl5 = 0x20 | 6;  // Enable slot 5, map to source 6
    VICIntEnable |= 1 << 6;   // Enable UART0 interrupt
}

/**
 * @brief  Compare string equality for 8-byte RFID IDs.
 */
u8 is_same_id(const unsigned char *s1, const char *s2, u8 len)
{
    u8 i;
    for (i = 0; i < len; i++)
    {
        if (s1[i] != s2[i]) return 0;
    }
    return 1;
}

int main(void)
{
    unsigned char card[9];
    u8 u_idx;
    u8 found;
    extern volatile u8 rtc_interrupted_flag;
    
    System_Init();
    
    // Switch CPU mode to User Mode (0x10) AFTER initialization
    __asm
    {
        msr cpsr_c, #0x10
    }
    
    // Welcome Screen Dashboard
    CmdLCD(CLEAR_LCD);
    CmdLCD(GOTO_LINE1_POS0);
    StrLCD("********************");
    CmdLCD(GOTO_LINE2_POS0);
    StrLCD("    RFID UNIFIED    ");
    CmdLCD(GOTO_LINE3_POS0);
    StrLCD("   CITIZEN SYSTEM   ");
    CmdLCD(GOTO_LINE4_POS0);
    StrLCD("********************");
    delay_ms(2500);
    
    while (1)
    {
        current_user_index = -1; // Clear active user index
        
        {
            extern volatile u8 auto_logout_flag;
            auto_logout_flag = 0;
        }
        
        // Ensure buzzer and LEDs are OFF before any card is scanned
        IOCLR0 = (1 << BUZZER_PIN) | (1 << RED_LED_PIN) | (1 << GREEN_LED_PIN);
        
        CmdLCD(CLEAR_LCD);
        CmdLCD(GOTO_LINE1_POS0);
        StrLCD("--------------------");
        CmdLCD(GOTO_LINE2_POS0);
        StrLCD(" RFID CITIZEN CARD  ");
        CmdLCD(GOTO_LINE3_POS0);
        StrLCD("Waiting for scan...");
        CmdLCD(GOTO_LINE4_POS0);
        StrLCD("--------------------");
        
        while (!rfid_ready)
        {
            if (rtc_interrupted_flag)
            {
                rtc_interrupted_flag = 0;
                CmdLCD(CLEAR_LCD);
                CmdLCD(GOTO_LINE1_POS0);
                StrLCD("--------------------");
                CmdLCD(GOTO_LINE2_POS0);
                StrLCD(" RFID CITIZEN CARD  ");
                CmdLCD(GOTO_LINE3_POS0);
                StrLCD("Waiting for scan...");
                CmdLCD(GOTO_LINE4_POS0);
                StrLCD("--------------------");
            }
            if (switch_pressed_flag)
            {
                switch_pressed_flag = 0;
                handle_external_switch();
            }
            delay_ms(5);
        }
        
        rfid_ready = 0;
        for (u_idx = 0; u_idx < 8; u_idx++) card[u_idx] = rfid_buffer[u_idx];
        card[8] = '\0';
        
        found = 0;
        // Scan user database for a matching card ID
        for (u_idx = 0; u_idx < NUM_USERS; u_idx++)
        {
            if (is_same_id(card, card_ids[u_idx], 8))
            {
                current_user_index = u_idx;
                found = 1;
                break;
            }
        }
        
        if (found)
        {
            CmdLCD(CLEAR_LCD);
            CmdLCD(GOTO_LINE1_POS0);
            StrLCD("Card Valid!");
            CmdLCD(GOTO_LINE2_POS0);
            StrLCD("Welcome: ");
            CmdLCD(GOTO_LINE3_POS0);
            StrLCD((s8 *)names[current_user_index]);
            
            // Transmit character constant, string constant, and integers via UART
            UART0_SendString("\r\nCard: ");
            UART0_SendString(names[current_user_index]);
            UART0_SendString(" [Valid ID: ");
            UART0_SendInteger(current_user_index + 1);
            UART0_SendString("]\r\n");
            
            // Turn on Green LED, keep Red LED and Buzzer off
            IOSET0 = 1 << GREEN_LED_PIN;
            IOCLR0 = (1 << RED_LED_PIN) | (1 << BUZZER_PIN);
            
            delay_ms(2500); // Turn on Green LED for a few seconds
            IOCLR0 = 1 << GREEN_LED_PIN; // Let it go OFF
            
            // Proceed directly to user menu
            user_menu();
        }
        else if (is_same_id(card, "12543380", 8)) // Officer Card
        {
            CmdLCD(CLEAR_LCD);
            CmdLCD(GOTO_LINE1_POS0);
            StrLCD("Officer Card!");
            CmdLCD(GOTO_LINE2_POS0);
            StrLCD("Welcome Officer");
            
            UART0_SendString("\r\nOfficer Card Swiped!\r\n");
            
            officer_beep();
            delay_ms(1000);
            
            // Open Officer Menu
            officer_menu();
        }
        else
        {
            CmdLCD(CLEAR_LCD);
            CmdLCD(GOTO_LINE1_POS0);
            StrLCD("Invalid Card!");
            CmdLCD(GOTO_LINE2_POS0);
            StrLCD("ID: ");
            StrLCD((s8 *)card);
            
            UART0_SendString("\r\nInvalid Card Access Blocked: ");
            UART0_SendString((s8 *)card);
            UART0_SendString("\r\n");
            
            // Turn on Red LED and Buzzer, keep Green LED off
            IOSET0 = (1 << RED_LED_PIN) | (1 << BUZZER_PIN);
            IOCLR0 = 1 << GREEN_LED_PIN;
            
            delay_ms(2500); // Hold invalid indicators active on screen
        }
    }
}
