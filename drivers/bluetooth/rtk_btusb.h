/*
 *
 *  Realtek Bluetooth USB driver
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/errno.h>
#include <linux/usb.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/poll.h>

#include <linux/version.h>
#include <linux/pm_runtime.h>
#include <linux/firmware.h>
#include <linux/suspend.h>


#define CONFIG_BLUEDROID        1 //bleuz 0 ;  bluedroid 1

#if CONFIG_BLUEDROID //for 4.2
#else //for blueZ	
	#include <net/bluetooth/bluetooth.h>
	#include <net/bluetooth/hci_core.h>
	#include <net/bluetooth/hci.h>
#endif


/***********************************
** Realtek - For rtk_btusb driver **
***********************************/
#define BTUSB_RPM		0* USB_RPM 	//	1 SS enable; 0 SS disable
#define LOAD_CONFIG		1         // set 1 if need to reconfig bt efuse  
#define URB_CANCELING_DELAY_MS	10  	 // Added by Realtek
//when os suspend, module is still powered,usb is not powered, 
//this may set to 1 ,and must comply with special patch code
#define CONFIG_RESET_RESUME		1
#define PRINT_CMD_EVENT			0
#define PRINT_ACL_DATA			0

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 33)
#define HDEV_BUS		hdev->bus
#define USB_RPM			1
#else
#define HDEV_BUS		hdev->type
#define USB_RPM			0
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 38)
#define NUM_REASSEMBLY 3
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 4, 0)
#define GET_DRV_DATA(x)		hci_get_drvdata(x)
#else
#define GET_DRV_DATA(x)		x->driver_data
#endif

static int patch_add(struct usb_interface* intf);
static void patch_remove(struct usb_interface* intf);
static int download_patch(struct usb_interface* intf);
static int set_btoff(struct usb_interface* intf);
static void print_event(struct sk_buff *skb);
static void print_command(struct sk_buff *skb);
static void print_acl (struct sk_buff *skb,int dataOut);
static void hci_fake_hardware_error();

#define BTUSB_MAX_ISOC_FRAMES	10
#define BTUSB_INTR_RUNNING		0
#define BTUSB_BULK_RUNNING		1
#define BTUSB_ISOC_RUNNING		2
#define BTUSB_SUSPENDING		3
#define BTUSB_DID_ISO_RESUME	4
/*******************************/
// Test (Realtek)
#define BTUSB_NEXT_RX_URB_SUBMITTING		5
/*******************************/

struct btusb_data {
	struct hci_dev       *hdev;
	struct usb_device    *udev;
	struct usb_interface *intf;
	struct usb_interface *isoc;

	spinlock_t lock;

	unsigned long flags;

	struct work_struct work;
	struct work_struct waker;

	struct usb_anchor tx_anchor;
	struct usb_anchor intr_anchor;
	struct usb_anchor bulk_anchor;
	struct usb_anchor isoc_anchor;
	struct usb_anchor deferred;
	int tx_in_flight;
	spinlock_t txlock;

	struct usb_endpoint_descriptor *intr_ep;
	struct usb_endpoint_descriptor *bulk_tx_ep;
	struct usb_endpoint_descriptor *bulk_rx_ep;
	struct usb_endpoint_descriptor *isoc_tx_ep;
	struct usb_endpoint_descriptor *isoc_rx_ep;

	__u8 cmdreq_type;

	unsigned int sco_num;
	int isoc_altsetting;
	int suspend_count;

#if CONFIG_BLUEDROID //for 4.2
	wait_queue_head_t read_wait;
	struct sk_buff_head readq;
#endif
};

/* Realtek - For rtk_btusb driver end */

//========================================================================

#if CONFIG_BLUEDROID //for 4.2
#define SUCCESS               0 /* Linux success code */
#define ERROR                -1 /* Linux error code */
#define QUEUE_SIZE 500
static int btfcd_init(void);
static void btfcd_exit(void);
static int btfcd_open(struct inode *inode_p, struct file *file_p);
static int btfcd_close(struct inode *inode_p, struct file *file_p);
static ssize_t btfcd_read(struct file *file_p, char *buf_p, size_t count, loff_t *pos_p);
static ssize_t btfcd_write(struct file *file_p, const char *buf_p, size_t count, loff_t *pos_p);
static unsigned int btfcd_poll(struct file *file, poll_table *wait);

/*****************************************
** Realtek - Integrate from bluetooth.h **
*****************************************/
/* Reserv for core and drivers use */
#define BT_SKB_RESERVE	8

/* BD Address */
typedef struct {
	__u8 b[6];
} __packed bdaddr_t;

/* Skb helpers */
struct bt_skb_cb {
	__u8 pkt_type;
	__u8 incoming;
	__u16 expect;
	__u16 tx_seq;
	__u8 retries;
	__u8 sar;
	__u8 force_active;
};
#define bt_cb(skb) ((struct bt_skb_cb *)((skb)->cb))

static inline struct sk_buff *bt_skb_alloc(unsigned int len, gfp_t how)
{
	struct sk_buff *skb;

	if ((skb = alloc_skb(len + BT_SKB_RESERVE, how))) {
		skb_reserve(skb, BT_SKB_RESERVE);
		bt_cb(skb)->incoming  = 0;
	}
	return skb;
}
/* Realtek - Integrate from bluetooth.h end */

/***********************************
** Realtek - Integrate from hci.h **
***********************************/
#define HCI_MAX_ACL_SIZE	1024
#define HCI_MAX_SCO_SIZE	255
#define HCI_MAX_EVENT_SIZE	260
#define HCI_MAX_FRAME_SIZE	(HCI_MAX_ACL_SIZE + 4)

/* HCI bus types */
#define HCI_VIRTUAL	0
#define HCI_USB		1
#define HCI_PCCARD	2
#define HCI_UART	3
#define HCI_RS232	4
#define HCI_PCI		5
#define HCI_SDIO	6

/* HCI controller types */
#define HCI_BREDR	0x00
#define HCI_AMP		0x01

/* HCI device flags */
enum {
	HCI_UP,
	HCI_INIT,
	HCI_RUNNING,

	HCI_PSCAN,
	HCI_ISCAN,
	HCI_AUTH,
	HCI_ENCRYPT,
	HCI_INQUIRY,

	HCI_RAW,

	HCI_RESET,
};

/*
 * BR/EDR and/or LE controller flags: the flags defined here should represent
 * states from the controller.
 */
enum {
	HCI_SETUP,
	HCI_AUTO_OFF,
	HCI_MGMT,
	HCI_PAIRABLE,
	HCI_SERVICE_CACHE,
	HCI_LINK_KEYS,
	HCI_DEBUG_KEYS,
	HCI_UNREGISTER,

	HCI_LE_SCAN,
	HCI_SSP_ENABLED,
	HCI_HS_ENABLED,
	HCI_LE_ENABLED,
	HCI_CONNECTABLE,
	HCI_DISCOVERABLE,
	HCI_LINK_SECURITY,
	HCI_PENDING_CLASS,
};

/* HCI data types */
#define HCI_COMMAND_PKT		0x01
#define HCI_ACLDATA_PKT		0x02
#define HCI_SCODATA_PKT		0x03
#define HCI_EVENT_PKT		0x04
#define HCI_VENDOR_PKT		0xff

#define HCI_MAX_NAME_LENGTH		248
#define HCI_MAX_EIR_LENGTH		240

#define HCI_OP_READ_LOCAL_VERSION	0x1001
struct hci_rp_read_local_version {
	__u8     status;
	__u8     hci_ver;
	__le16   hci_rev;
	__u8     lmp_ver;
	__le16   manufacturer;
	__le16   lmp_subver;
} __packed;

#define HCI_EV_CMD_COMPLETE		0x0e
struct hci_ev_cmd_complete {
	__u8     ncmd;
	__le16   opcode;
} __packed;

/* ---- HCI Packet structures ---- */
#define HCI_COMMAND_HDR_SIZE 3
#define HCI_EVENT_HDR_SIZE   2
#define HCI_ACL_HDR_SIZE     4
#define HCI_SCO_HDR_SIZE     3

struct hci_command_hdr {
	__le16	opcode;		/* OCF & OGF */
	__u8	plen;
} __packed;

struct hci_event_hdr {
	__u8	evt;
	__u8	plen;
} __packed;

struct hci_acl_hdr {
	__le16	handle;		/* Handle & Flags(PB, BC) */
	__le16	dlen;
} __packed;

struct hci_sco_hdr {
	__le16	handle;
	__u8	dlen;
} __packed;

static inline struct hci_event_hdr *hci_event_hdr(const struct sk_buff *skb)
{
	return (struct hci_event_hdr *) skb->data;
}

static inline struct hci_acl_hdr *hci_acl_hdr(const struct sk_buff *skb)
{
	return (struct hci_acl_hdr *) skb->data;
}

static inline struct hci_sco_hdr *hci_sco_hdr(const struct sk_buff *skb)
{
	return (struct hci_sco_hdr *) skb->data;
}

/* ---- HCI Ioctl requests structures ---- */
struct hci_dev_stats {
	__u32 err_rx;
	__u32 err_tx;
	__u32 cmd_tx;
	__u32 evt_rx;
	__u32 acl_tx;
	__u32 acl_rx;
	__u32 sco_tx;
	__u32 sco_rx;
	__u32 byte_rx;
	__u32 byte_tx;
};
/* Realtek - Integrate from hci.h end */

/*****************************************
** Realtek - Integrate from hci_core.h  **
*****************************************/
struct hci_conn_hash {
	struct list_head list;
	unsigned int     acl_num;
	unsigned int     sco_num;
	unsigned int     le_num;
};

#define HCI_MAX_SHORT_NAME_LENGTH	10

#define NUM_REASSEMBLY 4
struct hci_dev {
	struct mutex	lock;

	char		name[8];
	unsigned long	flags;
	__u16		id;
	__u8		bus;
	__u8		dev_type;

	struct sk_buff		*reassembly[NUM_REASSEMBLY];

	struct hci_conn_hash	conn_hash;

	struct hci_dev_stats	stat;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0)
	atomic_t        refcnt;
	struct module           *owner;
	void                    *driver_data;
#endif

	atomic_t		promisc;

    struct device		*parent;
	struct device		dev;

	unsigned long		dev_flags;

	int (*open)(struct hci_dev *hdev);
	int (*close)(struct hci_dev *hdev);
	int (*flush)(struct hci_dev *hdev);
	int (*send)(struct sk_buff *skb);
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0)
	void (*destruct)(struct hci_dev *hdev);
#endif
	void (*notify)(struct hci_dev *hdev, unsigned int evt);
	int (*ioctl)(struct hci_dev *hdev, unsigned int cmd, unsigned long arg);
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0)
static inline struct hci_dev *__hci_dev_hold(struct hci_dev *d)
{
        atomic_inc(&d->refcnt);
        return d;
}

static inline void __hci_dev_put(struct hci_dev *d)
{
        if (atomic_dec_and_test(&d->refcnt))
                d->destruct(d);
}
#endif

static inline void *hci_get_drvdata(struct hci_dev *hdev)
{
	return dev_get_drvdata(&hdev->dev);
}

static inline void hci_set_drvdata(struct hci_dev *hdev, void *data)
{
	dev_set_drvdata(&hdev->dev, data);
}

static struct hci_dev *hci_dev_get(int index);

static struct hci_dev *hci_alloc_dev(void);
static void hci_free_dev(struct hci_dev *hdev);
static int hci_register_dev(struct hci_dev *hdev);
static void hci_unregister_dev(struct hci_dev *hdev);
static int hci_dev_open(__u16 dev);
static int hci_dev_close(__u16 dev);

static int hci_recv_frame(struct sk_buff *skb);
static int hci_recv_fragment(struct hci_dev *hdev, int type, void *data, int count);

#define SET_HCIDEV_DEV(hdev, pdev) ((hdev)->parent = (pdev))
/* Realtek - Integrate from hci_core.h end */



/* -----  HCI Commands ---- */
#define HCI_OP_INQUIRY			0x0401
#define HCI_OP_INQUIRY_CANCEL		0x0402
#define HCI_OP_EXIT_PERIODIC_INQ	0x0404
#define HCI_OP_CREATE_CONN		0x0405
#define HCI_OP_ADD_SCO			0x0407
#define HCI_OP_CREATE_CONN_CANCEL	0x0408
#define HCI_OP_ACCEPT_CONN_REQ		0x0409
#define HCI_OP_REJECT_CONN_REQ		0x040a
#define HCI_OP_LINK_KEY_REPLY		0x040b
#define HCI_OP_LINK_KEY_NEG_REPLY	0x040c
#define HCI_OP_PIN_CODE_REPLY		0x040d
#define HCI_OP_PIN_CODE_NEG_REPLY	0x040e
#define HCI_OP_CHANGE_CONN_PTYPE	0x040f
#define HCI_OP_AUTH_REQUESTED		0x0411
#define HCI_OP_SET_CONN_ENCRYPT		0x0413
#define HCI_OP_CHANGE_CONN_LINK_KEY	0x0415
#define HCI_OP_REMOTE_NAME_REQ		0x0419
#define HCI_OP_REMOTE_NAME_REQ_CANCEL	0x041a
#define HCI_OP_READ_REMOTE_FEATURES	0x041b
#define HCI_OP_READ_REMOTE_EXT_FEATURES	0x041c
#define HCI_OP_READ_REMOTE_VERSION	0x041d
#define HCI_OP_SETUP_SYNC_CONN		0x0428
#define HCI_OP_ACCEPT_SYNC_CONN_REQ	0x0429
#define HCI_OP_REJECT_SYNC_CONN_REQ	0x042a
#define HCI_OP_SNIFF_MODE		0x0803
#define HCI_OP_EXIT_SNIFF_MODE		0x0804
#define HCI_OP_ROLE_DISCOVERY		0x0809
#define HCI_OP_SWITCH_ROLE		0x080b
#define HCI_OP_READ_LINK_POLICY		0x080c
#define HCI_OP_WRITE_LINK_POLICY	0x080d
#define HCI_OP_READ_DEF_LINK_POLICY	0x080e
#define HCI_OP_WRITE_DEF_LINK_POLICY	0x080f
#define HCI_OP_SNIFF_SUBRATE		0x0811
#define HCI_OP_SET_EVENT_MASK		0x0c01
#define HCI_OP_RESET			0x0c03
#define HCI_OP_SET_EVENT_FLT		0x0c05

/* -----  HCI events---- */
#define HCI_OP_DISCONNECT		0x0406
#define HCI_EV_INQUIRY_COMPLETE		0x01
#define HCI_EV_INQUIRY_RESULT		0x02
#define HCI_EV_CONN_COMPLETE		0x03
#define HCI_EV_CONN_REQUEST			0x04
#define HCI_EV_DISCONN_COMPLETE		0x05
#define HCI_EV_AUTH_COMPLETE		0x06
#define HCI_EV_REMOTE_NAME			0x07
#define HCI_EV_ENCRYPT_CHANGE		0x08
#define HCI_EV_CHANGE_LINK_KEY_COMPLETE	0x09

#define HCI_EV_REMOTE_FEATURES		0x0b
#define HCI_EV_REMOTE_VERSION		0x0c
#define HCI_EV_QOS_SETUP_COMPLETE	0x0d
#define HCI_EV_CMD_COMPLETE			0x0e
#define HCI_EV_CMD_STATUS			0x0f

#define HCI_EV_ROLE_CHANGE			0x12
#define HCI_EV_NUM_COMP_PKTS		0x13
#define HCI_EV_MODE_CHANGE			0x14
#define HCI_EV_PIN_CODE_REQ			0x16
#define HCI_EV_LINK_KEY_REQ			0x17
#define HCI_EV_LINK_KEY_NOTIFY		0x18
#define HCI_EV_CLOCK_OFFSET			0x1c
#define HCI_EV_PKT_TYPE_CHANGE		0x1d
#define HCI_EV_PSCAN_REP_MODE		0x20

#define HCI_EV_INQUIRY_RESULT_WITH_RSSI	0x22
#define HCI_EV_REMOTE_EXT_FEATURES	0x23
#define HCI_EV_SYNC_CONN_COMPLETE	0x2c
#define HCI_EV_SYNC_CONN_CHANGED	0x2d
#define HCI_EV_SNIFF_SUBRATE			0x2e
#define HCI_EV_EXTENDED_INQUIRY_RESULT	0x2f
#define HCI_EV_IO_CAPA_REQUEST		0x31
#define HCI_EV_SIMPLE_PAIR_COMPLETE	0x36
#define HCI_EV_REMOTE_HOST_FEATURES	0x3d


#endif //if bluedroid 4.2
