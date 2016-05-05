#ifndef MNDOMU_H_INCLUDED
#define MNDOMU_H_INCLUDED

int send_request_to_dom0(void);

ssize_t file_write (struct file *filp, const char __user *buff,
                    unsigned long len, void *data);

int file_read (char* page, char**start, off_t off,
               int count, int *eof, void *data);

int create_procfs_entry(char* filename);

static irqreturn_t as_int (int irq, void *dev_id);

static int __init init_domumodule(void);

void __exit cleanup_domumodule(void);

#endif // XEN-EG_H_INCLUDED
