#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = KBUILD_MODNAME,
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
 .arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x73a3c4ca, "module_layout" },
	{ 0xcc614f02, "usb_deregister" },
	{ 0xd3e47da0, "usb_register_driver" },
	{ 0xb295f519, "usb_find_interface" },
	{ 0x544588a9, "_raw_spin_unlock" },
	{ 0x9d56fa0a, "usb_deregister_dev" },
	{ 0x90db4d23, "dev_get_drvdata" },
	{ 0x38f3745f, "_raw_spin_lock" },
	{ 0x667c87bc, "usb_put_dev" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x43de957f, "usb_control_msg" },
	{ 0x2f287f0d, "copy_to_user" },
	{ 0x77638126, "usb_bulk_msg" },
	{ 0x37a0cba, "kfree" },
	{ 0x367e255c, "usb_set_interface" },
	{ 0xe0d98881, "usb_register_dev" },
	{ 0x7b66f186, "dev_set_drvdata" },
	{ 0x615bd38b, "usb_get_dev" },
	{ 0x83800bfa, "kref_init" },
	{ 0x9bd79589, "kmem_cache_alloc_trace" },
	{ 0xff64f6b3, "kmalloc_caches" },
	{ 0xd5b037e1, "kref_put" },
	{ 0x50eedeb8, "printk" },
	{ 0xb4390f9a, "mcount" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

MODULE_ALIAS("usb:v046Dp08CCd*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v046Dp0994d*dc*dsc*dp*ic*isc*ip*");

MODULE_INFO(srcversion, "6F3C18B503B701505A6CC5F");
