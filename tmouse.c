#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt


#include <linux/slab.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/device.h>



#include <linux/irqreturn.h>

#include <asm/irq.h>
#include <asm/io.h>
static char *name="tmouse";
// This file makes a device, that hte evdev module can open, and so it gets registed as /dev/input/eventX
// But then if I use the evbug base, it will get all events, including this one
MODULE_LICENSE("GPL");

static char devname[32] = {0};

static struct input_dev *button_dev;

static void evbug_event(struct input_handle *handle, unsigned int type, unsigned int code, int value)
{
	if( strcmp(dev_name(&handle->dev->dev), dev_name(&button_dev->dev)) != 0 ){
//	if( strcmp(dev_name(&handle->dev->dev), "input2") == 4000 ){
		input_event(button_dev, type, code, value);
		//input_sync(button_dev);
	}


        //printk(KERN_DEBUG pr_fmt("Event. Dev: %s, Type: %d, Code: %d, Value: %d\n"),
          //     dev_name(&handle->dev->dev), type, code, value);
}


static int evbug_connect(struct input_handler *handler, struct input_dev *dev,
                         const struct input_device_id *id)
{
        struct input_handle *handle;
        int error;

        handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
        if (!handle)
                return -ENOMEM;

        handle->dev = dev;
        handle->handler = handler;
        handle->name = "tmouse";

        error = input_register_handle(handle);
        if (error)
                goto err_free_handle;

        error = input_open_device(handle);
        if (error)
                goto err_unregister_handle;

        printk(KERN_DEBUG pr_fmt("Connected device: %s (%s at %s)\n"),
               dev_name(&dev->dev),
               dev->name ?: "unknown",
               dev->phys ?: "unknown");

        return 0;

 err_unregister_handle:
        input_unregister_handle(handle);
 err_free_handle:
        kfree(handle);
        return error;
}



static void evbug_disconnect(struct input_handle *handle)
{
        printk(KERN_DEBUG pr_fmt("Disconnected device: %s\n"),
               dev_name(&handle->dev->dev));

        input_close_device(handle);
        input_unregister_handle(handle);
        kfree(handle);
}


static const struct input_device_id evbug_ids[] = {
        {
                 .flags = INPUT_DEVICE_ID_MATCH_EVBIT |
                                 INPUT_DEVICE_ID_MATCH_KEYBIT |
                                 INPUT_DEVICE_ID_MATCH_RELBIT,
                 .evbit = { BIT_MASK(EV_KEY) | BIT_MASK(EV_REL) },
                 .keybit = { [BIT_WORD(BTN_LEFT)] = BIT_MASK(BTN_LEFT) },
                 .relbit = { BIT_MASK(REL_X) | BIT_MASK(REL_Y) },
         },      /* A mouse like device, at least one button,
                    two relative axes */
        { },                    /* Terminating zero entry */
};


MODULE_DEVICE_TABLE(input, evbug_ids);

static struct input_handler evbug_handler = {
        .event =        evbug_event,
        .connect =      evbug_connect,
        .disconnect =   evbug_disconnect,
        .name =         "tmouse",
        .id_table =     evbug_ids,
};




static int __init button_init(void)
{
	int error;

	struct input_id id = { 
		.bustype = 0x03, 
		.vendor = 0x0, 
		.product = 0x03, 
		.version = 0x110,
};
	button_dev = input_allocate_device();
	if (!button_dev) {
		printk(KERN_ERR "tmouse.c: Not enough memory\n");
		error = -ENOMEM;
		goto err_free_irq;
	}
	dev_set_name(&button_dev->dev, "tmouse");


	button_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_REL) | BIT_MASK(EV_MSC);
	button_dev->keybit[BIT_WORD(BTN_LEFT)] |= BIT_MASK(BTN_LEFT);
	button_dev->keybit[BIT_WORD(BTN_RIGHT)] |= BIT_MASK(BTN_RIGHT);
	button_dev->keybit[BIT_WORD(BTN_MIDDLE)] |= BIT_MASK(BTN_MIDDLE);
	button_dev->keybit[BIT_WORD(BTN_SIDE)] |= BIT_MASK(BTN_SIDE);
	button_dev->keybit[BIT_WORD(BTN_EXTRA)] |= BIT_MASK(BTN_EXTRA);
	button_dev->keybit[BIT_WORD(BTN_FORWARD)] |= BIT_MASK(BTN_FORWARD);
	button_dev->keybit[BIT_WORD(BTN_BACK)] |= BIT_MASK(BTN_BACK);
	button_dev->keybit[BIT_WORD(BTN_TASK)] |= BIT_MASK(BTN_TASK);

	button_dev->relbit[BIT_WORD(REL_X)] |= BIT_MASK(REL_X);
	button_dev->relbit[BIT_WORD(REL_Y)] |= BIT_MASK(REL_Y);
	button_dev->relbit[BIT_WORD(REL_WHEEL)] |= BIT_MASK(REL_WHEEL);

	button_dev->mscbit[BIT_WORD(MSC_SCAN)] |= BIT_MASK(MSC_SCAN);


	button_dev->name = name;

	button_dev->id = id;

	error = input_register_device(button_dev);
	if (error) {
		printk(KERN_ERR "button.c: Failed to register device\n");
		goto err_free_dev;
	}


	strncpy(devname, dev_name(&button_dev->dev), 32 );
	if( devname[31] != 0 ){
		printk(KERN_ERR "button.c: Failed to obtain name %s\n", dev_name(&button_dev->dev));
		goto err_free_dev;
	}
	error = input_register_handler(&evbug_handler);
	if (error) {
		printk(KERN_ERR "button.c: Failed to register device\n");
		goto err_free_dev;
	}
	return 0;

 err_free_dev:
	input_free_device(button_dev);

 err_free_irq:
	return error;
}

static void __exit button_exit(void)
{



	input_unregister_handler(&evbug_handler);

        input_unregister_device(button_dev);
}

module_init(button_init);
module_exit(button_exit);
