/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2011 Uwe Hermann <uwe@hermann-uwe.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

 
#include "../libsigrok-internal.h"
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "../config.h" /* Needed for PACKAGE_STRING and others. */
#include "../log.h"
#include <stdio.h>

#undef LOG_PREFIX
#define LOG_PREFIX "csv: "
  
struct context {
	unsigned int num_enabled_channels;
	uint64_t samplerate;
    uint64_t limit_samples;
	char separator;
	bool header_done;
	int *channel_index;
    int *channel_unit;
    float *channel_scale;
    uint16_t *channel_offset;
    double *channel_mmin;
    double *channel_mmax;
    uint32_t ref_min;
    uint32_t ref_max;
    uint64_t mask;
    uint64_t pre_data;
    uint64_t index;
    int type;
};

/*
 * TODO:
 *  - Option to specify delimiter character and/or string.
 *  - Option to (not) print metadata as comments.
 *  - Option to specify the comment character(s), e.g. # or ; or C/C++-style.
 *  - Option to (not) print samplenumber / time as extra column.
 *  - Option to "compress" output (only print changed samples, VCD-like).
 *  - Option to print comma-separated bits, or whole bytes/words (for 8/16
 *    channel LAs) as ASCII/hex etc. etc.
 *  - Trigger support.
 */

static int init(struct sr_output *o, GHashTable *options)
{
	struct context *ctx = NULL;
	struct sr_channel *ch;
	GSList *l;
	int i;
    float range;

	if (!o || !o->sdi)
		return SR_ERR_ARG;

	ctx = malloc(sizeof(struct context));
    if (ctx == NULL){
        sr_err("%s,ERROR:failed to alloc memory.", __func__);
        return SR_ERR;
    }
    memset(ctx, 0, sizeof(struct context));

	o->priv = ctx;
	ctx->separator = ',';
    ctx->mask = 0;
    ctx->index = 0;
    ctx->type = g_variant_get_int16(g_hash_table_lookup(options, "type"));

	/* Get the number of channels, and the unitsize. */
	for (l = o->sdi->channels; l; l = l->next) {
		ch = l->data;
        if (ch->type != ctx->type)
			continue;
		if (!ch->enabled)
			continue;
		ctx->num_enabled_channels++;
	}

	ctx->channel_index = malloc(sizeof(int) * ctx->num_enabled_channels);
    ctx->channel_unit = malloc(sizeof(int) * ctx->num_enabled_channels);
    ctx->channel_scale = malloc(sizeof(float) * ctx->num_enabled_channels);
    ctx->channel_offset = malloc(sizeof(uint16_t) * ctx->num_enabled_channels);
    ctx->channel_mmax = malloc(sizeof(double) * ctx->num_enabled_channels);
    ctx->channel_mmin = malloc(sizeof(double) * ctx->num_enabled_channels);

    if (ctx->channel_index == NULL || ctx->channel_mmin == NULL){
        sr_err("%s,ERROR:failed to alloc memory.", __func__);
        return SR_ERR;
    }

	/* Once more to map the enabled channels. */
	for (i = 0, l = o->sdi->channels; l; l = l->next) {
		ch = l->data;
        if (ch->type != ctx->type)
			continue;
		if (!ch->enabled)
			continue;
        ctx->channel_index[i] = ch->index;
        //ctx->mask |= (1 << ch->index);
        ctx->mask |= (1 << i);
        range = ch->vdiv * ch->vfactor * DS_CONF_DSO_VDIVS;
        ctx->channel_unit[i] = (range >= 5000000) ? 1000000 :
                                (range >= 5000) ? 1000 : 1;
        ctx->channel_scale[i] = range / ctx->channel_unit[i];
        ctx->channel_offset[i] = ch->hw_offset;
        ctx->channel_mmax[i] = ch->map_max;
        ctx->channel_mmin[i] = ch->map_min;
        i++;
	}

	return SR_OK;
}

static GString *gen_header(const struct sr_output *o)
{
	struct context *ctx;
	struct sr_channel *ch;
	GString *header;
	GSList *l;
	time_t t;
	int num_channels, i;

	ctx = o->priv;
	header = g_string_sized_new(512);

	/* Some metadata */
	t = time(NULL);
	g_string_append_printf(header, "; CSV, generated by %s on %s",
			PACKAGE_STRING, ctime(&t));

	/* Columns / channels */
    if (ctx->type == SR_CHANNEL_LOGIC)
        num_channels = g_slist_length(o->sdi->channels);
    else
        num_channels = ctx->num_enabled_channels;
    g_string_append_printf(header, "; Channels (%d/%d)\n",
			ctx->num_enabled_channels, num_channels);


    char *samplerate_s = sr_samplerate_string(ctx->samplerate);
    g_string_append_printf(header, "; Sample rate: %s\n", samplerate_s);
    g_free(samplerate_s);

    char *depth_s = sr_samplecount_string(ctx->limit_samples);
    g_string_append_printf(header, "; Sample count: %s\n", depth_s);
    g_free(depth_s);

    if (ctx->type == SR_CHANNEL_LOGIC)
        g_string_append_printf(header, "Time(s),");
    for (i = 0, l = o->sdi->channels; l; l = l->next, i++) {
        ch = l->data;
        if (ch->type != ctx->type)
            continue;
        if (!ch->enabled)
            continue;
        if (ctx->type == SR_CHANNEL_DSO) {
            char *unit_s = ctx->channel_unit[i] >= 1000000 ? "kV" :
                           ctx->channel_unit[i] >= 1000 ?  "V" : "mV";
            g_string_append_printf(header, " %s (Unit: %s),", ch->name, unit_s);
        } else if (ctx->type == SR_CHANNEL_ANALOG) {
            g_string_append_printf(header, " %s (Unit: %s),", ch->name, ch->map_unit);
        } else {
            g_string_append_printf(header, " %s,", ch->name);
        }
    }
    if (o->sdi->channels)
        /* Drop last separator. */
        g_string_truncate(header, header->len - 1);
    g_string_append_printf(header, "\n");

	return header;
}

static int receive(const struct sr_output *o, const struct sr_datafeed_packet *packet,
		GString **out)
{
	const struct sr_datafeed_meta *meta;
	const struct sr_datafeed_logic *logic;
    const struct sr_datafeed_dso *dso;
    const struct sr_datafeed_analog *analog;
	const struct sr_config *src;
    GSList *l;
	struct context *ctx;
	int idx;
    unsigned char *p, c;
    double tmpv;
    struct sr_channel *ch;

	*out = NULL;
	if (!o || !o->sdi)
		return SR_ERR_ARG;
	if (!(ctx = o->priv))
		return SR_ERR_ARG;

	switch (packet->type) {
	case SR_DF_META:
		meta = packet->payload;
		for (l = meta->config; l; l = l->next) {
			src = l->data;
            if (src->key == SR_CONF_SAMPLERATE)
                ctx->samplerate = g_variant_get_uint64(src->data);
            else if (src->key == SR_CONF_LIMIT_SAMPLES)
                ctx->limit_samples = g_variant_get_uint64(src->data);
            else if (src->key == SR_CONF_REF_MIN)
                ctx->ref_min = g_variant_get_uint32(src->data);
            else if (src->key == SR_CONF_REF_MAX)
                ctx->ref_max = g_variant_get_uint32(src->data);
        }
		break;
	case SR_DF_LOGIC:
		logic = packet->payload;
		if (!ctx->header_done) {
			*out = gen_header(o);
			ctx->header_done = TRUE;
		} else {
			*out = g_string_sized_new(512);
		}

		for (size_t i = 0; i <= logic->length - logic->unitsize; i += logic->unitsize) {
            ctx->index++;

            if (packet->bExportOriginalData == 0){
                if (ctx->index > 1 && (*(uint64_t *)(logic->data + i) & ctx->mask) == ctx->pre_data)
                   continue;
            } 
            
            tmpv = (double)(ctx->index-1) / (double)ctx->samplerate;
            g_string_append_printf(*out, "%0.15g", tmpv); 

            for (size_t j = 0; j < ctx->num_enabled_channels; j++) {
                idx = j;
				p = logic->data + i + idx / 8;
				c = *p & (1 << (idx % 8));
                g_string_append_c(*out, ctx->separator);
                g_string_append_c(*out, c ? '1' : '0');
			}
			g_string_append_printf(*out, "\n");
            ctx->pre_data = (*(uint64_t *)(logic->data + i) & ctx->mask);
		}
		break;
     case SR_DF_DSO:
        dso = packet->payload;
        if (!ctx->header_done) {
            *out = gen_header(o);
            ctx->header_done = TRUE;
        } else {
            *out = g_string_sized_new(512);
        }

        for (size_t i = 0; i < (uint64_t)dso->num_samples; i++) {
            for (size_t j = 0; j < ctx->num_enabled_channels; j++) {
                idx = ctx->channel_index[j];
                p = dso->data + i * ctx->num_enabled_channels + idx * ((ctx->num_enabled_channels > 1) ? 1 : 0);
                g_string_append_printf(*out, "%0.5f", (ctx->channel_offset[j] - *p) *
                                                       ctx->channel_scale[j] /
                                                      (ctx->ref_max - ctx->ref_min));
                g_string_append_c(*out, ctx->separator);
            }

            /* Drop last separator. */
            g_string_truncate(*out, (*out)->len - 1);
            g_string_append_printf(*out, "\n");
        }
        break;
    case SR_DF_ANALOG:
       analog = packet->payload;
       if (!ctx->header_done) {
           *out = gen_header(o);
           ctx->header_done = TRUE;
       } else {
           *out = g_string_sized_new(512);
       }

       int enalbe_channel_flags[8];
       size_t ch_num = 0;

       for (l = o->sdi->channels; l; l = l->next){
            ch = l->data;
            enalbe_channel_flags[ch_num++] = ch->enabled;
       }

       double max_min_ref = (double)(ctx->ref_max - ctx->ref_min);
       double vf = 0;
       int ch_cfg_dex = 0;
       double hw_offset = 0;
       double mapRange = 0;
       double mmax = 0;
       double mmin = 0;
       void *ptr;

       for (size_t i = 0; i < (uint64_t)analog->num_samples; i++) {
            ch_cfg_dex = 0;

           for (size_t j = 0; j < ch_num; j++) {
              // idx = ctx->channel_index[j];
             //  p = analog->data + i * ctx->num_enabled_channels + idx * ((ctx->num_enabled_channels > 1) ? 1 : 0);

                // g_string_append_printf(*out, "%0.5f",  (ctx->channel_offset[j] - *p) *
                //                                      (ctx->channel_mmax[j] - ctx->channel_mmin[j]) /
                //                                      (ctx->ref_max - ctx->ref_min));

               if (enalbe_channel_flags[j] == 0){
                    continue;
               }

               ptr = (unsigned char*)analog->data + i * ch_num + j;
               p = (unsigned char*)ptr;
               hw_offset = (double)ctx->channel_offset[ch_cfg_dex];
               mmax = ctx->channel_mmax[ch_cfg_dex];
               mmin = ctx->channel_mmin[ch_cfg_dex];
               mapRange = (mmax - mmin);
               vf = (hw_offset - (double)(*p)) * mapRange / max_min_ref;

               g_string_append_printf(*out, "%0.5f",  vf);
               g_string_append_c(*out, ctx->separator);

               ch_cfg_dex++;
           }

           /* Drop last separator. */
           g_string_truncate(*out, (*out)->len - 1);
           g_string_append_printf(*out, "\n");
       }
       break;
	}

	return SR_OK;
}

static int cleanup(struct sr_output *o)
{
	struct context *ctx;

	if (!o || !o->sdi)
		return SR_ERR_ARG;

	if (o->priv) {
		ctx = o->priv;
		g_free(ctx->channel_index);
		g_free(o->priv);
		o->priv = NULL;
	}

	return SR_OK;
}

SR_PRIV struct sr_output_module output_csv = {
	.id = "csv",
	.name = "CSV",
	.desc = "Comma-separated values",
	.exts = (const char*[]){"csv", NULL},
	.options = NULL,
	.init = init,
	.receive = receive,
	.cleanup = cleanup,
};
 
