/**
 * Watchdog driver for WRN device
 *
 * Author: Aleksandr Borisenko, Copyright 2016 (c)
 * Based on ixp4xx_wdt.c driver, Copyright 2004 (c) MontaVista, Software, Inc.
 *
 * This file is licensed under  the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/bitops.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/time.h>
#include <linux/init.h>

#define	WDT_IN_USE 0
#define	WDT_OK_TO_CLOSE 1
#define WDT_MAGIC_CHAR 'V'

#define WDT_MIN_KEEP_ALIVE_INTERVAL 1000  // ms
#define WDT_MAX_CMD_SIZE 16

#define WDT_DEFAULT_SERIAL_PORT "/dev/ttyS0"
#define WDT_TIMEOUT_DEFAULT 180  // sec
#define WDT_TIMEOUT_MIN 30
#define WDT_TIMEOUT_MAX 300

static bool nowayout = WATCHDOG_NOWAYOUT;
static int timeout = WDT_TIMEOUT_DEFAULT;
static char *serial_port = WDT_DEFAULT_SERIAL_PORT;
static unsigned long wdt_status = 0;
static DEFINE_SPINLOCK(wrn_wdt_lock);
static struct timespec wdt_keep_alive_sent = {0, 0};
static struct file *filp_port = NULL;
static char *cmd_keep_alive = "W0\n";


static void wdt_enable(void)
{
	int ret;
	struct timespec t;
	unsigned long time_delta;
	mm_segment_t fs;
	loff_t pos = 0;

	spin_lock(&wrn_wdt_lock);
	getnstimeofday(&t);
	time_delta = (t.tv_sec - wdt_keep_alive_sent.tv_sec) * 1000;  // sec to ms
	time_delta += (t.tv_nsec - wdt_keep_alive_sent.tv_nsec) / 1000000;  // ns to ms
	if (time_delta >= WDT_MIN_KEEP_ALIVE_INTERVAL) {
		// watchdog is making this call too often
		fs = get_fs();
		set_fs(get_ds());
		ret = vfs_write(filp_port, cmd_keep_alive, strlen(cmd_keep_alive), &pos);
		set_fs(fs);

		if (ret != strlen(cmd_keep_alive))
			pr_err("Could not send keep alive: %d\n", ret);
		getnstimeofday(&wdt_keep_alive_sent);
	}

	spin_unlock(&wrn_wdt_lock);
}

static void wdt_disable(void)
{
	int ret;
	mm_segment_t fs;
	loff_t pos = 0;
	char *cmd_disable = "W1\n";
	int cmd_len = strlen(cmd_disable);

	spin_lock(&wrn_wdt_lock);
	fs = get_fs();
	set_fs(get_ds());
	ret = vfs_write(filp_port, cmd_disable, cmd_len, &pos);
	set_fs(fs);

	if (ret != cmd_len)
		pr_err("Cannot disable the watchdog: %d\n", ret);

	spin_unlock(&wrn_wdt_lock);
}

static void wdt_timeout(void)
{
	int ret;
	mm_segment_t fs;
	char cmd_set_timeout[WDT_MAX_CMD_SIZE];
	int cmd_len;
	loff_t pos = 0;

	snprintf(cmd_set_timeout, WDT_MAX_CMD_SIZE, "W3:%d\n", timeout);
	cmd_set_timeout[WDT_MAX_CMD_SIZE - 1] = '\0';
	cmd_len = strlen(cmd_set_timeout);

	spin_lock(&wrn_wdt_lock);
	fs = get_fs();
	set_fs(get_ds());
	ret = vfs_write(filp_port, cmd_set_timeout, cmd_len, &pos);
	set_fs(fs);

	if (ret != cmd_len)
		pr_err("Could not set timeout: %d\n", ret);
	//else
	//	pr_info("Timeout has been adjusted to %d sec\n", timeout);

	spin_unlock(&wrn_wdt_lock);
}

static int wrn_wdt_open(struct inode *inode, struct file *file)
{
	if (test_and_set_bit(WDT_IN_USE, &wdt_status))
		return -EBUSY;

	clear_bit(WDT_OK_TO_CLOSE, &wdt_status);
	wdt_enable();
	return nonseekable_open(inode, file);
}

static ssize_t wrn_wdt_write(struct file *file, const char *data, size_t len, loff_t *ppos)
{
	size_t i;
	char c;

	if (len > 0) {
		if (!nowayout) {
			clear_bit(WDT_OK_TO_CLOSE, &wdt_status);
			for (i = 0; i < len; i++) {
				if (get_user(c, data + i))
					return -EFAULT;
				if (c == WDT_MAGIC_CHAR)
					set_bit(WDT_OK_TO_CLOSE, &wdt_status);
			}
		}
		wdt_enable();
	}
	return len;
}

static const struct watchdog_info ident = {
	.options = WDIOF_MAGICCLOSE | WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING,
	.firmware_version = 1,  // __u32
	.identity = "WRN Watchdog",  // __u8 [32]
};

static long wrn_wdt_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = -ENOTTY;
	int t;

	switch (cmd) {
	case WDIOC_GETSUPPORT:
		ret = copy_to_user((struct watchdog_info *)arg, &ident, sizeof(ident)) ? -EFAULT : 0;
		break;

	case WDIOC_GETSTATUS:
	case WDIOC_GETBOOTSTATUS:
		ret = put_user(0, (int *)arg);
		break;

	case WDIOC_KEEPALIVE:
		wdt_enable();
		ret = 0;
		break;

	case WDIOC_SETTIMEOUT:
		ret = get_user(t, (int *)arg);
		if (ret)
			break;

		if (t < WDT_TIMEOUT_MIN || t > WDT_TIMEOUT_MAX) {
			ret = -EINVAL;
			break;
		}

		timeout = t;
		wdt_timeout();
		wdt_enable();
		/* Fall through */

	case WDIOC_GETTIMEOUT:
		ret = put_user(timeout, (int *)arg);
		break;
	}

	return ret;
}

static int wrn_wdt_release(struct inode *inode, struct file *file)
{
	if (test_bit(WDT_OK_TO_CLOSE, &wdt_status))
		wdt_disable();
	else
		pr_crit("Device closed unexpectedly - timer will not stop\n");

	clear_bit(WDT_IN_USE, &wdt_status);
	clear_bit(WDT_OK_TO_CLOSE, &wdt_status);

	return 0;
}

static const struct file_operations wrn_wdt_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.write = wrn_wdt_write,
	.unlocked_ioctl = wrn_wdt_ioctl,
	.open = wrn_wdt_open,
	.release = wrn_wdt_release,
};

static struct miscdevice wrn_wdt_miscdev = {
	.minor = WATCHDOG_MINOR,
	.name = "watchdog",
	.fops = &wrn_wdt_fops,
};

static int __init wrn_wdt_init(void)
{
	int ret;

	// that's enough, termios struct must be initialized in user space by the WRN Daemon
	filp_port = filp_open(serial_port, O_RDWR | O_NOCTTY | O_NDELAY, 0);

	if (IS_ERR(filp_port)) {
		ret = PTR_ERR(filp_port);
		pr_crit("Unable to open port %s: %d\n", serial_port, ret);
	} else {
		ret = misc_register(&wrn_wdt_miscdev);
		if (ret == 0)
			wdt_timeout();
	}

	return ret;
}

static void __exit wrn_wdt_exit(void)
{
	filp_close(filp_port, NULL);
	misc_deregister(&wrn_wdt_miscdev);
}


module_init(wrn_wdt_init);
module_exit(wrn_wdt_exit);

module_param(timeout, int, 0);
MODULE_PARM_DESC(timeout, "Watchdog timeout in seconds (default="
	__MODULE_STRING(WDT_TIMEOUT_DEFAULT) ")");

module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default="
	__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

module_param(serial_port, charp, 0);
MODULE_PARM_DESC(serial_port, "Watchdog serial port (default="
	__MODULE_STRING(WDT_DEFAULT_SERIAL_PORT) ")");

MODULE_DESCRIPTION("WRN Watchdog");
MODULE_AUTHOR("Aleksandr Borisenko");
MODULE_LICENSE("GPL v2");
