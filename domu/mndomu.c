#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <asm/page.h>
#include <asm/xen/page.h>
#include <xen/evtchn.h>
#include <xen/xenbus.h>
#include <xen/interface/io/netif.h>
#include <xen/interface/memory.h>
#include <xen/balloon.h>
#include <xen/interface/grant_table.h>

#include "mndomu.h"

#define AUTHOR "Charles Xu <xuchi.int@gmail.com>"
#define DESCRIPTION "MemNet architecture domu module"

#define DOM0_ID 0
#define SHARED_MEM 1

int page;

struct as_request
{
    unsigned int id; /* private guest value echoed in resp */
    unsigned int status;
    unsigned int operation;
};
struct as_response
{
    unsigned int  id; /* copied from request */
    unsigned int  status;
    unsigned int  operation; /* copied from request */
};

DEFINE_RING_TYPES(as, struct as_request, struct as_response);

struct info_t
{
    struct as_front_ring   ring;
    grant_ref_t gref;
    int irq;
    int port;
} info;

#define DOM0_ID 0

//module parameter
static char *filename = "";
module_param(filename, charp, 0644);
static char *ip = "";
module_param(ip, charp, 0644);
static int port;
module_param(port, int, 0644);

#define MAX_READBUFF_SIZE 128

static char writebuf[] = "";
static char readbuf[MAX_READBUFF_SIZE];

static int __init init_domumodule(void)
{
    int mfn;
#ifdef ENABLE_EVENT_IRQ
    int err;
#endif
    struct as_sring *sring;

    //Use command line argument to parse a file
    //Command line parameter filename, ip, port
    printk("Ready to open %s and send to Dom0.\n Then send to ip = %s, port = %d",filename, ip, port);

    struct file *fp;
    mm_segment_t fs;
    loff_t pos;

    fp =filp_open( filename ,O_RDWR | O_CREAT, 0644);

    if (IS_ERR(fp))
    {
        printk("DomU: cannot open file /n");
        return -1;
    }

    fs = get_fs();
    set_fs(KERNEL_DS);

    //if need to write file
    //pos =0;
    //vfs_write(fp,writebuf, sizeof(buf), &pos);

    pos =0;
    vfs_read(fp, readbuf, sizeof(readbuf), &pos);
    printk("read data: %s/n",readbuf);

    filp_close(fp,NULL);
    set_fs(fs);


    /*
    * Allocates and returns a pointer to the first byte of a memory area
    * that is several physically contiguous pages long, and doesn't zero
    * out the area.
    * GFP_KERNEL - process may sleep
    */

    /*
    *get_order function calculate order number
    */

    //order for page size 16*1024=16KB
    //int order = get_order(16*1024);
    //printk("\nxen:DomU: Page size = %d",PAGE_SIZE);


    /*
    * __get_free_pages() returns a 32-bit address, which cannot represent
    * a highmem page
    * order MAX_ORDER=10 or 11, usually less than 5
    */
    int order = 3;
    //page = __get_free_pages(GFP_KERNEL, 1);
    printk("Now try to allocate 2^%d pages, of which page size is %d",order,PAGE_SIZE);
    page = __get_free_pages(GFP_KERNEL, order);


    if (page == 0)
    {
        printk("\nxen:DomU: could not get free page");
        return 0;
    }

    mfn = virt_to_mfn(page);

    info.gref = gnttab_grant_foreign_access(DOM0_ID, mfn, 0);

    if (info.gref < 0)
    {

        printk("\nxen: could not grant foreign access");

        free_page((unsigned long)page);

        return 0;

    }

    strcpy((char*)page, readbuf);

    printk("\n gref = %d", info.gref);
    return 0;
}

void __exit cleanup_domumodule(void)
//void cleanup_module()
{
    printk("\nCleanup grant ref:");
    if (gnttab_query_foreign_access(info.gref) == 0)
    {
//Remove the grant to the page
        printk("\n xen: No one has mapped this frame");

// If 3rd param is non NULL, page has to be freed
        gnttab_end_foreign_access(info.gref, 0, page);
// free_pages(page,1);
    }
    else
    {
        printk("\n xen: Someone has mapped this frame");
// Guess, we still free the page, since we are rmmod-ed
        gnttab_end_foreign_access(info.gref, 0, page);
    }
}

module_init(init_domumodule);
module_exit(cleanup_domumodule);
MODULE_LICENSE("GPL");
MODULE_AUTHOR(AUTHOR);
MODULE_DESCRIPTION(DESCRIPTION);
