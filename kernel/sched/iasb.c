/*
 * Input Assisted SchedTune Boost
 *
 * Copyright (c) 2019 Nanda Oktavera <codeharuka.yusa@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/input.h>

static bool is_boosted;
static struct workqueue_struct *iasb_wq;
static struct work_struct iasb_work_boost;
static struct delayed_work iasb_work_restore;
static unsigned short custom_duration_ms;
static unsigned short finger_down_time;
static int boost_slot;

static bool enable = true;
static unsigned short boost_percent = CONFIG_DYNAMIC_STUNE_BOOST_VALUE;
static unsigned short boost_duration_ms = CONFIG_IASB_BOOST_MS;
static unsigned short min_touch_duration_ms = CONFIG_IASB_MIN_TOUCH_MS;

module_param(enable, bool, 0644);
module_param(boost_percent,  ushort, 0644);
module_param(boost_duration_ms, ushort, 0644);
module_param(min_touch_duration_ms, ushort, 0644);

static void __exec_boost(unsigned short duration_ms)
{
	if (!cancel_delayed_work_sync(&iasb_work_restore)) {
		do_stune_boost("top-app", boost_percent, &boost_slot);
		is_boosted = true;
	}

	queue_delayed_work(iasb_wq, &iasb_work_restore,
				msecs_to_jiffies(duration_ms));
}

void iasb_exec_boost(unsigned short duration_ms)
{
	/* skip boost when another custom boosting is active */
	if (is_boosted && (custom_duration_ms > 0))
		return;

	__exec_boost(duration_ms);	
	custom_duration_ms = duration_ms;
}

void iasb_kill_boost(void)
{
	reset_stune_boost("top-app", boost_slot);
	is_boosted = false;
	custom_duration_ms = 0;
}

static void iasb_stune_booster(struct work_struct *work)
{
	if (!is_boosted)
		__exec_boost(boost_duration_ms);	
}

static void iasb_stune_restorer(struct work_struct *work)
{
	if (is_boosted)
		iasb_kill_boost();
}

static int iasb_event_filter(unsigned int type, unsigned int code, int value)
{
	unsigned long interval;

	/* Skip boost when touch released */
	if (code == BTN_TOUCH && value == 0)
		return 1;

	/* Boost immediately when touch is a swipe or scroll */
	if (code == ABS_MT_TOUCH_MINOR && value > 8)
		goto out;

	/* Recording finger down time */
	if (code == ABS_MT_TRACKING_ID && value != -1) {
		finger_down_time = ktime_to_ms(ktime_get());
		return 1;
	}

	/* Boost when touch is reach the minimal requirement */
	interval = ktime_to_ms(ktime_get()) - finger_down_time;
	if (interval < min_touch_duration_ms)
		return 1;

out:
	finger_down_time = 0;
	return 0;
}

static void iasb_input_event(struct input_handle *handle,
		unsigned int type, unsigned int code, int value)
{
	if(!enable && iasb_event_filter(type, code, value))
		return;

	queue_work(iasb_wq, &iasb_work_boost);
}

static int iasb_input_connect(struct input_handler *handler,
		struct input_dev *dev, const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "iasb_handler";

	error = input_register_handle(handle);
	if (error)
		goto err_free_handle;

	error = input_open_device(handle);
	if (error)
		goto err_unreg_handle;

	iasb_wq = alloc_workqueue("iasb_worker", WQ_POWER_EFFICIENT, 0);
	if (!iasb_wq)
		return -EFAULT;

	INIT_WORK(&iasb_work_boost, iasb_stune_booster);
	INIT_DELAYED_WORK(&iasb_work_restore, iasb_stune_restorer);

	return 0;

err_unreg_handle:
	input_unregister_handle(handle);
err_free_handle:
	kfree(handle);
	return error;
}

static void iasb_input_disconnect(struct input_handle *handle)
{
	iasb_kill_boost();
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id iasb_eventids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
				INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_ABS) },
		.absbit = {
				BIT_MASK(ABS_MT_TRACKING_ID) | 
				BIT_MASK(ABS_MT_TOUCH_MINOR) |
				BIT_MASK(ABS_X) | 
				BIT_MASK(ABS_Y)
			},
	},
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
				INPUT_DEVICE_ID_MATCH_KEYBIT,
		.evbit = { BIT_MASK(EV_KEY) },
		.keybit = { BIT_MASK(BTN_TOUCH) },
	},
	{ },
};

static struct input_handler iasb_event_handler = {
	.name           = "iasb_evhandler",
	.event          = iasb_input_event,
	.connect        = iasb_input_connect,
	.disconnect     = iasb_input_disconnect,
	.id_table       = iasb_eventids,
};

static int __init iasb_init(void)
{
	return input_register_handler(&iasb_event_handler);
}

late_initcall(iasb_init);
