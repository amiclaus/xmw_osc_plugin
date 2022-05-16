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
#define CLK_DEVICE	"adf5356"
#define UPCONV		"admv1013"
#define DOWNCONV	"admv1014"

static struct iio_context *ctx;

static struct iio_widget iio_widgets[50];
static unsigned int num_widgets;

static GtkWidget *xmw_panel;

static gboolean plugin_detached;
static gint this_page;

const gdouble mhz_scale = 1000000.0;


static void save_widget_value(GtkWidget *widget, struct iio_widget *iio_w)
{
	iio_w->save(iio_w);
	/* refresh widgets so that, we know if our value was updated */
	iio_update_widgets(iio_widgets, num_widgets);
}

static void make_widget_update_signal_based(struct iio_widget *widgets,
		unsigned int num_widgets)
{
	char signal_name[25];
	unsigned int i;

	for (i = 0; i < num_widgets; i++) {
		if (GTK_IS_CHECK_BUTTON(widgets[i].widget))
			sprintf(signal_name, "%s", "toggled");
		else if (GTK_IS_TOGGLE_BUTTON(widgets[i].widget))
			sprintf(signal_name, "%s", "toggled");
		else if (GTK_IS_SPIN_BUTTON(widgets[i].widget))
			sprintf(signal_name, "%s", "value-changed");
		else if (GTK_IS_COMBO_BOX_TEXT(widgets[i].widget))
			sprintf(signal_name, "%s", "changed");
		else
			printf("unhandled widget type, attribute: %s\n",
			       widgets[i].attr_name);

		if (GTK_IS_SPIN_BUTTON(widgets[i].widget) &&
		    widgets[i].priv_progress != NULL) {
			iio_spin_button_progress_activate(&widgets[i]);
		} else {
			g_signal_connect(G_OBJECT(widgets[i].widget),
					 signal_name,
					 G_CALLBACK(save_widget_value),
					 &widgets[i]);
		}
	}
}

static GtkWidget *xmw_init(struct osc_plugin *plugin, GtkWidget *notebook,
			      const char *ini_fn)
{
	GtkBuilder *builder;
	struct iio_channel *clk_ch_out;
	struct iio_channel *downconv_ch;
	struct iio_channel *downconv_ch_i;
	struct iio_channel *downconv_ch_q;
	struct iio_channel *upconv_ch_i;
	struct iio_channel *upconv_ch_q;
	struct iio_channel *upconv_ch;
	struct iio_device *clk;
	struct iio_device *upconv;
	struct iio_device *downconv;

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

	if (!clk || !upconv || !downconv) {
		printf("Could not find expected iio devices\n");
		osc_destroy_context(ctx);
		return NULL;
	}

	xmw_panel = GTK_WIDGET(gtk_builder_get_object(builder,
							 "xmw_panel"));

	clk_ch_out = iio_device_find_channel(clk, "altvoltage0", true);

	downconv_ch = iio_device_find_channel(downconv, "altvoltage0", true);
	upconv_ch = iio_device_find_channel(upconv, "altvoltage0", true);
	upconv_ch_i = iio_device_find_channel(upconv, "altvoltage0_i", true);
	upconv_ch_q = iio_device_find_channel(upconv, "altvoltage0_q", true);
	downconv_ch_i = iio_device_find_channel(downconv, "altvoltage0_i", true);
	downconv_ch_q = iio_device_find_channel(downconv, "altvoltage0_q", true);

	/* ADF5356 */
	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], clk,
					      clk_ch_out, "frequency",
					      builder, "spinbutton_out_freq",
					      &mhz_scale);

	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], clk,
					      clk_ch_out, "refin_frequency",
					      builder, "spinbutton_refin_freq",
					      &mhz_scale);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], clk,
					    clk_ch_out, "powerdown", builder,
					    "clk_powerdown_enable", 0);

	/* ADMV1014 */
	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], downconv,
					      downconv_ch_i, "hardwaregain",
					      builder, "spinbutton_i_gain_downconv",
					      NULL);

	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], downconv,
					      downconv_ch_q, "hardwaregain",
					      builder, "spinbutton_q_gain_downconv",
					      NULL);

	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], downconv,
					      downconv_ch_i, "phase",
					      builder, "spinbutton_i_phase_downconv",
					      NULL);

	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], downconv,
					      downconv_ch_q, "phase",
					      builder, "spinbutton_q_phase_downconv",
					      NULL);

	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], downconv,
					      downconv_ch_i, "offset",
					      builder, "spinbutton_i_offset_downconv",
					      NULL);

	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], downconv,
					      downconv_ch_q, "offset",
					      builder, "spinbutton_q_offset_downconv",
					      NULL);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], downconv,
					    downconv_ch, "bandgap_powerdown", builder,
					    "downconv_bandgap_pd_enable", 0);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], downconv,
					    downconv_ch, "ibias_powerdown", builder,
					    "downconv_ibias_pd_enable", 0);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], downconv,
					    downconv_ch, "lo_path_powerdown", builder,
					    "downconv_lo_path_pd_enable", 0);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], downconv,
					    downconv_ch, "detector_powerdown", builder,
					    "downconv_detector_pd_enable", 0);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], downconv,
					    downconv_ch, "device_powerdown", builder,
					    "downconv_device_pd_enable", 0);

	/* ADMV1013 */
	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], upconv,
					      upconv_ch_i, "phase",
					      builder, "spinbutton_i_phase_upconv",
					      NULL);

	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], upconv,
					      upconv_ch_q, "phase",
					      builder, "spinbutton_q_phase_upconv",
					      NULL);

	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], upconv,
					      upconv_ch_i, "offset",
					      builder, "spinbutton_i_offset_upconv",
					      NULL);

	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], upconv,
					      upconv_ch_q, "offset",
					      builder, "spinbutton_q_offset_upconv",
					      NULL);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], upconv,
					    upconv_ch, "quadrupler_powerdown", builder,
					    "upconv_quad_pd_enable", 0);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], upconv,
					    upconv_ch, "vga_powerdown", builder,
					    "upconv_vga_pd_enable", 0);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], upconv,
					    upconv_ch, "mixer_powerdown", builder,
					    "upconv_mixer_pd_enable", 0);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], upconv,
					    upconv_ch, "detector_powerdown", builder,
					    "upconv_detector_pd_enable", 0);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], upconv,
					    upconv_ch, "device_powerdown", builder,
					    "upconv_device_pd_enable", 0);

	make_widget_update_signal_based(iio_widgets, num_widgets);
	iio_update_widgets(iio_widgets, num_widgets);

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
