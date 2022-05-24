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
#define UPCONV_A	"admv1013_a"
#define UPCONV_B	"admv1013_b"
#define DOWNCONV_A	"admv1014_a"
#define DOWNCONV_B	"admv1014_b"

static struct iio_context *ctx;

static struct iio_widget iio_widgets[100];
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
	char signal_name[100];
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
	struct iio_device *clk;
	struct iio_channel *clk_ch_out;

	struct iio_channel *downconv_a_ch;
	struct iio_channel *downconv_a_ch_i;
	struct iio_channel *downconv_a_ch_q;
	struct iio_channel *upconv_a_ch;
	struct iio_channel *upconv_a_ch_i;
	struct iio_channel *upconv_a_ch_q;
	struct iio_channel *downconv_b_ch;
	struct iio_channel *downconv_b_ch_i;
	struct iio_channel *downconv_b_ch_q;
	struct iio_channel *upconv_b_ch;
	struct iio_channel *upconv_b_ch_i;
	struct iio_channel *upconv_b_ch_q;

	struct iio_device *upconv_a;
	struct iio_device *downconv_a;
	struct iio_device *upconv_b;
	struct iio_device *downconv_b;

	builder = gtk_builder_new();

	ctx = osc_create_context();
	if (!ctx)
		return NULL;

	if (osc_load_glade_file(builder, "xmw") < 0) {
		osc_destroy_context(ctx);
		return NULL;
	}

	clk = iio_context_find_device(ctx, CLK_DEVICE);
	upconv_a = iio_context_find_device(ctx, UPCONV_A);
	upconv_b = iio_context_find_device(ctx, UPCONV_B);
	downconv_a = iio_context_find_device(ctx, DOWNCONV_A);
	downconv_b = iio_context_find_device(ctx, DOWNCONV_B);

	if (!clk || !upconv_a || !downconv_a || !upconv_b || !downconv_b) {
		printf("Could not find expected iio devices\n");
		osc_destroy_context(ctx);
		return NULL;
	}

	xmw_panel = GTK_WIDGET(gtk_builder_get_object(builder, "xmw_panel"));

	clk_ch_out = iio_device_find_channel(clk, "altvoltage0", true);

	downconv_a_ch = iio_device_find_channel(downconv_a, "altvoltage0", true);
	upconv_a_ch = iio_device_find_channel(upconv_a, "altvoltage0", true);

	upconv_a_ch_i = iio_device_find_channel(upconv_a, "altvoltage0_i", true);
	upconv_a_ch_q = iio_device_find_channel(upconv_a, "altvoltage0_q", true);
	downconv_a_ch_i = iio_device_find_channel(downconv_a, "altvoltage0_i", true);
	downconv_a_ch_q = iio_device_find_channel(downconv_a, "altvoltage0_q", true);

	downconv_b_ch = iio_device_find_channel(downconv_b, "altvoltage0", true);
	upconv_b_ch = iio_device_find_channel(upconv_b, "altvoltage0", true);

	upconv_b_ch_i = iio_device_find_channel(upconv_b, "altvoltage0_i", true);
	upconv_b_ch_q = iio_device_find_channel(upconv_b, "altvoltage0_q", true);
	downconv_b_ch_i = iio_device_find_channel(downconv_b, "altvoltage0_i", true);
	downconv_b_ch_q = iio_device_find_channel(downconv_b, "altvoltage0_q", true);

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

	/* ADMV1014_A */
	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], downconv_a,
					      downconv_a_ch_i, "hardwaregain",
					      builder, "spinbutton_i_gain_downconv_a",
					      NULL);

	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], downconv_a,
					      downconv_a_ch_q, "hardwaregain",
					      builder, "spinbutton_q_gain_downconv_a",
					      NULL);

	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], downconv_a,
					      downconv_a_ch_i, "phase",
					      builder, "spinbutton_i_phase_downconv_a",
					      NULL);

	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], downconv_a,
					      downconv_a_ch_q, "phase",
					      builder, "spinbutton_q_phase_downconv_a",
					      NULL);

	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], downconv_a,
					      downconv_a_ch_i, "offset",
					      builder, "spinbutton_i_offset_downconv_a",
					      NULL);

	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], downconv_a,
					      downconv_a_ch_q, "offset",
					      builder, "spinbutton_q_offset_downconv_a",
					      NULL);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], downconv_a,
					    downconv_a_ch, "bandgap_powerdown", builder,
					    "downconv_a_bandgap_pd_enable", 0);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], downconv_a,
					    downconv_a_ch, "ibias_powerdown", builder,
					    "downconv_a_ibias_pd_enable", 0);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], downconv_a,
					    downconv_a_ch, "lo_path_powerdown", builder,
					    "downconv_a_lo_path_pd_enable", 0);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], downconv_a,
					    downconv_a_ch, "detector_powerdown", builder,
					    "downconv_a_detector_pd_enable", 0);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], downconv_a,
					    downconv_a_ch, "device_powerdown", builder,
					    "downconv_a_device_pd_enable", 0);

	/* ADMV1013_A */
	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], upconv_a,
					      upconv_a_ch_i, "phase",
					      builder, "spinbutton_i_phase_upconv_a",
					      NULL);

	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], upconv_a,
					      upconv_a_ch_q, "phase",
					      builder, "spinbutton_q_phase_upconv_a",
					      NULL);

	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], upconv_a,
					      upconv_a_ch_i, "offset",
					      builder, "spinbutton_i_offset_upconv_a",
					      NULL);

	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], upconv_a,
					      upconv_a_ch_q, "offset",
					      builder, "spinbutton_q_offset_upconv_a",
					      NULL);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], upconv_a,
					    upconv_a_ch, "quadrupler_powerdown", builder,
					    "upconv_a_quad_pd_enable", 0);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], upconv_a,
					    upconv_a_ch, "vga_powerdown", builder,
					    "upconv_a_vga_pd_enable", 0);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], upconv_a,
					    upconv_a_ch, "mixer_powerdown", builder,
					    "upconv_a_mixer_pd_enable", 0);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], upconv_a,
					    upconv_a_ch, "detector_powerdown", builder,
					    "upconv_a_detector_pd_enable", 0);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], upconv_a,
					    upconv_a_ch, "device_powerdown", builder,
					    "upconv_a_device_pd_enable", 0);

	/* ADMV1014_B */
	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], downconv_b,
					      downconv_b_ch_i, "hardwaregain",
					      builder, "spinbutton_i_gain_downconv_b",
					      NULL);

	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], downconv_b,
					      downconv_b_ch_q, "hardwaregain",
					      builder, "spinbutton_q_gain_downconv_b",
					      NULL);

	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], downconv_b,
					      downconv_b_ch_i, "phase",
					      builder, "spinbutton_i_phase_downconv_b",
					      NULL);

	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], downconv_b,
					      downconv_b_ch_q, "phase",
					      builder, "spinbutton_q_phase_downconv_b",
					      NULL);

	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], downconv_b,
					      downconv_b_ch_i, "offset",
					      builder, "spinbutton_i_offset_downconv_b",
					      NULL);

	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], downconv_b,
					      downconv_b_ch_q, "offset",
					      builder, "spinbutton_q_offset_downconv_b",
					      NULL);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], downconv_b,
					    downconv_b_ch, "bandgap_powerdown", builder,
					    "downconv_b_bandgap_pd_enable", 0);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], downconv_b,
					    downconv_b_ch, "ibias_powerdown", builder,
					    "downconv_b_ibias_pd_enable", 0);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], downconv_b,
					    downconv_b_ch, "lo_path_powerdown", builder,
					    "downconv_b_lo_path_pd_enable", 0);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], downconv_b,
					    downconv_b_ch, "detector_powerdown", builder,
					    "downconv_b_detector_pd_enable", 0);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], downconv_b,
					    downconv_b_ch, "device_powerdown", builder,
					    "downconv_b_device_pd_enable", 0);

	/* ADMV1013_B */
	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], upconv_b,
					      upconv_b_ch_i, "phase",
					      builder, "spinbutton_i_phase_upconv_b",
					      NULL);

	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], upconv_b,
					      upconv_b_ch_q, "phase",
					      builder, "spinbutton_q_phase_upconv_b",
					      NULL);

	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], upconv_b,
					      upconv_b_ch_i, "offset",
					      builder, "spinbutton_i_offset_upconv_b",
					      NULL);

	iio_spin_button_int_init_from_builder(&iio_widgets[num_widgets++], upconv_b,
					      upconv_b_ch_q, "offset",
					      builder, "spinbutton_q_offset_upconv_b",
					      NULL);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], upconv_b,
					    upconv_b_ch, "quadrupler_powerdown", builder,
					    "upconv_b_quad_pd_enable", 0);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], upconv_b,
					    upconv_b_ch, "vga_powerdown", builder,
					    "upconv_b_vga_pd_enable", 0);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], upconv_b,
					    upconv_b_ch, "mixer_powerdown", builder,
					    "upconv_b_mixer_pd_enable", 0);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], upconv_b,
					    upconv_b_ch, "detector_powerdown", builder,
					    "upconv_b_detector_pd_enable", 0);

	iio_toggle_button_init_from_builder(&iio_widgets[num_widgets++], upconv_b,
					    upconv_b_ch, "device_powerdown", builder,
					    "upconv_b_device_pd_enable", 0);

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
		!!iio_context_find_device(osc_ctx, UPCONV_A) &&
		!!iio_context_find_device(osc_ctx, DOWNCONV_A) &&
		!!iio_context_find_device(osc_ctx, UPCONV_B) &&
		!!iio_context_find_device(osc_ctx, DOWNCONV_B);
}

struct osc_plugin plugin = {
	.name = THIS_DRIVER,
	.identify = xmw_identify,
	.init = xmw_init,
	.update_active_page = update_active_page,
	.get_preferred_size = xmw_get_preferred_size,
	.destroy = context_destroy,
};
