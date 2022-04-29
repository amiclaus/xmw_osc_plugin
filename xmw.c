/**
 * Copyright (C) 2022 Analog Devices, Inc.
 *
 * Licensed under the GPL-2.
 *
 **/
#include <errno.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <math.h>

#include "../osc.h"
#include "../osc_plugin.h"
#include "../iio_widget.h"

#define THIS_DRIVER	"XMW"
#define CLK_DEVICE	"ad5356"
#define UPCONV		"admv1013"
#define DOWNCONV	"admv1014"

/*
 * For now leave this define as it is but, in future it would be nice if
 * the scaling is done in the driver so that, the plug in does not have
 * to knwow about  DAC_MAX_AMPLITUDE.
 */
#define DAC_MAX_AMPLITUDE	32767.0
#define SCALE_MINUS_INFINITE	-91.0
#define SCALE_MAX		0

static struct iio_context *ctx;

static struct iio_widget iio_widgets[25];
static unsigned int num_widgets;

static GtkWidget *xmw_panel;
static GtkWidget *scale_offset;
static GtkAdjustment *adj_scale;

static gboolean plugin_detached;
static gint this_page;
static int scale_offset_db = 0;

const gdouble mhz_scale = 1000000.0;

static GtkWidget *xmw_init(struct osc_plugin *plugin, GtkWidget *notebook,
			      const char *ini_fn)
{
	GtkBuilder *builder;
	struct iio_channel *dac_ch;
	struct iio_device *clk;
	struct iio_device *upconv;
	struct iio_device *downconv;
	int ret;

	builder = gtk_builder_new();

	ctx = osc_create_context();
	if (!ctx)
		return NULL;

	if (osc_load_glade_file(builder, "xmw") < 0) {
		osc_destroy_context(ctx);
		return NULL;
	}

	clk = iio_context_find_device(ctx, CLK_DEVICE);
	upconv = iio_context_find_device(ctx, UPCONV);
	downconv = iio_context_find_device(ctx, DOWNCONV);

	if (!clk || !upconv || !upconv) {
		printf("Could not find expected iio devices\n");
		osc_destroy_context(ctx);
		return NULL;
	}

	xmw_panel = GTK_WIDGET(gtk_builder_get_object(builder,
							 "xmw_panel"));
	scale_offset = GTK_WIDGET(gtk_builder_get_object(builder,
						"spinbutton_nco_offset"));
	adj_scale = GTK_ADJUSTMENT(gtk_builder_get_object(builder,
						"adj_altVoltage0_scale"));

	dac_ch = iio_device_find_channel(dac, "altvoltage0", true);

	ret = iio_device_attr_write_longlong(dac, "fir85_enable", 1);
	if (ret < 0) {
		fprintf(stderr, "Failed to enable FIR85. Error: %d\n", ret);
	}

	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], dac,
					      NULL, "sampling_frequency",
					      builder, "spinbutton_sample_freq",
					      &mhz_scale);

	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], dac,
					      dac_ch, "nco_frequency",
					      builder, "spinbutton_nco_freq",
					      &mhz_scale);

	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], dac,
					      dac_ch, "raw", builder,
					      "spinbutton_nco_scale", NULL);
	iio_spin_button_set_convert_function(&iio_widgets[num_widgets - 1],
					     db_full_scale_convert);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++],
					    dac_amp, NULL, "en", builder,
					    "dac_amplifier_enable", 0);


	make_widget_update_signal_based(iio_widgets, num_widgets);
	iio_update_widgets(iio_widgets, num_widgets);

	g_signal_connect(G_OBJECT(scale_offset), "value-changed",
			 G_CALLBACK(save_scale_offset), NULL);

	return xmw_panel;
}

static void update_active_page(struct osc_plugin *plugin, gint active_page,
			       gboolean is_detached)
{
	this_page = active_page;
	plugin_detached = is_detached;
}

static void xmw_get_preferred_size(const struct osc_plugin *plugin,
				      int *width, int *height)
{
	if (width)
		*width = 640;
	if (height)
		*height = 480;
}

static void context_destroy(struct osc_plugin *plugin, const char *ini_fn)
{
	osc_destroy_context(ctx);
}

struct osc_plugin plugin;

static bool xmw_identify(const struct osc_plugin *plugin)
{
	/* Use the OSC's IIO context just to detect the devices */
	struct iio_context *osc_ctx = get_context_from_osc();

	return !!iio_context_find_device(osc_ctx, CLK_DEVICE) &&
		!!iio_context_find_device(osc_ctx, UPCONV) &&
		!!iio_context_find_device(osc_ctx, DOWNCONV);
}

struct osc_plugin plugin = {
	.name = THIS_DRIVER,
	.identify = xmw_identify,
	.init = xmw_init,
	.update_active_page = update_active_page,
	.get_preferred_size = xmw_get_preferred_size,
	.destroy = context_destroy,
};
