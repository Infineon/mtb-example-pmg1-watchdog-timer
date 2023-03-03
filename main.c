/******************************************************************************
* File Name: main.c
*
* Description: This is the source code for PMG1 Watchdog timer Example
*              for ModusToolbox.
*
* Related Document: See README.md
*
*******************************************************************************
* Copyright 2022-2023, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/


/*******************************************************************************
 * Include header files
 ******************************************************************************/
#include "cybsp.h"
#include "cy_pdl.h"
#include <stdio.h>
#include <inttypes.h>

/*******************************************************************************
 * Macros
 ******************************************************************************/

/* Watchdog Timer (WDT) mode configuration: INTERUPT MODE/RESET MODE */
#define WDT_INTERRUPT_MODE        (0u)    /* Set the macro to '1u' to configure WDT as a periodic interrupt generator 
                                           * Set the macro to '0u' to enable WDT reset mode */

#define LED_DELAY_MS              (100u)
#define CY_ASSERT_FAILED          (0u)

/* The number of WDT ticks before a Match */
#define WDT_TICKS_COUNT           (65535u)    /* Set WDT_COUNTER MATCH value <= 65535 (0b1111111111111111) (16-bit up-counter) 
                                               * Whenever the WDT counter reaches the match value, a WDT interrupt is triggered 
                                               * This value can be used in fine adjustment of WDT interrupt period */

/* WDT ignore bits */
#define WDT_IGNORE_BITS           (0u)    /* Set the number of MSB bits to be ignored (0-15) from WDT_TICKS_COUNT (match value), in WDT_MATCH register 
                                           * This value can be used for course adjustment of WDT interrupt period 
                                           * For example, if WDT_IGNORE_BITS = 2, WDT becomes a 14-bit counter */

/* Assign WDT interrupt number and priority */
#define WDT_INTR_NUM              ((IRQn_Type) srss_interrupt_wdt_IRQn)
#define WDT_INTR_PRIORITY         (3u)

/* Debug print macro to enable UART print */
#define DEBUG_PRINT               (0u)

/* User switch interrupt configuration structure */
const cy_stc_sysint_t User_Switch_intr_config =
{
    .intrSrc = CYBSP_USER_SW_IRQ,       /* Source of interrupt signal */
    .intrPriority = 3u,                 /* Interrupt priority */
};

/* Flag to detect switch press event */
volatile uint8_t SwitchPressFlag;

/*******************************************************************************
* Function prototype
*******************************************************************************/
void User_Switch_Interrupt_Handler(void);

#if WDT_INTERRUPT_MODE

/* WDT interrupt configuration structure */
cy_stc_sysint_t wdtIntrConfig =
{
    .intrSrc      = WDT_INTR_NUM,
    .intrPriority = WDT_INTR_PRIORITY,
};

/*******************************************************************************
* Function prototype
*******************************************************************************/
void WDT_Isr(void);

#endif

#if DEBUG_PRINT

/* Structure for UART Context */
cy_stc_scb_uart_context_t CYBSP_UART_context;

/* Variable used for tracking the print status */
volatile bool ENTER_LOOP = true;

/*******************************************************************************
* Function Name: check_status
********************************************************************************
* Summary:
*  Prints the error message.
*
* Parameters:
*  error_msg - message to print if any error encountered.
*  status - status obtained after evaluation.
*
* Return:
*  void
*
*******************************************************************************/
void check_status(char *message, cy_rslt_t status)
{
    char error_msg[50];

    sprintf(error_msg, "Error Code: 0x%08" PRIX32 "\n", status);

    Cy_SCB_UART_PutString(CYBSP_UART_HW, "\r\n=====================================================\r\n");
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "\nFAIL: ");
    Cy_SCB_UART_PutString(CYBSP_UART_HW, message);
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "\r\n");
    Cy_SCB_UART_PutString(CYBSP_UART_HW, error_msg);
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "\r\n=====================================================\r\n");
}
#endif

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
*  System entrance point. This function performs
*  - initial setup of device
*  - initialize WDT and user switch interrupts
*  - configure WDT with match value and ignore bits
*  - toggle the user LED based on the reset cause
*
* Parameters:
*  none
*
* Return:
*  int
*
*******************************************************************************/

int main(void)
{
    cy_rslt_t result;
    cy_en_sysint_status_t intr_result;

    uint8_t i;
    SwitchPressFlag = 0;

    /* Initialize the device and board peripherals */
    result = cybsp_init();

    /* Board initialization failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        /* Insert error handler here */
        CY_ASSERT(CY_ASSERT_FAILED);
    }

#if DEBUG_PRINT

    /* Configure and enable the UART peripheral */
    result = Cy_SCB_UART_Init(CYBSP_UART_HW, &CYBSP_UART_config, &CYBSP_UART_context);
    Cy_SCB_UART_Enable(CYBSP_UART_HW);

    /* Sequence to clear screen */
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "\x1b[2J\x1b[;H");

    /* Print "Watchdog timer" */
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "****************** ");
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "PMG1 MCU: Watchdog timer");
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "****************** \r\n\n");
#endif

    /* Initialize user switch GPIO interrupt */
    intr_result = Cy_SysInt_Init(&User_Switch_intr_config, &User_Switch_Interrupt_Handler);
    if (intr_result != CY_SYSINT_SUCCESS)
    {
#if DEBUG_PRINT
        check_status("API Cy_SysInt_Init failed with error code", intr_result);
#endif
        /* Insert error handler here */
        CY_ASSERT(CY_ASSERT_FAILED);
    }

    /* Clear any pending interrupt and enable the User Switch Interrupt */
    NVIC_ClearPendingIRQ(User_Switch_intr_config.intrSrc);
    NVIC_EnableIRQ(User_Switch_intr_config.intrSrc);

#if WDT_INTERRUPT_MODE

    /* Hook interrupt service routine and enable interrupt */
    intr_result = Cy_SysInt_Init(&wdtIntrConfig, &WDT_Isr);
    if (intr_result != CY_SYSINT_SUCCESS)
    {
#if DEBUG_PRINT
        check_status("API Cy_SysInt_Init failed with error code", intr_result);
#endif
        /* Insert error handler here */
        CY_ASSERT(CY_ASSERT_FAILED);
    }

    /* Enable the WDT Interrupt */
    NVIC_EnableIRQ(WDT_INTR_NUM);
#endif


    /* Enable global interrupts */
    __enable_irq();

    /* Disable the system reset generation. After a device reset, reset by WDT is enabled by default. */
    Cy_WDT_Disable();

    /* Configure WDT: set the match value and ignore bits. */
    Cy_WDT_SetMatch(WDT_TICKS_COUNT);
    Cy_WDT_SetIgnoreBits(WDT_IGNORE_BITS);

    /* Clear all the pending WDT interrupts */
    Cy_WDT_ClearInterrupt();

#if WDT_INTERRUPT_MODE

    /* Unmask WDT interrupt */
    Cy_WDT_UnmaskInterrupt();

#else

    /* Enables the watchdog timer reset generation. */
    Cy_WDT_Enable();

#endif

    if(Cy_SysLib_GetResetReason() == CY_SYSLIB_RESET_HWWDT)
    {
        /* For loop to flash the user LED thrice in case of WDT reset */
        for(i=0;i<6;i++)
        {
            /* Toggle the user LED state */
            Cy_GPIO_Inv(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN);

            /* Delay for LED toggling */
            Cy_SysLib_Delay(LED_DELAY_MS);
         }
    }
    else
    {
        /* Toggle the user LED state */
        Cy_GPIO_Inv(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN);

        /* Delay for LED toggling */
        Cy_SysLib_Delay(LED_DELAY_MS);

        /* Toggle the user LED state */
        Cy_GPIO_Inv(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN);
    }

    for(;;)
    {
#if !WDT_INTERRUPT_MODE
        /* Check if switch is pressed */
        if(SwitchPressFlag)
        {
            while(1)
            {
                /* Infinite loop to simulate firmware failure */
            }
        }
        else
        {
            /* Clears the WatchDog to prevent device reset */
            Cy_WDT_ClearWatchdog();
        }
#endif

#if DEBUG_PRINT
        if (ENTER_LOOP)
        {
            Cy_SCB_UART_PutString(CYBSP_UART_HW, "Entered for loop\r\n");
            ENTER_LOOP = false;
        }
#endif

    }
}

/*******************************************************************************
* Function Name: WDT_Isr
********************************************************************************
*
* Summary:
* - This function is executed only for WDT INTERRUPT MODE, to assign a new match value to the Watchdog Timer (WDT).
* - It also toggles the user LED to indicate a WDT interrupt.
*
*******************************************************************************/
void WDT_Isr(void)
{
    /* Calculate new match value */
    uint16_t setValue = (uint16_t) Cy_WDT_GetCount() + (uint16_t) WDT_TICKS_COUNT;

    /* Update match value */
    Cy_WDT_SetMatch(setValue);

    /* Clear WDT interrupt */
    Cy_WDT_ClearInterrupt();

    /* Toggle the user LED state */
    Cy_GPIO_Inv(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN);
}

/*******************************************************************************
* Function Name: User_Switch_Interrupt_Handler
********************************************************************************
*
* Summary:
*  This function is executed when interrupt is triggered through the user switch press.
*
*******************************************************************************/
void User_Switch_Interrupt_Handler(void)
{
    /* Set Switch press flag to 1 to store the switch press event */
    SwitchPressFlag = 1;

    /* Clear the Interrupt */
    Cy_GPIO_ClearInterrupt(CYBSP_USER_SW_PORT, CYBSP_USER_SW_NUM);
}
/* [] END OF FILE */
