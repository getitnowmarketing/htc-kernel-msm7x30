/* linux/arch/arm/mach-msm/board-lexikon-wifi.c
*/
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <asm/mach-types.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <linux/skbuff.h>
#ifdef CONFIG_BCM4329_PURE_ANDROID
#include <linux/wlan_plat.h>
#else
#include <linux/wifi_tiwlan.h>
#endif

#include "board-lexikon.h"

int lexikon_wifi_power(int on);
int lexikon_wifi_reset(int on);
int lexikon_wifi_set_carddetect(int on);

#define PREALLOC_WLAN_NUMBER_OF_SECTIONS	4
#define PREALLOC_WLAN_NUMBER_OF_BUFFERS		160
#define PREALLOC_WLAN_SECTION_HEADER		24

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 1024)

#define WLAN_SKB_BUF_NUM	16

static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

typedef struct wifi_mem_prealloc_struct {
	void *mem_ptr;
	unsigned long size;
} wifi_mem_prealloc_t;

static wifi_mem_prealloc_t wifi_mem_array[PREALLOC_WLAN_NUMBER_OF_SECTIONS] = {
	{ NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER) }
};

static void *lexikon_wifi_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_NUMBER_OF_SECTIONS)
		return wlan_static_skb;
	if ((section < 0) || (section > PREALLOC_WLAN_NUMBER_OF_SECTIONS))
		return NULL;
	if (wifi_mem_array[section].size < size)
		return NULL;
	return wifi_mem_array[section].mem_ptr;
}

int __init lexikon_init_wifi_mem(void)
{
	int i;

	for(i=0;( i < WLAN_SKB_BUF_NUM );i++) {
		if (i < (WLAN_SKB_BUF_NUM/2))
			wlan_static_skb[i] = dev_alloc_skb(4096);
		else
			wlan_static_skb[i] = dev_alloc_skb(8192);
	}
	for(i=0;( i < PREALLOC_WLAN_NUMBER_OF_SECTIONS );i++) {
		wifi_mem_array[i].mem_ptr = kmalloc(wifi_mem_array[i].size,
							GFP_KERNEL);
		if (wifi_mem_array[i].mem_ptr == NULL)
			return -ENOMEM;
	}
	return 0;
}

static struct resource lexikon_wifi_resources[] = {
	[0] = {
		.name		= "bcm4329_wlan_irq",
		.start		= MSM_GPIO_TO_INT(LEXIKON_GPIO_WIFI_IRQ),
		.end		= MSM_GPIO_TO_INT(LEXIKON_GPIO_WIFI_IRQ),
#ifdef CONFIG_BCM4329_PURE_ANDROID
		.flags		= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL | IORESOURCE_IRQ_SHAREABLE,
#else
		.flags          = IORESOURCE_IRQ | IORESOURCE_IRQ_LOWEDGE,
#endif
	},
};

static struct wifi_platform_data lexikon_wifi_control = {
	.set_power      = lexikon_wifi_power,
	.set_reset      = lexikon_wifi_reset,
	.set_carddetect = lexikon_wifi_set_carddetect,
	.mem_prealloc   = lexikon_wifi_mem_prealloc,
#ifndef CONFIG_BCM4329_PURE_ANDROID
	.dot11n_enable  = 1,
#endif
};

static struct platform_device lexikon_wifi_device = {
        .name           = "bcm4329_wlan",
        .id             = 1,
        .num_resources  = ARRAY_SIZE(lexikon_wifi_resources),
        .resource       = lexikon_wifi_resources,
        .dev            = {
                .platform_data = &lexikon_wifi_control,
        },
};

extern unsigned char *get_wifi_nvs_ram(void);

static unsigned lexikon_wifi_update_nvs(char *str)
{
#define NVS_LEN_OFFSET		0x0C
#define NVS_DATA_OFFSET		0x40
	unsigned char *ptr;
	unsigned len;

	if (!str)
		return -EINVAL;
	ptr = get_wifi_nvs_ram();
	/* Size in format LE assumed */
	memcpy(&len, ptr + NVS_LEN_OFFSET, sizeof(len));

	/* the last bye in NVRAM is 0, trim it */
	if (ptr[NVS_DATA_OFFSET + len -1] == 0)
		len -= 1;

	if (ptr[NVS_DATA_OFFSET + len -1] != '\n') {
		len += 1;
		ptr[NVS_DATA_OFFSET + len -1] = '\n';
	}

	strcpy(ptr + NVS_DATA_OFFSET + len, str);
	len += strlen(str);
	memcpy(ptr + NVS_LEN_OFFSET, &len, sizeof(len));
	return 0;
}

int __init lexikon_wifi_init(void)
{
	int ret;

	printk(KERN_INFO "%s: start\n", __func__);
	lexikon_wifi_update_nvs("sd_oobonly=1\n");
	lexikon_wifi_update_nvs("btc_params80=0\n");
	lexikon_wifi_update_nvs("btc_params6=30\n");
	lexikon_wifi_update_nvs("btc_params70=0x32\n");
	lexikon_init_wifi_mem();
	ret = platform_device_register(&lexikon_wifi_device);
	return ret;
}
