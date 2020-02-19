// SPDX-License-Identifier: GPL-2.0
/*
 * Dummy clk driver (to set/enable clocks for testing purposes)
 *
 * Copyright 2020 TOPIC Embedded Products. All rights reserved.
 *
 */

#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_clk.h>

MODULE_DESCRIPTION("Dummy clock driver");
MODULE_AUTHOR("TOPIC Embedded Products");
MODULE_LICENSE("GPL v2");

#define DRIVER_NAME "topic-dummy-clk"

struct dummy_clk_item {
	u32 id;
	struct clk *clock;
	u32 frequency;
	bool enabled;
};

struct dummy_clk_data {
	struct platform_device *pdev;
	struct dummy_clk_item *clocks;
	u32 n_clocks;
};

static int dummy_clk_enable(struct dummy_clk_data *data,
	struct dummy_clk_item *clock_item)
{
	int err;

	if (clock_item->enabled)
		return 0;

	err = clk_set_rate(clock_item->clock,
		clock_item->frequency);
	if (err < 0) {
		dev_err(&data->pdev->dev,
			"Failed to set clock frequency of clock %u to %u Hz\n",
			clock_item->id, clock_item->frequency);
		return err;
	}

	err = clk_prepare_enable(clock_item->clock);
	if (err < 0) {
		dev_err(&data->pdev->dev, "Failed to enable clock %u\n",
			clock_item->id);
		return err;
	}

	dev_info(&data->pdev->dev, "Clock %u enabled at %lu Hz\n",
		clock_item->id, clk_get_rate(clock_item->clock));

	clock_item->enabled = true;

	return 0;
}

static void dummy_clk_disable(struct dummy_clk_data *data,
	struct dummy_clk_item *clock_item)
{
	if (!clock_item->enabled)
		return;

	clk_disable_unprepare(clock_item->clock);

	dev_info(&data->pdev->dev, "Clock %u disabled\n", clock_item->id);
	clock_item->enabled = false;
}

static int dummy_clk_probe(struct platform_device *pdev)
{
	struct dummy_clk_data *data;
	int i;
	int ret;
	u32 *tmp;

	dev_info(&pdev->dev, "Dummy clk driver probing...\n");

	data = devm_kzalloc(&pdev->dev, sizeof(*data),
		GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	/* Connect driver data to platform device */
	platform_set_drvdata(pdev, data);
	data->pdev = pdev;

	/* Check number of clocks in devicetree */
	data->n_clocks = of_clk_get_parent_count(pdev->dev.of_node);
	if (data->n_clocks == 0) {
		dev_info(&pdev->dev, "No clocks found in devicetree\n");
		return -EINVAL;
	}
	dev_info(&pdev->dev, "Found %u clocks in devicetree\n", data->n_clocks);

	/* Allocate memory for clock data */
	data->clocks = devm_kmalloc_array(&pdev->dev, data->n_clocks,
		sizeof(struct dummy_clk_item), GFP_KERNEL);
	if (!data->clocks)
		return -ENOMEM;

	/* Get clock-info from devicetree */
	for (i = 0; i < data->n_clocks; ++i) {

		/* Get clock */
		data->clocks[i].clock = of_clk_get(pdev->dev.of_node, i);
		if (data->clocks[i].clock == NULL) {
			dev_err(&pdev->dev, "Could not get clock %u\n",	i);
			return -EINVAL;
		}

		data->clocks[i].id = i;
		data->clocks[i].enabled = false;
	}

	/* Get frequencies */
	/* Get complete array and put in tmp array */
	tmp = kmalloc_array(data->n_clocks, sizeof(u32), GFP_KERNEL);
	ret = of_property_read_u32_array(pdev->dev.of_node, "clock-frequencies",
		tmp, data->n_clocks);
	if (ret) {
		dev_err(&pdev->dev,
			"Devicetree clock-frequencies missing for clock %u\n",
			i);
		return -EINVAL;
	}

	/* Copy frequencies to data struct and free tmp array */
	for (i = 0; i < data->n_clocks; ++i) {
		data->clocks[i].frequency = tmp[i];
		dev_info(&pdev->dev, "Got frequency %u Hz for clock %u\n",
		data->clocks[i].frequency, i);
	}
	kfree(tmp);

	/* Configure and enable all clocks */
	for (i = 0; i < data->n_clocks; ++i) {
		if (dummy_clk_enable(data, &data->clocks[i]) != 0) {
			dev_err(&pdev->dev, "Could not enable clock %u\n", i);
			return -EINVAL;
		}
	}

	dev_info(&pdev->dev, "clk-dummy-driver probed, enabled %u clocks\n",
		data->n_clocks);

	return 0;
}

/* ------------------------------------------------------------------ */

static int dummy_clk_remove(struct platform_device *pdev)
{
	struct dummy_clk_data *data = dev_get_platdata(&pdev->dev);
	int i;

	for (i = 0; i < data->n_clocks; ++i)
		dummy_clk_disable(data, &data->clocks[i]);

	return 0;
}

/* ------------------------------------------------------------------ */

static const struct of_device_id dummy_clk_id[] = {
	{ .compatible = "topic,dummy-clk" },
	{ },
};
MODULE_DEVICE_TABLE(of, dummy_clk_id);

static struct platform_driver dummy_clk_drvr = {
	.probe = dummy_clk_probe,
	.remove = dummy_clk_remove,
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = dummy_clk_id,
	},
};

module_platform_driver(dummy_clk_drvr);
