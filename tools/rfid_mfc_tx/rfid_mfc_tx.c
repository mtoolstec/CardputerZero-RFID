// SPDX-License-Identifier: GPL-2.0
/* Cardputer Zero: bounded transparent ST25R3916 TX + deterministic RX rearm.
 * Cooperates with spi-bcm2835 through its bus lock. Never changes arbitrary
 * addresses: only SPI0 and GPIO11/22 on the BCM2837 Cardputer Zero layout.
 */
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/spi/spi.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/mutex.h>

struct rfid_wave { __u32 length; __u32 reserved; __u64 data; };
#define RFID_TX _IOW('N', 0x42, struct rfid_wave)
#define CS 0x00
#define FIFO 0x04
#define CLK 0x08
#define CS_TA BIT(7)
#define CS_DONE BIT(16)
#define CS_RXD BIT(17)
#define CS_TXD BIT(18)
#define CS_CLEAR (BIT(4) | BIT(5))
#define SCLK_MASK (7u << 3)
#define MAX_WAVE 512
static DEFINE_MUTEX(tx_lock);
static void __iomem *spi_regs, *gpio_regs;
static struct spi_device *target;
static struct clk *spi_clk;

/* IRQs are off here; no sleeping, allocation or SPI core callbacks. */
static int pio(const u8 *data, unsigned int len, u32 mode, u64 deadline)
{
 unsigned int tx=0,rx=0;
 writel(mode|CS_CLEAR,spi_regs+CS);
 writel(mode|CS_TA,spi_regs+CS);
 while (rx<len) {
  u32 flags=readl(spi_regs+CS);
  if (tx<len && (flags&CS_TXD)) writel(data[tx++],spi_regs+FIFO);
  if (flags&CS_RXD) { readl(spi_regs+FIFO); ++rx; }
  if (ktime_get_mono_fast_ns()>deadline) { writel(mode|CS_CLEAR,spi_regs+CS); return -ETIMEDOUT; }
  cpu_relax();
 }
 while (!(readl(spi_regs+CS)&CS_DONE)) {
  if (ktime_get_mono_fast_ns()>deadline) { writel(mode|CS_CLEAR,spi_regs+CS); return -ETIMEDOUT; }
  cpu_relax();
 }
 writel(mode|CS_CLEAR,spi_regs+CS);
 return 0;
}
static int command(u8 cmd,u32 mode,u64 deadline)
{
 int ret;
 writel(BIT(22),gpio_regs+0x28); /* CAP BSS active LOW */
 ndelay(150);
 ret=pio(&cmd,1,mode,deadline);
 ndelay(150);
 writel(BIT(22),gpio_regs+0x1c);
 return ret;
}
static long rfid_ioctl(struct file *file,unsigned int request,unsigned long arg)
{
 struct rfid_wave wave;
 u8 *data;
 u32 saved_cs,saved_clk,saved_sclk,mode,divider;
 unsigned long irq_flags,rate;
 u64 deadline;
 bool runtime_ref=false;
 int ret;
 if (request!=RFID_TX) return -ENOTTY;
 if (copy_from_user(&wave,(void __user *)arg,sizeof(wave))) return -EFAULT;
 if (wave.reserved || wave.length<4 || wave.length>MAX_WAVE) return -EINVAL;
 data=memdup_user(u64_to_user_ptr(wave.data),wave.length);
 if (IS_ERR(data)) return PTR_ERR(data);
 if (mutex_lock_interruptible(&tx_lock)) { kfree(data); return -ERESTARTSYS; }
 if (pm_runtime_enabled(target->controller->dev.parent)) {
  ret=pm_runtime_resume_and_get(target->controller->dev.parent);
  if (ret<0) goto out_mutex;
  runtime_ref=true;
 }
 /* Some Cardputer kernels keep SPI0's clock on and disable runtime PM.
  * Hold our own balanced clock reference in either configuration. */
 ret=clk_prepare_enable(spi_clk);
 if (ret) goto out_pm;
 ret=spi_bus_lock(target->controller);
 if (ret) goto out_clk;
 rate=clk_get_rate(spi_clk);
 divider=DIV_ROUND_CLOSEST(rate,1695000UL);
 divider=(divider+1)&~1U;
 if (divider<2 || divider>65534 || rate<100000000UL) { ret=-EINVAL; goto out_bus; }
 /* Check ownership/layout before touching pin mux or controller state. */
 saved_cs=readl(spi_regs+CS);
 saved_clk=readl(spi_regs+CLK);
 saved_sclk=readl(gpio_regs+4)&SCLK_MASK;
 if ((saved_cs&CS_TA) || saved_sclk!=(4u<<3) ||
     ((readl(gpio_regs+8)>>6)&7)!=1 || !(readl(gpio_regs+0x34)&BIT(22))) {
  ret=-EBUSY; goto out_bus;
 }
 /* Native CS=2 means neither native CE pin. GPIO22 alone selects the CAP. */
 mode=2 | ((target->mode&SPI_CPHA)?BIT(2):0) | ((target->mode&SPI_CPOL)?BIT(3):0);
 local_irq_save(irq_flags);
 deadline=ktime_get_mono_fast_ns()+5000000ULL;
 writel(divider,spi_regs+CLK);
 ret=command(0xdc,mode,deadline);
 if (!ret) {
  writel(BIT(11),gpio_regs+0x1c);
  writel((readl(gpio_regs+4)&~SCLK_MASK)|(1u<<3),gpio_regs+4);
  readl(gpio_regs+4);
  ret=pio(data,wave.length,mode,deadline);
 }
 writel((readl(gpio_regs+4)&~SCLK_MASK)|saved_sclk,gpio_regs+4);
 readl(gpio_regs+4);
 /* Even on TX timeout, recover normal framing and the RX path. */
 if (command(0xd1,mode,ktime_get_mono_fast_ns()+100000ULL) && !ret) ret=-EIO;
 writel(BIT(22),gpio_regs+0x1c);
 writel(saved_clk,spi_regs+CLK);
 writel(saved_cs&~(CS_TA|BIT(8)|BIT(9)|BIT(10)|CS_CLEAR),spi_regs+CS);
 local_irq_restore(irq_flags);
 out_bus: spi_bus_unlock(target->controller);
 out_clk: clk_disable_unprepare(spi_clk);
 out_pm: if (runtime_ref) {
  pm_runtime_mark_last_busy(target->controller->dev.parent);
  pm_runtime_put_autosuspend(target->controller->dev.parent);
 }
 out_mutex: mutex_unlock(&tx_lock); kfree(data); return ret;
}
static const struct file_operations rfid_fops={
 .owner=THIS_MODULE,.unlocked_ioctl=rfid_ioctl,
#ifdef CONFIG_COMPAT
 .compat_ioctl=rfid_ioctl,
#endif
};
static struct miscdevice rfid_misc={.minor=MISC_DYNAMIC_MINOR,.name="rfid_mfc_tx",.fops=&rfid_fops,.mode=0660};
static int __init rfid_init(void)
{
 struct device *dev;
 struct device_node *node;
 struct resource resource;
 int ret;
 dev=bus_find_device_by_name(&spi_bus_type,NULL,"spi0.2");
 if (!dev) return -ENODEV;
 target=to_spi_device(dev);
 if (!of_device_is_compatible(target->controller->dev.parent->of_node,"brcm,bcm2835-spi")) { ret=-ENODEV; goto fail_dev; }
 if (of_address_to_resource(target->controller->dev.parent->of_node,0,&resource)) { ret=-ENODEV; goto fail_dev; }
 if (resource.start!=0x3f204000) { ret=-ENODEV; goto fail_dev; }
 spi_regs=ioremap(resource.start,resource_size(&resource));
 if (!spi_regs) { ret=-ENOMEM; goto fail_dev; }
 node=of_find_compatible_node(NULL,NULL,"brcm,bcm2835-gpio");
 if (!node) { ret=-ENODEV; goto fail_spi; }
 ret=of_address_to_resource(node,0,&resource); of_node_put(node);
 if (ret || resource.start!=0x3f200000) { ret=-ENODEV; goto fail_spi; }
 gpio_regs=ioremap(resource.start,0x100);
 if (!gpio_regs) { ret=-ENOMEM; goto fail_spi; }
 spi_clk=clk_get(target->controller->dev.parent,NULL);
 if (IS_ERR(spi_clk)) { ret=PTR_ERR(spi_clk); goto fail_gpio; }
 ret=misc_register(&rfid_misc);
 if (ret) goto fail_clk;
 pr_info("rfid_mfc_tx: Cardputer Zero SPI0 helper ready, clock=%lu\n",clk_get_rate(spi_clk));
 return 0;
 fail_clk: clk_put(spi_clk);
 fail_gpio: iounmap(gpio_regs);
 fail_spi: iounmap(spi_regs);
 fail_dev: put_device(dev); return ret;
}
static void __exit rfid_exit(void)
{
 misc_deregister(&rfid_misc);clk_put(spi_clk);iounmap(gpio_regs);iounmap(spi_regs);put_device(&target->dev);
}
module_init(rfid_init);module_exit(rfid_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Bounded Cardputer Zero ST25R3916 transparent TX and RX turnaround");
