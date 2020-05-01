
/**
 * Author: Dov Salomon
 *
 * A simple pulseaudio program to toggle between pulseaudio
 * sinks and move over all inputs to the new sink.
 *
 * Compiling:
 *
 *     gcc $(pkg-config libpulse --cflags --libs) patoggle.c -o patoggle
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <pulse/pulseaudio.h>

pa_context *context;
pa_mainloop_api *mainloop_api;

char *new_sink = NULL;
char *old_sink = NULL;
char *sinks[10] = { NULL };

int nsinks = 0;

#define LEN(a) (sizeof(a)/sizeof(a[0]))
char *sink_input_blacklist[] = {
    "alsa_output.usb-Blue_Microphones_Yeti_Stereo_Microphone_REV8-00.analog-stereo",
};

void quit(int ret)
{
    if (mainloop_api)
	mainloop_api->quit(mainloop_api, ret);
    else
	exit(ret);
}

/*
 * prints message and exit
 */
void die(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    fprintf(stderr, "pulsemon: ");
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    if (fmt[0] && fmt[strlen(fmt) - 1] != ':') {
	fputc('\n', stderr);
    } else {
	fputc(' ', stderr);
	perror(NULL);
    }

    quit(1);
}

void context_drain_complete(pa_context * c, void *userdata)
{
    pa_context_disconnect(c);
}

void drain()
{
    pa_operation *o;

    if (!(o = pa_context_drain(context, context_drain_complete, NULL)))
	pa_context_disconnect(context);
    else
	pa_operation_unref(o);
}

void
sink_input_info_callback(pa_context * c, const pa_sink_input_info * i,
			 int eol, void *data)
{
    if (eol < 0)
	die("failed to get sink input info: %s",
	    pa_strerror(pa_context_errno(c)));
    if (eol) {
	drain();
	return;
    }

    pa_context_move_sink_input_by_name(c, i->index, new_sink, NULL, NULL);
}

void patoggle()
{
    int i, j, cur = 0, next;
    pa_operation *o;

    if (nsinks < 2)
	quit(1);

    for (i = 0; i < nsinks; i++)
	if (strcmp(old_sink, sinks[i]) == 0)
	    cur = i;

    next = cur;
    for (i = 1; i < nsinks; i++) {
	next = (cur + i) % nsinks;
	for (j = 0; j < LEN(sink_input_blacklist); j++)
	    if (strcmp(sinks[next], sink_input_blacklist[j]) == 0)
		break;
	if (j == LEN(sink_input_blacklist))
	    break;
    }
    if (next == cur)
	quit(1);

    new_sink = strdup(sinks[next]);

    o = pa_context_set_default_sink(context, new_sink, NULL, NULL);
    if (o)
	pa_operation_unref(o);

    o = pa_context_get_sink_input_info_list(context,
					    sink_input_info_callback,
					    NULL);
    if (o)
	pa_operation_unref(o);
}

void
sink_info_callback(pa_context * c, const pa_sink_info * i, int eol,
		   void *data)
{
    if (eol < 0) {
	die("failed to get sink info: %s",
	    pa_strerror(pa_context_errno(c)));
	return;
    }
    if (eol) {
	patoggle();
	return;
    }
    sinks[nsinks++] = strdup(i->name);
}

void
server_info_callback(pa_context * c, const pa_server_info * i, void *data)
{
    pa_operation *o;

    old_sink = strdup(i->default_sink_name);
    if ((o = pa_context_get_sink_info_list(c, sink_info_callback, NULL)))
	pa_operation_unref(o);
}

void
subscribe_callback(pa_context * c, pa_subscription_event_type_t type,
		   uint32_t idx, void *data)
{
    pa_operation *o;

    if ((o = pa_context_get_server_info(c, server_info_callback, NULL))) {
	pa_operation_unref(o);
    }
}

void context_state_callback(pa_context * c, void *data)
{
    pa_operation *o;

    switch (pa_context_get_state(c)) {
    case PA_CONTEXT_READY:
	if ((o =
	     pa_context_get_server_info(c, server_info_callback, NULL)))
	    pa_operation_unref(o);
	break;
    case PA_CONTEXT_FAILED:
	die("Connection failure: %s", pa_strerror(pa_context_errno(c)));
	break;
    case PA_CONTEXT_TERMINATED:
	quit(0);
	break;
    default:
	break;
    }

}

int main()
{
    int ret;
    pa_mainloop *m = NULL;

    setlinebuf(stdout);

    if (!(m = pa_mainloop_new()))
	die("pa_mainloop_new():");

    mainloop_api = pa_mainloop_get_api(m);

    if (!(context = pa_context_new(mainloop_api, NULL)))
	die("pa_context_new():");

    if (pa_context_connect(context, NULL, 0, NULL) < 0)
	die("pa_context_connect():");

    pa_context_set_state_callback(context, context_state_callback, NULL);

    if (pa_mainloop_run(m, &ret) < 0)
	die("pa_mainloop_run():");

    pa_mainloop_free(m);
    return ret;
}
