/******************************************************************************
 *
 *  $Id: generic.c,v e69a5010914f 2011/12/14 15:47:28 fp $
 *
 *  Copyright (C) 2006-2008  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT Master.
 *
 *  The IgH EtherCAT Master is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License version 2, as
 *  published by the Free Software Foundation.
 *
 *  The IgH EtherCAT Master is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 *  Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with the IgH EtherCAT Master; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  ---
 *
 *  The license mentioned above concerns the source code only. Using the
 *  EtherCAT technology and brand is only permitted in compliance with the
 *  industrial property and similar rights of Beckhoff Automation GmbH.
 *
 *****************************************************************************/

/** \file
 * EtherCAT generic Ethernet device module.
 */

/*****************************************************************************/

#include <linux/module.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/version.h>
#include <linux/if_arp.h> /* ARPHRD_ETHER */
#include <linux/etherdevice.h>

#include <linux/proc_fs.h>

#include "../globals.h"
#include "ecdev.h"

#define PFX "ec_generic: "

#define ETH_P_ETHERCAT 0x88A4

#define EC_GEN_RX_BUF_SIZE 1600


#define IGH_EC_PROC_DIR "ethercat"
#define IGH_GEN_PROC_NAME "gen_log"
#define IGH_EC_NUMBER 2000

/*****************************************************************************/

int __init ec_gen_init_module(void);
void __exit ec_gen_cleanup_module(void);

/*****************************************************************************/

/** \cond */

MODULE_AUTHOR("Florian Pose <fp@igh-essen.com>");
MODULE_DESCRIPTION("EtherCAT master generic Ethernet device module");
MODULE_LICENSE("GPL");
MODULE_VERSION(EC_MASTER_VERSION);

/** \endcond */

struct list_head generic_devices;

static int ethercat_times_count = 0;
static int ethercat_times[IGH_EC_NUMBER];

typedef struct {
    struct list_head list;
    struct net_device *netdev;
    struct net_device *used_netdev;
    struct socket *socket;
    ec_device_t *ecdev;
    uint8_t *rx_buf;
} ec_gen_device_t;

typedef struct {
    struct list_head list;
    struct net_device *netdev;
    char name[IFNAMSIZ];
    int ifindex;
    uint8_t dev_addr[ETH_ALEN];
} ec_gen_interface_desc_t;

int ec_gen_device_open(ec_gen_device_t *);
int ec_gen_device_stop(ec_gen_device_t *);
int ec_gen_device_start_xmit(ec_gen_device_t *, struct sk_buff *);
void ec_gen_device_poll(ec_gen_device_t *);

static int igh_gen_proc_show(struct seq_file *m, void *v);
static int igh_gen_proc_open(struct inode *inode, struct file *file);
static ssize_t igh_gen_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *f_pos);
static int igh_gen_proc_init(void);
static void igh_gen_proc_exit(void);

/*****************************************************************************/

static int ec_gen_netdev_open(struct net_device *dev)
{
    ec_gen_device_t *gendev = *((ec_gen_device_t **) netdev_priv(dev));
    return ec_gen_device_open(gendev);
}

/*****************************************************************************/

static int ec_gen_netdev_stop(struct net_device *dev)
{
    ec_gen_device_t *gendev = *((ec_gen_device_t **) netdev_priv(dev));
    return ec_gen_device_stop(gendev);
}

/*****************************************************************************/

static int ec_gen_netdev_start_xmit(
        struct sk_buff *skb,
        struct net_device *dev
        )
{
    ec_gen_device_t *gendev = *((ec_gen_device_t **) netdev_priv(dev));
    return ec_gen_device_start_xmit(gendev, skb);
}

/*****************************************************************************/

void ec_gen_poll(struct net_device *dev)
{
    ec_gen_device_t *gendev = *((ec_gen_device_t **) netdev_priv(dev));
    ec_gen_device_poll(gendev);
}

/*****************************************************************************/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29)
static const struct net_device_ops ec_gen_netdev_ops = {
    .ndo_open       = ec_gen_netdev_open,
    .ndo_stop       = ec_gen_netdev_stop,
    .ndo_start_xmit = ec_gen_netdev_start_xmit,
};
#endif

/*****************************************************************************/

/** Init generic device.
 */
int ec_gen_device_init(
        ec_gen_device_t *dev
        )
{
    ec_gen_device_t **priv;
    char null = 0x00;

    dev->ecdev = NULL;
    dev->socket = NULL;
    dev->rx_buf = NULL;

	dev->netdev = alloc_netdev(sizeof(ec_gen_device_t *),
			&null, NET_NAME_UNKNOWN, ether_setup);
    if (!dev->netdev) {
        return -ENOMEM;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29)
    dev->netdev->netdev_ops = &ec_gen_netdev_ops;
#else
    dev->netdev->open = ec_gen_netdev_open;
    dev->netdev->stop = ec_gen_netdev_stop;
    dev->netdev->hard_start_xmit = ec_gen_netdev_start_xmit;
#endif

    priv = netdev_priv(dev->netdev);
    *priv = dev;

    return 0;
}

/*****************************************************************************/

/** Clear generic device.
 */
void ec_gen_device_clear(
        ec_gen_device_t *dev
        )
{
    if (dev->ecdev) {
        ecdev_close(dev->ecdev);
        ecdev_withdraw(dev->ecdev);
    }
    if (dev->socket) {
        sock_release(dev->socket);
    }
    free_netdev(dev->netdev);

    if (dev->rx_buf) {
        kfree(dev->rx_buf);
    }
}

/*****************************************************************************/

/** Creates a network socket.
 */
int ec_gen_device_create_socket(
        ec_gen_device_t *dev,
        ec_gen_interface_desc_t *desc
        )
{
    int ret;
    struct sockaddr_ll sa;

    dev->rx_buf = kmalloc(EC_GEN_RX_BUF_SIZE, GFP_KERNEL);
    if (!dev->rx_buf) {
        return -ENOMEM;
    }

	ret = sock_create_kern(&init_net, PF_PACKET, SOCK_RAW,
			htons(ETH_P_ETHERCAT), &dev->socket);
    if (ret) {
        printk(KERN_ERR PFX "Failed to create socket (ret = %i).\n", ret);
        return ret;
    }

    printk(KERN_ERR PFX "Binding socket to interface %i (%s).\n",
            desc->ifindex, desc->name);

    memset(&sa, 0x00, sizeof(sa));
    sa.sll_family = AF_PACKET;
    sa.sll_protocol = htons(ETH_P_ETHERCAT);
    sa.sll_ifindex = desc->ifindex;
    ret = kernel_bind(dev->socket, (struct sockaddr *) &sa, sizeof(sa));
    if (ret) {
        printk(KERN_ERR PFX "Failed to bind() socket to interface"
                " (ret = %i).\n", ret);
        sock_release(dev->socket);
        dev->socket = NULL;
        return ret;
    }

    return 0;
}

/*****************************************************************************/

/** Offer generic device to master.
 */
int ec_gen_device_offer(
        ec_gen_device_t *dev,
        ec_gen_interface_desc_t *desc
        )
{
    int ret = 0;

    dev->used_netdev = desc->netdev;
    memcpy(dev->netdev->dev_addr, desc->dev_addr, ETH_ALEN);

    dev->ecdev = ecdev_offer(dev->netdev, ec_gen_poll, THIS_MODULE);
    if (dev->ecdev) {
        if (ec_gen_device_create_socket(dev, desc)) {
            ecdev_withdraw(dev->ecdev);
            dev->ecdev = NULL;
        } else if (ecdev_open(dev->ecdev)) {
            ecdev_withdraw(dev->ecdev);
            dev->ecdev = NULL;
        } else {
            ecdev_set_link(dev->ecdev, netif_carrier_ok(dev->used_netdev)); // FIXME
            ret = 1;
        }
    }

    return ret;
}

/*****************************************************************************/

/** Open the device.
 */
int ec_gen_device_open(
        ec_gen_device_t *dev
        )
{
    return 0;
}

/*****************************************************************************/

/** Stop the device.
 */
int ec_gen_device_stop(
        ec_gen_device_t *dev
        )
{
    return 0;
}

/*****************************************************************************/

int ec_gen_device_start_xmit(
        ec_gen_device_t *dev,
        struct sk_buff *skb
        )
{
    struct msghdr msg;
    struct kvec iov;
    size_t len = skb->len;
    int ret;

	uint8_t *frame_data = NULL, *cur_data = NULL;
	uint8_t type;
	__be16 protocol = 0xa488;
	struct timeval t;

    ecdev_set_link(dev->ecdev, netif_carrier_ok(dev->used_netdev));

    iov.iov_base = skb->data;
    iov.iov_len = len;
    memset(&msg, 0, sizeof(msg));

	frame_data = skb->data + ETH_HLEN;
	cur_data = frame_data + 2;
	memcpy(&type, cur_data, 1);
	if (type == 11) {/**EC_DATAGRAM_LWR > Logical Write. */
		do_gettimeofday(&t);
		if (ethercat_times_count >= 0 && ethercat_times_count < IGH_EC_NUMBER)
			ethercat_times[ethercat_times_count] = t.tv_usec;
		ethercat_times_count++;
	}

    ret = kernel_sendmsg(dev->socket, &msg, &iov, 1, len);
    
    return ret == len ? NETDEV_TX_OK : NETDEV_TX_BUSY;
}

/*****************************************************************************/

/** Polls the device.
 */
void ec_gen_device_poll(
        ec_gen_device_t *dev
        )
{
    struct msghdr msg;
    struct kvec iov;
    int ret, budget = 10; // FIXME

    ecdev_set_link(dev->ecdev, netif_carrier_ok(dev->used_netdev));

    do {
        iov.iov_base = dev->rx_buf;
        iov.iov_len = EC_GEN_RX_BUF_SIZE;
        memset(&msg, 0, sizeof(msg));

        ret = kernel_recvmsg(dev->socket, &msg, &iov, 1, iov.iov_len,
                MSG_DONTWAIT);
        if (ret > 0) {
            ecdev_receive(dev->ecdev, dev->rx_buf, ret);
        } else if (ret < 0) {
            break;
        }
        budget--;
    } while (budget);
}

/*****************************************************************************/

/** Offer device.
 */
int offer_device(
        ec_gen_interface_desc_t *desc
        )
{
    ec_gen_device_t *gendev;
    int ret = 0;

    gendev = kmalloc(sizeof(ec_gen_device_t), GFP_KERNEL);
    if (!gendev) {
        return -ENOMEM;
    }

    ret = ec_gen_device_init(gendev);
    if (ret) {
        kfree(gendev);
        return ret;
    }

    if (ec_gen_device_offer(gendev, desc)) {
        list_add_tail(&gendev->list, &generic_devices);
    } else {
        ec_gen_device_clear(gendev);
        kfree(gendev);
    }

    return ret;
}

/*****************************************************************************/

/** Clear devices.
 */
void clear_devices(void)
{
    ec_gen_device_t *gendev, *next;

    list_for_each_entry_safe(gendev, next, &generic_devices, list) {
        list_del(&gendev->list);
        ec_gen_device_clear(gendev);
        kfree(gendev);
    }
}

/*****************************************************************************/

/** Performance test .
 */

static int igh_gen_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static int igh_gen_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, igh_gen_proc_show, NULL);
}

static ssize_t igh_gen_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *f_pos)
{
	if (buffer && strncmp(buffer, "start" , strlen("start")) == 0) {
		ethercat_times_count = 0;
		memset(ethercat_times, 0, sizeof(ethercat_times));
	} else if (buffer && strncmp(buffer, "stop" , strlen("stop")) == 0) {
		struct file *fp;
		mm_segment_t fs;
		loff_t pos = 0;
		char buf[8];
		int i = 0;
		fp = filp_open("./igh_gen_time.log", O_RDWR | O_CREAT | O_TRUNC, 0777);
		if (IS_ERR(fp)) {
			printk("create file error\n");
		}
		fs = get_fs();
		set_fs(KERNEL_DS);
		for(i = 0; i < IGH_EC_NUMBER; i++){
			memset(buf, 0, sizeof(buf));
			sprintf(buf, "%d\n", ethercat_times[i]);
			vfs_write(fp, buf, strlen(buf), &pos);
		}
		filp_close(fp, NULL);
		set_fs(fs);
	}
	return count;
}

static struct file_operations igh_gen_proc_fops = {
	.owner	= THIS_MODULE,
	.open	= igh_gen_proc_open,
	.release = single_release,
	.read	= seq_read,
	.llseek	= seq_lseek,
	.write 	= igh_gen_proc_write,
};

static int igh_gen_proc_init(void)
{
	struct proc_dir_entry* file;
	file = proc_create(IGH_EC_PROC_DIR"/"IGH_GEN_PROC_NAME, 0777, NULL, &igh_gen_proc_fops);
	if (!file) {
		printk("%s proc_create failed!\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

static void igh_gen_proc_exit(void)
{
	remove_proc_entry(IGH_EC_PROC_DIR"/"IGH_GEN_PROC_NAME, NULL);
}

/*****************************************************************************/

/** Module initialization.
 *
 * Initializes \a master_count masters.
 * \return 0 on success, else < 0
 */
int __init ec_gen_init_module(void)
{
    int ret = 0;
    struct list_head descs;
    struct net_device *netdev;
    ec_gen_interface_desc_t *desc, *next;

    printk(KERN_INFO PFX "EtherCAT master generic Ethernet device module %s\n",
            EC_MASTER_VERSION);

    INIT_LIST_HEAD(&generic_devices);
    INIT_LIST_HEAD(&descs);

    read_lock(&dev_base_lock);
    for_each_netdev(&init_net, netdev) {
        if (netdev->type != ARPHRD_ETHER)
            continue;
        desc = kmalloc(sizeof(ec_gen_interface_desc_t), GFP_ATOMIC);
        if (!desc) {
            ret = -ENOMEM;
            read_unlock(&dev_base_lock);
            goto out_err;
        }
        strncpy(desc->name, netdev->name, IFNAMSIZ);
        desc->netdev = netdev;
        desc->ifindex = netdev->ifindex;
        memcpy(desc->dev_addr, netdev->dev_addr, ETH_ALEN);
        list_add_tail(&desc->list, &descs);
    }
    read_unlock(&dev_base_lock);

    list_for_each_entry_safe(desc, next, &descs, list) {
        ret = offer_device(desc);
        if (ret) {
            goto out_err;
        }
        kfree(desc);
    }
	igh_gen_proc_init();
    return ret;

out_err:
    list_for_each_entry_safe(desc, next, &descs, list) {
        list_del(&desc->list);
        kfree(desc);
    }
    clear_devices();
	igh_gen_proc_exit();
    return ret;
}

/*****************************************************************************/

/** Module cleanup.
 *
 * Clears all master instances.
 */
void __exit ec_gen_cleanup_module(void)
{
    clear_devices();
    printk(KERN_INFO PFX "Unloading.\n");
}

/*****************************************************************************/

/** \cond */

module_init(ec_gen_init_module);
module_exit(ec_gen_cleanup_module);

/** \endcond */

/*****************************************************************************/
