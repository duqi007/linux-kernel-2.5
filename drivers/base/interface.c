/*
 * drivers/base/interface.c - common driverfs interface that's exported to 
 * 	the world for all devices.
 * Copyright (c) 2002 Patrick Mochel
 *		 2002 Open Source Development Lab
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/stat.h>

/**
 * device_read_status - report some device information
 * @page:	page-sized buffer to write into
 * @count:	number of bytes requested
 * @off:	offset into buffer
 * @data:	device-specific data
 *
 * Report some human-readable information about the device.
 * This includes the name, the bus id, and the current power state.
 */
static ssize_t device_read_status(struct device * dev, char * page, size_t count, loff_t off)
{
	return off ? 0 : sprintf(page,"%s\n",dev->bus_id);
}

/**
 * device_write_status - forward a command to a driver
 * @buf:	encoded command
 * @count:	number of bytes in buffer
 * @off:	offset into buffer to start with
 * @data:	device-specific data
 *
 * Send a comamnd to a device driver.
 * The following actions are supported:
 * probe - scan slot for device
 * remove - detach driver from slot
 * suspend <state> <stage> - perform <stage> for entering <state>
 * resume <stage> - perform <stage> for waking device up.
 * (See Documentation/driver-model.txt for the theory of an n-stage
 * suspend sequence).
 */
static ssize_t device_write_status(struct device * dev, const char* buf, size_t count, loff_t off)
{
	char command[20];
	int num;
	int arg = 0;
	int error = 0;

	if (off)
		return 0;

	/* everything involves dealing with the driver. */
	if (!dev->driver)
		return 0;

	num = sscanf(buf,"%10s %d",command,&arg);

	if (!num)
		return 0;

	if (!strcmp(command,"probe")) {
		if (dev->driver->probe)
			error = dev->driver->probe(dev);

	} else if (!strcmp(command,"remove")) {
		if (dev->driver->remove)
			error = dev->driver->remove(dev,REMOVE_NOTIFY);
	} else
		error = -EFAULT;
	return error < 0 ? error : count;
}

static struct driver_file_entry device_status_entry = {
	name:		"status",
	mode:		S_IWUSR | S_IRUGO,
	show:		device_read_status,
	store:		device_write_status,
};

static ssize_t device_read_name(struct device * dev, char * buf, size_t count, loff_t off)
{
	return off ? 0 : sprintf(buf,"%s\n",dev->name);
}

static struct driver_file_entry device_name_entry = {
	name:	"name",
	mode:	S_IRUGO,
	show:	device_read_name,
};

static ssize_t
device_read_power(struct device * dev, char * page, size_t count, loff_t off)
{
	return off ? 0 : sprintf(page,"%d\n",dev->current_state);
}

static ssize_t
device_write_power(struct device * dev, const char * buf, size_t count, loff_t off)
{
	char	str_command[20];
	char	str_level[20];
	int	num_args;
	u32	state;
	u32	int_level;
	int	error = 0;

	if (off)
		return 0;

	if (!dev->driver)
		goto done;

	num_args = sscanf(buf,"%10s %10s %u",str_command,str_level,&state);

	error = -EINVAL;

	if (!num_args)
		goto done;

	if (!strnicmp(str_command,"suspend",7)) {
		if (num_args != 3)
			goto done;
		if (!strnicmp(str_level,"notify",6))
			int_level = SUSPEND_NOTIFY;
		else if (!strnicmp(str_level,"save",4))
			int_level = SUSPEND_SAVE_STATE;
		else if (!strnicmp(str_level,"disable",7))
			int_level = SUSPEND_DISABLE;
		else if (!strnicmp(str_level,"powerdown",8))
			int_level = SUSPEND_POWER_DOWN;
		else
			goto done;

		if (dev->driver->suspend)
			error = dev->driver->suspend(dev,state,int_level);
		else
			error = 0;
	} else if (!strnicmp(str_command,"resume",6)) {
		if (num_args != 2)
			goto done;

		if (!strnicmp(str_level,"poweron",7))
			int_level = RESUME_POWER_ON;
		else if (!strnicmp(str_level,"restore",7))
			int_level = RESUME_RESTORE_STATE;
		else if (!strnicmp(str_level,"enable",6))
			int_level = RESUME_ENABLE;
		else
			goto done;

		if (dev->driver->resume)
			error = dev->driver->resume(dev,int_level);
		else
			error = 0;
	}
 done:
	return error < 0 ? error : count;
}

static struct driver_file_entry device_power_entry = {
	name:		"power",
	mode:		S_IWUSR | S_IRUGO,
	show:		device_read_power,
	store:		device_write_power,
};

struct driver_file_entry * device_default_files[] = {
	&device_status_entry,
	&device_name_entry,
	&device_power_entry,
	NULL,
};
