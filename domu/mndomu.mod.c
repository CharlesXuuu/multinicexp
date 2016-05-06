#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
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
	{ 0xbff2abe1, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x5d41c87c, __VMLINUX_SYMBOL_STR(param_ops_charp) },
	{ 0xb6b46a7c, __VMLINUX_SYMBOL_STR(param_ops_int) },
	{ 0xedbc6f67, __VMLINUX_SYMBOL_STR(gnttab_end_foreign_access) },
	{ 0xb4e14553, __VMLINUX_SYMBOL_STR(gnttab_query_foreign_access) },
	{ 0xb6230f1f, __VMLINUX_SYMBOL_STR(gnttab_grant_foreign_access) },
	{ 0xfe727411, __VMLINUX_SYMBOL_STR(get_phys_to_machine) },
	{ 0x55526907, __VMLINUX_SYMBOL_STR(xen_features) },
	{ 0x4c9d28b0, __VMLINUX_SYMBOL_STR(phys_base) },
	{ 0x93fca811, __VMLINUX_SYMBOL_STR(__get_free_pages) },
	{        0, __VMLINUX_SYMBOL_STR(filp_close) },
	{ 0x41ef837e, __VMLINUX_SYMBOL_STR(vfs_read) },
	{ 0x4c4fef19, __VMLINUX_SYMBOL_STR(kernel_stack) },
	{ 0x83d8a344, __VMLINUX_SYMBOL_STR(filp_open) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "675363D2C4D0AB15E6E612E");
