/*
 * Dummy clk driver (to set/enable clocks for testing purposes)
 *
 * Copyright 2018 TOPIC Embedded Products. All rights reserved.
 *
 * This program is free software; you may redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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

struct dummy_clk_data {
	struct platform_device *pdev;
	struct clk **clocks;
	u32 *clk_frequencies;
	bool *clk_enabled;
	u32 n_clocks;
};

static int dummy_clk_enable(struct dummy_clk_data *data, 
	int clk_nr)
{
	int err;

	if (data->clk_enabled[clk_nr])
		return 0;

	err = clk_set_rate(data->clocks[clk_nr], 
		data->clk_frequencies[clk_nr]);
	if (err < 0) {
		dev_err(&data->pdev->dev,
			"Failed to set clock frequency %u Hz\n",
			data->clk_frequencies[clk_nr]);
		return err;
	}
	
	dev_info(&data->pdev->dev, "Clock %u configured, freq: %u\n", 
		clk_nr, data->clk_frequencies[clk_nr]);

	err = clk_prepare_enable(data->clocks[clk_nr]);
	if (err < 0) {
		dev_err(&data->pdev->dev, "Failed to enable clock\n");
		return err;
	}
	
	dev_info(&data->pdev->dev, "Clock %u enabled\n", clk_nr);
	
	data->clk_enabled[clk_nr] = true;

	return 0;
}

static void dummy_clk_disable(struct dummy_clk_data *data,
	int clk_nr)
{
	if (!data->clk_enabled[clk_nr])
		return;

	clk_disable_unprepare(data->clocks[clk_nr]);
	
	dev_info(&data->pdev->dev, "Clock %u disabled\n", clk_nr);
	data->clk_enabled[clk_nr] = false;
}

static int dummy_clk_probe(struct platform_device *pdev)
{
	struct dummy_clk_data *data = dev_get_platdata(&pdev->dev);
	int i;
	int ret;
	
	dev_info(&pdev->dev, "Dummy clk driver probing...\n");

	data = devm_kzalloc(&pdev->dev, sizeof(struct dummy_clk_data),
			    GFP_KERNEL);
	if (!data) {
		dev_err(&pdev->dev, "Could not allocate memory\n");
		return -ENOMEM;
	}
	data->pdev = pdev;
	
	/* Check number of clocks in devicetree */
	data->n_clocks = of_clk_get_parent_count(pdev->dev.of_node);
	if (data->n_clocks == 0) {
		dev_info(&pdev->dev, "No clocks found in devicetree\n");
		return -ENOMEM;
	}
	dev_info(&pdev->dev, "Found %u clocks in devicetree\n", 
		data->n_clocks);
	
	/* Allocate memory for clock data */
	data->clocks = devm_kmalloc_array(&pdev->dev, data->n_clocks, 
		sizeof(struct clk*), GFP_KERNEL);
	data->clk_frequencies = devm_kzalloc(&pdev->dev, 
		sizeof(u32) * data->n_clocks, GFP_KERNEL);
	data->clk_enabled = devm_kzalloc(&pdev->dev, 
		sizeof(bool) * data->n_clocks, GFP_KERNEL);
	
	if (!data->clocks || !data->clk_frequencies || !data->clk_enabled) {
		dev_err(&pdev->dev, "Could not allocate memory\n");
		return -ENOMEM;
	}
	
	/* Get clocks from devicetree */
	for(i = 0; i < data->n_clocks; ++i)	{
		data->clocks[i] = of_clk_get(pdev->dev.of_node, i);
		if (data->clocks[i] == NULL) {
			dev_err(&pdev->dev, "Could not get clock %u, returned %p\n", 
				i, data->clocks[i]);
			return -EINVAL;
		}
	}
	
	/* Get frequencies for clocks from devicetree */
	ret = of_property_read_u32_array(pdev->dev.of_node, 
		"clock-frequencies", data->clk_frequencies, data->n_clocks);
	if (ret) {
		dev_err(&pdev->dev, "Devicetree clock-frequencies missing\n");
		return -EINVAL;
	}
		
	for(i = 0; i < data->n_clocks; ++i)	{
		if (dummy_clk_enable(data, i) != 0) {
			dev_err(&pdev->dev, "Could not enable clock\n");
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
	
	for(i = 0; i < data->n_clocks; ++i)
	{
		dummy_clk_disable(data, i);
	}
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
