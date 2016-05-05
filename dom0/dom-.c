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


struct gnttab_map_grant_ref   ops;//根据（dom，GR）奖对应的页映射到自己的地址空间
struct gnttab_unmap_grant_ref unmap_ops;//撤销页映射


struct as_request
{
    unsigned int  id;           /* private guest value, echoed in resp  */
    unsigned int  status;
    unsigned int  operation;
};


struct as_response
{
    unsigned int  id;              /* copied from request */
    unsigned int  status;
    unsigned int  operation;       /* copied from request */
};


typedef struct as_request as_request_t;
typedef struct as_response as_response_t;


// From /include/xen/interface/io/ring.h
// The following makes the as_sring, as_back_ring, as_back_ring "types"
DEFINE_RING_TYPES(as, struct as_request, struct as_response);//#define DEFINE_RING_TYPES(__name, __req_t, __rsp_t)
struct info_t
{
    int irq;
    int gref;
    int remoteDomain;
    int evtchn;
    struct as_back_ring ring;
} info;


int gref;
int port;
module_param(gref, int, 0644);//在domU中 gref port需要手动加入 init_module()
module_param(port, int, 0644);//编写一个内核模块则通过module_param()传递参数


/*关于中断处理函数的返回值:中断程序的返回值是一个特殊类型—irqreturn_t。但是中断程序的返回值却只有两个—IRQ_NONE和IRQ_HANDLED。

#ifndef _LINUX_IRQRETURN_H
#define _LINUX_IRQRETURN_H
typedef int irqreturn_t;

#define IRQ_NONE       (0)
#define IRQ_HANDLED       (1)
#define IRQ_RETVAL(x)      ((x) != 0)  //这个宏只是返回0或非0

#endif*/


#if ENABLE_SRING
static irqreturn_t as_int (int irq, void *dev_id)//io环操作 **请求，应答
{
    RING_IDX rc, rp;//typedef unsigned int RING_IDX;
    as_request_t req;
    as_response_t resp;
    int more_to_do, notify;


    // dev_id is a pointer to the info structure
    printk("\nxen:Dom0: as_int called with dev_id %x info=%x",
           (unsigned int)dev_id, (unsigned int)&info);
    rc = info.ring.req_cons;
    rp = info.ring.sring->req_prod;
    printk("  rc =%d rp =%d", rc, rp);
    while(rc!=rp)
    {
        /*
        define RING_REQUEST_CONS_OVERFLOW(_r, _cons)
        (((_cons) - (_r)->rsp_prod_pvt) >= RING_SIZE(_r))


        */
        if(RING_REQUEST_CONS_OVERFLOW(&info.ring, rc))//RING 请求溢出
            break;
        // what did we get from the frontend at index rc
        memcpy(&req, RING_GET_REQUEST(&info.ring, rc), sizeof(req));
        resp.id = req.id;
        resp.operation = req.operation;
        resp.status = req.status+1; // Send back a status +1 of what was recvd
        printk("\nxen:Dom0: Recvd at IDX-%d: id=%d, op=%d, status=%d",
               rc, req.id, req.operation, req.status);


        // update the req-consumer
        info.ring.req_cons = ++rc;
        barrier();//防止读写出错
        switch (req.operation)
        {
        case 0:
            printk("\nxen:Dom0: req.operation = 0");
            break;
        default:
            printk("\nxen:Dom0: req.operation = %d", req.operation);
            break;
        }


        memcpy(RING_GET_RESPONSE(&info.ring, info.ring.rsp_prod_pvt),
               &resp, sizeof(resp));
        info.ring.rsp_prod_pvt++;
        RING_PUSH_RESPONSES_AND_CHECK_NOTIFY(&info.ring, notify);
        if(info.ring.rsp_prod_pvt == info.ring.req_cons)
        {
            RING_FINAL_CHECK_FOR_REQUESTS(&info.ring, more_to_do);
        }
        else if (RING_HAS_UNCONSUMED_REQUESTS(&info.ring))    //还有未处理req
        {
            more_to_do = 1;
        }


        if(notify)
        {
            printk("\nxen:Dom0: Send notify to DomU");
            notify_remote_via_irq(info.irq);
        }
    }


    return IRQ_HANDLED;
}
#endif

static int __init init_dom0module(void)
{
    struct vm_struct *v_start;
#if ENABLE_SRING
    as_sring_t *sring;
#endif
    int err;

    info.gref = gref;
    info.remoteDomain = 1;
    info.evtchn = port;
    printk("\nxen: dom0: init_module with gref = %d", info.gref);

    // The following function reserves a range of kernel address space and
    // allocates pagetables to map that range. No actual mappings are created.
    v_start = alloc_vm_area(PAGE_SIZE, NULL);//分配虚拟地址结构
    if (v_start == 0)  //无法分配
    {
        free_vm_area(v_start);
        printk("\nxen: dom0: could not allocate page");
        return -EFAULT;
    }


    /* struct vm_struct {
        struct vm_struct    *next;//指向下一虚拟地址，加速查询
        void            *addr;//地址
        unsigned long       size;//大小
        unsigned long       flags;//标志
        struct page     **pages;//页指针
        unsigned int        nr_pages;
        unsigned long       phys_addr;//物理地址
    };
    struct vm_struct *alloc_vm_area(size_t size, NULL)；//分配虚拟地址结构
    void free_vm_area(struct vm_struct *area)；//释放虚拟地址结构*/


    /*
     * ops struct in paramaeres
     * host_addr, flags, ref
     * ops struct out parameters
     * status (zero if OK), handle (used to unmap later), dev_bus_addr
     */
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


#if ENABLE_SRING
    sring = (as_sring_t*)v_start->addr;
    BACK_RING_INIT(&info.ring, sring, PAGE_SIZE);


    /* Seetup an event channel to the frontend */
    err = bind_interdomain_evtchn_to_irqhandler(info.remoteDomain,
            info.evtchn, as_int, 0, "dom0-backend", &info);
    if (err < 0)
    {
        printk("\nxen: dom0: init_module failed binding to evtchn !");
        err = HYPERVISOR_grant_table_op(GNTTABOP_unmap_grant_ref,
                                        &unmap_ops, 1);
        return -EFAULT;
    }


    info.irq = err;


    printk("\nxen: dom0: end init_module: int = %d", info.irq);
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
MODULE_AUTHOR("Charles Xu");
