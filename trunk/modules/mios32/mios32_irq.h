// $Id$
/*
 * This file collects all interrupt priorities
 *
 * ==========================================================================
 *
 *  Copyright (C) 2008 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

#ifndef _MIOS32_IRQ_H
#define _MIOS32_IRQ_H

// we are using 4 bits for pre-emption priority, and no bits for subpriority
// this means: subpriority can always be set to 0, therefore no special 
// define is available for thi setting
#define MIOS32_IRQ_PRIGROUP    NVIC_PriorityGroup_4


// than lower the value, than higher the priority!

// note that FreeRTOS allows priority level < 10 for "SysCalls"
// means: FreeRTOS tasks can be interrupted by level<10 IRQs


// DMA Channel IRQ used by MIOS32_SRIO, called each mS
#define MIOS32_IRQ_SRIO_DMA_PRIORITY    5


// DMA Channel IRQ used by MIOS32_AIN, called after 
// all ADC channels have been converted
#define MIOS32_IRQ_AIN_DMA_PRIORITY     5


// IIC IRQs used by MIOS32_IIC, called rarely on IIC accesses
// should be very high to overcome peripheral flaws (see header of mios32_iic.c)
// estimated requirement for "reaction time": less than 9/400 kHz = 22.5 uS
// EV and ER IRQ should have same priority since they are sharing resources
#define MIOS32_IRQ_IIC_EV_PRIORITY      2
#define MIOS32_IRQ_IIC_ER_PRIORITY      2


// UART IRQs used by MIOS32_UART
// typically called each 320 mS if full MIDI bandwidth is used
// priority should be high to avoid data loss
#define MIOS32_IRQ_UART_PRIORITY        3


#endif /* _MIOS32_IRQ_H */
