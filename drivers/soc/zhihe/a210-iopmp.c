// SPDX-License-Identifier: GPL-2.0
#include <asm/sbi.h>
#include <asm/a210-iopmp.h>

static long iopmp_ctrl(u32 *device_ids, u32 count,
		       int iopmp_ext_id, int fid)
{
	struct sbiret ret = {0};
	u32 *p_devices = device_ids;

	for (int i = 0; i < count; i += 5) {
		u32 devices[5] = {0};
		u32 send_count = min((count - i), (u32)5);
		int j;

		for (j = 0; j < send_count; j++)
			devices[j] = *(p_devices + j);

		ret = sbi_ecall(iopmp_ext_id, fid, send_count,
				devices[0], devices[1], devices[2],
				devices[3], devices[4]);
		if (ret.error)
			break;

		p_devices += send_count;
	}

	return ret.error ? ret.error : ret.value;
}

/**
 * iopmp_enable - enable iopmp config for domain
 * @device_ids: device id array
 * @count: number of device ids
 *
 * Return: sbi_ecall result
 */
long iopmp_enable(u32 *device_ids, u32 count)
{
	return iopmp_ctrl(device_ids, count, SBI_EXT_CONFIG_IOPMP,
			  SBI_EXT_CONFIG_IOPMP_ADD_RULE);
}
EXPORT_SYMBOL(iopmp_enable);

/**
 * iopmp_disable - disable iopmp config for domain
 * @device_ids: device id array
 * @count: number of device ids
 *
 * Return: sbi_ecall result
 */
long iopmp_disable(u32 *device_ids, u32 count)
{
	return iopmp_ctrl(device_ids, count, SBI_EXT_CONFIG_IOPMP,
			  SBI_EXT_CONFIG_IOPMP_REMOVE_RULE);
}
EXPORT_SYMBOL(iopmp_disable);
