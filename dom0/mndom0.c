#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>


#if 0
#include <xen/interface/grant_table.h>
#include <xen/interface/io/blkif.h>  // for definition of blkif_sring_t
#include <xen/gnttab.h>
#include <linux/vmalloc.h>
#include <asm-x86/xen/hypervisor.h>
#include <xen/evtchn.h>
#else
#include <linux/vmalloc.h>
#include <xen/xen.h>
#include <xen/xenbus.h>
#include <xen/events.h>
#include <xen/page.h>
#include <xen/grant_table.h>


#include <xen/interface/io/netif.h>
#include <xen/interface/memory.h>
#include <xen/interface/grant_table.h>
#endif

#define AUTHOR "Charles Xu <xuchi.int@gmail.com>"
#define DESCRIPTION "MemNet architecture dom0 module"


struct gnttab_map_grant_ref   ops;//根据（dom，GR）奖对应的页映射到自己的地址空间
struct gnttab_unmap_grant_ref unmap_ops;//撤销页映射



int gref;
int port;
module_param(gref, int, 0644);//在domU中 gref port需要手动加入 init_module()
module_param(port, int, 0644);//编写一个内核模块则通过module_param()传递参数

static int __init init_dom0module(void)
{
    struct vm_struct *v_start;


    int remoteDomain = 1;
    int evtchn = port;
    printk("\nxen: dom0: init_module with gref = %d", gref);

    // The following function reserves a range of kernel address space and
    // allocates pagetables to map that range. No actual mappings are created.
    v_start = alloc_vm_area(PAGE_SIZE, NULL);//分配虚拟地址结构
    if (v_start == 0)  //无法分配
    {
        free_vm_area(v_start);
        printk("\nxen: dom0: could not allocate page");
        return -EFAULT;
    }


//分配内存
    gnttab_set_map_op(&ops, (unsigned long)v_start->addr, GNTMAP_host_map,
                      info.gref, info.remoteDomain); /* flags, ref, domID */
//GNTTABOP_map_grant_ref  **操作码（映射到自己空间）
//HYPERVISOR_grant_table_op超级调用
    if (HYPERVISOR_grant_table_op(GNTTABOP_map_grant_ref, &ops, 1))
    {
        printk("\nxen: dom0: HYPERVISOR map grant ref failed");
        return -EFAULT;
    }


    if (ops.status)
    {
        printk("\nxen: dom0: HYPERVISOR map grant ref failed status = %d",
               ops.status);
        return -EFAULT;
    }
    printk("\nxen: dom0: shared_page = %x, handle = %x, status = %x",
           (unsigned int)v_start->addr, ops.handle, ops.status);


    // Used for unmapping
    unmap_ops.host_addr = (unsigned long)(v_start->addr);
    unmap_ops.handle = ops.handle;

#define ENABLE_PRINT_PAGE 1
#if ENABLE_PRINT_PAGE  //验证DomU page中写入的字符串"chix"
    {
        int i;
        printk("\nBytes in page ");
        for(i=0; i<=100; i++)
        {
            printk("%c", ((char*)(v_start->addr))[i]);
        }
    }
#endif


    printk("\nXEN: dom: end init_module\n");
    return 0;
}


static void __exit cleanup_dom0module(void)
{
    int ret;


    printk("\nxen: dom0: cleanup_module");
    // Unmap foreign frames
    // ops.handle points to the pages that were initially mapped. Set in the
    // __init() function
    //ops.host_addr ponts to the heap where the pages were mapped
    ret = HYPERVISOR_grant_table_op(GNTTABOP_unmap_grant_ref, &unmap_ops, 1);
    if (ret == 0)
    {
        printk(" cleanup_module: unmapped shared frame");
    }
    else
    {
        printk(" cleanup_module: unmapped shared frame failed");
    }
    printk("...\n");
}

module_init(init_dom0module);
module_exit(cleanup_dom0module);
MODULE_LICENSE("GPL");
MODULE_AUTHOR(AUTHOR);
MODULE_DESCRIPTION(DESCRIPTION);
