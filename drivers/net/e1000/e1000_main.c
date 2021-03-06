/*******************************************************************************

  This software program is available to you under a choice of one of two
  licenses. You may choose to be licensed under either the GNU General Public
  License 2.0, June 1991, available at http://www.fsf.org/copyleft/gpl.html,
  or the Intel BSD + Patent License, the text of which follows:
  
  Recipient has requested a license and Intel Corporation ("Intel") is willing
  to grant a license for the software entitled Linux Base Driver for the
  Intel(R) PRO/1000 Family of Adapters (e1000) (the "Software") being provided
  by Intel Corporation. The following definitions apply to this license:
  
  "Licensed Patents" means patent claims licensable by Intel Corporation which
  are necessarily infringed by the use of sale of the Software alone or when
  combined with the operating system referred to below.
  
  "Recipient" means the party to whom Intel delivers this Software.
  
  "Licensee" means Recipient and those third parties that receive a license to
  any operating system available under the GNU General Public License 2.0 or
  later.
  
  Copyright (c) 1999 - 2002 Intel Corporation.
  All rights reserved.
  
  The license is provided to Recipient and Recipient's Licensees under the
  following terms.
  
  Redistribution and use in source and binary forms of the Software, with or
  without modification, are permitted provided that the following conditions
  are met:
  
  Redistributions of source code of the Software may retain the above
  copyright notice, this list of conditions and the following disclaimer.
  
  Redistributions in binary form of the Software may reproduce the above
  copyright notice, this list of conditions and the following disclaimer in
  the documentation and/or materials provided with the distribution.
  
  Neither the name of Intel Corporation nor the names of its contributors
  shall be used to endorse or promote products derived from this Software
  without specific prior written permission.
  
  Intel hereby grants Recipient and Licensees a non-exclusive, worldwide,
  royalty-free patent license under Licensed Patents to make, use, sell, offer
  to sell, import and otherwise transfer the Software, if any, in source code
  and object code form. This license shall include changes to the Software
  that are error corrections or other minor changes to the Software that do
  not add functionality or features when the Software is incorporated in any
  version of an operating system that has been distributed under the GNU
  General Public License 2.0 or later. This patent license shall apply to the
  combination of the Software and any operating system licensed under the GNU
  General Public License 2.0 or later if, at the time Intel provides the
  Software to Recipient, such addition of the Software to the then publicly
  available versions of such operating systems available under the GNU General
  Public License 2.0 or later (whether in gold, beta or alpha form) causes
  such combination to be covered by the Licensed Patents. The patent license
  shall not apply to any other combinations which include the Software. NO
  hardware per se is licensed hereunder.
  
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MECHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR IT CONTRIBUTORS BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  ANY LOSS OF USE; DATA, OR PROFITS; OR BUSINESS INTERUPTION) HOWEVER CAUSED
  AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY OR
  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

#define __E1000_MAIN__
#include "e1000.h"

char e1000_driver_name[] = "e1000";
char e1000_driver_string[] = "Intel(R) PRO/1000 Network Driver";
char e1000_driver_version[] = "4.2.8";
char e1000_copyright[] = "Copyright (c) 1999-2002 Intel Corporation.";

/* e1000_pci_tbl - PCI Device ID Table
 *
 * Private driver_data field (last one) stores an index into e1000_strings
 * Wildcard entries (PCI_ANY_ID) should come last
 * Last entry must be all 0s
 *
 * { Vendor ID, Device ID, SubVendor ID, SubDevice ID,
 *   Class, Class Mask, String Index }
 */
static struct pci_device_id e1000_pci_tbl[] __devinitdata = {
	/* Intel(R) PRO/1000 Network Connection */
	{0x8086, 0x1000, 0x8086, 0x1000, 0, 0, 0},
	{0x8086, 0x1001, 0x8086, 0x1003, 0, 0, 0},
	{0x8086, 0x1004, 0x8086, 0x1004, 0, 0, 0},
	{0x8086, 0x1008, 0x8086, 0x1107, 0, 0, 0},
	{0x8086, 0x1009, 0x8086, 0x1109, 0, 0, 0},
	{0x8086, 0x100C, 0x8086, 0x1112, 0, 0, 0},
	{0x8086, 0x100E, 0x8086, 0x001E, 0, 0, 0},
	/* Compaq Gigabit Ethernet Server Adapter */
	{0x8086, 0x1000, 0x0E11, PCI_ANY_ID, 0, 0, 1},
	{0x8086, 0x1001, 0x0E11, PCI_ANY_ID, 0, 0, 1},
	{0x8086, 0x1004, 0x0E11, PCI_ANY_ID, 0, 0, 1},
	/* IBM Mobile, Desktop & Server Adapters */
	{0x8086, 0x1000, 0x1014, PCI_ANY_ID, 0, 0, 2},
	{0x8086, 0x1001, 0x1014, PCI_ANY_ID, 0, 0, 2},
	{0x8086, 0x1004, 0x1014, PCI_ANY_ID, 0, 0, 2},
	/* Generic */
	{0x8086, 0x1000, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{0x8086, 0x1001, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{0x8086, 0x1004, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{0x8086, 0x1008, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{0x8086, 0x1009, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{0x8086, 0x100C, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{0x8086, 0x100D, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{0x8086, 0x100E, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	/* required last entry */
	{0,}
};

MODULE_DEVICE_TABLE(pci, e1000_pci_tbl);

static char *e1000_strings[] = {
	"Intel(R) PRO/1000 Network Connection",
	"Compaq Gigabit Ethernet Server Adapter",
	"IBM Mobile, Desktop & Server Adapters"
};

/* e1000_main.c Function Prototypes */

int e1000_up(struct e1000_adapter *adapter);
void e1000_down(struct e1000_adapter *adapter);
void e1000_reset(struct e1000_adapter *adapter);

static int e1000_init_module(void);
static void e1000_exit_module(void);
static int e1000_probe(struct pci_dev *pdev, const struct pci_device_id *ent);
static void e1000_remove(struct pci_dev *pdev);
static void e1000_sw_init(struct e1000_adapter *adapter);
static int e1000_open(struct net_device *netdev);
static int e1000_close(struct net_device *netdev);
static int e1000_setup_tx_resources(struct e1000_adapter *adapter);
static int e1000_setup_rx_resources(struct e1000_adapter *adapter);
static void e1000_configure_tx(struct e1000_adapter *adapter);
static void e1000_configure_rx(struct e1000_adapter *adapter);
static void e1000_setup_rctl(struct e1000_adapter *adapter);
static void e1000_clean_tx_ring(struct e1000_adapter *adapter);
static void e1000_clean_rx_ring(struct e1000_adapter *adapter);
static void e1000_free_tx_resources(struct e1000_adapter *adapter);
static void e1000_free_rx_resources(struct e1000_adapter *adapter);
static void e1000_set_multi(struct net_device *netdev);
static void e1000_update_phy_info(unsigned long data);
static void e1000_watchdog(unsigned long data);
static int e1000_xmit_frame(struct sk_buff *skb, struct net_device *netdev);
static void e1000_tx_timeout(struct net_device *dev);
static struct net_device_stats * e1000_get_stats(struct net_device *netdev);
static int e1000_change_mtu(struct net_device *netdev, int new_mtu);
static int e1000_set_mac(struct net_device *netdev, void *p);
static void e1000_update_stats(struct e1000_adapter *adapter);
static inline void e1000_irq_disable(struct e1000_adapter *adapter);
static inline void e1000_irq_enable(struct e1000_adapter *adapter);
static void e1000_intr(int irq, void *data, struct pt_regs *regs);
static void e1000_clean_tx_irq(struct e1000_adapter *adapter);
static void e1000_clean_rx_irq(struct e1000_adapter *adapter);
static void e1000_alloc_rx_buffers(struct e1000_adapter *adapter);
static int e1000_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd);
static void e1000_enter_82542_rst(struct e1000_adapter *adapter);
static void e1000_leave_82542_rst(struct e1000_adapter *adapter);
static inline void e1000_rx_checksum(struct e1000_adapter *adapter,
                                     struct e1000_rx_desc *rx_desc,
                                     struct sk_buff *skb);
void e1000_enable_WOL(struct e1000_adapter *adapter);
#ifdef NETIF_F_HW_VLAN_TX
static void e1000_vlan_rx_register(struct net_device *netdev, struct vlan_group *grp);
static void e1000_vlan_rx_add_vid(struct net_device *netdev, uint16_t vid);
static void e1000_vlan_rx_kill_vid(struct net_device *netdev, uint16_t vid);
#endif

/* Exported from other modules */

extern void e1000_check_options(struct e1000_adapter *adapter);
extern void e1000_proc_dev_setup(struct e1000_adapter *adapter);
extern void e1000_proc_dev_free(struct e1000_adapter *adapter);
extern int e1000_ethtool_ioctl(struct net_device *netdev, struct ifreq *ifr);

static struct pci_driver e1000_driver = {
	name:     e1000_driver_name,
	id_table: e1000_pci_tbl,
	probe:    e1000_probe,
	remove:   __devexit_p(e1000_remove),
	/* Power Managment Hooks */
	suspend:  NULL,
	resume:   NULL
};

MODULE_AUTHOR("Intel Corporation, <linux.nics@intel.com>");
MODULE_DESCRIPTION("Intel(R) PRO/1000 Network Driver");
MODULE_LICENSE("Dual BSD/GPL");
EXPORT_NO_SYMBOLS;

/**
 * e1000_init_module - Driver Registration Routine
 *
 * e1000_init_module is the first routine called when the driver is
 * loaded. All it does is register with the PCI subsystem.
 **/

static int __init
e1000_init_module(void)
{
	printk(KERN_INFO "%s - version %s\n",
	       e1000_driver_string, e1000_driver_version);

	printk(KERN_INFO "%s\n", e1000_copyright);

	return pci_module_init(&e1000_driver);
}

module_init(e1000_init_module);

/**
 * e1000_exit_module - Driver Exit Cleanup Routine
 *
 * e1000_exit_module is called just before the driver is removed
 * from memory.
 **/

static void __exit
e1000_exit_module(void)
{
	pci_unregister_driver(&e1000_driver);
}

module_exit(e1000_exit_module);


int
e1000_up(struct e1000_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;

	if(request_irq(netdev->irq, &e1000_intr, SA_SHIRQ,
	               netdev->name, netdev))
		return -1;

	/* hardware has been reset, we need to reload some things */

	e1000_set_multi(netdev);

	e1000_configure_tx(adapter);
	e1000_setup_rctl(adapter);
	e1000_configure_rx(adapter);
	e1000_alloc_rx_buffers(adapter);

	mod_timer(&adapter->watchdog_timer, jiffies);
	e1000_irq_enable(adapter);

	return 0;
}

void
e1000_down(struct e1000_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;

	e1000_irq_disable(adapter);
	free_irq(netdev->irq, netdev);
	del_timer_sync(&adapter->watchdog_timer);
	del_timer_sync(&adapter->phy_info_timer);
	adapter->link_speed = 0;
	adapter->link_duplex = 0;
	netif_carrier_off(netdev);
	netif_stop_queue(netdev);

	e1000_reset(adapter);
	e1000_clean_tx_ring(adapter);
	e1000_clean_rx_ring(adapter);
}

void
e1000_reset(struct e1000_adapter *adapter)
{
	/* Repartition Pba for greater than 9k mtu
	 * To take effect CTRL.RST is required.
	 */

	if(adapter->rx_buffer_len > E1000_RXBUFFER_8192)
		E1000_WRITE_REG(&adapter->hw, PBA, E1000_JUMBO_PBA);
	else
		E1000_WRITE_REG(&adapter->hw, PBA, E1000_DEFAULT_PBA);

	adapter->hw.fc = adapter->hw.original_fc;
	e1000_reset_hw(&adapter->hw);
	e1000_init_hw(&adapter->hw);
	e1000_reset_adaptive(&adapter->hw);
	e1000_phy_get_info(&adapter->hw, &adapter->phy_info);

	e1000_enable_WOL(adapter);
}

/**
 * e1000_probe - Device Initialization Routine
 * @pdev: PCI device information struct
 * @ent: entry in e1000_pci_tbl
 *
 * Returns 0 on success, negative on failure
 *
 * e1000_probe initializes an adapter identified by a pci_dev structure.
 * The OS initialization, configuring of the adapter private structure,
 * and a hardware reset occur.
 **/

static int __devinit
e1000_probe(struct pci_dev *pdev,
            const struct pci_device_id *ent)
{
	struct net_device *netdev;
	struct e1000_adapter *adapter;
	static int cards_found = 0;
	unsigned long mmio_start;
	int mmio_len;
	int pci_using_dac;
	int i;

	if((i = pci_enable_device(pdev)))
		return i;

	if(!(i = pci_set_dma_mask(pdev, (u64) 0xffffffffffffffff))) {
		pci_using_dac = 1;
	} else {
		if((i = pci_set_dma_mask(pdev, (u64) 0xffffffff))) {
			E1000_ERR("No usable DMA configuration, aborting\n");
			return i;
		}
		pci_using_dac = 0;
	}

	if((i = pci_request_regions(pdev, e1000_driver_name)))
		return i;

	pci_set_master(pdev);

	netdev = alloc_etherdev(sizeof(struct e1000_adapter));
	if(!netdev)
		goto err_alloc_etherdev;

	SET_MODULE_OWNER(netdev);

	pci_set_drvdata(pdev, netdev);
	adapter = netdev->priv;
	adapter->netdev = netdev;
	adapter->pdev = pdev;
	adapter->hw.back = adapter;

	mmio_start = pci_resource_start(pdev, BAR_0);
	mmio_len = pci_resource_len(pdev, BAR_0);

	adapter->hw.hw_addr = ioremap(mmio_start, mmio_len);
	if(!adapter->hw.hw_addr)
		goto err_ioremap;

	netdev->open = &e1000_open;
	netdev->stop = &e1000_close;
	netdev->hard_start_xmit = &e1000_xmit_frame;
	netdev->get_stats = &e1000_get_stats;
	netdev->set_multicast_list = &e1000_set_multi;
	netdev->set_mac_address = &e1000_set_mac;
	netdev->change_mtu = &e1000_change_mtu;
	netdev->do_ioctl = &e1000_ioctl;
	netdev->tx_timeout = &e1000_tx_timeout;
	netdev->watchdog_timeo = HZ;
#ifdef NETIF_F_HW_VLAN_TX
	netdev->vlan_rx_register = e1000_vlan_rx_register;
	netdev->vlan_rx_add_vid = e1000_vlan_rx_add_vid;
	netdev->vlan_rx_kill_vid = e1000_vlan_rx_kill_vid;
#endif

	netdev->irq = pdev->irq;
	netdev->base_addr = mmio_start;

	adapter->bd_number = cards_found;
	adapter->id_string = e1000_strings[ent->driver_data];

	/* setup the private structure */

	e1000_sw_init(adapter);

	if(adapter->hw.mac_type >= e1000_82543) {
#ifdef NETIF_F_HW_VLAN_TX
		netdev->features = NETIF_F_SG |
			           NETIF_F_HW_CSUM |
		       	           NETIF_F_HW_VLAN_TX |
		                   NETIF_F_HW_VLAN_RX |
				   NETIF_F_HW_VLAN_FILTER;
#else
		netdev->features = NETIF_F_SG | NETIF_F_HW_CSUM;
#endif
	} else {
		netdev->features = NETIF_F_SG;
	}

	if(pci_using_dac)
		netdev->features |= NETIF_F_HIGHDMA;

	/* make sure the EEPROM is good */

	if(e1000_validate_eeprom_checksum(&adapter->hw) < 0)
		goto err_eeprom;

	/* copy the MAC address out of the EEPROM */

	e1000_read_mac_addr(&adapter->hw);
	memcpy(netdev->dev_addr, adapter->hw.mac_addr, netdev->addr_len);

	if(!is_valid_ether_addr(netdev->dev_addr))
		goto err_eeprom;

	e1000_read_part_num(&adapter->hw, &(adapter->part_num));

	e1000_get_bus_info(&adapter->hw);

	if((adapter->hw.mac_type == e1000_82544) &&
	   (adapter->hw.bus_type == e1000_bus_type_pcix))

		adapter->max_data_per_txd = 4096;
	else
		adapter->max_data_per_txd = MAX_JUMBO_FRAME_SIZE;


	init_timer(&adapter->watchdog_timer);
	adapter->watchdog_timer.function = &e1000_watchdog;
	adapter->watchdog_timer.data = (unsigned long) adapter;

	init_timer(&adapter->phy_info_timer);
	adapter->phy_info_timer.function = &e1000_update_phy_info;
	adapter->phy_info_timer.data = (unsigned long) adapter;

	register_netdev(netdev);

	/* we're going to reset, so assume we have no link for now */
	
	netif_carrier_off(netdev);
	netif_stop_queue(netdev);

	printk(KERN_INFO "%s: %s\n", netdev->name, adapter->id_string);
	e1000_check_options(adapter);
	e1000_proc_dev_setup(adapter);

	/* reset the hardware with the new settings */

	e1000_reset(adapter);

	cards_found++;
	return 0;

err_eeprom:
	iounmap(adapter->hw.hw_addr);
err_ioremap:
	pci_release_regions(pdev);
	kfree(netdev);
err_alloc_etherdev:
	return -ENOMEM;
}

/**
 * e1000_remove - Device Removal Routine
 * @pdev: PCI device information struct
 *
 * e1000_remove is called by the PCI subsystem to alert the driver
 * that it should release a PCI device.  The could be caused by a
 * Hot-Plug event, or because the driver is going to be removed from
 * memory.
 **/

static void __devexit
e1000_remove(struct pci_dev *pdev)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct e1000_adapter *adapter = netdev->priv;

	unregister_netdev(netdev);

	e1000_phy_hw_reset(&adapter->hw);

	e1000_proc_dev_free(adapter);

	iounmap(adapter->hw.hw_addr);
	pci_release_regions(pdev);

	kfree(netdev);
}

/**
 * e1000_sw_init - Initialize general software structures (struct e1000_adapter)
 * @adapter: board private structure to initialize
 *
 * e1000_sw_init initializes the Adapter private data structure.
 * Fields are initialized based on PCI device information and
 * OS network device settings (MTU size).
 **/

static void __devinit
e1000_sw_init(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	struct net_device *netdev = adapter->netdev;
	struct pci_dev *pdev = adapter->pdev;

	/* PCI config space info */

	uint16_t *vendor = &hw->vendor_id;
	uint16_t *device = &hw->device_id;
	uint16_t *subvendor = &hw->subsystem_vendor_id;
	uint16_t *subsystem = &hw->subsystem_id;
	uint8_t  *revision  = &hw->revision_id;

	pci_read_config_word(pdev, PCI_VENDOR_ID, vendor);
	pci_read_config_word(pdev, PCI_DEVICE_ID, device);
	pci_read_config_byte(pdev, PCI_REVISION_ID, revision);
	pci_read_config_word(pdev, PCI_SUBSYSTEM_VENDOR_ID, subvendor);
	pci_read_config_word(pdev, PCI_SUBSYSTEM_ID, subsystem);

	pci_read_config_word(pdev, PCI_COMMAND, &hw->pci_cmd_word);

	adapter->rx_buffer_len = E1000_RXBUFFER_2048;
	hw->max_frame_size = netdev->mtu +
	                         ENET_HEADER_SIZE + ETHERNET_FCS_SIZE;
	hw->min_frame_size = MINIMUM_ETHERNET_FRAME_SIZE;

	/* identify the MAC */

	switch (*device) {
	case E1000_DEV_ID_82542:
		switch (*revision) {
		case E1000_82542_2_0_REV_ID:
			hw->mac_type = e1000_82542_rev2_0;
			break;
		case E1000_82542_2_1_REV_ID:
			hw->mac_type = e1000_82542_rev2_1;
			break;
		default:
			hw->mac_type = e1000_82542_rev2_0;
			E1000_ERR("Could not identify 82542 revision\n");
		}
		break;
	case E1000_DEV_ID_82543GC_FIBER:
	case E1000_DEV_ID_82543GC_COPPER:
		hw->mac_type = e1000_82543;
		break;
	case E1000_DEV_ID_82544EI_COPPER:
	case E1000_DEV_ID_82544EI_FIBER:
	case E1000_DEV_ID_82544GC_COPPER:
	case E1000_DEV_ID_82544GC_LOM:
		hw->mac_type = e1000_82544;
		break;
	case E1000_DEV_ID_82540EM:
		hw->mac_type = e1000_82540;
		break;
	default:
		/* should never have loaded on this device */
		BUG();
	}

	/* flow control settings */

	hw->fc_high_water = FC_DEFAULT_HI_THRESH;
	hw->fc_low_water = FC_DEFAULT_LO_THRESH;
	hw->fc_pause_time = FC_DEFAULT_TX_TIMER;
	hw->fc_send_xon = 1;

	/* Media type - copper or fiber */

	if(hw->mac_type >= e1000_82543) {
		uint32_t status = E1000_READ_REG(hw, STATUS);

		if(status & E1000_STATUS_TBIMODE)
			hw->media_type = e1000_media_type_fiber;
		else
			hw->media_type = e1000_media_type_copper;
	} else {
		hw->media_type = e1000_media_type_fiber;
	}

	if(hw->mac_type < e1000_82543)
		hw->report_tx_early = 0;
	else
		hw->report_tx_early = 1;

	hw->wait_autoneg_complete = FALSE;
	hw->tbi_compatibility_en = TRUE;
	hw->adaptive_ifs = TRUE;

	atomic_set(&adapter->irq_sem, 1);
	spin_lock_init(&adapter->tx_lock);
	spin_lock_init(&adapter->stats_lock);
}

/**
 * e1000_open - Called when a network interface is made active
 * @netdev: network interface device structure
 *
 * Returns 0 on success, negative value on failure
 *
 * The open entry point is called when a network interface is made
 * active by the system (IFF_UP).  At this point all resources needed
 * for transmit and receive operations are allocated, the interrupt
 * handler is registered with the OS, the watchdog timer is started,
 * and the stack is notified that the interface is ready.
 **/

static int
e1000_open(struct net_device *netdev)
{
	struct e1000_adapter *adapter = netdev->priv;

	/* allocate transmit descriptors */

	if(e1000_setup_tx_resources(adapter))
		goto err_setup_tx;

	/* allocate receive descriptors */

	if(e1000_setup_rx_resources(adapter))
		goto err_setup_rx;

	if(e1000_up(adapter))
		goto err_up;

	return 0;

err_up:
	e1000_free_rx_resources(adapter);
err_setup_rx:
	e1000_free_tx_resources(adapter);
err_setup_tx:
	e1000_reset(adapter);

	return -EBUSY;
}

/**
 * e1000_close - Disables a network interface
 * @netdev: network interface device structure
 *
 * Returns 0, this is not allowed to fail
 *
 * The close entry point is called when an interface is de-activated
 * by the OS.  The hardware is still under the drivers control, but
 * needs to be disabled.  A global MAC reset is issued to stop the
 * hardware, and all transmit and receive resources are freed.
 **/

static int
e1000_close(struct net_device *netdev)
{
	struct e1000_adapter *adapter = netdev->priv;

	e1000_down(adapter);

	e1000_free_tx_resources(adapter);
	e1000_free_rx_resources(adapter);

	return 0;
}

/**
 * e1000_setup_tx_resources - allocate Tx resources (Descriptors)
 * @adapter: board private structure
 *
 * Return 0 on success, negative on failure
 **/

static int
e1000_setup_tx_resources(struct e1000_adapter *adapter)
{
	struct e1000_desc_ring *txdr = &adapter->tx_ring;
	struct pci_dev *pdev = adapter->pdev;
	int size;

	size = sizeof(struct e1000_buffer) * txdr->count;
	txdr->buffer_info = kmalloc(size, GFP_KERNEL);
	if(!txdr->buffer_info) {
		return -ENOMEM;
	}
	memset(txdr->buffer_info, 0, size);

	/* round up to nearest 4K */

	txdr->size = txdr->count * sizeof(struct e1000_tx_desc);
	E1000_ROUNDUP(txdr->size, 4096);

	txdr->desc = pci_alloc_consistent(pdev, txdr->size, &txdr->dma);
	if(!txdr->desc) {
		kfree(txdr->buffer_info);
		return -ENOMEM;
	}
	memset(txdr->desc, 0, txdr->size);

	txdr->next_to_use = 0;
	txdr->next_to_clean = 0;

	return 0;
}

/**
 * e1000_configure_tx - Configure 8254x Transmit Unit after Reset
 * @adapter: board private structure
 *
 * Configure the Tx unit of the MAC after a reset.
 **/

static void
e1000_configure_tx(struct e1000_adapter *adapter)
{
	uint64_t tdba = adapter->tx_ring.dma;
	uint32_t tdlen = adapter->tx_ring.count * sizeof(struct e1000_tx_desc);
	uint32_t tctl, tipg;

	E1000_WRITE_REG(&adapter->hw, TDBAL, (tdba & 0x00000000FFFFFFFF));
	E1000_WRITE_REG(&adapter->hw, TDBAH, (tdba >> 32));

	E1000_WRITE_REG(&adapter->hw, TDLEN, tdlen);

	/* Setup the HW Tx Head and Tail descriptor pointers */

	E1000_WRITE_REG(&adapter->hw, TDH, 0);
	E1000_WRITE_REG(&adapter->hw, TDT, 0);

	/* Set the default values for the Tx Inter Packet Gap timer */

	switch (adapter->hw.mac_type) {
	case e1000_82542_rev2_0:
	case e1000_82542_rev2_1:
		tipg = DEFAULT_82542_TIPG_IPGT;
		tipg |= DEFAULT_82542_TIPG_IPGR1 << E1000_TIPG_IPGR1_SHIFT;
		tipg |= DEFAULT_82542_TIPG_IPGR2 << E1000_TIPG_IPGR2_SHIFT;
		break;
	default:
		if(adapter->hw.media_type == e1000_media_type_fiber)
			tipg = DEFAULT_82543_TIPG_IPGT_FIBER;
		else
			tipg = DEFAULT_82543_TIPG_IPGT_COPPER;
		tipg |= DEFAULT_82543_TIPG_IPGR1 << E1000_TIPG_IPGR1_SHIFT;
		tipg |= DEFAULT_82543_TIPG_IPGR2 << E1000_TIPG_IPGR2_SHIFT;
	}
	E1000_WRITE_REG(&adapter->hw, TIPG, tipg);

	/* Set the Tx Interrupt Delay register */

	E1000_WRITE_REG(&adapter->hw, TIDV, 64);

	/* Program the Transmit Control Register */

	tctl = E1000_READ_REG(&adapter->hw, TCTL);

	tctl &= ~E1000_TCTL_CT;	
	tctl |= E1000_TCTL_EN | E1000_TCTL_PSP |
	       (E1000_COLLISION_THRESHOLD << E1000_CT_SHIFT);

	E1000_WRITE_REG(&adapter->hw, TCTL, tctl);

	e1000_config_collision_dist(&adapter->hw);

	/* Setup Transmit Descriptor Settings for this adapter */
	adapter->txd_cmd = E1000_TXD_CMD_IFCS | E1000_TXD_CMD_IDE;

	if(adapter->hw.report_tx_early == 1)
		adapter->txd_cmd |= E1000_TXD_CMD_RS;
	else
		adapter->txd_cmd |= E1000_TXD_CMD_RPS;
}

/**
 * e1000_setup_rx_resources - allocate Rx resources (Descriptors)
 * @adapter: board private structure
 *
 * Returns 0 on success, negative on failure
 **/

static int
e1000_setup_rx_resources(struct e1000_adapter *adapter)
{
	struct e1000_desc_ring *rxdr = &adapter->rx_ring;
	struct pci_dev *pdev = adapter->pdev;
	int size;

	size = sizeof(struct e1000_buffer) * rxdr->count;
	rxdr->buffer_info = kmalloc(size, GFP_KERNEL);
	if(!rxdr->buffer_info) {
		return -ENOMEM;
	}
	memset(rxdr->buffer_info, 0, size);

	/* Round up to nearest 4K */

	rxdr->size = rxdr->count * sizeof(struct e1000_rx_desc);
	E1000_ROUNDUP(rxdr->size, 4096);

	rxdr->desc = pci_alloc_consistent(pdev, rxdr->size, &rxdr->dma);

	if(!rxdr->desc) {
		kfree(rxdr->buffer_info);
		return -ENOMEM;
	}
	memset(rxdr->desc, 0, rxdr->size);

	rxdr->next_to_clean = 0;
	rxdr->next_to_use = 0;

	return 0;
}

/**
 * e1000_setup_rctl - configure the receive control register
 * @adapter: Board private structure
 **/

static void
e1000_setup_rctl(struct e1000_adapter *adapter)
{
	uint32_t rctl;

	rctl = E1000_READ_REG(&adapter->hw, RCTL);

	rctl &= ~(3 << E1000_RCTL_MO_SHIFT);

	rctl |= E1000_RCTL_EN | E1000_RCTL_BAM |
	        E1000_RCTL_LBM_NO | E1000_RCTL_RDMTS_HALF |
	        (adapter->hw.mc_filter_type << E1000_RCTL_MO_SHIFT);

	if(adapter->hw.tbi_compatibility_on == 1)
		rctl |= E1000_RCTL_SBP;
	else
		rctl &= ~E1000_RCTL_SBP;

	rctl &= ~(E1000_RCTL_SZ_4096);
	switch (adapter->rx_buffer_len) {
	case E1000_RXBUFFER_2048:
	default:
		rctl |= E1000_RCTL_SZ_2048;
		rctl &= ~(E1000_RCTL_BSEX | E1000_RCTL_LPE);
		break;
	case E1000_RXBUFFER_4096:
		rctl |= E1000_RCTL_SZ_4096 | E1000_RCTL_BSEX | E1000_RCTL_LPE;
		break;
	case E1000_RXBUFFER_8192:
		rctl |= E1000_RCTL_SZ_8192 | E1000_RCTL_BSEX | E1000_RCTL_LPE;
		break;
	case E1000_RXBUFFER_16384:
		rctl |= E1000_RCTL_SZ_16384 | E1000_RCTL_BSEX | E1000_RCTL_LPE;
		break;
	}

	E1000_WRITE_REG(&adapter->hw, RCTL, rctl);
}

/**
 * e1000_configure_rx - Configure 8254x Receive Unit after Reset
 * @adapter: board private structure
 *
 * Configure the Rx unit of the MAC after a reset.
 **/

static void
e1000_configure_rx(struct e1000_adapter *adapter)
{
	uint64_t rdba = adapter->rx_ring.dma;
	uint32_t rdlen = adapter->rx_ring.count * sizeof(struct e1000_rx_desc);
	uint32_t rctl;
	uint32_t rxcsum;

	/* make sure receives are disabled while setting up the descriptors */

	rctl = E1000_READ_REG(&adapter->hw, RCTL);
	E1000_WRITE_REG(&adapter->hw, RCTL, rctl & ~E1000_RCTL_EN);

	/* set the Receive Delay Timer Register */

	if(adapter->hw.mac_type == e1000_82540) {
		E1000_WRITE_REG(&adapter->hw, RADV, adapter->rx_int_delay);
		E1000_WRITE_REG(&adapter->hw, RDTR, 64);

		/* Set the interrupt throttling rate.  Value is calculated
		 * as DEFAULT_ITR = 1/(MAX_INTS_PER_SEC * 256ns) */
#define MAX_INTS_PER_SEC        8000
#define DEFAULT_ITR             1000000000/(MAX_INTS_PER_SEC * 256)
		E1000_WRITE_REG(&adapter->hw, ITR, DEFAULT_ITR);

	} else {
		E1000_WRITE_REG(&adapter->hw, RDTR, adapter->rx_int_delay);
	}

	/* Setup the Base and Length of the Rx Descriptor Ring */

	E1000_WRITE_REG(&adapter->hw, RDBAL, (rdba & 0x00000000FFFFFFFF));
	E1000_WRITE_REG(&adapter->hw, RDBAH, (rdba >> 32));

	E1000_WRITE_REG(&adapter->hw, RDLEN, rdlen);

	/* Setup the HW Rx Head and Tail Descriptor Pointers */
	E1000_WRITE_REG(&adapter->hw, RDH, 0);
	E1000_WRITE_REG(&adapter->hw, RDT, 0);

	/* Enable 82543 Receive Checksum Offload for TCP and UDP */
	if((adapter->hw.mac_type >= e1000_82543) &&
	   (adapter->rx_csum == TRUE)) {
		rxcsum = E1000_READ_REG(&adapter->hw, RXCSUM);
		rxcsum |= E1000_RXCSUM_TUOFL;
		E1000_WRITE_REG(&adapter->hw, RXCSUM, rxcsum);
	}

	/* Enable Receives */

	E1000_WRITE_REG(&adapter->hw, RCTL, rctl);
}

/**
 * e1000_free_tx_resources - Free Tx Resources
 * @adapter: board private structure
 *
 * Free all transmit software resources
 **/

static void
e1000_free_tx_resources(struct e1000_adapter *adapter)
{
	struct pci_dev *pdev = adapter->pdev;

	e1000_clean_tx_ring(adapter);

	kfree(adapter->tx_ring.buffer_info);
	adapter->tx_ring.buffer_info = NULL;

	pci_free_consistent(pdev, adapter->tx_ring.size,
	                    adapter->tx_ring.desc, adapter->tx_ring.dma);

	adapter->tx_ring.desc = NULL;
}

/**
 * e1000_clean_tx_ring - Free Tx Buffers
 * @adapter: board private structure
 **/

static void
e1000_clean_tx_ring(struct e1000_adapter *adapter)
{
	struct pci_dev *pdev = adapter->pdev;
	unsigned long size;
	int i;

	/* Free all the Tx ring sk_buffs */

	for(i = 0; i < adapter->tx_ring.count; i++) {
		if(adapter->tx_ring.buffer_info[i].skb) {

			pci_unmap_page(pdev,
			               adapter->tx_ring.buffer_info[i].dma,
			               adapter->tx_ring.buffer_info[i].length,
			               PCI_DMA_TODEVICE);

			dev_kfree_skb(adapter->tx_ring.buffer_info[i].skb);

			adapter->tx_ring.buffer_info[i].skb = NULL;
		}
	}

	size = sizeof(struct e1000_buffer) * adapter->tx_ring.count;
	memset(adapter->tx_ring.buffer_info, 0, size);

	/* Zero out the descriptor ring */

	memset(adapter->tx_ring.desc, 0, adapter->tx_ring.size);

	adapter->tx_ring.next_to_use = 0;
	adapter->tx_ring.next_to_clean = 0;

	E1000_WRITE_REG(&adapter->hw, TDH, 0);
	E1000_WRITE_REG(&adapter->hw, TDT, 0);
}

/**
 * e1000_free_rx_resources - Free Rx Resources
 * @adapter: board private structure
 *
 * Free all receive software resources
 **/

static void
e1000_free_rx_resources(struct e1000_adapter *adapter)
{
	struct pci_dev *pdev = adapter->pdev;

	e1000_clean_rx_ring(adapter);

	kfree(adapter->rx_ring.buffer_info);
	adapter->rx_ring.buffer_info = NULL;

	pci_free_consistent(pdev, adapter->rx_ring.size,
	                    adapter->rx_ring.desc, adapter->rx_ring.dma);

	adapter->rx_ring.desc = NULL;
}

/**
 * e1000_clean_rx_ring - Free Rx Buffers
 * @adapter: board private structure
 **/

static void
e1000_clean_rx_ring(struct e1000_adapter *adapter)
{
	struct pci_dev *pdev = adapter->pdev;
	unsigned long size;
	int i;

	/* Free all the Rx ring sk_buffs */

	for(i = 0; i < adapter->rx_ring.count; i++) {
		if(adapter->rx_ring.buffer_info[i].skb) {

			pci_unmap_single(pdev,
			                 adapter->rx_ring.buffer_info[i].dma,
			                 adapter->rx_ring.buffer_info[i].length,
			                 PCI_DMA_FROMDEVICE);

			dev_kfree_skb(adapter->rx_ring.buffer_info[i].skb);

			adapter->rx_ring.buffer_info[i].skb = NULL;
		}
	}

	size = sizeof(struct e1000_buffer) * adapter->rx_ring.count;
	memset(adapter->rx_ring.buffer_info, 0, size);

	/* Zero out the descriptor ring */

	memset(adapter->rx_ring.desc, 0, adapter->rx_ring.size);

	adapter->rx_ring.next_to_clean = 0;
	adapter->rx_ring.next_to_use = 0;

	E1000_WRITE_REG(&adapter->hw, RDH, 0);
	E1000_WRITE_REG(&adapter->hw, RDT, 0);
}

/* The 82542 2.0 (revision 2) needs to have the receive unit in reset
 * and memory write and invalidate disabled for certain operations
 */
static void
e1000_enter_82542_rst(struct e1000_adapter *adapter)
{
	struct pci_dev *pdev = adapter->pdev;
	struct net_device *netdev = adapter->netdev;
	uint16_t pci_command_word = adapter->hw.pci_cmd_word;
	uint32_t rctl;

	if(pci_command_word & PCI_COMMAND_INVALIDATE) {
		pci_command_word &= ~PCI_COMMAND_INVALIDATE;
		pci_write_config_word(pdev, PCI_COMMAND, pci_command_word);
	}

	rctl = E1000_READ_REG(&adapter->hw, RCTL);
	rctl |= E1000_RCTL_RST;
	E1000_WRITE_REG(&adapter->hw, RCTL, rctl);
	msec_delay(5);

	if(netif_running(netdev))
		e1000_clean_rx_ring(adapter);
}

static void
e1000_leave_82542_rst(struct e1000_adapter *adapter)
{
	struct pci_dev *pdev = adapter->pdev;
	struct net_device *netdev = adapter->netdev;
	uint16_t pci_command_word = adapter->hw.pci_cmd_word;
	uint32_t rctl;

	rctl = E1000_READ_REG(&adapter->hw, RCTL);
	rctl &= ~E1000_RCTL_RST;
	E1000_WRITE_REG(&adapter->hw, RCTL, rctl);
	msec_delay(5);

	if(pci_command_word & PCI_COMMAND_INVALIDATE)
		pci_write_config_word(pdev, PCI_COMMAND, pci_command_word);

	if(netif_running(netdev)) {
		e1000_configure_rx(adapter);
		e1000_alloc_rx_buffers(adapter);
	}
}

/**
 * e1000_set_mac - Change the Ethernet Address of the NIC
 * @netdev: network interface device structure
 * @p: pointer to an address structure
 *
 * Returns 0 on success, negative on failure
 **/

static int
e1000_set_mac(struct net_device *netdev, void *p)
{
	struct e1000_adapter *adapter = netdev->priv;
	struct sockaddr *addr = p;

	/* 82542 2.0 needs to be in reset to write receive address registers */

	if(adapter->hw.mac_type == e1000_82542_rev2_0)
		e1000_enter_82542_rst(adapter);

	memcpy(netdev->dev_addr, addr->sa_data, netdev->addr_len);
	memcpy(adapter->hw.mac_addr, addr->sa_data, netdev->addr_len);

	e1000_rar_set(&adapter->hw, adapter->hw.mac_addr, 0);

	if(adapter->hw.mac_type == e1000_82542_rev2_0)
		e1000_leave_82542_rst(adapter);

	return 0;
}

/**
 * e1000_set_multi - Multicast and Promiscuous mode set
 * @netdev: network interface device structure
 *
 * The set_multi entry point is called whenever the multicast address
 * list or the network interface flags are updated.  This routine is
 * resposible for configuring the hardware for proper multicast,
 * promiscuous mode, and all-multi behavior.
 **/

static void
e1000_set_multi(struct net_device *netdev)
{
	struct e1000_adapter *adapter = netdev->priv;
	struct e1000_hw *hw = &adapter->hw;
	struct dev_mc_list *mc_ptr;
	uint32_t rctl;
	uint32_t hash_value;
	int i;

	/* Check for Promiscuous and All Multicast modes */

	rctl = E1000_READ_REG(hw, RCTL);

	if(netdev->flags & IFF_PROMISC) {
		rctl |= (E1000_RCTL_UPE | E1000_RCTL_MPE);
	} else if(netdev->flags & IFF_ALLMULTI) {
		rctl |= E1000_RCTL_MPE;
		rctl &= ~E1000_RCTL_UPE;
	} else {
		rctl &= ~(E1000_RCTL_UPE | E1000_RCTL_MPE);
	}

	E1000_WRITE_REG(hw, RCTL, rctl);

	/* 82542 2.0 needs to be in reset to write receive address registers */

	if(hw->mac_type == e1000_82542_rev2_0)
		e1000_enter_82542_rst(adapter);

	/* load the first 15 multicast address into the exact filters 1-15
	 * RAR 0 is used for the station MAC adddress
	 * if there are not 15 addresses, go ahead and clear the filters
	 */
	mc_ptr = netdev->mc_list;

	for(i = 1; i < E1000_RAR_ENTRIES; i++) {
		if(mc_ptr) {
			e1000_rar_set(hw, mc_ptr->dmi_addr, i);
			mc_ptr = mc_ptr->next;
		} else {
			E1000_WRITE_REG_ARRAY(hw, RA, i << 1, 0);
			E1000_WRITE_REG_ARRAY(hw, RA, (i << 1) + 1, 0);
		}
	}

	/* clear the old settings from the multicast hash table */

	for(i = 0; i < E1000_NUM_MTA_REGISTERS; i++)
		E1000_WRITE_REG_ARRAY(hw, MTA, i, 0);

	/* load any remaining addresses into the hash table */

	for(; mc_ptr; mc_ptr = mc_ptr->next) {
		hash_value = e1000_hash_mc_addr(hw, mc_ptr->dmi_addr);
		e1000_mta_set(hw, hash_value);
	}

	if(hw->mac_type == e1000_82542_rev2_0)
		e1000_leave_82542_rst(adapter);
}


/* need to wait a few seconds after link up to get diagnostic information from the phy */

static void
e1000_update_phy_info(unsigned long data)
{
	struct e1000_adapter *adapter = (struct e1000_adapter *) data;
	e1000_phy_get_info(&adapter->hw, &adapter->phy_info);
}

/**
 * e1000_watchdog - Timer Call-back
 * @data: pointer to netdev cast into an unsigned long
 **/

static void
e1000_watchdog(unsigned long data)
{
	struct e1000_adapter *adapter = (struct e1000_adapter *) data;
	struct net_device *netdev = adapter->netdev;

	e1000_check_for_link(&adapter->hw);

	if(E1000_READ_REG(&adapter->hw, STATUS) & E1000_STATUS_LU) {
		if(!netif_carrier_ok(netdev)) {
			e1000_get_speed_and_duplex(&adapter->hw,
			                           &adapter->link_speed,
			                           &adapter->link_duplex);

			printk(KERN_INFO
			       "e1000: %s NIC Link is Up %d Mbps %s\n",
			       netdev->name, adapter->link_speed,
			       adapter->link_duplex == FULL_DUPLEX ?
			       "Full Duplex" : "Half Duplex");

			netif_carrier_on(netdev);
			adapter->trans_finish = jiffies;
			netif_wake_queue(netdev);
			mod_timer(&adapter->phy_info_timer, jiffies + 2 * HZ);
		}
	} else {
		if(netif_carrier_ok(netdev)) {
			adapter->link_speed = 0;
			adapter->link_duplex = 0;
			printk(KERN_INFO
			       "e1000: %s NIC Link is Down\n",
			       netdev->name);
			netif_carrier_off(netdev);
			netif_stop_queue(netdev);
			mod_timer(&adapter->phy_info_timer, jiffies + 2 * HZ);
		}
	}

	e1000_update_stats(adapter);
	e1000_update_adaptive(&adapter->hw);

	/* Reset the timer */
	mod_timer(&adapter->watchdog_timer, jiffies + 2 * HZ);
}

#define E1000_TX_FLAGS_CSUM		0x00000001
#define E1000_TX_FLAGS_VLAN		0x00000002
#define E1000_TX_FLAGS_VLAN_MASK	0xffff0000
#define E1000_TX_FLAGS_VLAN_SHIFT	16

static inline boolean_t
e1000_tx_csum(struct e1000_adapter *adapter, struct sk_buff *skb)
{
	struct e1000_context_desc *context_desc;
	int i;
	uint8_t css, cso;

	if(skb->ip_summed == CHECKSUM_HW) {
		css = skb->h.raw - skb->data;
		cso = (skb->h.raw + skb->csum) - skb->data;

		i = adapter->tx_ring.next_to_use;
		context_desc = E1000_CONTEXT_DESC(adapter->tx_ring, i);

		context_desc->upper_setup.tcp_fields.tucss = css;
		context_desc->upper_setup.tcp_fields.tucso = cso;
		context_desc->upper_setup.tcp_fields.tucse = 0;
		context_desc->tcp_seg_setup.data = 0;
		context_desc->cmd_and_length =
			cpu_to_le32(adapter->txd_cmd | E1000_TXD_CMD_DEXT);

		i = (i + 1) % adapter->tx_ring.count;
		adapter->tx_ring.next_to_use = i;

		return TRUE;
	}

	return FALSE;
}

static inline int
e1000_tx_map(struct e1000_adapter *adapter, struct sk_buff *skb)
{
	struct e1000_desc_ring *tx_ring = &adapter->tx_ring;
	int len, offset, size, count, i;

	int f;
	len = skb->len - skb->data_len;

	i = (tx_ring->next_to_use + tx_ring->count - 1) % tx_ring->count;
	count = 0;

	offset = 0;

	while(len) {
		i = (i + 1) % tx_ring->count;
		size = min(len, adapter->max_data_per_txd);
		tx_ring->buffer_info[i].length = size;
		tx_ring->buffer_info[i].dma =
			pci_map_single(adapter->pdev,
				skb->data + offset,
				size,
				PCI_DMA_TODEVICE);

		len -= size;
		offset += size;
		count++;
	}

	for(f = 0; f < skb_shinfo(skb)->nr_frags; f++) {
		struct skb_frag_struct *frag;

		frag = &skb_shinfo(skb)->frags[f];
		len = frag->size;
		offset = 0;

		while(len) {
			i = (i + 1) % tx_ring->count;
			size = min(len, adapter->max_data_per_txd);
			tx_ring->buffer_info[i].length = size;
			tx_ring->buffer_info[i].dma =
				pci_map_page(adapter->pdev,
					frag->page,
					frag->page_offset + offset,
					size,
					PCI_DMA_TODEVICE);

			len -= size;
			offset += size;
			count++;
		}
	}
	tx_ring->buffer_info[i].skb = skb;

	return count;
}

static inline void
e1000_tx_queue(struct e1000_adapter *adapter, int count, int tx_flags)
{
	struct e1000_desc_ring *tx_ring = &adapter->tx_ring;
	struct e1000_tx_desc *tx_desc = NULL;
	uint32_t txd_upper, txd_lower;
	int i;

	txd_upper = 0;
	txd_lower = adapter->txd_cmd;

	if(tx_flags & E1000_TX_FLAGS_CSUM) {
		txd_lower |= E1000_TXD_CMD_DEXT | E1000_TXD_DTYP_D;
		txd_upper |= E1000_TXD_POPTS_TXSM << 8;
	}

	if(tx_flags & E1000_TX_FLAGS_VLAN) {
		txd_lower |= E1000_TXD_CMD_VLE;
		txd_upper |= (tx_flags & E1000_TX_FLAGS_VLAN_MASK);
	}

	i = tx_ring->next_to_use;

	while(count--) {
		tx_desc = E1000_TX_DESC(*tx_ring, i);
		tx_desc->buffer_addr = cpu_to_le64(tx_ring->buffer_info[i].dma);
		tx_desc->lower.data =
			cpu_to_le32(txd_lower | tx_ring->buffer_info[i].length);
		tx_desc->upper.data = cpu_to_le32(txd_upper);
		i = (i + 1) % tx_ring->count;
	}

	tx_desc->lower.data |= cpu_to_le32(E1000_TXD_CMD_EOP);

	tx_ring->next_to_use = i;
	E1000_WRITE_REG(&adapter->hw, TDT, i);
}

#define TXD_USE_COUNT(S, X) (((S) / (X)) + (((S) % (X)) ? 1 : 0))

static int
e1000_xmit_frame(struct sk_buff *skb, struct net_device *netdev)
{
	struct e1000_adapter *adapter = netdev->priv;
	unsigned long flags;
	int tx_flags = 0, count;

	int f;


	if(time_after(netdev->trans_start, adapter->trans_finish + HZ) &&
	   !(E1000_READ_REG(&adapter->hw, STATUS) & E1000_STATUS_TXOFF)) {

		adapter->trans_finish = jiffies;
		netif_stop_queue(netdev);
		return 1;
	}

	count = TXD_USE_COUNT(skb->len - skb->data_len,
	                      adapter->max_data_per_txd);
	for(f = 0; f < skb_shinfo(skb)->nr_frags; f++)
		count += TXD_USE_COUNT(skb_shinfo(skb)->frags[f].size,
		                       adapter->max_data_per_txd);
	if(skb->ip_summed == CHECKSUM_HW)
		count++;

	spin_lock_irqsave(&adapter->tx_lock, flags);
	e1000_clean_tx_irq(adapter);
	if(E1000_DESC_UNUSED(&adapter->tx_ring) < count) {
		netif_stop_queue(netdev);
		spin_unlock_irqrestore(&adapter->tx_lock, flags);
		return 1;
	}
	spin_unlock_irqrestore(&adapter->tx_lock, flags);

	if(e1000_tx_csum(adapter, skb))
		tx_flags |= E1000_TX_FLAGS_CSUM;

#ifdef NETIF_F_HW_VLAN_TX
	if(adapter->vlgrp && vlan_tx_tag_present(skb)) {
		tx_flags |= E1000_TX_FLAGS_VLAN;
		tx_flags |= (vlan_tx_tag_get(skb) << E1000_TX_FLAGS_VLAN_SHIFT);
	}
#endif
	
	count = e1000_tx_map(adapter, skb);

	e1000_tx_queue(adapter, count, tx_flags);

	netdev->trans_start = jiffies;

	return 0;
}

/**
 * e1000_tx_timeout - Respond to a Tx Hang
 * @netdev: network interface device structure
 **/

static void
e1000_tx_timeout(struct net_device *netdev)
{
	struct e1000_adapter *adapter = netdev->priv;

	e1000_down(adapter);
	e1000_up(adapter);
}

/**
 * e1000_get_stats - Get System Network Statistics
 * @netdev: network interface device structure
 *
 * Returns the address of the device statistics structure.
 * The statistics are actually updated from the timer callback.
 **/

static struct net_device_stats *
e1000_get_stats(struct net_device *netdev)
{
	struct e1000_adapter *adapter = netdev->priv;

	return &adapter->net_stats;
}

/**
 * e1000_change_mtu - Change the Maximum Transfer Unit
 * @netdev: network interface device structure
 * @new_mtu: new value for maximum frame size
 *
 * Returns 0 on success, negative on failure
 **/

static int
e1000_change_mtu(struct net_device *netdev, int new_mtu)
{
	struct e1000_adapter *adapter = netdev->priv;
	int old_mtu = adapter->rx_buffer_len;
	int max_frame = new_mtu + ENET_HEADER_SIZE + ETHERNET_FCS_SIZE;

	if((max_frame < MINIMUM_ETHERNET_FRAME_SIZE) ||
	   (max_frame > MAX_JUMBO_FRAME_SIZE)) {
		E1000_ERR("Invalid MTU setting\n");
		return -EINVAL;
	}

	if(max_frame <= E1000_RXBUFFER_2048) {
		adapter->rx_buffer_len = E1000_RXBUFFER_2048;

	} else if(adapter->hw.mac_type < e1000_82543) {
		E1000_ERR("Jumbo Frames not supported on 82542\n");
		return -EINVAL;

	} else if(max_frame <= E1000_RXBUFFER_4096) {
		adapter->rx_buffer_len = E1000_RXBUFFER_4096;

	} else if(max_frame <= E1000_RXBUFFER_8192) {
		adapter->rx_buffer_len = E1000_RXBUFFER_8192;

	} else {
		adapter->rx_buffer_len = E1000_RXBUFFER_16384;
	}

	if(old_mtu != adapter->rx_buffer_len && netif_running(netdev)) {

		e1000_down(adapter);
		e1000_up(adapter);
	}

	netdev->mtu = new_mtu;
	adapter->hw.max_frame_size = max_frame;

	return 0;
}

/**
 * e1000_update_stats - Update the board statistics counters
 * @adapter: board private structure
 **/

static void
e1000_update_stats(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	unsigned long flags;
	uint16_t phy_tmp;

#define PHY_IDLE_ERROR_COUNT_MASK 0x00FF

	spin_lock_irqsave(&adapter->stats_lock, flags);

	/* these counters are modified from e1000_adjust_tbi_stats,
	 * called from the interrupt context, so they must only
	 * be written while holding adapter->stats_lock
	 */

	adapter->stats.crcerrs += E1000_READ_REG(hw, CRCERRS);
	adapter->stats.gprc += E1000_READ_REG(hw, GPRC);
	adapter->stats.gorcl += E1000_READ_REG(hw, GORCL);
	adapter->stats.gorch += E1000_READ_REG(hw, GORCH);
	adapter->stats.bprc += E1000_READ_REG(hw, BPRC);
	adapter->stats.mprc += E1000_READ_REG(hw, MPRC);
	adapter->stats.roc += E1000_READ_REG(hw, ROC);
	adapter->stats.prc64 += E1000_READ_REG(hw, PRC64);
	adapter->stats.prc127 += E1000_READ_REG(hw, PRC127);
	adapter->stats.prc255 += E1000_READ_REG(hw, PRC255);
	adapter->stats.prc511 += E1000_READ_REG(hw, PRC511);
	adapter->stats.prc1023 += E1000_READ_REG(hw, PRC1023);
	adapter->stats.prc1522 += E1000_READ_REG(hw, PRC1522);

	spin_unlock_irqrestore(&adapter->stats_lock, flags);

	/* the rest of the counters are only modified here */

	adapter->stats.symerrs += E1000_READ_REG(hw, SYMERRS);
	adapter->stats.mpc += E1000_READ_REG(hw, MPC);
	adapter->stats.scc += E1000_READ_REG(hw, SCC);
	adapter->stats.ecol += E1000_READ_REG(hw, ECOL);
	adapter->stats.mcc += E1000_READ_REG(hw, MCC);
	adapter->stats.latecol += E1000_READ_REG(hw, LATECOL);
	adapter->stats.dc += E1000_READ_REG(hw, DC);
	adapter->stats.sec += E1000_READ_REG(hw, SEC);
	adapter->stats.rlec += E1000_READ_REG(hw, RLEC);
	adapter->stats.xonrxc += E1000_READ_REG(hw, XONRXC);
	adapter->stats.xontxc += E1000_READ_REG(hw, XONTXC);
	adapter->stats.xoffrxc += E1000_READ_REG(hw, XOFFRXC);
	adapter->stats.xofftxc += E1000_READ_REG(hw, XOFFTXC);
	adapter->stats.fcruc += E1000_READ_REG(hw, FCRUC);
	adapter->stats.gptc += E1000_READ_REG(hw, GPTC);
	adapter->stats.gotcl += E1000_READ_REG(hw, GOTCL);
	adapter->stats.gotch += E1000_READ_REG(hw, GOTCH);
	adapter->stats.rnbc += E1000_READ_REG(hw, RNBC);
	adapter->stats.ruc += E1000_READ_REG(hw, RUC);
	adapter->stats.rfc += E1000_READ_REG(hw, RFC);
	adapter->stats.rjc += E1000_READ_REG(hw, RJC);
	adapter->stats.torl += E1000_READ_REG(hw, TORL);
	adapter->stats.torh += E1000_READ_REG(hw, TORH);
	adapter->stats.totl += E1000_READ_REG(hw, TOTL);
	adapter->stats.toth += E1000_READ_REG(hw, TOTH);
	adapter->stats.tpr += E1000_READ_REG(hw, TPR);
	adapter->stats.ptc64 += E1000_READ_REG(hw, PTC64);
	adapter->stats.ptc127 += E1000_READ_REG(hw, PTC127);
	adapter->stats.ptc255 += E1000_READ_REG(hw, PTC255);
	adapter->stats.ptc511 += E1000_READ_REG(hw, PTC511);
	adapter->stats.ptc1023 += E1000_READ_REG(hw, PTC1023);
	adapter->stats.ptc1522 += E1000_READ_REG(hw, PTC1522);
	adapter->stats.mptc += E1000_READ_REG(hw, MPTC);
	adapter->stats.bptc += E1000_READ_REG(hw, BPTC);

	/* used for adaptive IFS */

	hw->tx_packet_delta = E1000_READ_REG(hw, TPT);
	adapter->stats.tpt += hw->tx_packet_delta;
	hw->collision_delta = E1000_READ_REG(hw, COLC);
	adapter->stats.colc += hw->collision_delta;

	if(hw->mac_type >= e1000_82543) {
		adapter->stats.algnerrc += E1000_READ_REG(hw, ALGNERRC);
		adapter->stats.rxerrc += E1000_READ_REG(hw, RXERRC);
		adapter->stats.tncrs += E1000_READ_REG(hw, TNCRS);
		adapter->stats.cexterr += E1000_READ_REG(hw, CEXTERR);
		adapter->stats.tsctc += E1000_READ_REG(hw, TSCTC);
		adapter->stats.tsctfc += E1000_READ_REG(hw, TSCTFC);
	}

	/* Fill out the OS statistics structure */

	adapter->net_stats.rx_packets = adapter->stats.gprc;
	adapter->net_stats.tx_packets = adapter->stats.gptc;
	adapter->net_stats.rx_bytes = adapter->stats.gorcl;
	adapter->net_stats.tx_bytes = adapter->stats.gotcl;
	adapter->net_stats.multicast = adapter->stats.mprc;
	adapter->net_stats.collisions = adapter->stats.colc;

	/* Rx Errors */

	adapter->net_stats.rx_errors = adapter->stats.rxerrc +
		adapter->stats.crcerrs + adapter->stats.algnerrc +
		adapter->stats.rlec + adapter->stats.rnbc +
		adapter->stats.mpc + adapter->stats.cexterr;
	adapter->net_stats.rx_dropped = adapter->stats.rnbc;
	adapter->net_stats.rx_length_errors = adapter->stats.rlec;
	adapter->net_stats.rx_crc_errors = adapter->stats.crcerrs;
	adapter->net_stats.rx_frame_errors = adapter->stats.algnerrc;
	adapter->net_stats.rx_fifo_errors = adapter->stats.mpc;
	adapter->net_stats.rx_missed_errors = adapter->stats.mpc;

	/* Tx Errors */

	adapter->net_stats.tx_errors = adapter->stats.ecol +
	                               adapter->stats.latecol;
	adapter->net_stats.tx_aborted_errors = adapter->stats.ecol;
	adapter->net_stats.tx_window_errors = adapter->stats.latecol;

	/* Tx Dropped needs to be maintained elsewhere */

	/* Phy Stats */

	if(hw->media_type == e1000_media_type_copper) {
		if((adapter->link_speed == SPEED_1000) &&
		   (!e1000_read_phy_reg(hw, PHY_1000T_STATUS, &phy_tmp))) {
			phy_tmp &= PHY_IDLE_ERROR_COUNT_MASK;
			adapter->phy_stats.idle_errors += phy_tmp;
		}

		if(!e1000_read_phy_reg(hw, M88E1000_RX_ERR_CNTR, &phy_tmp))
			adapter->phy_stats.receive_errors += phy_tmp;
	}
}

/**
 * e1000_irq_disable - Mask off interrupt generation on the NIC
 * @adapter: board private structure
 **/

static inline void
e1000_irq_disable(struct e1000_adapter *adapter)
{
	atomic_inc(&adapter->irq_sem);
	E1000_WRITE_REG(&adapter->hw, IMC, ~0);
	synchronize_irq();
}

/**
 * e1000_irq_enable - Enable default interrupt generation settings
 * @adapter: board private structure
 **/

static inline void
e1000_irq_enable(struct e1000_adapter *adapter)
{
	if(atomic_dec_and_test(&adapter->irq_sem))
		E1000_WRITE_REG(&adapter->hw, IMS, IMS_ENABLE_MASK);
}

/**
 * e1000_intr - Interrupt Handler
 * @irq: interrupt number
 * @data: pointer to a network interface device structure
 * @pt_regs: CPU registers structure
 **/

static void
e1000_intr(int irq, void *data, struct pt_regs *regs)
{
	struct net_device *netdev = data;
	struct e1000_adapter *adapter = netdev->priv;
	uint32_t icr;
	int i = E1000_MAX_INTR;

	while(i && (icr = E1000_READ_REG(&adapter->hw, ICR))) {

		if(icr & (E1000_ICR_RXSEQ | E1000_ICR_LSC)) {
			adapter->hw.get_link_status = 1;
			mod_timer(&adapter->watchdog_timer, jiffies);
		}

		e1000_clean_rx_irq(adapter);

		if((icr & E1000_ICR_TXDW) && spin_trylock(&adapter->tx_lock)) {
			e1000_clean_tx_irq(adapter);
			spin_unlock(&adapter->tx_lock);
		}

		i--;

	}
}

/**
 * e1000_clean_tx_irq - Reclaim resources after transmit completes
 * @adapter: board private structure
 **/

static void
e1000_clean_tx_irq(struct e1000_adapter *adapter)
{
	struct e1000_desc_ring *tx_ring = &adapter->tx_ring;
	struct net_device *netdev = adapter->netdev;
	struct pci_dev *pdev = adapter->pdev;
	struct e1000_tx_desc *tx_desc;
	int i;

	i = tx_ring->next_to_clean;
	tx_desc = E1000_TX_DESC(*tx_ring, i);

	while(tx_desc->upper.data & cpu_to_le32(E1000_TXD_STAT_DD)) {

		if(tx_ring->buffer_info[i].dma) {

			pci_unmap_page(pdev,
			               tx_ring->buffer_info[i].dma,
			               tx_ring->buffer_info[i].length,
			               PCI_DMA_TODEVICE);

			tx_ring->buffer_info[i].dma = 0;
		}

		if(tx_ring->buffer_info[i].skb) {

			dev_kfree_skb_any(tx_ring->buffer_info[i].skb);

			tx_ring->buffer_info[i].skb = NULL;
		}

		memset(tx_desc, 0, sizeof(struct e1000_tx_desc));
		mb();

		i = (i + 1) % tx_ring->count;
		tx_desc = E1000_TX_DESC(*tx_ring, i);

		adapter->trans_finish = jiffies;
	}

	tx_ring->next_to_clean = i;

	if(netif_queue_stopped(netdev) && netif_carrier_ok(netdev) &&
	   (E1000_DESC_UNUSED(tx_ring) > E1000_TX_QUEUE_WAKE)) {

		netif_wake_queue(netdev);
	}
}

/**
 * e1000_clean_rx_irq - Send received data up the network stack,
 * @adapter: board private structure
 **/

static void
e1000_clean_rx_irq(struct e1000_adapter *adapter)
{
	struct e1000_desc_ring *rx_ring = &adapter->rx_ring;
	struct net_device *netdev = adapter->netdev;
	struct pci_dev *pdev = adapter->pdev;
	struct e1000_rx_desc *rx_desc;
	struct sk_buff *skb;
	unsigned long flags;
	uint32_t length;
	uint8_t last_byte;
	int i;

	i = rx_ring->next_to_clean;
	rx_desc = E1000_RX_DESC(*rx_ring, i);

	while(rx_desc->status & E1000_RXD_STAT_DD) {

		pci_unmap_single(pdev,
		                 rx_ring->buffer_info[i].dma,
		                 rx_ring->buffer_info[i].length,
		                 PCI_DMA_FROMDEVICE);

		skb = rx_ring->buffer_info[i].skb;
		length = le16_to_cpu(rx_desc->length);

		if(!(rx_desc->status & E1000_RXD_STAT_EOP)) {

			/* All receives must fit into a single buffer */

			E1000_DBG("Receive packet consumed multiple buffers\n");

			dev_kfree_skb_irq(skb);
			memset(rx_desc, 0, sizeof(struct e1000_rx_desc));
			mb();
			rx_ring->buffer_info[i].skb = NULL;

			i = (i + 1) % rx_ring->count;

			rx_desc = E1000_RX_DESC(*rx_ring, i);
			continue;
		}

		if(rx_desc->errors & E1000_RXD_ERR_FRAME_ERR_MASK) {

			last_byte = *(skb->data + length - 1);

			if(TBI_ACCEPT(&adapter->hw, rx_desc->status,
			              rx_desc->errors, length, last_byte)) {

				spin_lock_irqsave(&adapter->stats_lock, flags);

				e1000_tbi_adjust_stats(&adapter->hw,
				                       &adapter->stats,
				                       length, skb->data);

				spin_unlock_irqrestore(&adapter->stats_lock,
				                       flags);
				length--;
			} else {

				dev_kfree_skb_irq(skb);
				memset(rx_desc, 0, sizeof(struct e1000_rx_desc));
				mb();
				rx_ring->buffer_info[i].skb = NULL;

				i = (i + 1) % rx_ring->count;

				rx_desc = E1000_RX_DESC(*rx_ring, i);
				continue;
			}
		}

		/* Good Receive */
		skb_put(skb, length - ETHERNET_FCS_SIZE);

		/* Receive Checksum Offload */
		e1000_rx_checksum(adapter, rx_desc, skb);

		skb->protocol = eth_type_trans(skb, netdev);
#ifdef NETIF_F_HW_VLAN_TX
		if(adapter->vlgrp && (rx_desc->status & E1000_RXD_STAT_VP)) {
			vlan_hwaccel_rx(skb, adapter->vlgrp,
				(rx_desc->special & E1000_RXD_SPC_VLAN_MASK));
		} else {
			netif_rx(skb);
		}
#else
		netif_rx(skb);
#endif
		netdev->last_rx = jiffies;

		memset(rx_desc, 0, sizeof(struct e1000_rx_desc));
		mb();
		rx_ring->buffer_info[i].skb = NULL;

		i = (i + 1) % rx_ring->count;

		rx_desc = E1000_RX_DESC(*rx_ring, i);
	}

	rx_ring->next_to_clean = i;

	e1000_alloc_rx_buffers(adapter);
}

/**
 * e1000_alloc_rx_buffers - Replace used receive buffers
 * @data: address of board private structure
 **/

static void
e1000_alloc_rx_buffers(struct e1000_adapter *adapter)
{
	struct e1000_desc_ring *rx_ring = &adapter->rx_ring;
	struct net_device *netdev = adapter->netdev;
	struct pci_dev *pdev = adapter->pdev;
	struct e1000_rx_desc *rx_desc;
	struct sk_buff *skb;
	int reserve_len;
	int i;

	if(!netif_running(netdev))
		return;

	reserve_len = 2;

	i = rx_ring->next_to_use;

	while(!rx_ring->buffer_info[i].skb) {
		rx_desc = E1000_RX_DESC(*rx_ring, i);

		skb = alloc_skb(adapter->rx_buffer_len + reserve_len,
		                GFP_ATOMIC);

		if(!skb) {
			/* Better luck next round */
			break;
		}

		/* Make buffer alignment 2 beyond a 16 byte boundary
		 * this will result in a 16 byte aligned IP header after
		 * the 14 byte MAC header is removed
		 */
		skb_reserve(skb, reserve_len);

		skb->dev = netdev;

		rx_ring->buffer_info[i].skb = skb;
		rx_ring->buffer_info[i].length = adapter->rx_buffer_len;
		rx_ring->buffer_info[i].dma =
			pci_map_single(pdev,
			               skb->data,
			               adapter->rx_buffer_len,
			               PCI_DMA_FROMDEVICE);

		rx_desc->buffer_addr = cpu_to_le64(rx_ring->buffer_info[i].dma);

		if(!(i % E1000_RX_BUFFER_WRITE))
			E1000_WRITE_REG(&adapter->hw, RDT, i);

		i = (i + 1) % rx_ring->count;
	}

	rx_ring->next_to_use = i;
}

/**
 * e1000_ioctl -
 * @netdev:
 * @ifreq:
 * @cmd:
 **/

static int
e1000_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd)
{
	switch (cmd) {
	case SIOCETHTOOL:
		return e1000_ethtool_ioctl(netdev, ifr);
	default:
		return -EOPNOTSUPP;
	}
}

/**
 * e1000_rx_checksum - Receive Checksum Offload for 82543
 * @adapter: board private structure
 * @rx_desc: receive descriptor
 * @sk_buff: socket buffer with received data
 **/

static inline void
e1000_rx_checksum(struct e1000_adapter *adapter,
                  struct e1000_rx_desc *rx_desc,
                  struct sk_buff *skb)
{
	/* 82543 or newer only */
	if((adapter->hw.mac_type < e1000_82543) ||
	/* Ignore Checksum bit is set */
	(rx_desc->status & E1000_RXD_STAT_IXSM) ||
	/* TCP Checksum has not been calculated */
	(!(rx_desc->status & E1000_RXD_STAT_TCPCS))) {
		skb->ip_summed = CHECKSUM_NONE;
		return;
	}

	/* At this point we know the hardware did the TCP checksum */
	/* now look at the TCP checksum error bit */
	if(rx_desc->errors & E1000_RXD_ERR_TCPE) {
		/* let the stack verify checksum errors */
		skb->ip_summed = CHECKSUM_NONE;
		adapter->hw_csum_err++;
	} else {
	/* TCP checksum is good */
		skb->ip_summed = CHECKSUM_UNNECESSARY;
		adapter->hw_csum_good++;
	}
}

/**
 * e1000_enable_WOL - Wake On Lan Support (Magic Pkt)
 * @adapter: Adapter structure
 **/

void
e1000_enable_WOL(struct e1000_adapter *adapter)
{
	uint32_t wuc;

	if(adapter->hw.mac_type < e1000_82544)
		return;

	if(adapter->wol) {
		wuc = E1000_WUC_APME | E1000_WUC_PME_EN |
		      E1000_WUC_PME_STATUS | E1000_WUC_APMPME;

		E1000_WRITE_REG(&adapter->hw, WUC, wuc);

		E1000_WRITE_REG(&adapter->hw, WUFC, adapter->wol);
	}
}

void
e1000_write_pci_cfg(struct e1000_hw *hw,
                    uint32_t reg, uint16_t *value)
{
	struct e1000_adapter *adapter = hw->back;

	pci_write_config_word(adapter->pdev, reg, *value);
}

#ifdef NETIF_F_HW_VLAN_TX
static void
e1000_vlan_rx_register(struct net_device *netdev, struct vlan_group *grp)
{
	struct e1000_adapter *adapter = netdev->priv;
	uint32_t ctrl, rctl;

	e1000_irq_disable(adapter);
	adapter->vlgrp = grp;

	if(grp) {
		/* enable VLAN tag insert/strip */

		E1000_WRITE_REG(&adapter->hw, VET, ETHERNET_IEEE_VLAN_TYPE);

		ctrl = E1000_READ_REG(&adapter->hw, CTRL);
		ctrl |= E1000_CTRL_VME;
		E1000_WRITE_REG(&adapter->hw, CTRL, ctrl);

		/* enable VLAN receive filtering */

		rctl = E1000_READ_REG(&adapter->hw, RCTL);
		rctl |= E1000_RCTL_VFE;
		rctl &= ~E1000_RCTL_CFIEN;
		E1000_WRITE_REG(&adapter->hw, RCTL, rctl);
	} else {
		/* disable VLAN tag insert/strip */

		ctrl = E1000_READ_REG(&adapter->hw, CTRL);
		ctrl &= ~E1000_CTRL_VME;
		E1000_WRITE_REG(&adapter->hw, CTRL, ctrl);

		/* disable VLAN filtering */

		rctl = E1000_READ_REG(&adapter->hw, RCTL);
		rctl &= ~E1000_RCTL_VFE;
		E1000_WRITE_REG(&adapter->hw, RCTL, rctl);
	}

	e1000_irq_enable(adapter);
}

static void
e1000_vlan_rx_add_vid(struct net_device *netdev, uint16_t vid)
{
	struct e1000_adapter *adapter = netdev->priv;
	uint32_t vfta, index;

	/* add VID to filter table */

	index = (vid >> 5) & 0x7F;
	vfta = E1000_READ_REG_ARRAY(&adapter->hw, VFTA, index);
	vfta |= (1 << (vid & 0x1F));
	e1000_write_vfta(&adapter->hw, index, vfta);
}

static void
e1000_vlan_rx_kill_vid(struct net_device *netdev, uint16_t vid)
{
	struct e1000_adapter *adapter = netdev->priv;
	uint32_t vfta, index;

	e1000_irq_disable(adapter);
	
	if(adapter->vlgrp)
		adapter->vlgrp->vlan_devices[vid] = NULL;
	
	e1000_irq_enable(adapter);

	/* remove VID from filter table*/

	index = (vid >> 5) & 0x7F;
	vfta = E1000_READ_REG_ARRAY(&adapter->hw, VFTA, index);
	vfta &= ~(1 << (vid & 0x1F));
	e1000_write_vfta(&adapter->hw, index, vfta);
}
#endif

/* e1000_main.c */
