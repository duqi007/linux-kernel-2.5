/************************************************************************/
/* This module supports the iSeries PCI bus interrupt handling          */
/* Copyright (C) 20yy  <Robert L Holtorf> <IBM Corp>                    */
/*                                                                      */
/* This program is free software; you can redistribute it and/or modify */
/* it under the terms of the GNU General Public License as published by */
/* the Free Software Foundation; either version 2 of the License, or    */
/* (at your option) any later version.                                  */
/*                                                                      */
/* This program is distributed in the hope that it will be useful,      */ 
/* but WITHOUT ANY WARRANTY; without even the implied warranty of       */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        */
/* GNU General Public License for more details.                         */
/*                                                                      */
/* You should have received a copy of the GNU General Public License    */ 
/* along with this program; if not, write to the:                       */
/* Free Software Foundation, Inc.,                                      */ 
/* 59 Temple Place, Suite 330,                                          */ 
/* Boston, MA  02111-1307  USA                                          */
/************************************************************************/
/* Change Activity:                                                     */
/*   Created, December 13, 2000 by Wayne Holm                           */ 
/* End Change Activity                                                  */
/************************************************************************/

#include <linux/pci.h>
#include <linux/config.h>
#include <linux/init.h>
#include <linux/threads.h>
#include <linux/smp.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/bootmem.h>
#include <linux/blk.h>
#include <linux/ide.h>

#include <linux/irq.h>
#include <linux/spinlock.h>
#include <linux/slab.h>

#include <asm/iSeries/HvCallPci.h>
#include <asm/iSeries/HvCallXm.h>
#include <asm/iSeries/iSeries_irq.h>
#include <asm/iSeries/XmPciLpEvent.h>


hw_irq_controller iSeries_IRQ_handler = {
    "iSeries irq controller",
    iSeries_startup_IRQ,	/* startup */
    iSeries_shutdown_IRQ,	/* shutdown */
    iSeries_enable_IRQ,		/* enable */
    iSeries_disable_IRQ,	/* disable */
    NULL,			/* ack  */
    iSeries_end_IRQ,		/* end  */
    NULL			/* set_affinity */
};


struct iSeries_irqEntry {
    u32 dsa;
    struct iSeries_irqEntry* next;
};

struct iSeries_irqAnchor {
    u8  valid : 1;
    u8  reserved : 7;
    u16  entryCount;
    struct iSeries_irqEntry* head;
};

struct iSeries_irqAnchor iSeries_irqMap[NR_IRQS];

void iSeries_init_irqMap(int irq);

/*  This is called by init_IRQ.  set in ppc_md.init_IRQ by iSeries_setup.c */
void __init iSeries_init_IRQ(void)
{

	int i;

	for (i = 0; i < NR_IRQS; i++) {
		irq_desc[i].handler = &iSeries_IRQ_handler;
		irq_desc[i].status = 0;
		irq_desc[i].status |= IRQ_DISABLED;
		irq_desc[i].depth = 1;
		iSeries_init_irqMap(i);
	}

	/* Register PCI event handler and open an event path */
	XmPciLpEvent_init();

	return;
}

/*  Called by iSeries_init_IRQ */
void __init iSeries_init_irqMap(int irq) {
   /* Prevent IRQs 0 and 255 from being used.  IRQ 0 appears in
      uninitialized devices.  IRQ 255 appears in the PCI interrupt
      line register if a PCI error occurs */
    iSeries_irqMap[irq].valid = (irq == 0 || irq == 255)? 0 : 1;
    iSeries_irqMap[irq].entryCount = 0;
    iSeries_irqMap[irq].head = NULL;
}

/* This is called out of iSeries_scan_slot to allocate an IRQ for an EADS slot */
int __init iSeries_allocate_IRQ(HvBusNumber busNumber, HvSubBusNumber subBusNumber, HvAgentId deviceId) {

    u8 idsel = (deviceId >> 4);
    u8 function = deviceId & 0x0F;
    int irq = ((((busNumber-1)*16 + (idsel-1)*8 + function)*9/8) % 254) + 1; 
    return irq;
}

/* This is called out of iSeries_scan_slot to assign the EADS slot to its IRQ number */
int __init iSeries_assign_IRQ(int irq, HvBusNumber busNumber, HvSubBusNumber subBusNumber, HvAgentId deviceId) {

    int rc;
    u32 dsa = (busNumber << 16) | (subBusNumber << 8) | deviceId;
    struct iSeries_irqEntry* newEntry;
    unsigned long flags;

    if (irq < 0 || irq >= NR_IRQS)
	return -1;

    newEntry = kmalloc(sizeof(*newEntry), GFP_KERNEL);
    if (newEntry == NULL)
	return -ENOMEM;
    newEntry->dsa = dsa;
    newEntry->next = NULL;

    /* Probably not necessary to lock the irq since allocation is only
       done during buswalk, but it should not hurt anything except a little
       performance */
    spin_lock_irqsave(&irq_desc[irq].lock, flags);

    if (iSeries_irqMap[irq].valid) {
        /* Push the new element onto the irq stack */
	newEntry->next = iSeries_irqMap[irq].head;
	iSeries_irqMap[irq].head = newEntry;
	++iSeries_irqMap[irq].entryCount;
	rc = 0;
    }
    else
	rc = -1;

    spin_unlock_irqrestore(&irq_desc[irq].lock, flags);

    if (rc != 0 && newEntry)
	kfree(newEntry);

    return rc;

}


/* This is called by iSeries_activate_IRQs */
unsigned int iSeries_startup_IRQ(unsigned int irq) {
    struct iSeries_irqEntry* entry;
    u32 bus, subBus, deviceId, function, mask;

    /* irq should be locked by the caller */

    for(entry=iSeries_irqMap[irq].head; entry!=NULL; entry=entry->next) {
	bus = (entry->dsa >> 16) & 0xFFFF;
	subBus = (entry->dsa >> 8) & 0xFF;
	deviceId = entry->dsa & 0xFF;
	function = deviceId & 0x0F;
	/* Link the IRQ number to the bridge */
	HvCallXm_connectBusUnit(bus, subBus, deviceId, irq);
        /* Unmask bridge interrupts in the FISR */
	mask = 0x01010000 << function;
	HvCallPci_unmaskFisr(bus, subBus, deviceId, mask);
    }

    return 0;
}

/* This is called out of iSeries_fixup to
   activate interrupt generation for usable slots */
void __init iSeries_activate_IRQs() {
    int irq;
    unsigned long flags;

    for (irq=0; irq < NR_IRQS; irq++) {
	spin_lock_irqsave(&irq_desc[irq].lock, flags);
	irq_desc[irq].handler->startup(irq);
	spin_unlock_irqrestore(&irq_desc[irq].lock, flags);
    }

}

/*  this is not called anywhere currently */
void iSeries_shutdown_IRQ(unsigned int irq) {
    struct iSeries_irqEntry* entry;
    u32 bus, subBus, deviceId, function, mask;

    /* irq should be locked by the caller */

    for(entry=iSeries_irqMap[irq].head; entry; entry=entry->next) {
	bus = (entry->dsa >> 16) & 0xFFFF;
	subBus = (entry->dsa >> 8) & 0xFF;
	deviceId = entry->dsa & 0xFF;
	function = deviceId & 0x0F;
	/* Invalidate the IRQ number in the bridge */
	HvCallXm_connectBusUnit(bus, subBus, deviceId, 0);
        /* Mask bridge interrupts in the FISR */
	mask = 0x01010000 << function;
	HvCallPci_maskFisr(bus, subBus, deviceId, mask);
    }

}


/* This will be called by device drivers (via disable_IRQ to disable
   INTA in the bridge interrupt status register */
void iSeries_disable_IRQ(unsigned int irq) {
    struct iSeries_irqEntry* entry;
    u32 bus, subBus, deviceId, mask;

    /* The IRQ has already been locked by the caller */

    for(entry=iSeries_irqMap[irq].head; entry; entry=entry->next) {
	bus = (entry->dsa >> 16) & 0xFFFF;
	subBus = (entry->dsa >> 8) & 0xFF;
	deviceId = entry->dsa & 0xFF;
        /* Mask secondary INTA   */
	mask = 0x80000000;
	HvCallPci_maskInterrupts(bus, subBus, deviceId, mask);
    }
}

/* This will be called by device drivers (via enable_IRQ to enable
   INTA in the bridge interrupt status register */
void iSeries_enable_IRQ(unsigned int irq) {
    struct iSeries_irqEntry* entry;
    u32 bus, subBus, deviceId, mask;

    /* The IRQ has already been locked by the caller */

    for(entry=iSeries_irqMap[irq].head; entry; entry=entry->next) {
	bus = (entry->dsa >> 16) & 0xFFFF;
	subBus = (entry->dsa >> 8) & 0xFF;
	deviceId = entry->dsa & 0xFF;
	/* Unmask secondary INTA */
	mask = 0x80000000;
	HvCallPci_unmaskInterrupts(bus, subBus, deviceId, mask);
    }
}

/* Need to define this so ppc_irq_dispatch_handler will NOT call
   enable_IRQ at the end of interrupt handling.  However, this
   does nothing because there is not enough information provided
   to do the EOI HvCall.  This is done by XmPciLpEvent.c */
void iSeries_end_IRQ(unsigned int irq) {
}

