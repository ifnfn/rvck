// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2025 Zhihe Computing Limited.
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset.h>

#include "core.h"

struct dwc3_zhihe {
	struct device		*dev;
	struct regmap		*usb31_sysreg;
	struct clk_bulk_data	*clks;
	int			num_clocks;
	struct reset_control	*resets;
};

static int dwc3_zhihe_probe(struct platform_device *pdev)
{
	struct device		*dev = &pdev->dev;
	struct device_node	*np = dev->of_node;
	struct dwc3_zhihe	*zhihe;
	struct device_node	*dwc3_np;
	struct platform_device	*dwc3_pdev;
	int			ret;

	zhihe = devm_kzalloc(&pdev->dev, sizeof(*zhihe), GFP_KERNEL);
	if (!zhihe)
		return -ENOMEM;

	platform_set_drvdata(pdev, zhihe);
	zhihe->dev = &pdev->dev;

	zhihe->usb31_sysreg = syscon_regmap_lookup_by_phandle(np, "syscon");
	if (IS_ERR(zhihe->usb31_sysreg))
		return dev_err_probe(dev, PTR_ERR(zhihe->usb31_sysreg),
				     "Failed to get regmap\n");

	ret = clk_bulk_get_all(dev, &zhihe->clks);
	if (ret < 0)
		return dev_err_probe(dev, ret,
				     "failed to get DWC3 bulk clks\n");
	zhihe->num_clocks = ret;

	zhihe->resets = devm_reset_control_array_get_exclusive(dev);
	if (IS_ERR(zhihe->resets))
		return dev_err_probe(dev, PTR_ERR(zhihe->resets),
				     "failed to get DWC3 resets\n");

	ret = clk_bulk_prepare_enable(zhihe->num_clocks, zhihe->clks);
	if (ret)
		return dev_err_probe(dev, ret, "failed to enable DWC3 bulk clks\n");

	ret = reset_control_deassert(zhihe->resets);
	if (ret)
		goto disable_clks;

	dwc3_np = of_get_compatible_child(np, "snps,dwc3");
	if (!dwc3_np) {
		ret = dev_err_probe(dev, -ENODEV, "No DWC3 subnode found\n");
		goto assert_resets;
	}

	ret = of_platform_populate(np, NULL, NULL, dev);
	if (ret) {
		dev_err_probe(dev, ret, "failed to register dwc3 core\n");
		goto put_dwc3_np;
	}

	dwc3_pdev = of_find_device_by_node(dwc3_np);
	if (!dwc3_pdev) {
		of_platform_depopulate(dev);
		ret = dev_err_probe(dev, -ENODEV,
				    "failed to get dwc3 platform device\n");
		goto put_dwc3_np;
	}
	platform_device_put(dwc3_pdev);

	of_node_put(dwc3_np);

	return 0;

put_dwc3_np:
	of_node_put(dwc3_np);
assert_resets:
	reset_control_assert(zhihe->resets);
disable_clks:
	clk_bulk_disable_unprepare(zhihe->num_clocks, zhihe->clks);
	return ret;
}

static int dwc3_zhihe_remove(struct platform_device *pdev)
{
	struct dwc3_zhihe *zhihe = platform_get_drvdata(pdev);

	of_platform_depopulate(zhihe->dev);

	reset_control_assert(zhihe->resets);
	clk_bulk_disable_unprepare(zhihe->num_clocks, zhihe->clks);

	return 0;
}

static const struct of_device_id dwc3_zhihe_of_match[] = {
	{ .compatible = "zhihe,a210-dwc3" },
	{ },
};
MODULE_DEVICE_TABLE(of, dwc3_zhihe_of_match);

static struct platform_driver dwc3_zhihe_driver = {
	.probe		= dwc3_zhihe_probe,
	.remove		= dwc3_zhihe_remove,
	.driver		= {
		.name	= "dwc3-zhihe",
		.of_match_table	= dwc3_zhihe_of_match,
	},
};

module_platform_driver(dwc3_zhihe_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DesignWare DWC3 ZHIHE Glue Driver");
MODULE_AUTHOR("Zhiguo Zhu <zhiguo.zhu@linux.alibaba.com>");
