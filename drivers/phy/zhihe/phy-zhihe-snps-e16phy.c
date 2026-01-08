// SPDX-License-Identifier: GPL-2.0
/*
 * Synopsys DesignWare E16 PCIe 3.0 PHY driver for ZHIHE SoC
 *
 * Copyright (C) ZHIHE
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/phy/pcie.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset.h>

/* Registers definition for ZHIHE A210 */
#define E16PHY_GLB_CTRL_REG		0x00000000
#define  PYH_SATA_MODE			BIT(0)
#define  PHY_PCIE_X1_MODE		BIT(8)
#define  PHY0_CR_PARA_SEL		BIT(16)
#define  PHY1_CR_PARA_SEL		BIT(20)

#define E16PHY_SRC_SEL_REG		0x00000004
#define  PIPE_LANE0_PHY_SRC_SEL_SHIFT	0
#define  PIPE_LANE1_PHY_SRC_SEL_SHIFT	4
#define  PIPE_LINE2_PHY_SRC_SEL_SHIFT	8
#define  PIPE_LINE3_PHY_SRC_SEL_SHIFT	12
#define  PHY0				0
#define  PHY1				1

#define E16PHY_PROTLCOL_REG		0x00000008
#define  LANE0_PROTOCOL_SHIFT		0
#define  LANE1_PROTOCOL_SHIFT		4
#define  LINE2_PROTOCOL_SHIFT		8
#define  LINE3_PROTOCOL_SHIFT		12
#define  SATA_MODE			2
#define  PCIE_MODE			0

#define E16PHY_RES_RTUNE_REG		0x00000048
#define  PHY_RTUNE_REQ			BIT(0)
#define  PHY_RTUNE_ACK			BIT(4)
#define  PHY_RES_REQ_IN			BIT(8)
#define  PHY_RES_REQ_OUT		BIT(12)
#define  PHY_RES_ACK_IN			BIT(16)
#define  PHY_RES_ACK_OUT		BIT(20)

#define E16PHY_PCIE_EXT_CTRL_REG2	0x00000108

#define E16PHY_PHY0_MPLL_REG		0x00000034
#define E16PHY_PHY1_MPLL_REG		0x00000038
#define  MPLLA_STATE			BIT(12)
#define  MPLLB_STATE			BIT(28)

#define E16PHY_PHY0_PPM_REG		0x00000050
#define E16PHY_PHY1_PPM_REG		0x00000054

struct e16phy_seq {
	u32 addr;
	u32 val;
};

struct zhihe_e16phy_priv {
	void __iomem *mmio;
	int mode;
	struct reset_control *apb_rst;
	struct reset_control *phy_rst;
	struct phy *phy;
	struct clk_bulk_data *clks;
	int num_clks;
	struct e16phy_seq *init_seq;
	int num_init_seq;
	struct gpio_descs *base_en;
	struct gpio_descs *sata_en;
	struct gpio_descs *pcie_en;
};

static void e16phy_dump_mmio(struct device *dev, void __iomem *start,
			     unsigned int bytes)
{
	unsigned int b, w, o, offset = 0;
	unsigned char linebuf[38];

	for (b = 0; b < bytes;) {
		for (w = 0, o = 0; b < bytes && w < 4; w++) {
			o += scnprintf(linebuf + o, sizeof(linebuf) - o,
				       "%08x ", readl(start + b));
			b += sizeof(u32);
		}
		dev_info(dev, "%03x: %s\n", offset, linebuf);
		offset += w * sizeof(u32);
	}
}

static int zhihe_e16phy_init(struct phy *phy)
{
	struct zhihe_e16phy_priv *priv = phy_get_drvdata(phy);
	unsigned int timeout;
	int ret, i;
	u32 val;

	ret = clk_bulk_prepare_enable(priv->num_clks, priv->clks);
	if (ret) {
		dev_err(&priv->phy->dev, "failed to enable PCIe bulk clks %d\n",
			ret);
		return ret;
	}

	reset_control_deassert(priv->apb_rst);
	reset_control_assert(priv->phy_rst);
	usleep_range(100, 150);

	for (i = 0; i < priv->num_init_seq; i++) {
		struct e16phy_seq *seq = &priv->init_seq[i];

		writel(seq->val, priv->mmio + seq->addr);
		dev_info(&phy->dev, "[0x%03x] = 0x%08x\n",
			 seq->addr, seq->val);
	}

	reset_control_deassert(priv->phy_rst);

	/* Wait for PHY RTUNE acknowledgment */
	timeout = 100;
	while (timeout--) {
		val = readl(priv->mmio + E16PHY_RES_RTUNE_REG);
		if (val & PHY_RTUNE_ACK)
			break;
		udelay(1);
	}

	if (!timeout) {
		dev_err(&phy->dev, "PHY RTUNE timeout\n");
		ret = -ETIMEDOUT;
		goto err_disable_clks;
	}

	e16phy_dump_mmio(&phy->dev, priv->mmio, 0x100);
	mdelay(200);

	return 0;

err_disable_clks:
	clk_bulk_disable_unprepare(priv->num_clks, priv->clks);
	return ret;
}

static int zhihe_e16phy_exit(struct phy *phy)
{
	struct zhihe_e16phy_priv *priv = phy_get_drvdata(phy);

	clk_bulk_disable_unprepare(priv->num_clks, priv->clks);
	reset_control_assert(priv->phy_rst);
	reset_control_assert(priv->apb_rst);

	return 0;
}

static void zhihe_e16phy_gpio_set_value(struct gpio_descs *gpios, int val)
{
	if (!gpios)
		return;
	for (int i = 0; i < gpios->ndescs; i++)
		gpiod_set_value(gpios->desc[i], val);
}

static int zhihe_e16phy_set_mode(struct phy *phy, enum phy_mode mode, int submode)
{
	struct zhihe_e16phy_priv *priv = phy_get_drvdata(phy);

	/* Actually We don't care EP/RC mode, but just record it */
	switch (mode) {
	case PHY_MODE_SATA:
		priv->mode = PHY_MODE_SATA;
		zhihe_e16phy_gpio_set_value(priv->base_en, 1);
		zhihe_e16phy_gpio_set_value(priv->sata_en, 1);
		break;
	case PHY_MODE_PCIE:
		priv->mode = PHY_MODE_PCIE;
		zhihe_e16phy_gpio_set_value(priv->base_en, 1);
		zhihe_e16phy_gpio_set_value(priv->pcie_en, 1);
		break;
	default:
		dev_err(&phy->dev, "%s, invalid mode\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static const struct phy_ops zhihe_e16phy_ops = {
	.init = zhihe_e16phy_init,
	.exit = zhihe_e16phy_exit,
	.set_mode = zhihe_e16phy_set_mode,
	.owner = THIS_MODULE,
};

static int zhihe_e16phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct zhihe_e16phy_priv *priv;
	struct phy_provider *phy_provider;
	const char *mode_name;
	char prop_name[64];
	int ret, size;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->mmio = devm_platform_get_and_ioremap_resource(pdev, 0, NULL);
	if (IS_ERR(priv->mmio))
		return PTR_ERR(priv->mmio);

	priv->phy = devm_phy_create(dev, NULL, &zhihe_e16phy_ops);
	if (IS_ERR(priv->phy)) {
		dev_err(dev, "failed to create combphy\n");
		return PTR_ERR(priv->phy);
	}

	priv->apb_rst = devm_reset_control_get_shared(dev, "apb");
	if (IS_ERR(priv->apb_rst))
		return dev_err_probe(dev, PTR_ERR(priv->apb_rst),
				     "failed to get apb reset control\n");

	priv->phy_rst = devm_reset_control_get_shared(dev, "phy");
	if (IS_ERR(priv->phy_rst))
		return dev_err_probe(dev, PTR_ERR(priv->phy_rst),
				     "failed to get phy reset control\n");

	priv->num_clks = devm_clk_bulk_get_all(dev, &priv->clks);
	if (priv->num_clks < 1)
		return -ENODEV;

	/* Get init-seq-select property to determine which init-seq to use */
	ret = of_property_read_string(dev->of_node, "init-seq-select",
				      &mode_name);
	if (ret) {
		dev_err(dev, "failed to get init-seq-select property\n");
		return ret;
	}

	snprintf(prop_name, sizeof(prop_name), "init-seq-%s", mode_name);
	size = of_property_count_u32_elems(dev->of_node, prop_name);
	if (size < 0) {
		dev_err(dev, "failed to find property '%s'\n", prop_name);
		return -EINVAL;
	}

	priv->num_init_seq = size / 2;
	priv->init_seq = devm_kmalloc_array(dev, priv->num_init_seq,
					    sizeof(*priv->init_seq),
					    GFP_KERNEL);
	if (!priv->init_seq)
		return -ENOMEM;

	ret = of_property_read_u32_array(dev->of_node, prop_name,
					  (u32 *)priv->init_seq, size);
	if (ret) {
		dev_err(dev, "failed to read property '%s'\n", prop_name);
		return ret;
	}

	/* Get base-en-gpios property */
	priv->base_en = devm_gpiod_get_array_optional(&pdev->dev, "base-en",
						       GPIOD_OUT_LOW);
	if (IS_ERR(priv->base_en))
		return dev_err_probe(&pdev->dev, PTR_ERR(priv->base_en),
				     "Failed to get base-en GPIO");

	priv->pcie_en = devm_gpiod_get_array_optional(&pdev->dev, "pcie-en",
						       GPIOD_OUT_LOW);
	if (IS_ERR(priv->pcie_en))
		return dev_err_probe(&pdev->dev, PTR_ERR(priv->pcie_en),
				     "Failed to get pcie-en GPIO");

	priv->sata_en = devm_gpiod_get_array_optional(&pdev->dev, "sata-en",
						       GPIOD_OUT_LOW);
	if (IS_ERR(priv->sata_en))
		return dev_err_probe(&pdev->dev, PTR_ERR(priv->sata_en),
				     "Failed to get sata-en GPIO");

	dev_set_drvdata(dev, priv);
	phy_set_drvdata(priv->phy, priv);

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);

	return PTR_ERR_OR_ZERO(phy_provider);
}

static const struct of_device_id zhihe_e16phy_of_match[] = {
	{ .compatible = "zhihe,a210-e16phy" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, zhihe_e16phy_of_match);

static struct platform_driver zhihe_e16phy_driver = {
	.probe = zhihe_e16phy_probe,
	.driver = {
		.name = "zhihe-snps-pcie3-phy",
		.of_match_table = zhihe_e16phy_of_match,
	},
};
module_platform_driver(zhihe_e16phy_driver);

MODULE_DESCRIPTION("Zhihe Synopsys PCIe 3.0 PHY driver");
MODULE_LICENSE("GPL");
