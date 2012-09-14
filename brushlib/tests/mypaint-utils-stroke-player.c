#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "mypaint-utils-stroke-player.h"

int lines_in_string(const char *str) {
    int lines = 0;
    while (*str) {
        if (*str == '\n') {
            lines++;
        }
        str++;
    }
    return lines;
}

typedef struct {
    gboolean valid;
    float time;
    float x;
    float y;
    float pressure;
    float xtilt;
    float ytilt;
} MotionEvent;

struct _MyPaintUtilsStrokePlayer {
    MyPaintSurface *surface;
    MyPaintBrush *brush;
    MotionEvent *events;
    int current_event_index;
    int number_of_events;
    gboolean transaction_on_stroke; /* If MyPaintBrush::stroke_to should be done between MyPaintSurface::begin_atomic() end_atomic() calls.*/
};

MyPaintUtilsStrokePlayer *
mypaint_utils_stroke_player_new()
{
    MyPaintUtilsStrokePlayer *self = (MyPaintUtilsStrokePlayer *)malloc(sizeof(MyPaintUtilsStrokePlayer));

    self->surface = NULL;
    self->brush = NULL;
    self->events = NULL;
    self->number_of_events = 0;
    self->current_event_index = 0;
    self->transaction_on_stroke = TRUE;

    return self;
}

void
mypaint_utils_stroke_player_free(MyPaintUtilsStrokePlayer *self)
{
    if (self->events) {
        free(self->events);
    }
    free(self);
}

void
mypaint_utils_stroke_player_set_brush(MyPaintUtilsStrokePlayer *self, MyPaintBrush *brush)
{
    self->brush = brush;
}

void mypaint_utils_stroke_player_set_surface(MyPaintUtilsStrokePlayer *self, MyPaintSurface *surface)
{
    self->surface = surface;
}

void
mypaint_utils_stroke_player_set_source_data(MyPaintUtilsStrokePlayer *self, const char *data)
{
    self->number_of_events = lines_in_string(data);
    self->events = (MotionEvent *)malloc(sizeof(MotionEvent) * self->number_of_events);

    char * data_copy = strdup(data);

    assert(data_copy);

    char * line = strtok(data_copy, "\n");;
    for (int i=0; i<self->number_of_events; i++) {
        MotionEvent *event = &self->events[i];

        int matches = sscanf(line, "%f %f %f %f",
                             &event->time, &event->x, &event->y, &event->pressure);
        if (matches != 4) {
            event->valid = FALSE;
            fprintf(stderr, "Error: Unable to parse line '%s'\n", line);
        } else {
            event->valid = TRUE;
        }
        event->xtilt = 0.0;
        event->ytilt = 0.0;

        line = strtok(NULL, "\n");
    }

    free(data_copy);

    mypaint_utils_stroke_player_reset(self);
}

gboolean
mypaint_utils_stroke_player_iterate(MyPaintUtilsStrokePlayer *self)
{
    const MotionEvent *event = &self->events[self->current_event_index];
    const int last_event_index = self->current_event_index-1;
    float last_event_time = 0.0;
    if (last_event_index >= 0) {
        const MotionEvent *last_event = &self->events[last_event_index];
        last_event_time = last_event->time;
    }
    const float dtime = event->time - last_event_time;

    if (event->valid) {
        if (self->transaction_on_stroke) {
            mypaint_surface_begin_atomic(self->surface);
        }

        mypaint_brush_stroke_to(self->brush, self->surface,
                                event->x, event->y, event->pressure,
                                event->xtilt, event->ytilt, dtime);

        if (self->transaction_on_stroke) {
            mypaint_surface_end_atomic(self->surface);
        }
    }
    self->current_event_index++;

    if (self->current_event_index < self->number_of_events) {
        return TRUE;
    } else {
        mypaint_utils_stroke_player_reset(self);
        return FALSE;
    }
}

void
mypaint_utils_stroke_player_reset(MyPaintUtilsStrokePlayer *self)
{
    self->current_event_index = 0;
}

void
mypaint_utils_stroke_player_run_sync(MyPaintUtilsStrokePlayer *self)
{
    while(mypaint_utils_stroke_player_iterate(self)) {
        ;
    }
}