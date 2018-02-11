/*
 * Copyright (C) 2018, Ryan Andri <https://github.com/ryan-andri>
 *
 * Lightweight utility for dynamic limit min/max cpu "sched core control"
 * Based on FB Notifier. Purposed to limiting min/max cpu for each cluster
 * while device on suspend state. when resume triggered by screen on,
 * restoring again min/max cpu to user config (previous) min/max
 * for each cpu cluster.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. For more details, see the GNU
 * General Public License included with the Linux kernel or available
 * at www.gnu.org/licenses
 */

#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#include <linux/module.h>

/* enabled by default */
static int enabled = 1;
module_param(enabled, uint, 0664);

/* 2 seconds for enter suspend */
#define DEF_SUSPEND_TIME 2000

/* Worker */
static struct delayed_work suspend_work;
static struct work_struct resume_work;

/* Notifier block */
static struct notifier_block ctl_nb;

/* prevent scheduling work when device on boot */
static bool booted = false;

static bool suspend = false;

/* functions from core_ctl */
extern void core_ctl_suspend_work(bool suspended);

static void corectl_resume(struct work_struct *work)
{
	if (enabled || suspend) {
		core_ctl_suspend_work(false);
		suspend = false;
	}
}

static void corectl_suspend(struct work_struct *work)
{
	if (enabled) {
		core_ctl_suspend_work(true);
		suspend = true;
	}
}

static int corectl_notifier_cb(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;

	if (evdata && evdata->data &&
		event == FB_EVENT_BLANK) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK) {
			if (!booted) {
				booted = true;
				return 0;
			}
			cancel_delayed_work_sync(&suspend_work);
			schedule_work(&resume_work);
		} else if (*blank == FB_BLANK_POWERDOWN) {
			flush_work(&resume_work);
			schedule_delayed_work(&suspend_work,
					msecs_to_jiffies(DEF_SUSPEND_TIME));
		}
	}

	return 0;
}

static int __init core_ctl_suspend_init(void)
{
	INIT_WORK(&resume_work, corectl_resume);
	INIT_DELAYED_WORK(&suspend_work, corectl_suspend);

	ctl_nb.notifier_call = corectl_notifier_cb;
	if (fb_register_client(&ctl_nb))
		pr_info("%s: failed register to fb notifier\n", __func__);

	return 0;
}

MODULE_AUTHOR("Ryan Andri <ryanandri@linuxmail.org");
MODULE_DESCRIPTION("'core_ctl_suspend' - Lightweight utility for dynamic limit min/max cpu "
		"'sched core control' Based on FB Notifier");
MODULE_LICENSE("GPL");

late_initcall(core_ctl_suspend_init);
