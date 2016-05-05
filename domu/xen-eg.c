#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/ethtool.h>
#include <linux/in.h>
#include <linux/if_ether.h>
#include <linux/io.h>
#include <linux/moduleparam.h>
#include <net/sock.h>
#include <net/pkt_sched.h>
#include <net/arp.h>
#include <net/route.h>
#include <asm/uaccess.h>
#include <asm/page.h>
#include <asm/xen/page.h>
//#include <xenpvdrivers/evtchn.h>
#include <xen/evtchn.h>
//#include <xenpvdrivers/xenbus.h>
#include <xen/xenbus.h>
//#include <xenpvdrivers/interface/io/netif.h>
#include <xen/interface/io/netif.h>
//#include <xenpvdrivers/interface/memory.h>
#include <xen/interface/memory.h>
//#include <xenpvdrivers/balloon.h>
#include <xen/balloon.h>
//#include <xenpvdrivers/asm/maddr.h>
//#include <mach-xen/asm/maddr*.h>
//#include <xenpvdrivers/grant_table.h>
#include <xen/interface/grant_table.h>

#define AUTHOR "Charles Xu <xuchi.int@gmail.com>"
#define DESCRIPTION "MemNet architecture domu module"


//module parameter
static char *filename = "";
module_param(filename, charp, 0644);
static char *ip = "";
module_param(ip, charp, 0644);
static int port;
module_param(port, int, 0644);

//int page;
void *page;
struct as_request
{
    unsigned int id; /* private guest value echoed in resp */
    unsigned int status;
    unsigned int operation;
};

struct as_response
{
    unsigned int id; /* copied from request */
    unsigned int status;
    unsigned int operation; /* copied from request */
};

// The following makes the as_sring, as_back_ring, as_back_ring "types"
DEFINE_RING_TYPES(as, struct as_request, struct as_response);

struct info_t
{
    struct as_front_ring ring;
    grant_ref_t gref;
    int irq;
    int port;
} info;

#define DOM0_ID 0

// Related the proc fs entries
static struct proc_dir_entry *proc_dir = NULL;
static struct proc_dir_entry *proc_file = NULL;
char proc_data[20];

#ifdef SHARED_MEM
/*
* Send an request via the shared ring to Dom0, following by an INT
*/

int send_request_to_dom0(void)
{
    struct as_request *ring_req;
    int notify;
    static int reqid=9;

    /* Write a request into the ring and update the req-prod pointer */
    ring_req = RING_GET_REQUEST(&(info.ring), info.ring.req_prod_pvt);
    ring_req->id = reqid;
    ring_req->operation = reqid;
    ring_req->status = reqid;
    printk("\nxen:DomU: Fill in IDX-%d, with id=%d, op=%d, st=%d",
           info.ring.req_prod_pvt, ring_req->id, ring_req->operation,
           ring_req->status);
    reqid++;
    info.ring.req_prod_pvt += 1;

// Send a reqest to backend followed by an int if needed
    RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(&(info.ring), notify);

    if (notify)
    {
        printk("\nxen:DomU: Sent a req to Dom0");
        notify_remote_via_irq(info.irq);
    }
    else
    {
        printk("\nxen:DomU: No notify req to Dom0");
        notify_remote_via_irq(info.irq);
    }
    printk("...\n");
    return 0;
}

ssize_t file_write (struct file *filp, const char __user *buff,
                    unsigned long len, void *data)
{
    int value;

    printk("\nxen:domU: file_write %lu bytes", len);
    //copy from user : copy data from user space to kernel space,
    //if failed return the bytes for the copied data
    //if succeed return 0
    //buff->proc
    if (copy_from_user(&proc_data[0], buff, len))
        return -EFAULT;
    proc_data[len] = '\x0';
    //printk(" ,%s", &proc_data[0]);
    value = simple_strtol(proc_data, 0, 10); //transfer string to a long signed int


    switch(value)
    {
    case 1:
        send_request_to_dom0();
        printk(" ,value = %d", value);
        break;
    default:
        printk(" ,value not recognized !");
    }
    return len;
}

int file_read (char* page, char**start, off_t off,
               int count, int *eof, void *data)
{
    sprintf(page, "%s", proc_data);
//

//把格式化的数据写入某个字符串缓冲区 写入page
    return strlen(page);
}

/*
* We create a /proc/memnet/filename entry. When we write a "1" into this file once
* the module is loaded, the file_write function() above is called and this
* sends a request on the shared ring to the Dom0. This way we test the
* event channel and shared ring routines.
*/
int create_procfs_entry(char* filename)//create vitual folder and file
{
    int ret = 0;

    proc_dir = proc_mkdir("memnet", NULL);
    if (!proc_dir)
    {
        printk("\nxen:domU Could not create memnet entry in procfs");
        ret = -EAGAIN;
        return ret;
    }
    /*create_proc_entry() function create a virtual folder in the /proc folder
    this function take a file name, e.g. "file"; access info; location as the input, and
    return the proc_dir_entry pointer (NULL if failed)*/


    proc_file = create_proc_entry(filename, 0644, proc_dir);
    if (proc_file)
    {
        proc_file->read_proc = file_read;
        proc_file->write_proc = file_write;
#if PROC_OWNER
        proc_file->owner = THIS_MODULE;
#endif
    }
    else
    {
        printk("\nxen:domU Could not create /proc/demo/"+filename);
        ret = -EAGAIN;
        return ret;
    }
    return ret;
}

/*
* Our interrupt handler for event channel that we set up
*/

static irqreturn_t as_int (int irq, void *dev_id)//irq handler
{
    struct as_response *ring_resp;
    RING_IDX i, rp;

    printk("\nxen:DomU: as_int called");
again:
    rp = info.ring.sring->rsp_prod;
    printk("\nxen:DomU: ring pointers %d to %d", info.ring.rsp_cons, rp);
    for(i=info.ring.rsp_cons; i != rp; i++)
    {
        unsigned long id;
// what did we get from Dom0
        ring_resp = RING_GET_RESPONSE(&(info.ring), i);
        printk("\nxen:DomU: Recvd in IDX-%d, with id=%d, op=%d, st=%d",
               i, ring_resp->id, ring_resp->operation, ring_resp->status);
        id = ring_resp->id;
        switch(ring_resp->operation)
        {
        case 0:
            printk("\nxen:DomU: operation:0");
            break;
        default:
            break;
        }
    }

    info.ring.rsp_cons = i;
    if (i != info.ring.req_prod_pvt)
    {
        int more_to_do;
        RING_FINAL_CHECK_FOR_RESPONSES(&info.ring, more_to_do);
        if(more_to_do)
            goto again;
    }
    else
        info.ring.sring->rsp_event = i+1;
    return IRQ_HANDLED;
}
#endif

//int __init init_domumodule(void)
int init_module()
{
    int mfn;
#ifdef ENABLE_EVENT_IRQ
    int err;
#endif
    struct as_sring *sring;

    //Use command line argument to parse a file
    //Command line parameter filename, ip, port
    printk("Ready to open %s and send to Dom0.\n Then send to ip = %s, port = %d",filename, ip, port);


    create_procfs_entry(filename);


    /*
    * Allocates and returns a pointer to the first byte of a memory area
    * that is several physically contiguous pages long, and doesn't zero
    * out the area.
    * GFP_KERNEL - process may sleep
    */


    /*
    在linux内核空间申请内存涉及的函数主要包括kmalloc()、__get_free_pages()和vmalloc()等。
    kmalloc()和__get_free_pages()申请的内存位于物理内存映射区域（《896M，所以容易操作，
    可以得到虚拟地址与物理地址），而且在物理上也是连续的，它们与真实的物理地址只有一个
    固定的偏移，因此存在简单的转换关系。而vmalloc()在虚拟内存空间给出一块连续的内存空间
    （>896,虚拟地址上连续），实质上，这片连续的虚拟内存在物理内存中并不一定连续，
    而vmalloc()申请的虚拟内存和物理内存之间也没有简单的换算关系。*/


    /*
    *get_order function calculate order number
    *
    *
    */

    //order for page size 16*1024=16KB
    //int order = get_order(16*1024);
    //printk("\nxen:DomU: Page size = %d",PAGE_SIZE);


    /*
    * __get_free_pages() returns a 32-bit address, which cannot represent
    * a highmem page
    * order MAX_ORDER=10 or 11, usually less than 5
    */
    int order = 5;
    //page = __get_free_pages(GFP_KERNEL, 1);
    printk("Now try to allocate 2^%d pages, of which page size is %d",order,PAGE_SIZE);
    page = __get_free_pages(GFP_KERNEL, order);


    if (page == 0)
    {
        printk("\nxen:DomU: could not get free page");
        return 0;
    }

#if ENABLE_SHARED_RING
    /* Put a shared ring structure on this page */

    sring = (struct as_sring*) page;

    SHARED_RING_INIT(sring);
    /*前端分配一个用于共享通信 ring 的内存页, 授权它给后端domain, 并放授权引用到xenstore,
    这样后端就能 map 这个页. 有共享ring这个页是一个主页, 用于传递更多的授权引用*/

    /* info.ring is the front_ring structure */

    FRONT_RING_INIT(&(info.ring), sring, PAGE_SIZE);
#endif


    /*
     VIRT <-> MACHINE conversion
     mfn: machine frame numbers

    */
    mfn = virt_to_mfn(page);

    /*
    * The following grant table func is in drivers/xen/grant-table.c
    * For shared pages, used for synchronous data, advertise a page to
    * be shared via the hypervisor fu[nction call gnttab_grant_foreign_access.
    * This call notifies the hypervisor that other domains are allowed to
    * access this page.
    *
    * gnttab_map() has been called earlier to setup gnttable_setup_table
    * during init phase, with a call to HYPERVISOR_grant_table_op(
    * GNTTAB_setup_table...) and
    * "shared" pages have been malloc'ed. This "shared" page is then used
    * below later during the actual grant of a ref by this DOM.
    *
    * gnttab_grant_foreign_access()
    * => get_free_entries
    * gnttab_free_head - points to the ref of the head
    * gnttab_free_count- keeps number of free refs
    *
    * Get a ref id by calling gnttab_entry(head)
    * gnttab_list[entry/RPP][entry%RPP]
    * => gnttab_grat_foreign_access_ref
    * =>update_grant_entry
    * shared[ref].frame/domid/flags are updated
    * "shared" above is a pointer to struct grant_entry (flags/domid/frame)
    */

    info.gref = gnttab_grant_foreign_access(DOM0_ID, mfn, 0);

    if (info.gref < 0)
    {

        printk("\nxen: could not grant foreign access");

        free_page((unsigned long)page);

        return 0;

    }

    /*
    * The following strcpy is commented out, but was used initally to test
    * is the memory page is indeed shared with Dom0, when in Dom0, we do a
    * sprintf of the same memory location and get the same characters.
    */


    strcpy((char*)page, "chixu:test123456789ABCDEF");
    /*
    * TBD: Save gref to be sent via Xenstore to dom-0. As of now both the
    * gref and the event channel port id is sent manually during insmod
    * in the dom0 module.
    */

    printk("\n gref = %d", info.gref);

    /* Setup an event channel to Dom0 */
#ifdef ENABLE_EVENT_IRQ
    err = bind_listening_port_to_irqhandler(DOM0_ID, as_int, 0,
                                            "xen-eg", &info);
    if (err < 0)
    {
        printk("\nxen:DomU failed to setup evtchn !");
        gnttab_end_foreign_access(info.gref, 0, page);
        return 0;
    }

    info.irq = err;
    info.port = irq_to_evtchn_port(info.irq);
    printk(" interupt = %d, local-port = %d", info.irq, info.port);
    printk("....\n...");
    //----chix====
    //create_procfs_entry();
#endif
    return 0;
}

//void __exit cleanup_domumodule(void)
void cleanup_module()
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

    /* Cleanup proc entry */
    remove_proc_entry(filename, NULL);

    remove_proc_entry("memnet", proc_dir);

    printk("....\n...");
}

//module_init(init_domumodule);
//module_exit(cleanup_domumodule);
MODULE_LICENSE("GPL");
MODULE_AUTHOR(AUTHOR);
MODULE_DESCRIPTION(DESCRIPTION);
