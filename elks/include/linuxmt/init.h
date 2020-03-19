#ifndef __LINUXMT_INIT_H
#define __LINUXMT_INIT_H

/* Assorted initializers */

#include <linuxmt/types.h>

struct task_struct;
struct gendisk;

extern int directhd_init(void);
extern int rs_init(void);

extern void buffer_init(void);
extern void floppy_init(void);
extern void fs_init(void);
extern void idt_init(void);
extern void inode_init(void);
extern void irqtab_init(void);
extern void lp_init(void);
extern void meta_init(void);
extern void mm_init(seg_t,seg_t);
extern void pmodekern_init(void);
extern void proto_init(void);
extern void pty_init(void);
extern void rd_init(void);
extern void sched_init(void);
extern void sock_init(void);
extern void ssd_init(void);
extern void tcpdev_init(void);
extern void eth_init (void);
extern void tty_init(void);
extern void xtk_init(void);

extern void init_console(void);
extern void device_setup(void);

extern void kfork_proc(void ());
extern void arch_setup_user_stack(struct task_struct *, word_t entry);
extern void setup_dev(register struct gendisk *);
extern void mem_dev_init(void);
extern void init_bioshd(void);

void eth_drv_init (void);

#endif
